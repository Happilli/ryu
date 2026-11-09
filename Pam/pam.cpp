#include "./pam.hpp"

#include <QByteArray>
#include <QMetaObject>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <pwd.h>
#include <security/pam_appl.h>
#include <thread>
#include <unistd.h>

struct PamSession {
  std::mutex mutex;
  std::condition_variable cv;
  QString response;
  bool hasResponse = false;
  bool aborted = false;
};

namespace {

struct ConvData {
  std::shared_ptr<PamSession> session;
  QPointer<PamContext> context;
  quint64 generation = 0;
};

void releaseReplies(pam_response *replies, int count) {
  if (!replies) {
    return;
  }
  for (int i = 0; i < count; ++i) {
    if (replies[i].resp) {
      std::memset(replies[i].resp, 0, std::strlen(replies[i].resp));
      std::free(replies[i].resp);
    }
  }
  std::free(replies);
}

int converse(int count, const pam_message **messages, pam_response **out,
             void *ptr) {
  auto *data = static_cast<ConvData *>(ptr);
  auto *replies = static_cast<pam_response *>(
      std::calloc(static_cast<size_t>(count), sizeof(pam_response)));
  if (!replies) {
    return PAM_BUF_ERR;
  }

  for (int i = 0; i < count; ++i) {
    const pam_message *message = messages[i];
    const int style = message->msg_style;
    const bool prompt =
        style == PAM_PROMPT_ECHO_OFF || style == PAM_PROMPT_ECHO_ON;
    const QString text = QString::fromUtf8(message->msg ? message->msg : "");

    {
      std::lock_guard<std::mutex> lock(data->session->mutex);
      if (data->session->aborted || !data->context) {
        releaseReplies(replies, count);
        return PAM_CONV_ERR;
      }
      data->session->hasResponse = false;
    }

    QPointer<PamContext> context = data->context;
    const quint64 generation = data->generation;
    QMetaObject::invokeMethod(
        context.data(),
        [context, generation, text, style]() {
          if (context) {
            context->handleMessage(generation, text, style);
          }
        },
        Qt::QueuedConnection);

    if (!prompt) {
      continue;
    }

    std::unique_lock<std::mutex> lock(data->session->mutex);
    data->session->cv.wait(lock, [&]() {
      return data->session->hasResponse || data->session->aborted;
    });
    if (data->session->aborted) {
      lock.unlock();
      releaseReplies(replies, count);
      return PAM_CONV_ERR;
    }
    const QByteArray bytes = data->session->response.toUtf8();
    data->session->response.clear();
    replies[i].resp =
        static_cast<char *>(std::malloc(static_cast<size_t>(bytes.size()) + 1));
    if (!replies[i].resp) {
      lock.unlock();
      releaseReplies(replies, count);
      return PAM_BUF_ERR;
    }
    std::memcpy(replies[i].resp, bytes.constData(),
                static_cast<size_t>(bytes.size()));
    replies[i].resp[bytes.size()] = '\0';
  }

  *out = replies;
  return PAM_SUCCESS;
}

void postFinished(const QPointer<PamContext> &context, quint64 generation,
                  int code, bool startFailed) {
  if (!context) {
    return;
  }
  QMetaObject::invokeMethod(
      context.data(),
      [context, generation, code, startFailed]() {
        if (context) {
          context->handleFinished(generation, code, startFailed);
        }
      },
      Qt::QueuedConnection);
}

void runSession(std::shared_ptr<PamSession> session,
                QPointer<PamContext> context, quint64 generation,
                QString config, QString directory, QString user) {
  ConvData data{session, context, generation};
  pam_conv conv{converse, &data};
  pam_handle_t *handle = nullptr;

  const QByteArray service = config.toUtf8();
  const QByteArray account = user.toUtf8();
  const QByteArray dir = directory.toUtf8();

  int code =
      dir.isEmpty()
          ? pam_start(service.constData(), account.constData(), &conv, &handle)
          : pam_start_confdir(service.constData(), account.constData(), &conv,
                              dir.constData(), &handle);

  if (code != PAM_SUCCESS) {
    postFinished(context, generation, code, true);
    return;
  }

  code = pam_authenticate(handle, 0);
  pam_end(handle, code);
  postFinished(context, generation, code, false);
}

QString currentUser() {
  if (const passwd *entry = getpwuid(getuid())) {
    return QString::fromLocal8Bit(entry->pw_name);
  }
  return qEnvironmentVariable("USER");
}

} // namespace

PamContext::PamContext(QObject *parent) : QObject(parent) {}

PamContext::~PamContext() {
  if (!m_session) {
    return;
  }
  {
    std::lock_guard<std::mutex> lock(m_session->mutex);
    m_session->aborted = true;
  }
  m_session->cv.notify_all();
}

void PamContext::setConfig(const QString &config) {
  if (m_config == config) {
    return;
  }
  m_config = config;
  emit configChanged();
}

void PamContext::setConfigDirectory(const QString &dir) {
  if (m_configDirectory == dir) {
    return;
  }
  m_configDirectory = dir;
  emit configDirectoryChanged();
}

void PamContext::setUser(const QString &user) {
  if (m_user == user) {
    return;
  }
  m_user = user;
  emit userChanged();
}

void PamContext::clearPrompt() {
  const bool hadMessage = !m_message.isEmpty() || m_messageIsError;
  const bool hadResponse = m_responseRequired || m_responseVisible;
  m_message.clear();
  m_messageIsError = false;
  m_responseRequired = false;
  m_responseVisible = false;
  if (hadMessage) {
    emit messageChanged();
  }
  if (hadResponse) {
    emit responseChanged();
  }
}

bool PamContext::start() {
  if (m_active) {
    return false;
  }
  clearPrompt();

  m_session = std::make_shared<PamSession>();
  const quint64 generation = ++m_generation;
  const QString user = m_user.isEmpty() ? currentUser() : m_user;

  m_active = true;
  emit activeChanged();

  std::thread(runSession, m_session, QPointer<PamContext>(this), generation,
              m_config, m_configDirectory, user)
      .detach();
  return true;
}

void PamContext::abort() {
  if (!m_session) {
    return;
  }
  {
    std::lock_guard<std::mutex> lock(m_session->mutex);
    m_session->aborted = true;
  }
  m_session->cv.notify_all();
  m_session.reset();
  ++m_generation;
  clearPrompt();
  if (m_active) {
    m_active = false;
    emit activeChanged();
  }
}

void PamContext::respond(const QString &response) {
  if (!m_session || !m_responseRequired) {
    return;
  }
  m_responseRequired = false;
  m_responseVisible = false;
  emit responseChanged();
  {
    std::lock_guard<std::mutex> lock(m_session->mutex);
    m_session->response = response;
    m_session->hasResponse = true;
  }
  m_session->cv.notify_all();
}

void PamContext::handleMessage(quint64 generation, const QString &text,
                               int style) {
  if (generation != m_generation) {
    return;
  }
  m_message = text;
  m_messageIsError = style == PAM_ERROR_MSG;
  m_responseRequired =
      style == PAM_PROMPT_ECHO_OFF || style == PAM_PROMPT_ECHO_ON;
  m_responseVisible = style == PAM_PROMPT_ECHO_ON;
  emit messageChanged();
  emit responseChanged();
  emit pamMessage();
}

void PamContext::handleFinished(quint64 generation, int code,
                                bool startFailed) {
  if (generation != m_generation) {
    return;
  }
  m_session.reset();
  clearPrompt();
  m_active = false;
  emit activeChanged();

  if (startFailed) {
    emit error(PamError::StartFailed);
    return;
  }

  PamResult::Enum result = PamResult::Error;
  switch (code) {
  case PAM_SUCCESS:
    result = PamResult::Success;
    break;
  case PAM_AUTH_ERR:
  case PAM_USER_UNKNOWN:
  case PAM_CRED_INSUFFICIENT:
    result = PamResult::Failed;
    break;
  case PAM_MAXTRIES:
    result = PamResult::MaxTries;
    break;
  case PAM_PERM_DENIED:
    result = PamResult::PermissionDenied;
    break;
  case PAM_CONV_ERR:
    emit error(PamError::ConversationFailed);
    return;
  default:
    break;
  }
  emit completed(result);
}

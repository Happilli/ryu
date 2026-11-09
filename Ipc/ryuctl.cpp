#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace {

std::string socketPath() {
  if (const char *path = std::getenv("RYU_IPC_SOCKET"); path && *path)
    return path;
  if (const char *runtime = std::getenv("XDG_RUNTIME_DIR"); runtime && *runtime)
    return std::string(runtime) + "/ryu-ipc.sock";
  return "/tmp/ryu-ipc-" + std::to_string(getuid()) + ".sock";
}

bool writeAll(int fd, const std::string &data) {
  size_t offset = 0;
  while (offset < data.size()) {
    const ssize_t written =
        write(fd, data.data() + offset, data.size() - offset);
    if (written < 0) {
      if (errno == EINTR)
        continue;
      return false;
    }
    offset += static_cast<size_t>(written);
  }
  return true;
}

int usage() {
  std::fputs("usage: ryuctl show\n"
             "       ryuctl call <target> <function> [args...]\n",
             stderr);
  return 2;
}

} // namespace

int main(int argc, char **argv) {
  std::signal(SIGPIPE, SIG_IGN);

  std::vector<std::string> fields;
  if (argc == 2 && std::strcmp(argv[1], "show") == 0) {
    fields.emplace_back("@show");
  } else if (argc >= 4 && std::strcmp(argv[1], "call") == 0) {
    for (int i = 2; i < argc; ++i)
      fields.emplace_back(argv[i]);
  } else {
    return usage();
  }

  const std::string path = socketPath();
  sockaddr_un addr{};
  addr.sun_family = AF_UNIX;
  if (path.size() >= sizeof(addr.sun_path)) {
    std::fputs("ryuctl: socket path too long\n", stderr);
    return 1;
  }
  std::memcpy(addr.sun_path, path.c_str(), path.size() + 1);

  const int fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
  if (fd < 0) {
    std::fprintf(stderr, "ryuctl: socket: %s\n", std::strerror(errno));
    return 1;
  }

  if (::connect(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
    std::fprintf(stderr, "ryuctl: cannot connect to %s: %s\n", path.c_str(),
                 std::strerror(errno));
    return 1;
  }

  std::string message = std::to_string(fields.size());
  message.push_back('\0');
  for (const std::string &field : fields) {
    message += field;
    message.push_back('\0');
  }

  if (!writeAll(fd, message)) {
    std::fputs("ryuctl: write failed\n", stderr);
    return 1;
  }

  std::string reply;
  char chunk[4096];
  while (true) {
    const ssize_t got = read(fd, chunk, sizeof(chunk));
    if (got < 0) {
      if (errno == EINTR)
        continue;
      std::fprintf(stderr, "ryuctl: read: %s\n", std::strerror(errno));
      return 1;
    }
    if (got == 0)
      break;
    reply.append(chunk, static_cast<size_t>(got));
  }
  close(fd);

  if (reply.empty()) {
    std::fputs("ryuctl: empty reply\n", stderr);
    return 1;
  }

  const bool ok = reply[0] == 'o';
  const std::string payload = reply.substr(1);
  FILE *out = ok ? stdout : stderr;
  if (!payload.empty()) {
    std::fwrite(payload.data(), 1, payload.size(), out);
    if (payload.back() != '\n')
      std::fputc('\n', out);
  }
  return ok ? 0 : 1;
}

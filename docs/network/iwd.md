# Iwd

A Wi-Fi backend for [iwd](https://iwd.wiki.kernel.org/). Iwd talks to the iwd daemon over D-Bus and gives QML the device state, the visible and saved networks, and a password agent for the prompts iwd sends. It draws nothing itself. You decide whether the networks become a list, a menu, or a tray popup.

This one is a minimal Wi-Fi panel: a power switch, a scan button and a list of visible networks.

```qml
import QtQuick
import QtQuick.Controls
import Ryu.Network

Column {
    spacing: 8

    Iwd {
        id: iwd
        onError: msg => console.warn("Iwd:", msg)
    }

    Row {
        spacing: 12
        Switch {
            checked: iwd.powered
            enabled: iwd.available
            onToggled: iwd.powered = checked
        }
        Button {
            text: iwd.scanning ? "Scanning" : "Scan"
            enabled: iwd.powered && !iwd.scanning
            onClicked: iwd.scan()
        }
        Label { text: iwd.connectedSsid || iwd.state }
    }

    Repeater {
        model: iwd.networks

        delegate: Button {
            required property var modelData
            text: modelData.name + "  " + modelData.strength + "%"
            highlighted: modelData.connected
            onClicked: iwd.connectToNetwork(modelData.path)
        }
    }
}
```

How it works:

- **No setup call:** Iwd connects as soon as it is created. There is no `start()`.
- **Live data:** the lists and state properties update by themselves when iwd reports a change.
- **Connect by path:** every network has a `path`. Pass it to `connectToNetwork()`.

!!! note "iwd must be running"
    Iwd needs the iwd daemon on the system D-Bus. If it is not running, `available` is `false` and everything else stays empty. When the daemon starts later, Iwd picks it up on its own.

## Properties

### State

| Property | Type | Description |
|---|---|---|
| `available` | `bool` | Read-only. True while the iwd daemon is on the bus. |
| `powered` | `bool` | Whether the Wi-Fi device is on. **Writable.** Setting it powers the device on or off. |
| `scanning` | `bool` | Read-only. True while a scan is in progress. |
| `state` | `string` | Read-only. The station state: `connected`, `connecting`, `disconnecting`, `disconnected` or `roaming`. Iwd also reports `off` when the device has no station (powered off) and `unavailable` when there is no device or no daemon. |
| `deviceName` | `string` | Read-only. The interface name, for example `wlan0`. Empty when there is no device. |
| `connectedSsid` | `string` | Read-only. The name of the connected network, or empty. |

### Networks

| Property | Type | Description |
|---|---|---|
| `networks` | `list<var>` | Read-only. Visible networks, strongest first. See [Network entries](#network-entries). |
| `knownNetworks` | `list<var>` | Read-only. Saved networks, sorted by name. See [Saved entries](#saved-entries). |

### Password request

These describe the question iwd is currently asking. See [Password requests](#password-requests).

| Property | Type | Description |
|---|---|---|
| `requestPending` | `bool` | Read-only. True while iwd is waiting for a credential. |
| `requestKind` | `string` | Read-only. `passphrase`, `privateKeyPassphrase`, `userAndPassword` or `userPassword`. Empty when nothing is pending. |
| `requestNetwork` | `string` | Read-only. The name of the network being asked about. Empty when nothing is pending. |
| `requestUser` | `string` | Read-only. A username supplied by iwd, to prefill a field. Often empty. |

## Network entries

Each item in `networks` is a map with these keys:

| Key | Type | Description |
|---|---|---|
| `path` | `string` | D-Bus path. Use it with `connectToNetwork()`, `forget()` and `setAutoConnect()`. |
| `name` | `string` | The SSID. |
| `type` | `string` | `open`, `psk` or `8021x` (enterprise). |
| `secured` | `bool` | `true` for everything except `open`. |
| `connected` | `bool` | Whether this is the connected network. |
| `known` | `bool` | Whether it is saved. |
| `knownPath` | `string` | Path of the saved entry, or empty. |
| `autoConnect` | `bool` | The saved network's auto-connect flag. `false` if not saved. |
| `signal` | `real` | Signal level in dBm, for example `-61.0`. |
| `strength` | `int` | Signal as a percentage, 0 to 100. |

`strength` is `(dBm + 90) × 2.5`, clamped. So -90 dBm is 0 and -50 dBm is 100.

## Saved entries

Each item in `knownNetworks` is a map with these keys:

| Key | Type | Description |
|---|---|---|
| `path` | `string` | D-Bus path of the saved entry. |
| `name` | `string` | The SSID. |
| `type` | `string` | `open`, `psk` or `8021x`. |
| `hidden` | `bool` | Whether iwd saved it as a hidden network. |
| `autoConnect` | `bool` | The auto-connect flag. |
| `lastConnected` | `string` | When it was last joined, as iwd reports it. |

Saved entries have no signal data, so they cannot be connected to from this list. Connect from `networks` instead.

## Methods

| Method | Description |
|---|---|
| `scan()` | Starts a scan. Does nothing without a device. |
| `refresh()` | Re-reads all state from iwd. Called for you whenever iwd changes, so you rarely need it. |
| `connectToNetwork(path, passphrase = "", user = "")` | Connects to a visible network. If you pass a secret, the agent answers iwd's prompt with it and no request is shown. |
| `connectToSsid(ssid, passphrase = "", user = "")` | Looks the network up by name in `networks` and connects. Emits `error` with `network not found: <ssid>` if it is not visible. |
| `connectHidden(ssid, passphrase = "")` | Connects to a hidden network by name. |
| `disconnectNetwork()` | Disconnects the station. |
| `forget(path)` | Deletes a saved network. Accepts either a network `path` or a saved `path`. |
| `setAutoConnect(path, on)` | Sets the auto-connect flag on a saved network. Same path rules as `forget()`. |
| `submitPassphrase(passphrase)` | Answers a pending request that needs only a secret. |
| `submitCredentials(user, password)` | Answers a pending `userAndPassword` request. |
| `cancelRequest()` | Rejects the pending request. |

Calls are asynchronous. Results arrive through `connectResult`, `error` and the property signals.

## Signals

| Signal | Description |
|---|---|
| `connectResult(string path, bool ok, string message)` | A `connectToNetwork()` or `connectHidden()` call finished. `message` is empty on success. For hidden networks `path` is the SSID. |
| `error(string message)` | A D-Bus call failed. See [Errors](#errors). |
| `stateChanged()` | Any of `powered`, `scanning`, `state`, `deviceName` or `connectedSsid` changed. |
| `networksChanged()` | The visible list changed. |
| `knownNetworksChanged()` | The saved list changed. |
| `availableChanged()` | The daemon appeared or disappeared. |
| `requestChanged()` | The pending request appeared, changed or was cleared. |

All state properties share `stateChanged`, so a handler runs when any of them changes. Bindings are unaffected.

## Password requests

Iwd asks for credentials through an agent, and Iwd registers one for you. There are two ways a request resolves.

**Silently**, when you pass the secret up front:

```qml
iwd.connectToNetwork(path, "hunter2")
```

Iwd stores the secret for the duration of the call and answers the prompt with it. `requestPending` never becomes true. For an enterprise network (`userAndPassword`) pass both the password and the user, otherwise the request falls through to the next case.

**Interactively**, when you pass nothing (or not enough):

1. Iwd asks, and `requestPending` becomes `true`.
2. Read `requestNetwork`, `requestKind` and `requestUser` to build a prompt.
3. Call `submitPassphrase()` or `submitCredentials()`, or `cancelRequest()` to give up.

```qml
Iwd {
    id: iwd
    onRequestChanged: {
        if (!requestPending)
            return
        userField.visible = requestKind === "userAndPassword"
        userField.text = requestUser
        dialog.title = requestNetwork
        dialog.open()
    }
}

// in the dialog's accept handler
iwd.requestKind === "userAndPassword"
    ? iwd.submitCredentials(userField.text, passField.text)
    : iwd.submitPassphrase(passField.text)

// in the dialog's cancel handler
iwd.cancelRequest()
```

Only one request is pending at a time. A new request cancels the previous one.

## Errors

`error(message)` carries the text of a failed D-Bus call. Four kinds of failure are swallowed on purpose, because they are the normal result of the user cancelling or of overlapping calls:

- `Aborted`
- `Canceled`
- `Busy`
- `InProgress`

So cancelling a prompt does not produce a red error. A failed connection arrives through `connectResult` with `ok` set to `false`, and also through `error` if the call itself was rejected.

Normal calls time out after 25 seconds, and connects after 120 seconds.

## How it works

- Iwd reads everything from iwd's object manager: the device, the station, every network and every saved network.
- Any property or object change from iwd starts a 150 ms timer. When it fires, one refresh runs, so a burst of changes becomes one update.
- Properties only emit their signal when the value really changed, so a refresh that finds nothing new is silent.
- The first Wi-Fi device found is used. Iwd does not manage several devices.
- If the daemon leaves the bus, all state resets to empty and `state` becomes `unavailable`. When it comes back, the agent is registered again and state is refreshed.
- On destruction Iwd unregisters its agent.

## Auto-connect

The `autoConnect` flag is **iwd's**, not Iwd's. `setAutoConnect()` only writes the flag, and iwd decides when to join.

- After a failed attempt iwd backs off its background scans, so a reconnect can take a while.
- Calling `disconnectNetwork()` pauses autoconnect until you connect manually again.

To make iwd retry faster, put this in `/etc/iwd/main.conf` and restart iwd:

```
[Scan]
InitialPeriodicScanInterval=3
MaximumPeriodicScanInterval=20
```

## More snippets

### Connect by name

```qml
iwd.connectToSsid("HomeWiFi", "hunter2")
```

### Show what happened

```qml
Iwd {
    onConnectResult: (path, ok, message) =>
        status.text = ok ? "Connected" : "Failed: " + message
    onError: msg => status.text = msg
}
```

### A busy indicator

```qml
property bool connecting: false

Iwd {
    id: iwd
    onConnectResult: root.connecting = false
    onError: root.connecting = false
}

BusyIndicator {
    running: iwd.scanning || root.connecting || iwd.state === "connecting"
}
```

Iwd has no `connecting` property of its own. Track it yourself from the call to `connectResult`, as above.

### Saved networks

```qml
Repeater {
    model: iwd.knownNetworks

    delegate: CheckBox {
        required property var modelData
        text: modelData.name
        checked: modelData.autoConnect
        onToggled: iwd.setAutoConnect(modelData.path, checked)
    }
}
```

### One shared instance

Make one Iwd and share it. In a Quickshell config that means a singleton:

```qml
pragma Singleton
import Quickshell
import Ryu.Network

Singleton {
    readonly property var client: iwd
    Iwd { id: iwd }
}
```

## Notes

!!! warning "Create only one Iwd"
    Each instance registers its own password agent, and iwd handles a single agent. A second instance can end up unable to answer prompts. Keep one instance and share it, for example in a singleton.

!!! warning "Networks are not rescanned for you"
    `networks` only contains what iwd already knows about. It refreshes when iwd scans in the background, but call `scan()` when a user opens a network list. Iwd does not rate-limit scans for you, so avoid calling it in a loop.

!!! warning "`knownNetworks` entries cannot connect"
    A saved entry has no signal information. To connect, find the same network in `networks` (match on `name`) and use its `path`.

- **Paths are not stable names.** Treat `path` as an opaque ID for the current session. Use `name` for display and matching.
- **`powered` is optimistic-free.** Setting it sends the request, and the property only changes once iwd confirms. Bind a switch to `iwd.powered`, not to your own flag.
- **Setting `powered` without a device** does nothing.
- **Secrets are in memory only.** A secret passed to `connectToNetwork()` is cleared when the call finishes.
- **Hidden networks** can only be joined through `connectHidden()`. They do not appear in `networks` until they have been joined, after which they show up in `knownNetworks` with `hidden: true`.
- **WEP** is not supported by iwd, so `type` is never `wep`.
- **Requirements:** iwd, plus a running system D-Bus. If NetworkManager or wpa_supplicant is also managing the device, the two will fight.
- **Threading:** all calls are asynchronous and all properties and signals are safe to use from QML.


# Cleave

An audio visualizer backend. Cleave listens to your system audio through PipeWire, runs an FFT on it, and hands QML a list of frequency band levels. It draws nothing itself. You decide whether the bands become bars, a ring, a blob, or something worse.

This one is a string pinned at both ends that vibrates with the music, with the bass in the middle and the treble toward the edges. Its color comes from the system palette, so it follows light and dark mode automatically.

```qml
import QtQuick
import Ryu.Cleave

Item {
    id: root
    width: 480
    height: 160

    property color stringColor: palette.windowText

    Cleave {
        id: cleave
        bandCount: 24
        smoothing: 0.85
        Component.onCompleted: startCapture()
        onDataChanged: canvas.requestPaint()
        onError: msg => console.warn("Cleave:", msg)
    }

    onStringColorChanged: canvas.requestPaint()

    Canvas {
        id: canvas
        anchors.fill: parent
        antialiasing: true

        onPaint: {
            const ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)

            const m = cleave.magnitudes
            const n = m.length
            if (n === 0)
                return

            const midY = height / 2
            const maxAmp = height * 0.4
            const pad = 12
            const w = width - pad * 2
            const count = n * 2

            const pts = []
            for (let i = 0; i < count; ++i) {
                const t = i / (count - 1)
                const k = i < n ? n - 1 - i : i - n
                const level = Math.min(1, m[k] * 5)
                const pin = Math.sin(t * Math.PI)
                const dir = (i % 2 === 0) ? 1 : -1

                pts.push({
                    x: pad + t * w,
                    y: midY + dir * level * maxAmp * pin
                })
            }

            ctx.lineWidth = 2.5
            ctx.lineCap = "round"
            ctx.lineJoin = "round"
            ctx.strokeStyle = root.stringColor

            ctx.beginPath()
            ctx.moveTo(pts[0].x, pts[0].y)
            for (let i = 1; i < pts.length - 1; ++i) {
                const mx = (pts[i].x + pts[i + 1].x) / 2
                const my = (pts[i].y + pts[i + 1].y) / 2
                ctx.quadraticCurveTo(pts[i].x, pts[i].y, mx, my)
            }
            const last = pts[pts.length - 1]
            ctx.lineTo(last.x, last.y)
            ctx.stroke()
        }
    }
}
```

How it works:

- **Theme:** `palette.windowText` is dark in light mode and light in dark mode, so the line is always readable. Use `palette.highlight` for an accent color, or a fixed color if you prefer.
- **Pinned ends:** the `sin(t * π)` taper flattens the line at both ends, like a real string.
- **Mirroring:** the spectrum is reflected around the center so the curve is symmetric and has no seam.
- **Tuning:** change the `* 5` gain and `maxAmp` to taste, and raise `smoothing` toward 0.9 for a calmer line.
- **Repaint on theme change:** the `onStringColorChanged` handler redraws the canvas, otherwise the old color would stay until the next audio update.

!!! note "Cleave does not start by itself"
    Nothing happens until you call `startCapture()`. The example does it in `Component.onCompleted`.

## Properties

### Analysis

| Property | Type | Default | Description |
|---|---|---|---|
| `bandCount` | `int` | `32` | Number of frequency bands, clamped to 2 to 512. Changing it resets all levels to zero. |
| `smoothing` | `real` | `0.75` | How much of the previous level is kept each frame, clamped to 0 to 0.99. Higher is smoother and slower. `0` is raw. |
| `peakDecay` | `real` | `0.02` | How much the peak markers fall per frame, clamped to 0.001 to 0.5. Also the speed at which the levels fall to zero after the sound stops. |
| `silenceThreshold` | `real` | `0.0001` | Frames quieter than this (RMS of the samples, 0 to 1) are ignored. |
| `source` | `string` | empty | What to capture. Empty means the default audio output (what you hear). See [Choosing a source](#choosing-a-source). |
| `debugMode` | `bool` | `false` | Prints capture state and a once-a-second status line to the console. |

### Output

| Property | Type | Description |
|---|---|---|
| `magnitudes` | `list<real>` | Current level of each band, lowest frequency first. Its length is `bandCount`. |
| `peaks` | `list<real>` | Slowly falling peak of each band, the same length as `magnitudes`. |
| `fps` | `int` | How many FFT frames were processed in the last second. This is the analysis rate, not your screen's frame rate. |

### State

| Property | Type | Description |
|---|---|---|
| `active` | `bool` | Read-only. True while a capture stream is running. |
| `suspended` | `bool` | Read-only. True after Cleave stopped itself because the audio went quiet. See [Auto-suspend](#auto-suspend). |

## Methods

| Method | Description |
|---|---|
| `startCapture()` | Connects to PipeWire and starts analysing. Returns `true` on success. On failure it returns `false` and emits `error`. Calling it again restarts the stream. |
| `stopCapture()` | Stops the stream and clears the buffers. `active` and `suspended` both become `false`. |
| `resumeCapture()` | Restarts the capture and emits `suspendedChanged` if it had been suspended. Use this to wake Cleave up after an auto-suspend. |
| `reset()` | Zeroes all levels and clears the pending audio, without touching the stream. |

## Signals

| Signal | Description |
|---|---|
| `dataChanged()` | New levels are available. Emitted for `magnitudes`, `peaks` and `fps`. |
| `activeChanged()` | `active` changed. |
| `suspendedChanged()` | `suspended` changed. |
| `error(string msg)` | Something failed: creating the PipeWire loop or stream, connecting, or a stream error while running. |

The other `...Changed` signals (`bandCountChanged`, `smoothingChanged`, `peakDecayChanged`, `silenceThresholdChanged`, `sourceChanged`, `debugModeChanged`) work as usual.

## How it works

- Cleave asks PipeWire for a mono, 32-bit float stream at 48 kHz.
- Audio is cut into windows of 2048 samples, shaped with a Hann window and run through an FFT. Windows overlap by half, so you get up to about 47 updates per second.
- The bands are spaced logarithmically from about 70 Hz to about 16 kHz, so the bass gets as much room as the treble. Higher bands are boosted slightly to even out the natural tilt of music.
- Each band is smoothed with `smoothing` and tracked with a falling peak.

The first few bands are the bass, which is what makes a kick drum thump.

## Choosing a source

By default Cleave captures the **default output**, so it reacts to whatever you are playing. To pick something else, set `source`:

```qml
Cleave {
    source: "alsa_output.pci-0000_00_1f.3.analog-stereo.monitor"
}
```

- A name ending in `.monitor` means "capture what this output is playing".
- Any other name targets that node directly, for example a microphone.

To find the names on your system, run `pactl list short sources` or `pw-cli ls Node`.

## Auto-suspend

To avoid burning CPU on silence, Cleave stops itself after the sound has been quiet for 3 seconds:

1. The sound stops. The levels fall to zero at the rate set by `peakDecay`.
2. After 3 seconds the stream is closed, `active` becomes `false` and `suspended` becomes `true`.
3. **Nothing wakes it up automatically.** You have to call `resumeCapture()` when audio comes back.

The usual way is to listen for your player's playing state:

```qml
Cleave {
    id: cleave
    Component.onCompleted: startCapture()
}

Connections {
    target: player

    function onPlayingChanged() {
        if (player.playing && cleave.suspended)
            cleave.resumeCapture()
    }
}
```

Here `player` is whatever you already use to know that music is playing, such as an MPRIS player.

## More snippets

### Handling errors

`startCapture()` returns `false` when it fails, and `error` carries the reason. This also covers stream errors that happen while running.

```qml
Cleave {
    onError: msg => console.warn("Cleave:", msg)
    Component.onCompleted: {
        if (!startCapture())
            console.warn("could not start capture")
    }
}
```

### Calmer, slower response

```qml
Cleave {
    smoothing: 0.9
    peakDecay: 0.005
    Component.onCompleted: startCapture()
}
```

### Start and stop with a button

```qml
Cleave {
    id: cleave
}

Button {
    text: cleave.active ? "Stop" : "Start"
    onClicked: cleave.active ? cleave.stopCapture() : cleave.startCapture()
}
```

## Notes

!!! warning "Scale the levels yourself"
    `magnitudes` are not clamped to 0..1. How big they get depends on the volume and the content, so apply your own gain and clamp, as the example does with `* 5`. Try a few values until your visual looks right.

!!! warning "Read `magnitudes` once per frame"
    `magnitudes` and `peaks` build a new list every time you read them. In a paint handler, copy the list once (`const m = cleave.magnitudes`) and index the copy, rather than writing `cleave.magnitudes[i]` inside a loop. If you use a `Repeater`, give it `model: cleave.bandCount` instead of binding it to `magnitudes`, which can rebuild every delegate on every update.

- **Repainting:** call `requestPaint()` from `onDataChanged`, as the example does. Updates come at the analysis rate, up to about 47 per second.
- **`startCapture()` vs `resumeCapture()`:** after an auto-suspend, use `resumeCapture()`. Calling `startCapture()` works too, but it does not emit `suspendedChanged`, so bindings on `suspended` would not update.
- **Changing `source` while running** has no effect until the next `startCapture()`.
- **Changing `bandCount`** zeroes the levels, so it is best set once up front.
- **Silent frames are skipped:** raise `silenceThreshold` if background hiss keeps your visual twitching.
- **Threading:** audio is captured on the PipeWire thread and handed to the main thread, so all signals and properties are safe to use from QML.
- **Many bands cost more:** every `dataChanged` re-evaluates anything bound to `magnitudes`, so keep `bandCount` reasonable.
- **Requirements:** PipeWire (`pipewire` on Arch). The AUR packages pull it in for you.

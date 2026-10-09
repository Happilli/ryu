# WavyProgress

A progress indicator that can be a straight bar or a ring, with an optional animated wave. With `amplitude: 0` it is a plain flat ring or bar.

```qml
import Ryu.Extraa

WavyProgress {
    width: 38
    height: 38
    style: WavyProgress.Circular
    indeterminate: false
    progress: 0.7
}
```

<!-- screenshot: linear + circular, wavy and flat -->

## Properties

| Property | Type | Default | Description |
|---|---|---|---|
| `style` | `Style` | `Linear` | `Linear` or `Circular`. |
| `progress` | `real` | `0.0` | Value from 0 to 1. Clamped. Ignored while `indeterminate` is true. |
| `indeterminate` | `bool` | `true` | Plays a looping animation instead of showing `progress`. |
| `color` | `color` | `#6750A4` | Color of the filled part. |
| `trackColor` | `color` | `#E8DEF8` | Color of the unfilled part. |
| `amplitude` | `real` | `3.0` | Wave height in pixels. `0` gives a flat line. |
| `wavelength` | `real` | `28.0` | Distance between wave peaks in pixels. Minimum 4. |
| `waveSpeed` | `real` | `0.8` | Wave travel speed in cycles per second. `0` freezes the wave. |
| `strokeWidth` | `real` | `4.0` | Line thickness. Minimum 0.5. |
| `gap` | `real` | `4.0` | Space between the filled part and the track. Minimum 0. |

## Enums

**`Style`**: `Linear`, `Circular`

## Signals

Only the automatic `<property>Changed` signals. There are no custom signals.

## Examples

### Flat ring

```qml
WavyProgress {
    width: 48
    height: 48
    style: WavyProgress.Circular
    indeterminate: false
    progress: 0.7
    amplitude: 0
    waveSpeed: 0
    strokeWidth: 4
    gap: 0
    color: "#6750A4"
    trackColor: "#E8DEF8"
}
```

### Wavy bar

```qml
WavyProgress {
    width: 240
    height: 16
    style: WavyProgress.Linear
    indeterminate: false
    progress: 0.4
    amplitude: 3
    wavelength: 28
    waveSpeed: 0.8
}
```

### Loading spinner

```qml
WavyProgress {
    width: 48
    height: 48
    style: WavyProgress.Circular
}
```

### Animated progress

```qml
WavyProgress {
    id: bar
    width: 240
    height: 16
    indeterminate: false

    NumberAnimation on progress {
        from: 0
        to: 1
        duration: 4000
        loops: Animation.Infinite
    }
}
```

## Notes

!!! tip "Static is free"
    The item only subscribes to window frames while something is moving: `indeterminate` is true, or `amplitude > 0` and `waveSpeed != 0`. A flat ring with `amplitude: 0` and `waveSpeed: 0` costs nothing while idle.

!!! note "Defaults animate"
    `indeterminate` defaults to `true`, so a bare `WavyProgress {}` spins forever. Set `indeterminate: false` to show `progress`.

- **Circular layout:** the ring starts at 12 o'clock and fills clockwise. The radius is `min(width, height) / 2 - strokeWidth / 2 - amplitude`, so a larger amplitude shrinks the ring to keep the wave inside the item.
- **Circular waves:** the number of waves around the ring is `round(circumference / wavelength)`, so `wavelength` is approximate.
- **Linear amplitude** is clamped so the wave never leaves the item's height.
- **Colors** default to the M3 purple palette, so always bind them to your theme.

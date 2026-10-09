# MaterialSlider

A Material 3 expressive slider with a floating handle, segmented track and an optional value bubble. It works horizontally or vertically.

```qml
import Ryu.Extraa

MaterialSlider {
    width: 240
    value: 0.5
    onMoved: console.log(value)
}
```

<!-- screenshot: horizontal and vertical, with and without stops -->

## Properties

### Value

| Property | Type | Default | Description |
|---|---|---|---|
| `from` | `real` | `0.0` | Minimum value. |
| `to` | `real` | `1.0` | Maximum value. |
| `value` | `real` | `0.0` | Current value, clamped to `from`..`to`. |
| `stepSize` | `real` | `0.0` | Snap increment. `0` means continuous. |
| `pressed` | `bool` | | Read-only. True while dragging. |

### Layout and appearance

| Property | Type | Default | Description |
|---|---|---|---|
| `orientation` | `Orientation` | `Horizontal` | `Horizontal` or `Vertical`. |
| `sizeStyle` | `SizeStyle` | `ExtraSmall` | Preset track and handle sizes (table below). |
| `showStops` | `bool` | `true` | Dots along the track. Dots only appear for each step when `stepSize > 0`; otherwise just the end dot. |
| `showValueIndicator` | `bool` | `true` | Bubble with the value, shown while pressed. |
| `decimals` | `int` | `-1` | Decimals in the bubble. `-1` picks automatically from `stepSize` or the range. |
| `trackHeight` | `real` | from style | Overrides the preset. |
| `handleWidth` | `real` | `4` | Handle thickness. It narrows to half while pressed. |
| `handleHeight` | `real` | from style | Overrides the preset. |
| `handleGap` | `real` | `6` | Space between the handle and each track segment. |
| `outerRadius` | `real` | `trackHeight / 2` | Corner radius of the track ends. |
| `innerRadius` | `real` | `2` | Corner radius next to the handle. |
| `stopSize` | `real` | `4` | Dot diameter. |
| `borderWidth` | `real` | `0` | Outline on the track segments. |

### Icon

| Property | Type | Default | Description |
|---|---|---|---|
| `iconText` | `string` | empty | Glyph drawn inside the active segment. Needs `iconFont`. |
| `iconFont` | `font` | | Font for `iconText` (Nerd Font or Material Symbols). |
| `iconSource` | `url` | empty | Image alternative. It is tinted with `iconColor`. |
| `iconColor` | `color` | `#FFFFFF` | Icon tint. |
| `iconSize` | `real` | `24` | Icon size in pixels. |

### Colors

| Property | Default | Description |
|---|---|---|
| `color` | `#6750A4` | Active (filled) segment. |
| `trackColor` | `#E8DEF8` | Inactive segment and dots on the active side. |
| `handleColor` | `#6750A4` | The handle. |
| `borderColor` | transparent | Used with `borderWidth`. |
| `indicatorColor` | `#322F35` | Value bubble background. |
| `indicatorTextColor` | `#F5EFF7` | Value bubble text. |

## Enums

**`Orientation`**: `Horizontal`, `Vertical`

**`SizeStyle`**: `ExtraSmall`, `Small`, `Medium`, `Large`, `ExtraLarge`

| SizeStyle | Track height | Handle height |
|---|---|---|
| `ExtraSmall` | 16 | 44 |
| `Small` | 24 | 44 |
| `Medium` | 40 | 52 |
| `Large` | 56 | 68 |
| `ExtraLarge` | 96 | 108 |

## Signals

| Signal | Description |
|---|---|
| `moved()` | The user changed the value (mouse or keyboard). |
| `valueChanged()` | The value changed from any source. |
| `pressedChanged()` | Drag started or ended. |


## Examples

### Basic horizontal slider

```qml
MaterialSlider {
    width: 240
    value: 0.5
}
```

### Vertical slider

```qml
MaterialSlider {
    orientation: MaterialSlider.Vertical
    sizeStyle: MaterialSlider.Medium
    showValueIndicator: false
    value: 0.7

    Component.onCompleted: {
        width = 52
        height = 300
    }
}
```

### Stepped slider with a value bubble

```qml
MaterialSlider {
    width: 280
    from: 0
    to: 10
    stepSize: 1
    sizeStyle: MaterialSlider.Small
    value: 5
}
```

### Reacting to the user

```qml
Column {
    spacing: 12

    MaterialSlider {
        id: slider
        width: 240
        onMoved: label.text = "Value: " + value.toFixed(2)
    }

    Text {
        id: label
        text: "Value: 0.00"
    }
}
```

### Custom colors

```qml
MaterialSlider {
    width: 240
    value: 0.6
    color: "#a0ec4c"
    handleColor: "#a0ec4c"
    trackColor: "#32362b"
}
```

## Notes

!!! warning "Set the size after the orientation"
    Changing `orientation` swaps `width` and `height`. If you bind a size and also set `orientation: Vertical`, the swap can fight your values. Setting `width` and `height` in `Component.onCompleted`, as the example does, avoids this.

!!! note "Indicator takes space"
    With `showValueIndicator: true`, the item reserves 52 px on the cross axis for the bubble, which affects its implicit size. Turn it off when the slider sits in a tight layout.

- **Vertical direction:** the value grows upward, so the bottom is `from` and the top is `to`.
- **`moved` vs `valueChanged`:** use `onMoved` to react to the user only. Setting `value` from code emits `valueChanged` but not `moved`, which avoids feedback loops when writing back to a service.
- **Binding is kept:** dragging does not break a `value: ...` binding, but any change of the bound source overwrites the dragged value. Pair the binding with `onMoved` to write back.
- **Programmatic `value`** is clamped but not snapped to `stepSize`. Only user input snaps.
- **Keyboard:** the slider takes focus on click or tab. Arrow keys step by `stepSize`, or by 1/20 of the range when continuous. `Home` and `End` jump to the ends.
- **Icon visibility:** the icon only draws when `trackHeight >= iconSize` and the active segment is long enough to hold it.
- **Colors** default to the M3 purple palette, so always bind them to your theme.

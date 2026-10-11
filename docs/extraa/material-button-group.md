# MaterialButtonGroup

A Material 3 expressive button group. A row of buttons that squish and spring when pressed, with optional single or multiple selection. It comes in two variants: spaced-out pills (`Standard`) and a tightly joined strip (`Connected`).

```qml
import Ryu.Extraa

MaterialButtonGroup {
    labels: ["Day", "Week", "Month"]
    selectionMode: MaterialButtonGroup.Single
    currentIndex: 0
    onToggled: (index, checked) => console.log(index, checked)
}
```

<!-- screenshot: Standard and Connected variants, all five sizes -->

## Properties

### Content

| Property | Type | Default | Description |
|---|---|---|---|
| `labels` | `list<string>` | empty | Text for each button. |
| `icons` | `list<string>` | empty | Icon glyph for each button. Needs `iconFont`. |
| `font` | `font` | | Font for the labels. Its pixel size is overridden by `sizeStyle`. |
| `iconFont` | `font` | | Font for the icon glyphs (Nerd Font or Material Symbols). |
| `fontWeight` | `int` | `400` | Label weight, 1 to 1000. |

The number of buttons is the larger of `labels.length` and `icons.length`. A button can have a label, an icon, or both (icon first). Missing entries are just empty, so `icons: ["a", "b"]` with no `labels` gives you two icon-only buttons.

### Selection

| Property | Type | Default | Description |
|---|---|---|---|
| `selectionMode` | `SelectionMode` | `NoSelection` | `NoSelection`, `Single` or `Multiple`. |
| `checkedIndices` | `list<int>` | `[]` | Indices of the checked buttons. Sorted, de-duplicated and clamped for you. |
| `currentIndex` | `int` | `-1` | The first checked index, or `-1` if nothing is checked. Setting it to a negative number clears the selection. |

### Layout and appearance

| Property | Type | Default | Description |
|---|---|---|---|
| `variant` | `Variant` | `Standard` | `Standard` or `Connected`. |
| `sizeStyle` | `SizeStyle` | `Small` | Preset height, padding, icon and font size (table below). |
| `spacing` | `real` | `-1` | Gap between buttons. A negative value picks the M3 default for the variant and size. |
| `fillWidth` | `bool` | `false` | Stretch the buttons to fill the item's width, sharing the extra space equally. |
| `borderWidth` | `real` | `0` | Outline width on every button. |
| `springStiffness` | `real` | `520` | Spring stiffness. Higher is snappier. Minimum `1`. |
| `springDamping` | `real` | `0.5` | Damping ratio, clamped to `0.05` to `2`. Lower is bouncier. |

### Colors

| Property | Default | Description |
|---|---|---|
| `color` | `#E8DEF8` | Background of unchecked buttons. |
| `checkedColor` | `#6750A4` | Background of checked buttons. |
| `textColor` | `#1D192B` | Text and icon color of unchecked buttons. |
| `checkedTextColor` | `#FFFFFF` | Text and icon color of checked buttons. |
| `borderColor` | `#79747E` | Used with `borderWidth`. |

## Enums

**`Variant`**: `Standard`, `Connected`

**`SelectionMode`**: `NoSelection`, `Single`, `Multiple`

**`SizeStyle`**: `ExtraSmall`, `Small`, `Medium`, `Large`, `ExtraLarge`

| SizeStyle | Height | Side padding | Icon size | Font size | Icon gap |
|---|---|---|---|---|---|
| `ExtraSmall` | 32 | 12 | 20 | 14 | 4 |
| `Small` | 40 | 16 | 24 | 14 | 8 |
| `Medium` | 56 | 24 | 24 | 16 | 8 |
| `Large` | 96 | 48 | 32 | 24 | 12 |
| `ExtraLarge` | 136 | 64 | 40 | 32 | 16 |

The default `spacing` for `Standard` is 18, 12, 8, 8 and 8 for the five sizes. For `Connected` it is always 2.

## Variants

- **`Standard`**: separate buttons. Unchecked buttons are full pills, and checked or pressed ones morph to rounded squares.
- **`Connected`**: buttons sit 2 px apart as one strip. The outer ends are always fully round. The inner corners are small, and a checked or pressed button rounds off into a pill.

## Signals

| Signal | Description |
|---|---|
| `clicked(int index)` | The user activated a button, with the mouse or keyboard. Fires in every `selectionMode`. |
| `toggled(int index, bool checked)` | The user changed the checked state of a button. Fires before `clicked`. |
| `checkedIndicesChanged()` | The selection changed from any source. `currentIndex` shares this signal. |

## Methods

| Method | Description |
|---|---|
| `isChecked(int index)` | Returns `true` if that button is checked. |
| `setChecked(int index, bool on)` | Checks or unchecks a button, following the rules of the current `selectionMode`. |

## Examples

### Segmented control (single choice)

```qml
MaterialButtonGroup {
    labels: ["Day", "Week", "Month"]
    selectionMode: MaterialButtonGroup.Single
    currentIndex: 1
    onToggled: (index, checked) => console.log("picked", index)
}
```

### Connected variant

```qml
MaterialButtonGroup {
    variant: MaterialButtonGroup.Connected
    sizeStyle: MaterialButtonGroup.Medium
    labels: ["Left", "Center", "Right"]
    selectionMode: MaterialButtonGroup.Single
    currentIndex: 0
}
```

### Multiple selection

```qml
MaterialButtonGroup {
    labels: ["Bold", "Italic", "Underline"]
    selectionMode: MaterialButtonGroup.Multiple
    checkedIndices: [0, 2]
    onToggled: (index, checked) => console.log(index, checked)
}
```

### Plain buttons (no selection)

With the default `NoSelection`, nothing stays checked and you just get `clicked`:

```qml
MaterialButtonGroup {
    labels: ["Cut", "Copy", "Paste"]
    onClicked: index => console.log("pressed", index)
}
```

### Icons and labels

```qml
MaterialButtonGroup {
    iconFont.family: "Material Symbols Rounded"
    icons: ["", "", ""]   // use the glyphs from your font
    labels: ["Home", "Search", "Settings"]
    selectionMode: MaterialButtonGroup.Single
    currentIndex: 0
}
```

Leave `labels` out for icon-only buttons.

### Fill a container

```qml
MaterialButtonGroup {
    width: parent.width
    fillWidth: true
    labels: ["One", "Two", "Three"]
    selectionMode: MaterialButtonGroup.Single
}
```

### Custom colors and bounce

```qml
MaterialButtonGroup {
    labels: ["A", "B", "C"]
    selectionMode: MaterialButtonGroup.Single
    currentIndex: 0
    color: "#32362b"
    checkedColor: "#a0ec4c"
    textColor: "#e2e3d8"
    checkedTextColor: "#1b2b00"
    springStiffness: 700
    springDamping: 0.35
}
```

## Notes

!!! warning "Declare `labels` and `selectionMode` before the selection"
    QML applies properties in the order you write them. `checkedIndices` and `currentIndex` throw away indices that don't exist yet, and are cleared outright while `selectionMode` is `NoSelection`. If you put `currentIndex: 1` above `labels`, or above `selectionMode: Single`, the selection is silently dropped. Write the content and mode first, as the examples do.

!!! warning "Icons need `iconFont`"
    Icons are text glyphs, not images. Without an `iconFont` that contains them you get empty boxes.

- **Single selection is radio-like:** clicking the already checked button does not uncheck it. It only emits `clicked`. Use `Multiple` if you need toggling off.
- **Programmatic changes are quiet:** setting `checkedIndices`, `currentIndex` or calling `setChecked()` emits `checkedIndicesChanged` but not `clicked` or `toggled`. Use those two signals to react to the user only.
- **Switching mode trims the selection:** moving to `Single` keeps only the first checked button, and moving to `NoSelection` clears it.
- **Signals are queued:** `toggled` and `clicked` fire on the next event loop turn, not inside the mouse event.
- **Press squish:** the pressed button grows by about 28% of the group's height and its neighbors shrink by the same amount, so the total width stays constant. It needs at least two buttons.
- **Size:** the implicit width is the natural width of all buttons plus gaps, and the implicit height comes from `sizeStyle`. The buttons are centered vertically if you make the item taller.
- **No wrapping or clipping:** if you set a width smaller than the natural width, the buttons simply run past the edge. `fillWidth` only ever grows them.
- **Mouse:** a button activates when you release on the same one you pressed. Dragging off before release cancels it. The hit area of each button extends across the gap to its right.
- **Keyboard:** the group takes focus on click or tab. `Left` and `Right` move the focus (no wrapping), and `Space`, `Enter` or `Return` activates the focused button. The focus highlight only shows after keyboard use.
- **Disabled:** `enabled: false` dims the group to 38% opacity and ignores input.
- **Colors** default to the M3 purple palette, so always bind them to your theme.

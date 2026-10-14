# MaterialList

A Material 3 list. Rows can have a leading icon, avatar or image, an overline, a headline, supporting text and trailing text or icon. It supports single and multiple selection, and comes in two variants: a flat `Standard` list and a `Segmented` expressive one where every row is its own rounded card.

```qml
import Ryu.Extraa

MaterialList {
    width: 360
    selectionMode: MaterialList.Single
    model: [
        { headline: "Inbox", supporting: "12 unread", leadingIcon: "\uf01c", trailingText: "12" },
        { headline: "Sent", leadingIcon: "\uf1d8" },
        "-",
        { overline: "Folder", headline: "Archive", supporting: "Everything old", leadingIcon: "\uf187" }
    ]
    iconFont.family: "CaskaydiaCove NF"
    onClicked: (index, item) => console.log(index, item.headline)
}
```

## Model

`model` is a list. Each entry is one of:

| Entry | Meaning |
|---|---|
| `"Text"` | A one-line item with that headline. |
| `"-"` | A divider. |
| `{ ... }` | An item with the keys below. |

| Key | Type | Description |
|---|---|---|
| `headline` | `string` | Main text. |
| `supporting` | `string` | Secondary text below the headline. |
| `overline` | `string` | Small text above the headline. |
| `trailingText` | `string` | Small text on the right edge. |
| `leadingIcon` | `string` | Icon glyph on the left. Needs `iconFont`. |
| `trailingIcon` | `string` | Icon glyph on the right. Needs `iconFont`. |
| `avatarText` | `string` | Text (usually one letter) in a 40 px circle on the left. |
| `image` | `string` | Image URL or path, shown as a 56 px rounded square on the left. |
| `enabled` | `bool` | Default `true`. Disabled rows are dimmed and ignore input. |
| `divider` | `bool` | Turns the entry into a divider. |

If a row has several leading keys, `image` wins over `avatarText`, which wins over `leadingIcon`.

The row height follows the number of text lines: 56 for one line, 72 for two and 88 for three (overline, headline and supporting text all set).

## Properties

| Property | Type | Default | Description |
|---|---|---|---|
| `model` | `list` | empty | The entries (see above). |
| `count` | `int` | | Read-only. Number of entries, dividers included. |
| `variant` | `Variant` | `Standard` | `Standard` or `Segmented`. |
| `selectionMode` | `SelectionMode` | `NoSelection` | `NoSelection`, `Single` or `Multiple`. |
| `checkedIndices` | `list<int>` | `[]` | Checked rows. Sorted, de-duplicated and clamped for you. |
| `currentIndex` | `int` | `-1` | First checked index, or `-1`. |
| `font` | `font` | | Text font. Pixel sizes are fixed by the spec (16, 14 and 11). |
| `iconFont` | `font` | | Font for icon glyphs (Nerd Font or Material Symbols). |
| `springStiffness` | `real` | `520` | Spring stiffness. |
| `springDamping` | `real` | `0.6` | Damping ratio, clamped to 0.05 to 2. |

### Colors

| Property | Default | Description |
|---|---|---|
| `itemColor` | transparent (`Standard`), `#F3EDF7` (`Segmented`) | Background of unchecked rows. |
| `selectedColor` | `#E8DEF8` | Background of checked rows. |
| `textColor` | `#1D1B20` | Headline color. Also the hover tint. |
| `selectedTextColor` | `#1D192B` | Text and icon color of checked rows. |
| `supportingColor` | `#49454F` | Overline, supporting and trailing text. |
| `iconColor` | `supportingColor` | Icon color. |
| `avatarColor` | `#EADDFF` | Avatar circle. |
| `avatarTextColor` | `#21005D` | Avatar letter. |
| `dividerColor` | `#CAC4D0` | Divider line (`Standard` only). |

## Enums

**`Variant`**: `Standard`, `Segmented`

**`SelectionMode`**: `NoSelection`, `Single`, `Multiple`

## Variants

- **`Standard`**: rows touch each other and have no corners. A `"-"` entry draws a 1 px divider inset by 16.
- **`Segmented`**: rows sit 2 px apart. The first and last row of a group have round outer corners (16) and small inner ones (4). A pressed row springs to fully rounded. A `"-"` entry becomes an 8 px gap that splits the list into separate groups.

## Signals

| Signal | Description |
|---|---|
| `clicked(int index, var item)` | The user activated a row. `item` is the original entry from `model`. Fires in every `selectionMode`. |
| `checkedIndicesChanged()` | The selection changed from any source. `currentIndex` shares this signal. |

## Methods

| Method | Description |
|---|---|
| `isChecked(int index)` | Returns `true` if that row is checked. |
| `setChecked(int index, bool on)` | Checks or unchecks a row, following the current `selectionMode`. |

## Keyboard and mouse

| Input | Action |
|---|---|
| `Up`, `Down` | Move the focus, skipping dividers and disabled rows. Wraps around. |
| `Home`, `End` | First or last row. |
| `Enter`, `Return`, `Space` | Activate the focused row. |
| Hover | Adds an 8% tint. |
| Click | A row activates when you release on the same one you pressed. |

## Examples

### Segmented, multiple selection

```qml
MaterialList {
    width: 360
    variant: MaterialList.Segmented
    selectionMode: MaterialList.Multiple
    model: ["Wi-Fi", "Bluetooth", "Airplane mode"]
    checkedIndices: [0]
}
```

### Avatars and trailing text

```qml
MaterialList {
    width: 360
    model: [
        { avatarText: "A", headline: "Ada", supporting: "Online", trailingText: "now" },
        { avatarText: "L", headline: "Linus", supporting: "Away", trailingText: "5 min" }
    ]
}
```

### Inside a scroll view

`MaterialList` is as tall as its rows and does not scroll. Put it in a `Flickable` for long lists:

```qml
Flickable {
    width: 360
    height: 400
    contentHeight: list.height
    clip: true

    MaterialList {
        id: list
        width: parent.width
        model: myItems
    }
}
```

## Notes

!!! warning "Declare `model` and `selectionMode` before the selection"
    Like `MaterialButtonGroup`, indices that don't exist yet are dropped, and everything is cleared while `selectionMode` is `NoSelection`. Write `model` and `selectionMode` above `checkedIndices` or `currentIndex`.

!!! warning "Icons need `iconFont`"
    Icons are text glyphs, not images. Without an `iconFont` that contains them you get empty boxes.

- **Images** are loaded once when `model` is set. Use absolute paths, `~/` paths, or `file://` and `qrc:` URLs. For files next to your QML use `Qt.resolvedUrl("x.png").toString()`.
- **Dividers** are not selectable and are counted in indices.
- **Programmatic changes are quiet:** setting `checkedIndices` or calling `setChecked()` emits `checkedIndicesChanged` but not `clicked`.
- **Single selection is radio-like:** clicking the checked row keeps it checked.
- **Text is elided** with `…` when it doesn't fit. There is no wrapping.
- **Changing `model`** rebuilds the list and drops selected rows that no longer exist.
- **Disabled:** `enabled: false` dims the list to 38% opacity and ignores input.
- **Colors** default to the M3 purple palette, so always bind them to your theme.

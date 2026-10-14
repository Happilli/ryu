# MaterialMenu

A Material 3 expressive context menu. A rounded popup with icons, shortcut hints, separators and a highlight that springs between rows. It opens from the point you click, flips away from screen edges, and supports keyboard navigation.

```qml
import Ryu.Extraa

MaterialMenu {
    id: menu
    model: [
        { label: "Refresh", icon: "\uf021", shortcut: "F5" },
        "-",
        { label: "Delete", icon: "\uf1f8", danger: true }
    ]
    onTriggered: (index, item) => console.log(index, item.label)
}

MouseArea {
    anchors.fill: parent
    acceptedButtons: Qt.RightButton
    onPressed: mouse => menu.popup(mouse.x, mouse.y)
}
```

## Model

`model` is a list. Each entry is one of:

| Entry | Meaning |
|---|---|
| `"Text"` | A plain item with that label. |
| `"-"` | A separator. |
| `{ ... }` | An item with the keys below. |

| Key | Type | Default | Description |
|---|---|---|---|
| `label` | `string` | empty | Item text. |
| `icon` | `string` | empty | Icon glyph, drawn with `iconFont`. If any item has an icon, every row reserves the icon column. |
| `shortcut` | `string` | empty | Hint text aligned to the right edge. Display only. |
| `enabled` | `bool` | `true` | Disabled items are dimmed and cannot be selected. |
| `danger` | `bool` | `false` | Uses `dangerColor` for the row. |
| `separator` | `bool` | `false` | Turns the entry into a separator. |

## Properties

| Property | Type | Default | Description |
|---|---|---|---|
| `model` | `list` | empty | The entries (see above). |
| `open` | `bool` | `false` | Whether the menu is shown. Prefer `popup()` to open it. |
| `currentIndex` | `int` | `-1` | Read-only. The hovered or keyboard-selected row. |
| `closeOnTrigger` | `bool` | `true` | Close the menu when an item is activated. |
| `font` | `font` | | Label font. Its pixel size is overridden (14 px). |
| `fontWeight` | `int` | `500` | Label weight, 1 to 1000. |
| `iconFont` | `font` | | Font for icon glyphs (Nerd Font or Material Symbols). |
| `itemHeight` | `real` | `40` | Height of one row. Minimum 24. |
| `radius` | `real` | `16` | Corner radius of the menu. |
| `minimumWidth` | `real` | `200` | Minimum width. The menu grows to fit its content. |
| `edgeMargin` | `real` | `8` | Minimum distance kept from the parent's edges by `popup()`. |
| `borderWidth` | `real` | `0` | Outline width. |
| `springStiffness` | `real` | `520` | Spring stiffness. |
| `springDamping` | `real` | `0.6` | Damping ratio, clamped to 0.05 to 2. |

### Colors

| Property | Default | Description |
|---|---|---|
| `color` | `#F3EDF7` | Menu background. |
| `textColor` | `#1D1B20` | Label color. Also the hover tint. |
| `iconColor` | `textColor` | Icon color. |
| `shortcutColor` | `#49454F` | Shortcut hint color. |
| `dangerColor` | `#B3261E` | Text, icon and hover tint of `danger` items. |
| `separatorColor` | `#CAC4D0` | Separator line. |
| `borderColor` | `#79747E` | Used with `borderWidth`. |

## Methods

| Method | Description |
|---|---|
| `popup(x, y)` | Opens the menu at a point in the parent's coordinates. Flips and clamps so it stays inside the parent. |
| `close()` | Closes the menu. |

## Signals

| Signal | Description |
|---|---|
| `triggered(int index, var item)` | An item was activated. `item` is the original entry from `model`. |
| `opened()` | The menu opened. |
| `closed()` | The menu closed (item chosen, `Escape`, or `close()`). |

## Keyboard and mouse

| Input | Action |
|---|---|
| `Up`, `Down` | Move the selection, skipping separators and disabled items. Wraps around. |
| `Home`, `End` | First or last item. |
| `Enter`, `Return`, `Space` | Activate the selected item. |
| `Escape` | Close. |
| Hover | Selects the row under the pointer. |
| Click | An item activates when you release on the same one you pressed. |

## Notes

!!! warning "Dismiss on outside click yourself"
    A menu only receives input inside its own bounds. To close it when the user clicks elsewhere, call `close()` from a `MouseArea` behind it, as in the example above.

!!! note "Put it in a full-size parent"
    `popup()` flips and clamps against the parent's size. Give the menu a parent that covers the area you want it to stay inside, such as the whole window or layer surface.

- **Opacity and scale are driven by the open animation.** Don't set your own `opacity` or `scale` on the menu.
- **No submenus, scrolling or checkable items.** A very long model simply makes a tall menu.
- **Changing `model`** rebuilds the menu and resizes it to fit.
- **Icons** are text glyphs, so they need an `iconFont` that contains them.
- **Disabled:** `enabled: false` dims the menu to 38% opacity and ignores input.
- **Colors** default to the M3 purple palette, so always bind them to your theme.

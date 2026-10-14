# MaterialSearchBox

A Material 3 expressive search bar. A pill-shaped, single-line text field with a leading icon, an optional trailing icon, and a clear button that springs in when there is text. It handles its own cursor, selection, clipboard and input method (IME) support.

```qml
import Ryu.Extraa

MaterialSearchBox {
    width: 320
    placeholderText: "Search"
    onTextChanged: console.log(text)
}
```

<!-- screenshot: empty, focused with text, with trailing icon, with border -->

## Properties

### Text

| Property | Type | Default | Description |
|---|---|---|---|
| `text` | `string` | empty | The current text. Setting it from code moves the cursor to the end. |
| `placeholderText` | `string` | empty | Shown while `text` is empty. |
| `font` | `font` | | Font for the text and placeholder. Its pixel size is overridden and always drawn at 16 px. |
| `fontWeight` | `int` | `400` | Text weight, 1 to 1000. |
| `maxLength` | `int` | `0` | Maximum number of characters. `0` means unlimited. Longer text is truncated. |
| `readOnly` | `bool` | `false` | Blocks editing. Selecting and copying still work, and the cursor and clear button are hidden. |
| `cursorPosition` | `int` | | Read-only. Index of the cursor in `text`. |
| `hovered` | `bool` | | Read-only. True while the mouse is over the item. |

### Icons

| Property | Type | Default | Description |
|---|---|---|---|
| `showSearchIcon` | `bool` | `true` | Shows the built-in magnifier on the left. |
| `leadingIconSource` | `url` | empty | Image for the leading icon. Replaces the magnifier. |
| `leadingIconText` | `string` | empty | Glyph for the leading icon. Needs `iconFont`. |
| `trailingIconSource` | `url` | empty | Image for a trailing icon on the right. |
| `trailingIconText` | `string` | empty | Glyph for the trailing icon. Needs `iconFont`. |
| `clearIconSource` | `url` | empty | Image to use instead of the built-in X on the clear button. |
| `iconFont` | `font` | | Font for `leadingIconText` and `trailingIconText` (Nerd Font or Material Symbols). |
| `clearable` | `bool` | `true` | Shows the clear button while there is text. Ignored when `readOnly` is true. |

Icons are drawn at 24 px (the clear icon at 20 px). If both a source and a text glyph are set for the same slot, the source wins.

### Colors and border

| Property | Default | Description |
|---|---|---|
| `color` | `#ECE6F0` | Background of the bar. |
| `textColor` | `#1D1B20` | Text color. Also used for the hover tint. |
| `placeholderColor` | `#49454F` | Placeholder color. |
| `iconColor` | `#49454F` | Tint for every icon, the clear X, and the icon hover layer. |
| `borderColor` | `#79747E` | Outline color while unfocused. |
| `borderWidth` | `0` | Outline width. `0` means no outline. |
| `cursorColor` | `#6750A4` | Text cursor color. Also the outline color while focused. |
| `selectionColor` | `rgba(103, 80, 164, 0.31)` | Selection highlight. |

### Animation

| Property | Type | Default | Description |
|---|---|---|---|
| `springStiffness` | `real` | `520` | Spring stiffness. Higher is snappier. Minimum `1`. |
| `springDamping` | `real` | `0.6` | Damping ratio, clamped to `0.05` to `2`. Lower is bouncier. |

## Signals

| Signal | Description |
|---|---|
| `textChanged()` | `text` changed from any source. |
| `textEdited()` | The text was changed by the user: typing, pasting, deleting, cutting, IME input, or the clear button. Not emitted when you assign `text` from code. |
| `accepted()` | `Enter` or `Return` was pressed. |
| `cleared()` | The text was cleared with the clear button or by calling `clear()`. |
| `leadingClicked()` | The leading icon was clicked. |
| `trailingClicked()` | The trailing icon was clicked. |
| `cursorPositionChanged()` | The cursor moved. |
| `hoveredChanged()` | The mouse entered or left. |

Every other property has the usual `<property>Changed` signal.

## Methods

| Method | Description |
|---|---|
| `clear()` | Empties the text, moves the cursor to 0, and emits `textChanged`, `textEdited` and `cleared`. Does nothing if the text is already empty. |
| `selectAll()` | Selects the whole text. |

## Animations

Three springs run inside the component:

| Spring | What it drives | Visible as |
|---|---|---|
| Hover | An 8% tint of `textColor` over the bar | A soft fade. |
| Focus | The outline color, from `borderColor` to `cursorColor` | A color fade. Only visible when `borderWidth > 0`. |
| Clear button | The scale of the X and its hover circle | A pop-in with overshoot up to 1.4x. |

Only the clear button changes size, so it is the only spring where you can see a bounce. Lower `springDamping` (0.2 to 0.3) to make it more pronounced. The spring runs only while something is moving, so an idle search box costs nothing.

## Keyboard and mouse

### Keyboard

| Keys | Action |
|---|---|
| `Left`, `Right` | Move the cursor. With a selection and no `Shift`, collapses to the start or end of it. |
| `Ctrl+Left`, `Ctrl+Right` | Move by word. |
| `Home`, `End` | Jump to the start or end. |
| `Shift` + any movement | Extend the selection. |
| `Backspace`, `Delete` | Delete one character, or the selection if there is one. |
| `Ctrl+Backspace` | Delete the word to the left of the cursor. |
| `Ctrl+Delete` | Delete the word to the right of the cursor. |
| `Ctrl+A` | Select all. |
| `Ctrl+C`, `Ctrl+X`, `Ctrl+V` | Copy, cut, paste. Line breaks in pasted text become spaces. |
| `Enter`, `Return` | Emit `accepted()`. |

The item takes focus on click or `Tab`.

### Mouse

- **Click** places the cursor. `Shift`+click extends the selection.
- **Drag** inside the text selects.
- **Double click** selects all the text (not a single word).
- **Icons and the clear button** have a 40 px round hit area. They activate when you release on the same one you pressed. Dragging off cancels it.
- The cursor is an I-beam over the text, a pointing hand over icons, and an arrow elsewhere.

## Examples

### Basic

```qml
MaterialSearchBox {
    width: 320
    placeholderText: "Search"
    onAccepted: console.log("search for", text)
}
```

### Filtering a list

```qml
Column {
    spacing: 12

    MaterialSearchBox {
        id: search
        width: 320
        placeholderText: "Search fruits"
    }

    ListView {
        width: 320
        height: 240
        clip: true
        model: ["Apple", "Banana", "Cherry", "Mango"].filter(
            f => f.toLowerCase().includes(search.text.toLowerCase()))
        delegate: Text { text: modelData }
    }
}
```

### Custom icon

```qml
MaterialSearchBox {
    width: 320
    leadingIconSource: Qt.resolvedUrl("./search.svg")
    onLeadingClicked: console.log("leading clicked")
}
```

### Glyph icons

```qml
MaterialSearchBox {
    width: 320
    iconFont.family: "CaskaydiaCove NF"
    leadingIconText: "\ue8b6"
    trailingIconText: "\ue8b8"
    onTrailingClicked: console.log("settings")
}
```

Use the codepoints from your own font.

### Leading and trailing icons together

```qml
MaterialSearchBox {
    width: 360
    leadingIconSource: "search.svg"
    trailingIconSource: "mic.svg"
    onTrailingClicked: console.log("start voice search")
}
```

With a trailing icon, the clear button sits to its left.

### Outline that reacts to focus

```qml
MaterialSearchBox {
    width: 320
    borderWidth: 2
    borderColor: "#CAC4D0"
    cursorColor: "#6750A4"
}
```

The outline fades from `borderColor` to `cursorColor` when focused.

### Expand on focus

```qml
MaterialSearchBox {
    width: activeFocus ? 420 : 260

    Behavior on width {
        SpringAnimation { spring: 3.5; damping: 0.18; epsilon: 0.25 }
    }
}
```

The component does not resize itself, so the width bounce comes from a normal QML `SpringAnimation`.

### Bouncier clear button

```qml
MaterialSearchBox {
    width: 320
    springStiffness: 700
    springDamping: 0.25
}
```

### Limited and read-only

```qml
MaterialSearchBox {
    width: 240
    maxLength: 12
    placeholderText: "12 characters max"
}

MaterialSearchBox {
    width: 240
    readOnly: true
    text: "You can select me"
}
```

### Programmatic control

```qml
MaterialSearchBox {
    id: box
    width: 320
}

Row {
    Button { text: "Fill"; onClicked: box.text = "hello" }
    Button { text: "Select"; onClicked: box.selectAll() }
    Button { text: "Clear"; onClicked: box.clear() }
}
```

### Custom colors

```qml
MaterialSearchBox {
    width: 320
    color: "#32362b"
    textColor: "#e2e3d8"
    placeholderColor: "#a4a798"
    iconColor: "#a4a798"
    cursorColor: "#a0ec4c"
    selectionColor: "#40a0ec4c"
}
```

## Notes

!!! warning "Icons are tinted"
    `leadingIconSource`, `trailingIconSource` and `clearIconSource` are drawn as a mask and filled with `iconColor`. A multicolor image becomes a single-color silhouette, so use simple monochrome icons.

!!! warning "SVG needs the Qt SVG plugin"
    Loading `.svg` files needs the Qt SVG image plugin (`qt6-svg` on Arch). Without it you get `failed to load icon` in the console even when the path is correct.

!!! warning "Glyph icons need `iconFont`"
    `leadingIconText` and `trailingIconText` are text glyphs. Without an `iconFont` that contains them you get empty boxes.

- **Image paths:** `*IconSource` accepts `file://` and `qrc:` URLs, absolute paths, `~/` paths, and paths relative to the QML file. If an icon does not show, check the console for `could not resolve icon` or `failed to load icon`.
- **Icon priority:** for each slot, a source beats a text glyph, which beats the built-in icon. The leading slot shows if `showSearchIcon`, `leadingIconText` or `leadingIconSource` is set. Set `showSearchIcon: false` with no leading icon to remove it entirely.
- **Trailing and clear layout:** the clear button moves left by 40 px when a trailing icon is present. The text area always keeps room for the clear button while `clearable` is true, even when it is hidden.
- **`textEdited` vs `textChanged`:** use `onTextEdited` to react to the user only. Setting `text` from code emits `textChanged` but not `textEdited`, which avoids feedback loops. `clear()` counts as a user edit.
- **Setting `text`:** the cursor moves to the end, any IME composition is dropped, and `maxLength` is applied.
- **Size:** the bar is always 56 px tall with fully round ends. The implicit size is 360 x 56. If you make the item taller, the bar is centered vertically and clicks outside the bar are ignored.
- **Single line:** the text scrolls horizontally to keep the cursor visible and is clipped to the text area. There is no wrapping.
- **Font size:** `font.pixelSize` and `font.pointSize` are overridden. The text is always 16 px. Family, weight and style still apply.
- **IME:** composition text is shown underlined and committed on confirmation, so CJK and other input methods work.
- **Disabled:** `enabled: false` dims the item to 38% opacity and ignores all input.
- **Not a full `TextInput`:** there is no undo/redo, context menu, validator, password mode, or `Escape` handling. Double click selects all, not one word.
- **Colors** default to the M3 purple palette, so always bind them to your theme.

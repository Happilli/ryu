# MaterialShape

A filled shape from a set of 35 Material 3 Expressive shapes. Changing `shape` morphs the outline into the new one on a spring. It can also hold an image or a glyph, and it has a built-in loading mode that cycles through shapes.

```qml
import Ryu.Shapes

MaterialShape {
    width: 96
    height: 96
    shape: MaterialShape.Cookie9Sided
    color: "#6750A4"
}
```

<!-- screenshot: a grid of all 35 shapes -->

## Properties

### Shape and appearance

| Property | Type | Default | Description |
|---|---|---|---|
| `shape` | `Type` | `Circle` | Which shape to draw. Changing it morphs to the new shape. |
| `color` | `color` | `#6750A4` | Fill color. |
| `border.width` | `real` | `0` | Outline thickness. The shape is inset by half of it so the stroke stays inside the item. |
| `border.color` | `color` | transparent | Outline color. |
| `animated` | `bool` | `true` | When false, shape changes snap instead of morphing. |
| `morphing` | `bool` | | Read-only. True while a morph is running. |
| `springStiffness` | `real` | `420` | Morph spring stiffness. Minimum 1. |
| `springDamping` | `real` | `0.6` | Morph damping ratio, clamped to 0.05..2. Lower values bounce more. |

### Loading

| Property | Type | Default | Description |
|---|---|---|---|
| `loading` | `bool` | `false` | Cycles through shapes forever, rotating a step each time. |
| `loadingGap` | `int` | `120` | Pause in ms between a morph finishing and the next one starting. |
| `loadingKick` | `real` | `90` | Degrees added to the rotation target on every step. Can be negative. |
| `loadingAllShapes` | `bool` | `false` | Cycle through all 35 shapes in enum order. |
| `loadingSequence` | `list<int>` | empty | Custom order, using `MaterialShape.<Shape>` values. |

If none of the sequence options are set, the loop uses a curated 7-shape order: `SoftBurst`, `Cookie9Sided`, `Pentagon`, `Pill`, `Sunny`, `Cookie4Sided`, `Oval`.

### Contained

| Property | Type | Default | Description |
|---|---|---|---|
| `contained` | `bool` | `false` | Draws a circle behind the shape and shrinks the shape to fit inside it. |
| `containerColor` | `color` | `#D0BCFF` | Color of the circle. |
| `containedScale` | `real` | `0.62` | Shape size relative to the circle, clamped to 0.1..1. |

### Image

| Property | Type | Default | Description |
|---|---|---|---|
| `source` | `url` | empty | Image clipped to the shape. |
| `fillMode` | `FillMode` | `Crop` | `Crop`, `Fit`, or `Stretch`. |

### Glyph

| Property | Type | Default | Description |
|---|---|---|---|
| `glyph` | `string` | empty | Text or icon glyph drawn in the center. Needs `glyphFont` for icons. |
| `glyphFont` | `font` | | Font for the glyph (Nerd Font or Material Symbols). |
| `glyphColor` | `color` | `#FFFFFF` | Glyph color. |
| `glyphSize` | `real` | `24` | Glyph size in pixels. |

## Enums

**`FillMode`**: `Crop`, `Fit`, `Stretch`

**`Type`**: the 35 shapes, in this order (also the order used by `loadingAllShapes`):

| # | Shape | # | Shape | # | Shape |
|---|---|---|---|---|---|
| 0 | `Circle` | 12 | `Pentagon` | 24 | `Burst` |
| 1 | `Square` | 13 | `Gem` | 25 | `SoftBurst` |
| 2 | `Slanted` | 14 | `Sunny` | 26 | `Boom` |
| 3 | `Arch` | 15 | `VerySunny` | 27 | `SoftBoom` |
| 4 | `Fan` | 16 | `Cookie4Sided` | 28 | `Flower` |
| 5 | `Arrow` | 17 | `Cookie6Sided` | 29 | `Puffy` |
| 6 | `SemiCircle` | 18 | `Cookie7Sided` | 30 | `PuffyDiamond` |
| 7 | `Oval` | 19 | `Cookie9Sided` | 31 | `PixelCircle` |
| 8 | `Pill` | 20 | `Cookie12Sided` | 32 | `PixelTriangle` |
| 9 | `Triangle` | 21 | `Ghostish` | 33 | `Bun` |
| 10 | `Diamond` | 22 | `Clover4Leaf` | 34 | `Heart` |
| 11 | `ClamShell` | 23 | `Clover8Leaf` | | |

## Signals

| Signal | Description |
|---|---|
| `shapeChanged()` | `shape` changed. Fires when a morph starts, not when it ends. |
| `morphingChanged()` | A morph started or finished. |

Every other property has the usual `<property>Changed` signal.

## Examples

### Static shape

```qml
MaterialShape {
    width: 64
    height: 64
    shape: MaterialShape.Heart
    color: "#B3261E"
}
```

### Morph on click

```qml
MaterialShape {
    id: s
    width: 96
    height: 96
    shape: MaterialShape.Circle

    TapHandler {
        onTapped: s.shape = s.shape === MaterialShape.Circle
                  ? MaterialShape.Flower
                  : MaterialShape.Circle
    }
}
```

### Loading indicator

```qml
MaterialShape {
    width: 48
    height: 48
    loading: true
}
```

### Contained loading indicator

```qml
MaterialShape {
    width: 48
    height: 48
    loading: true
    contained: true
    containedScale: 0.8
    color: "#4F378B"
    containerColor: "#D0BCFF"
}
```

### Loading through every shape

```qml
MaterialShape {
    width: 96
    height: 96
    loading: true
    loadingAllShapes: true
}
```

### Custom loading sequence

```qml
MaterialShape {
    width: 96
    height: 96
    loading: true
    loadingSequence: [
        MaterialShape.Circle,
        MaterialShape.Heart,
        MaterialShape.Flower,
        MaterialShape.Cookie12Sided
    ]
}
```

### Image avatar

```qml
MaterialShape {
    width: 96
    height: 96
    shape: MaterialShape.Cookie9Sided
    source: "avatar.png"
    fillMode: MaterialShape.Crop
    border.width: 3
    border.color: "#6750A4"
}
```

### Glyph

```qml
MaterialShape {
    width: 64
    height: 64
    shape: MaterialShape.Sunny
    glyph: "\uf0c7"
    glyphFont.family: "Symbols Nerd Font"
    glyphSize: 28
}
```

## Notes

!!! warning "Always set a size"
    `MaterialShape` has no implicit size. Without `width` and `height` (or a layout that provides them) it draws nothing, or fills whatever the parent gives it.

!!! note "Shapes fill 90% of the item"
    The outline is drawn at 90% of the smaller side, leaving room for the spring overshoot during a morph. Keep that in mind when aligning it with other items.

- **Contained size:** in contained mode the shape's extent is `0.9 × containedScale` of the circle. For a tight fit try `0.8` to `0.9`. Near `1.0`, spiky shapes like `Sunny` and `Burst` can touch the edge while they rotate.
- **Loading rotation:** the item rotates itself with `rotation`. A glyph or image is counter-rotated so it stays upright. Don't set your own `rotation` while `loading` is true.
- **Loading priority:** `loadingAllShapes` wins over `loadingSequence`, which wins over the default loop. Invalid numbers in `loadingSequence` are dropped, and an empty or fully invalid list falls back to the default.
- **Hidden items cost nothing:** if the item is hidden or removed from its window, a running morph finishes immediately and the loading timer stops. It resumes when the item is visible again.
- **No animation when hidden:** setting `shape` on an invisible item, or before the component is complete, snaps instead of morphing.
- **First morph cost:** each pair of shapes is aligned the first time it is used and cached, so the first lap of `loadingAllShapes` can hitch slightly on the detailed shapes (`Boom`, `SoftBoom`).
- **Image paths:** `source` accepts `file://` and `qrc:` URLs, absolute paths, `~/` paths, and paths relative to the QML file.
- **Contained border:** when `contained` is true, the border outlines the circle, not the shape.
- **Colors** default to the M3 purple palette, so always bind them to your theme.

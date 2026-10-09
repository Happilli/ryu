# Ryu

> *"Nothing ever works on the first compile. That's what makes the second one feel like a power-up arc."*

Ryu is a set of C++/QML components for Qt 6, built because the existing ones were either ugly or didn't exist. Material 3 styling, springy animations, and widgets that have seen things.

It is a personal project. It works on my machine. Your machine is a side quest.

## What's inside

| Module | What it does | Docs |
|---|---|---|
| **Extraa** | Material 3 widgets: `WavyProgress`, `MaterialSlider`, `MaterialIconButton`, `MaterialButtonGroup` | Partly |
| **Shapes** | `MaterialShape`, with 35 shapes that morph into each other using springs | Soon™ |
| **Cleave** | Audio visualizer. PipeWire in, FFT out, bars go brrr | Soon™ |
| **Clipsh** | Clipboard history on top of `cliphist` and `wl-copy` | Soon™ |
| **Drawness** | A scene-graph drawing item with undo/redo | Soon™ |
| **Sqliter** | SQLite-backed events storage | Soon™ |
| **Warsa** | Nepali (BS) calendar logic | Soon™ |

"Soon™" means the code exists and the documentation is still in its training arc.

## Documented so far

- [**WavyProgress**](extraa/wavy-progress.md): a bar or ring with an optional wave. Set `amplitude: 0` and it becomes a calm, flat ring with no wave.
- [**MaterialSlider**](extraa/material-slider.md): a floating-handle slider, horizontal or vertical, with a value bubble that appears when you grab it.

## Quick start

```bash
git clone https://github.com/Happilli/ryu
cd ryu
./build.sh            # configure + build with CMake and Ninja
./build.sh --clean    # burn the build directory and start over
```

Then, in QML:

```qml
import Ryu.Extraa

MaterialSlider {
    width: 240
    value: 0.5
    onMoved: console.log(value)
}
```

To try a file without an app around it, point the `qml` tool at the build directory:

```bash
qml -I build test.qml
```

## Requirements

You'll need:

- **Qt 6** with Quick and Sql
- **CMake** and **Ninja**
- A font with the icons you want (a Nerd Font or Material Symbols), because icon glyphs without one are just tofu and regret

And, depending on what you use:

- **PipeWire** for Cleave
- **cliphist** and **wl-clipboard** for Clipsh (Wayland only)
- **Python** with `requests` and `beautifulsoup4` for the Warsa month-length scraper

## A note on colors

Every Extraa component defaults to the classic M3 purple. Set your own colors, or enjoy purple forever:

```qml
MaterialSlider {
    color: "#a0ec4c"
    handleColor: "#a0ec4c"
    trackColor: "#32362b"
}
```

## Known quirks

!!! warning "Things that will bite you"
    - `WavyProgress` animates by default (`indeterminate: true`). A bare one spins forever, like a deadline.
    - Changing `MaterialSlider.orientation` swaps width and height. Set the size in `Component.onCompleted` if they start fighting.
    - Icon glyphs need `iconFont` set, or you get empty boxes.

## Contributing

Found a bug? Open an issue. Fixed one? Open a PR. Both are welcome, and neither will be judged harshly, only silently.

## License

BSD 2-Clause. Do what you want, keep the notice, and don't blame me when it breaks.

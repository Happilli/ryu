# Embedding Ryu

Besides installing Ryu system-wide, you can build it **into your own app** as a git submodule. The modules are compiled statically and linked straight into your executable, so users don't need Ryu installed.

This is how [typeShi](https://github.com/RyuZinOh/typeshi-mirror) uses it.

## When to embed

| | Installed (AUR) | Embedded (submodule) |
|---|---|---|
| Users need Ryu installed | Yes | No |
| Ryu version | Whatever is installed | Pinned by your repo |
| Works for | Quickshell configs, `qml` scripts | Standalone Qt 6 apps |
| Rebuild on Ryu update | No | Yes |

## 1. Add the submodule

```bash
git submodule add https://github.com/Happilli/ryu third_party/ryu
git commit -m "add ryu"
```

Your repo records the exact Ryu commit, so upstream changes never reach you until you update the submodule yourself.

## 2. CMake

Set the options **before** `add_subdirectory`, then link the targets.

```cmake
find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick)

set(RYU_MODULES "Shapes" CACHE STRING "" FORCE)
set(RYU_STATIC  ON       CACHE BOOL   "" FORCE)
set(RYU_INSTALL OFF      CACHE BOOL   "" FORCE)
add_subdirectory(third_party/ryu)

qt_add_executable(myapp main.cpp)

target_link_libraries(myapp PRIVATE
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick
    Ryu::Shapes
    Ryu::Shapesplugin
)

qt_import_qml_plugins(myapp)
```

- **`RYU_MODULES`** is a semicolon-separated list. Build only what you use. Valid names: `Cleave`, `Clipsh`, `Sqliter`, `Warsa`, `Shapes`, `Extraa`, `Widgets`. An unknown name stops the configure with `Unknown Ryu module`.
- **`RYU_STATIC` / `RYU_INSTALL`**: when Ryu is added as a subdirectory, these already default to `ON` / `OFF`. The example sets them anyway so the intent is visible. When Ryu is the top-level project the defaults are reversed.
- **Targets:** each module `Name` gives you two aliases, `Ryu::Name` and `Ryu::Nameplugin`. Link both. Without the plugin target the QML types are not registered.
- **`qt_import_qml_plugins`** is required for static builds. It pulls the plugins your QML imports into the executable.
- Several modules: `set(RYU_MODULES "Shapes;Extraa" CACHE STRING "" FORCE)` and link `Ryu::Extraa` and `Ryu::Extraaplugin` as well.

`Extraa`, `Shapes` and `Widgets` depend on an internal `RyuCore` library. It is built and linked for you.

## 3. Use it in QML

Import the module by name, exactly as with the installed version:

```qml
import QtQuick
import Ryu.Shapes

MaterialShape {
    width: 96
    height: 96
    shape: MaterialShape.Cookie7Sided
}
```

Nothing else is needed in `main.cpp`.

## Module requirements

| Module | Extra dependency |
|---|---|
| `Shapes`, `Extraa`, `Widgets`, `Warsa`, `Clipsh` | None beyond Qt Quick |
| `Sqliter` | `Qt6::Sql` (added for you) |
| `Cleave` | `pipewire` (found with `pkg-config`; configure fails if it is missing) |
| `Clipsh` (at runtime) | `cliphist` and `wl-clipboard` |

Only the modules listed in `RYU_MODULES` are checked, so leaving out `Cleave` means no PipeWire needed to build.

## Packaging an embedded app

!!! warning "Source tarballs do not contain submodules"
    Archives that GitHub or Gitea generate for a tag (`.../archive/refs/tags/v1.0.tar.gz`) leave `third_party/ryu` as an **empty directory**. A build from such a tarball fails with:

    ```
    The source directory .../third_party/ryu does not contain a CMakeLists.txt file.
    ```

Pick one of these fixes.

### Option A: `git+` source (PKGBUILD)

```bash
makedepends=('git' 'cmake' 'ninja')
source=("$pkgname::git+https://github.com/you/myapp.git#tag=v$pkgver")
sha256sums=('SKIP')

prepare() {
  cd "$srcdir/$pkgname"
  git submodule update --init --recursive
}
```

`SKIP` is expected for `git+` sources. The pinned tag (or `#commit=`) is the integrity check, and the submodule commit is recorded in your repo.

### Option B: tarball plus a pinned Ryu tarball

Use this if you want real checksums for everything.

```bash
_ryu_commit=<the commit your submodule points at>
source=("$pkgname-$pkgver.tar.gz::https://github.com/you/myapp/archive/refs/tags/v$pkgver.tar.gz"
        "ryu-$_ryu_commit.tar.gz::https://github.com/Happilli/ryu/archive/$_ryu_commit.tar.gz")
sha256sums=('...' '...')   # updpkgsums fills these in

prepare() {
  cp -a "$srcdir/ryu-$_ryu_commit/." "$srcdir/myapp-$pkgver/third_party/ryu/"
}
```

Find the commit with `git ls-tree v1.0 third_party/ryu` (look for mode `160000`). Whenever you bump the submodule, change `_ryu_commit` and re-run `updpkgsums`.

### CI

With GitHub Actions, check out the submodule:

```yaml
- uses: actions/checkout@v5
  with:
    submodules: recursive
```

## Updating Ryu

```bash
git -C third_party/ryu fetch
git -C third_party/ryu checkout <new-commit>
git add third_party/ryu
git commit -m "update ryu"
```

If you package with Option B, update `_ryu_commit` to match.

## Troubleshooting

| Symptom | Cause and fix |
|---|---|
| `does not contain a CMakeLists.txt file` | Submodule not fetched. Run `git submodule update --init --recursive`, or see [Packaging](#packaging-an-embedded-app). |
| `Unknown Ryu module: X` | Typo in `RYU_MODULES`. Names are case-sensitive. |
| `module "Ryu.Shapes" is not installed` at runtime | The plugin target is missing from `target_link_libraries`, or `qt_import_qml_plugins` was not called. |
| `plugin 'Shapesplugin' ... does not exist in the current scope` | The module was not built. Check that it is listed in `RYU_MODULES` before `add_subdirectory`. |
| `qmldir file not found at .../build/<YourModule>` | A harmless message when your own QML module is built with `NO_PLUGIN`. It is not related to Ryu. |
| `Could NOT find WrapVulkanHeaders` | Optional Qt dependency. Ignore it. |

# droidcheck — headless DROID patch problem checker

`droidcheck` runs the **DROID Forge's own patch validator** from the command
line, so you can check `droid.ini` patches without opening the GUI. It reuses
the Forge's real parser and `Patch::updateProblems()`, so its output matches the
problem panel you'd see in the Forge (same firmware: **blue-7**).

This is handy for validating hand-written or generated patches (e.g. the files
in `../../patches/`) in CI or a quick shell loop.

## How it works

The Forge (`Zarkuun/droidforge`, GPLv3) is a Qt GUI app with no CLI. Its
validation logic, however, lives in GUI-free model classes (`parser/`,
`patch/`). `droidcheck` links just those classes plus a tiny driver
(`droidcheck.cpp`) and skips everything visual:

- The `Module`/rackview graphics stack is **not** compiled. The two
  `ModuleBuilder` symbols the checker references are supplied by
  `modulebuilder_stub.cpp` (a faithful `controllerExists()` + a no-op
  `allRegistersOf()`, which is only used by GUI editing, never by the checker).
- `Clipboard` (a `QObject` touched only by a never-called `PatchSection`
  method) is stubbed in `clipboard_stub.cpp` + its moc output.
- `droidfirmware.json` (the circuit/jack/RAM definitions) is embedded as a Qt
  resource via `check_resources.qrc`.

The checker catches the same problems the Forge does, e.g.:
`unknown circuit`, `deprecated circuit`, `duplicate usage of O3 as output`,
`output register used only as input`, invalid registers for the chosen master,
and out-of-memory patches.

## Build

Requires **Qt 6** (`brew install qt`), clang (C++17), and git.

```sh
./build.sh          # clones the Forge source into vendor/ on first run, then builds
```

Output binary: `build/droidcheck`. Neither `build/` nor `vendor/` is committed
(see `.gitignore`).

## Use

```sh
./build/droidcheck ../../patches/*.ini      # check every patch
./build/droidcheck ../../patches/mine-01-clock-divider.ini
```

Exit code = number of patches that have at least one problem (`0` = all clean),
so it works as a CI gate. Note that "deprecated circuit" counts as a problem —
older community patches often trip this on the blue-7 firmware.

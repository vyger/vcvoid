# plugin/

VCV Rack 2 plugin (`vcvoid`) — emulates the DROID master and its circuits.
Not affiliated with or supported by Der Mann mit der Maschine.
Ships two masters — **vcvoid master** (slug `master`, 8-in / 8-out + 4×4 LED
matrix) and **vcvoid master18** (`master18`, 2 gate ins / 8 CV + 4 gate outs) —
the nine controllers (`P2B8`, `P4B2`, `P10`, `S10`, `P8S8`, `B32`, `E4`, `M4`,
`DB8E`), the `G8` gate and `X7` MIDI expanders, and a decorative `Bling` LED
module (registered in `src/plugin.cpp`). The headless engine under `../engine/`
is compiled directly into the plugin (one engine, two builds).

`src/MasterBase.hpp` is the shared master host (engine, patch load/hot-reload,
controller-chain + X7/MIDI feed, `process()`); the concrete masters are thin
subclasses. Expanders derive `src/ChainModule.hpp` and relay the
`droid::chain` protocol (`../engine/src/chain.hpp`). For where-to-touch
walkthroughs see [../docs/code-tour.md](../docs/code-tour.md); for the overall
picture, [../architecture.md](../architecture.md).

## Building

Requires the [VCV Rack SDK](https://vcvrack.com/manual/Building#Setting-up-your-development-environment)
and a C++17 toolchain.

```sh
cd plugin
make RACK_DIR=/path/to/Rack-SDK            # build plugin.dylib
make install RACK_DIR=/path/to/Rack-SDK   # build + copy into the Rack user plugin folder
make clean RACK_DIR=/path/to/Rack-SDK     # remove build artifacts
```

`RACK_DIR` defaults to `../../Rack-SDK`; pass it explicitly if the SDK lives
elsewhere. After `make install`, launch VCV Rack and add the **vcvoid master**
module from the `vcvoid` brand.

Note: the Rack SDK's `compile.mk` forces `-std=c++11`; the Makefile strips that
and re-adds `-std=c++17` after including `plugin.mk` so the engine sources build.

## Attribution

Faceplate art (`plugin/res/faceplates/*.png`) is derived from the
[Zarkuun/droidforge](https://github.com/Zarkuun/droidforge) faceplate renders
(GPL-3, © Der Mann mit der Maschine) with the author's permission, with the
DROID name and DROID/DMMDM branding removed (vcvoid wordmark in
Share Tech Mono, OFL). This plugin is licensed GPL-3.0-or-later.

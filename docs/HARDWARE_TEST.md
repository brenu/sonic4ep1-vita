# PS Vita setup and manual test

This is a development port and does not include the commercial game files. Use a compatible homebrew Vita with the kernel support required by the loader (including kubridge) and VitaShell installed.

## Install and data

1. Build the VPK as described in the repository README and install it with VitaShell.
2. Provide the matching Android game's `libs3e_android.so`, other required extension libraries, APK, and OBB in the private data layout expected by the loader under `ux0:data/sonic4ep1/`.
3. Keep the OBB and extracted game files on the Vita only. Do not commit or distribute them from this project.
4. Launch **Sonic 4 Episode I - WIP** from LiveArea. The loader writes its log to `ux0:data/sonic4ep1/port.log`.

The exact storage layout and callback behavior are still evolving. Check `docs/HARDWARE_RESULTS.md` for the latest tested revision and known issues before treating a build as a release.

## Current controls

| Input | Action |
| --- | --- |
| D-pad / left stick | Move |
| Cross | Jump / select |
| Start / Circle | Pause / back |
| Any gameplay button | Start at the title prompt |
| Touchscreen | Original touch input |

## Report a test

Record the VPK version and whether the test reached the title, entered a level, completed a level, or exited normally. Note subjective smoothness, audio distortion, visual artifacts, freezes, and any exact on-screen error. If it crashes, preserve the new Vita dump and `port.log` from the same run; identify the process and app version before drawing conclusions from a dump.

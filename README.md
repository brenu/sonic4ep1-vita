# Sonic 4 Episode I — PS Vita

A work-in-progress PS Vita loader/compatibility port for the Android release of **Sonic 4 Episode I** (package version 1.5.0). It runs the game's ARMv7 native library in a Vita homebrew environment; it is not an official Sega release.

## Status

The game boots to the title screen and has been played through some levels on a PS Vita. Gameplay performance is below the 30 FPS goal: the best recent stable full-level sample was about 20.8 FPS.

## Repository contents

- `source/`, `lib/`, and `extras/`: loader, compatibility bridges, and vendored dependencies.
- `tools/`: build preflight, device helper, analysis utilities, and host-side regression checks.
- `docs/`: architecture notes, callback coverage, build/device instructions, and chronological test results.
- `lib/`: pinned upstream dependency sources are vendored into the repository; see [third-party notices](docs/THIRD_PARTY_NOTICES.md).

The Android APK, OBB, extracted proprietary libraries, Vita packages, crash dumps, and local analysis/build outputs are not included.

## Build

Build on Linux with a configured VitaSDK toolchain and the libraries/headers listed by `python3 tools/preflight.py`:

```sh
git clone https://github.com/brenu/sonic4ep1-vita.git
cd sonic4ep1-vita
python3 tools/preflight.py
SONIC_PERFORMANCE_PROFILE=ON \
SONIC_HOST_OPT_LEVEL=3 \
SONIC_VITAGL_WVP_ON_GPU=ON \
bash build.sh
```

The VPK is generated at `work/build/sonic4ep1_wip.vpk`. `VITASDK` defaults to `/usr/local/vitasdk`; set it to your SDK path if needed. The preflight reports missing VitaSDK libraries and headers.

### Should I enable `SONIC_PERFORMANCE_PROFILE`?

This is a **build-time** setting, not something to enable on the Vita. If you install a prebuilt VPK, you do not need to set it. When building from source, `build.sh` enables it by default, along with host optimization level 3 and the tested WVP-on-GPU option. This is recommended for playing: an unoptimized debug build measured 16.2 FPS, while the optimized stable build measured 20.8 FPS over a full level. The profile quiets repeated diagnostics but does not disable VitaGL validation. Leave `SONIC_VITAGL_SKIP_ERROR_HANDLING=OFF`; disabling error handling caused GPU-driver crashes in testing. For manual CMake builds, set `SONIC_PERFORMANCE_PROFILE=ON`, `SONIC_HOST_OPT_LEVEL=3`, and `SONIC_VITAGL_WVP_ON_GPU=ON` to get the same player profile.

## Game data setup

The port expects the Android release **1.5.0 / build 6200011**. It does not read an arbitrary APK/OBB layout. Use your own copy of that game's APK and expansion OBB; this repository does not provide them.

Put those two files in this exact layout on the PC, relative to the repository root:

```text
sonic4ep1-vita/
├── Sonic the Hedgehog 4 - Episode I.apk
└── com.sega.sonic4epi/
    └── main.6200011.com.sega.sonic4epi.obb
```

Keep the APK filename, OBB filename, and subfolder exactly as shown. The staging script checks their SHA-256 hashes against the analyzed game revision and stops if they differ. The APK should be 38,851,237 bytes; the OBB should be 86,710,212 bytes.

Run:

```sh
python3 tools/stage_data.py
```

This creates `staging/ux0/data/sonic4ep1/` on the PC. It places the original APK there as `game.apk`, copies the OBB unchanged as `main.6200011.com.sega.sonic4epi.obb`, and extracts **all** `.so` files from the APK's `lib/armeabi/` directory into the same folder. The core library is named `libs3e_android.so`; S3E extension libraries are stored beside it.

Copy the **contents** of `staging/ux0/data/sonic4ep1/` to the Vita folder `ux0:data/sonic4ep1/` using VitaShell USB/FTP or another file-transfer method. Do not copy the outer `staging` directory, rename or unzip the OBB, or put the APK inside another subfolder. On the Vita, the resulting layout should look like:

```text
ux0:data/sonic4ep1/
├── game.apk
├── main.6200011.com.sega.sonic4epi.obb
├── libs3e_android.so
├── libs3eDialog.so
├── libs3eFlurry.so
├── libs3eAPKExpansion.so
└── [other .so files extracted from lib/armeabi/]
```

The loader checks the APK and OBB filenames and sizes at startup. Keep all game data out of Git; ignore rules exclude APKs, OBBs, and native `.so` files. The project does not bypass DRM or modify game data.

## Vita setup and controls

The Vita needs VitaShell, the `kubridge.skprx` kernel plugin enabled, and `libshacccg.suprx` at either `ur0:data/libshacccg.suprx` or `ur0:data/external/libshacccg.suprx`. Install the built VPK with VitaShell and provide the game's matching data using the layout above. VitaCompanion is optional and used only for development-time deployment/log collection. See [hardware setup](docs/HARDWARE_TEST.md).

| Vita input | Game action |
| --- | --- |
| D-pad or left stick | Move |
| Cross | Jump / select |
| Start or Circle | Pause / back |
| Any gameplay button | Start from the title prompt |
| Touchscreen | Original touch input |

## Development

Run the host regression suite with `bash tools/test.sh`. It needs a host C compiler and writes generated test executables only under ignored `work/`.

For automated Vita deployment, set your device address explicitly and follow [AUTOMATED_TESTING.md](docs/AUTOMATED_TESTING.md). Never commit device addresses, private game files, crash dumps, or generated packages.

## Documentation

- [Porting plan](docs/PORTING_PLAN.md)
- [Native analysis](docs/ANALYSIS.md)
- [Java callback coverage](docs/JAVA_CALLBACKS.md)
- [JNI methods](docs/JNI_METHODS.md)
- [Hardware results](docs/HARDWARE_RESULTS.md)
- [Hardware setup and testing](docs/HARDWARE_TEST.md)
- [Automated device testing](docs/AUTOMATED_TESTING.md)
- [Third-party notices](docs/THIRD_PARTY_NOTICES.md)

The long porting and hardware documents are chronological engineering records; the status summary at the top of this README is the current overview.

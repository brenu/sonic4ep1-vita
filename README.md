# Sonic 4 Episode I — PS Vita

A work-in-progress PS Vita loader/compatibility port for the Android release of **Sonic 4 Episode I** (package version 1.5.0, build 6200011). It runs the game's ARMv7 native library in a Vita homebrew environment; it is not an official Sega release.

## Status

The game boots to the title screen and has been played through a complete level on a PS Vita. D-pad and left-stick movement, Cross to jump/select, Start or Circle to pause/back, title-screen button start, touchscreen input, and music have been exercised on hardware. Gameplay performance is below the 30 FPS goal: the best recent stable full-level sample was about 20.8 FPS. Short sound effects have had reports of crackling/distortion. Suspend/resume, video playback, and some extension services remain incomplete.

The last known stable tested build was probe 37 (version 01.36). Later experimental graphics builds were slower; a build with VitaGL error handling disabled caused GPU-driver crashes and should not be used. See [hardware results](docs/HARDWARE_RESULTS.md) for measurements and exact probe history.

## Repository contents

- `source/`, `lib/`, and `extras/`: loader, compatibility bridges, and vendored dependencies.
- `tools/`: build preflight, device helper, analysis utilities, and host-side regression checks.
- `docs/`: architecture notes, callback coverage, build/device instructions, and chronological test results.
- `lib/`: pinned upstream dependency sources are vendored into the repository; see [third-party notices](docs/THIRD_PARTY_NOTICES.md).

The Android APK, OBB, extracted proprietary libraries, Vita packages, crash dumps, and local analysis/build outputs are not included.

## Build

Build on Linux with a configured VitaSDK toolchain and the libraries/headers listed by `python3 tools/preflight.py`:

```sh
git clone https://github.com/YOUR_NAME/sonic4ep1-vita.git
cd sonic4ep1-vita
python3 tools/preflight.py
SONIC_PERFORMANCE_PROFILE=ON \
SONIC_HOST_OPT_LEVEL=3 \
SONIC_VITAGL_WVP_ON_GPU=ON \
bash build.sh
```

The VPK is generated under `work/build/sonic4ep1_wip.vpk`. `VITASDK` defaults to `/usr/local/vitasdk`; set it to your SDK path if needed. The preflight reports missing VitaSDK libraries and headers. The build uses the vendored VitaGL source and its softfp build configuration.

For the locally validated Android revision, place your own APK at `Sonic the Hedgehog 4 - Episode I.apk` and OBB at `com.sega.sonic4epi/main.6200011.com.sega.sonic4epi.obb`, then run `python3 tools/stage_data.py` if you need the private staging layout. These files are ignored by Git. The project does not bypass DRM or modify the game data.

## Vita setup and controls

Install the built VPK with VitaShell and provide the game's matching data under `ux0:data/sonic4ep1/`. A compatible homebrew setup with the required Vita kernel support is necessary; see [hardware setup](docs/HARDWARE_TEST.md). VitaCompanion is optional and used only for development-time deployment/log collection.

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

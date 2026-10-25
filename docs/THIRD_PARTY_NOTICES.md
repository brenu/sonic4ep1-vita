# Third-party code

This project builds on the Android shared-library loader and vendors the following upstream sources at the recorded commits:

| Component | Upstream | Pinned commit | License file |
| --- | --- | --- | --- |
| FalsoJNI | [v-atamanenko/FalsoJNI](https://github.com/v-atamanenko/FalsoJNI) | `083d5a07e025c6dfbb54ac68fd1acf696c23a86c` | `lib/falso_jni/LICENSE` |
| so_util | [Rinnegatamante/so_util](https://github.com/Rinnegatamante/so_util) | `c4732373e33d808cd885dec3c2c75302cc4c739b` | `lib/so_util/LICENSE` |
| FalsoNDK | [elliencode/FalsoNDK](https://github.com/elliencode/FalsoNDK) | `7dceb3bb34de5c71a3c5b98ed30c57a0eb53c73a` | `lib/falso_ndk/LICENSE` |
| vitaGL | [Rinnegatamante/vitaGL](https://github.com/Rinnegatamante/vitaGL) | `9c23758ff17893db63887f95e9a4d9350c986d88` | `lib/vitagl/COPYING` |

The loader template's original license is retained as [`licenses/soloader-boilerplate-LICENSE`](../licenses/soloader-boilerplate-LICENSE). Other vendored directories retain upstream notices where provided. Review each component's license before redistributing a modified binary or source bundle. No project-wide license is declared here.

No Sega game code, APK, OBB, extracted native game library, or game asset is included.

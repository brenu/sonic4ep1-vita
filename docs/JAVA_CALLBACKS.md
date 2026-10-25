> Historical engineering record: entries below reflect the project state at the time they were written and may be superseded. See the repository README for the current status.

# Verified startup Java callbacks

All 80 startup methods and two fields were cached successfully on Vita in checkpoint 01. The rows below describe boot probe 13. Implemented paths include fixed-display/offline capability responses; showError deliberately stops after recording the engine message. Other unfinished feature invocations stop with a named error. The Flurry extension constructor and 13 public analytics methods are also described below; other new extension callbacks still need implementation.

| Class | Name | Signature | Boot probe 13 |
|---|---|---|---|
| LoaderThread | doResume | `()V` | Stops if invoked |
| LoaderThread | doSuspend | `()V` | Stops if invoked |
| LoaderThread | fixOrientation | `(I)V` | Implemented |
| LoaderThread | getOrientation | `()I` | Implemented |
| LoaderThread | touchSetWait | `(I)V` | Stops if invoked |
| LoaderThread | soundInit | `(IZI)I` | PCM format/port initialization; returns actual rate or zero |
| LoaderThread | soundStart | `()V` | Native mixer worker and Vita PCM playback |
| LoaderThread | soundStop | `()V` | Requests stop; defers cleanup until callback exits |
| LoaderThread | soundSetVolume | `(I)V` | Vita channel volume, clamped to 0–100% |
| LoaderThread | getSilentMode | `()Z` | Implemented |
| LoaderThread | getPrivateExternalDir | `()Ljava/lang/String;` | Implemented |
| LoaderThread | getRstDir | `()Ljava/lang/String;` | Implemented |
| LoaderThread | getCacheDir | `()Ljava/lang/String;` | Implemented |
| LoaderThread | getTmpDir | `()Ljava/lang/String;` | Implemented |
| LoaderThread | glInit | `(I)V` | Implemented |
| LoaderThread | glReInit | `()V` | Implemented |
| LoaderThread | glTerm | `()V` | Implemented |
| LoaderThread | hasMultitouch | `()Z` | Implemented |
| LoaderThread | getBatteryLevel | `()I` | Implemented |
| LoaderThread | chargerIsConnected | `()Z` | Implemented |
| LoaderThread | getDeviceId | `()Ljava/lang/String;` | Implemented |
| LoaderThread | getDeviceModel | `()Ljava/lang/String;` | Implemented |
| LoaderThread | getDeviceIMSI | `()Ljava/lang/String;` | Implemented |
| LoaderThread | getDeviceNumber | `()Ljava/lang/String;` | Implemented |
| LoaderThread | getNetworkType | `()I` | Implemented |
| LoaderThread | getNetworkSubType | `()I` | Implemented |
| LoaderThread | launchBrowser | `(Ljava/lang/String;)Z` | Stops if invoked |
| LoaderThread | locationStart | `()Z` | Implemented |
| LoaderThread | locationStop | `()Z` | Implemented |
| LoaderThread | locationGpsData | `()Z` | Implemented |
| LoaderThread | recordAvailable | `()I` | Stops if invoked |
| LoaderThread | recordStart | `(I)I` | Stops if invoked |
| LoaderThread | recordStop | `()I` | Stops if invoked |
| LoaderThread | accelStart | `()V` | Stops if invoked |
| LoaderThread | accelStop | `()V` | Stops if invoked |
| LoaderThread | compassStart | `()V` | Stops if invoked |
| LoaderThread | compassStop | `()V` | Stops if invoked |
| LoaderThread | smsStart | `()Z` | Implemented |
| LoaderThread | smsStop | `()V` | Stops if invoked |
| LoaderThread | clipboardGet | `()Ljava/lang/String;` | Implemented |
| LoaderThread | clipboardSet | `(Ljava/lang/String;)V` | Implemented |
| LoaderThread | networkCheckStart | `()Z` | Implemented |
| LoaderThread | networkCheckStop | `()Z` | Implemented |
| LoaderThread | runRunnable | `(Ljava/lang/Runnable;)V` | Stops if invoked |
| LoaderThread | runOnOSSignal | `()V` | Queues native OS tick on separate dispatcher; hardware pending |
| LoaderThread | acquireMulticastLock | `()Z` | Implemented |
| LoaderThread | releaseMulticastLock | `()Z` | Implemented |
| LoaderThread | sendEmail | `(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Z` | Stops if invoked |
| LoaderThread | getDeviceDpi | `(Z)I` | Implemented |
| LoaderThread | m_MediaPlayerManager | `Lcom/ideaworks3d/marmalade/LoaderThread$MediaPlayerManager;` | Object handle |
| LoaderThread$MediaPlayerManager | audioPlay | `(Ljava/lang/String;IJJI)I` | Stops if invoked |
| LoaderThread$MediaPlayerManager | audioStop | `(I)V` | Stops if invoked |
| LoaderThread$MediaPlayerManager | audioPause | `(I)I` | Stops if invoked |
| LoaderThread$MediaPlayerManager | audioResume | `(I)I` | Stops if invoked |
| LoaderThread$MediaPlayerManager | audioGetPosition | `(I)I` | Stops if invoked |
| LoaderThread$MediaPlayerManager | audioSetPosition | `(II)V` | Stops if invoked |
| LoaderThread$MediaPlayerManager | audioGetStatus | `(I)I` | Stops if invoked |
| LoaderThread$MediaPlayerManager | audioGetDuration | `(I)I` | Stops if invoked |
| LoaderThread$MediaPlayerManager | audioSetVolume | `(II)V` | Stops if invoked |
| LoaderThread$MediaPlayerManager | audioIsPlaying | `(I)Z` | Stops if invoked |
| LoaderThread$MediaPlayerManager | audioGetNumChannels | `()I` | Returns original constant 16 logical stream slots |
| LoaderView | doDraw | `()V` | Software Canvas presentation; skipped in GLES mode |
| LoaderView | glSwapBuffers | `()V` | Implemented |
| LoaderView | vibrateStart | `(J)V` | Stops if invoked |
| LoaderView | vibrateStop | `()V` | Stops if invoked |
| LoaderView | vibrateAvailable | `()Z` | Implemented |
| LoaderView | showError | `(Ljava/lang/String;Ljava/lang/String;I)I` | Captures error and stops |
| LoaderView | backlightOn | `()V` | Implemented |
| LoaderView | getLocale | `()Ljava/lang/String;` | Implemented |
| LoaderView | getInputString | `(Ljava/lang/String;Ljava/lang/String;I)V` | Stops if invoked |
| LoaderView | videoSetVolume | `(I)V` | Idle no-op when no video view exists |
| LoaderView | videoPlay | `(Ljava/lang/String;IIIIIIZJJ)I` | Stops if invoked |
| LoaderView | videoStop | `()V` | Idle no-op; no completion notification |
| LoaderView | videoPause | `()I` | Returns -1 while idle |
| LoaderView | videoResume | `()I` | Returns -1 while idle |
| LoaderView | videoGetStatus | `()I` | Returns idle state 0 |
| LoaderView | videoGetPosition | `()I` | Returns 0 with no video view |
| LoaderView | runOnOSThread | `()V` | Stops if invoked |
| LoaderView | enableRespondingToRotation | `()V` | Implemented |
| LoaderView | m_LoaderKeyboard | `Lcom/ideaworks3d/marmalade/LoaderKeyboard;` | Object handle |
| LoaderKeyboard | setShowOnScreenKeyboard | `(Z)V` | Stops if invoked |
| LoaderKeyboard | getKeyboardInfo | `()I` | Implemented |

PCM callbacks are compiled and host resampling tests pass; probe 05 initialized PCM, but the worker had not started before its callback stop. Hardware playback is pending. Input rates 8–48 kHz are resampled to 48 kHz stereo. Rapid restart while a mixer callback is still stopping reports an explicit error. Streamed MediaPlayer music and complete suspend/resume remain unfinished.

Idle video handlers match the original initial state. videoPlay remains an explicit stop, so active playback is unreachable; update the idle handlers when a video backend is implemented.

## Flurry extension — probe 11

Constructor is empty in the original class. NewObject/NewObjectV/NewObjectA return a retained offline object only for the exact s3eFlurry constructor descriptor. GetObjectClass and global-reference deletion preserve that token. All listed public methods are disabled analytics no-ops; hardware invocation is pending.

| Class | Method | Signature | Behavior |
|---|---|---|---|
| s3eFlurry | <init> | `()V` | Empty constructor, retained offline token |
| s3eFlurry | s3eFlurryCountPageView | `()V` | Analytics disabled; no SDK/network call |
| s3eFlurry | s3eFlurryEndSession | `()V` | Analytics disabled; no SDK/network call |
| s3eFlurry | s3eFlurryEndTimedEvent | `(Ljava/lang/String;Ljava/lang/String;)V` | Analytics disabled; no SDK/network call |
| s3eFlurry | s3eFlurryLogError | `(Ljava/lang/String;Ljava/lang/String;)V` | Analytics disabled; no SDK/network call |
| s3eFlurry | s3eFlurryLogEvent | `(Ljava/lang/String;)V` | Analytics disabled; no SDK/network call |
| s3eFlurry | s3eFlurryLogEventParams | `(Ljava/lang/String;Ljava/lang/String;)V` | Analytics disabled; no SDK/network call |
| s3eFlurry | s3eFlurryLogEventParamsTimed | `(Ljava/lang/String;Ljava/lang/String;)V` | Analytics disabled; no SDK/network call |
| s3eFlurry | s3eFlurryLogEventTimed | `(Ljava/lang/String;)V` | Analytics disabled; no SDK/network call |
| s3eFlurry | s3eFlurrySetAge | `(I)V` | Analytics disabled; no SDK/network call |
| s3eFlurry | s3eFlurrySetSessionReportsOnCloseEnabled | `(Z)V` | Analytics disabled; no SDK/network call |
| s3eFlurry | s3eFlurrySetSessionReportsOnPauseEnabled | `(Z)V` | Analytics disabled; no SDK/network call |
| s3eFlurry | s3eFlurrySetUserID | `(Ljava/lang/String;)V` | Analytics disabled; no SDK/network call |
| s3eFlurry | s3eFlurryStartSession | `(Ljava/lang/String;)V` | Analytics disabled; no SDK/network call |

## APK expansion — probe 12

Seven native-facing descriptors; constructor keeps downloader/UI absent, static state begins 0 and becomes -1 only after installed-file type/size validation or 3 on error. Main OBB size must be 86710212 bytes. Start/stop do nothing without a downloader stub. Constructor dispatch remains class-aware across both extensions.

| Class | Method | Signature | Behavior |
|---|---|---|---|
| s3eAPKExpansion | <init> | `()V` | Retained installed-OBB object |
| s3eAPKExpansion | s3eAPKExpansionGetAbsolutePath | `()Ljava/lang/String;` | raw://ux0:data/sonic4ep1/ |
| s3eAPKExpansion | s3eAPKExpansionGetDownloadState | `()I` | Undefined 0 / no download required -1 / error 3 |
| s3eAPKExpansion | s3eAPKExpansionGetMainExpansionFilename | `()Ljava/lang/String;` | Original version/package main OBB filename |
| s3eAPKExpansion | s3eAPKExpansionInitialize | `(Ljava/lang/String;)V` | Validates regular-file type and exact 64-bit size |
| s3eAPKExpansion | s3eAPKExpansionStart | `()V` | No-op without download stub |
| s3eAPKExpansion | s3eAPKExpansionStop | `()V` | No-op without download stub |

## Dialog extension — probe 13

| Class | Method | Signature | Behavior |
|---|---|---|---|
| s3eDialog | <init> | `()V` | Retained dialog token; dialog IDs passed per call |
| s3eDialog | s3eDialogAlertBox | `(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;I)Z` | Modal Vita message; real user selection sent to registered native callback |

Button indices follow Java order 0/1/2; cancellation is -1. One/two-button system labels show the corresponding game actions in the body. Three-button labels are custom. Concurrent/nested modal support and full asynchronous Android UI lifecycle remain unfinished. Hardware dialog invocation is pending.

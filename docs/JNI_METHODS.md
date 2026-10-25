> Historical engineering record: entries below reflect the project state at the time they were written and may be superseded. See the repository README for the current status.

# JNI table candidates

Static ELF virtual addresses, before relocation. These candidates are not yet a validated runtime dispatch table.

| Method | JNI signature | ELF address |
|---|---|---|
| s3eDebugTraceLine | `(Ljava/lang/String;)V` | `0x3a9b8` |
| s3eDeviceYield | `(I)V` | `0x3ad7c` |
| s3eConfigGet | `(Ljava/lang/String;I)I` | `0x3ac60` |
| s3eConfigGetInt | `(Ljava/lang/String;Ljava/lang/String;[I)I` | `0x3acb4` |
| setPixelsNative | `(II[IZ)V` | `0x3bed8` |
| videoStoppedNotify | `()V` | `0x3bd34` |
| setInputText | `(Ljava/lang/String;)V` | `0x3bfe8` |
| recordAudio | `([SII)V` | `0x3bd50` |
| shutdownNative | `()V` | `0x3ab44` |
| initNative | `()V` | `0x3ad84` |
| suspendAppThreads | `()V` | `0x3bcdc` |
| resumeAppThreads | `()V` | `0x3bcf8` |
| onAccelNative | `(FFF)V` | `0x3a878` |
| onCompassNative | `(IFFF)V` | `0x3d7c0` |
| onMotionEvent | `(IIII)V` | `0x40654` |
| setViewNative | `(Lcom/ideaworks3d/marmalade/LoaderView;)V` | `0x3a9f4` |
| runNative | `(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V` | `0x3c204` |
| audioStoppedNotify | `(I)V` | `0x3bd14` |
| chargerStateChanged | `(Z)V` | `0x3be08` |
| networkCheckChanged | `(Z)V` | `0x3be5c` |
| runOnOSThreadNative | `(Ljava/lang/Runnable;)V` | `0x3ac30` |
| runOnOSTickNative | `()V` | `0x3abc8` |
| signalSuspend | `(Z)Z` | `0x3abcc` |
| signalResume | `(Z)Z` | `0x3abe4` |
| lowMemoryWarning | `()V` | `0x3abfc` |
| onKeyEventNative | `(III)Z` | `0x3ff34` |
| setCharInputEnabledNative | `(Z)V` | `0x3ffbc` |
| generateAudio | `([SI)V` | `0x409e8` |
| onReceiveCallback | `(Ljava/lang/String;Ljava/lang/String;J)V` | `0x21114` |
| locationUpdate | `(IJDDDFFF)V` | `0x40280` |
| locationSatellite | `(IFFIFZ)V` | `0x404b0` |
| onOrientationChangedNative | `()V` | `0x3a82c` |
| setART | `(Z)V` | `0x3a99c` |
| previewCallback | `([BIIII)V` | `0x1be08` |
| ImagePicker_addResultString | `(Ljava/lang/String;)Z` | `0x1f0c0` |
| onMotionEvent | `(IIII)V` | `0x22648` |

# Installation

Recommended layout:

```text
SYS:ZZSpeech/
  ZZSpeechDaemon
  ZZSpeechGUI
  ZZSay
  zspeech_core1.bin
  slt.flitevox
  docs/
```

Copy the DOS handler to:

```text
L:ZZSpeech-Handler
```

Copy the startup script to:

```text
S:ZZSpeech-Start
```

Copy the `ZZSPEECH` mount file to:

```text
SYS:Storage/DOSDrivers/ZZSPEECH
```

This is the recommended setup. It does not auto-mount `ZZSPEECH:` at boot.

Example manual installation:

```shell
MakeDir SYS:ZZSpeech
Copy ZZSpeechDaemon SYS:ZZSpeech/
Copy ZZSpeechGUI SYS:ZZSpeech/
Copy ZZSay SYS:ZZSpeech/
Copy zspeech_core1.bin SYS:ZZSpeech/
Copy slt.flitevox SYS:ZZSpeech/

Copy ZZSpeech-Handler L:
Copy ZZSpeech-Start S:
Copy ZZSPEECH SYS:Storage/DOSDrivers/
```

Start ZZSpeech:

```shell
Execute S:ZZSpeech-Start
```

The startup script starts `ZZSpeechDaemon` and mounts `ZZSPEECH:`.

Advanced users may copy the `ZZSPEECH` mount file to `DEVS:DOSDrivers/` for auto-mounting at boot. This is not the recommended default unless `ZZSpeechDaemon` is also started automatically and the machine always runs the required XACP/Core1-compatible firmware.

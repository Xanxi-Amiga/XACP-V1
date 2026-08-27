# ZZSpeech

ZZSpeech is a speech synthesis service for AmigaOS and the MNT ZZ9000 running Xanxi XACP/Core1-compatible firmware.

It uses the ZZ9000 ARM Cortex-A9 Core1 to run a resident CMU Flite speech engine. The Amiga side provides a resident daemon, an Intuition/GadTools GUI frontend, a CLI client, and a write-only DOS handler.

The 68k sends text, the ARM Core1 synthesizes speech, and audio is played back through AHI. The main ZZ9000 `BOOT.bin` is not modified by ZZSpeech; the speech engine is loaded at runtime as an external Core1 blob.

## Features

- Resident daemon: the voice is loaded once, then speech requests are fast.
- Intuition/GadTools GUI with multi-line text input.
- CLI client for scripts and Shell usage.
- Write-only DOS handler: `echo "hello" >ZZSPEECH:` and `copy text.txt TO ZZSPEECH:`.
- File reading from the GUI / CLI.
- Speech export to standard 16-bit WAV.
- Adjustable speaking speed.
- External CMU Flite `.flitevox` voice support.

## Requirements

- AmigaOS 3.x
- MNT ZZ9000
- Xanxi XACP/Core1-compatible firmware
- AHI
- 68020 or better on the Amiga side

ZZSpeech requires a ZZ9000 running Xanxi XACP/Core1-compatible firmware. It does **not** work with stock MNT firmware 1.13.

Other firmware builds, including Blitterstudio builds, are unsupported unless they explicitly provide a compatible Core1 blob-launch and mailbox interface.

## Basic usage

Start the daemon first:

```shell
Execute S:ZZSpeech-Start
```

Then use the GUI:

```shell
ZZSpeechGUI
```

Or use the CLI:

```shell
ZZSay "Hello from ZZ9000"
ZZSay FILE RAM:text.txt
ZZSay STATUS
ZZSay QUIT
```

If the `ZZSPEECH:` DOS handler is mounted:

```shell
echo "Hello from ZZ9000" >ZZSPEECH:
copy RAM:text.txt TO ZZSPEECH:
```

Before launching another Core1-based application, stop the daemon cleanly:

```shell
ZZSay QUIT
```

Only one Core1 blob can run at a time.

## Installation summary

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

Install the DOS handler in:

```text
L:ZZSpeech-Handler
```

Install the startup script in:

```text
S:ZZSpeech-Start
```

Install the `ZZSPEECH` mount file in:

```text
SYS:Storage/DOSDrivers/ZZSPEECH
```

The recommended setup does not auto-mount `ZZSPEECH:` at boot. Start ZZSpeech with:

```shell
Execute S:ZZSpeech-Start
```

## Voices

The Aminet release includes `slt.flitevox` as the default voice.

Other free Flite voices can be downloaded from:

http://festvox.org/flite/packed/flite-2.1/voices/

ZZSpeech currently supports only `.flitevox` voices smaller than 16 MB.

## Downloads

Binary release on Aminet:

https://aminet.net/package/util/misc/ZZSpeech

## Source code status

ZZSpeech is currently distributed as closed-source freeware. Source code is not included in this repository.

Requests to use, integrate, bundle, or redistribute ZZSpeech as part of another
software, firmware, launcher, distribution, package, or commercial product are
welcome. Please contact Xanxi first.

## License

ZZSpeech is closed-source freeware by Xanxi.

You may freely use and redistribute the original unmodified archive, provided
all documentation, copyright notices, and third-party license notices are kept
intact.

Modified redistribution, rebranding, reverse engineering, repackaging, or
integration of ZZSpeech into another software, launcher, firmware,
distribution, package, commercial product, or derived project is not permitted
without prior authorization.

Requests for such use are welcome by Xanxi. Please contact the author first.

CMU Flite and CMU/FestVox voice data remain under their respective upstream licenses. See `docs/LICENSES/`.

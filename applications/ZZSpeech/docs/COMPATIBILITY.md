# Compatibility

ZZSpeech requires:

- AmigaOS 3.x
- MNT ZZ9000
- Xanxi XACP/Core1-compatible firmware
- AHI
- 68020 or better on the Amiga side

ZZSpeech does not work with stock MNT firmware 1.13.

Other firmware builds, including MidWan builds, are unsupported unless they explicitly provide a compatible Core1 blob-launch and mailbox interface.

Only one Core1 blob can run at a time. While `ZZSpeechDaemon` is active, it owns Core1. Before launching another Core1-based project such as ZZDoom, ZZPicoDrive, Julia, or another ARM blob, stop the daemon:

```shell
ZZSay QUIT
```

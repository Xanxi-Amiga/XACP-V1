# XACP Applications

This directory contains the Amiga-side applications, players, demos and validation tools for the Xanxi ZZ9000 XACP firmware branch.

XACP applications are not all tied to the same firmware build. Some belong to the older MP3 / MP2 line, some validate Core1, and some require the newer XACP v1.5 memory map.

Use each application with the firmware, `zz9000.card` and binaries from its matching release package.

---

## Application overview

| Directory     | Application / component           | Main role                                                 | Firmware baseline |
| ------------- | --------------------------------- | --------------------------------------------------------- | ----------------- |
| `mp3/`        | ZZPlayGUI / MP3 tools             | MP3 / MP2 audio playback through XACP and AHI             | XX16c line        |
| `mp3/`        | `zz9000.engine`                   | AmigaAMP external engine using the ZZ9000 ARM decode path | XX16c line        |
| `mp3/`        | `mpega.library` work              | MPEGA-compatible decode path backed by XACP               | XX16c line        |
| `mpegplayer/` | ZZ-MPEG                           | MPEG-1 / MP2 video player for ZZ9000                      | XX16c line        |
| `ZZMIDI/`     | ZZMIDIPlay                        | GUI SoundFont MIDI player                                 | XX19 / XACP v1.5  |
| `ZZDoom/`     | ZZDoom                            | Doom engine running on ZZ9000 ARM Core1                   | XX18c line        |
| `benchmarks/` | ZZBench GUI                       | 68k / ARM / memory-bandwidth benchmark tool               | XACP validation   |
| `fractals/`   | JuliaV2 / Core1 Julia             | Core1 launch, rendering and clean-return validation       | Core1 validation  |
| `fractals/`   | Mandelbrot / legacy fractal demos | Early XACP / Core1 graphical validation                   | Historical        |

Exact filenames and subdirectory names may vary between release packages.

---

## MP3 / MP2 audio tools

Directory:

```text
mp3/
```

This directory contains the historical XACP audio tools.

It may include:

```text
ZZPlayGUI
ZZMP3StreamPlay
zz9000.engine
mpega.library-related files
icons, docs or helper files
```

Main validated path:

```text
MP3 / MP2 compressed data
XACP stream-control block
ZZ9000 ARM-side decode
PCM ring
AHI playback on AmigaOS
```

The AmigaAMP external engine must keep the filename:

```text
zz9000.engine
```

This is the expected AmigaAMP external-engine name for the XACP playback path.

---

## ZZ-MPEG

Directory:

```text
mpegplayer/
```

ZZ-MPEG is a MPEG-1 / MP2 player for Amiga systems equipped with the ZZ9000.

Status:

```text
ZZ-MPEG 1.0 Advanced Beta
```

Included variants may include:

```text
ZZMpegDirect
ZZMpegStream
ZZMpegPlayer
```

Main role:

```text
MPEG-1 Program Stream input
ARM/Core1 video decode
RTG / Picasso96 display
MP2 audio through XACP
AHI playback
```

ZZ-MPEG remains marked as Advanced Beta while final audio/video synchronization validation is completed.

---

## ZZMIDIPlay

Directory:

```text
ZZMIDI/
```

ZZMIDIPlay is a GUI SoundFont MIDI player for the ZZ9000.

This release documents the GUI player only:

```text
ZZMIDIPlay
ZZMIDIPlay_UserGuide.md
```

Not part of this release package:

```text
ZZMIDIDaemon
ZZMIDIctl
realtime CAMD service
```

Firmware baseline:

```text
XX19
XACP v1.5
```

Main role:

```text
SoundFont MIDI playback
SF2 staging up to 32 MB
MIDI staging up to 6 MB
ARM-side synthesis
AHI playback on the Amiga side
```

---

## ZZDoom

Directory:

```text
ZZDoom/
```

ZZDoom is a Doom port based on `doomgeneric`.

The Doom engine runs on the ZZ9000 ARM Core1 while AmigaOS handles system integration.

Main validated features:

```text
Core1 Doom engine
RTG display
keyboard input
AHI sound effects
CAMD external MIDI
AmigaOS file access
save/load support
clean STOP / return path
```

Public baseline:

```text
XX18c
```

Typical binaries may include:

```text
ZZDoom320
ZZDoom640
```

Use ZZDoom with its matching firmware and `zz9000.card` package unless a newer combination has been explicitly tested.

---

## ZZBench GUI

Directory:

```text
benchmarks/
```

ZZBench GUI is a SysInfo-style validation and benchmarking tool.

It is used to compare and validate:

```text
68k CPU performance
ARM Core1 CPU performance
Amiga Chip RAM bandwidth
Amiga Fast RAM bandwidth
ZZ9000 DDR bandwidth through ARM
ZZ9000 DDR bandwidth through Zorro III
```

ZZBench GUI is not a media player. It is a validation tool for the XACP hardware/software model.

---

## JuliaV2 / Core1 Julia

Directory:

```text
fractals/
```

JuliaV2 is a Core1 validation application.

It proved that a ZZ9000 ARM Core1 blob can be launched from the Amiga side, render or compute independently, communicate through shared DDR, and return cleanly to the firmware runtime.

Validated model:

```text
copy ARM blob to DDR
launch Core1 through ARM_RUN registers
use shared mailbox
render or compute on Core1
STOP cleanly
return to AmigaOS
launch another Core1 application afterwards
```

JuliaV2 is an important non-regression test for Core1 applications.

If another Core1 application stops but JuliaV2 cannot be launched afterwards without a power cycle, the previous application probably did not return Core1 to a clean state.

---

## Mandelbrot / legacy Core1 demos

Directory:

```text
fractals/
```

Older Mandelbrot and fractal demos belong to the historical XACP / Core1 validation line.

They are useful as early examples of:

```text
ARM-side computation
shared DDR communication
framebuffer rendering
basic Amiga-side launch and display logic
```

They should be kept separate from JuliaV2, because JuliaV2 represents the later validated Core1 clean-launch / clean-return model.

---

## Firmware compatibility

Do not assume that every application in this directory works with every firmware build.

General rule:

```text
application binary
matching firmware
matching zz9000.card
matching documentation
```

should come from the same release package whenever possible.

After replacing firmware or `zz9000.card`, fully power off the Amiga before testing.

Mixing unrelated packages can cause:

```text
black screen
silent audio
wrong shared-memory offsets
RTG corruption
missing XACP opcode
crash or lock-up
failure to return cleanly to Workbench
```

---

## XACP v1.5 note

XACP v1.5 is the current protocol / ABI / shared-memory map documentation baseline.

It does not mean that every application here requires XX19.

Examples:

```text
MP3 / MP2 tools      historical XX16c line
ZZ-MPEG              historical XX16c line
ZZDoom               public XX18c baseline
ZZMIDIPlay           XX19 / XACP v1.5
ZZBench GUI          validation tool
JuliaV2              Core1 validation application
Mandelbrot demos     historical Core1 / graphics validation
```

XX19 is the firmware build implementing the public XACP v1.5 baseline.

---

## Applications not included in the current public release

The following projects may exist as development branches, experiments or future roadmap items, but are not documented here as current public applications:

```text
ZZMIDIDaemon
ZZMIDIctl
realtime CAMD ZZMIDI service
ZZSpeech
ZZReSID
PicoDrive
ZZQuake
ZZAudioMixer
```

They should receive their own README files only when they are included in a public release package.

---

## Documentation rule

Each substantial application should have its own README.

Recommended structure:

```text
applications/
  README.md
  mp3/
    README.md
  mpegplayer/
    README.md
  ZZMIDI/
    README.md
    ZZMIDIPlay_UserGuide.md
  ZZDoom/
    README.MD
  benchmarks/
    README.md
  fractals/
    README.md
```

The root `applications/README.md` is only an index. Detailed usage belongs in the application-specific README files.

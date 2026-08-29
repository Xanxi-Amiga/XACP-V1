# ZZ-MPEG

**ZZ-MPEG 1.0 Advanced Beta**
MPEG-1 / MP2 video player for the ZZ9000 on Amiga.

ZZ-MPEG is an ARM-assisted MPEG-1 player for Amiga systems equipped with a ZZ9000. It uses the ZZ9000 ARM/Core1 and XACP path to decode MPEG-1 video and MP2 audio, with video displayed through RTG/Picasso96 and audio played through AHI.

This release provides both a compact direct player and a streaming player for longer files.

---

## Status

ZZ-MPEG is currently marked as:

```text
1.0 Advanced Beta
```

The Core1 video path, MP2 audio path, AHI playback and STOP / return sequence are functional.

The remaining release-critical work is final audio/video synchronization validation and tuning, using demanding reference material such as the Princess Irulan opening sequence from *Dune*.

---

## Firmware baseline

Primary firmware line:

```text
XX16c
```

Related protocol history:

```text
XACP V1 / XACP v1.5 history
```

ZZ-MPEG belongs to the XX16c multimedia line and remains part of the XACP v1.5 release history.

---

## Versions included

### 1. ZZMpegDirect

The direct baseline player.

Features:

* loads the MPEG file directly into memory;
* simple and robust playback path;
* useful as a compatibility / reference player;
* limited to files up to approximately **16 MB**.

Use this version for small clips and baseline testing.

### 2. ZZMpegStream

The streaming player.

Features:

* streams MPEG Program Stream data from disk;
* supports files larger than 16 MB;
* MPEG-1 video playback through ZZ9000 ARM/Core1;
* MP2 audio playback through XACP + AHI;
* 25 fps paced playback;
* safe Core1 shutdown with STOP ACK;
* audio drain before screen closure;
* suitable for longer clips encoded with the recommended profile.

This is the main player for regular use.

---

## Supported format

ZZ-MPEG 1.0 Advanced Beta is designed for clean MPEG-1 Program Stream files.

Recommended format:

```text
Container: MPEG Program Stream (.mpg)
Video:     MPEG-1 video
Size:      320x240 recommended
FPS:       25 fps constant frame rate
Audio:     MP2
Rate:      44.1 kHz stereo
Bitrate:   moderate bitrate recommended
```

Known-good target profile:

```text
Video:     MPEG-1, 320x240, 25 fps CFR, approximately 600-800 kb/s
Audio:     MP2, 44.1 kHz stereo, 128 kb/s
Container: MPEG Program Stream
```

---

## Recommended FFmpeg commands

For 4:3 sources:

```sh
ffmpeg -i input.mp4 -vf "scale=320:240:flags=lanczos,fps=25" -fps_mode cfr -af "aresample=44100:async=1:first_pts=0" -c:v mpeg1video -b:v 600k -maxrate 800k -bufsize 224k -g 15 -bf 0 -c:a mp2 -ar 44100 -ac 2 -b:a 128k -f mpeg output_zzmpeg.mpg
```

For widescreen sources, preserving aspect ratio with black borders:

```sh
ffmpeg -i input.mp4 -vf "scale=320:240:force_original_aspect_ratio=decrease:flags=lanczos,pad=320:240:(ow-iw)/2:(oh-ih)/2,fps=25" -fps_mode cfr -af "aresample=44100:async=1:first_pts=0" -c:v mpeg1video -b:v 600k -maxrate 800k -bufsize 224k -g 15 -bf 0 -c:a mp2 -ar 44100 -ac 2 -b:a 128k -f mpeg output_zzmpeg.mpg
```

---

## Requirements

### Hardware

* Amiga with ZZ9000;
* RTG / Picasso96 compatible display mode;
* audio output configured through AHI.

### Software

* AmigaOS 3.x;
* Picasso96;
* AHI;
* compatible ZZ9000 firmware with XACP / ARM support.

---

## Usage

Run the GUI player from Workbench or Shell:

```text
ZZMpegPlayer
```

Use **Open...** to select an MPEG file, then choose either **Window** or **Full Screen**.

Command-line / direct variants may also be provided:

```text
ZZMpegDirect file.mpg
ZZMpegStream file.mpg
```

---

## Graphical interface and controls

The GUI version provides a simple main window with the following controls:

```text
Open... | Window | Full Screen
```

The Window and Full Screen buttons are disabled until a file has been selected.

---

## Playback modes

### Window mode

Window mode starts playback in a centered window on the Workbench screen.

In this mode, Core1 renders directly into the Workbench bitmap using the window position offset:

```text
wx / wy
```

### Full Screen mode

Full Screen opens a dedicated Picasso96 screen:

```text
320x240x32
```

In this mode, Core1 renders directly into the dedicated P96 framebuffer without any window offset.

---

## Keyboard controls during playback

```text
F1        Play / unpause
F2        Pause
F3 / Esc  Stop and return to GUI
F4        Stop and quit
F5        Toggle debug output / console stats
```

The same playback engine is used in both modes. Windowed and full-screen playback share the same video/audio streaming core, XACP audio path, AHI output, pacing logic, audio drain and STOP ACK shutdown sequence.

---

## Technical overview

ZZMpegStream uses independent video and audio paths.

The streaming player keeps separate file handles for video and audio demuxing:

```text
g_file_video
g_file_audio
```

### Startup sequence

1. prebuffer video;
2. prebuffer audio;
3. open P96 screen or Workbench window target;
4. prepare Core1 mailbox;
5. copy Core1 MPEG blob;
6. launch Core1;
7. wait for MAGIC OK;
8. wait for first `STATUS_FRAME_READY`;
9. open XACP audio stream;
10. wait for sample rate / channel information;
11. wait for first PCM audio;
12. start AHI;
13. enter playback loop.

### Main loop

```text
audio_tick()
refill_audio_tick()
refill_video_tick(), only while video is not done
video pacing through STATUS_FRAME_READY + MPEG_CMD_NEXT
GUI / keyboard event handling
```

### Shutdown sequence

1. detect video completion or user stop;
2. stop sending `MPEG_CMD_NEXT`;
3. stop video refill;
4. drain or stop remaining audio cleanly;
5. stop AHI;
6. close XACP audio stream;
7. send Core1 STOP;
8. wait for STOP ACK status `0xFF`;
9. close P96 screen or return to GUI.

This shutdown sequence is important for clean RTG recovery.

---

## Shared-memory areas

ZZ-MPEG uses the historical XACP stream-control and audio ring areas:

```text
fb+0x04002000  MP3 / MP2 StreamControl

fb+0x04100000  compressed audio input ring, 512 KB
fb+0x04200000  PCM output ring, 1 MB
```

The MPEG Program Stream ring is application-specific:

```text
fb+0x05000000  ZZMPEG Program Stream ring
```

All offsets are relative to:

```text
fb = board + 0x00010000
```

---

## Compatibility notes

ZZ-MPEG is optimized for clean, constant-frame-rate MPEG-1 Program Stream files.

Most issues reported during testing were caused by malformed, unusual or poorly muxed MPEG files rather than by the playback engine itself. Files originating from online video platforms may require re-encoding with the recommended command above.

If a file behaves incorrectly, re-encode it to the recommended profile before reporting a player issue.

Use matching firmware, `zz9000.card` and application binaries from the same release line when possible.

Do not mix unrelated firmware and application builds unless the combination has been tested.

After replacing firmware or `zz9000.card`, fully power off the Amiga before testing.

---

## Current limitations

* MPEG-1 Program Stream only;
* MP2 audio only;
* 25 fps playback target;
* no seeking yet;
* automatic FPS detection not implemented yet;
* audio/video synchronization still being finalized;
* badly muxed MPEG files may fail or display artifacts;
* full compatibility with arbitrary MPEG files is not a goal of this release.

---

## Safety note

ZZ-MPEG uses the ZZ9000 ARM/RTG path directly.

If a malformed file causes RTG display corruption, a full power cycle may be required before further testing.

Use the recommended encoding profile for reliable playback.

---

## Development roadmap

Planned improvements:

* * internal codebase cleanup;
* improved GUI integration;
* improved file validation;
* MPEG header parsing and FPS detection;
* safer rejection of malformed streams;
* optional audio-clock based sync;
* seeking;
* pause/resume in the core playback engine;
* broader MPEG-1 compatibility.

---

## Relation to XACP v1.5

ZZ-MPEG predates the XACP v1.5 ZZMIDI memory-map correction, but remains part of the validated XACP multimedia line.

It validates the Core1 multimedia model for video playback:

```text
heavy decode work on ARM/Core1
AmigaOS-side display/audio integration
shared DDR communication
AHI playback
STOP / return path
```

---

## License

Except for third-party components identified in
`THIRD_PARTY_LICENSES.txt`, ZZ-MPEG is proprietary closed-source freeware.

Copyright (C) 2026 Xanxi.
All rights reserved.

Redistribution of the original, unmodified binaries is permitted provided
that this documentation and the third-party license notices are included.

Modification, repackaging, or redistribution of modified versions is not
permitted without prior authorization.

The source code is not included or published.

Third-party components remain subject to their respective original licenses.
See `THIRD_PARTY_LICENSES.txt`.

---


## Third-party software

The ARM/Core1 MPEG-1 video decoder used by ZZ-MPEG is based on
PL_MPEG by Dominic Szablewski.

PL_MPEG is licensed under the MIT License.

https://github.com/phoboslab/pl_mpeg

See `THIRD_PARTY_LICENSES.txt` for the complete license notice.

---


## Credits

ZZ-MPEG is built for the ZZ9000 and its ARM capabilities.

ZZ-MPEG uses the XACP approach developed by **Xanxi**.

PL_MPEG by Dominic Szablewski - MPEG-1 decoding, MIT License.

Created for the Amiga and ZZ9000 community.

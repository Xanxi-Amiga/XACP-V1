# ZZMIDIPlay

ZZMIDIPlay v0.5 for XACP v1.5
SoundFont MIDI playback for Amiga systems equipped with the MNT ZZ9000 and the Xanxi/XACP firmware branch.

ZZMIDIPlay is a GUI MIDI file player built around the inclusion of TinySoundFont in the ZZ9000 XX19 firmware. The Amiga-side application provides the user interface, playlist handling, file selection and AHI playback, while the ZZ9000 ARM side performs the SoundFont synthesis.

MIDI files and SoundFont banks are staged through the XACP v1.5 shared-memory map. The firmware-side MIDI/SF2 service renders the MIDI stream through TinySoundFont, then writes PCM audio back to the Amiga side for playback through AHI.

This release focuses on MIDI file playback with SoundFont support up to 32 MB and the corrected XACP v1.5 memory map.

---

## Firmware baseline

Required firmware line:

```text id="3v0xfn"
XX19
```

Protocol / memory-map baseline:

```text id="k1dbbz"
XACP v1.5
```

ZZMIDIPlay v0.5 requires the XACP v1.5 SoundFont / MIDI staging map.

---

## Included tool

This release contains:

```text id="83svpr"
ZZMIDIPlay
```

ZZMIDIPlay is a Workbench / GUI MIDI player.

The daemon, command-line controller and realtime CAMD path are not part of this v0.5 release package.

---

## Main features

```text id="gq75r2"
GUI MIDI file player
SoundFont playback through the ZZ9000 ARM side
SoundFont support up to 32 MB
MIDI file staging up to 6 MB
AHI playback on the Amiga side
corrected XACP v1.5 memory map
```

---

## SoundFont and MIDI limits

XACP v1.5 supports:

```text id="bj5w5h"
SF2 staging size: 32 MB
MIDI staging size: 6 MB
```

Very large SoundFonts above 32 MB are not part of the v0.5 target.

---

## XACP opcode

ZZMIDIPlay uses:

```text id="uk5q2g"
OP_MIDI_SF2 = 0x0120
```

This opcode is implemented by the XX19 firmware line for the XACP v1.5 release.

---

## XACP v1.5 memory map

All addresses below are relative to:

```text id="7ekm1w"
fb = board + 0x00010000
```

ZZMIDI control and audio areas:

```text id="6z1z7f"
fb+0x04010000  ZZMIDI control block
fb+0x04800000  ZZMIDI PCM ring, 1 MB
```

SoundFont and MIDI staging:

```text id="gnn6w8"
fb+0x05800000  SF2 staging start
fb+0x07800000  SF2 staging end / MIDI staging start
fb+0x07E00000  MIDI staging end / firmware C heap start
```

The key XACP v1.5 correction is:

```text id="nvdnh3"
SF2  staging : fb+0x05800000 - fb+0x07800000 = 32 MB
MIDI staging : fb+0x07800000 - fb+0x07E00000 = 6 MB
```

The old MIDI staging location must not be reused:

```text id="hpdyof"
fb+0x07000000
```

That old offset overlaps the last 8 MB of the 32 MB SF2 staging area.

---

## Compatibility notes

Use the matching XX19 firmware, matching `zz9000.card` and matching ZZMIDIPlay binary from the same release package.

Do not mix unrelated ZZMIDI binaries with older firmware builds.

After replacing firmware or `zz9000.card`, fully power off the Amiga before testing.

A mismatched package may cause:

```text id="rme00d"
missing OP_MIDI_SF2 service
wrong memory offsets
SF2 / MIDI staging overlap
silent playback
crash or lock-up during MIDI loading
```

---

## User guide

A detailed ZZMIDIPlay GUI user guide will be provided separately.

---

## Relation to XACP v1.5

ZZMIDIPlay is the first public application to require the corrected XACP v1.5 multimedia memory map.

It is the reason the v1.5 map freezes:

```text id="iefls0"
SF2  staging at fb+0x05800000, 32 MB
MIDI staging at fb+0x07800000, 6 MB
hard end before firmware heap at fb+0x07E00000
```

Future XACP services must not reuse the ZZMIDI staging area without explicitly changing the protocol version.

---

## Credits

## Credits

ZZMIDIPlay and the XACP / ZZ9000 integration are developed by **Xanxi**.

ZZMIDIPlay uses the following third-party components:

```text
TinySoundFont
  SoundFont 2 synthesis library by Bernhard Schelling.
  Used for SF2 instrument rendering on the ZZ9000 ARM side.
  License: MIT.

TinyMidiLoader
  Minimal Standard MIDI File parser by Bernhard Schelling.
  Used for loading and parsing MIDI files before synthesis.
  License: zlib.
```

Original third-party license notices must be preserved in source distributions and should be included in binary release packages.

Created for the Amiga and ZZ9000 community.

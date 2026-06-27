# Changelog

## v1.5.0 / XX19 — XACP v1.5 public baseline

Public firmware baseline implementing the XACP v1.5 protocol and shared-memory map.

### Added

* XACP v1.5 shared multimedia memory map.
* ZZMIDI SoundFont playback path.
* `OP_MIDI_SF2 = 0x0120` implemented.
* 32 MB SF2 staging area:

  ```text
  fb+0x05800000 - fb+0x07800000
  ```
* 6 MB MIDI staging area:

  ```text
  fb+0x07800000 - fb+0x07E00000
  ```
* Private ARM pool documentation:

  ```text
  0x22000000 - 0x26000000  raw private copy area, 64 MB
  0x26000000 - 0x30000000  TSF / TML / service heap, 160 MB
  ```
* SDK memory-map header:

  ```text
  sdk/xacp_memory_map_v1_5.h
  ```

### Fixed

* Previous SF2 / MIDI staging overlap.
* MIDI staging moved away from the old overlapping offset:

  ```text
  fb+0x07000000
  ```
* Large SoundFont playback path stabilized.
* Persistent SF2 reuse across Next / Previous.
* MIDI / TML reload without reparsing unchanged SF2.

### Notes

XX19 replaces the internal XX18t development naming for the public XACP v1.5 release.

XACP v1.5 is the protocol / ABI / memory-map baseline.
XX19 is the firmware build implementing it.

---

## XX18m — first ZZMIDI firmware line

Firmware line introducing ZZMIDI into the public XACP tree.

### Added

* Initial ZZMIDI / SoundFont firmware path.
* `OP_MIDI_SF2 = 0x0120` groundwork.
* ZZMIDI player / daemon integration work.

### Fixed

* ZZ9000AX cold-boot initialization issue.

Earlier builds could require a reset after cold power-on before the ZZ9000AX initialized correctly. XX18m fixed that initialization issue.

---

## XX18c — ZZDoom baseline

Firmware line for the public ZZDoom baseline.

### Added

* Core1 application launch path for ZZDoom.
* Shared DDR command path.
* Deferred PAN / fullscreen RTG synchronization.
* ZZDoom 320x200 support.
* ZZDoom 640x400 support.
* AHI sound effects path.
* CAMD external MIDI path.
* Save / load through AmigaOS files.
* Clean STOP / return path.

### Notes

ZZDoom is a `doomgeneric`-based Doom port where the Doom engine runs on ZZ9000 ARM Core1 while AmigaOS handles RTG display, keyboard input, AHI sound effects, CAMD music, file access and savegames.

---

## XX16c — MP3 / MP2 / ZZMPEG / early Core1 baseline

First major public XACP multimedia baseline.

### Added

* ARM MP3 decoding.
* MP3 streaming pipeline.
* MP2 audio support.
* ZZPlayGUI / XACP MP3 player path.
* AmigaAMP external engine:

  ```text
  zz9000.engine
  ```
* `mpega.library` XACP integration work.
* ZZMPEG MPEG-1 / MP2 playback path.
* JuliaV2 / early Core1 validation path.

### Notes

XX16c is the firmware line where XACP became practically useful as a real Amiga multimedia offload path.

---

## JuliaV2 — Core1 validation application

JuliaV2 validated the Core1 application model later reused by heavier XACP applications.

### Validated

* Core1 launch.
* Shared DDR communication.
* ARM-side rendering.
* Amiga-side display integration.
* Clean STOP / return path.

---

## ZZMPEG — MPEG-1 / MP2 multimedia path

ZZMPEG validated the Core1 multimedia model for video playback.

### Validated

* MPEG-1 Program Stream input.
* Video decode on ARM/Core1.
* Workbench window output.
* Picasso96 fullscreen output.
* MP2 audio through XACP.
* AHI playback on the Amiga side.
* STOP / return path.

---

## ZZBench GUI — CPU and memory-bandwidth validation

ZZBench GUI validates the compute and memory-bandwidth model behind XACP.

### Validated

* 68k Dhrystone / Whetstone.
* ARM Core1 Dhrystone / Whetstone.
* Amiga Chip RAM bandwidth.
* Amiga Fast RAM bandwidth.
* ZZ9000 DDR via ARM Core1 bandwidth.
* ZZ9000 DDR via Zorro III bandwidth.

---

## Legacy XACP V1

The original XACP V1 README is preserved for historical context:

```text
docs/archive/README_XACP_V1_LEGACY.md
```

The original protocol notice remains available here:

```text
docs/XACP_V1_NOTICE.md
```

Current XACP v1.5 documentation starts here:

```text
README.md
docs/XACP_V1_5_HISTORY_MEMORY_MAP_PART2.md
sdk/xacp_memory_map_v1_5.h
```

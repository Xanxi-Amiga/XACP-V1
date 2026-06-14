# ZZDoom for ZZ9000

**ZZDoom** is a `doomgeneric`-based Doom port for Amiga systems equipped with the **MNT ZZ9000**.

It is not a normal 68k Doom port. The Doom engine runs on the ZZ9000’s ARM Core1, while AmigaOS handles the system integration: RTG display, input, AHI sound, CAMD music, file access and savegames.

This first public release is fully playable and includes the essential pieces needed for a useful Amiga/ZZ9000 build: video, controls, sound effects, MIDI music path, save/load and low 68k CPU usage.

---

## Features

* Doom engine running on the ZZ9000 ARM Core1.
* Based on `doomgeneric`.
* Fullscreen 4:3 RTG display.
* `ZZDoom320` and `ZZDoom640` executables.
* Tear-free fullscreen display using deferred PAN page flipping.
* Low 68k CPU usage.
* Keyboard controls, including function keys.
* AHI sound effects.
* CAMD music support, tested with an external MIDI synth.
* Future-ready path for ZZMIDI / SoundFont music support.
* Save/load support through AmigaOS files in `PROGDIR:`.
* Six save slots, named `save1` to `save6`.
* `ZZDoom320` can be combined with `ZZScanlinesV2` for optional scanlines.

Tested gameplay sequence:

```text
Intro / menu
Level 1
Intermission
Level 2
Return to menu
Return to level 1
Music transitions
Sound effects
Save / load
```

---

## Requirements

* Amiga with MNT ZZ9000.
* XACP-enabled ZZ9000 firmware with Core1 launch, shared DDR communication and deferred PAN support.
* Currently supported firmware package: **XX18c** from the Xanxi branch.
* Matching `zz9000.card` supplied with the XX18c package.
* RTG / Picasso96.
* AHI.
* `camd.library`.
* External MIDI synth for music, or music disabled.
* Your own legal Doom WAD.

Standard MNT / Blittersoft firmware builds are not supported unless they include the required XACP/Core1/deferred-PAN features.

Do not mix firmware, `zz9000.card` and ZZDoom executables from different packages. ZZDoom fullscreen output relies on a matched deferred-PAN implementation. Mixed versions may boot and run but can show severe screen tearing or refresh glitches.

---

## Supported WADs

For this first release, ZZDoom supports classic Doom WAD files under 16 MB.

Currently intended / supported targets:

```text
DOOM1.WAD   Shareware Doom
DOOM.WAD    Registered / complete Doom
DOOMU.WAD   Ultimate Doom
DOOM2.WAD   Doom II
```

No commercial WAD is included. You must provide your own legal Doom WAD.

Final Doom / Plutonia / TNT and larger WADs are not supported in this first release because the current WAD size limit is 16 MB. Expanding WAD support is planned later.

---

## Known limitations

* WAD size currently limited to less than 16 MB.
* Doom II is playable, but there is currently a known rendering issue affecting some floor textures on some setups. This will be investigated in a later version.
* Final Doom / Plutonia / TNT are not yet supported.
* Requires the matched XX18c firmware and `zz9000.card`.
* MIDWAN v2+ `zz9000.card` builds are currently not supported for ZZDoom deferred PAN.
* Music currently uses CAMD with an external MIDI synth, or can be disabled.
* Internal ZZMIDI / SoundFont music support is planned for a future release.
* No commercial game data is included.

---

## Installation

Copy the release files to a directory on your Amiga:

```text
ZZDoom320
ZZDoom320.info
ZZDoom640
ZZDoom640.info
DOOM.WAD
```

Install the matching XX18c firmware and matching `zz9000.card` from the same release package.

Fully power off the Amiga after replacing firmware or `zz9000.card`.

---

## Usage from Shell

The Core1 blob is embedded in the executable. You only need to provide your Doom WAD.

```text
ZZDoom320 DOOM.WAD
```

or:

```text
ZZDoom640 DOOM.WAD
```

---

## Usage from Workbench

You can place the WAD in the same directory as the executable and use an icon ToolType:

```text
WAD=DOOM.WAD
```

Example setup:

```text
PROGDIR:ZZDoom320
PROGDIR:ZZDoom320.info
PROGDIR:DOOM.WAD
```

Then start ZZDoom directly from Workbench.

---

## Music

Music currently uses `camd.library` with an external MIDI synth, or can be disabled.

Without an external MIDI synth, you will not hear music, but the game can still run.

Internal ZZMIDI / SoundFont music support is planned for a future release.

---

## Savegames

Save/load is supported through AmigaOS files in `PROGDIR:`.

There are six save slots, named:

```text
save1
save2
save3
save4
save5
save6
```

The actual save files are stored in the program directory.

---

## Firmware note

ZZDoom requires the matched **XX18c XACP/deferred-PAN firmware** and the matching `zz9000.card`.

This is required because ZZDoom relies on:

* Core1 launch;
* shared DDR communication;
* deferred PAN page flipping;
* stable framebuffer synchronization.

Using another `zz9000.card` or another firmware may cause black screens, tearing, refresh glitches or missing features.

---

## Credits

ZZDoom is based on the Doom source / `doomgeneric` family of ports, adapted for the ZZ9000 Core1 architecture.

Special thanks to the Amiga and ZZ9000 communities for testing, feedback and technical inspiration.

---

## Source and documentation

Source code release is being prepared. This binary release is provided as an early technical preview for ZZ9000 users.

GitHub: `https://github.com/Xanxi-Amiga`


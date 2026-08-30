# ZZMIDIPlay SoundFont Recommendations

This document lists SoundFont 2 (`.sf2`) banks suitable for ZZMIDI v0.5 and ZZMIDIPlay GUI on the ZZ9000.

ZZMIDI v0.5 uses the XACP v1.5 memory map and supports *.sf2 SoundFont files up to 32 MB.

---

## Hard limit

```text
Maximum SF2 size: 32 MB
```

SoundFonts larger than 32 MB are not part of the ZZMIDIPlay v0.5 target and should be rejected by the player rather than loaded.

The recommended SoundFonts are therefore small or medium General MIDI banks.

---

## Recommended default

### GeneralUser GS

Recommended public default SoundFont:

```text
GeneralUser GS v1.471
```

Status:

```text
Recommended default
Suitable for inclusion if the license file is included
Near the upper size limit, so test the exact .sf2 file before packaging
```

Why it is recommended:

```text
complete General MIDI bank
good overall instrument coverage
good compatibility with many MIDI files
suitable as a public default bank
redistributable with its license text
```

Packaging rule:

```text
If GeneralUser GS is included in the release package, include its license file next to it.
```

Recommended package layout:

```text
soundfonts/
  GeneralUser_GS_v1_471.sf2
  GeneralUser-GS-LICENSE.txt
```

GeneralUser GS is a good default for users who want a ready-to-use ZZMIDIPlay package without having to search for a SoundFont first.

---

## Best small-bank recommendation

### Creative 4 MB GM

Recommended small SoundFont:

```text
Creative Labs 4M GM_4gmgsmt.sf2
```

Status:

```text
Highly recommended
User-provided only unless redistribution rights are explicitly confirmed
```

Why it is recommended:

```text
very small
very fast to load
low memory pressure
excellent balance for many General MIDI game tracks
often sounds more coherent than larger banks on retro MIDI material
similar to Creative Labs SoundBlaster AWE32 soundfont
```

Creative 4 MB GM is one of the best practical choices for ZZMIDIPlay because it loads quickly and gives a classic Sound Blaster / General MIDI character.

Do not bundle Creative 4 MB GM in the public package unless its redistribution status is clear.

Recommended wording for users:

```text
Creative 4 MB GM is highly recommended if you already legally own or have obtained it.
Place the .sf2 file next to ZZMIDIPlay or select it with the SF2 button.
```

---

## Other suitable SoundFonts

The following banks are good candidates if they are legally obtained and remain below the 32 MB limit.

### Chorium / ChoriumRevA

Status:

```text
Recommended if the exact file fits below 32 MB
Good General MIDI balance
Good alternative to GeneralUser GS
```

Notes:

```text
Test the exact file size before release.
Include only if redistribution is allowed.
Otherwise, list it as a user-provided recommendation.
```

### Turtle Beach / Montego-style small GM banks

Status:

```text
Turtle Beach Montego II Aureal GM.sf2
Recommended small-bank family
Good for fast loading and low memory use
```

Notes:

```text
Useful for testing.
Often well suited to retro game MIDI playback.
Include only if redistribution is allowed.
```

### SC-55-style / SC-88-style small banks

Status:

```text
Recommended for retro game MIDI if legally obtained
User-provided only unless redistribution is explicitly allowed
```

Notes:

```text
Very good match for many DOS / Amiga / early PC game MIDI files.
Avoid bundling banks derived from commercial ROMs or unclear sources.
Use wording such as "SC-55-style" or "SC-88-style" rather than implying that real Roland ROM content is included.
```

---

## Banks not recommended for v0.5

Avoid SoundFonts that are too large or too heavy for the v0.5 target.

Examples:

```text
very large orchestral SoundFonts
large FluidR3-style banks above 32 MB
large Crisis / Timbres-style banks above 32 MB
multi-hundred MB SF2 banks
SF3 banks
SFZ libraries
commercial banks without redistribution permission
ROM dumps or banks of unclear legal origin
```

Reasons:

```text
above the 32 MB SF2 staging limit
slow to copy and parse
unnecessary for the v0.5 target
may fail to load
may create legal redistribution problems
```

ZZMIDIPlay v0.5 is designed for practical General MIDI playback, not for huge workstation-style sample libraries.

---

## Recommended user setup

A good first setup is:

```text
GeneralUser GS      default full GM bank
Creative 4 MB GM    fast retro/game bank
Chorium             alternative GM bank
```

For most users:

```text
Use GeneralUser GS first.
Use Creative 4 MB GM for fast loading and classic game MIDI.
Try Chorium if you want a different GM balance.
```




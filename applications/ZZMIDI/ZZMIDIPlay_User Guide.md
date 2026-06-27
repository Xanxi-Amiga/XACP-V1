# ZZMIDIPlay - User Guide

**ZZMIDIPlay v0.5 - Xanxi 2026**
A SoundFont MIDI player for the Amiga, powered by the MNT ZZ9000 ARM coprocessor.

ZZMIDIPlay uses the XACP v1.5 protocol to render MIDI on the ZZ9000 ARM Cortex-A9 using TinySoundFont and TinyMidiLoader. The generated PCM audio is streamed back to the Amiga side and played through AHI.

The result is General MIDI playback with low 68k CPU usage while the heavy synthesis work runs on the ZZ9000.

---

## Requirements

* Amiga with AmigaOS 3.1 or later.
* Required AmigaOS libraries: `intuition`, `graphics`, `gadtools`, and `asl` V37+.
* MNT ZZ9000 with Xanxi/XACP **XX19** firmware.
* Matching `zz9000.card` from the same release package.
* AHI installed and configured for audio output.
* A SoundFont 2 file (`.sf2`), up to 32 MB.
* One or more Standard MIDI Files (`.mid`).

---

## Firmware baseline

ZZMIDIPlay v0.5 requires:

```text
XX19 firmware
XACP v1.5 memory map
```

Do not use ZZMIDIPlay v0.5 with older ZZMIDI firmware builds unless that exact combination has been tested.

---

## Quick start

1. Launch `ZZMIDIPlay`. The main window opens with the title:

   ```text
   ZZMIDIPlay - Xanxi 2026
   ```

2. Click **SF2** and select a SoundFont.

   Wait until it finishes loading. Large SoundFonts can take a few seconds the first time.

3. Click **Open** and select one or more MIDI files.

4. Press **Play**.

That is the normal workflow: load an SF2 once, then play any number of MIDI files against it.

---

## Loading a SoundFont

The SoundFont is the instrument bank. It must be loaded before any MIDI file can play.

* Click the **SF2** button.
* A standard ASL file requester opens.
* Select a `.sf2` file and confirm.
* The SoundFont is copied to the ZZ9000 and parsed on the ARM side.
* The display shows the loading progress.
* When it is ready, the status line confirms that the SoundFont is loaded.

Notes:

* SoundFonts up to **32 MB** are supported.
* Files larger than 32 MB are rejected with a clear message.
* The SoundFont stays resident.
* Loading a new MIDI file does **not** reload the SoundFont.
* Switching tracks with the same SoundFont is therefore fast.
* To change the instrument bank, click **SF2** again and select another file.

Recommended SoundFonts are clean General MIDI-compatible `.sf2` banks, including small or medium GM banks and SC-55-style / SC-88-style SoundFonts.

---

## Loading and playing MIDI files

* Click **Open** to add MIDI files.
* The requester allows multiple selection.
* Selected `.mid` files are added to the playlist.
* Select a track and press **Play**.
* You can also double-click a row in the playlist window to play it.

Playback is rendered by the ZZ9000 ARM side. The Amiga side handles the GUI, playlist, AHI playback and system integration.

---

## Optional command-line usage

If supported by the release build, files may also be passed on the command line:

```text
ZZMIDIPlay mysoundfont.sf2 song1.mid song2.mid song3.mid
```

The first `.sf2` argument is loaded as the SoundFont. Every `.mid` argument is added to the playlist.

---

## Transport controls

The row of buttons along the bottom of the main window controls playback:

* **Play** - start the selected track.
* **Pause** - pause and resume.
* **Stop** - stop playback and rewind to the start.
* **Prev** - jump to the previous track in the playlist.
* **Next** - jump to the next track in the playlist.
* **Loop** - toggle playlist looping.
* **SF2** - open the SoundFont requester.
* **Open** - add MIDI files to the playlist.
* **PLS** - show or hide the playlist window.

The progress slider at the top shows the position within the current track.

---

## Keyboard shortcuts

```text
Esc          Stop
Left arrow   Previous track
Right arrow  Next track
```

---

## The playlist window

Click **PLS** to open the playlist.

The playlist lists every MIDI file you have added.

Playlist controls:

* **Add** - open a multi-select file requester and append the chosen MIDI files to the end of the playlist.
* **Del** - remove the currently selected entry.
* **Up** - move the selected entry one position earlier.
* **Dn** - move the selected entry one position later.
* **Clear** - empty the playlist.

The currently playing track is tracked across playlist edits.

Deleting or moving entries keeps the correct song associated with playback. Clearing the list stops playback cleanly.

---

## Typical session

1. Start `ZZMIDIPlay`.

2. Click **SF2**.

3. Choose a SoundFont, for example:

   ```text
   GeneralUser-GS.sf2
   ```

4. Wait for the SoundFont to load.

5. Click **Open** and select several `.mid` files.

6. Open **PLS**.

7. Reorder tracks with **Up** / **Dn**.

8. Remove unwanted tracks with **Del**.

9. Press **Play**.

10. Use **Next** / **Prev** or the arrow keys to move through the playlist.

11. Switch instrument banks at any time with **SF2**. The playlist is kept.

---

## Tips and limits

* Loading the SoundFont is the slow step.
* The SoundFont is only parsed again when the SF2 actually changes.
* Browsing through MIDI tracks with the same SoundFont is fast.
* MIDI files are expected to be Standard MIDI Files (`.mid`).
* Very large MIDI files are unusual and may be rejected.
* Keep test sessions reasonable during hot weather: the ZZ9000 can run warm under sustained ARM load.

---

## XACP v1.5 memory limits

ZZMIDIPlay v0.5 uses the XACP v1.5 memory map.

```text
SF2 staging  : fb+0x05800000 - fb+0x07800000 = 32 MB
MIDI staging : fb+0x07800000 - fb+0x07E00000 = 6 MB
```

The old MIDI staging offset must not be reused:

```text
fb+0x07000000
```

That old offset overlaps the last 8 MB of the 32 MB SoundFont staging area.

---

## Troubleshooting

If playback is silent or loading fails:

1. Check that AHI is installed and configured.
2. Check that the XX19 firmware and matching `zz9000.card` are installed.
3. Fully power off the Amiga after replacing firmware or `zz9000.card`.
4. Try a smaller known-good SoundFont.
5. Try a simple known-good General MIDI file.
6. Avoid mixing ZZMIDIPlay binaries with older firmware builds.

A mismatched package can cause:

```text
missing OP_MIDI_SF2 service
wrong memory offsets
SF2 / MIDI staging overlap
silent playback
crash or lock-up during MIDI loading
```

---

## Credits

ZZMIDIPlay - Xanxi 2026.

Synthesis on the MNT ZZ9000 ARM coprocessor via XACP.

SoundFont rendering by TinySoundFont.
MIDI parsing by TinyMidiLoader.

Created for the Amiga and ZZ9000 community.

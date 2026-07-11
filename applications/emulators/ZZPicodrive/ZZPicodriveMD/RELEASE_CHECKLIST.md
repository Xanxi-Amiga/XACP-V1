# Release checklist - ZZPicoDriveMD

## Before GitHub release

- [ ] Confirm version number in executable / About text / docs.
- [ ] Confirm README.md and INSTALL.md are up to date.
- [ ] Confirm firmware requirement wording is present.
- [ ] Confirm no commercial ROMs are included.
- [ ] Confirm no temporary debug files are included.
- [ ] Confirm license / copying files are included.
- [ ] Confirm ZZPicoDriveMD runs from Workbench.
- [ ] Confirm ZZPicoDriveMD runs from Shell.
- [ ] Confirm PAL and NTSC.
- [ ] Confirm AHI backend.
- [ ] Confirm Paula DMA backend.
- [ ] Confirm keyboard controls.
- [ ] Confirm DB9 controls.
- [ ] Confirm lowlevel.library controller mode.
- [ ] Confirm SRAM.
- [ ] Confirm savestates.
- [ ] Confirm quit / restart does not crash.
- [ ] Confirm 320x240x32 troubleshooting note is present.

## GitHub release assets

Recommended release assets:

- `ZZPicoDriveMD_V1.1.lha`
- `ZZPicoDriveMD_V1.1.readme` if mirroring the Aminet readme
- optional screenshots

## Aminet assets

Aminet requires matching basename:

- `ZZPicoDrive.lha`
- `ZZPicoDrive.readme`

or, if using versioned basename:

- `ZZPicoDriveMD_V1.1.lha`
- `ZZPicoDriveMD_V1.1.readme`

Type should be:

```text
misc/emu
```

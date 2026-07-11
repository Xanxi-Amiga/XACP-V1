# Installation

## 1. Install compatible ZZ9000 firmware

Install the matching ZZ9000 **XX19 firmware from Xanxi's branch**, or later XX
firmware, or any firmware explicitly compatible with ZZPicoDriveSMS.

Install the matching `zz9000.card` file.

The firmware is not included in this repository or in the emulator archive.

Download the XX19 firmware and matching `zz9000.card` from:

https://github.com/Xanxi-Amiga/XACP-V1/releases/tag/XX19_firmware

Do not mix `BOOT.BIN`, `zz9000.card` and ZZPicoDriveSMS executables from
different packages.

ZZPicoDriveSMS is not compatible with MNT firmwares 1.0 to 1.13.  
ZZPicoDriveSMS is not compatible with MiDWaN / BlitterStudio firmwares.

This is not a criticism of other ZZ9000 firmware branches. ZZPicoDriveSMS needs
specific low-level Core1, DDR mapping and RTG/PAN mechanisms for this
application, and therefore requires an explicitly compatible firmware.

## 2. Power cycle

Power off the Amiga completely after changing firmware/card files.

## 3. Check Picasso96

Make sure Picasso96 is installed and that your ZZ9000 has a valid
320x240x32 RTG mode.

A High Colour Workbench screen is not enough. ZZPicoDriveSMS needs a real
32-bit / True Colour ZZ9000 RTG mode.

## 4. Audio

Install AHI if you want to use the AHI audio backend.

The Paula DMA backend does not require AHI.

YM2413 / FM audio is not enabled in this release. Games with optional FM sound
should still run, but this release uses the standard PSG audio path.

## 5. Copy ZZPicoDriveSMS

Copy the ZZPicoDriveSMS directory to your Amiga.

## 6. Add ROMs

Copy your own Sega Master System ROMs to a directory of your choice.

No ROMs are included.

Only use ROM files you legally own.

Supported ROM formats:

- `.sms`
- `.bin`

Unsupported:

- `.zip`

Please decompress them before use.

## 7. Run

Start ZZPicoDriveSMS from Workbench or Shell.

Select:

- ROM file
- region
- audio backend
- input mode

Click **Start**.

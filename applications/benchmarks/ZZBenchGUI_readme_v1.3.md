# ZZBench GUI V1.3

**SysInfo-style dual benchmark for the ZZ9000 RTG board: your Amiga's 68k CPU
vs the card's ARM Cortex-A9, side by side - now with memory-bandwidth and XACP
latency tests.**

ZZBench runs the same integer (Dhrystone 2.1) and floating-point (Whetstone)
kernels on both processors of a ZZ9000-equipped Amiga - the 68k host and the
ARM Core1 on the card's Xilinx Zynq - and shows MIPS / MWIPS for each, with the
ARM/68k speed ratios.

It also measures the important memory paths of the card and, since V1.3, the
latency of the XACP command path between the 68k and the ZZ9000 ARM firmware.

**New in V1.3:** an XACP latency section measuring raw command dispatch,
verified command dispatch, an application-level guarded XACP call wrapper, and
optional 68k-to-ZZ9000 DDR payload upload cost.

**New in V1.2:** Amiga 4000T compatibility fix.

**New in V1.1:** a memory-bandwidth section that measures, in the same window,
how fast each path moves data: the Amiga's own Chip and Fast RAM (68k), the
ZZ9000 DDR seen by the ARM Core1, and the same ZZ9000 DDR reached by the 68k
through the Zorro bus.

---

## Highlights

- **One window, both CPUs, the memory paths, and XACP latency.** Comparison bars
  for integer (MIPS) and floating point (MWIPS), three groups of memory-
  bandwidth bars (MB/s), and command-latency figures in microseconds.
- **Single 68020+ binary.** There are no separate 68020 and 68060 builds.
  The release contains only the `-m68020` 68k executable, which runs on
  **68020 / 68030 / 68040 / 68060** systems. The real CPU and FPU are detected
  at runtime; a 68060 simply produces faster numbers from the same binary.
- **Self-contained.** The ARM benchmark blob is embedded in the executable -
  nothing else to copy.
- **Honest numbers.** Peak-performance measurement, not a CPU-usage meter.
  Best-of-3 passes for the CPU tests, task switching disabled during each 68k
  pass. The memory tests are non-destructive. The latency tests clearly separate
  command latency from DDR upload cost.

---

## Requirements

- An **MNT ZZ9000** (Zynq Z-7020). The **ZZ9000AX add-on is not required**.
- **AmigaOS 2.04+** (Kickstart 37 or later).
- A **68020 or newer** CPU. An FPU (68881/68882, or the integrated 68040/68060
  FPU) is used for the floating-point test; **without an FPU that test is
  skipped** and shown as `no FPU`, the integer test still runs.
- ZZ9000 firmware that exposes the Core1 launch path used by the ARM side and
  the XACP command registers used by the latency test. Developed and tested
  against the author's MNT 1.13-based XACP ZZ9000 firmware.

---

## Usage

Double-click the icon, or run `ZZBenchGUI` from a Shell. The benchmark starts
immediately and runs the CPU tests, the memory tests, then the XACP latency
tests.

- **The Amiga freezes for ~30 seconds during the run - this is normal.** Task
  switching is disabled while measuring so other tasks can't steal cycles, and
  the ARM memory test launches Core1. The window updates when everything is
  done.
- For the memory and latency payload tests, **no audio / MIDI / MP3 / Core1
  application should be running** so the shared DDR area is idle.
- The XACP latency test uses a reserved/not-implemented opcode as a safe ping.
  It does **not** launch Quake, Doom, MPEG, MP3, MIDI or any Core1 program.
- **Test** re-runs the whole benchmark. **Quit** closes the window.

No data files are needed - the ARM blob is embedded.

---

## Reading the results

### CPU

**Integer (Dhrystone 2.1)** - reported in MIPS plus raw Dhrystones/sec:

```
MIPS = Dhrystones per second / 1757
```

1757 Dhrystones/sec = the VAX-11/780, the historical "1 MIPS" reference. A
68060@50 landing around 45-55 MIPS is normal - that is VAX-normalized
Dhrystone, not marketing MHz.

**Floating point (Whetstone)** - reported in MWIPS.

### Memory bandwidth (V1.1+)

Three groups of bars, in MB/s (here **MB = 1024 KB**). **Each group is scaled to
its own maximum** - otherwise the slow Zorro path would be invisible next to the
ARM. Read the number next to each bar for the absolute values; the bar length
only compares within a group.

- **Amiga RAM (68k)** - Chip and Fast RAM read/write, as the 68k sees them.
- **ZZ9000 DDR via ARM Core1** - read / write / copy, as the ARM sees its own
  DDR.
- **ZZ9000 via Zorro III (68k)** - read / write, the 68k reaching the same
  ZZ9000 DDR through the Zorro bus.

Example on a reference **Amiga A4000 / 68060@50** with a ZZ9000 (ARM ~666 MHz):

| Test                          | 68k (68060) | ARM Core1 | ARM / 68k |
|-------------------------------|-------------|-----------|-----------|
| Integer (MIPS)                | ~52         | ~1059     | ~x20      |
| Float (MWIPS)                 | ~47         | ~917      | ~x19      |

| Memory path                   | read | write | copy |
|-------------------------------|------|-------|------|
| Amiga Chip RAM (68k)          | ~4.3 | ~6.6  | -    |
| Amiga Fast RAM (68k)          | ~27.5| ~21.7 | -    |
| ZZ9000 DDR via ARM Core1      | ~142 | ~360  | ~96  |
| ZZ9000 DDR via Zorro III (68k)| ~6.3 | ~9.6  | -    |

The single most useful figure here is the **ratio between the last two rows**:
the ARM reaches its DDR roughly **20-37x faster** than the 68k reaching the
*same* memory through Zorro. That gap is exactly why offloading work to the ARM
(the XACP approach) pays off - it is the data that doesn't have to cross the
Zorro bus.

The ARM figures are deterministic; the 68k figures vary by ~1-2% between runs.

### XACP latency (V1.3)

The XACP latency section measures how expensive it is for the 68k to issue a
minimal command to the ZZ9000 ARM firmware and receive a status back. It uses
reserved opcode `0x0340` (`OP_ARM_QUAKE`, currently not implemented) as a safe
ping. On the tested firmware, this opcode is dispatched and returns
`status = ERROR`, with no side effect.

The three main figures are:

| XACP latency test              | What it means                                      | Reference result |
|--------------------------------|----------------------------------------------------|------------------|
| Raw / FAST dispatch            | write opcode + read status                         | ~3 us/call       |
| VERIFIED dispatch              | proven `DONE -> ERROR` transition                  | ~3 us/call       |
| HIGH-LEVEL guarded XACP call   | local lock + request wrapper + verify + unlock     | ~10 us/call      |

The key number for a real application-style call is the **HIGH-LEVEL guarded
XACP call**. It is not a firmware echo ping: the current firmware does not echo
a request id, cookie or payload. It is an application-level wrapper around the
verified XACP command path, measuring the overhead that a clean local
`XACP_Call()`-style API would carry.

A separate payload section copies data from 68k Fast RAM to ZZ9000 DDR before
each ping. This is **not** command latency anymore; it is mostly Zorro upload
cost:

| DDR payload before ping | Reference result |
|-------------------------|------------------|
| 64 B                    | ~9 us            |
| 256 B                   | ~29-30 us        |
| 1 KB                    | ~107 us          |
| 4 KB                    | ~415 us          |
| 16 KB                   | ~1.9 ms          |
| 64 KB                   | ~7.9 ms          |

Read this carefully: a 64 KB payload taking ~7.9 ms does **not** mean XACP has a
7.9 ms command latency. It means the 68k spent most of that time uploading 64 KB
to ZZ9000 DDR through Zorro. The command path itself remains in the microsecond
range.

---

## How it measures

### CPU

**68k side.** Unmodified Dhrystone 2.1 (Weicker) and Whetstone kernels, timed
with the CIA E-Clock via `ReadEClock()`. The run length is auto-calibrated to a
~3-second target, each test is run **best-of-3** (the fastest pass = the peak),
and `Forbid()/Permit()` brackets each pass to remove task-switch jitter.

**ARM side.** The same kernels run **bare-metal on Core1**, timed by the
Cortex-A9 cycle counter. No OS, no multitasking - so the ARM result is
deterministic and repeatable to the cycle.

### Memory bandwidth (V1.1+)

All three groups use the same simple method: a **sequential, scalar 32-bit**
read/write loop over a buffer, sized well past the relevant cache, timed and
converted to MB/s. This is the **useful bandwidth of plain C code**, not the
chip's theoretical peak (which would need NEON/burst/prefetch tricks) - and it
is exactly the figure that matters for the kind of work the card actually does
(ZZDoom, MP3, MIDI, PicoDrive and future ports).

- **Amiga RAM (68k).** A private 512 KB buffer is allocated with
  `AllocMem(MEMF_FAST)` and `AllocMem(MEMF_CHIP)`, benchmarked, then freed -
  fully non-destructive (it is our own memory). Timed with the E-Clock,
  `Forbid()/Permit()` around each measured pass. Fast RAM reads beat writes
  (cacheable, burst line fills); Chip RAM writes beat reads (non-cacheable, the
  68060 store buffer hides latency that blocking reads cannot).

- **ZZ9000 DDR via ARM Core1.** Core1 benchmarks its **own** DDR in a region
  that belongs to the Core1 applications (mapped Normal write-back / write-
  allocate, i.e. cached), using two 2 MiB buffers for read / write / copy. It is
  timed by the Cortex-A9 cycle counter, with a `dsb` barrier before the clock is
  stopped so deferred write-backs are counted. The 68k brackets the run at the
  E-Clock and derives the **real ARM clock**, so the MB/s are frequency-robust
  rather than tied to the nominal 666 MHz.

- **ZZ9000 via Zorro III (68k).** The 68k reads/writes the **same** ZZ9000 DDR
  through the Zorro framebuffer window - so this measures the **Zorro bus cost**,
  not the DDR itself. The 1 MiB test area was verified to be **outside the
  AmigaOS memory list** (it is the card's framebuffer window, not allocatable
  system RAM), and the test is non-destructive: the area is **saved to Fast RAM,
  benchmarked, then restored**. Numbers in the single-digit MB/s range are the
  normal cost of scalar longword access over real Zorro III from a 68060.

### XACP latency (V1.3)

The latency test uses the normal XACP register command path. No firmware patch
or reflash is required.

- **FAST.** Writes reserved opcode `0x0340` to `REG_CMD`, then reads the status
  register. This is the minimum possible command-path measurement.

- **VERIFIED.** First uses a safe `OP_MEMCPY` of a 32-byte scratch buffer onto
  itself to re-arm the status to `DONE`, then sends opcode `0x0340` and waits
  for `ERROR`. This proves that each iteration produced a fresh firmware
  dispatch, not just a re-read of an old status.

- **HIGH-LEVEL guarded call.** Wraps the verified path in a local semaphore,
  local request structure, local request id, timeout and status verification.
  This measures the cost of a clean application-side XACP call wrapper without
  requiring a new firmware opcode.

- **DDR payload + ping.** Copies a block from 68k Fast RAM into ZZ9000 DDR via
  Zorro, then pings. These numbers show upload cost plus ping. They should not
  be compared to command latency figures.

---

## Notes on the numbers (honest)

**The floating-point path.** A 68060 has **no hardware** `sin`/`cos`/`exp`/`log`
- those trap to `68060.library` (Motorola FPSP). To keep the MWIPS figure
**consistent across CPUs and build targets**, ZZBench emits the FPU
transcendental instructions directly, so the path is always *"FPU + the
system's math support"*: the FPSP emulation on a 68060, real hardware on a
68881/68882/68040. On a 68060 it is therefore neither pure software nor pure
hardware - that is a property of the chip, not of the benchmark.

**The ARM "softfp".** The header shows `softfp` - that is the **ABI**, not
soft-float. The Cortex-A9 VFP is fully used for the maths; softfp vs hard only
changes how float arguments are passed across function calls.

**The memory bars are per-group.** A bar's length only means something *within*
its group; always read the MB/s value beside it. The ARM DDR figures are scalar,
cached bandwidth - not a "Zynq DDR peak". The Zorro figures are the bus cost of
the 68k reaching the card, not a measure of the DDR being slow.

**XACP latency vs DDR upload.** The raw and guarded XACP latency figures are
microsecond command-path measurements. The DDR payload figures include a memory
copy over Zorro. A large payload can take milliseconds even when the command
itself takes only a few microseconds.

---

## Version history

- **V1.3** - adds XACP latency tests: raw/FAST dispatch, VERIFIED dispatch,
  HIGH-LEVEL guarded call wrapper, and DDR payload upload + ping.
- **V1.2** - Amiga 4000T compatibility fix.
- **V1.1** - adds memory-bandwidth tests for Amiga RAM, ZZ9000 DDR via ARM
  Core1, and ZZ9000 DDR via Zorro III from the 68k.
- **V1.0** - initial CPU benchmark release: Dhrystone/Whetstone 68k vs ARM.

---

## License

ZZBench is closed-source freeware.

Copyright (C) 2026 Xanxi.

Redistribution is permitted only in unmodified form and with this
documentation included.

Modification, reverse engineering, repackaging, or redistribution of
modified binaries is not permitted without prior authorization.

The source code is not included or published.

Dhrystone 2.1 and Whetstone remain subject to their respective original
authorship and notices.

---

## Credits

- **Xanxi**, 2026. Part of the **XACP** (Xanxi ARM Coprocessor Protocol) work
  for the MNT ZZ9000.
- Dhrystone 2.1 by Reinhold P. Weicker; Whetstone by Curnow & Wichmann - both
  long-standing public benchmarks.
- MNT ZZ9000 hardware by MNT Research.


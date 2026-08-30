# XACP v1.6 Developer Documentation

## eXtended ARM Coprocessor Protocol for the MNT ZZ9000

XACP is a software architecture allowing Amiga applications to use the ARM Cortex-A9 processors and DDR memory of the MNT ZZ9000 as a general-purpose coprocessor platform.

The current public baseline is:

```text
Firmware:  XX19a
Protocol:  XACP v1.6
```

Firmware build numbers and XACP versions are deliberately separate.

```text
XX19a     = firmware build
XACP v1.6 = protocol / ABI / shared-memory baseline
```

This document explains the architecture and development rules of XACP v1.6.

For exact memory constants, the authoritative public reference is:

```text
../sdk/xacp_memory_map_v1_6.h
```

New software should use the SDK definitions rather than duplicating addresses from this document.

---

# 1. Architecture

The ZZ9000 uses a Xilinx Zynq-7020 containing two ARM Cortex-A9 cores and its own DDR memory.

XACP uses this hardware in two complementary ways:

```text
Core0  persistent firmware services
Core1  dynamically loaded application engines
```

The Amiga remains the host system.

Typical responsibilities remain divided as follows:

```text
Amiga / 68k
-----------
AmigaOS integration
files
GUI
keyboard / input
Picasso96 / RTG integration
AHI audio output
CAMD MIDI
application launch and control

ZZ9000 ARM
----------
CPU-intensive computation
decoding
synthesis
emulation
game engines
rendering
large private working sets
```

XACP is therefore not a replacement operating system for the Amiga.

It is a coprocessor architecture in which AmigaOS remains responsible for system integration while ARM code performs selected workloads.

---

# 2. Core0 firmware services

ARM Core0 executes the main ZZ9000 firmware.

XACP services integrated into the firmware can remain available while normal Amiga applications continue running.

Examples include:

```text
MP3 / MP2 decoding
streaming audio services
MIDI / SoundFont synthesis
shared command processing
shared audio buffers
```

ZZMIDI is the main example of the persistent-service model.

The SoundFont synthesis engine runs on the ARM side while the Amiga-side software handles:

```text
CAMD
files
preferences
control
AHI playback
```

Persistent services must not allocate memory from regions assigned to dynamic Core1 applications.

---

# 3. Core1 applications

The second ARM Cortex-A9 can execute dynamically loaded application engines.

Examples include:

```text
ZZDoom
ZZPicoDrive
ZZRastan
JuliaV2
benchmark workloads
video / graphics engines
experimental game engines
```

A Core1 application may require several different types of memory:

```text
executable blob
shared control structure
framebuffer
ROM or input data
private ARM heap
audio buffer
save / persistent data
```

These allocations must be coordinated with persistent Core0 services.

This requirement is one of the main reasons XACP v1.6 exists.

---

# 4. Why XACP v1.6 was required

Early XACP applications were developed progressively.

When only a few services existed, selecting unused DDR addresses independently was practical.

As the platform grew, DDR became shared by:

```text
firmware services
Core1 executable blobs
Core1 heaps
framebuffers
ROM images
audio rings
SoundFont data
MIDI data
save-state buffers
persistent data
firmware heaps
```

An address that appeared unused in one project could already belong to another component.

This could produce failures such as:

```text
white or black screen
RTG corruption
silent audio
incorrect save data
application crash
system lock-up
Core1 startup failure
Core1 return failure
```

XACP v1.6 therefore makes memory ownership an explicit part of the ABI.

---

# 5. Address spaces

Three forms of address must not be confused.

## 5.1 Amiga board address

On the Amiga side:

```c
board = cd->cd_BoardAddr;
```

This is the base address assigned to the ZZ9000 on the Zorro bus.

---

## 5.2 Framebuffer-relative shared address

The XACP framebuffer base as seen by Amiga software is:

```text
fb = board + 0x00010000
```

The SDK defines:

```c
XACP_MNT_FB_BASE
```

Shared XACP regions are normally expressed as offsets relative to this framebuffer base.

---

## 5.3 ARM physical address

The ARM physical base corresponding to the framebuffer is:

```text
0x00200000
```

Therefore, for a framebuffer-relative shared offset:

```text
ARM physical address =
    0x00200000 + framebuffer-relative offset
```

The SDK provides translation helpers for this purpose.

---

## 5.4 ARM-private addresses

Some regions are expressed directly as absolute ARM physical addresses.

These are not framebuffer-relative values.

Never treat:

```text
0x23000000
```

for example, as:

```text
fb + 0x23000000
```

The distinction between shared offsets and ARM absolute addresses is fundamental.

---

# 6. Shared XACP memory

Shared memory is DDR visible through the XACP / framebuffer mapping and used for communication between the Amiga and ARM.

Established generic XACP regions include:

```text
command structures
stream-control structures
MP3 input ring
PCM output ring
legacy Core1 areas
ZZMPEG streaming data
```

These older regions remain part of the environment because current firmware retains the corresponding services.

Their exact addresses are defined in:

```text
../sdk/xacp_memory_map_v1_6.h
```

---

# 7. ZZMIDI v1.6 shared corridor

XACP v1.6 assigns ZZMIDI a dedicated shared corridor.

Conceptually it contains:

```text
control area
realtime MIDI FIFO
PCM ring
chunked upload window
```

The complete shared allocation is 3 MiB.

The current layout is approximately:

```text
fb+0x06000000   control allocation
fb+0x06010000   realtime MIDI FIFO allocation
fb+0x06100000   PCM ring
fb+0x06200000   upload window
fb+0x06300000   end
```

The upload window is 1 MiB.

It is important to understand that this is an upload window, not permanent storage for the complete SoundFont or MIDI file.

Files are transferred through it in chunks and copied to ARM-private storage.

This replaces the old XACP v1.5 model where large SoundFont and MIDI staging areas occupied permanent Zorro-visible shared regions.

---

# 8. ZZMIDI ARM-private storage

XACP v1.6 separates large persistent ZZMIDI data from the shared upload corridor.

ARM-private allocations include reserved areas for:

```text
raw SoundFont data
raw MIDI / parsed data
TinySoundFont / TinyMidiLoader runtime heap
```

The current reserved allocations are:

```text
32 MiB   SoundFont private storage
 6 MiB   MIDI private storage
160 MiB  service runtime heap
```

These are allocation sizes in the ABI.

They must not automatically be interpreted as user-visible limits of a specific software release.

For example, the maximum SoundFont accepted by a given ZZMIDI release may be lower than the size of the reserved private storage.

Application-level limits belong in the ZZMIDI documentation.

---

# 9. ARM-private safety boundary

Not all high ARM physical addresses are safe for private allocation.

Part of the ARM address space projects into live AmigaOS-visible ZZ9000 Zorro III Fast RAM.

The SDK therefore explicitly identifies:

```text
Z3 collision region
no-man's-land / guard region
safe ARM-private region
```

Current firmware policy permits ARM-private allocations only within the documented safe window.

Do not create a private ARM heap below the published safe boundary simply because the address appears unused from the ARM application.

Doing so can corrupt Amiga-visible memory.

---

# 10. ZZPicoDrive private allocation

XACP v1.6 reserves an ARM-private area for the established ZZPicoDrive / Core1 environment.

This currently occupies the beginning of the documented safe private area.

ZZMIDI private storage begins after that allocation.

This separation is deliberate:

```text
Core1 private allocation
then
persistent ZZMIDI private allocation
```

Neither component should silently expand into the other's range.

The exact boundaries are defined by the SDK header.

---

# 11. Guard regions

XACP v1.6 uses explicit guard or reserved regions.

A guard region is not free memory.

It exists to:

```text
separate unrelated allocations
prevent accidental heap growth
protect future expansion
make overlap errors easier to detect
```

Do not reuse guard areas as generic scratch space.

The SDK also reserves memory above the current low private pool for specific multimedia use such as the SMUSH codec region.

Again, a reserved region must not be interpreted as unused DDR.

---

# 12. Cache coherency

The ARM Cortex-A9 uses a write-back data cache.

The Amiga and ARM do not automatically maintain cache coherency for shared XACP memory.

Cache maintenance is therefore part of the protocol.

General rule:

```text
68k writes -> ARM reads
    ARM invalidates the corresponding D-cache range before reading.

ARM writes -> 68k reads
    ARM flushes the corresponding D-cache range after writing.
```

Do not assume that a successful memory write means the other processor immediately sees the new value.

Incorrect cache handling can produce intermittent failures that resemble:

```text
stale commands
repeated MIDI events
missing audio
corrupted data
random application failures
```

Shared structures should be aligned and maintained according to the cache requirements of the ARM platform.

---

# 13. Endianness

The processors use different native byte order:

```text
Amiga 68k      big-endian
ARM Cortex-A9  little-endian
```

Every shared structure must therefore define an explicit endian convention.

The original XACP command and stream structures use big-endian values in shared DDR so that the Amiga can access them natively.

ARM code must convert values appropriately.

Do not remove endian conversion merely because a test happens to work with zero or small values.

---

# 14. Shared FIFO and ring ownership

For shared ring buffers and FIFOs, ownership of indexes must be explicit.

General producer / consumer rule:

```text
producer owns write pointer
consumer owns read pointer
```

One side must not arbitrarily rewrite the pointer owned by the other.

For example:

```text
Amiga -> ARM input ring

Amiga owns write index
ARM owns read index
```

and for an ARM-generated PCM ring:

```text
ARM owns write index
Amiga owns read index
```

This ownership model minimizes synchronization requirements and avoids conflicting updates.

---

# 15. Long-running operations

Long operations must not keep the 68k blocked inside an immediate Zorro command transaction.

A command should normally:

```text
validate request
ACK quickly
start or schedule work
return control
```

Long-running processing should continue through:

```text
firmware state machines
Core1 execution
shared status fields
sequence counters
pollable completion state
```

This is especially important for decoding, synthesis, emulation and other workloads that may execute for many milliseconds or indefinitely.

---

# 16. Core0 / Core1 coexistence

The existence of a coordinated memory map does not automatically mean that every possible combination of services and Core1 applications is safe.

Memory ownership is only one aspect of coexistence.

Other shared resources may include:

```text
video state
ARM hardware
interrupts
cache state
audio
firmware state machines
Core1 lifecycle
```

Combinations must therefore be validated explicitly.

ZZMIDIGate is an example of an application-level mechanism used to coordinate ZZMIDI realtime operation with Core1 startup.

Do not claim simultaneous compatibility for a combination that has not been tested.

---

# 17. Firmware and application compatibility

Do not treat firmware, protocol and applications as independent interchangeable pieces.

An XACP application may depend on:

```text
firmware build
XACP ABI version
zz9000.card
Amiga executable
Core1 binary
shared-memory definitions
```

The current public baseline is:

```text
XX19a / XACP v1.6
```

Historical software may require a historical firmware generation.

Conversely, an old binary may start under a newer firmware while still using incorrect shared addresses.

Successful startup is not proof of ABI compatibility.

---

# 18. Current version history

The main public XACP generations include:

```text
XX16c
    MP3 / MP2 / ZZMPEG multimedia baseline
    early Core1 work

XX18c
    ZZDoom public baseline

XX18m
    initial ZZMIDI firmware work
    ZZ9000AX cold-boot initialization fix

XX19
    XACP v1.5
    formal multimedia memory map
    large shared ZZMIDI staging model

XX19a
    XACP v1.6
    coordinated Core0 / Core1 allocation
    current public baseline
```

The old documentation is preserved under:

```text
archives/
```

for development history and regression reference.

---

# 19. Developing a new XACP application

Before assigning memory to a new project:

## Step 1 — Identify the execution model

Determine whether the project is:

```text
Core0 persistent firmware service
Core1 dynamic application
Amiga-side client only
```

---

## Step 2 — Identify required memory classes

Separate requirements into:

```text
shared communication
framebuffer
input / ROM data
audio
Core1 executable
ARM-private working memory
persistent / save data
```

Do not place everything in one arbitrary contiguous region.

---

## Step 3 — Use the SDK

Include:

```text
../sdk/xacp_memory_map_v1_6.h
```

where appropriate.

Do not redefine existing addresses locally.

---

## Step 4 — Check every overlap

Check the new allocation against:

```text
generic XACP shared regions
Core1 application areas
ZZMIDI shared corridor
ZZPicoDrive private area
ZZMIDI private storage
firmware service heap
guard regions
reserved multimedia regions
Amiga-visible Z3 memory
```

---

## Step 5 — Add the allocation centrally

If a new region is required, add it to the common XACP memory map.

Do not leave important allocation constants hidden only inside one application's source tree.

---

## Step 6 — Add compile-time checks

Where practical, add static assertions checking that:

```text
base < end
size matches expected allocation
adjacent regions meet correctly
regions do not overlap
shared -> ARM translations are correct
```

The current v1.6 header already uses this approach.

---

## Step 7 — Determine ABI impact

If the new allocation is compatible with the existing map, it may remain within the current ABI.

If it requires moving or redefining established regions used by existing binaries, the XACP ABI version must be updated.

An incompatible memory-map change must not silently retain the same protocol version.

---

# 20. Source of truth

The documentation hierarchy for XACP v1.6 is:

```text
sdk/xacp_memory_map_v1_6.h
    normative memory-map definitions

sdk/README.md
    SDK overview and memory-model summary

docs/XACP_V1_6_DEVELOPER_DOCUMENTATION.md
    architecture and development rules

firmware/README.md
    firmware history, installation and firmware-specific information

applications/*/
    application-specific requirements and behaviour
```

When exact addresses differ between an older document and the current SDK header, the current v1.6 SDK header takes precedence.

---

# 21. Historical documentation

The following generations are retained for reference:

```text
XACP V1
XACP v1.5
```

They document real historical implementations but are not current development specifications.

In particular, the XACP v1.5 large Zorro-visible ZZMIDI staging model has been superseded by the v1.6 model using:

```text
small shared upload corridor
+
ARM-private persistent storage
```

New software must not restore the obsolete v1.5 staging addresses.

---

# 22. Summary

XACP v1.6 establishes a coordinated memory and execution model for the ZZ9000 ARM coprocessor environment.

The core rules are:

```text
XX19a implements XACP v1.6.

Core0 services and Core1 applications have separate ownership.

Shared and ARM-private addresses are different address classes.

Use the SDK memory-map definitions.

Do not introduce undocumented DDR allocations.

Do not reuse reserved or guard regions.

Never use Amiga-visible Z3 memory as ARM-private storage.

Maintain cache coherency explicitly.

Define endian behaviour explicitly.

Keep producer / consumer ownership clear.

Do not block the 68k for long-running ARM operations.

Do not assume application combinations are safe unless tested.

Change the XACP ABI version when an incompatible memory-map change is required.
```

The purpose of these rules is to allow XACP to continue growing as a common acceleration platform without returning to incompatible per-application DDR maps.

---

## Credits

XACP / Xanxi, 2026.

For use with the MNT ZZ9000 Amiga RTG / ARM platform.

Thanks to the MNT ZZ9000 project and to the Amiga and ZZ9000 communities for the hardware platform, testing, feedback and technical discussion.

Third-party software retains its respective authorship and license.

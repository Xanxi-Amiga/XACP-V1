#ifndef XACP_MEMORY_MAP_V1_6_H
#define XACP_MEMORY_MAP_V1_6_H

/*
 * XACP v1.6 memory map
 *
 * eXtended ARM Coprocessor Protocol for the MNT ZZ9000.
 *
 * Public baseline:
 *
 *   Firmware : XX19a
 *   Protocol : XACP v1.6
 *
 * Firmware build numbers and XACP protocol versions are separate.
 *
 * Addressing conventions
 * ----------------------
 *
 * Amiga side:
 *
 *   board = cd->cd_BoardAddr
 *   fb    = board + XACP_MNT_FB_BASE
 *
 * ARM side:
 *
 *   framebuffer physical base = 0x00200000
 *
 * Unless explicitly marked as an ARM absolute physical address,
 * XACP shared-memory addresses in this header are framebuffer-relative.
 *
 * For a framebuffer-relative offset:
 *
 *   ARM physical = 0x00200000 + fb-relative offset
 *
 * IMPORTANT:
 *
 * XACP v1.6 makes shared DDR ownership part of the ABI.
 * Do not introduce new undocumented DDR allocations.
 */

#ifdef __cplusplus
extern "C" {
#endif


/* ------------------------------------------------------------------------- */
/* Version                                                                   */
/* ------------------------------------------------------------------------- */

#define XACP_VERSION_MAJOR                  1UL
#define XACP_VERSION_MINOR                  6UL
#define XACP_VERSION_PATCH                  0UL

#define XACP_VERSION_U32                    0x00010600UL


/* ------------------------------------------------------------------------- */
/* Addressing                                                                */
/* ------------------------------------------------------------------------- */

#define XACP_MNT_FB_BASE                    0x00010000UL
#define XACP_ARM_FB_PHYS_BASE               0x00200000UL

#define XACP_FB_OFFSET_TO_ARM_PHYS(x) \
    (XACP_ARM_FB_PHYS_BASE + (x))

#define XACP_ARM_PHYS_TO_FB_OFFSET(x) \
    ((x) - XACP_ARM_FB_PHYS_BASE)


/* ------------------------------------------------------------------------- */
/* Established generic XACP shared regions                                   */
/* ------------------------------------------------------------------------- */

/*
 * These regions predate v1.6 and remain part of the established XACP
 * environment.
 */

#define XACP_COMMAND_OFFSET                 0x04000000UL
#define XACP_STREAM_OFFSET                  0x04002000UL

#define XACP_MP3_RING_OFFSET                0x04100000UL
#define XACP_MP3_RING_SIZE                  (512UL * 1024UL)
#define XACP_MP3_RING_END \
    (XACP_MP3_RING_OFFSET + XACP_MP3_RING_SIZE)

#define XACP_PCM_RING_OFFSET                0x04200000UL
#define XACP_PCM_RING_SIZE                  (1UL * 1024UL * 1024UL)
#define XACP_PCM_RING_END \
    (XACP_PCM_RING_OFFSET + XACP_PCM_RING_SIZE)


/* ------------------------------------------------------------------------- */
/* XACP v1.6 ZZMIDI shared corridor                                          */
/* ------------------------------------------------------------------------- */

/*
 * All current ZZMIDI shared regions live inside:
 *
 *   fb+0x06000000 .. fb+0x06300000
 *
 * ARM physical view:
 *
 *   0x06200000 .. 0x06500000
 *
 * Layout:
 *
 *   fb+0x06000000  control block
 *                  actual block: 0xD0 bytes / 208 bytes
 *                  64 KB reserved before FIFO
 *
 *   fb+0x06010000  realtime MIDI FIFO
 *                  actual FIFO span: 0x2060 bytes
 *
 *   fb+0x06100000  MIDI PCM ring
 *                  1 MB
 *
 *   fb+0x06200000  chunked upload buffer
 *                  1 MB
 *
 *   fb+0x06300000  end of shared ZZMIDI corridor
 *
 * The unused space between the end of the actual FIFO and 0x06100000 is
 * reserved corridor space. It is NOT part of the logical FIFO payload and
 * must not be independently allocated.
 */

#define XMID_SHARED_START                   0x06000000UL


/* Control block ----------------------------------------------------------- */

#define XMID_CTRL_OFFSET                    0x06000000UL
#define XMID_CTRL_STRUCT_SIZE               0x000000D0UL   /* 208 bytes */
#define XMID_CTRL_RESERVED_SIZE             0x00010000UL   /* 64 KB */
#define XMID_CTRL_RESERVED_END \
    (XMID_CTRL_OFFSET + XMID_CTRL_RESERVED_SIZE)


/* Realtime FIFO ----------------------------------------------------------- */

/*
 * FIFO layout:
 *
 *   +0x00  magic
 *   +0x04  version
 *   +0x08  fifo_size
 *   +0x0C  dropped
 *   +0x20  write_idx
 *   +0x40  read_idx
 *   +0x60  events[]
 *
 * 1024 slots x 8 bytes plus 0x60-byte header:
 *
 *   0x60 + 1024 * 8 = 0x2060 bytes
 */

#define XMID_FIFO_OFFSET                    0x06010000UL

#define XMID_FIFO_MAGIC                     0x5A4D4646UL   /* 'ZMFF' */
#define XMID_FIFO_VERSION                   2u
#define XMID_FIFO_SLOTS                     1024u
#define XMID_FIFO_EVENTS_OFF                0x60u

#define XMID_FIFO_SPAN \
    (XMID_FIFO_EVENTS_OFF + XMID_FIFO_SLOTS * 8u)

#define XMID_FIFO_RESERVED_END              0x06100000UL


/* MIDI PCM ring ----------------------------------------------------------- */

#define XMID_PCM_RING_OFFSET                0x06100000UL
#define XMID_PCM_RING_SIZE                  (1UL * 1024UL * 1024UL)
#define XMID_PCM_RING_END \
    (XMID_PCM_RING_OFFSET + XMID_PCM_RING_SIZE)


/* Shared upload window ---------------------------------------------------- */

/*
 * SF2 and MIDI files are transferred through this 1 MB window in chunks.
 *
 * The complete files are copied to ARM-private DDR before being parsed.
 *
 * The old XACP v1.5 large Zorro-visible SF2 and MIDI staging buffers are
 * no longer part of the v1.6 layout.
 */

#define XMID_UPLOAD_OFFSET                  0x06200000UL
#define XMID_UPLOAD_SIZE                    (1UL * 1024UL * 1024UL)
#define XMID_UPLOAD_END \
    (XMID_UPLOAD_OFFSET + XMID_UPLOAD_SIZE)

#define XMID_SHARED_END                     0x06300000UL
#define XMID_SHARED_SIZE \
    (XMID_SHARED_END - XMID_SHARED_START)


/* ARM representation of the same shared corridor ------------------------- */

#define XMID_SHARED_ARM_BASE \
    XACP_FB_OFFSET_TO_ARM_PHYS(XMID_SHARED_START)

#define XMID_SHARED_ARM_END \
    XACP_FB_OFFSET_TO_ARM_PHYS(XMID_SHARED_END)


/* ------------------------------------------------------------------------- */
/* Published application safety boundaries                                  */
/* ------------------------------------------------------------------------- */

/*
 * These boundaries were used when validating the XACP v1.6 ZZMIDI corridor
 * against the released application layouts.
 *
 * Semi-open intervals are used: [base, end).
 */


/* ZZDoom Core1 heap: ARM [0x05A00000, 0x06200000) */

#define XACP_ARM_ZZDOOM_HEAP_BASE           0x05A00000UL
#define XACP_ARM_ZZDOOM_HEAP_END            0x06200000UL


/* ZZDoom save area: ARM [0x07E00000, 0x07F00000) */

#define XACP_ARM_ZZDOOM_SAVE_BASE           0x07E00000UL
#define XACP_ARM_ZZDOOM_SAVE_END            0x07F00000UL


/* ------------------------------------------------------------------------- */
/* ARM absolute physical memory safety zones                                 */
/* ------------------------------------------------------------------------- */

/*
 * Everything in this section is an ARM ABSOLUTE PHYSICAL ADDRESS.
 *
 * These values must never be treated as framebuffer-relative offsets.
 */


/*
 * ARM 0x20000000 .. 0x201F0000
 *
 * This projects into live AmigaOS ZZ9000 Z3 Fast RAM.
 *
 * NEVER USE for ARM-private allocations.
 */

#define XACP_ARM_Z3_COLLISION_BASE           0x20000000UL
#define XACP_ARM_Z3_COLLISION_END            0x201F0000UL


/*
 * ARM 0x201F0000 .. 0x22000000
 *
 * Guard / no-man's-land.
 *
 * Do not use and do not map as part of the ARM-private cacheable pool.
 */

#define XACP_ARM_NO_MANS_LAND_BASE           0x201F0000UL
#define XACP_ARM_NO_MANS_LAND_END            0x22000000UL


/*
 * Safe private XACP window.
 *
 * MMU policy for the current firmware:
 *
 *   map ONLY 0x22000000 .. 0x30000000 as NORM_WB_CACHE
 *
 * Do not map 0x20000000 .. 0x22000000 as ARM-private memory.
 */

#define XACP_ARM_PRIVATE_SAFE_BASE           0x22000000UL
#define XACP_ARM_PRIVATE_SAFE_END            0x30000000UL


/* ------------------------------------------------------------------------- */
/* ZZPicoDrive private allocation                                            */
/* ------------------------------------------------------------------------- */

/*
 * ARM 0x22000000 .. 0x23000000
 *
 * Reserved for ZZPicoDrive / established Core1 private use.
 *
 * ZZMIDI must never allocate below 0x23000000.
 */

#define XACP_ARM_ZZPICO_PRIVATE_BASE         0x22000000UL
#define XACP_ARM_ZZPICO_PRIVATE_SIZE         (16UL * 1024UL * 1024UL)
#define XACP_ARM_ZZPICO_PRIVATE_END \
    (XACP_ARM_ZZPICO_PRIVATE_BASE + XACP_ARM_ZZPICO_PRIVATE_SIZE)


/* ------------------------------------------------------------------------- */
/* ZZMIDI ARM-private allocation                                             */
/* ------------------------------------------------------------------------- */

/*
 * ARM 0x23000000 .. 0x25000000
 *
 * Raw SoundFont private copy.
 *
 * This is a reserved memory allocation size, not necessarily the maximum
 * SoundFont size accepted by a particular ZZMIDI software release.
 */

#define XMID_RAW_SF2_BASE_ABS                0x23000000UL
#define XMID_SF2_POOL_SIZE                   (32UL * 1024UL * 1024UL)
#define XMID_RAW_SF2_END_ABS \
    (XMID_RAW_SF2_BASE_ABS + XMID_SF2_POOL_SIZE)


/*
 * ARM 0x25000000 .. 0x25600000
 *
 * Raw MIDI private copy.
 */

#define XMID_RAW_MIDI_BASE_ABS               0x25000000UL
#define XMID_MIDI_POOL_SIZE                  (6UL * 1024UL * 1024UL)
#define XMID_RAW_MIDI_END_ABS \
    (XMID_RAW_MIDI_BASE_ABS + XMID_MIDI_POOL_SIZE)


/*
 * ARM 0x25600000 .. 0x2F600000
 *
 * TinySoundFont / TinyMidiLoader runtime heap.
 */

#define XMID_HEAP_BASE_ABS                   0x25600000UL
#define XMID_HEAP_SIZE                       (160u * 1024u * 1024u)
#define XMID_HEAP_END_ABS                    0x2F600000UL


/*
 * Complete ZZMIDI-owned private pool:
 *
 *   0x23000000 .. 0x30000000
 */

#define XMID_PRIVATE_POOL_BASE_ABS           0x23000000UL
#define XMID_PRIVATE_POOL_END_ABS            0x30000000UL


/* ------------------------------------------------------------------------- */
/* Guard after ZZMIDI heap                                                   */
/* ------------------------------------------------------------------------- */

/*
 * ARM 0x2F600000 .. 0x30000000
 *
 * Intentionally unused 10 MB guard.
 */

#define XACP_ARM_XMID_GUARD_BASE             0x2F600000UL
#define XACP_ARM_XMID_GUARD_SIZE             (10UL * 1024UL * 1024UL)
#define XACP_ARM_XMID_GUARD_END \
    (XACP_ARM_XMID_GUARD_BASE + XACP_ARM_XMID_GUARD_SIZE)


/* ------------------------------------------------------------------------- */
/* SMUSH reserved region                                                     */
/* ------------------------------------------------------------------------- */

/*
 * ARM 0x30000000 .. 0x33000000
 *
 * Reserved for SMUSH codec use.
 *
 * XACP applications and services must not allocate from this range.
 */

#define XACP_ARM_SMUSH_BASE                  0x30000000UL
#define XACP_ARM_SMUSH_END                   0x33000000UL
#define XACP_ARM_SMUSH_SIZE \
    (XACP_ARM_SMUSH_END - XACP_ARM_SMUSH_BASE)


/* ------------------------------------------------------------------------- */
/* Compile-time sanity checks                                                */
/* ------------------------------------------------------------------------- */

#define XACP_STATIC_ASSERT(name, cond) \
    typedef char xacp_static_assert_##name[(cond) ? 1 : -1]


/* Generic XACP regions ---------------------------------------------------- */

XACP_STATIC_ASSERT(mp3_ring_before_pcm_ring,
    XACP_MP3_RING_END <= XACP_PCM_RING_OFFSET);


/* ZZMIDI shared corridor -------------------------------------------------- */

XACP_STATIC_ASSERT(xmid_ctrl_starts_corridor,
    XMID_CTRL_OFFSET == XMID_SHARED_START);

XACP_STATIC_ASSERT(xmid_ctrl_struct_fits_reserved_area,
    XMID_CTRL_STRUCT_SIZE <= XMID_CTRL_RESERVED_SIZE);

XACP_STATIC_ASSERT(xmid_ctrl_reserved_ends_at_fifo,
    XMID_CTRL_RESERVED_END == XMID_FIFO_OFFSET);

XACP_STATIC_ASSERT(xmid_fifo_span_is_0x2060,
    XMID_FIFO_SPAN == 0x2060UL);

XACP_STATIC_ASSERT(xmid_fifo_fits_before_pcm,
    XMID_FIFO_OFFSET + XMID_FIFO_SPAN <= XMID_PCM_RING_OFFSET);

XACP_STATIC_ASSERT(xmid_fifo_reserved_area_ends_at_pcm,
    XMID_FIFO_RESERVED_END == XMID_PCM_RING_OFFSET);

XACP_STATIC_ASSERT(xmid_pcm_is_1mb,
    XMID_PCM_RING_SIZE == (1UL * 1024UL * 1024UL));

XACP_STATIC_ASSERT(xmid_pcm_ends_at_upload,
    XMID_PCM_RING_END == XMID_UPLOAD_OFFSET);

XACP_STATIC_ASSERT(xmid_upload_is_1mb,
    XMID_UPLOAD_SIZE == (1UL * 1024UL * 1024UL));

XACP_STATIC_ASSERT(xmid_upload_ends_corridor,
    XMID_UPLOAD_END == XMID_SHARED_END);

XACP_STATIC_ASSERT(xmid_shared_corridor_is_3mb,
    XMID_SHARED_SIZE == (3UL * 1024UL * 1024UL));


/* Shared offset -> ARM physical translation ------------------------------- */

XACP_STATIC_ASSERT(xmid_shared_arm_base_is_0x06200000,
    XMID_SHARED_ARM_BASE == 0x06200000UL);

XACP_STATIC_ASSERT(xmid_shared_arm_end_is_0x06500000,
    XMID_SHARED_ARM_END == 0x06500000UL);


/* Released-application collision checks ---------------------------------- */

XACP_STATIC_ASSERT(xmid_corridor_starts_after_zzdoom_heap,
    XMID_SHARED_ARM_BASE >= XACP_ARM_ZZDOOM_HEAP_END);

XACP_STATIC_ASSERT(xmid_corridor_ends_before_zzdoom_saves,
    XMID_SHARED_ARM_END <= XACP_ARM_ZZDOOM_SAVE_BASE);


/* Unsafe / safe ARM boundary --------------------------------------------- */

XACP_STATIC_ASSERT(z3_collision_ends_at_no_mans_land,
    XACP_ARM_Z3_COLLISION_END == XACP_ARM_NO_MANS_LAND_BASE);

XACP_STATIC_ASSERT(no_mans_land_ends_at_private_safe_base,
    XACP_ARM_NO_MANS_LAND_END == XACP_ARM_PRIVATE_SAFE_BASE);


/* ZZPicoDrive private allocation ----------------------------------------- */

XACP_STATIC_ASSERT(zzpico_starts_at_private_safe_base,
    XACP_ARM_ZZPICO_PRIVATE_BASE == XACP_ARM_PRIVATE_SAFE_BASE);

XACP_STATIC_ASSERT(zzpico_private_is_16mb,
    XACP_ARM_ZZPICO_PRIVATE_SIZE == (16UL * 1024UL * 1024UL));

XACP_STATIC_ASSERT(zzpico_private_ends_at_xmid_pool,
    XACP_ARM_ZZPICO_PRIVATE_END == XMID_PRIVATE_POOL_BASE_ABS);


/* ZZMIDI ARM-private allocation ------------------------------------------ */

XACP_STATIC_ASSERT(xmid_sf2_is_32mb,
    XMID_SF2_POOL_SIZE == (32UL * 1024UL * 1024UL));

XACP_STATIC_ASSERT(xmid_sf2_ends_at_midi,
    XMID_RAW_SF2_END_ABS == XMID_RAW_MIDI_BASE_ABS);

XACP_STATIC_ASSERT(xmid_midi_is_6mb,
    XMID_MIDI_POOL_SIZE == (6UL * 1024UL * 1024UL));

XACP_STATIC_ASSERT(xmid_midi_ends_at_heap,
    XMID_RAW_MIDI_END_ABS == XMID_HEAP_BASE_ABS);

XACP_STATIC_ASSERT(xmid_heap_is_160mb,
    XMID_HEAP_SIZE == (160UL * 1024UL * 1024UL));

XACP_STATIC_ASSERT(xmid_heap_calculated_end_matches,
    XMID_HEAP_BASE_ABS + XMID_HEAP_SIZE == XMID_HEAP_END_ABS);

XACP_STATIC_ASSERT(xmid_heap_ends_at_guard,
    XMID_HEAP_END_ABS == XACP_ARM_XMID_GUARD_BASE);

XACP_STATIC_ASSERT(xmid_guard_is_10mb,
    XACP_ARM_XMID_GUARD_SIZE == (10UL * 1024UL * 1024UL));

XACP_STATIC_ASSERT(xmid_guard_ends_at_private_safe_end,
    XACP_ARM_XMID_GUARD_END == XACP_ARM_PRIVATE_SAFE_END);

XACP_STATIC_ASSERT(xmid_pool_starts_at_sf2,
    XMID_PRIVATE_POOL_BASE_ABS == XMID_RAW_SF2_BASE_ABS);

XACP_STATIC_ASSERT(xmid_pool_ends_at_0x30000000,
    XMID_PRIVATE_POOL_END_ABS == 0x30000000UL);


/* SMUSH boundary --------------------------------------------------------- */

XACP_STATIC_ASSERT(smush_starts_after_private_safe_window,
    XACP_ARM_SMUSH_BASE == XACP_ARM_PRIVATE_SAFE_END);

XACP_STATIC_ASSERT(smush_is_48mb,
    XACP_ARM_SMUSH_SIZE == (48UL * 1024UL * 1024UL));


#ifdef __cplusplus
}
#endif

#endif /* XACP_MEMORY_MAP_V1_6_H */

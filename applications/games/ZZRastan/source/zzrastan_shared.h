#ifndef ZZRASTAN_SHARED_H
#define ZZRASTAN_SHARED_H

/* ZZRastan A0/A1 shared contract. ASCII only. */

typedef unsigned char  zzr_u8;
typedef unsigned short zzr_u16;
typedef signed short   zzr_s16;
typedef unsigned int   zzr_u32;
typedef signed int     zzr_s32;

#define ZZR_MAGIC       0x5A525354u /* 'ZRST' */
#define ZZR_VERSION     0x00010001u

/* XX19 / XACP v1.5 physical map. */
#define ZZR_ARM_BLOB        0x04500000u
#define ZZR_ARM_SHARED      0x04700000u
#define ZZR_ARM_ROM_STAGE   0x04B00000u
#define ZZR_ARM_MAINCPU     0x04C00000u
#define ZZR_ARM_GFX_STAGE   0x04D00000u   /* PC080SN raw (interleaved)   */
#define ZZR_ARM_GFX_FINAL   0x04E00000u   /* PC080SN deinterleaved       */
#define ZZR_ARM_OBJ_STAGE   0x04F00000u   /* PC090OJ sprites raw         */
#define ZZR_ARM_OBJ_FINAL   0x05000000u   /* PC090OJ sprites deinterleaved */
#define ZZR_ARM_Z80ROM      0x05100000u   /* b04-19.49 sound ROM, 64 KB (68k: fb+0x04F00000) */
#define ZZR_ARM_ADPCM       0x05110000u   /* b04-20.76 MSM5205 ADPCM, 64 KB (68k: fb+0x04F10000) */
#define ZZR_Z80ROM_SIZE     0x10000u
#define ZZR_ADPCM_SIZE      0x10000u
#define ZZR_ARM_VIDEO       0x05600000u
#define ZZR_ARM_AUDIO       0x05700000u
#define ZZR_ARM_DEBUG       0x05800000u
#define ZZR_ARM_BSS         0x22000000u
#define ZZR_ARM_BSS_END     0x22400000u
#define ZZR_ARM_HEAP        0x22800000u
#define ZZR_ARM_HEAP_SIZE   (8u * 1024u * 1024u)

#define ZZR_FB_BLOB         0x04300000u
#define ZZR_FB_SHARED       0x04500000u
#define ZZR_FB_ROM_STAGE    0x04900000u
#define ZZR_FB_GFX_STAGE    0x04B00000u   /* -> ARM 0x04D00000 */
#define ZZR_FB_OBJ_STAGE    0x04D00000u   /* -> ARM 0x04F00000 */

#define ZZR_REG_ARM_RUN_HI  0x0090u
#define ZZR_REG_ARM_RUN_LO  0x0092u

#define ZZR_ROM_FILE_COUNT  6u
#define ZZR_ROM_FILE_SIZE   0x00010000u
#define ZZR_ROM_STAGE_SIZE  0x00060000u
#define ZZR_MAINCPU_SIZE    0x00060000u
#define ZZR_ROM_PAD         256u
#define ZZR_RAW_CRC32       0x14789E54u
#define ZZR_MAIN_CRC32      0x87BF5F45u

/* B0b: PC080SN graphics. 4 files x 128K = 512K, two ROM_LOAD16_BYTE pairs. */
#define ZZR_GFX_FILE_COUNT  4u
#define ZZR_GFX_FILE_SIZE   0x00020000u
#define ZZR_GFX_STAGE_SIZE  0x00080000u
#define ZZR_GFX_FINAL_SIZE  0x00080000u
#define ZZR_OBJ_FILE_COUNT  4u
#define ZZR_OBJ_FILE_SIZE   0x00020000u
#define ZZR_OBJ_STAGE_SIZE  0x00080000u
#define ZZR_OBJ_FINAL_SIZE  0x00080000u

/* B0b video geometry */
#define ZZR_IMG_W           320u
#define ZZR_IMG_H           240u

/* Main control block: exactly 32 u32 slots before debug. */
typedef struct zzrastan_debug_s {
    zzr_u32 magic;
    zzr_u32 version;
    zzr_u32 status;
    zzr_u32 error;
    zzr_u32 frame_count;
    zzr_u32 irq5_count;
    zzr_u32 fps;
    zzr_u32 m68k_pc;
    zzr_u32 m68k_sp;
    zzr_u32 m68k_sr;
    zzr_u32 m68k_cycles;
    zzr_u32 palette_writes;
    zzr_u32 tilemap_writes;
    zzr_u32 spriteram_writes;
    zzr_u32 yscroll_writes;
    zzr_u32 xscroll_writes;
    zzr_u32 ctrl_writes;
    zzr_u32 pc090oj_writes;
    zzr_u32 pc060ha_cmd_count;
    zzr_u32 pc060ha_port_writes;
    zzr_u32 pc060ha_comm_reads;
    zzr_u32 watchdog_writes;
    zzr_u32 bad_read_count;
    zzr_u32 bad_write_count;
    zzr_u32 bus_error_count;
    zzr_u32 illegal_opcode_count;
    zzr_u32 frame_total_us;
    zzr_u32 m68k_us;
    zzr_u32 video_us;
    zzr_u32 audio_us;
    zzr_u32 last_bad_addr;
    zzr_u32 last_bad_value;
    zzr_u32 last_sound_cmd;
    zzr_u32 pc060ha_port;
    zzr_u32 same_pc_frames;
    zzr_u32 cpu_state_flags;
} zzrastan_debug_t;

typedef struct zzrastan_shared_s {
    volatile zzr_u32 magic;            /* 0 */
    volatile zzr_u32 version;          /* 1 */
    volatile zzr_u32 status;           /* 2 */
    volatile zzr_u32 command;          /* 3 */
    volatile zzr_u32 cmd_seq;          /* 4 */
    volatile zzr_u32 ack_seq;          /* 5 */
    volatile zzr_u32 heartbeat;        /* 6 */
    volatile zzr_u32 frame_counter;    /* 7 */
    volatile zzr_u32 error;            /* 8 */
    volatile zzr_u32 diag;             /* 9 */
    volatile zzr_u32 rom_stage_addr;   /* 10 */
    volatile zzr_u32 rom_stage_size;   /* 11 */
    volatile zzr_u32 rom_raw_crc_68k;  /* 12 */
    volatile zzr_u32 rom_raw_crc_arm;  /* 13 */
    volatile zzr_u32 rom_main_crc_arm; /* 14 */
    volatile zzr_u32 reset_ssp;        /* 15 */
    volatile zzr_u32 reset_pc;         /* 16 */
    volatile zzr_u32 input_p1;         /* 17 */
    volatile zzr_u32 input_p2;         /* 18 */
    volatile zzr_u32 input_special;    /* 19 */
    volatile zzr_u32 input_system;     /* 20 */
    volatile zzr_u32 input_dswa;       /* 21 */
    volatile zzr_u32 input_dswb;       /* 22 */
    volatile zzr_u32 step_req;         /* 23 */
    volatile zzr_u32 step_done;        /* 24 */
    volatile zzr_u32 run_flags;        /* 25 */
    volatile zzr_u32 fault_pc;         /* 26: ARM vector ABI */
    volatile zzr_u32 fault_far;        /* 27: ARM vector ABI */
    volatile zzr_u32 fault_fsr;        /* 28: ARM vector ABI */
    volatile zzr_u32 reserved29;
    volatile zzr_u32 reserved30;
    volatile zzr_u32 reserved31;
    volatile zzrastan_debug_t debug;   /* slots 32..67 */
    /* --- B0b: P96 handoff (identical semantics to the validated B0a path) --- */
    volatile zzr_u32 p96_enable;       /* 68 */
    volatile zzr_u32 p96_base;         /* 69: ARM address of the free backbuffer */
    volatile zzr_u32 p96_pitch;        /* 70: bytes per row */
    volatile zzr_u32 frame_ready;      /* 71: blob -> 68k */
    volatile zzr_u32 flip_seq;         /* 72: 68k -> blob ("taken, render next") */
    /* --- B0b: video instrumentation --- */
    volatile zzr_u32 palette_nonzero_entries; /* 73 */
    volatile zzr_u32 bg_nonzero_tiles;        /* 74 */
    volatile zzr_u32 bg_first_code;           /* 75 */
    volatile zzr_u32 bg_first_attr;           /* 76 */
    volatile zzr_u32 gfx_out_of_range;        /* 77: MUST stay 0 */
    volatile zzr_u32 color_out_of_range;      /* 78: color >= 128 (palette is 2048 = 128*16) */
    volatile zzr_u32 framebuffer_nonzero_pixels; /* 79 */
    volatile zzr_u32 framebuffer_crc;         /* 80 */
    volatile zzr_u32 render_cycles;           /* 81 */
    volatile zzr_u32 exec_from_ram_count;     /* 82 */
    volatile zzr_u32 last_exec_addr;          /* 83 */
    volatile zzr_u32 gfx_raw_crc;             /* 84 */
    volatile zzr_u32 gfx_final_crc;           /* 85 */
    volatile zzr_u32 rendered_frames;         /* 86 */
    /* --- B1 : split timing (last + max), MMU descriptors, scroll --- */
    volatile zzr_u32 render_core_cyc;         /* 87 */
    volatile zzr_u32 render_clean_cyc;        /* 88 */
    volatile zzr_u32 render_total_cyc;        /* 89 */
    volatile zzr_u32 render_core_max;         /* 90 */
    volatile zzr_u32 render_total_max;        /* 91 */
    volatile zzr_u32 mmu_bss_desc;            /* 92 */
    volatile zzr_u32 mmu_heap_desc;           /* 93 */
    volatile zzr_u32 mmu_gfx_desc;            /* 94 */
    volatile zzr_u32 mmu_p96_desc;            /* 95 */
    volatile zzr_u32 bg_scrollx;              /* 96: raw 68k value */
    volatile zzr_u32 bg_scrolly;              /* 97 */
    volatile zzr_u32 fg_scrollx;              /* 98 */
    volatile zzr_u32 fg_scrolly;              /* 99 */
    volatile zzr_u32 pc080sn_ctrl;            /* 100 */
    volatile zzr_u32 rowscroll_nonzero;       /* 101: BG rowscroll words != 0 */
    volatile zzr_u32 rowscroll_min;           /* 102 */
    volatile zzr_u32 rowscroll_max;           /* 103 */
    volatile zzr_u32 pace_resyncs;            /* 104: cadence resynchronisations */
    volatile zzr_u32 total_run_cyc;           /* 105 */
    volatile zzr_u32 fg_nonzero_tiles;        /* 106 */
    volatile zzr_u32 fg_nonzero_px;           /* 107 */
    volatile zzr_u32 obj_raw_crc;             /* 108 */
    volatile zzr_u32 obj_final_crc;           /* 109 */
    volatile zzr_u32 spr_active;              /* 110: sprites with nonzero data */
    volatile zzr_u32 spr_nonzero_px;          /* 111: sprite pixels drawn */
    volatile zzr_u32 spr_ctrl;                /* 112: sprite_ctrl (0x380000) */
    volatile zzr_u32 spr_first_code;          /* 113 */
    volatile zzr_u32 spr_first_xy;            /* 114: (x<<16)|y of first active */
    volatile zzr_u32 spr_oor;                 /* 115 */
    volatile zzr_u32 dbg_sysrd;               /* 116: count of 68k reads of 0x390006 */
    volatile zzr_u32 dbg_coinrd;              /* 117 */
    /* --- P2 per-stage PMU profile (last values, cycles @666MHz) --- */
    volatile zzr_u32 prof_palette;            /* 118 */
    volatile zzr_u32 prof_rowscroll;          /* 119 */
    volatile zzr_u32 prof_bg;                 /* 120 */
    volatile zzr_u32 prof_fg;                 /* 121 */
    volatile zzr_u32 prof_sprites;            /* 122 */
    volatile zzr_u32 prof_crc;                /* 123 */
    volatile zzr_u32 prof_clean;              /* 124 */
    volatile zzr_u32 prof_total_max;          /* 125 */
    /* --- A0a: PCM ring (ZZPicoDrive/ZZDoom model). S16 mono duplicated L/R in a stereo-agnostic
       byte ring at ARM 0x05700000 (NC), consumed by the 68k via AHI. --- */
    volatile zzr_u32 pcm_enable;              /* 126: 1 = ARM produces into the ring */
    volatile zzr_u32 pcm_base;                /* 127: ARM address of the ring */
    volatile zzr_u32 pcm_size;                /* 128: ring size in bytes */
    volatile zzr_u32 pcm_write_pos;           /* 129: ARM -> 68k, byte write cursor */
    volatile zzr_u32 pcm_read_pos;            /* 130: 68k -> ARM, byte read cursor */
    volatile zzr_u32 pcm_rate;                /* 131: sample rate (Hz) */
    volatile zzr_u32 pcm_underruns;           /* 132 */
    /* --- A1: sound board (Z80 + PC060HA), silent diagnostics --- */
    volatile zzr_u32 snd_enable;              /* 133: 1 = run the Z80 */
    volatile zzr_u32 snd_z80_pc;              /* 134: last Z80 PC */
    volatile zzr_u32 snd_cmds_68k;            /* 135: commands written by the 68000 */
    volatile zzr_u32 snd_nmi_count;           /* 136: NMIs delivered to the Z80 */
    volatile zzr_u32 snd_ym_writes;           /* 137: Z80 writes to YM2151 regs */
    volatile zzr_u32 snd_ym_reads;            /* 138: Z80 reads of YM2151 status */
    volatile zzr_u32 snd_bank_changes;        /* 139: ROM bank switches */
    volatile zzr_u32 snd_last_cmd;            /* 140: last sound command byte */
    volatile zzr_u32 snd_last_ym_reg;         /* 141: last YM register written */
    volatile zzr_u32 snd_last_ym_val;         /* 142: last YM value written */
    volatile zzr_u32 snd_msm_writes;          /* 143: MSM5205 register writes */
    volatile zzr_u32 snd_z80_cycles;          /* 144: cycles executed last frame */
    volatile zzr_u32 snd_ym_irqs;             /* 145: YM2151 timer IRQs raised to the Z80 */
    volatile zzr_u32 snd_ym_samples;          /* 146: FM samples generated */
    volatile zzr_u32 snd_ym_native_rate;      /* 147 */
    volatile zzr_u32 snd_tA_sets;             /* 148 */
    volatile zzr_u32 snd_tB_sets;             /* 149 */
    volatile zzr_u32 snd_tA_fires;            /* 150 */
    volatile zzr_u32 snd_tB_fires;            /* 151 */
    volatile zzr_u32 snd_tA_period;           /* 152 */
    volatile zzr_u32 snd_tB_period;           /* 153 */
    volatile zzr_u32 snd_irq_asserts;         /* 154 */
    volatile zzr_u32 snd_irq_clears;          /* 155 */
    volatile zzr_u32 snd_reg10;               /* 156 */
    volatile zzr_u32 snd_reg11;               /* 157 */
    volatile zzr_u32 snd_reg12;               /* 158 */
    volatile zzr_u32 snd_reg14;               /* 159 */
    volatile zzr_u32 snd_key_writes;          /* 160 */
    volatile zzr_u32 snd_key_on;              /* 161 */
    volatile zzr_u32 snd_key_off;             /* 162 */
    volatile zzr_u32 snd_nonzero;             /* 163 */
    volatile zzr_u32 snd_peak;                /* 164 */
    volatile zzr_u32 snd_nmi_enables;         /* 165: TRUE enables (A001 under mode 6) */
    volatile zzr_u32 snd_nmi_disables;        /* 166 */
    volatile zzr_u32 snd_pair01_to_z80;       /* 167 */
    volatile zzr_u32 snd_pair23_to_z80;       /* 168 */
    volatile zzr_u32 snd_ack01_by_z80;        /* 169 */
    volatile zzr_u32 snd_ack23_by_z80;        /* 170 */
    volatile zzr_u32 snd_z80_resets;          /* 171 */
    volatile zzr_u32 snd_ciu_status;          /* 172 */
    volatile zzr_u32 snd_port_fwd;            /* 173: master_port_w writes forwarded */
    volatile zzr_u32 snd_last_mainmode;       /* 174 */
    volatile zzr_u32 snd_ym_read0;            /* 175 */
    volatile zzr_u32 snd_last_status;         /* 176: last value returned for 0x9001 */
    volatile zzr_u32 snd_status_busy_reads;   /* 177 */
    volatile zzr_u32 snd_q_head;              /* 178: Z80 RAM 0x8F00 queue head */
    volatile zzr_u32 snd_q_tail;              /* 179: Z80 RAM 0x8F01 queue tail */
    volatile zzr_u32 snd_flag8f27;            /* 180: 0x8F27 - bit0 set => commands parked */
    volatile zzr_u32 snd_park8f28;            /* 181: 0x8F28 - parked command */
    volatile zzr_u32 snd_q_slots;             /* 182 */
    volatile zzr_u32 snd_qhead_writes;        /* 183 */
    volatile zzr_u32 snd_qtail_writes;        /* 184 */
    volatile zzr_u32 snd_qslot_writes;        /* 185 */
    volatile zzr_u32 snd_q_last_written;      /* 186 */
    volatile zzr_u32 snd_qslot_reads;         /* 187 */
    volatile zzr_u32 snd_q_last_read;         /* 188 */
    volatile zzr_u32 snd_table_reads;         /* 189 */
    volatile zzr_u32 snd_table_last_cmd;      /* 190 */
    volatile zzr_u32 snd_track1a_count;       /* 191 */
    volatile zzr_u32 snd_cmd_seen_by_z80;     /* 192 */
    volatile zzr_u32 snd_cmd_complete;        /* 193 */
    volatile zzr_u32 snd_ef_seen;             /* 194 */
    volatile zzr_u32 snd_ee_seen;             /* 195 */
    volatile zzr_u32 snd_ef_queued;           /* 196 */
    volatile zzr_u32 snd_ef_dequeued;         /* 197 */
    volatile zzr_u32 snd_gate_set0;           /* 198 */
    volatile zzr_u32 snd_gate_set1;           /* 199 */
    volatile zzr_u32 snd_gate_value;          /* 200 */
    volatile zzr_u32 snd_trackinit_entries;   /* 201 */
    volatile zzr_u32 snd_prof_z80;            /* 202: cumulative cycles in Cz80_Exec */
    volatile zzr_u32 snd_prof_ym;             /* 203: cumulative cycles generating FM */
    volatile zzr_u32 snd_prof_timer;          /* 204 */
    volatile zzr_u32 snd_prof_slice_total;    /* 205: whole sound slice incl FIFO/stats */
    volatile zzr_u32 prof_cyclone;            /* 206: 68000 emulation */
    volatile zzr_u32 prof_audio_produce;      /* 207: ring producer / resampler */
    volatile zzr_u32 prof_frame_work;         /* 208: everything before pacing */
    volatile zzr_u32 prof_pace_wait;          /* 209: time spent waiting in pace_60hz */
    volatile zzr_u32 prof_frames;             /* 210 */
    volatile zzr_u32 snd_prof_ympost;         /* 211: postmix (clamp/downmix) time */
    volatile zzr_u32 bootev_count;            /* 212 */
    volatile zzr_u32 bootev[32][4];           /* 213..: frame, type, value, flags */
    volatile zzr_u32 aq_generated;            /* 341 */
    volatile zzr_u32 aq_enqueued;             /* 342 */
    volatile zzr_u32 aq_drop_old;             /* 343 */
    volatile zzr_u32 aq_highwater;            /* 344 */
    volatile zzr_u32 aq_pumps;                /* 345 */
    volatile zzr_u32 aq_fifo_used;            /* 346 */
    volatile zzr_u32 pace_waits;              /* 347 */
    volatile zzr_u32 pace_catchup;            /* 348 */
    volatile zzr_u32 pace_hard;               /* 349 */
    volatile zzr_u32 pace_max_late_us;        /* 350 */
    volatile zzr_u32 aq_discarded_off;        /* 351 */
    volatile zzr_u32 aq_act_generated;        /* 352 */
    volatile zzr_u32 aq_act_enqueued;         /* 353 */
    volatile zzr_u32 aq_act_drop;             /* 354 */
    volatile zzr_u32 aq_act_high;             /* 355 */
    volatile zzr_u32 aq_act_pumps;            /* 356 */
    volatile zzr_u32 aq_act_frozen;           /* 357 */
    volatile zzr_u32 pace_period_cycles;      /* 358 */
    volatile zzr_u32 msm_starts;              /* 359 */
    volatile zzr_u32 msm_stops;               /* 360 */
    volatile zzr_u32 msm_nibbles;             /* 361 */
    volatile zzr_u32 msm_peak;                /* 362 */
    volatile zzr_u32 snd_reg1b_writes;        /* 363 */
    volatile zzr_u32 snd_reg1b_raw;           /* 364 */
    volatile zzr_u32 snd_reg1b_ct;            /* 365 */
    volatile zzr_u32 snd_bank_mismatch;       /* 366 */
    volatile zzr_u32 snd_bank_current;        /* 367 */
    volatile zzr_u32 ring_pre_min;            /* 368 */
    volatile zzr_u32 ring_pre_max;            /* 369 */
    volatile zzr_u32 ring_pre_avg;            /* 370 */
    volatile zzr_u32 ring_post_min;           /* 371 */
    volatile zzr_u32 ring_post_max;           /* 372 */
    volatile zzr_u32 ring_post_avg;           /* 373 */
    /* --- P1 PAN pipeline probe: Core1 video gate instrumentation (appended at the END so no
       existing offset moves; no existing diagnostic slot is reused). MEASURE ONLY. --- */
    volatile zzr_u32 video_present_calls;     /* 374 */
    volatile zzr_u32 video_gate_pass;         /* 375 */
    volatile zzr_u32 video_gate_block;        /* 376 */
    volatile zzr_u32 video_gate_block_cons_max; /* 377 */
    volatile zzr_u32 render_start_cpu_frame;  /* 378 */
    volatile zzr_u32 render_ready_cpu_frame;  /* 379 */
    volatile zzr_u32 last_gate_flip_seq;      /* 380 */
    volatile zzr_u32 last_gate_frame_seq;     /* 381 */
    /* --- P2A: asynchronous triple buffering. RENDER_SEQ is a NEW authorisation counter; it does
       NOT replace FLIP_SEQ, which keeps its meaning "frame actually acked and displayed". --- */
    volatile zzr_u32 render_seq;              /* 382: 68k -> Core1, "you may render this one" */
    volatile zzr_u32 render_done_seq;         /* 383: Core1 -> 68k, last one it finished */
    volatile zzr_u32 prof_clear;              /* 384 */
    volatile zzr_u32 prof_convert;            /* 385 */
    volatile zzr_u32 prof_video_per_render;   /* 386: us, averaged over RENDERED frames only */
    volatile zzr_u32 prof_rendered_frames;    /* 387 */
    volatile zzr_u32 gfx_dec_bytes;           /* 388 */
    volatile zzr_u32 p2e_bg_a;                /* 389 */
    volatile zzr_u32 p2e_fg_a;                /* 390 */
    volatile zzr_u32 p2e_bg_b;                /* 391 */
    volatile zzr_u32 p2e_fg_b;                /* 392 */
    volatile zzr_u32 p2e_bg_c;                /* 393 */
    volatile zzr_u32 p2e_fg_c;                /* 394 */
    volatile zzr_u32 p2e_frames_a;            /* 395 */
    volatile zzr_u32 p2e_frames_b;            /* 396 */
    volatile zzr_u32 p2e_frames_c;            /* 397 */
    volatile zzr_u32 p2e_bg_stores;           /* 398 */
    volatile zzr_u32 p2e_fg_stores;           /* 399 */
    volatile zzr_u32 fg_mask_bytes;           /* 400 */
    volatile zzr_u32 fg_runs_total;           /* 401 */
    volatile zzr_u32 fg_runs_skipped;         /* 402 */
    volatile zzr_u32 fg_px_avoided;           /* 403 */
    volatile zzr_u32 prof_bg_avg_us;          /* 404 */
    volatile zzr_u32 prof_fg_avg_us;          /* 405 */
    volatile zzr_u32 pause_flag;              /* 406: 68k -> Core1, freeze the emulation */
    volatile zzr_u32 paused_frames;           /* 407 */
    volatile zzr_u32 trainer_flags;           /* 408: bit0 = infinite energy, bit1 = invulnerable */
    volatile zzr_u32 trainer_value;           /* 409: value held in the flag word */
    volatile zzr_u32 trainer_writes;          /* 410 */
    volatile zzr_u32 trainer_lives;           /* 411 */
    volatile zzr_u32 trainer_energy;          /* 412 */
    volatile zzr_u32 rom_set;                 /* 413: 0 = World, 1 = Japan */
} zzrastan_shared_t;

enum zzrastan_status {
    ZZR_ST_BOOTING = 0,
    ZZR_ST_READY = 1,
    ZZR_ST_RUNNING = 2,
    ZZR_ST_STOPPING = 3,
    ZZR_ST_ERROR = 0xEE,
    ZZR_ST_STOPPED = 0xFF
};

enum zzrastan_command {
    ZZR_CMD_NONE = 0,
    ZZR_CMD_INIT = 1,
    ZZR_CMD_LOAD_ROMS = 2,
    ZZR_CMD_START = 3,
    ZZR_CMD_STOP = 4,
    ZZR_CMD_STEP_FRAME = 5,
    ZZR_CMD_SELFTEST = 6
};

enum zzrastan_error {
    ZZR_ERR_NONE = 0,
    ZZR_ERR_BAD_MAGIC = 1,
    ZZR_ERR_ROM_NULL = 2,
    ZZR_ERR_ROM_SIZE = 3,
    ZZR_ERR_ROM_RAW_CRC = 4,
    ZZR_ERR_ROM_FILE_CRC = 5,
    ZZR_ERR_ROM_MAIN_CRC = 6,
    ZZR_ERR_BAD_VECTOR = 7,
    ZZR_ERR_MEMMAP = 8,
    ZZR_ERR_PC060HA = 9,
    ZZR_ERR_CPU_RESET = 10,
    ZZR_ERR_CPU_ILLEGAL = 11,
    ZZR_ERR_DATA_ABORT = 12,
    ZZR_ERR_PREFETCH_ABORT = 13,
    ZZR_ERR_UNDEF_INSTR = 14,
    ZZR_ERR_EXEC_FROM_RAM = 15,
    ZZR_ERR_GFX_SIZE = 16
};

enum zzrastan_diag {
    ZZR_DIAG_BOOT = 0xA000,
    ZZR_DIAG_MMU_OK = 0xA010,
    ZZR_DIAG_VECTORS_OK = 0xA020,
    ZZR_DIAG_READY = 0xA030,
    ZZR_DIAG_A0_INIT_OK = 0xA100,
    ZZR_DIAG_A0_ROM_OK = 0xA200,
    ZZR_DIAG_A0_DEINTERLEAVE_OK = 0xA210,
    ZZR_DIAG_A0_VECTOR_OK = 0xA220,
    ZZR_DIAG_A0_MEMMAP_OK = 0xA230,
    ZZR_DIAG_A0_PC060HA_OK = 0xA240,
    ZZR_DIAG_A1_CPU_RESET_OK = 0xA300,
    ZZR_DIAG_A1_RUNNING = 0xA310,
    ZZR_DIAG_B0B_GFX_OK = 0xB0B0,
    ZZR_DIAG_B0B_RENDERING = 0xB0B1
};

#endif

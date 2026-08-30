/* ARM Cortex-A9 MMU diagnostic helpers. */
typedef unsigned int u32;

u32 read_sctlr(void){ u32 r; __asm__ volatile("mrc p15,0,%0,c1,c0,0":"=r"(r)); return r; }
u32 read_ttbr0(void){ u32 r; __asm__ volatile("mrc p15,0,%0,c2,c0,0":"=r"(r)); return r; }
u32 read_ttbcr(void){ u32 r; __asm__ volatile("mrc p15,0,%0,c2,c0,2":"=r"(r)); return r; }
u32 read_dacr(void){ u32 r; __asm__ volatile("mrc p15,0,%0,c3,c0,0":"=r"(r)); return r; }

/* mmu_desc_for(va): read TTBR0, mask low 14 bits, index L1 table by va>>20 */
u32 mmu_desc_for(u32 va){
    u32 ttbr, desc;
    __asm__ volatile("mrc p15,0,%0,c2,c0,0":"=r"(ttbr));
    ttbr &= ~0x3FFFu;              /* bfc r3,#0,#14 */
    desc = ((volatile u32*)ttbr)[va>>20];
    return desc;
}

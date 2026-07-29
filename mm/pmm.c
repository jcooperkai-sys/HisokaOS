/* pmm.c - bitmap allocator over physical RAM. Each bit = one 4 KiB frame
 * (0 = free, 1 = used). The first 1 MiB and the kernel image are reserved. */
#include "pmm.h"
#include "string.h"

#define FRAME_SIZE 4096
#define MAX_FRAMES (512 * 1024 / 4)      /* support up to 512 MiB of RAM */

static uint8_t  bitmap[MAX_FRAMES / 8];
static uint32_t total_frames, used_frames;

extern uint32_t kernel_end;              /* from linker.ld */

static void set_used(uint32_t f)  { bitmap[f / 8] |=  (1 << (f % 8)); }
static void set_free(uint32_t f)  { bitmap[f / 8] &= ~(1 << (f % 8)); }
static int  is_used(uint32_t f)   { return bitmap[f / 8] & (1 << (f % 8)); }

void pmm_init(uint32_t mem_upper_kb) {
    uint32_t ram = (1024 + mem_upper_kb) * 1024;          /* total bytes */
    total_frames = ram / FRAME_SIZE;
    if (total_frames > MAX_FRAMES) total_frames = MAX_FRAMES;

    memset(bitmap, 0, sizeof(bitmap));
    used_frames = 0;

    /* reserve everything below the end of the kernel (incl. low 1 MiB) */
    uint32_t reserved = ((uint32_t)&kernel_end + FRAME_SIZE - 1) / FRAME_SIZE;
    for (uint32_t f = 0; f < reserved && f < total_frames; f++) { set_used(f); used_frames++; }
}

uint32_t pmm_alloc_frame(void) {
    for (uint32_t f = 0; f < total_frames; f++) {
        if (!is_used(f)) { set_used(f); used_frames++; return f * FRAME_SIZE; }
    }
    return 0;   /* out of memory */
}

void pmm_free_frame(uint32_t addr) {
    uint32_t f = addr / FRAME_SIZE;
    if (f < total_frames && is_used(f)) { set_free(f); used_frames--; }
}

uint32_t pmm_free_frames(void)  { return total_frames - used_frames; }
uint32_t pmm_total_frames(void) { return total_frames; }

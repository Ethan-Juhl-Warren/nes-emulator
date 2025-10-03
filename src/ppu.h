/**
 * @file   ppu.h
 *
 * @brief  Picture Processing Unit: renders 262 scanlines x 341 cycles each
 *         frame, fetches background tiles and sprites, handles scrolling and
 *         palette mirroring
 *
 * @author E Warren (ethanwarren768@gmail.com)
 * @date   2025-10-04
 */
#ifndef PPU_H
#define PPU_H

#include <stdint.h>
#include <stdbool.h>

// Frame buffer dimensions in pixels
#define FRAME_BUFFER_WIDTH 256
#define FRAME_BUFFER_HEIGHT 240

/**
 * Represents the NES Picture Processing Unit (PPU) state.
 */
typedef struct {
    uint32_t framebuffer[FRAME_BUFFER_WIDTH * FRAME_BUFFER_HEIGHT]; 
        // 32-bit RGBA framebuffer for rendered pixels
    uint64_t ppu_cycles; 
        // Master cycle counter (341 cycles per scanline)
    int scanline;        
        // Current scanline (0–261)
    bool frame_done;     
        // True when a full frame has been rendered
} PPU;

/**
 * Initializes the PPU state.
 *
 * This function clears the framebuffer, resets cycle and scanline counters,
 * and prepares the PPU for rendering frames.
 *
 * @param[in,out] p
 *     Pointer to the PPU instance to initialize.
 */
void ppu_init(PPU *p);

/**
 * Advances the PPU by one cycle.
 *
 * This function simulates the PPU’s internal timing. Once all
 * scanlines for a frame have been processed, it fills the framebuffer
 * with the rendered image and sets frame_done to true.
 *
 * @param[in,out] p
 *     Pointer to the PPU instance.
 */
void ppu_clock(PPU *p);

#endif // PPU_H


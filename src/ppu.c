
/**
 * @file   ppu.c
 *
 * @brief  Implementation of Picture Processing Unit (PPU) logic.
 *         Manages framebuffer, scanline timing, and pixel rendering.
 *
 * @author E Warren (ethanwarren768@gmail.com)
 * @date   2025-10-04
 */

#include "ppu.h"
#include <string.h>

/**
 * Initialize PPU state.
 *
 * Clears framebuffer, resets cycle counters, and prepares scanline state.
 */
void ppu_init(PPU *p)
{
	memset(p->framebuffer, 0, sizeof(p->framebuffer));
	p->ppu_cycles = 0;
	p->scanline = 0;
	p->frame_done = false;

	// TODO: Initialize palette, nametables, OAM, and registers
}

void ppu_clock(PPU *p)
{
	p->ppu_cycles++;

	// NTSC: 341 dots per scanline × 262 scanlines ≈ 89342 PPU cycles/frame
	const uint64_t cycles_per_frame = 341 * 262;

	if (p->ppu_cycles >= cycles_per_frame) {
		// TODO: Replace with full PPU tile fetching and rendering
		uint32_t c = (uint32_t) (p->ppu_cycles & 0xFFFFFF);
		for (int y = 0; y < FRAME_BUFFER_HEIGHT; ++y) {
			for (int x = 0; x < FRAME_BUFFER_WIDTH; ++x) {
				// Simple moving pattern for placeholder rendering
				uint8_t r =
					(uint8_t) ((x + (p->ppu_cycles / 1000)) & 0xFF);
				uint8_t g =
					(uint8_t) ((y + (p->ppu_cycles / 2000)) & 0xFF);
				uint8_t b = (uint8_t) ((x ^ y) & 0xFF);
				p->framebuffer[y * FRAME_BUFFER_WIDTH + x] =
					(0xFFu << 24) | (r << 16) | (g << 8) | b;
			}
		}
		p->ppu_cycles = 0;
		p->frame_done = true;

		// TODO: Advance to next frame, update vblank status, trigger NMI
		// TODO: Implement background rendering
		// TODO: Implement sprite rendering with priority   
	} else {
		p->frame_done = false;
	}
}

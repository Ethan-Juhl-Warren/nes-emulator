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
#include "ines.h"
// Frame buffer dimensions in pixels
#define SCREEN_WIDTH 256
#define SCREEN_HEIGHT 240

// PPU register addresses
#define PPUCTRL   0x2000
#define PPUMASK   0x2001  
#define PPUSTATUS 0x2002
#define OAMADDR   0x2003
#define OAMDATA   0x2004
#define PPUSCROLL 0x2005
#define PPUADDR   0x2006
#define PPUDATA   0x2007


/**
 * Represents the NES Picture Processing Unit (PPU) state.
 */
typedef struct {
    // Existing fields
    uint32_t framebuffer[SCREEN_WIDTH * SCREEN_HEIGHT];
    uint8_t read_buffer;
	uint64_t ppu_cycles;
    int scanline;
	int cycle;
    bool frame_done;
    
    // New: PPU Memory
    uint8_t pal_ram[32];        // Palette RAM ($3F00-$3F1F)
    uint8_t oam[256];           // Sprite OAM ($2000-$2FFF)
    uint8_t vram[2048];         // Nametable RAM ($2000-$2FFF)
    
    // PPU Registers ($2000-$2007)
    uint8_t ppuctrl;           // $2000 - Control
    uint8_t ppumask;           // $2001 - Mask  
    uint8_t ppustatus;         // $2002 - Status
    uint8_t oamaddr;           // $2003 - OAM Address
    uint8_t oamdata;           // $2004 - OAM Data
    uint8_t ppuscroll;         // $2005 - Scroll
    uint8_t ppuaddr;           // $2006 - PPU Address
    uint8_t ppudata;           // $2007 - PPU Data
    
    // Internal state
    uint16_t vram_addr;        // Current VRAM address
    uint16_t temp_addr;        // Temporary VRAM address
    uint8_t fine_x;            // Fine X scroll
    bool write_toggle;         // For $2005/$2006 writes
    bool nmi_occurred;
    
    INES *cart;                // Pointer to cartridge (for CHR ROM)
} PPU;

extern PPU *global_ppu;

/**
 * Initializes the PPU state.
 *
 * This function clears the framebuffer, resets cycle and scanline counters,
 * and prepares the PPU for rendering frames.
 *
 * @param[in,out] p
 *     Pointer to the PPU instance to initialize.
 */
void ppu_init(PPU *p, INES *cart);

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

uint8_t ppu_reg_read(PPU *p, uint16_t addr);
void ppu_reg_write(PPU *p, uint16_t addr, uint8_t val);
void ppu_trigger_nmi(PPU *p);
#endif // PPU_H


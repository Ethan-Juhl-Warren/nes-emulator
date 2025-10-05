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
#include "cpu.h"
#include "ines.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>
static const uint32_t nes_palette[64] = {
	0x757575, 0x271B8F, 0x0000AB, 0x47009F, 0x8F0077, 0xAB0013, 0xA70000,
	0x7F0B00,
	0x432F00, 0x004700, 0x005100, 0x003F17, 0x1B3F5F, 0x000000, 0x000000,
	0x000000,
	0xBCBCBC, 0x0073EF, 0x233BEF, 0x8300F3, 0xBF00BF, 0xE7005B, 0xDB2B00,
	0xCB4F0F,
	0x8B7300, 0x009700, 0x00AB00, 0x00933B, 0x00838B, 0x000000, 0x000000,
	0x000000,
	0xFFFFFF, 0x3FBFFF, 0x5F73FF, 0xA78BFD, 0xF77BFF, 0xFF77B7, 0xFF7763,
	0xFF9B3B,
	0xF3BF3F, 0x83D313, 0x4FDF4B, 0x58F898, 0x00EBDB, 0x000000, 0x000000,
	0x000000,
	0xFFFFFF, 0xABE7FF, 0xC7D7FF, 0xD7CBFF, 0xFFC7FF, 0xFFC7DB, 0xFFBFB3,
	0xFFDBAB,
	0xFFE7A3, 0xE3FFA3, 0xABF3BF, 0xB3FFCF, 0x9FFFF3, 0x000000, 0x000000,
	0x000000
};
// Forward declarations of static functions
static void render_background_pixel(PPU * p);
static void render_sprite_pixel(PPU * p);
static void fetch_background_data(PPU * p);
static uint8_t ppu_vram_read_byte(PPU * p, uint16_t addr);
static void ppu_vram_write_byte(PPU * p, uint16_t addr, uint8_t val);
static uint8_t ppu_vram_read(PPU * p);
static void ppu_vram_write(PPU * p, uint8_t val);

/**
 * Initialize PPU state.
 */
void ppu_init(PPU *p, INES *cart)
{
	memset(p, 0, sizeof(PPU));
	p->cart = cart;
	p->ppu_cycles = 0;
	p->scanline = 261;			// Start at pre-render scanline
	p->cycle = 0;
	p->read_buffer = 0;
	p->frame_done = false;
	p->ppustatus = 0xA0;
	p->write_toggle = false;
}

void ppu_clock(PPU *p)
{
	// Pre-render scanline (261)
	if (p->scanline == 261) {
		if (p->cycle == 1) {
			// Clear VBlank, sprite 0 hit, and sprite overflow flags
			p->ppustatus &= 0x1F;
		} else if (p->cycle == 304) {
			// Copy temporary address to current address (vertical scroll)
			if (p->ppumask & 0x18) {
				p->vram_addr = p->temp_addr;
			}
		}
	}
	// Visible scanlines (0-239)
	if (p->scanline < 240) {
		// Background rendering
		if (p->ppumask & 0x08) {
			if (p->cycle >= 1 && p->cycle <= 256) {
				render_background_pixel(p);
			}
		}
		// Sprite rendering
		if (p->ppumask & 0x10) {
			if (p->cycle >= 1 && p->cycle <= 256) {
				render_sprite_pixel(p);
			}
		}
		// Fetch background data during visible cycles
		if (p->ppumask & 0x08) {
			if ((p->cycle >= 1 && p->cycle <= 256)
				|| (p->cycle >= 321 && p->cycle <= 336)) {
				fetch_background_data(p);
			}
		}
	}
	// VBlank scanlines (241-260)
	if (p->scanline == 241 && p->cycle == 1) {
		// Set VBlank flag
		p->ppustatus |= 0x80;
		// Trigger NMI if enabled
		if (p->ppuctrl & 0x80) {
			p->nmi_occurred = true;
		}
		static int vb_count = 0;
		if (vb_count < 5) {
			printf("VBlank SET: PPUSTATUS now = %02X, PPUCTRL = %02X\n",
				   p->ppustatus, p->ppuctrl);
			vb_count++;
		}
	}
	// Increment cycle counter
	p->cycle++;
	p->ppu_cycles++;

	// Handle end of scanline
	if (p->cycle >= 341) {
		p->cycle = 0;
		p->scanline++;

		// Handle end of frame
		if (p->scanline >= 262) {
			p->scanline = 0;
			p->frame_done = true;
		}
	}
}

// NES master 64-color palette (RGB values)
// You can tweak the RGBs later for gamma or palette accuracy

static void render_background_pixel(PPU *p)
{
	int x = p->cycle - 1;
	int y = p->scanline;
	if (x < 0 || x >= SCREEN_WIDTH || y < 0 || y >= SCREEN_HEIGHT)
		return;

	// Compute coarse tile coordinates
	int tile_x = x / 8;
	int tile_y = y / 8;

	// Base nametable address (we’ll just use $2000 for now)
	uint16_t nametable_addr = 0x2000;

	// Each nametable is 32×30 tiles
	uint16_t tile_index_addr = nametable_addr + tile_y * 32 + tile_x;
	uint8_t tile_index = ppu_vram_read_byte(p, tile_index_addr);

	// Attribute table byte for this 4×4 tile block
	uint16_t attr_addr =
		nametable_addr + 0x03C0 + (tile_y / 4) * 8 + (tile_x / 4);
	uint8_t attr_byte = ppu_vram_read_byte(p, attr_addr);

	// Select palette based on quadrant
	uint8_t shift = ((tile_y % 4) / 2) * 4 + ((tile_x % 4) / 2) * 2;
	uint8_t palette_bits = (attr_byte >> shift) & 0x03;

	// Fine Y inside the tile
	uint8_t fine_y = y % 8;

	// Pattern table base from PPUCTRL
	uint16_t pattern_base = (p->ppuctrl & 0x10) ? 0x1000 : 0x0000;
	uint16_t tile_pattern_addr = pattern_base + tile_index * 16 + fine_y;

	uint8_t plane0 = ppu_vram_read_byte(p, tile_pattern_addr);
	uint8_t plane1 = ppu_vram_read_byte(p, tile_pattern_addr + 8);

	// Fine X within tile
	uint8_t fine_x = x % 8;
	int bit = 7 - fine_x;
	uint8_t pixel_bit = ((plane1 >> bit) & 1) << 1 | ((plane0 >> bit) & 1);

	// Background color (index 0 = universal background)
	if (pixel_bit == 0) {
		uint8_t bg_index = ppu_vram_read_byte(p, 0x3F00);
		uint8_t nes_color = bg_index & 0x3F;
		p->framebuffer[y * SCREEN_WIDTH + x] =
			0xFF000000 | nes_palette[nes_color];
		return;
	}

	uint16_t palette_base = 0x3F00 + (palette_bits << 2);
	uint8_t palette_index = ppu_vram_read_byte(p, palette_base + pixel_bit);
	uint8_t nes_color = palette_index & 0x3F;
	p->framebuffer[y * SCREEN_WIDTH + x] = 0xFF000000 | nes_palette[nes_color];
}

// Sprite rendering placeholder
static void render_sprite_pixel(PPU *p)
{
	int x = p->cycle - 1;
	int y = p->scanline;

	if (x < 0 || x >= SCREEN_WIDTH || y < 0 || y >= SCREEN_HEIGHT)
		return;

	// Check if background pixel is transparent
	uint32_t bg_pixel = p->framebuffer[y * SCREEN_WIDTH + x];
	bool bg_transparent =
		(bg_pixel ==
		 (0xFF000000 | nes_palette[ppu_vram_read_byte(p, 0x3F00) & 0x3F]));

	// Sprite size: 8x8 or 8x16 based on PPUCTRL bit 5
	int sprite_height = (p->ppuctrl & 0x20) ? 16 : 8;

	// Pattern table for 8x8 sprites from PPUCTRL bit 3
	uint16_t sprite_pattern_base = (p->ppuctrl & 0x08) ? 0x1000 : 0x0000;

	// Scan through OAM to find sprites on this scanline
	for (int i = 0; i < 64; i++) {
		int oam_index = i * 4;
		uint8_t sprite_y = p->oam[oam_index];
		uint8_t tile_index = p->oam[oam_index + 1];
		uint8_t attributes = p->oam[oam_index + 2];
		uint8_t sprite_x = p->oam[oam_index + 3];

		// Check if sprite is on this scanline
		int sprite_row = y - (sprite_y + 1);
		if (sprite_row < 0 || sprite_row >= sprite_height)
			continue;

		// Check if sprite is at this x position
		int sprite_col = x - sprite_x;
		if (sprite_col < 0 || sprite_col >= 8)
			continue;

		// Extract attribute bits
		uint8_t palette_num = attributes & 0x03;
		bool priority = (attributes & 0x20);	// 0 = in front, 1 = behind background
		bool flip_h = (attributes & 0x40);
		bool flip_v = (attributes & 0x80);

		// Skip if sprite is behind background and background is opaque
		if (priority && !bg_transparent)
			continue;

		// Handle vertical flip
		int pattern_row = sprite_row;
		if (flip_v)
			pattern_row = sprite_height - 1 - sprite_row;

		// For 8x16 sprites, tile_index determines pattern table
		uint16_t pattern_addr;
		if (sprite_height == 16) {
			uint16_t pattern_table = (tile_index & 0x01) ? 0x1000 : 0x0000;
			uint8_t tile_num = tile_index & 0xFE;
			if (pattern_row >= 8) {
				tile_num += 1;
				pattern_row -= 8;
			}
			pattern_addr = pattern_table + tile_num * 16 + pattern_row;
		} else {
			pattern_addr = sprite_pattern_base + tile_index * 16 + pattern_row;
		}

		// Read pattern data
		uint8_t plane0 = ppu_vram_read_byte(p, pattern_addr);
		uint8_t plane1 = ppu_vram_read_byte(p, pattern_addr + 8);

		// Handle horizontal flip
		int bit = flip_h ? sprite_col : (7 - sprite_col);
		uint8_t pixel_value =
			((plane1 >> bit) & 1) << 1 | ((plane0 >> bit) & 1);

		// Skip transparent pixels
		if (pixel_value == 0)
			continue;

		// Get color from sprite palette
		uint16_t palette_addr = 0x3F10 + (palette_num * 4) + pixel_value;
		uint8_t palette_index = ppu_vram_read_byte(p, palette_addr);
		uint8_t nes_color = palette_index & 0x3F;

		// Render sprite pixel
		p->framebuffer[y * SCREEN_WIDTH + x] =
			0xFF000000 | nes_palette[nes_color];

		// Sprite 0 hit detection
		if (i == 0 && !bg_transparent && x != 255) {
			p->ppustatus |= 0x40;	// Set sprite 0 hit flag
		}
		// Only render the first non-transparent sprite pixel (priority)
		break;
	}
}
// Background data fetching placeholder
static void fetch_background_data(PPU *p)
{
	// Increment horizontal scroll
	if ((p->cycle & 7) == 0) {
		if ((p->vram_addr & 0x001F) == 31) {
			p->vram_addr &= ~0x001F;
			p->vram_addr ^= 0x0400;
		} else {
			p->vram_addr += 1;
		}
	}
	// Increment vertical scroll at end of scanline
	if (p->cycle == 256) {
		if ((p->vram_addr & 0x7000) != 0x7000) {
			p->vram_addr += 0x1000;
		} else {
			p->vram_addr &= ~0x7000;
			uint16_t y = (p->vram_addr & 0x03E0) >> 5;
			if (y == 29) {
				y = 0;
				p->vram_addr ^= 0x0800;
			} else if (y == 31) {
				y = 0;
			} else {
				y += 1;
			}
			p->vram_addr = (p->vram_addr & ~0x03E0) | (y << 5);
		}
	}
	// Reset horizontal position at cycle 257
	if (p->cycle == 257) {
		if (p->ppumask & 0x08) {
			p->vram_addr = (p->vram_addr & ~0x041F) | (p->temp_addr & 0x041F);
		}
	}
}

// Read from PPU memory (VRAM/Palette)
static uint8_t ppu_vram_read(PPU *p)
{
	uint16_t addr = p->vram_addr;
	uint8_t data;

	// Palette reads are not buffered
	if (addr >= 0x3F00) {
		data = ppu_vram_read_byte(p, addr);
		p->read_buffer = ppu_vram_read_byte(p, addr & 0x2FFF);	// Fill buffer with mirrored nametable
	} else {
		data = p->read_buffer;
		p->read_buffer = ppu_vram_read_byte(p, addr);
	}

	if (p->ppuctrl & 0x04) {
		p->vram_addr += 32;
	} else {
		p->vram_addr += 1;
	}

	return data;
}

static uint8_t ppu_vram_read_byte(PPU *p, uint16_t addr)
{
	addr &= 0x3FFF;

	if (addr < 0x2000) {
		return p->cart->chr_rom[addr % p->cart->chr_size];
	} else if (addr < 0x3F00) {
		uint16_t mirrored_addr = addr & 0x0FFF;
		if (p->cart->mirror == 0) {
			if (mirrored_addr >= 0x0400 && mirrored_addr < 0x0800)
				mirrored_addr -= 0x0400;
			if (mirrored_addr >= 0x0C00)
				mirrored_addr -= 0x0800;
		} else {
			if (mirrored_addr >= 0x0800)
				mirrored_addr -= 0x0800;
		}
		return p->vram[mirrored_addr & 0x07FF];
	} else if (addr < 0x4000) {
		addr &= 0x001F;
		if (addr == 0x0010)
			addr = 0x0000;
		if (addr == 0x0014)
			addr = 0x0004;
		if (addr == 0x0018)
			addr = 0x0008;
		if (addr == 0x001C)
			addr = 0x000C;
		return p->pal_ram[addr];
	}

	return 0;
}

// Write to PPU memory (VRAM/Palette)
static void ppu_vram_write_byte(PPU *p, uint16_t addr, uint8_t val)
{
	addr &= 0x3FFF;

	if (addr < 0x2000) {
		return;
	} else if (addr < 0x3F00) {
		uint16_t mirrored_addr = addr & 0x0FFF;
		if (p->cart->mirror == 0) {
			if (mirrored_addr >= 0x0400 && mirrored_addr < 0x0800)
				mirrored_addr -= 0x0400;
			if (mirrored_addr >= 0x0C00)
				mirrored_addr -= 0x0800;
		} else {
			if (mirrored_addr >= 0x0800)
				mirrored_addr -= 0x0800;
		}
		p->vram[mirrored_addr & 0x07FF] = val;
	} else if (addr < 0x4000) {
		addr &= 0x001F;
		if (addr == 0x0010)
			addr = 0x0000;
		if (addr == 0x0014)
			addr = 0x0004;
		if (addr == 0x0018)
			addr = 0x0008;
		if (addr == 0x001C)
			addr = 0x000C;
		p->pal_ram[addr] = val;
	}
}

static void ppu_vram_write(PPU *p, uint8_t val)
{
	ppu_vram_write_byte(p, p->vram_addr, val);

	if (p->ppuctrl & 0x04) {
		p->vram_addr += 32;
	} else {
		p->vram_addr += 1;
	}
}

uint8_t ppu_reg_read(PPU *p, uint16_t addr)
{
	uint8_t reg = addr & 7;
	uint8_t result;
	switch (reg) {
	case 0:
		return 0;
	case 1:
		return 0;
	case 2:
		result = p->ppustatus;
		p->write_toggle = false;
		static int debug_reads = 0;
		if (debug_reads < 20) {
			printf
				("READ PPUSTATUS: scanline=%d result=%02X (VBlank=%d) - will%s clear\n",
				 p->scanline, result, (result & 0x80) ? 1 : 0,
				 (p->scanline < 241 || p->scanline > 260) ? "" : " NOT");
			debug_reads++;
		}
		// ONLY clear VBlank if we're NOT in VBlank period
		// VBlank should only be cleared at pre-render scanline
		if (p->scanline < 241 || p->scanline > 260) {
			p->ppustatus &= ~0x80;
			p->nmi_occurred = false;
		}

		return result;
	case 3:
		return 0;
	case 4:
		return p->oam[p->oamaddr];
	case 5:
		return 0;
	case 6:
		return 0;
	case 7:
		result = ppu_vram_read(p);
		return result;
	default:
		return 0;
	}
}

void ppu_reg_write(PPU *p, uint16_t addr, uint8_t val)
{
	uint8_t reg = addr & 7;
	switch (reg) {
	case 0:
		p->ppuctrl = val;
		p->temp_addr = (p->temp_addr & 0xF3FF) | ((val & 0x03) << 10);
		break;
	case 1:
		p->ppumask = val;
		break;
	case 2:
		break;
	case 3:
		p->oamaddr = val;
		break;
	case 4:
		p->oam[p->oamaddr++] = val;
		break;
	case 5:
		if (!p->write_toggle) {
			p->temp_addr = (p->temp_addr & 0xFFE0) | (val >> 3);
			p->fine_x = val & 0x07;
		} else {
			p->temp_addr = (p->temp_addr & 0x8FFF) | ((val & 0x07) << 12);
			p->temp_addr = (p->temp_addr & 0xFC1F) | ((val & 0xF8) << 2);
		}
		p->write_toggle = !p->write_toggle;
		break;
	case 6:
		if (!p->write_toggle) {
			p->temp_addr = (p->temp_addr & 0x80FF) | ((val & 0x3F) << 8);
		} else {
			p->temp_addr = (p->temp_addr & 0xFF00) | val;
			p->vram_addr = p->temp_addr;
		}
		p->write_toggle = !p->write_toggle;
		break;
	case 7:
		ppu_vram_write(p, val);
		break;
	}
}
void ppu_trigger_nmi(PPU *p)
{
	if (p->nmi_occurred) {
		p->nmi_occurred = false;
		cpu_interrupt(global_cpu, (1 << 0));
	}
}

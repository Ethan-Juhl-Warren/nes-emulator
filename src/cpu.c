/**
 * @file   cpu.c
 *
 * @brief  Implementation of the 6502 CPU core.
 *         Handles instruction decoding, register state,
 *         flags, cycle counting, and memory access.
 *
 * @author E Warren (ethanwarren768@gmail.com)
 * @date   2025-07-29
 */

#include "cpu.h"
#include <string.h>
#include <stdio.h>

// Status flag bit definitions
enum {
	FLAG_C = 1 << 0,			///< Carry
	FLAG_Z = 1 << 1,			///< Zero
	FLAG_I = 1 << 2,			///< Interrupt disable
	FLAG_D = 1 << 3,			///< Decimal mode
	FLAG_B = 1 << 4,			///< Break
	FLAG_U = 1 << 5,			///< Unused
	FLAG_V = 1 << 6,			///< Overflow
	FLAG_N = 1 << 7				///< Negative
};

// Helper: set or clear a CPU status flag
static inline void set_flag(CPU *c, uint8_t f, int v)
{
	if (v)
		c->P |= f;
	else
		c->P &= ~f;
}

// Helper: read the value of a CPU status flag
static inline int get_flag(CPU *c, uint8_t f)
{
	return (c->P & f) ? 1 : 0;
}

/**
 * Read a byte from the CPU address space.
 *
 * Handles internal RAM, PRG ROM, and memory-mapped registers.
 */
uint8_t cpu_read(CPU *c, uint16_t addr)
{
	if (addr < 0x0800)
		return c->ram[addr & 0x07FF];

	if (addr >= 0x8000) {
		size_t prg_size = c->cart->prg_size;
		uint32_t offset = addr - 0x8000;
		if (prg_size == 16384) {
			offset &= 0x3FFF;	// mirror 16KB banks
		} else {
			offset &= (uint32_t) (prg_size - 1);
		}
		return c->cart->prg_rom[offset];
	}
	// TODO: Implement memory-mapped I/O reads (PPU/APU/controllers)
	return 0;
}

/**
 * Write a byte to the CPU address space.
 *
 * Handles internal RAM and memory-mapped registers.
 * PRG ROM is read-only.
 */
void cpu_write(CPU *c, uint16_t addr, uint8_t val)
{
	if (addr < 0x0800) {
		c->ram[addr & 0x07FF] = val;
		return;
	}
	// TODO: Implement writes to PPU/APU registers and cartridge RAM
}

/**
 * Reset CPU state to power-on defaults.
 *
 * Initializes registers and stack pointer,
 * clears RAM, and sets PC from reset vector.
 */
void cpu_reset(CPU *c)
{
	memset(c->ram, 0, sizeof(c->ram));
	c->A = c->X = c->Y = 0;
	c->SP = 0xFD;
	c->P = FLAG_U | FLAG_I;

	uint8_t lo = cpu_read(c, 0xFFFC);
	uint8_t hi = cpu_read(c, 0xFFFD);
	c->PC = (hi << 8) | lo;
	c->cycles = 0;

	// TODO: Implement proper reset behavior according to NES spec
}

/**
 * Fetch a single byte from memory and increment PC.
 */
static inline uint8_t fetch_byte(CPU *c)
{
	return cpu_read(c, c->PC++);
}

/**
 * Fetch a 16-bit little-endian word and increment PC by 2.
 */
static inline uint16_t fetch_word(CPU *c)
{
	uint8_t lo = fetch_byte(c);
	uint8_t hi = fetch_byte(c);
	return (uint16_t) lo | ((uint16_t) hi << 8);
}

/**
 * Execute one CPU instruction cycle.
 *
 * Fetches opcode, decodes, executes, and returns cycle count.
 */
int cpu_step(CPU *c)
{
	uint8_t opcode = fetch_byte(c);
	int cycles = 0;

	switch (opcode) {
	case 0xEA:					// NOP
		cycles = 2;
		break;
	case 0xA9:{				// LDA immediate
			uint8_t v = fetch_byte(c);
			c->A = v;
			set_flag(c, FLAG_Z, c->A == 0);
			set_flag(c, FLAG_N, c->A & 0x80);
			cycles = 2;
			break;
		}
	case 0xAA:					// TAX
		c->X = c->A;
		set_flag(c, FLAG_Z, c->X == 0);
		set_flag(c, FLAG_N, c->X & 0x80);
		cycles = 2;
		break;
	case 0xE8:					// INX
		c->X++;
		set_flag(c, FLAG_Z, c->X == 0);
		set_flag(c, FLAG_N, c->X & 0x80);
		cycles = 2;
		break;
	case 0x8D:{				// STA absolute
			uint16_t addr = fetch_word(c);
			cpu_write(c, addr, c->A);
			cycles = 4;
			break;
		}
	case 0x4C:{				// JMP absolute
			uint16_t addr = fetch_word(c);
			c->PC = addr;
			cycles = 3;
			break;
		}
	default:
		fprintf(stderr, "Unimplemented opcode %02X at PC=%04X\n",
				opcode, (uint16_t) (c->PC - 1));
		cycles = 2;
		break;
	}

	// TODO: Implement interrupts (IRQ/NMI) handling
	// TODO: Implement stack instructions (PHA, PLA, PHP, PLP, JSR, RTS, RTI)
	// TODO: Implement decimal mode (BCD)
	// TODO: Optimize opcode execution (lookup tables)

	c->cycles += cycles;
	return cycles;
}

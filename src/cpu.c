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
#include "logging.h"
#include "opcodes.h"
#include "itable.h"
#include "cntrler.h"
#include "ppu.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#define INTERRUPT_NMI (1 << 0)
#define INTERRUPT_IRQ (1 << 1)
#define DELAYED_INTERRUPT_DISABLE (1 << 2)

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

static Controller *controller1;
static Controller *controller2;
static int interupt_disable = 0;
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

static inline uint8_t fetch_byte(CPU *c)
{
	return cpu_read(c, c->PC++);
}

static inline uint16_t fetch_word(CPU *c)
{
	uint8_t lo = fetch_byte(c);
	uint8_t hi = fetch_byte(c);
	return (uint16_t) lo | ((uint16_t) hi << 8);
}

static inline void push_byte(CPU *c, uint8_t value)
{
	cpu_write(c, 0x100 + c->SP, value);
	c->SP--;
}

static inline uint8_t pop_byte(CPU *c)
{
	c->SP++;
	return cpu_read(c, 0x100 + c->SP);
}

static inline void push_word(CPU *c, uint16_t value)
{
	push_byte(c, (value >> 8) & 0xFF);
	push_byte(c, value & 0xFF);
}

static inline uint16_t pop_word(CPU *c)
{
	uint8_t lo = pop_byte(c);
	uint8_t hi = pop_byte(c);
	return (hi << 8) | lo;
}

static inline int handle_branch(CPU *c, int condition)
{
	int8_t offset = (int8_t) fetch_byte(c);	// Signed relative offset

	if (condition) {
		uint16_t old_pc = c->PC;
		c->PC += offset;		// Apply the branch offset

		// Check for page crossing (extra cycle if branch crosses page)
		int page_crossed = (old_pc & 0xFF00) != (c->PC & 0xFF00);
		return 3 + page_crossed;	// 3 or 4 cycles
	}
	return 2;					// 2 cycles if branch not taken
}

void cpu_interrupt(CPU *c, int type)
{
	c->interupt_flags |= type;
}

static void handle_interupt(CPU *c)
{
	uint16_t vector;
	uint8_t pushed_flags = c->P;

	if (c->interupt_flags & INTERRUPT_NMI) {
		vector = cpu_read(c, 0xFFFA) | (cpu_read(c, 0xFFFB) << 8);
		c->interupt_flags &= ~INTERRUPT_NMI;
	} else if (c->interupt_flags & INTERRUPT_IRQ) {
		vector = cpu_read(c, 0xFFFE) | (cpu_read(c, 0xFFFF) << 8);
		c->interupt_flags &= ~INTERRUPT_IRQ;
	} else {
		return;
	}

	push_word(c, c->PC);
	push_byte(c, pushed_flags | FLAG_U);

	set_flag(c, FLAG_I, 1);

	c->PC = vector;
}

uint8_t cpu_read(CPU *c, uint16_t addr)
{
	if (addr >= 0x2000 && addr < 0x4000) {
		if (global_ppu) {
			return ppu_reg_read(global_ppu, addr);
		}
	}

	if (addr == 0x4016) {		// Controller 1
		return controller_read(controller1) | 0x40;
	}

	if (addr == 0x4017) {		// Controller 2
		return controller_read(controller2) | 0x40;
	}

	if (addr >= 0x4000 && addr <= 0x4017) {
		// Return 0 for most APU registers
		if (addr == 0x4015) {	// APU status
			return 0x00;		// No channels active
		}
		return 0;
	}

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

void cpu_write(CPU *c, uint16_t addr, uint8_t val)
{
	if (addr >= 0x2000 && addr < 0x4000) {
		if (global_ppu) {
			ppu_reg_write(global_ppu, addr, val);
		}
		return;
	}

	if (addr == 0x4016) {
		controller_write_strobe(controller1, val);
		controller_write_strobe(controller2, val);
		return;
	}

	if (addr < 0x0800) {
		c->ram[addr & 0x07FF] = val;
		return;
	}
	// TODO: Implement writes to PPU/APU registers and cartridge RAM
}

void set_controllers(Controller *c1, Controller *c2)
{
	controller1 = c1;
	controller2 = c2;
}

void cpu_reset(CPU *c)
{
	memset(c->ram, 0, sizeof(c->ram));
	c->A = c->X = c->Y = 0;
	c->SP = 0xFD;
	c->P = FLAG_U | FLAG_I;

	memset(controller1, 0, sizeof(Controller));
	memset(controller2, 0, sizeof(Controller));

	uint8_t lo = cpu_read(c, 0xFFFC);
	uint8_t hi = cpu_read(c, 0xFFFD);
	c->PC = (hi << 8) | lo;
	c->cycles = 0;

	// TODO: Implement proper reset behavior according to NES spec
}

int cpu_step(CPU *c)
{
	if ((c->interupt_flags & DELAYED_INTERRUPT_DISABLE)) {
		set_flag(c, FLAG_I, interupt_disable);
		c->interupt_flags &= ~DELAYED_INTERRUPT_DISABLE;
	}

	if (c->interupt_flags & (INTERRUPT_NMI | INTERRUPT_IRQ)) {
		handle_interupt(c);
		return 7;
	}

	uint8_t opcode = fetch_byte(c);
	int cycles = 0;

	InstructionHandler handler = i_table[opcode];
	if (!handler) {
		abort_e("Invalid opcode %02x", opcode);
		return 0;
	}
	cycles = handler(c);

	c->cycles += cycles;
	return cycles;

	// TODO: Implement interrupts (IRQ/NMI) handling
}

static inline void SET_FLAGS(CPU *cpu, uint8_t reg)
{
	set_flag(cpu, FLAG_Z, reg == 0);
	set_flag(cpu, FLAG_N, reg & 0x80);
}

/*****************************OPCODE_HANDLERS*********************************/
/**
 * LDA immediate
 *
 */
int handle_LDA_IMM(CPU *c)
{
	uint8_t v = fetch_byte(c);
	c->A = v;
	SET_FLAGS(c, c->A);
	return 2;
}
int handle_LDA_ZP(CPU *c)
{
	uint8_t addr = fetch_byte(c);
	c->A = cpu_read(c, addr);
	SET_FLAGS(c, c->A);
	return 3;
}
int handle_LDA_ZPX(CPU *c)
{
	uint8_t addr = fetch_byte(c);
	addr = (addr + c->X) & 0xff;
	c->A = cpu_read(c, addr);
	SET_FLAGS(c, c->A);
	return 4;
}
int handle_LDA_ABS(CPU *c)
{
	uint16_t addr = fetch_word(c);
	uint8_t v = cpu_read(c, addr);
	c->A = v;
	SET_FLAGS(c, c->A);
	return 4;
}
int handle_LDA_ABSX(CPU *c)
{
	uint16_t base_addr = fetch_word(c);
	uint16_t addr = base_addr + c->X;
	int page_crossed = (base_addr & 0xff00) != (addr & 0xff00);
	uint8_t v = cpu_read(c, addr);
	c->A = v;
	SET_FLAGS(c, c->A);
	return 4 + page_crossed;
}
int handle_LDA_ABSY(CPU *c)
{
	uint16_t base_addr = fetch_word(c);
	uint16_t addr = base_addr + c->Y;
	int page_crossed = (base_addr & 0xff00) != (addr & 0xff00);
	uint8_t v = cpu_read(c, addr);
	c->A = v;
	SET_FLAGS(c, c->A);
	return 4 + page_crossed;
}
int handle_LDA_INDX(CPU *c)
{
	uint8_t zp_addr = fetch_byte(c);
	uint8_t pointer_addr = (zp_addr + c->X) & 0xFF;

	uint16_t addr = cpu_read(c, pointer_addr);
	addr |= cpu_read(c, (pointer_addr + 1) & 0xFF) << 8;

	c->A = cpu_read(c, addr);
	SET_FLAGS(c, c->A);
	return 6;
}
int handle_LDA_INDY(CPU *c)
{
	uint8_t zp_addr = fetch_byte(c);

	uint16_t base_addr = cpu_read(c, zp_addr);
	base_addr |= cpu_read(c, (zp_addr + 1) & 0xFF) << 8;

	uint16_t addr = base_addr + c->Y;

	int page_crossed = (base_addr & 0xFF00) != (addr & 0xFF00);

	c->A = cpu_read(c, addr);
	SET_FLAGS(c, c->A);
	return 5 + page_crossed;
}
int handle_LDX_IMM(CPU *c)
{
	uint8_t v = fetch_byte(c);
	c->X = v;
	SET_FLAGS(c, c->X);
	return 2;
}
int handle_LDX_ZP(CPU *c)
{
	uint8_t addr = fetch_byte(c);
	c->X = cpu_read(c, addr);
	SET_FLAGS(c, c->X);
	return 3;
}
int handle_LDX_ZPY(CPU *c)
{
	uint8_t addr = fetch_byte(c);
	addr = (addr + c->Y) & 0xff;
	c->X = cpu_read(c, addr);
	SET_FLAGS(c, c->X);
	return 4;
}
int handle_LDX_ABS(CPU *c)
{
	uint16_t addr = fetch_word(c);
	uint8_t v = cpu_read(c, addr);
	c->X = v;
	SET_FLAGS(c, c->X);
	return 4;
}
int handle_LDX_ABSY(CPU *c)
{
	uint16_t base_addr = fetch_word(c);
	uint16_t addr = base_addr + c->Y;
	int page_crossed = (base_addr & 0xff00) != (addr & 0xff00);
	uint8_t v = cpu_read(c, addr);
	c->X = v;
	SET_FLAGS(c, c->X);
	return 4 + page_crossed;
}
int handle_LDY_IMM(CPU *c)
{
	uint8_t v = fetch_byte(c);
	c->Y = v;
	SET_FLAGS(c, c->Y);
	return 2;
}
int handle_LDY_ZP(CPU *c)
{
	uint8_t addr = fetch_byte(c);
	c->Y = cpu_read(c, addr);
	SET_FLAGS(c, c->Y);
	return 3;
}
int handle_LDY_ZPX(CPU *c)
{
	uint8_t addr = fetch_byte(c);
	addr = (addr + c->X) & 0xff;
	c->Y = cpu_read(c, addr);
	SET_FLAGS(c, c->Y);
	return 4;
}
int handle_LDY_ABS(CPU *c)
{
	uint16_t addr = fetch_word(c);
	uint8_t v = cpu_read(c, addr);
	c->Y = v;
	SET_FLAGS(c, c->Y);
	return 4;
}
int handle_LDY_ABSX(CPU *c)
{
	uint16_t base_addr = fetch_word(c);
	uint16_t addr = base_addr + c->X;
	int page_crossed = (base_addr & 0xff00) != (addr & 0xff00);
	uint8_t v = cpu_read(c, addr);
	c->Y = v;
	SET_FLAGS(c, c->Y);
	return 4 + page_crossed;
}
int handle_STA_ZP(CPU *c)
{
	uint8_t addr = fetch_byte(c);
	cpu_write(c, addr, c->A);
	return 3;
}
int handle_STA_ZPX(CPU *c)
{
	uint8_t addr = fetch_byte(c);
	addr = (addr + c->X) & 0xff;
	cpu_write(c, addr, c->A);
	return 4;
}
int handle_STA_ABS(CPU *c)
{
	uint16_t addr = fetch_word(c);
	cpu_write(c, addr, c->A);
	return 4;
}
int handle_STA_ABSX(CPU *c)
{
	uint16_t base_addr = fetch_word(c);
	uint16_t addr = base_addr + c->X;
	cpu_write(c, addr, c->A);
	return 5;
}
int handle_STA_ABSY(CPU *c)
{
	uint16_t base_addr = fetch_word(c);
	uint16_t addr = base_addr + c->Y;
	cpu_write(c, addr, c->A);
	return 5;
}
int handle_STA_INDX(CPU *c)
{
	uint8_t zp_addr = fetch_byte(c);
	uint8_t pointer_addr = (zp_addr + c->X) & 0xFF;

	uint16_t addr = cpu_read(c, pointer_addr);
	addr |= cpu_read(c, (pointer_addr + 1) & 0xFF) << 8;

	cpu_write(c, addr, c->A);
	return 6;
}
int handle_STA_INDY(CPU *c)
{
	uint8_t zp_addr = fetch_byte(c);

	uint16_t base_addr = cpu_read(c, zp_addr);
	base_addr |= cpu_read(c, (zp_addr + 1) & 0xFF) << 8;

	uint16_t addr = base_addr + c->Y;

	cpu_write(c, addr, c->A);
	return 6;
}
int handle_STX_ZP(CPU *c)
{
	uint8_t addr = fetch_byte(c);
	cpu_write(c, addr, c->X);
	return 3;
}
int handle_STX_ZPY(CPU *c)
{
	uint8_t addr = fetch_byte(c);
	addr = (addr + c->Y) & 0xff;
	cpu_write(c, addr, c->X);
	return 4;
}
int handle_STX_ABS(CPU *c)
{
	uint16_t addr = fetch_word(c);
	cpu_write(c, addr, c->X);
	return 4;
}
int handle_STY_ZP(CPU *c)
{
	uint8_t addr = fetch_byte(c);
	cpu_write(c, addr, c->Y);
	return 3;
}
int handle_STY_ZPX(CPU *c)
{
	uint8_t addr = fetch_byte(c);
	addr = (addr + c->X) & 0xff;
	cpu_write(c, addr, c->Y);
	return 4;
}
int handle_STY_ABS(CPU *c)
{
	uint16_t addr = fetch_word(c);
	cpu_write(c, addr, c->Y);
	return 4;
}
int handle_TAX(CPU *c)
{
	c->X = c->A;
	SET_FLAGS(c, c->X);
	return 2;
}
int handle_TAY(CPU *c)
{
	c->Y = c->A;
	SET_FLAGS(c, c->Y);
	return 2;
}
int handle_TXA(CPU *c)
{
	c->A = c->X;
	SET_FLAGS(c, c->A);
	return 2;
}
int handle_TYA(CPU *c)
{
	c->A = c->Y;
	SET_FLAGS(c, c->A);
	return 2;
}
int handle_TSX(CPU *c)
{
	c->X = c->SP;
	SET_FLAGS(c, c->X);
	return 2;
}
int handle_TXS(CPU *c)
{
	c->SP = c->X;
	return 2;
}
int handle_PHA(CPU *c)
{
	push_byte(c, c->A);
	return 3;
}
int handle_PHP(CPU *c)
{
	push_byte(c, c->P | FLAG_B | FLAG_U);
	return 3;
}
int handle_PLA(CPU *c)
{
	c->A = pop_byte(c);
	SET_FLAGS(c, c->A);
	return 4;
}
int handle_PLP(CPU *c)
{
	uint8_t flags = pop_byte(c);

	uint8_t mask = FLAG_N | FLAG_V | FLAG_D | FLAG_Z | FLAG_C;
	c->P = (c->P & ~mask) | (flags & mask);

	uint8_t new_i_flag = flags & FLAG_I;
	if ((c->P & FLAG_I) != new_i_flag) {
		interupt_disable = (new_i_flag != 0);
		c->interupt_flags |= DELAYED_INTERRUPT_DISABLE;
	}
	return 4;
}
int handle_AND_IMM(CPU *c)
{
	uint8_t v = fetch_byte(c);
	c->A &= v;
	SET_FLAGS(c, c->A);
	return 2;
}
int handle_AND_ZP(CPU *c)
{
	uint8_t addr = fetch_byte(c);
	uint8_t v = cpu_read(c, addr);
	c->A &= v;
	SET_FLAGS(c, c->A);
	return 3;
}
int handle_AND_ZPX(CPU *c)
{
	uint8_t base_addr = fetch_byte(c);
	uint8_t addr = (base_addr + c->X) & 0xff;
	uint8_t v = cpu_read(c, addr);
	c->A &= v;
	SET_FLAGS(c, c->A);
	return 4;
}
int handle_AND_ABS(CPU *c)
{
	uint16_t addr = fetch_word(c);
	uint8_t v = cpu_read(c, addr);
	c->A &= v;
	SET_FLAGS(c, c->A);
	return 4;
}
int handle_AND_ABSX(CPU *c)
{
	uint16_t base_addr = fetch_word(c);
	uint16_t addr = base_addr + c->X;
	int page_crossed = (base_addr & 0xFF00) != (addr & 0xFF00);

	uint8_t v = cpu_read(c, addr);
	c->A &= v;
	SET_FLAGS(c, c->A);
	return 4 + page_crossed;
}
int handle_AND_ABSY(CPU *c)
{
	uint16_t base_addr = fetch_word(c);
	uint16_t addr = base_addr + c->Y;
	int page_crossed = (base_addr & 0xFF00) != (addr & 0xFF00);

	uint8_t v = cpu_read(c, addr);
	c->A &= v;
	SET_FLAGS(c, c->A);
	return 4 + page_crossed;
}
int handle_AND_INDX(CPU *c)
{
	uint8_t zp_addr = fetch_byte(c);
	uint8_t pointer_addr = (zp_addr + c->X) & 0xFF;

	uint16_t addr = cpu_read(c, pointer_addr);
	addr |= cpu_read(c, (pointer_addr + 1) & 0xFF) << 8;

	uint8_t v = cpu_read(c, addr);
	c->A &= v;
	SET_FLAGS(c, c->A);
	return 6;
}
int handle_AND_INDY(CPU *c)
{
	uint8_t zp_addr = fetch_byte(c);

	uint16_t base_addr = cpu_read(c, zp_addr);
	base_addr |= cpu_read(c, (zp_addr + 1) & 0xFF) << 8;

	uint16_t addr = base_addr + c->Y;
	int page_crossed = (base_addr & 0xFF00) != (addr & 0xFF00);

	uint8_t v = cpu_read(c, addr);
	c->A &= v;
	SET_FLAGS(c, c->A);
	return 5 + page_crossed;
}
int handle_ORA_IMM(CPU *c)
{
	uint8_t v = fetch_byte(c);
	c->A |= v;
	SET_FLAGS(c, c->A);
	return 2;
}
int handle_ORA_ZP(CPU *c)
{
	uint8_t addr = fetch_byte(c);
	uint8_t v = cpu_read(c, addr);
	c->A |= v;
	SET_FLAGS(c, c->A);
	return 3;
}
int handle_ORA_ZPX(CPU *c)
{
	uint8_t base_addr = fetch_byte(c);
	uint8_t addr = (base_addr + c->X) & 0xff;
	uint8_t v = cpu_read(c, addr);
	c->A |= v;
	SET_FLAGS(c, c->A);
	return 4;
}
int handle_ORA_ABS(CPU *c)
{
	uint16_t addr = fetch_word(c);
	uint8_t v = cpu_read(c, addr);
	c->A |= v;
	SET_FLAGS(c, c->A);
	return 4;
}
int handle_ORA_ABSX(CPU *c)
{
	uint16_t base_addr = fetch_word(c);
	uint16_t addr = base_addr + c->X;
	int page_crossed = (base_addr & 0xFF00) != (addr & 0xFF00);

	uint8_t v = cpu_read(c, addr);
	c->A |= v;
	SET_FLAGS(c, c->A);
	return 4 + page_crossed;
}
int handle_ORA_ABSY(CPU *c)
{
	uint16_t base_addr = fetch_word(c);
	uint16_t addr = base_addr + c->Y;
	int page_crossed = (base_addr & 0xFF00) != (addr & 0xFF00);

	uint8_t v = cpu_read(c, addr);
	c->A |= v;
	SET_FLAGS(c, c->A);
	return 4 + page_crossed;
}
int handle_ORA_INDX(CPU *c)
{
	uint8_t zp_addr = fetch_byte(c);
	uint8_t pointer_addr = (zp_addr + c->X) & 0xFF;

	uint16_t addr = cpu_read(c, pointer_addr);
	addr |= cpu_read(c, (pointer_addr + 1) & 0xFF) << 8;

	uint8_t v = cpu_read(c, addr);
	c->A |= v;
	SET_FLAGS(c, c->A);
	return 6;
}
int handle_ORA_INDY(CPU *c)
{
	uint8_t zp_addr = fetch_byte(c);

	uint16_t base_addr = cpu_read(c, zp_addr);
	base_addr |= cpu_read(c, (zp_addr + 1) & 0xFF) << 8;

	uint16_t addr = base_addr + c->Y;
	int page_crossed = (base_addr & 0xFF00) != (addr & 0xFF00);

	uint8_t v = cpu_read(c, addr);
	c->A |= v;
	SET_FLAGS(c, c->A);
	return 5 + page_crossed;
}
int handle_EOR_IMM(CPU *c)
{
	uint8_t v = fetch_byte(c);
	c->A ^= v;
	SET_FLAGS(c, c->A);
	return 2;
}
int handle_EOR_ZP(CPU *c)
{
	uint8_t addr = fetch_byte(c);
	uint8_t v = cpu_read(c, addr);
	c->A ^= v;
	SET_FLAGS(c, c->A);
	return 3;
}
int handle_EOR_ZPX(CPU *c)
{
	uint8_t base_addr = fetch_byte(c);
	uint8_t addr = (base_addr + c->X) & 0xff;
	uint8_t v = cpu_read(c, addr);
	c->A ^= v;
	SET_FLAGS(c, c->A);
	return 4;
}
int handle_EOR_ABS(CPU *c)
{
	uint16_t addr = fetch_word(c);
	uint8_t v = cpu_read(c, addr);
	c->A ^= v;
	SET_FLAGS(c, c->A);
	return 4;
}
int handle_EOR_ABSX(CPU *c)
{
	uint16_t base_addr = fetch_word(c);
	uint16_t addr = base_addr + c->X;
	int page_crossed = (base_addr & 0xFF00) != (addr & 0xFF00);

	uint8_t v = cpu_read(c, addr);
	c->A ^= v;
	SET_FLAGS(c, c->A);
	return 4 + page_crossed;
}
int handle_EOR_ABSY(CPU *c)
{
	uint16_t base_addr = fetch_word(c);
	uint16_t addr = base_addr + c->Y;
	int page_crossed = (base_addr & 0xFF00) != (addr & 0xFF00);

	uint8_t v = cpu_read(c, addr);
	c->A ^= v;
	SET_FLAGS(c, c->A);
	return 4 + page_crossed;
}
int handle_EOR_INDX(CPU *c)
{
	uint8_t zp_addr = fetch_byte(c);
	uint8_t pointer_addr = (zp_addr + c->X) & 0xFF;

	uint16_t addr = cpu_read(c, pointer_addr);
	addr |= cpu_read(c, (pointer_addr + 1) & 0xFF) << 8;

	uint8_t v = cpu_read(c, addr);
	c->A ^= v;
	SET_FLAGS(c, c->A);
	return 6;
}
int handle_EOR_INDY(CPU *c)
{
	uint8_t zp_addr = fetch_byte(c);

	uint16_t base_addr = cpu_read(c, zp_addr);
	base_addr |= cpu_read(c, (zp_addr + 1) & 0xFF) << 8;

	uint16_t addr = base_addr + c->Y;
	int page_crossed = (base_addr & 0xFF00) != (addr & 0xFF00);

	uint8_t v = cpu_read(c, addr);
	c->A ^= v;
	SET_FLAGS(c, c->A);
	return 5 + page_crossed;
}
int handle_BIT_ZP(CPU *c)
{
	uint8_t addr = fetch_byte(c);
	uint8_t v = cpu_read(c, addr);
	uint8_t res = c->A & v;

	set_flag(c, FLAG_Z, res == 0);
	set_flag(c, FLAG_N, v & 0x80);
	set_flag(c, FLAG_V, v & 0x40);
	return 3;
}
int handle_BIT_ABS(CPU *c)
{
	uint16_t addr = fetch_word(c);
	uint8_t v = cpu_read(c, addr);
	uint8_t res = c->A & v;

	set_flag(c, FLAG_Z, res == 0);
	set_flag(c, FLAG_N, v & 0x80);
	set_flag(c, FLAG_V, v & 0x40);
	return 4;
}
int handle_ADC_IMM(CPU *c)
{
	uint8_t v = fetch_byte(c);
	uint8_t carry = get_flag(c, FLAG_C);
	uint8_t old_a = c->A;
	uint16_t res = (uint16_t) c->A + ((uint16_t) v + (uint16_t) carry);
	c->A = (uint8_t) res;

	set_flag(c, FLAG_C, res > 0xff);
	set_flag(c, FLAG_Z, c->A == 0);
	set_flag(c, FLAG_V, (old_a ^ c->A) & (v ^ c->A) & 0x80);
	set_flag(c, FLAG_N, c->A & 0x80);
	return 2;
}
int handle_ADC_ZP(CPU *c)
{
	uint8_t addr = fetch_byte(c);
	uint8_t v = cpu_read(c, addr);
	uint8_t carry = get_flag(c, FLAG_C);
	uint8_t old_a = c->A;
	uint16_t res = (uint16_t) c->A + ((uint16_t) v + (uint16_t) carry);
	c->A = (uint8_t) res;

	set_flag(c, FLAG_C, res > 0xff);
	set_flag(c, FLAG_Z, c->A == 0);
	set_flag(c, FLAG_V, (old_a ^ c->A) & (v ^ c->A) & 0x80);
	set_flag(c, FLAG_N, c->A & 0x80);
	return 3;
}
int handle_ADC_ZPX(CPU *c)
{
	uint8_t base_addr = fetch_byte(c);
	uint8_t addr = (base_addr + c->X) & 0xff;
	uint8_t v = cpu_read(c, addr);
	uint8_t carry = get_flag(c, FLAG_C);
	uint8_t old_a = c->A;
	uint16_t res = (uint16_t) c->A + ((uint16_t) v + (uint16_t) carry);
	c->A = (uint8_t) res;

	set_flag(c, FLAG_C, res > 0xff);
	set_flag(c, FLAG_Z, c->A == 0);
	set_flag(c, FLAG_V, (old_a ^ c->A) & (v ^ c->A) & 0x80);
	set_flag(c, FLAG_N, c->A & 0x80);
	return 4;
}
int handle_ADC_ABS(CPU *c)
{
	uint16_t addr = fetch_word(c);
	uint8_t v = cpu_read(c, addr);
	uint8_t carry = get_flag(c, FLAG_C);
	uint8_t old_a = c->A;
	uint16_t res = (uint16_t) c->A + ((uint16_t) v + (uint16_t) carry);
	c->A = (uint8_t) res;

	set_flag(c, FLAG_C, res > 0xff);
	set_flag(c, FLAG_Z, c->A == 0);
	set_flag(c, FLAG_V, (old_a ^ c->A) & (v ^ c->A) & 0x80);
	set_flag(c, FLAG_N, c->A & 0x80);
	return 4;
}
int handle_ADC_ABSX(CPU *c)
{
	uint16_t base_addr = fetch_word(c);
	uint16_t addr = base_addr + c->X;
	int page_crossed = (base_addr & 0xFF00) != (addr & 0xFF00);

	uint8_t v = cpu_read(c, addr);
	uint8_t carry = get_flag(c, FLAG_C);
	uint8_t old_a = c->A;
	uint16_t res = (uint16_t) c->A + (uint16_t) v + (uint16_t) carry;
	c->A = (uint8_t) res;

	set_flag(c, FLAG_C, res > 0xFF);
	set_flag(c, FLAG_Z, c->A == 0);
	set_flag(c, FLAG_V, (old_a ^ c->A) & (v ^ c->A) & 0x80);
	set_flag(c, FLAG_N, c->A & 0x80);
	return 4 + page_crossed;
}
int handle_ADC_ABSY(CPU *c)
{
	uint16_t base_addr = fetch_word(c);
	uint16_t addr = base_addr + c->Y;
	int page_crossed = (base_addr & 0xFF00) != (addr & 0xFF00);

	uint8_t v = cpu_read(c, addr);
	uint8_t carry = get_flag(c, FLAG_C);
	uint8_t old_a = c->A;
	uint16_t res = (uint16_t) c->A + (uint16_t) v + (uint16_t) carry;
	c->A = (uint8_t) res;

	set_flag(c, FLAG_C, res > 0xFF);
	set_flag(c, FLAG_Z, c->A == 0);
	set_flag(c, FLAG_V, (old_a ^ c->A) & (v ^ c->A) & 0x80);
	set_flag(c, FLAG_N, c->A & 0x80);
	return 4 + page_crossed;
}
int handle_ADC_INDX(CPU *c)
{
	uint8_t zp_addr = fetch_byte(c);
	uint8_t pointer_addr = (zp_addr + c->X) & 0xFF;

	uint16_t addr = cpu_read(c, pointer_addr);
	addr |= cpu_read(c, (pointer_addr + 1) & 0xFF) << 8;

	uint8_t v = cpu_read(c, addr);
	uint8_t carry = get_flag(c, FLAG_C);
	uint8_t old_a = c->A;
	uint16_t res = (uint16_t) c->A + (uint16_t) v + (uint16_t) carry;
	c->A = (uint8_t) res;

	set_flag(c, FLAG_C, res > 0xFF);
	set_flag(c, FLAG_Z, c->A == 0);
	set_flag(c, FLAG_V, (old_a ^ c->A) & (v ^ c->A) & 0x80);
	set_flag(c, FLAG_N, c->A & 0x80);
	return 6;
}
int handle_ADC_INDY(CPU *c)
{
	uint8_t zp_addr = fetch_byte(c);

	uint16_t base_addr = cpu_read(c, zp_addr);
	base_addr |= cpu_read(c, (zp_addr + 1) & 0xFF) << 8;

	uint16_t addr = base_addr + c->Y;
	int page_crossed = (base_addr & 0xFF00) != (addr & 0xFF00);

	uint8_t v = cpu_read(c, addr);
	uint8_t carry = get_flag(c, FLAG_C);
	uint8_t old_a = c->A;
	uint16_t res = (uint16_t) c->A + (uint16_t) v + (uint16_t) carry;
	c->A = (uint8_t) res;

	set_flag(c, FLAG_C, res > 0xFF);
	set_flag(c, FLAG_Z, c->A == 0);
	set_flag(c, FLAG_V, (old_a ^ c->A) & (v ^ c->A) & 0x80);
	set_flag(c, FLAG_N, c->A & 0x80);
	return 5 + page_crossed;
}
int handle_SBC_IMM(CPU *c)
{
	uint8_t v = fetch_byte(c);
	uint8_t _A = c->A;
	uint8_t carry = get_flag(c, FLAG_C);
	c->A = c->A + ~v + carry;

	set_flag(c, FLAG_C,
			 (uint16_t) _A + (uint16_t) (~v) + (uint16_t) carry > 0xFF);
	set_flag(c, FLAG_Z, c->A == 0);
	set_flag(c, FLAG_V, (_A ^ c->A) & (_A ^ ~v) & 0x80);
	set_flag(c, FLAG_N, c->A & 0x80);
	return 2;
}
int handle_SBC_ZP(CPU *c)
{
	uint8_t addr = fetch_byte(c);
	uint8_t v = cpu_read(c, addr);
	uint8_t old_A = c->A;
	uint8_t carry = get_flag(c, FLAG_C);
	c->A = c->A + ~v + carry;

	set_flag(c, FLAG_C,
			 (uint16_t) old_A + (uint16_t) (~v) + (uint16_t) carry > 0xFF);
	set_flag(c, FLAG_Z, c->A == 0);
	set_flag(c, FLAG_V, (old_A ^ c->A) & (old_A ^ ~v) & 0x80);
	set_flag(c, FLAG_N, c->A & 0x80);
	return 3;
}
int handle_SBC_ZPX(CPU *c)
{
	uint8_t base_addr = fetch_byte(c);
	uint8_t addr = (base_addr + c->X) & 0xff;
	uint8_t v = cpu_read(c, addr);
	uint8_t old_A = c->A;
	uint8_t carry = get_flag(c, FLAG_C);
	c->A = c->A + ~v + carry;

	set_flag(c, FLAG_C,
			 (uint16_t) old_A + (uint16_t) (~v) + (uint16_t) carry > 0xFF);
	set_flag(c, FLAG_Z, c->A == 0);
	set_flag(c, FLAG_V, (old_A ^ c->A) & (old_A ^ ~v) & 0x80);
	set_flag(c, FLAG_N, c->A & 0x80);
	return 4;
}
int handle_SBC_ABS(CPU *c)
{
	uint16_t addr = fetch_word(c);
	uint8_t v = cpu_read(c, addr);
	uint8_t old_A = c->A;
	uint8_t carry = get_flag(c, FLAG_C);
	c->A = c->A + ~v + carry;

	set_flag(c, FLAG_C,
			 (uint16_t) old_A + (uint16_t) (~v) + (uint16_t) carry > 0xFF);
	set_flag(c, FLAG_Z, c->A == 0);
	set_flag(c, FLAG_V, (old_A ^ c->A) & (old_A ^ ~v) & 0x80);
	set_flag(c, FLAG_N, c->A & 0x80);
	return 4;
}
int handle_SBC_ABSX(CPU *c)
{
	uint16_t base_addr = fetch_word(c);
	uint16_t addr = base_addr + c->X;
	int page_crossed = (base_addr & 0xFF00) != (addr & 0xFF00);

	uint8_t v = cpu_read(c, addr);
	uint8_t old_A = c->A;
	uint8_t carry = get_flag(c, FLAG_C);
	c->A = c->A + ~v + carry;

	set_flag(c, FLAG_C,
			 (uint16_t) old_A + (uint16_t) (~v) + (uint16_t) carry > 0xFF);
	set_flag(c, FLAG_Z, c->A == 0);
	set_flag(c, FLAG_V, (old_A ^ c->A) & (old_A ^ ~v) & 0x80);
	set_flag(c, FLAG_N, c->A & 0x80);
	return 4 + page_crossed;
}
int handle_SBC_ABSY(CPU *c)
{
	uint16_t base_addr = fetch_word(c);
	uint16_t addr = base_addr + c->Y;
	int page_crossed = (base_addr & 0xFF00) != (addr & 0xFF00);

	uint8_t v = cpu_read(c, addr);
	uint8_t old_A = c->A;
	uint8_t carry = get_flag(c, FLAG_C);
	c->A = c->A + ~v + carry;

	set_flag(c, FLAG_C,
			 (uint16_t) old_A + (uint16_t) (~v) + (uint16_t) carry > 0xFF);
	set_flag(c, FLAG_Z, c->A == 0);
	set_flag(c, FLAG_V, (old_A ^ c->A) & (old_A ^ ~v) & 0x80);
	set_flag(c, FLAG_N, c->A & 0x80);
	return 4 + page_crossed;
}
int handle_SBC_INDX(CPU *c)
{
	uint8_t zp_addr = fetch_byte(c);
	uint8_t pointer_addr = (zp_addr + c->X) & 0xFF;

	uint16_t addr = cpu_read(c, pointer_addr);
	addr |= cpu_read(c, (pointer_addr + 1) & 0xFF) << 8;

	uint8_t v = cpu_read(c, addr);
	uint8_t old_A = c->A;
	uint8_t carry = get_flag(c, FLAG_C);
	c->A = c->A + ~v + carry;

	set_flag(c, FLAG_C,
			 (uint16_t) old_A + (uint16_t) (~v) + (uint16_t) carry > 0xFF);
	set_flag(c, FLAG_Z, c->A == 0);
	set_flag(c, FLAG_V, (old_A ^ c->A) & (old_A ^ ~v) & 0x80);
	set_flag(c, FLAG_N, c->A & 0x80);
	return 6;
}
int handle_SBC_INDY(CPU *c)
{
	uint8_t zp_addr = fetch_byte(c);

	uint16_t base_addr = cpu_read(c, zp_addr);
	base_addr |= cpu_read(c, (zp_addr + 1) & 0xFF) << 8;

	uint16_t addr = base_addr + c->Y;
	int page_crossed = (base_addr & 0xFF00) != (addr & 0xFF00);

	uint8_t v = cpu_read(c, addr);
	uint8_t old_A = c->A;
	uint8_t carry = get_flag(c, FLAG_C);
	c->A = c->A + ~v + carry;

	set_flag(c, FLAG_C,
			 (uint16_t) old_A + (uint16_t) (~v) + (uint16_t) carry > 0xFF);
	set_flag(c, FLAG_Z, c->A == 0);
	set_flag(c, FLAG_V, (old_A ^ c->A) & (old_A ^ ~v) & 0x80);
	set_flag(c, FLAG_N, c->A & 0x80);
	return 5 + page_crossed;
}
int handle_CMP_IMM(CPU *c)
{
	uint8_t v = fetch_byte(c);
	set_flag(c, FLAG_C, c->A >= v);
	set_flag(c, FLAG_Z, c->A == v);
	set_flag(c, FLAG_N, c->A & 0x80);
	return 2;
}
int handle_CMP_ZP(CPU *c)
{
	uint8_t addr = fetch_byte(c);
	uint8_t v = cpu_read(c, addr);
	set_flag(c, FLAG_C, c->A >= v);
	set_flag(c, FLAG_Z, c->A == v);
	set_flag(c, FLAG_N, c->A & 0x80);
	return 3;
}
int handle_CMP_ZPX(CPU *c)
{
	uint8_t base_addr = fetch_byte(c);
	uint8_t addr = (base_addr + c->X) & 0xff;
	uint8_t v = cpu_read(c, addr);
	set_flag(c, FLAG_C, c->A >= v);
	set_flag(c, FLAG_Z, c->A == v);
	set_flag(c, FLAG_N, c->A & 0x80);
	return 4;
}
int handle_CMP_ABS(CPU *c)
{
	uint16_t addr = fetch_word(c);
	uint8_t v = cpu_read(c, addr);
	set_flag(c, FLAG_C, c->A >= v);
	set_flag(c, FLAG_Z, c->A == v);
	set_flag(c, FLAG_N, c->A & 0x80);
	return 4;
}
int handle_CMP_ABSX(CPU *c)
{
	uint16_t base_addr = fetch_word(c);
	uint16_t addr = base_addr + c->X;
	int page_crossed = (base_addr & 0xFF00) != (addr & 0xFF00);

	uint8_t v = cpu_read(c, addr);
	set_flag(c, FLAG_C, c->A >= v);
	set_flag(c, FLAG_Z, c->A == v);
	set_flag(c, FLAG_N, c->A & 0x80);
	return 4 + page_crossed;
}
int handle_CMP_ABSY(CPU *c)
{
	uint16_t base_addr = fetch_word(c);
	uint16_t addr = base_addr + c->Y;
	int page_crossed = (base_addr & 0xFF00) != (addr & 0xFF00);

	uint8_t v = cpu_read(c, addr);
	set_flag(c, FLAG_C, c->A >= v);
	set_flag(c, FLAG_Z, c->A == v);
	set_flag(c, FLAG_N, c->A & 0x80);
	return 4 + page_crossed;
}
int handle_CMP_INDX(CPU *c)
{
	uint8_t zp_addr = fetch_byte(c);
	uint8_t pointer_addr = (zp_addr + c->X) & 0xFF;

	uint16_t addr = cpu_read(c, pointer_addr);
	addr |= cpu_read(c, (pointer_addr + 1) & 0xFF) << 8;

	uint8_t v = cpu_read(c, addr);
	set_flag(c, FLAG_C, c->A >= v);
	set_flag(c, FLAG_Z, c->A == v);
	set_flag(c, FLAG_N, c->A & 0x80);
	return 6;
}
int handle_CMP_INDY(CPU *c)
{
	uint8_t zp_addr = fetch_byte(c);

	uint16_t base_addr = cpu_read(c, zp_addr);
	base_addr |= cpu_read(c, (zp_addr + 1) & 0xFF) << 8;

	uint16_t addr = base_addr + c->Y;
	int page_crossed = (base_addr & 0xFF00) != (addr & 0xFF00);

	uint8_t v = cpu_read(c, addr);
	set_flag(c, FLAG_C, c->A >= v);
	set_flag(c, FLAG_Z, c->A == v);
	set_flag(c, FLAG_N, c->A & 0x80);
	return 5 + page_crossed;
}
int handle_CPX_IMM(CPU *c)
{
	uint8_t v = fetch_byte(c);
	set_flag(c, FLAG_C, c->X >= v);
	set_flag(c, FLAG_Z, c->X == v);
	set_flag(c, FLAG_N, c->X & 0x80);
	return 2;
}
int handle_CPX_ZP(CPU *c)
{
	uint8_t addr = fetch_byte(c);
	uint8_t v = cpu_read(c, addr);
	set_flag(c, FLAG_C, c->X >= v);
	set_flag(c, FLAG_Z, c->X == v);
	set_flag(c, FLAG_N, c->X & 0x80);
	return 3;
}
int handle_CPX_ABS(CPU *c)
{
	uint16_t addr = fetch_word(c);
	uint8_t v = cpu_read(c, addr);
	set_flag(c, FLAG_C, c->X >= v);
	set_flag(c, FLAG_Z, c->X == v);
	set_flag(c, FLAG_N, c->X & 0x80);
	return 4;
}
int handle_CPY_IMM(CPU *c)
{
	uint8_t v = fetch_byte(c);
	set_flag(c, FLAG_C, c->Y >= v);
	set_flag(c, FLAG_Z, c->Y == v);
	set_flag(c, FLAG_N, c->Y & 0x80);
	return 2;
}
int handle_CPY_ZP(CPU *c)
{
	uint8_t addr = fetch_byte(c);
	uint8_t v = cpu_read(c, addr);
	set_flag(c, FLAG_C, c->Y >= v);
	set_flag(c, FLAG_Z, c->Y == v);
	set_flag(c, FLAG_N, c->Y & 0x80);
	return 3;
}
int handle_CPY_ABS(CPU *c)
{
	uint16_t addr = fetch_word(c);
	uint8_t v = cpu_read(c, addr);
	set_flag(c, FLAG_C, c->Y >= v);
	set_flag(c, FLAG_Z, c->Y == v);
	set_flag(c, FLAG_N, c->Y & 0x80);
	return 4;
}
int handle_INC_ZP(CPU *c)
{
	uint8_t addr = fetch_byte(c);
	uint8_t v = cpu_read(c, addr);
	v++;
	cpu_write(c, addr, v);
	set_flag(c, FLAG_Z, v == 0);
	set_flag(c, FLAG_N, v & 0x80);
	return 5;
}
int handle_INC_ZPX(CPU *c)
{
	uint8_t base_addr = fetch_byte(c);
	uint8_t addr = (base_addr + c->X) & 0xff;
	uint8_t v = cpu_read(c, addr);
	v++;
	cpu_write(c, addr, v);
	set_flag(c, FLAG_Z, v == 0);
	set_flag(c, FLAG_N, v & 0x80);
	return 6;
}
int handle_INC_ABS(CPU *c)
{
	uint16_t addr = fetch_word(c);
	uint8_t v = cpu_read(c, addr);
	v++;
	cpu_write(c, addr, v);
	set_flag(c, FLAG_Z, v == 0);
	set_flag(c, FLAG_N, v & 0x80);
	return 6;
}
int handle_INC_ABSX(CPU *c)
{
	uint16_t base_addr = fetch_word(c);
	uint16_t addr = base_addr + c->X;

	uint8_t v = cpu_read(c, addr);
	cpu_write(c, addr, v);		// Read-modify-write: write original
	v++;
	cpu_write(c, addr, v);		// Write modified

	set_flag(c, FLAG_Z, v == 0);
	set_flag(c, FLAG_N, v & 0x80);
	return 7;
}
int handle_INX(CPU *c)
{
	c->X++;
	set_flag(c, FLAG_Z, c->X == 0);
	set_flag(c, FLAG_N, c->X & 0x80);
	return 2;
}
int handle_INY(CPU *c)
{
	c->Y++;
	set_flag(c, FLAG_Z, c->Y == 0);
	set_flag(c, FLAG_N, c->Y & 0x80);
	return 2;
}
int handle_DEC_ZP(CPU *c)
{
	uint8_t addr = fetch_byte(c);
	uint8_t v = cpu_read(c, addr);
	v--;
	cpu_write(c, addr, v);
	set_flag(c, FLAG_Z, v == 0);
	set_flag(c, FLAG_N, v & 0x80);
	return 5;
}
int handle_DEC_ZPX(CPU *c)
{
	uint8_t base_addr = fetch_byte(c);
	uint8_t addr = (base_addr + c->X) & 0xff;
	uint8_t v = cpu_read(c, addr);
	v--;
	cpu_write(c, addr, v);
	set_flag(c, FLAG_Z, v == 0);
	set_flag(c, FLAG_N, v & 0x80);
	return 6;
}
int handle_DEC_ABS(CPU *c)
{
	uint16_t addr = fetch_word(c);
	uint8_t v = cpu_read(c, addr);
	v--;
	cpu_write(c, addr, v);
	set_flag(c, FLAG_Z, v == 0);
	set_flag(c, FLAG_N, v & 0x80);
	return 6;
}
int handle_DEC_ABSX(CPU *c)
{
	uint16_t base_addr = fetch_word(c);
	uint16_t addr = base_addr + c->X;

	uint8_t v = cpu_read(c, addr);
	cpu_write(c, addr, v);
	v--;
	cpu_write(c, addr, v);

	set_flag(c, FLAG_Z, v == 0);
	set_flag(c, FLAG_N, v & 0x80);
	return 7;
}
int handle_DEX(CPU *c)
{
	c->X--;
	set_flag(c, FLAG_Z, c->X == 0);
	set_flag(c, FLAG_N, c->X & 0x80);
	return 2;
}
int handle_DEY(CPU *c)
{
	c->Y--;
	set_flag(c, FLAG_Z, c->Y == 0);
	set_flag(c, FLAG_N, c->Y & 0x80);
	return 2;
}
int handle_ASL_ACC(CPU *c)
{
	uint8_t old_A = c->A;
	c->A <<= 1;
	set_flag(c, FLAG_C, old_A & 0x80);
	SET_FLAGS(c, c->A);
	return 2;
}
int handle_ASL_ZP(CPU *c)
{
	uint8_t addr = fetch_byte(c);
	uint8_t v = cpu_read(c, addr);
	uint8_t old_v = v;
	cpu_write(c, addr, v);
	v <<= 1;
	cpu_write(c, addr, v);
	set_flag(c, FLAG_C, old_v & 0x80);
	SET_FLAGS(c, v);
	return 5;
}
int handle_ASL_ZPX(CPU *c)
{
	uint8_t base_addr = fetch_byte(c);
	uint8_t addr = (base_addr + c->X) & 0xff;
	uint8_t v = cpu_read(c, addr);
	uint8_t old_v = v;
	cpu_write(c, addr, v);
	v <<= 1;
	cpu_write(c, addr, v);
	set_flag(c, FLAG_C, old_v & 0x80);
	SET_FLAGS(c, v);
	return 6;
}
int handle_ASL_ABS(CPU *c)
{
	uint16_t addr = fetch_word(c);
	uint8_t v = cpu_read(c, addr);
	uint8_t old_v = v;
	cpu_write(c, addr, v);
	v <<= 1;
	cpu_write(c, addr, v);
	set_flag(c, FLAG_C, old_v & 0x80);
	SET_FLAGS(c, v);
	return 6;
}
int handle_ASL_ABSX(CPU *c)
{
	uint16_t base_addr = fetch_word(c);
	uint16_t addr = base_addr + c->X;

	uint8_t v = cpu_read(c, addr);
	cpu_write(c, addr, v);
	uint8_t old_v = v;
	v <<= 1;
	cpu_write(c, addr, v);

	set_flag(c, FLAG_C, old_v & 0x80);
	SET_FLAGS(c, v);
	return 7;
}
int handle_LSR_ACC(CPU *c)
{
	uint8_t old_A = c->A;
	c->A >>= 1;
	set_flag(c, FLAG_C, old_A & 0x01);
	SET_FLAGS(c, c->A);
	return 2;
}
int handle_LSR_ZP(CPU *c)
{
	uint8_t addr = fetch_byte(c);
	uint8_t v = cpu_read(c, addr);
	uint8_t old_v = v;
	cpu_write(c, addr, v);
	v >>= 1;
	cpu_write(c, addr, v);
	set_flag(c, FLAG_C, old_v & 0x01);
	SET_FLAGS(c, v);
	return 5;
}
int handle_LSR_ZPX(CPU *c)
{
	uint8_t base_addr = fetch_byte(c);
	uint8_t addr = (base_addr + c->X) & 0xff;
	uint8_t v = cpu_read(c, addr);
	uint8_t old_v = v;
	cpu_write(c, addr, v);
	v >>= 1;
	cpu_write(c, addr, v);
	set_flag(c, FLAG_C, old_v & 0x80);
	SET_FLAGS(c, v);
	return 6;
}
int handle_LSR_ABS(CPU *c)
{
	uint16_t addr = fetch_word(c);
	uint8_t v = cpu_read(c, addr);
	uint8_t old_v = v;
	cpu_write(c, addr, v);
	v >>= 1;
	cpu_write(c, addr, v);
	set_flag(c, FLAG_C, old_v & 0x01);
	SET_FLAGS(c, v);
	return 6;
}
int handle_LSR_ABSX(CPU *c)
{
	uint16_t base_addr = fetch_word(c);
	uint16_t addr = base_addr + c->X;

	uint8_t v = cpu_read(c, addr);
	cpu_write(c, addr, v);
	uint8_t old_v = v;
	v >>= 1;
	cpu_write(c, addr, v);

	set_flag(c, FLAG_C, old_v & 0x01);
	SET_FLAGS(c, v);
	return 7;
}
int handle_ROL_ACC(CPU *c)
{
	uint8_t old_A = c->A;
	uint8_t carry_in = get_flag(c, FLAG_C);

	c->A = (old_A << 1) | carry_in;

	set_flag(c, FLAG_C, old_A & 0x80);
	SET_FLAGS(c, c->A);
	return 2;
}
int handle_ROL_ZP(CPU *c)
{
	uint8_t addr = fetch_byte(c);
	uint8_t v = cpu_read(c, addr);

	cpu_write(c, addr, v);

	uint8_t carry_in = get_flag(c, FLAG_C);
	uint8_t new_value = (v << 1) | carry_in;

	cpu_write(c, addr, new_value);

	set_flag(c, FLAG_C, v & 0x80);
	SET_FLAGS(c, new_value);
	return 5;
}
int handle_ROL_ZPX(CPU *c)
{
	uint8_t base_addr = fetch_byte(c);
	uint8_t addr = (base_addr + c->X) & 0xff;
	uint8_t v = cpu_read(c, addr);

	cpu_write(c, addr, v);

	uint8_t carry_in = get_flag(c, FLAG_C);
	uint8_t new_value = (v << 1) | carry_in;

	cpu_write(c, addr, new_value);

	set_flag(c, FLAG_C, v & 0x80);
	SET_FLAGS(c, new_value);
	return 6;
}
int handle_ROL_ABS(CPU *c)
{
	uint16_t addr = fetch_word(c);
	uint8_t v = cpu_read(c, addr);

	cpu_write(c, addr, v);

	uint8_t carry_in = get_flag(c, FLAG_C);
	uint8_t new_value = (v << 1) | carry_in;

	cpu_write(c, addr, new_value);

	set_flag(c, FLAG_C, v & 0x80);
	SET_FLAGS(c, new_value);
	return 6;
}
int handle_ROL_ABSX(CPU *c)
{
	uint16_t base_addr = fetch_word(c);
	uint16_t addr = base_addr + c->X;

	uint8_t value = cpu_read(c, addr);
	cpu_write(c, addr, value);

	uint8_t carry_in = get_flag(c, FLAG_C);
	uint8_t new_value = (value << 1) | carry_in;

	cpu_write(c, addr, new_value);

	set_flag(c, FLAG_C, value & 0x80);
	SET_FLAGS(c, new_value);
	return 7;
}
int handle_ROR_ACC(CPU *c)
{
	uint8_t old_A = c->A;
	uint8_t carry_in = get_flag(c, FLAG_C);

	c->A = (old_A >> 1) | (carry_in ? 0x80 : 0);

	set_flag(c, FLAG_C, old_A & 0x80);
	SET_FLAGS(c, c->A);
	return 2;
}
int handle_ROR_ZP(CPU *c)
{
	uint8_t addr = fetch_byte(c);
	uint8_t v = cpu_read(c, addr);

	cpu_write(c, addr, v);

	uint8_t carry_in = get_flag(c, FLAG_C);
	uint8_t new_value = (v >> 1) | (carry_in ? 0x80 : 0);

	cpu_write(c, addr, new_value);

	set_flag(c, FLAG_C, v & 0x01);
	SET_FLAGS(c, new_value);
	return 5;
}
int handle_ROR_ZPX(CPU *c)
{
	uint8_t base_addr = fetch_byte(c);
	uint8_t addr = (base_addr + c->X) & 0xff;
	uint8_t v = cpu_read(c, addr);

	cpu_write(c, addr, v);

	uint8_t carry_in = get_flag(c, FLAG_C);
	uint8_t new_value = (v >> 1) | (carry_in ? 0x80 : 0);

	cpu_write(c, addr, new_value);

	set_flag(c, FLAG_C, v & 0x01);
	SET_FLAGS(c, new_value);
	return 5;
}
int handle_ROR_ABS(CPU *c)
{
	uint16_t addr = fetch_word(c);
	uint8_t value = cpu_read(c, addr);

	cpu_write(c, addr, value);

	uint8_t carry_in = get_flag(c, FLAG_C);
	uint8_t new_value = (value >> 1) | (carry_in ? 0x80 : 0);

	cpu_write(c, addr, new_value);

	set_flag(c, FLAG_C, value & 0x01);
	SET_FLAGS(c, new_value);
	return 6;
}
int handle_ROR_ABSX(CPU *c)
{
	uint16_t base_addr = fetch_word(c);
	uint16_t addr = base_addr + c->X;

	uint8_t value = cpu_read(c, addr);
	cpu_write(c, addr, value);

	uint8_t carry_in = get_flag(c, FLAG_C);
	uint8_t new_value = (value >> 1) | (carry_in ? 0x80 : 0);

	cpu_write(c, addr, new_value);

	set_flag(c, FLAG_C, value & 0x01);
	SET_FLAGS(c, new_value);
	return 7;
}
int handle_JMP_ABS(CPU *c)
{
	uint16_t addr = fetch_word(c);
	c->PC = addr;
	return 3;
}
int handle_JMP_IND(CPU *c)
{
	uint16_t pointer_addr = fetch_word(c);

	// Read 16-bit address from pointer location (little-endian)
	// Note: 6502 has a bug where crossing page boundary doesn't work correctly
	uint16_t addr = cpu_read(c, pointer_addr);

	// If the pointer is at a page boundary (e.g., $xxFF), the high byte wraps
	if ((pointer_addr & 0x00FF) == 0x00FF) {
		// 6502 bug: reads from $xxFF and $xx00 instead of $xxFF and $xy00
		addr |= cpu_read(c, pointer_addr & 0xFF00) << 8;
	} else {
		addr |= cpu_read(c, pointer_addr + 1) << 8;
	}

	c->PC = addr;
	return 5;
}
int handle_JSR_ABS(CPU *c)
{
	uint16_t target_addr = fetch_word(c);

	// Push return address - 1 (PC already points after the instruction)
	push_word(c, c->PC - 1);

	c->PC = target_addr;
	return 6;
}
int handle_RTS(CPU *c)
{
	uint16_t return_addr = pop_word(c);
	c->PC = return_addr + 1;	// Return to instruction after JSR
	return 6;
}
int handle_BCC(CPU *c)			// Branch if Carry Clear
{
	return handle_branch(c, !get_flag(c, FLAG_C));
}

int handle_BCS(CPU *c)			// Branch if Carry Set
{
	return handle_branch(c, get_flag(c, FLAG_C));
}

int handle_BEQ(CPU *c)			// Branch if Equal (Zero set)
{
	return handle_branch(c, get_flag(c, FLAG_Z));
}

int handle_BNE(CPU *c)			// Branch if Not Equal (Zero clear)
{
	return handle_branch(c, !get_flag(c, FLAG_Z));
}

int handle_BMI(CPU *c)			// Branch if Minus (Negative set)
{
	return handle_branch(c, get_flag(c, FLAG_N));
}

int handle_BPL(CPU *c)			// Branch if Plus (Negative clear)
{
	return handle_branch(c, !get_flag(c, FLAG_N));
}

int handle_BVC(CPU *c)			// Branch if Overflow Clear
{
	return handle_branch(c, !get_flag(c, FLAG_V));
}

int handle_BVS(CPU *c)			// Branch if Overflow Set
{
	return handle_branch(c, get_flag(c, FLAG_V));
}
int handle_CLC(CPU *c)
{
	set_flag(c, FLAG_C, 0);
	return 2;
}
int handle_CLD(CPU *c)
{
	set_flag(c, FLAG_D, 0);
	return 2;
}
int handle_CLI(CPU *c)
{
	interupt_disable = 0;
	c->interupt_flags |= DELAYED_INTERRUPT_DISABLE;
	return 2;
}
int handle_CLV(CPU *c)
{
	set_flag(c, FLAG_V, 0);
	return 2;
}
int handle_SEC(CPU *c)
{
	set_flag(c, FLAG_C, 1);
	return 2;
}
int handle_SED(CPU *c)
{
	set_flag(c, FLAG_D, 1);
	return 2;
}
int handle_SEI(CPU *c)
{
	interupt_disable = 1;
	c->interupt_flags |= DELAYED_INTERRUPT_DISABLE;
	return 2;
}
int handle_BRK(CPU *c)
{
	c->PC++;					// Skip next byte (padding)

	// Push PC and status register
	push_word(c, c->PC);
	push_byte(c, c->P | FLAG_B | FLAG_U);	// Set B flag for BRK

	// Set interrupt disable flag
	set_flag(c, FLAG_I, 1);

	// Jump to interrupt vector
	uint16_t vector = cpu_read(c, 0xFFFE) | (cpu_read(c, 0xFFFF) << 8);
	c->PC = vector;
	return 7;
}
int handle_NOP(CPU *c)
{
	return 2;
}
int handle_RTI(CPU *c)
{
	// Pop status register - only update the 6 actual flags
	uint8_t flags = pop_byte(c);
	uint8_t mask = FLAG_N | FLAG_V | FLAG_D | FLAG_Z | FLAG_C;
	c->P = (c->P & ~mask) | (flags & mask);

	// For RTI, I flag change is immediate (no delay)
	uint8_t new_i_flag = flags & FLAG_I;
	if ((c->P & FLAG_I) != new_i_flag) {
		interupt_disable = (new_i_flag != 0);
		// NO delayed interrupt flag for RTI
	}
	// Pop program counter
	c->PC = pop_word(c);
	return 6;
}
/*********************************DEBUG***************************************/
void print_ram(CPU *c)
{
	printf("RAM:\n");
	for (int i = 0; i < RAM_SIZE_BYTES; i++) {
		if (i % 10 == 0 && i != 0) {
			printf("\n");
		}
		printf("%02x ", c->ram[i]);
	}
	printf("\n");
}

void cpu_coredump(CPU *c)
{
	const char *fmt =
		"CPU STATE:\nA: %02x\nX: %02x\nY: %02x\nSP: %02x\nPC: %04x\nP: %02x\n";
	printf(fmt, c->A, c->X, c->Y, c->SP, c->PC, c->P);
	print_ram(c);
}

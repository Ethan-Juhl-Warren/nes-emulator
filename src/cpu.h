/**
 * @file   cpu.h
 *
 * @brief  6502 CPU: core, registers, flags, memory bus 
 *         runs game code
 *
 * @author E Warren (ethanwarren768@gmail.com)
 * @date   2025-10-04
 */
#ifndef CPU_H
#define CPU_H

#include <stdint.h>
#include "ines.h"

#define RAM_SIZE_BYTES 2048 // Amount of internal NES RAM

/**
 * Represents the 6502 CPU core state.
 */
typedef struct {
    uint8_t A;                      // Accumulator register
    uint8_t X, Y;                   // Index registers
    uint8_t SP;                     // Stack pointer
    uint16_t PC;                    // Program counter
    uint8_t P;                      // Status flags register
    uint8_t ram[RAM_SIZE_BYTES];    // 2 KB of internal CPU RAM
	uint8_t interupt_flags; 
    INES *cart;                     // Pointer to loaded cartridge
    uint64_t cycles;                // Global cycle counter
    // TODO: Add callbacks for memory-mapped I/O and PPU/APU interaction
} CPU;

extern CPU *global_cpu;
/**
 * Resets the CPU to its power-on state.
 *
 * This function initializes registers, clears internal RAM,
 * and sets the program counter based on the reset vector in PRG ROM.
 *
 * @param[in,out] c
 *     Pointer to the CPU instance to reset.
 */
void cpu_reset(CPU *c);

/**
 * Executes a single instruction cycle.
 *
 * This function fetches the next opcode at PC, increments PC,
 * decodes the instruction, executes it, and updates the cycle count.
 *
 * @param[in,out] c
 *     Pointer to the CPU instance being stepped.
 *
 * @return
 *     The number of cycles consumed by the executed instruction.
 */
int cpu_step(CPU *c);

/**
 * Reads a byte from the CPU address space.
 *
 * This function handles internal RAM, cartridge PRG ROM,
 * and memory-mapped hardware such as the PPU and APU.
 *
 * @param[in] c
 *     Pointer to the CPU instance.
 * @param[in] addr
 *     16-bit memory address to read from.
 *
 * @return
 *     The value read from memory at the given address.
 */
uint8_t cpu_read(CPU *c, uint16_t addr);

/**
 * Sends an interrupt to the CPU
 *
 * @param[in] c
 *		the cpu to interrupt
 * @param[in] type
 *		the interrupt to send
 *
 */
void cpu_interrupt(CPU *c, int type);

/**
 * Writes a byte into the CPU address space.
 *
 * This function supports writes to internal RAM, cartridge RAM (if present),
 * and memory-mapped hardware registers for the PPU and APU.
 *
 * @param[in,out] c
 *     Pointer to the CPU instance.
 * @param[in] addr
 *     16-bit memory address to write to.
 * @param[in] val
 *     Value to store at the specified address.
 */
void cpu_write(CPU *c, uint16_t addr, uint8_t val);

/**
 * Prints the full CPU state to trace
 *
 * @param[in] c
 *		Pointer to the CPU instance.
 */
void cpu_coredump(CPU *c);

#endif // CPU_H


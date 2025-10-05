/**
 * @file   main.c
 *
 * @brief  Entry point for the NES emulator:
 *         Initializes SDL, loads the ROM, initializes CPU and PPU,
 *         runs the main emulation loop, and handles rendering/input.
 *
 * @author E Warren (ethanwarren768@gmail.com)
 * @date   2025-07-29
 */

#include <SDL2/SDL.h>
#include <SDL2/SDL_timer.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "cntrler.h"
#include "cpu.h"
#include "ines.h"
#include "logging.h"
#include "ppu.h"
#include "screen.h"

#define DEBUG_MODE

PPU *global_ppu = NULL;
CPU *global_cpu = NULL;

int main(int argc, char **argv)
{
	// Initilize Logging
	init_log();

#ifdef DEBUG_MODE
	set_logstream(LOG_TO_CONSOLE);
#else
	set_logstream(LOG_TO_FILE);
#endif

	if (argc < 2) {
		print_error("No ROM specified. Usage: %s <path-to-ines-rom>", argv[0]);
		return 1;
	}
	// Load cartridge ROM
	INES cart = { 0 };
	ines_load(argv[1], &cart);

	if (cart.mapper != 0) {
		print_warning("Only mapper 0 (NROM) supported. ROM mapper found: %d",
					  cart.mapper);
	}

	Controller c1 = { 0 };
	Controller c2 = { 0 };

	// Initialize CPU
	CPU cpu = { 0 };
	cpu.cart = &cart;
	set_controllers(&c1, &c2);
	cpu_reset(&cpu);
	global_cpu = &cpu;
	// Initialize PPU
	PPU ppu;
	ppu_init(&ppu, &cart);
	global_ppu = &ppu;

	// Initilize the screen
	Screen screen = { 0 };
	if (screen_init(&screen, argv[1], SCREEN_WIDTH, SCREEN_HEIGHT)) {
		ines_free(&cart);
		return 1;
	}
	// Main emulation loop
	bool running = true;
	SDL_Event ev;
	const int cpu_cycles_per_frame = 29780;
	while (running) {
		uint32_t frame_start = SDL_GetTicks();

		// Process SDL events (window close, input)
		while (SDL_PollEvent(&ev)) {
			if (ev.type == SDL_QUIT)
				running = false;
			if (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_ESCAPE)
				running = false;
		}

		controller_set_state(&c1, get_controller_state_from_device());
		controller_set_state(&c2, get_controller_state_from_device());

		//Update controllers

		while (!ppu.frame_done) {
			// Execute one CPU step
			int cyc = cpu_step(&cpu);
			if (cpu.PC == 0x0000) {
				abort_e("Crashed to zero page");
			}
			// Run the PPU: 3 PPU cycles per CPU cycle (NTSC)
			for (int i = 0; i < cyc * 3; ++i) {
				ppu_clock(&ppu);
			}

			if (ppu.nmi_occurred && (ppu.ppuctrl & 0x80)) {
				cpu_interrupt(&cpu, (1 << 0));
				ppu.nmi_occurred = false;
			}
		}
		// TODO: Implement frame timing so emulator runs at ~60.098Hz (NTSC)
		// TODO: Support multiple mappers beyond NROM
		// TODO: Implement SDL audio output for APU
		// TODO: Add save/load state support
		// TODO: Add debug logging

		// Render framebuffer when a frame is ready
		screen_render(&screen, ppu.framebuffer);
		ppu.frame_done = false;

		int frame_time = SDL_GetTicks() - frame_start;
		const int frame_delay = 1000 / 60;	// ~16.67 ms
		if (frame_delay > frame_time) {
			SDL_Delay(frame_delay - frame_time);
		}
	}
	cpu_coredump(&cpu);
	// Cleanup
	screen_destroy(&screen);
	ines_free(&cart);
	return 0;
}

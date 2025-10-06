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

	INES cart = { 0 };
	ines_load(argv[1], &cart);

	if (cart.mapper != 0) {
		print_warning("Only mapper 0 (NROM) supported. ROM mapper found: %d",
					  cart.mapper);
	}

	Controller c1 = { 0 };
	Controller c2 = { 0 };

	CPU cpu = { 0 };
	cpu.cart = &cart;
	set_controllers(&c1, &c2);
	cpu_reset(&cpu);
	global_cpu = &cpu;

	PPU ppu;
	ppu_init(&ppu, &cart);
	global_ppu = &ppu;

	Screen screen = { 0 };
	if (screen_init(&screen, argv[1], SCREEN_WIDTH, SCREEN_HEIGHT)) {
		ines_free(&cart);
		return 1;
	}

	int frame_count = 0;
	int nmi_count = 0;
	uint8_t last_ppumask = 0xFF;
	uint8_t last_ppuctrl = 0xFF;

	bool running = true;
	SDL_Event ev;

	printf("Starting emulation...\n");

	while (running) {
		uint32_t frame_start = SDL_GetTicks();

		while (SDL_PollEvent(&ev)) {
			if (ev.type == SDL_QUIT)
				running = false;
			if (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_ESCAPE)
				running = false;
		}

		controller_set_state(&c1, get_controller_state_from_device());
		controller_set_state(&c2, get_controller_state_from_device());

		ppu.frame_done = false;
		while (!ppu.frame_done) {
			int cyc = cpu_step(&cpu);
			if (cpu.PC == 0x0000) {
				abort_e("Crashed to zero page");
			}
			for (int i = 0; i < cyc * 3; ++i) {
				ppu_clock(&ppu);
			}

			if (ppu.nmi_occurred && (ppu.ppuctrl & 0x80)) {
				cpu_interrupt(&cpu, (1 << 0));
				ppu.nmi_occurred = false;
				nmi_count++;
			}
			if (ppu.frame_done) {
				screen_render(&screen, ppu.framebuffer);
				ppu.frame_done = false;
				break;			// Exit to process SDL events
			}
		}

		// Report PPUMASK/PPUCTRL changes IMMEDIATELY
		if (ppu.ppumask != last_ppumask || ppu.ppuctrl != last_ppuctrl) {
			printf
				("Frame %d: PPUCTRL %02X->%02X, PPUMASK %02X->%02X (BG:%s SPR:%s)\n",
				 frame_count, last_ppuctrl, ppu.ppuctrl, last_ppumask,
				 ppu.ppumask, (ppu.ppumask & 0x08) ? "ON" : "OFF",
				 (ppu.ppumask & 0x10) ? "ON" : "OFF");
			last_ppumask = ppu.ppumask;
			last_ppuctrl = ppu.ppuctrl;
		}
		// Report sprite status every 60 frames
		if (frame_count % 60 == 0) {
			int visible_sprites = 0;
			for (int i = 0; i < 64; i++) {
				uint8_t y = ppu.oam[i * 4];
				if (y > 0 && y < 0xEF)
					visible_sprites++;
			}

			printf
				("Frame %d: PC=%04X NMIs=%d Visible sprites=%d PPUMASK=%02X\n",
				 frame_count, cpu.PC, nmi_count, visible_sprites, ppu.ppumask);

			// Show first visible sprite
			for (int i = 0; i < 64; i++) {
				uint8_t y = ppu.oam[i * 4];
				if (y > 0 && y < 0xEF) {
					printf
						("  First visible sprite: Y=%d Tile=%02X Attr=%02X X=%d\n",
						 y, ppu.oam[i * 4 + 1], ppu.oam[i * 4 + 2],
						 ppu.oam[i * 4 + 3]);
					break;
				}
			}
		}

		frame_count++;

		int frame_time = SDL_GetTicks() - frame_start;
		const int frame_delay = 1000 / 60;
		if (frame_delay > frame_time) {
			SDL_Delay(frame_delay - frame_time);
		}
	}

	printf("Exited at PC=%04X after %d frames\n", cpu.PC, frame_count);
	cpu_coredump(&cpu);

	screen_destroy(&screen);
	ines_free(&cart);
	return 0;
}

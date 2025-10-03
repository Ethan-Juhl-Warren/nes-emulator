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
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "cpu.h"
#include "ines.h"
#include "ppu.h"

int main(int argc, char **argv)
{
	if (argc < 2) {
		fprintf(stderr, "Usage: %s <path-to-ines-rom>\n", argv[0]);
		return 1;
	}
	// Load cartridge ROM
	INES cart = { 0 };
	if (ines_load(argv[1], &cart) != 0) {
		fprintf(stderr, "Failed to load ROM: %s\n", argv[1]);
		return 1;
	}

	if (cart.mapper != 0) {
		fprintf(stderr,
				"Warning: this starter only supports mapper 0 (NROM). ROM mapper=%d\n",
				cart.mapper);
	}
	// Initialize CPU
	CPU cpu = { 0 };
	cpu.cart = &cart;
	cpu_reset(&cpu);

	// Initialize PPU
	PPU ppu;
	ppu_init(&ppu);

	// Initialize SDL
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS) != 0) {
		fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
		ines_free(&cart);
		return 1;
	}

	SDL_Window *win =
		SDL_CreateWindow("nes-starter", SDL_WINDOWPOS_CENTERED,
						 SDL_WINDOWPOS_CENTERED,
						 256 * 2, 240 * 2, SDL_WINDOW_SHOWN);
	if (!win) {
		fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
		SDL_Quit();
		ines_free(&cart);
		return 1;
	}

	SDL_Renderer *ren =
		SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
	SDL_Texture *tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888,
										 SDL_TEXTUREACCESS_STREAMING, 256,
										 240);

	// Main emulation loop
	bool running = true;
	SDL_Event ev;
	while (running) {
		// Process SDL events (window close, input)
		while (SDL_PollEvent(&ev)) {
			if (ev.type == SDL_QUIT)
				running = false;
			if (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_ESCAPE)
				running = false;
		}

		// Execute one CPU step
		int cyc = cpu_step(&cpu);

		// Run the PPU: 3 PPU cycles per CPU cycle (NTSC)
		for (int i = 0; i < cyc * 3; ++i) {
			ppu_clock(&ppu);
		}

		// TODO: Implement frame timing so emulator runs at ~60.098Hz (NTSC)
		// TODO: Implement input handling/mapping for controllers
		// TODO: Support multiple mappers beyond NROM
		// TODO: Implement SDL audio output for APU
		// TODO: Add save/load state support
		// TODO: Add debug logging

		// Render framebuffer when a frame is ready
		if (ppu.frame_done) {
			SDL_UpdateTexture(tex, NULL, ppu.framebuffer,
							  256 * sizeof(uint32_t));
			SDL_RenderClear(ren);
			SDL_RenderCopy(ren, tex, NULL, NULL);
			SDL_RenderPresent(ren);
		}
		// TODO: Implement frame timing to match real NES speed
		SDL_Delay(0);			// temporary, avoid pegging CPU
	}

	// Cleanup
	SDL_DestroyTexture(tex);
	SDL_DestroyRenderer(ren);
	SDL_DestroyWindow(win);
	SDL_Quit();
	ines_free(&cart);
	return 0;
}

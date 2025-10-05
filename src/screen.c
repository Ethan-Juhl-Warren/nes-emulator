/**
 * @file screen.c
 *
 * @brief Handles the rendering of the PPU output to the screen
 *
 * @author E Warren (ethanwarren768@gmail.com)
 * @date 2025-10-04
 */
#include "screen.h"
#include "logging.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_video.h>
#include <stdint.h>

int screen_init(Screen *screen, const char *title, int width, int height)
{
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS) != 0) {
		print_error("SDL_Init failed: %s", SDL_GetError());
		return 1;
	}
	screen->window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED,
									  SDL_WINDOWPOS_CENTERED,
									  width * 2, height * 2, SDL_WINDOW_SHOWN);
	if (!screen->window) {
		print_error("SDL_CreateWindow failed: %s", SDL_GetError());
		SDL_Quit();
		return 1;
	}

	screen->renderer = SDL_CreateRenderer(screen->window, -1,
										  SDL_RENDERER_ACCELERATED);
	screen->texture = SDL_CreateTexture(screen->renderer,
										SDL_PIXELFORMAT_ARGB8888,
										SDL_TEXTUREACCESS_STREAMING,
										width, height);
	screen->screen_width = width;
	screen->screen_height = height;
	return 0;
}

void screen_render(Screen *screen, const void *framebuffer)
{
	SDL_UpdateTexture(screen->texture, NULL, framebuffer,
					  screen->screen_width * sizeof(uint32_t));
	SDL_RenderClear(screen->renderer);
	SDL_RenderCopy(screen->renderer, screen->texture, NULL, NULL);
	SDL_RenderPresent(screen->renderer);
}

void screen_destroy(Screen *screen)
{
	SDL_DestroyTexture(screen->texture);
	SDL_DestroyRenderer(screen->renderer);
	SDL_DestroyWindow(screen->window);
	SDL_Quit();
}

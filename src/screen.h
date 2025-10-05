/**
 * @file screen.h
 *
 * @brief Handles the rendering of the PPU output to the screen
 *
 * @author E Warren (ethanwarren768@gmail.com)
 * @date 2025-10-04
 */
#ifndef SCREEN_H
#define SCREEN_H

#include <SDL2/SDL.h>

typedef struct {
	SDL_Window *window;
	SDL_Renderer *renderer;
	SDL_Texture *texture;
	int screen_width;
	int screen_height;
} Screen;

/**
 * Initilizes the screen and SDL
 * 
 * @param[out] screen
 *		The resulting screen struct
 * @param[in] title
 *		The title of the screen
 * @param[in] width
 *		The width of the screen
 * @param[in] height
 *		The height of the screen
 * @return
 *		If the screen was intilized correctly
 */
int screen_init(Screen *screen, const char *title, int width, int height);

/**
 * Renders a framebuffer to the screen
 *
 * @param[in] screen
 *		The screen to render to
 * @param[in] framebuffer
 *		The frame buffer to render
 */
void screen_render(Screen *screen, const void *framebuffer);

/**
 * Frees the memory connected to the screen
 *
 * @param[in] screen
 *		The screen to free
 */
void screen_destroy(Screen *screen);
#endif //SCREEN_H

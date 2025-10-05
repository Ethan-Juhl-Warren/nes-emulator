/**
 * @file cntrler.h
 *
 * @brief Describes the nes controller
 *
 * @author E Warren (ethanwarren768@gmail.com)
 * @date 2025-10-05
 */
#ifndef CNTRLER_H
#define CNTRLER_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_scancode.h>
#include "stdint.h"
#define BUTTON_A      0x01
#define BUTTON_B      0x02
#define BUTTON_SELECT 0x04
#define BUTTON_START  0x08
#define BUTTON_UP     0x10
#define BUTTON_DOWN   0x20
#define BUTTON_LEFT   0x40
#define BUTTON_RIGHT  0x80

typedef struct {
	uint8_t state;
	uint8_t shift_reg;
	uint8_t strobe; //bool
} Controller;

typedef struct {
	SDL_Scancode a;
	SDL_Scancode b;
	SDL_Scancode select; 
	SDL_Scancode start;
    SDL_Scancode up;
	SDL_Scancode down;
	SDL_Scancode left;
	SDL_Scancode right;
} ButtonMap;


void controller_set_state(Controller *cntrl, uint8_t state);

uint8_t controller_read(Controller *cntrl);

void controller_write_strobe(Controller *cntrl, uint8_t val);

void set_controllers(Controller *c1, Controller *c2);
 
uint8_t get_controller_state_from_device(void);

#endif

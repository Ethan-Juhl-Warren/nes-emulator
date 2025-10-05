#include "cntrler.h"
#include <SDL2/SDL_scancode.h>
#include <stdint.h>

static ButtonMap btn_map = {
	.a = SDL_SCANCODE_X,
	.b = SDL_SCANCODE_Z,
	.select = SDL_SCANCODE_A,
	.start = SDL_SCANCODE_S,
	.up = SDL_SCANCODE_UP,
	.down = SDL_SCANCODE_DOWN,
	.left = SDL_SCANCODE_LEFT,
	.right = SDL_SCANCODE_RIGHT
};

void controller_set_state(Controller *cntrl, uint8_t state)
{
	cntrl->state = state;
}

uint8_t controller_read(Controller *cntrl)
{

	if (cntrl->strobe) {
		cntrl->shift_reg = cntrl->state;
	}
	// Return bit 0 of shift register, then shift right
	uint8_t result = cntrl->shift_reg & 1;
	cntrl->shift_reg >>= 1;
	cntrl->shift_reg |= 0x40;	// Set bit 6 (open bus behavior)

	return result;
}

void controller_write_strobe(Controller *ctrl, uint8_t val)
{
	uint8_t old_strobe = ctrl->strobe;
	ctrl->strobe = (val & 1);

	// On falling edge of strobe, latch controller state
	if (old_strobe && !ctrl->strobe) {
		ctrl->shift_reg = ctrl->state;
	}
}

uint8_t get_controller_state_from_device()
{
	const uint8_t *keys = SDL_GetKeyboardState(NULL);
	uint8_t state = 0;
	if (keys[btn_map.a])
		state |= BUTTON_A;
	if (keys[btn_map.b])
		state |= BUTTON_B;
	if (keys[btn_map.select])
		state |= BUTTON_SELECT;
	if (keys[btn_map.start])
		state |= BUTTON_START;
	if (keys[btn_map.up])
		state |= BUTTON_UP;
	if (keys[btn_map.down])
		state |= BUTTON_DOWN;
	if (keys[btn_map.left])
		state |= BUTTON_LEFT;
	if (keys[btn_map.right])
		state |= BUTTON_RIGHT;

	return state;
}

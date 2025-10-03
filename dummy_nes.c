/**
 * @file   dummy_nes.c
 *
 * @brief Generates a dummy .nes file which is valid but does nothing
 *
 * @author E Warren (ethanwarren768@gmail.com)
 * @date   2025-04-10
 */
#include <stdio.h>
#include <stdint.h>

int main(void)
{
	FILE *f = fopen("dummy.nes", "wb");
	if (!f)
		return 1;

	uint8_t header[16] = {
		0x4E, 0x45, 0x53, 0x1A,	// "NES<EOF>"
		0x01,					// 1 * 16KB PRG ROM
		0x00,					// 0 * 8KB CHR ROM
		0x00,					// flags 6
		0x00,					// flags 7
		0x00,					// flags 8
		0x00,					// flags 9
		0x00,					// flags 10
		0x00, 0x00, 0x00, 0x00, 0x00	// padding
	};
	fwrite(header, 1, 16, f);

	// Write 16KB of 0xEA (6502 NOP)
	for (int i = 0; i < 16 * 1024; i++) {
		fputc(0xEA, f);
	}

	fclose(f);
	return 0;
}

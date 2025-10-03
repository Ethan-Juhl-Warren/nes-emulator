/**
 * @file   ines.c
 *
 * @brief  Implementation of iNES ROM loader.
 *         Parses the iNES header, loads PRG/CHR banks,
 *         and manages cartridge memory.
 *
 * @author E Warren (ethanwarren768@gmail.com)
 * @date   2025-07-29
 */

#include "ines.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int ines_load(const char *path, INES *out)
{
	FILE *f = fopen(path, "rb");
	if (!f)
		return -1;

	uint8_t header[16];
	if (fread(header, 1, 16, f) != 16) {
		fclose(f);
		return -1;
	}

	if (memcmp(header, "NES\x1A", 4) != 0) {
		fclose(f);
		return -1;
	}

	size_t prg_pages = header[4];
	size_t chr_pages = header[5];
	uint8_t flags6 = header[6];

	out->mapper = (header[6] >> 4) | (header[7] & 0xF0);
	out->mirror = (flags6 & 0x01) ? 1 : 0;

	out->prg_size = prg_pages * 16384;
	out->chr_size = chr_pages * 8192;

	out->prg_rom = malloc(out->prg_size);
	if (!out->prg_rom) {
		fclose(f);
		return -1;
	}

	if (out->chr_size)
		out->chr_rom = malloc(out->chr_size);
	else
		out->chr_rom = NULL;

	// TODO: Implement support for trainer data if present
	if (flags6 & 0x04)
		fseek(f, 512, SEEK_CUR);

	if (fread(out->prg_rom, 1, out->prg_size, f) != out->prg_size) {
		fclose(f);
		return -1;
	}

	if (out->chr_size) {
		if (fread(out->chr_rom, 1, out->chr_size, f) != out->chr_size) {
			fclose(f);
			return -1;
		}
	}

	fclose(f);

	// TODO: Validate ROM integrity and iNES format variations
	// TODO: Support iNES 2.0 format
	return 0;
}

/**
 * Free allocated memory for an iNES structure.
 *
 * Frees PRG and CHR ROM buffers and clears pointers.
 */
void ines_free(INES *in)
{
	if (!in)
		return;

	free(in->prg_rom);
	free(in->chr_rom);

	in->prg_rom = NULL;
	in->chr_rom = NULL;

	// TODO: Consider clearing other INES metadata for safety
}

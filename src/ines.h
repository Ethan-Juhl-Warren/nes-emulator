/**
 * @file   ines.h
 *
 * @brief  ROM loader: parses the .nes iNES header and loads
 *         PRG/CHR banks
 *
 * @author E Warren (ethanwarren768@gmail.com)
 * @date   2025-10-04
 */
#ifndef INES_H
#define INES_H
#include <stdint.h>
#include <stddef.h>

/**
 * Holds cartridge metadata and raw ROM data
 * parsed from an iNES file.
 */
typedef struct {
    uint8_t *prg_rom;  // Pointer to PRG ROM data (CPU program)
    size_t prg_size;   // Size of PRG ROM in bytes
    uint8_t *chr_rom;  // Pointer to CHR ROM data (graphics patterns)
    size_t chr_size;   // Size of CHR ROM in bytes
    int mapper;        // Mapper ID (defines memory banking scheme)
    int mirror;        // Nametable mirroring: 0 = horizontal, 1 = vertical
} INES;

/**
 * Loads a .nes file into memory and parses the iNES header.
 *
 * This function allocates memory for PRG and CHR data,
 * validates the iNES header, and extracts mapper/mirroring info.
 *
 * @param[in]  path
 *     Path to the .nes ROM file to load.
 * @param[out] out
 *     Pointer to an INES struct to populate with cartridge data.
 *
 * @return
 *     EXIT_SUCCESS if the file was loaded successfully,
 *     or EXIT_FAILURE if the file was invalid or could not be opened.
 */
int ines_load(const char *path, INES *out);

/**
 * Frees memory associated with an INES structure.
 *
 * This function deallocates PRG/CHR buffers and clears metadata.
 *
 * @param[in,out] ines
 *     Pointer to an INES structure to release.
 */
void ines_free(INES *ines);

#endif // INES_H


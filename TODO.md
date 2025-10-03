General:
   TODO: Qwell the Wizards, put an end to magic number madness.
   TODO: Implement logging/debugging system for CPU/PPU state.
   TODO: Implement configuration file support for emulator settings.
   TODO: Implement graphical debugger (memory viewer, CPU state, PPU viewer).
   TODO: Implement test ROM runner to verify correctness against known test ROMs.
   TODO: Optimize performance for real-time emulation.
   TODO: Implement a clean shutdown and resource freeing mechanism.
   TODO: Write unit tests for CPU and PPU core logic.


Main Loop:
   TODO: Implement accurate frame timing (NES runs at ~60.098Hz NTSC).
   TODO: Add controller input mapping.
   TODO: Implement support for multiple mappers.
   TODO: Implement audio output via SDL2 Audio API.
   TODO: Add save/load state functionality.
   TODO: Add debug logging or a debug mode.

CPU:
   TODO: Implement all 6502 opcodes (currently only a few are implemented).
   TODO: Implement full addressing modes (immediate, zero-page, absolute, indirect, indexed, etc.).
   TODO: Implement IRQ and NMI interrupt handling.
   TODO: Implement stack operations (PHA, PLA, PHP, PLP, JSR, RTS, RTI).
   TODO: Implement decimal mode (BCD support).
   TODO: Implement memory-mapped I/O for PPU/APU and controller reads.
   TODO: Optimize opcode execution (e.g., using a lookup table).

INES:
   TODO: Support additional iNES header variations (e.g., iNES 2.0 format).
   TODO: Implement trainer data loading if present.
   TODO: Implement proper mapper selection and support.
   TODO: Implement error reporting for corrupted or unsupported ROMs.
   TODO: Support CHR RAM for games without CHR ROM.

PPU:
   TODO: Implement full PPU register interface (control, mask, status, OAM address, etc.).
   TODO: Implement pattern table fetching for background and sprites.
   TODO: Implement name table fetching and mirroring modes.
   TODO: Implement background rendering with correct palette application.
   TODO: Implement sprite rendering with priority and flipping.
   TODO: Implement attribute table handling.
   TODO: Implement vblank timing and NMI generation.
   TODO: Implement PAL timing differences if supporting PAL.
   TODO: Implement scroll handling and fine X/Y scrolling.
   TODO: Implement sprite 0 hit detection.


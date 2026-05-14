#ifndef ASSEMBLER_H
#define ASSEMBLER_H

#include <stdint.h>

#include "cpu_uapi.h"

// Parses the assembly file into an array of executable instructions.
// Populates instruction_text with raw string lines for userspace display.
instruction *parse_assembly(const char *fileName, size_t *num_instr, char ***instruction_text);

// Loads the compiled machine code directly into the simulated CPU memory array.
void store_machine_code(uint8_t *memory, size_t memory_size, instruction *instructions, size_t num_instructions);

#endif
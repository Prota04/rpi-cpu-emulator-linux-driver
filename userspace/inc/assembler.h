#ifndef ASSEMBLER_H
#define ASSEMBLER_H

#include <stdint.h>

#include "cpu_uapi.h"

instruction *parse_assembly(const char *fileName, size_t *num_instr, char ***instruction_text);
void store_machine_code(uint8_t *memory, size_t memory_size, instruction *instructions, size_t num_instructions);

#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "cpu_emulator_userspace.h"
#include "assembler.h"

extern uint8_t *memory;  // Simulated memory
extern size_t memory_size;  // Size of the simulated memory

int main(int argc, char *argv[])
{
    char path[128];
    strcpy(path, "./inc/assembly/");
    strcat(path, argv[1]);

    initialize_memory(0x3000);  // Initialize memory

    size_t num_instructions = 0;
    char **instruction_text = NULL;

    instruction *instructions = parse_assembly(path, &num_instructions, &instruction_text);  // Parse assembly code from file
    store_machine_code(memory, memory_size, instructions, num_instructions);  // Store the parsed instructions in memory

    run("cpu_emulator", instruction_text, num_instructions);

    for (int i = 0; i < num_instructions; i++)
    {
        //printf("%d - op1: %x, op2: %llx\n", i, instructions[i].operand1, instructions[i].operand2);
        free(instruction_text[i]);
    }
    free(instruction_text);

    return 0;
}
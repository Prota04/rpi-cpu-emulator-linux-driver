#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "cpu_emulator_userspace.h"
#include "assembler.h"
#include "cache_simulator.h"

extern uint8_t memory[];  // Simulated memory

int main(int argc, char *argv[])
{
    char path[128];
    strcpy(path, "./doc/");
    strcat(path, argv[1]);

    size_t num_instructions = 0;
    char **instruction_text = NULL;

    instruction *instructions = parse_assembly(path, &num_instructions, &instruction_text);  // Parse assembly code from file
    if (instructions == NULL)
    {
        return EXIT_FAILURE;
    }

    store_machine_code(memory, MEM_SIZE, instructions, num_instructions);  // Store the parsed instructions in memory

    if (run(instruction_text, num_instructions))
    {
        return EXIT_FAILURE;
    }

    for (int i = 0; i < num_instructions; i++)
    {
        free(instruction_text[i]);
    }
    free(instruction_text);

    free(instructions);

    run_cache_sim();

    return EXIT_SUCCESS;
}
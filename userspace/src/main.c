#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "cpu_emulator_userspace.h"
#include "assembler.h"
#include "cache_simulator.h"

// Simulated 64-bit address space memory used by the userspace emulator
extern uint8_t memory[];  

int main(int argc, char *argv[])
{
    // Construct the path to the assembly source file (assumes files are in the ./doc/ directory)
    char path[128];
    strcpy(path, "./doc/");
    strcat(path, argv[1]);

    size_t num_instructions = 0;
    char **instruction_text = NULL;

    // Phase 1: Parse the assembly file into structured machine code instructions and raw text lines
    instruction *instructions = parse_assembly(path, &num_instructions, &instruction_text);  
    if (instructions == NULL)
    {
        return EXIT_FAILURE;
    }

    // Phase 2: Load the parsed machine code instructions into the simulated memory array
    store_machine_code(memory, MEM_SIZE, instructions, num_instructions);  

    // Phase 3: Start the CPU emulator execution loop (communicates with the kernel driver)
    if (run(instruction_text, num_instructions))
    {
        return EXIT_FAILURE;
    }

    // Phase 4: Clean up dynamically allocated memory for the raw instruction strings
    for (int i = 0; i < num_instructions; i++)
    {
        free(instruction_text[i]);
    }
    free(instruction_text);
    free(instructions);

    // Phase 5: Run the cache simulator using the memory access trace generated during CPU execution
    run_cache_sim();

    return EXIT_SUCCESS;
}
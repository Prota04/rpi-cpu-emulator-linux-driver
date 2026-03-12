#ifndef CPU_EMULATOR_USERSPACE_H
#define CPU_EMULATOR_USERSPACE_H

#include <stdint.h>

#include "cpu_uapi.h"

void initialize_memory(size_t size);
int run(const char *chrdev, char **instruction_text, int count);

#endif
#ifndef CPU_EMULATOR_USERSPACE_H
#define CPU_EMULATOR_USERSPACE_H

#include <stdint.h>

#include "cpu_uapi.h"

#define MEM_SIZE 0x3000

int run(const char *chrdev, char **instruction_text, int count);

#endif
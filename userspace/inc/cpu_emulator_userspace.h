#ifndef CPU_EMULATOR_USERSPACE_H
#define CPU_EMULATOR_USERSPACE_H

#include <stdint.h>

#include "cpu_uapi.h"

#define MEM_SIZE 0x10000

int run(char **instruction_text, int count);

#endif
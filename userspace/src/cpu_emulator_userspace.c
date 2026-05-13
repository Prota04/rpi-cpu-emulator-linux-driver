#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#include "cpu_emulator_userspace.h"

uint8_t memory[MEM_SIZE] = {0};    // Simulated memory

static void read_from_memory(uint64_t address, uint64_t *return_value)
{
    *return_value = 0; // Clear before loading
    for (int i = 0; i < 8; i++)
    {
        *return_value |= ((uint64_t)memory[address + i]) << (i * 8);
    }
}

static void write_to_memory(uint64_t address, uint64_t value)
{
    for (int i = 0; i < 8; i++)
    {
        memory[address + i] = (value >> (i * 8)) & 0xFF;
    }
}

static instruction fetch_next(uint64_t PC)
{
    uint8_t opcode = *(memory + PC);       // Fetch opcode from memory
    uint8_t mode = *(memory + PC + 1);     // Fetch addressing mode from memory
    uint8_t operand1 = *(memory + PC + 2); // Fetch operand1 from memory
    uint64_t operand2;

    memcpy(&operand2, memory + PC + 3, sizeof(uint64_t)); // Fetch operand2 from memory

    return (instruction){operand2, operand1, opcode, mode};
}

int run(char **instruction_text, int count)
{
    int fd_cpu_emulator, fd_cache_sim;
    fd_cpu_emulator = open("/dev/cpu_emulator", O_RDWR);
    if (fd_cpu_emulator < 0)
    {
        perror("Character device 'cpu_emulator' nije pronadjen!\n");
        return fd_cpu_emulator;
    }

    fd_cache_sim = open("/dev/trace_collector", O_WRONLY);
    if (fd_cache_sim < 0)
    {
        perror("Character device 'trace_collector' nije pronadjen!\n");
        return fd_cache_sim;
    }

    write_value write_buf = {fetch_next(0), 0, 1};
    read_value read_buf = {0};

    mem_access access = {0}; //prva instrukcija na adresi 0x0000000000000000
    if (write(fd_cache_sim, &access, sizeof(mem_access)) < 0)
    {
        printf("Provjeri kernel logove, error code: %d\n", errno);
    }

    int i;
    while (1)
    {
        i = read_buf.pc / INSTR_SIZE;

        if (write(fd_cpu_emulator, &write_buf, sizeof(write_value)) < 0)
        {
            printf("Provjeri kernel logove, error code: %d\n", errno);
            break;
        }

        if (read(fd_cpu_emulator, &read_buf, sizeof(read_value)) < 0)
        {
            printf("Provjeri kernel logove, error code: %d\n", errno);
            break;
        }

        if (i >= count)
        {
            printf("Prevazidjen broj instrukcija! Program se gasi\n");
            break;
        }
        printf("Executed instruction %s\n", instruction_text[i]);

        switch (read_buf.exec_return)
        {
        case EXEC_SUCCESS:
            break;
        case EXEC_HALT:
            goto end;
        case EXEC_PENDING_MEM_READ:
            write_buf.is_instruction = 0;
            read_from_memory(read_buf.mem_addr, &write_buf.val);

            write(fd_cpu_emulator, &write_buf, sizeof(write_value));

            access.addr = read_buf.mem_addr;
            access.type = 0;
            write(fd_cache_sim, &access, sizeof(mem_access));

            write_buf.is_instruction = 1;

            break;
        case EXEC_PENDING_MEM_WRITE:
            write_to_memory(read_buf.mem_addr, read_buf.val);

            access.addr = read_buf.mem_addr;
            access.type = 1;
            write(fd_cache_sim, &access, sizeof(mem_access));

            break;
        case EXEC_PENDING_MMIO_READ:
            printf("Input button is %spressed\n", read_buf.val == 1 ? "" : "not ");

            break;
        default:
            printf("Nevalidan exec kod!\n");
            return -1;
        }

        write_buf.instruction = fetch_next(read_buf.pc);
        access.addr = read_buf.pc;
        access.type = 0;
        if (write(fd_cache_sim, &access, sizeof(mem_access)) < 0)
        {
            printf("Provjeri kernel logove, error code: %d\n", errno);
        }
    }

end:
    close(fd_cpu_emulator);
    close(fd_cache_sim);
    return 0; // Successfully ran
}
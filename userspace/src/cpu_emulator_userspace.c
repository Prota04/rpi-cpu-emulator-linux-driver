#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#include "cpu_emulator_userspace.h"

uint8_t *memory;    // Simulated memory
size_t memory_size; // Size of the simulated memory

void initialize_memory(size_t size)
{
    memory = (uint8_t *)calloc(size, sizeof(uint8_t));
    if (memory == NULL)
    {
        // Handle memory allocation failure
        exit(EXIT_FAILURE);
    }
    memory_size = size;
}

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

int run(const char *chrdev, char **instruction_text, int count)
{
    char path[64];

    strcpy(path, "/dev/");
    strcat(path, chrdev);

    int fd;
    fd = open(path, O_RDWR);
    if (fd < 0)
    {
        perror("Character device nije pronadjen!\n");
        return fd;
    }

    write_value write_buf = {fetch_next(0), 0, 1};
    read_value read_buf = {0};
    int i;
    while (1)
    {
        i = read_buf.pc / INSTR_SIZE;

        if (write(fd, &write_buf, sizeof(write_value)) < 0)
        {
            printf("Provjeri kernel logove, error code: %d\n", errno);
            break;
        }

        if (read(fd, &read_buf, sizeof(read_value)) < 0)
        {
            printf("Provjeri kernel logove, error code: %d\n", errno);
            break;
        }

        if (i > count)
        {
            printf("Prevazidjen broj instrukcija! Program se gasi\n");
            break;
        }
        printf("Executed instruction %s\n", instruction_text[i]);

        switch (read_buf.exec_return)
        {
        case EXEC_SUCCESS:
            write_buf.instruction = fetch_next(read_buf.pc);

            break;
        case EXEC_HALT:
            goto end;
        case EXEC_PENDING_MEM_READ:
            write_buf.is_instruction = 0;
            read_from_memory(read_buf.mem_addr, &write_buf.val);

            write(fd, &write_buf, sizeof(write_value));

            write_buf.instruction = fetch_next(read_buf.pc);

            break;
        case EXEC_PENDING_MEM_WRITE:
            write_to_memory(read_buf.mem_addr, read_buf.val);

            write_buf.instruction = fetch_next(read_buf.pc);

            break;
        case EXEC_PENDING_MMIO_READ:
            printf("Input button is %spressed\n", read_buf.val == 1 ? "" : "not ");

            write_buf.instruction = fetch_next(read_buf.pc);

            break;
        default:
            printf("Nevalidan exec kod!\n");
            return -1;
        }
    }

end:
    close(fd);
    return 0; // Successfully ran
}
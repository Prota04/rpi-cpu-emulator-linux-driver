#ifndef _UAPI_CPU_INTERFACE_H
#define _UAPI_CPU_INTERFACE_H

#include <linux/types.h>

// Opcodes
#define ADD 0x00
#define SUB 0x01
#define AND 0x02
#define OR 0x03
#define NOT 0x04

#define MOV 0x10

#define CMP 0x20
#define JMP 0x21
#define JE 0x22
#define JG 0x23
#define JL 0x24

// Register definitions
#define REG_0 0x00
#define REG_1 0x01
#define REG_2 0x02
#define REG_3 0x03

// Addressing modes
#define IMMEDIATE 0x00
#define REGISTER 0x01
#define DIRECT_LOAD 0x02
#define DIRECT_STORE 0x03
#define LABEL 0x04

// Flag definitions
#define CF 0x01  // Carry Flag
#define AF 0x10  // Auxiliary Carry Flag
#define ZF 0x40  // Zero Flag
#define SF 0x80  // Sign Flag
#define OF 0x800  // Overflow Flag

#define INSTR_SIZE 11  // 1 byte for opcode, 1 byte for mode, 1 byte for operand1, and 8 bytes for operand2

// Execution results
#define EXEC_SUCCESS 0
#define EXEC_HALT -1
#define EXEC_PENDING_MEM_READ 1
#define EXEC_PENDING_MEM_WRITE 2
#define EXEC_PENDING_MMIO_READ 3

// HALT address
#define MMIO_HALT 0xFFFF

typedef struct instruction
{
    __u64 operand2;
    __u8 operand1;
    __u8 opcode;
    __u8 mode;

    __u8 padding[5];
} instruction;

typedef struct write_value
{
    instruction instruction;
    __u64 val;
    __u8 is_instruction;

    __u8 padding[7];
} write_value;

typedef struct read_value
{
    __u64 pc;
    __u64 mem_addr;
    __u64 val;
    __s32 exec_return;

    __u8 padding[4];
} read_value;

#endif

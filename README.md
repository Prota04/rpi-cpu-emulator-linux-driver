# Raspberry Pi CPU Emulator Linux Driver

## Overview

This project implements a **simple custom CPU emulator inside a Linux kernel driver** designed to run on a Raspberry Pi. The emulator executes instructions stored in memory and interacts with the physical world using **GPIO pins for input/output devices and status LEDs**.

The system demonstrates concepts such as:

* Linux kernel module development
* Character device drivers
* Memory-mapped I/O
* GPIO control from kernel space
* Simple CPU architecture emulation
* Userspace ↔ kernel communication

The CPU executes a minimal instruction set and supports branching, arithmetic operations, and device I/O through MMIO addresses.

---

# Architecture

The system consists of two main parts:

```
Projekat
│
├── cpu_driver
│   ├── cpu_emulator_driver.c   # Linux kernel CPU emulator
│   └── Makefile
│
├── userspace
│   └── Makefile                # Userspace utilities (future expansion)
│
├── cpu_uapi.h                  # Shared CPU definitions (instructions, registers)
├── run.sh                      # Script to load and run the module
└── todo.txt                    # Future improvements
```

---

# CPU Design

## Registers

The CPU contains:

| Register | Description              |
| -------- | ------------------------ |
| R0       | General purpose register |
| R1       | General purpose register |
| R2       | General purpose register |
| R3       | General purpose register |
| PC       | Program Counter          |
| FR       | Flags register           |

---

## Flags

| Flag | Meaning         |
| ---- | --------------- |
| CF   | Carry Flag      |
| AF   | Auxiliary Carry |
| ZF   | Zero Flag       |
| SF   | Sign Flag       |
| OF   | Overflow Flag   |

---

## Instruction Format

Each instruction occupies **11 bytes** in memory:

```
| opcode | mode | operand1 | operand2 (8 bytes) |
```

Defined in:

```
cpu_uapi.h
```

---

# Instruction Set

## Arithmetic

| Instruction | Description     |
| ----------- | --------------- |
| ADD         | Add values      |
| SUB         | Subtract values |
| AND         | Bitwise AND     |
| OR          | Bitwise OR      |
| NOT         | Bitwise NOT     |

---

## Data movement

| Instruction | Description                        |
| ----------- | ---------------------------------- |
| MOV         | Move data between registers/memory |

---

## Control flow

| Instruction | Description        |
| ----------- | ------------------ |
| CMP         | Compare values     |
| JMP         | Unconditional jump |
| JE          | Jump if equal      |
| JG          | Jump if greater    |
| JL          | Jump if lower      |

---

# Addressing Modes

| Mode         | Meaning          |
| ------------ | ---------------- |
| IMMEDIATE    | Immediate value  |
| REGISTER     | Register operand |
| DIRECT_LOAD  | Load from memory |
| DIRECT_STORE | Store to memory  |
| LABEL        | Label address    |

---

# Memory Mapped I/O

The CPU supports basic MMIO for hardware interaction.

| Address  | Device        |
| -------- | ------------- |
| `0xFFFD` | Input device  |
| `0xFFFE` | Output device |
| `0xFFF0` | MMIO start    |

---

# Hardware Interface

The driver uses **Raspberry Pi GPIO pins**:

| GPIO | Function      |
| ---- | ------------- |
| 17   | LED 1         |
| 27   | LED 2         |
| 22   | LED 3         |
| 26   | LED 4         |
| 23   | RUN switch    |
| 24   | PAUSE switch  |
| 25   | Input device  |
| 16   | Output device |

LEDs visualize CPU activity such as register updates or execution status.

---

# Building the Kernel Module

Navigate to the driver directory:

```bash
cd cpu_driver
make
```

This will build the kernel module.

---

# Loading the Module

Insert the module:

```bash
sudo insmod cpu_emulator_driver.ko
```

Check kernel logs:

```bash
dmesg
```

Remove the module:

```bash
sudo rmmod cpu_emulator_driver
```

---

# Running the Emulator

A helper script is provided:

```bash
./run.sh
```

This script typically:

1. Loads the kernel module
2. Initializes the emulator
3. Starts execution when the RUN switch is pressed

---

# Execution Model

The CPU waits for the **RUN switch** to start execution.

```
RUN switch pressed
        ↓
Fetch instruction
        ↓
Decode instruction
        ↓
Execute instruction
        ↓
Update registers / memory / MMIO
        ↓
Repeat
```

The **PAUSE switch** temporarily halts execution.

which would make this project **look very strong on GitHub (especially for OS/embedded internships)**.

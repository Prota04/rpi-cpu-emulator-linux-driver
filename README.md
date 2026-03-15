# Raspberry Pi CPU Emulator Driver

A Linux kernel project that emulates a small custom CPU on a Raspberry Pi and exposes it through a character device, with a companion userspace assembler and runner.

The project combines **kernel development**, **embedded I/O**, and **instruction set emulation** in one system. Programs are assembled in userspace, sent to the kernel driver instruction by instruction, and executed with support for memory access, branching, GPIO-backed input/output, and simple execution control through hardware switches.

---

## Highlights

- **Linux kernel module** implementing a custom CPU execution engine
- **Character device interface** for communication between userspace and kernelspace
- **Userspace assembler** that parses custom assembly into machine instructions
- **Userspace execution driver** that coordinates instruction fetch, memory access, and MMIO requests
- **GPIO integration** for LEDs, run/pause control, and simple input/output devices
- **Configurable clock speed** through a kernel module parameter

---

## System Overview

The project is split into three main layers:

1. **Assembler (userspace)**  
   Parses assembly source and converts it into the project’s machine instruction format.

2. **Execution client (userspace)**  
   Loads instructions into simulated memory, communicates with `/dev/cpu_emulator`, and services memory/MMIO requests returned by the kernel driver.

3. **CPU emulator (kernel module)**  
   Executes instructions, updates CPU state, handles GPIO-based control flow, and exposes the device interface.

This gives the project a clean separation of responsibilities:

- parsing and program preparation in userspace
- execution state and hardware interaction in kernelspace

---

## Repository Structure

```text
.
├── cpu_driver/
│   ├── cpu_emulator_driver.c    # Kernel module implementing the CPU
│   └── Makefile                 # Kernel module build rules
├── userspace/
│   ├── Makefile                 # Userspace build rules
│   └── src/
│       ├── assembler.c          # Custom assembler
│       ├── cpu_emulator_userspace.c
│       └── main.c               # Userspace entry point
├── cpu_uapi.h                   # Shared user/kernel interface definitions
├── run.sh                       # Convenience script for loading and running
└── todo.txt                     # Planned improvements
```

---

## CPU Model

The emulator currently implements a compact CPU with:

- **4 general-purpose registers**: `R0` to `R3`
- **Program counter**: `PC`
- **Flags register**: `FR`

### Supported flags

- `CF` — Carry Flag
- `AF` — Auxiliary Carry Flag
- `ZF` — Zero Flag
- `SF` — Sign Flag
- `OF` — Overflow Flag

---

## Instruction Format

Each instruction occupies **11 bytes**.

```text
| opcode (1) | mode (1) | operand1 (1) | operand2 (8) |
```

The shared layout is defined in `cpu_uapi.h`.

---

## Supported Instruction Set

### Arithmetic and logic

- `ADD`
- `SUB`
- `AND`
- `OR`
- `NOT`

### Data movement

- `MOV`

### Comparison and branching

- `CMP`
- `JMP`
- `JE`
- `JG`
- `JL`

---

## Addressing Modes

The current interface defines these modes:

- `IMMEDIATE`
- `REGISTER`
- `DIRECT_LOAD`
- `DIRECT_STORE`
- `LABEL`

These are shared between the assembler, userspace runtime, and kernel driver through `cpu_uapi.h`.

---

## Execution Flow

At a high level, execution works like this:

1. An assembly program is parsed in userspace.
2. Instructions are stored in simulated memory.
3. The userspace runner opens `/dev/cpu_emulator`.
4. The next instruction is sent to the kernel driver.
5. The kernel executes it and returns one of the following:
   - success
   - halt
   - pending memory read
   - pending memory write
   - pending MMIO read
6. Userspace services the request when needed and continues execution.

This handshake makes the driver responsible for CPU semantics while keeping the backing memory model simple and easy to inspect in userspace.

---

## GPIO Integration

The kernel module is wired to Raspberry Pi GPIO pins for simple physical interaction.

### LEDs

- `GPIO17` → LED 1
- `GPIO27` → LED 2
- `GPIO22` → LED 3
- `GPIO26` → LED 4

The LEDs are used to reflect register state changes during execution.

### Switches and I/O

- `GPIO23` → Run switch
- `GPIO24` → Pause switch
- `GPIO25` → Input device / step signal
- `GPIO16` → Output device

### MMIO addresses

- `0xFFF0` → MMIO region start
- `0xFFFD` → MMIO input
- `0xFFFE` → MMIO output
- `0xFFFF` → Halt marker

---

## Build Instructions

### Kernel module

From the driver directory:

```bash
cd cpu_driver
make
```

### Userspace tools

From the userspace directory:

```bash
cd userspace
make
```

---

## Running the Project

A helper script is included:

```bash
./run.sh [clock_speed_ms] <assembly_file>
```

### Example

```bash
./run.sh 200 demo.asm
```

What the script does:

1. unloads any previously loaded `cpu_emulator_driver`
2. loads the new kernel module
3. optionally sets `clock_speed`
4. launches the userspace runner

If no clock speed is provided, the module uses the default value of **500 ms**.

---

## Character Device Interface

The project uses a character device named:

```text
/dev/cpu_emulator
```

The interface between userspace and kernelspace is defined in `cpu_uapi.h` through shared structures such as:

- `instruction`
- `write_value`
- `read_value`

This shared UAPI header keeps both sides synchronized and makes the project easier to evolve.

---

## Why This Project Is Interesting

This is not just a simple emulator. It is a compact demonstration of several important systems concepts working together:

- building a **real Linux kernel module**
- exposing functionality through a **device file**
- designing a **custom ISA**
- coordinating **userspace and kernelspace responsibilities**
- using **GPIO-backed hardware control** on Raspberry Pi
- handling **execution timing, pause, and stepping** in a low-level system

It is a strong educational project for courses in:

- operating systems
- embedded systems
- computer architecture
- device driver development

---

## Current Limitations

Based on the current codebase, this project is still a work in progress. A few areas that can be improved further are:

- richer instruction set
- stronger input validation and error reporting
- easier program loading workflow
- support for more advanced MMIO devices
- improved debugging and tracing tools
- more example assembly programs and documentation

The current `todo.txt` also notes a planned improvement for writing hexadecimal values into registers.

---

## Development Notes

A few implementation details worth noting:

- the emulator uses a **userspace-backed memory model**
- memory reads and writes are coordinated through explicit execution return codes
- execution can be **started**, **paused**, and **single-stepped** using GPIO signals
- the module exposes a `clock_speed` parameter to control instruction pacing

---

## Example Resume Description

If you want to describe this project briefly on a CV or portfolio, this version works well:

> Built a custom CPU emulator as a Linux kernel character driver for Raspberry Pi, with a userspace assembler/runtime, GPIO-backed I/O, MMIO support, and hardware-controlled run/pause/step execution.

---

## Future Improvements

Good next steps for this repository would be:

- add sample assembly programs
- document the assembly syntax formally
- add a memory map diagram
- add execution screenshots or hardware photos
- extend the ISA with load/store and debugging instructions
- add automated tests for assembler parsing and execution behavior

---

## License

This repository appears to be an academic / educational project. Add a formal license here if you plan to publish or share it more broadly.

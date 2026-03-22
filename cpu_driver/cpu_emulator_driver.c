#include <linux/init.h>
#include <linux/module.h>
#include <linux/gpio/consumer.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/cdev.h>
#include <linux/interrupt.h>

#include "cpu_uapi.h"

// GPIO pins
#define LED_1 17
#define LED_2 27
#define LED_3 22
#define LED_4 26

#define RUN_SWITCH 23
#define PAUSE_SWITCH 24

#define INPUT_DEVICE 25
#define OUTPUT_DEVICE 16

#define IO_OFFSET 512

#define NUM_LEDS 4

// MMIO addresses
#define MMIO_START 0xFFF0
#define MMIO_INPUT 0xFFFD
#define MMIO_OUTPUT 0xFFFE

typedef struct my_cpu
{
    u64 regs[4]; // General Purpose Registers
    u64 pc;      // Program Counter
    u16 fr;      // Flags register

    // Internal helper variables
    u64 pending_mem_addr;
    u8 pending_reg_idx;

    u8 pending_mmio_read;
} my_cpu;

typedef struct cpu_context
{
    my_cpu cpu;
    instruction pending_instruction;
} cpu_context;

static int led_gpio[NUM_LEDS] = {LED_1, LED_2, LED_3, LED_4};

static struct gpio_desc *leds[NUM_LEDS];
static struct gpio_desc *output_dev;
static struct gpio_desc *run_sw;
static struct gpio_desc *pause_sw;
static struct gpio_desc *input_dev;

static int run_irq;
static int pause_irq;
static int step_irq;

static atomic_t cpu_running = ATOMIC_INIT(0);
static atomic_t cpu_paused = ATOMIC_INIT(0);
static atomic_t step_count = ATOMIC_INIT(0);

static DECLARE_WAIT_QUEUE_HEAD(exec_wq);

static dev_t dev_num;
static struct cdev cpu_cdev;
static struct class *cpu_class;

static unsigned int clock_speed = 500;
module_param(clock_speed, uint, 0444);

static irqreturn_t run_irq_thread(int irq, void *data)
{
    if (gpiod_get_value_cansleep(run_sw))
    {
        atomic_set(&cpu_running, 1);
        wake_up_interruptible(&exec_wq);
    }

    return IRQ_HANDLED;
}

static irqreturn_t pause_irq_thread(int irq, void *data)
{
    if (gpiod_get_value_cansleep(pause_sw))
    {
        atomic_set(&cpu_paused, 1);
    }
    else
    {
        atomic_set(&cpu_paused, 0);
        wake_up_interruptible(&exec_wq);
    }

    return IRQ_HANDLED;
}

static irqreturn_t step_irq_thread(int irq, void *data)
{
    if (!atomic_read(&cpu_paused))
        return IRQ_HANDLED;

    msleep(50);

    if (gpiod_get_value_cansleep(input_dev)) {
        atomic_inc(&step_count);
        wake_up_interruptible(&exec_wq);
    }
    return IRQ_HANDLED;
}

static void pause(void)
{
    if (clock_speed < 20)
    {
        unsigned int adjusted_clock = clock_speed * 1000;
        unsigned int max = adjusted_clock + adjusted_clock / 50;
        usleep_range(adjusted_clock, max);
    }
    else
    {
        msleep(clock_speed);
    }
}

static u64 *get_register(my_cpu *cpu, u64 reg)
{
    switch (reg)
    {
    case REG_0:
        return &cpu->regs[0];
    case REG_1:
        return &cpu->regs[1];
    case REG_2:
        return &cpu->regs[2];
    case REG_3:
        return &cpu->regs[3];

    default:
        pr_err("Invalid register\n");
        return NULL; // error
    }
}

static void handle_compare(my_cpu *cpu, s64 result)
{
    // Set or clear Zero Flag
    if (result == 0)
    {
        cpu->fr |= ZF; // Set Zero Flag
    }
    else
    {
        cpu->fr &= ~ZF; // Clear Zero Flag
    }

    if (result < 0)
    {
        cpu->fr |= SF; // Set Sign Flag
        cpu->fr |= CF; // Set Carry Flag
    }
    else
    {
        cpu->fr &= ~SF; // Clear Sign Flag
        cpu->fr &= ~CF; // Clear Carry Flag
    }
}
static void handle_compare_immediate(my_cpu *cpu, u64 *reg1, u64 operand)
{
    s64 result = (s64)*reg1 - (s64)operand;
    handle_compare(cpu, result);
}
static void handle_compare_register(my_cpu *cpu, u64 *reg1, u64 *reg2)
{
    s64 result = (s64)*reg1 - (s64)*reg2;
    handle_compare(cpu, result);
}

static void set_diodes(my_cpu current_state, my_cpu prev_state)
{
    for (int i = 0; i < 4; i++)
    {
        if (current_state.regs[i] == prev_state.regs[i])
        {
            gpiod_set_value(leds[i], 0);
        }
        else
        {
            gpiod_set_value(leds[i], 1);
        }
    }
}

static int is_mmio(u64 addr)
{
    return addr >= MMIO_START;
}
static int is_mmio_input(u64 addr)
{
    return addr == MMIO_INPUT;
}
static int is_mmio_output(u64 addr)
{
    return addr == MMIO_OUTPUT;
}

static s32 execute_instruction(my_cpu *cpu, instruction instruction)
{
    my_cpu prev_state = *cpu;

    switch (instruction.opcode)
    {
    case ADD:
        if (instruction.mode == IMMEDIATE)
        {
            u64 *reg = get_register(cpu, instruction.operand1);

            if (reg == NULL)
            {
                set_diodes(*cpu, prev_state);
                return -EINVAL; // Error
            }

            *reg += instruction.operand2;
        }
        else if (instruction.mode == REGISTER)
        {
            u64 *reg1 = get_register(cpu, instruction.operand1);
            u64 *reg2 = get_register(cpu, instruction.operand2);

            if (reg1 == NULL || reg2 == NULL)
            {
                set_diodes(*cpu, prev_state);
                return -EINVAL; // Error
            }

            *reg1 += *reg2;
        }
        else
        {
            set_diodes(*cpu, prev_state);
            return -EINVAL; // Error
        }

        break;
    case SUB:
        if (instruction.mode == IMMEDIATE)
        {
            u64 *reg = get_register(cpu, instruction.operand1);

            if (reg == NULL)
            {
                set_diodes(*cpu, prev_state);
                return -EINVAL; // Error
            }

            *reg -= instruction.operand2;
        }
        else if (instruction.mode == REGISTER)
        {
            u64 *reg1 = get_register(cpu, instruction.operand1);
            u64 *reg2 = get_register(cpu, instruction.operand2);

            if (reg1 == NULL || reg2 == NULL)
            {
                set_diodes(*cpu, prev_state);
                return -EINVAL; // Error
            }

            *reg1 -= *reg2;
        }
        else
        {
            set_diodes(*cpu, prev_state);
            return -EINVAL; // Error
        }

        break;
    case AND:
        if (instruction.mode == IMMEDIATE)
        {
            u64 *reg = get_register(cpu, instruction.operand1);

            if (reg == NULL)
            {
                set_diodes(*cpu, prev_state);
                return -EINVAL; // Error
            }

            *reg &= instruction.operand2;
        }
        else if (instruction.mode == REGISTER)
        {
            u64 *reg1 = get_register(cpu, instruction.operand1);
            u64 *reg2 = get_register(cpu, instruction.operand2);

            if (reg1 == NULL || reg2 == NULL)
            {
                set_diodes(*cpu, prev_state);
                return -EINVAL; // Error
            }

            *reg1 &= *reg2;
        }
        else
        {
            set_diodes(*cpu, prev_state);
            return -EINVAL; // Error
        }

        break;
    case OR:
        if (instruction.mode == IMMEDIATE)
        {
            u64 *reg = get_register(cpu, instruction.operand1);

            if (reg == NULL)
            {
                set_diodes(*cpu, prev_state);
                return -EINVAL; // Error
            }

            *reg |= instruction.operand2;
        }
        else if (instruction.mode == REGISTER)
        {
            u64 *reg1 = get_register(cpu, instruction.operand1);
            u64 *reg2 = get_register(cpu, instruction.operand2);

            if (reg1 == NULL || reg2 == NULL)
            {
                set_diodes(*cpu, prev_state);
                return -EINVAL; // Error
            }

            *reg1 |= *reg2;
        }
        else
        {
            set_diodes(*cpu, prev_state);
            return -EINVAL; // Error
        }

        break;
    case NOT:
        if (instruction.mode != REGISTER)
        {
            set_diodes(*cpu, prev_state);
            return -EINVAL;
        }

        u64 *reg = get_register(cpu, instruction.operand2);

        if (reg == NULL)
        {
            set_diodes(*cpu, prev_state);
            return -EINVAL;
        }

        *reg = ~(*reg);

        break;
    case MOV:
        if (instruction.mode == IMMEDIATE)
        {
            u64 *reg = get_register(cpu, instruction.operand1);

            if (reg == NULL)
            {
                set_diodes(*cpu, prev_state);
                return -EINVAL; // Error
            }

            *reg = instruction.operand2;
        }
        else if (instruction.mode == REGISTER)
        {
            u64 *reg1 = get_register(cpu, instruction.operand1);
            u64 *reg2 = get_register(cpu, instruction.operand2);

            if (reg1 == NULL || reg2 == NULL)
            {
                set_diodes(*cpu, prev_state);
                return -EINVAL; // Error
            }

            *reg1 = *reg2;
        }
        else if (instruction.mode == DIRECT_LOAD)
        {
            if (is_mmio(instruction.operand2))
            {
                if (is_mmio_input(instruction.operand2))
                {
                    u64 *reg = get_register(cpu, instruction.operand1);
                    if (!reg)
                    {
                        set_diodes(*cpu, prev_state);
                        return -EINVAL; // Error
                    }

                    cpu->pending_mmio_read = gpiod_get_value(input_dev);
                    *reg = cpu->pending_mmio_read;

                    set_diodes(*cpu, prev_state);
                    if (cpu->pending_mmio_read < 0)
                    {
                        pr_err("cpu_cdev - An error occured while reading output device value\n");
                        return cpu->pending_mmio_read;
                    }

                    cpu->pc += INSTR_SIZE;

                    return EXEC_PENDING_MMIO_READ;
                }
                else
                {
                    pr_warn("cpu_cdev - Invalid address for mmio input!\n");
                    set_diodes(*cpu, prev_state);
                    return -EINVAL;
                }
            }
            else
            {
                cpu->pending_mem_addr = instruction.operand2;
                cpu->pending_reg_idx = instruction.operand1;

                cpu->pc += INSTR_SIZE;

                set_diodes(*cpu, prev_state);
                return EXEC_PENDING_MEM_READ;
            }
        }
        else if (instruction.mode == DIRECT_STORE)
        {
            if (is_mmio(instruction.operand2))
            {
                if (instruction.operand2 == MMIO_HALT)
                {
                    set_diodes(*cpu, prev_state);

                    return EXEC_HALT;
                }
                else if (is_mmio_output(instruction.operand2))
                {
                    u64 *reg = get_register(cpu, instruction.operand1);
                    if (!reg)
                    {
                        set_diodes(*cpu, prev_state);
                        return -EINVAL;
                    }

                    if (*reg == 0)
                    {
                        gpiod_set_value(output_dev, 0);
                        goto end;
                    }
                    else if (*reg == 1)
                    {
                        gpiod_set_value(output_dev, 1);
                        goto end;
                    }
                    else
                    {
                        pr_warn("cpu_cdev - Invalid value for mmio output!\n");
                        set_diodes(*cpu, prev_state);
                        return -EINVAL;
                    }
                }
                else
                {
                    pr_warn("cpu_cdev - Invalid address for mmio output!\n");
                    set_diodes(*cpu, prev_state);
                    return -EINVAL;
                }
            }
            else
            {
                cpu->pending_mem_addr = instruction.operand2;
                cpu->pending_reg_idx = instruction.operand1;

                cpu->pc += INSTR_SIZE;

                set_diodes(*cpu, prev_state);
                return EXEC_PENDING_MEM_WRITE;
            }
        }
        else
        {
            set_diodes(*cpu, prev_state);
            return -EINVAL; // Error
        }

        break;

    case CMP:
        if (instruction.mode == IMMEDIATE)
        {
            u64 *reg = get_register(cpu, instruction.operand1);

            if (reg == NULL)
            {
                set_diodes(*cpu, prev_state);
                return -EINVAL; // Error
            }

            handle_compare_immediate(cpu, reg, instruction.operand2);
        }
        else if (instruction.mode == REGISTER)
        {
            u64 *reg1 = get_register(cpu, instruction.operand1);
            u64 *reg2 = get_register(cpu, instruction.operand2);

            if (reg1 == NULL || reg2 == NULL)
            {
                set_diodes(*cpu, prev_state);
                return -EINVAL; // Error
            }

            handle_compare_register(cpu, reg1, reg2);
        }
        else
        {
            set_diodes(*cpu, prev_state);
            return -EINVAL; // Error
        }

        break;
    case JMP:
        cpu->pc = instruction.operand2 * INSTR_SIZE; // Set PC to the address of the label

        set_diodes(*cpu, prev_state);
        return EXEC_SUCCESS;
    case JE:
        // If Zero Flag is set
        if (cpu->fr & ZF)
        {
            cpu->pc = instruction.operand2 * INSTR_SIZE; // Jump to the label

            set_diodes(*cpu, prev_state);
            return EXEC_SUCCESS;
        }

        break;
    case JG:
        // If Sign Flag is clear and Zero Flag is clear
        if ((cpu->fr & SF) == 0 && (cpu->fr & ZF) == 0)
        {
            cpu->pc = instruction.operand2 * INSTR_SIZE; // Jump to the label

            set_diodes(*cpu, prev_state);
            return EXEC_SUCCESS;
        }

        break;
    case JL:
        // If Sign Flag is set
        if (cpu->fr & SF)
        {
            cpu->pc = instruction.operand2 * INSTR_SIZE; // Jump to the label

            set_diodes(*cpu, prev_state);
            return EXEC_SUCCESS;
        }

        break;
    default:
        set_diodes(*cpu, prev_state);
        return -EINVAL; // Error
    }

end:
    set_diodes(*cpu, prev_state);
    cpu->pc += INSTR_SIZE; // Move to the next instruction
    return EXEC_SUCCESS;
}

static int cpu_open(struct inode *inode, struct file *fp)
{
    int val = gpiod_get_value(run_sw);
    if (val < 0)
    {
        pr_err("cpu_cdev - Error reading run switch start value\n");
        return val;
    }
    else if (val == 0)
    {
        pr_info("cpu_cdev - Toggle run switch to start program execution\n");
    }
    atomic_set(&cpu_running, val);

    val = gpiod_get_value(pause_sw);
    if (val < 0)
    {
        pr_err("cpu_cdev - Error reading pause switch start value\n");
    }
    else if (val == 1)
    {
        pr_info("cpu_cdev - Toggle pause switch to start program execution\n");
    }
    atomic_set(&cpu_paused, val);

    cpu_context *ctx;

    ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
    if (!ctx)
    {
        return -ENOMEM;
    }

    fp->private_data = ctx;

    return 0;
}

static int cpu_release(struct inode *inode, struct file *file)
{
    kfree(file->private_data);
    return 0;
}

static ssize_t cpu_write(struct file *fp, const char __user *user_buf, size_t len, loff_t *off)
{
    if (len != sizeof(write_value))
    {
        pr_err("Velicina strukture mora biti %d bajtova, a proslijedjeno je %d bajtova\n", sizeof(write_value), len);
        return -EINVAL;
    }

    cpu_context *ctx = fp->private_data;

    my_cpu *cpu = &ctx->cpu;

    write_value wv;
    if (copy_from_user(&wv, user_buf, sizeof(write_value)))
    {
        pr_err("Error copying instruction into kernel space\n");
        return -EFAULT;
    }

    if (wv.is_instruction)
    {
        ctx->pending_instruction = wv.instruction;
    }
    else
    {
        my_cpu prev_state = *cpu;

        u64 *reg = get_register(cpu, cpu->pending_reg_idx);
        *reg = wv.val;

        set_diodes(*cpu, prev_state);
    }

    return len;
}

static ssize_t cpu_read(struct file *fp, char __user *user_buf, size_t len, loff_t *off)
{
    if (len != sizeof(read_value))
    {
        pr_err("Velicina strukture mora biti %d bajtova, a proslijedjeno je %d bajtova\n", sizeof(read_value), len);
        return -EINVAL;
    }

    cpu_context *ctx = fp->private_data;

    my_cpu *cpu = &ctx->cpu;

    pr_info("r0: %llu, r1: %llu, r2: %llu, r3: %llu\n", cpu->regs[0], cpu->regs[1], cpu->regs[2], cpu->regs[3]);
    pr_info("pc: %llu\n", cpu->pc);
    pr_info("----------------------------\n");

    int ret;

    ret = wait_event_interruptible(exec_wq, atomic_read(&cpu_running) && (!atomic_read(&cpu_paused) || atomic_read(&step_count) > 0));
    if (ret)
    {
        return -ERESTARTSYS;
    }

    if (atomic_read(&step_count) > 0)
    {
        /* We got through because of a step press — consume token, skip pause */
        atomic_dec(&step_count);
    }
    else
    {
        /* We got through because we're running normally — apply tick delay */
        pause();
    }

    s32 exec_res = execute_instruction(cpu, ctx->pending_instruction);
    read_value read_buf = {
        .pc = cpu->pc,
        .exec_return = exec_res};
    if (exec_res == -EINVAL)
    {
        return -EINVAL;
    }
    else if (exec_res == EXEC_PENDING_MEM_READ)
    {
        read_buf.mem_addr = cpu->pending_mem_addr;
    }
    else if (exec_res == EXEC_PENDING_MEM_WRITE)
    {
        read_buf.mem_addr = cpu->pending_mem_addr;
        u64 *reg = get_register(cpu, cpu->pending_reg_idx);
        read_buf.val = *reg;
    }
    else if (exec_res == EXEC_PENDING_MMIO_READ)
    {
        read_buf.val = cpu->pending_mmio_read;
    }

    if (copy_to_user(user_buf, &read_buf, len))
    {
        pr_err("Error sending result to userspace\n");
        return -EFAULT;
    }

    return len;
}

static struct file_operations fops = {
    .open = cpu_open,
    .release = cpu_release,
    .write = cpu_write,
    .read = cpu_read};

static int __init gpio_init(void)
{
    int status;

    for (int i = 0; i < NUM_LEDS; i++)
    {
        leds[i] = gpio_to_desc(led_gpio[i] + IO_OFFSET);
        if (!leds[i])
        {
            pr_err("gpioctrl - Error getting pin %d\n", led_gpio[i]);
            return -ENODEV;
        }
    }

    output_dev = gpio_to_desc(OUTPUT_DEVICE + IO_OFFSET);
    if (!output_dev)
    {
        pr_err("gpioctrl - Error getting pin %d\n", OUTPUT_DEVICE);
        return -ENODEV;
    }

    run_sw = gpio_to_desc(RUN_SWITCH + IO_OFFSET);
    if (!run_sw)
    {
        pr_err("gpioctrl - Error getting pin %d\n", RUN_SWITCH);
        return -ENODEV;
    }

    pause_sw = gpio_to_desc(PAUSE_SWITCH + IO_OFFSET);
    if (!pause_sw)
    {
        pr_err("gpioctrl - Error getting pin %d\n", PAUSE_SWITCH);
        return -ENODEV;
    }

    input_dev = gpio_to_desc(INPUT_DEVICE + IO_OFFSET);
    if (!input_dev)
    {
        pr_err("gpioctrl - Error getting pin %d\n", INPUT_DEVICE);
        return -ENODEV;
    }

    for (int i = 0; i < NUM_LEDS; i++)
    {
        status = gpiod_direction_output(leds[i], 0);
        if (status)
        {
            pr_err("gpioctrl - Error getting pin %d to output\n", led_gpio[i]);
            return status;
        }
    }

    status = gpiod_direction_output(output_dev, 0);
    if (status)
    {
        pr_err("gpioctrl - Error getting pin %d to output\n", OUTPUT_DEVICE);
        return status;
    }

    status = gpiod_direction_input(run_sw);
    if (status)
    {
        pr_err("gpioctrl - Error getting pin %d to input\n", RUN_SWITCH);
        return status;
    }

    status = gpiod_direction_input(pause_sw);
    if (status)
    {
        pr_err("gpioctrl - Error getting pin %d to input\n", PAUSE_SWITCH);
        return status;
    }

    status = gpiod_direction_input(input_dev);
    if (status)
    {
        pr_err("gpioctrl - Error getting pin %d to output\n", INPUT_DEVICE);
        return status;
    }

    run_irq = gpiod_to_irq(run_sw);
    if (run_irq < 0)
    {
        return run_irq;
    }

    status = request_threaded_irq(
        run_irq,
        NULL,
        run_irq_thread,
        IRQF_TRIGGER_RISING | IRQF_ONESHOT,
        "run_switch",
        NULL);
    if (status)
    {
        return status;
    }

    pause_irq = gpiod_to_irq(pause_sw);
    if (pause_irq < 0)
    {
        free_irq(run_irq, NULL);
        return pause_irq;
    }

    status = request_threaded_irq(
        pause_irq,
        NULL,
        pause_irq_thread,
        IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
        "pause_switch",
        NULL);
    if (status)
    {
        free_irq(run_irq, NULL);
        return status;
    }

    step_irq = gpiod_to_irq(input_dev);
    if (step_irq < 0)
    {
        free_irq(run_irq, NULL);
        free_irq(pause_irq, NULL);
        return step_irq;
    }

    status = request_threaded_irq(
        step_irq,
        NULL,
        step_irq_thread,
        IRQF_TRIGGER_RISING | IRQF_ONESHOT,
        "step_button",
        NULL);
    if (status)
    {
        free_irq(run_irq, NULL);
        free_irq(pause_irq, NULL);
        return status;
    }

    return 0;
}

static char *cpu_devnode(const struct device *dev, umode_t *mode)
{
    if (mode)
        *mode = 0666; // rw-rw-rw-
    return NULL;
}

static int __init chrdev_init(void)
{
    int status;

    status = alloc_chrdev_region(&dev_num, 0, 1, "cpu_cdev");
    if (status)
    {
        pr_err("cpu_cdev - Error reserving the device number\n");
        return status;
    }

    cdev_init(&cpu_cdev, &fops);
    cpu_cdev.owner = THIS_MODULE;

    status = cdev_add(&cpu_cdev, dev_num, 1);
    if (status)
    {
        pr_err("cpu_cdev - Error adding cdev\n");
        goto free_dev_num;
    }

    pr_info("cpu_cdev - Registered character device for Major %d\n", MAJOR(dev_num));

    cpu_class = class_create("cpu_class");
    if (!cpu_class)
    {
        pr_err("cpu_cdev - Could not create class cpu_class\n");
        status = -ENOMEM;
        goto delete_cdev;
    }
    cpu_class->devnode = cpu_devnode;

    if (!device_create(cpu_class, NULL, dev_num, NULL, "cpu_emulator"))
    {
        pr_err("cpu_cdev - Could not create device cpu_emulator\n");
        status = -ENOMEM;
        goto delete_class;
    }

    pr_info("cpu_cdev - Created device cpu_emulator\n");

    return 0;

delete_class:
    class_unregister(cpu_class);
    class_destroy(cpu_class);
delete_cdev:
    cdev_del(&cpu_cdev);
free_dev_num:
    unregister_chrdev_region(dev_num, 1);
    return status;
}

static int __init cpu_init(void)
{
    pr_info("Clock speed set to %dms\n", clock_speed);
    int gpio_status = gpio_init();
    if (gpio_status)
    {
        return gpio_status;
    }
    return chrdev_init();
}

static void __exit cpu_exit(void)
{
    free_irq(run_irq, NULL);
    free_irq(pause_irq, NULL);
    free_irq(step_irq, NULL);

    for (int i = 0; i < 4; i++)
    {
        gpiod_set_value(leds[i], 0);
    }
    gpiod_set_value(output_dev, 0);

    device_destroy(cpu_class, dev_num);
    class_unregister(cpu_class);
    class_destroy(cpu_class);
    cdev_del(&cpu_cdev);
    unregister_chrdev_region(dev_num, 1);

    pr_info("cpu_cdev - Unregistered character device\n");
}

module_init(cpu_init);
module_exit(cpu_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Filip");
MODULE_DESCRIPTION("A driver that emulates the core of a cpu");
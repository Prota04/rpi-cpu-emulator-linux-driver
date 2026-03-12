#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#include "uthash.h"
#include "assembler.h"

#define MAX_LEN 256

typedef struct label_address
{
    char key[64];      // Label name
    uint64_t value;    // Address of the label
    UT_hash_handle hh; // Makes this structure hashable
} label_address;

static void remove_comment(char *line)
{
    char *comment_start = strchr(line, ';');
    if (comment_start != NULL)
    {
        *comment_start = '\0'; // Terminate the string at the start of the comment
    }
}

static int is_empty(char *str)
{
    for (int i = 0; str[i] != '\0'; i++)
    {
        if (str[i] != ' ' && str[i] != '\n')
        {
            return (str[i] == ';') ? 1 : 0;
        }
    }

    return 1;
}

static void put(label_address **labels, const char *key, uint64_t value)
{
    label_address *entry;

    HASH_FIND_STR(*labels, key, entry);
    if (entry == NULL)
    {
        entry = (label_address *)malloc(sizeof(*entry));
        strncpy(entry->key, key, sizeof(entry->key) - 1);
        entry->key[sizeof(entry->key) - 1] = '\0'; // Ensure null-termination
        entry->value = value;
        HASH_ADD_STR(*labels, key, entry);
    }
    entry->value = value; // Update the value if the key already exists
}

static int get(label_address **labels, const char *key, uint64_t *value)
{
    label_address *entry;
    HASH_FIND_STR(*labels, key, entry);
    if (!entry)
    {
        return 0; // Key not found
    }
    *value = entry->value;
    return 1; // Key found
}

static char *substring(const char *str, int start, int end)
{
    if (start >= end || end > strlen(str))
    {
        return NULL; // Invalid indices
    }

    int length = end - start;
    if (length <= 0)
    {
        return NULL; // length has to be a positive integer
    }

    char *sub = (char *)malloc(length + 1);
    if (sub == NULL)
    {
        return NULL; // Memory allocation failed
    }

    strncpy(sub, str + start, length);
    sub[length] = '\0'; // Null-terminate the substring

    return sub;
}

static void split_by_char(char *str, char delimiter, char **part1, char **part2, char **part3)
{
    while (str[0] == ' ')
    {
        memmove(str, str + 1, strlen(str)); // Remove leading spaces
    }

    int start_pos = 0, end_pos = 0;

    for (int i = start_pos; i < strlen(str); i++)
    {
        if (str[i] == delimiter)
        {
            end_pos = i;
            break;
        }
    }

    *part1 = substring(str, start_pos, end_pos);

    start_pos = end_pos + 1;

    while (str[start_pos] == ' ')
    {
        memmove(str + start_pos, str + start_pos + 1, strlen(str) - start_pos); // Remove spaces after the first part
    }

    for (int i = start_pos; i < strlen(str); i++)
    {
        if (str[i] == delimiter)
        {
            end_pos = i;
            break;
        }
    }

    *part2 = substring(str, start_pos, end_pos);

    start_pos = end_pos + 1;

    while (str[start_pos] == ' ')
    {
        memmove(str + start_pos, str + start_pos + 1, strlen(str) - start_pos); // Remove spaces after the second part
    }

    for (int i = start_pos; i < strlen(str); i++)
    {
        if (str[i] == delimiter || str[i] == '\n')
        {
            end_pos = i;
            break;
        }
    }

    if (end_pos < start_pos)
    {
        end_pos = strlen(str);
    }

    *part3 = substring(str, start_pos, end_pos);
}

static int parse_int(const char *str, uint64_t *value)
{
    char *endptr;
    errno = 0;
    uint64_t val = strtoull(str, &endptr, 10);

    if (str == endptr || errno == ERANGE || *endptr != '\0')
    {
        return 0; // Error in parsing
    }

    *value = val;
    return 1; // Successfully parsed
}

static instruction parse_line(char *line, label_address **labels)
{
    instruction instr = {UINT64_MAX, UINT8_MAX, UINT8_MAX, UINT8_MAX}; // Initialize instruction with default values

    char *comma_pos = strchr(line, ',');

    if (comma_pos != NULL)
    {
        *comma_pos = ' '; // Replace comma with space for easier parsing
    }
    char *operation, *operand1, *operand2;
    split_by_char(line, ' ', &operation, &operand1, &operand2);

    if (strcmp(operation, "ADD") == 0)
    {
        instr.opcode = ADD;
    }
    else if (strcmp(operation, "SUB") == 0)
    {
        instr.opcode = SUB;
    }
    else if (strcmp(operation, "AND") == 0)
    {
        instr.opcode = AND;
    }
    else if (strcmp(operation, "OR") == 0)
    {
        instr.opcode = OR;
    }
    else if (strcmp(operation, "NOT") == 0)
    {
        instr.opcode = NOT;

        instr.mode = REGISTER;
    }
    else if (strcmp(operation, "MOV") == 0)
    {
        instr.opcode = MOV;
    }
    else if (strcmp(operation, "CMP") == 0)
    {
        instr.opcode = CMP;
    }
    else if (strcmp(operation, "JMP") == 0)
    {
        instr.opcode = JMP;
        instr.mode = LABEL;

        if (operand1 == NULL)
        {
            get(labels, operand2, &instr.operand2);
        }
        else
        {
            get(labels, operand1, &instr.operand2);
        }

        goto end;
    }
    else if (strcmp(operation, "JE") == 0)
    {
        instr.opcode = JE;
        instr.mode = LABEL;

        if (operand1 == NULL)
        {
            get(labels, operand2, &instr.operand2);
        }
        else
        {
            get(labels, operand1, &instr.operand2);
        }

        goto end;
    }
    else if (strcmp(operation, "JG") == 0)
    {
        instr.opcode = JG;
        instr.mode = LABEL;

        if (operand1 == NULL)
        {
            get(labels, operand2, &instr.operand2);
        }
        else
        {
            get(labels, operand1, &instr.operand2);
        }

        goto end;
    }
    else if (strcmp(operation, "JL") == 0)
    {
        instr.opcode = JL;
        instr.mode = LABEL;

        if (operand1 == NULL)
        {
            get(labels, operand2, &instr.operand2);
        }
        else
        {
            get(labels, operand1, &instr.operand2);
        }

        goto end;
    }

    if (!operand2)
    {
        operand2 = (char *)malloc((strlen(operand1) + 1) * sizeof(char));
        if (!operand2)
        {
            perror("malloc\n");
            goto end;
        }

        strcpy(operand2, operand1);
    }

    if (strcmp(operand2, "R0") == 0)
    {
        instr.operand2 = REG_0;
    }
    else if (strcmp(operand2, "R1") == 0)
    {
        instr.operand2 = REG_1;
    }
    else if (strcmp(operand2, "R2") == 0)
    {
        instr.operand2 = REG_2;
    }
    else if (strcmp(operand2, "R3") == 0)
    {
        instr.operand2 = REG_3;
    }
    else if (operand2[0] == '[' && operand2[strlen(operand2) - 1] == ']')
    {
        instr.mode = DIRECT_LOAD;

        operand2[strlen(operand2) - 1] = '\0';
        for (int i = 0; operand2[i] != '\0'; i++)
        {
            operand2[i] = operand2[i + 1];
        }

        char *endptr;
        instr.operand2 = strtoull(operand2, &endptr, 16);
    }
    else if (parse_int(operand2, &instr.operand2))
    {
        instr.mode = IMMEDIATE;
    }

    if (operand1 == NULL)
    {
        goto end;
    }

    if (strcmp(operand1, "R0") == 0)
    {
        instr.operand1 = REG_0;
    }
    else if (strcmp(operand1, "R1") == 0)
    {
        instr.operand1 = REG_1;
    }
    else if (strcmp(operand1, "R2") == 0)
    {
        instr.operand1 = REG_2;
    }
    else if (strcmp(operand1, "R3") == 0)
    {
        instr.operand1 = REG_3;
    }
    else if (operand1[0] == '[' && operand1[strlen(operand1) - 1] == ']')
    {
        instr.mode = DIRECT_STORE;
        instr.operand1 = instr.operand2;

        operand1[strlen(operand1) - 1] = '\0';
        for (int i = 0; operand1[i] != '\0'; i++)
        {
            operand1[i] = operand1[i + 1];
        }

        char *endptr;
        instr.operand2 = strtoull(operand1, &endptr, 16);
    }

    if (instr.mode == UINT8_MAX)
    {
        instr.mode = REGISTER;
    }

end:
    free(operation);
    free(operand1);
    free(operand2);

    return instr; // Return the parsed instruction
}

char *remove_newline(const char *str)
{
    if (str == NULL)
    {
        return NULL;
    }

    size_t len = strlen(str);
    size_t new_len = len;

    // Determine new length (remove newline if present)
    if (len > 0 && str[len - 1] == '\n')
    {
        new_len = len - 1;
    }

    // Allocate memory for the new string
    char *new_str = malloc(new_len + 1); // +1 for null terminator

    if (new_str == NULL)
    {
        return NULL; // Memory allocation failed
    }

    // Copy the string (without newline if it was removed)
    strncpy(new_str, str, new_len);
    new_str[new_len] = '\0'; // Null terminate

    return new_str;
}

instruction *parse_assembly(const char *fileName, size_t *num_instr, char ***instruction_text)
{
    FILE *file = fopen(fileName, "r");
    if (file == NULL)
    {
        perror("Error opening file");
        return NULL;
    }

    label_address *labels = NULL; // Hash table for labels and their addresses
    size_t line_number = 0;

    while (!feof(file))
    {
        char line[MAX_LEN] = {0};

        if (fgets(line, sizeof(line), file) != NULL && !is_empty(line))
        {
            remove_comment(line);
            char *colon_pos = strchr(line, ':');

            if (colon_pos != NULL)
            {
                *colon_pos = '\0';               // Terminate the string at the colon
                put(&labels, line, line_number); // Store the label and its address in the hash table
                line_number--;                   // Decrement line number as labels do not count as instructions
            }

            line_number++;
        }
    }

    label_address *entry, *tmp;
    HASH_ITER(hh, labels, entry, tmp) {
        printf("Label: %s → %llu\n", entry->key, entry->value);
    }

    fseek(file, 0, SEEK_SET); // Reset file pointer to the beginning for second pass

    instruction *instructions = NULL; // Array to hold parsed instructions
    size_t num_instructions = 0;
    size_t instructions_capacity = 0;
    line_number = 0;

    while (!feof(file))
    {
        char line[MAX_LEN] = {0};

        if (num_instructions == instructions_capacity - 1 || instructions_capacity == 0)
        {
            instructions_capacity = (instructions_capacity == 0) ? 4 : instructions_capacity * 2;
            instructions = realloc(instructions, instructions_capacity * sizeof(instruction));
            if (!instructions)
            {
                perror("realloc for instructions failed!\n");
                fclose(file);
                return NULL;
            }

            char **temp = realloc(*instruction_text, instructions_capacity * sizeof(char *));
            if (!temp)
            {
                perror("realloc for instruction_text failed!\n");
                fclose(file);
                return NULL;
            }
            *instruction_text = temp;
        }

        if (fgets(line, sizeof(line), file) != NULL && !is_empty(line))
        {
            // 1. Make a working copy of the line to safely modify
            char line_copy[MAX_LEN];
            strcpy(line_copy, line);
            
            // 2. Strip comments from the copy, not the original
            remove_comment(line_copy);

            // 3. Check the comment-stripped copy for a colon
            if (strchr(line_copy, ':') == NULL && !is_empty(line_copy))
            {
                // 4. Use the ORIGINAL line (with comments) for your instruction text
                char *line_wo_newline = remove_newline(line);
                
                (*instruction_text)[num_instructions] = (char *)malloc((strlen(line_wo_newline) + 1) * sizeof(char));
                if (!(*instruction_text)[num_instructions])
                {
                    perror("malloc\n");
                    fclose(file);
                    return NULL;
                }

                strcpy((*instruction_text)[num_instructions], line_wo_newline);

                // 5. Pass the comment-stripped copy to your parser 
                // (Assuming parse_line doesn't need the comments)
                instructions[num_instructions++] = parse_line(line_copy, &labels); 
            }
        }
    }

    fclose(file);

    (*instruction_text)[num_instructions] = (char *)malloc(5 * sizeof(char));
    if (!(*instruction_text)[num_instructions])
    {
        perror("malloc\n");
        return NULL;
    }
    strcpy((*instruction_text)[num_instructions], "HALT");

    instruction halt_instr = {MMIO_HALT, 0, MOV, DIRECT_STORE};
    instructions[num_instructions++] = halt_instr; // Add a HALT instruction at the end of the instructions array

    *num_instr = num_instructions;

    return instructions; // Return the array of parsed instructions
}

void store_machine_code(uint8_t *memory, size_t memory_size, instruction *instructions, size_t num_instructions)
{
    if (num_instructions * INSTR_SIZE + INSTR_SIZE > memory_size)
    {
        printf("Not enough memory to store instructions\n");
        return;
    }

    for (int i = 0; i < num_instructions; i++)
    {
        *(memory + i * INSTR_SIZE + 0) = instructions[i].opcode;
        *(memory + i * INSTR_SIZE + 1) = instructions[i].mode;
        *(memory + i * INSTR_SIZE + 2) = instructions[i].operand1;
        memcpy(memory + i * INSTR_SIZE + 3, &(instructions[i].operand2), sizeof(uint64_t));
    }
}
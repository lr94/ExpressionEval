#include <string.h>
#include <math.h>
#include <sys/mman.h>
#include "expreval_internal.h"
#include "list.h"

#define ERROR(a,b)        do { compiler_SetError(c, a, b); return NULL; } while(0)

////
typedef enum {EIL_REG_0 = 0, EIL_REG_1, EIL_REG_2, EIL_REG_3, EIL_REG_4, EIL_REG_5, EIL_REG_6, EIL_REG_7} eil_register_t;
typedef enum
{
    EIL_PUSH,       // Push a register into the stack
    EIL_POP,        // Pop a value from the stack to a register
    EIL_MOVLIT,     // Copy a literal value from the literal table to a register (es MOV REG_1, 1),
                    //      where 1 is the index of the literal
    EIL_ADD,        // ADD REG_1, REG_2
    EIL_SUB,        // ...
    EIL_MUL,
    EIL_DIV,

    EIL_NEG,        // Change the sign of a number; NEG REG_1
    EIL_CALL        // Call a function
} eil_opcode_t;
union eil_operand
{
    eil_register_t reg;
    void *mem;
    int imm;
};
typedef struct _eil_instruction
{
    eil_opcode_t opcode;
    union eil_operand op1;
    union eil_operand op2;
} eil_instruction_t;
typedef struct
{
    list_t *instructions;
    list_t *literals;
} eil_expression_t;
eil_expression_t *compile_to_eil(compiler c, token first_token)
{
    eil_expression_t *expression = malloc(sizeof(eil_expression_t));
    expression->instructions = list_New();
    expression->literals = list_New();

    token t = first_token;
    int stack_size = 0;

    do
    {
        // If the token is a number_value
        if(t->type == TOKEN_NUMBER)
        {
            // Add the number to the list of literals
            double *value = malloc(sizeof(double));
            (*value) = t->number_value;
            list_Append(expression->literals, value);
            int literal_index = expression->literals->count - 1;

            // MOVLIT REG_7, {literal_index}
            eil_instruction_t *movlit = malloc(sizeof(eil_instruction_t));
            movlit->opcode = EIL_MOVLIT;
            movlit->op1.reg = EIL_REG_7;
            movlit->op2.imm = literal_index;
            list_Append(expression->instructions, movlit);

            // PUSH REG_7
            eil_instruction_t *push = malloc(sizeof(eil_instruction_t));
            push->opcode = EIL_PUSH;
            push->op1.reg = EIL_REG_7;
            list_Append(expression->instructions, push);
            stack_size++;
        }
        // If the token is a variable
        else if(t->type == TOKEN_VARIABLE)
        {
            // If it's mapped as argument
            int arg_index = compiler_GetArgumentIndex(c, t->text);
            double constant_value;
            if(arg_index >= 0)
            {
                // PUSH REG_{arg_index}
                eil_instruction_t *push = malloc(sizeof(eil_instruction_t));
                push->opcode = EIL_PUSH;
                push->op1.reg = arg_index;
                list_Append(expression->instructions, push);
                stack_size++;
            }
            // If it's a constant treat it as a literal
            else if(parser_GetVariable(c->p, t->text, &constant_value))
            {
                // Add the number to the list of literals
                double *value = malloc(sizeof(double));
                (*value) = constant_value;
                list_Append(expression->literals, value);
                int literal_index = expression->literals->count - 1;

                // MOVLIT REG_7, {literal_index}
                eil_instruction_t *movlit = malloc(sizeof(eil_instruction_t));
                movlit->opcode = EIL_MOVLIT;
                movlit->op1.reg = EIL_REG_7;
                movlit->op2.imm = literal_index;
                list_Append(expression->instructions, movlit);

                // PUSH REG_7
                eil_instruction_t *push = malloc(sizeof(eil_instruction_t));
                push->opcode = EIL_PUSH;
                push->op1.reg = EIL_REG_7;
                list_Append(expression->instructions, push);
                stack_size++;
            }
            else // If it's an unidentified identifier
                    ERROR(ERROR_UNDEFINED_VAR, t);
        }
        // If it's a binary opearator
        else if(t->type == TOKEN_OPERATOR && op_args[t->op] == 2)
        {
            // The stack must contain at least two values
            if(stack_size < 2)
                    ERROR(ERROR_UNKNOWN, NULL);

            // POP REG_7
            // POP REG_6
            eil_instruction_t *pop = malloc(sizeof(eil_instruction_t));
            pop->opcode = EIL_POP;
            pop->op1.reg = EIL_REG_7;
            list_Append(expression->instructions, pop);
            pop = malloc(sizeof(eil_instruction_t));
            pop->opcode = EIL_POP;
            pop->op1.reg = EIL_REG_6;
            list_Append(expression->instructions, pop);
            stack_size -= 2;

            eil_instruction_t *op;


            switch(t->op)
            {
                case OP_ADD:
                    op = malloc(sizeof(eil_instruction_t));
                    op->opcode = EIL_ADD;
                    op->op1.reg = EIL_REG_6;
                    op->op2.reg = EIL_REG_7;
                    list_Append(expression->instructions, op);
                    break;
                case OP_SUB:
                    op = malloc(sizeof(eil_instruction_t));
                    op->opcode = EIL_SUB;
                    op->op1.reg = EIL_REG_6;
                    op->op2.reg = EIL_REG_7;
                    list_Append(expression->instructions, op);
                    break;
                case OP_MUL:
                    op = malloc(sizeof(eil_instruction_t));
                    op->opcode = EIL_MUL;
                    op->op1.reg = EIL_REG_6;
                    op->op2.reg = EIL_REG_7;
                    list_Append(expression->instructions, op);
                    break;
                case OP_DIV:
                    op = malloc(sizeof(eil_instruction_t));
                    op->opcode = EIL_DIV;
                    op->op1.reg = EIL_REG_6;
                    op->op2.reg = EIL_REG_7;
                    list_Append(expression->instructions, op);
                    break;
                default:
                    ERROR(ERROR_UNKNOWN, NULL); // TODO implement % and ^
            }

            // PUSH REG_6
            eil_instruction_t *push = malloc(sizeof(eil_instruction_t));
            push->opcode = EIL_PUSH;
            push->op1.reg = EIL_REG_6;
            list_Append(expression->instructions, push);
            stack_size++;
        }
        // Unary operators
        else if(t->type == TOKEN_OPERATOR && op_args[t->op] == 1)
        {
            eil_instruction_t *pop;
            eil_instruction_t *op;
            eil_instruction_t *push;

            switch (t->op)
            {
                case OP_SUB:
                    // POP REG_6
                    pop = malloc(sizeof(eil_instruction_t));
                    pop->opcode = EIL_POP;
                    pop->op1.reg = EIL_REG_6;
                    list_Append(expression->instructions, pop);
                    stack_size--;

                    // NEG REG_6
                    op = malloc(sizeof(eil_instruction_t));
                    op->opcode = EIL_NEG;
                    op->op1.reg = EIL_REG_6;
                    list_Append(expression->instructions, op);

                    // PUSH REG_6
                    push = malloc(sizeof(eil_instruction_t));
                    push->opcode = EIL_PUSH;
                    push->op1.reg = EIL_REG_6;
                    list_Append(expression->instructions, push);
                    stack_size++;
                    break;
                case OP_POS:
                    break;
            }
        }
        else if(t->type == TOKEN_FUNCTION)
        {
            ERROR(ERROR_UNKNOWN, NULL); // TODO implement function calling
        }
    } while((t = t->next) != NULL);

    // Now the stack should contain just one value
    if(stack_size != 1)
            ERROR(ERROR_UNKNOWN, NULL);

    return expression;
}

#include <stdio.h>
void eil_expression_dump(eil_expression_t *expression)
{
    listnode_t *n;
    printf("\nInstructions:\n");
    for(n = expression->instructions->first; n != NULL; n = n->next)
    {
        eil_instruction_t *instruction = n->ptr;

        const char *vals[] = {"PUSH", "POP", "MOVLIT", "ADD", "SUB", "MUL", "DIV", "NEG", "CALL"};
        const char *regs[] = {"REG_0", "REG_1", "REG_2", "REG_3", "REG_4", "REG_5", "REG_6", "REG_7"};

        printf("\t%s %s", vals[instruction->opcode], regs[instruction->op1.reg]);

        switch (instruction->opcode)
        {
            case EIL_ADD:
            case EIL_SUB:
            case EIL_MUL:
            case EIL_DIV:
                printf(" %s", regs[instruction->op2.reg]);
                break;
            case EIL_MOVLIT:
                printf(" %d", instruction->op2.imm);
                break;
            case EIL_CALL:
                printf(" %p", instruction->op2.mem);
                break;
        }
        printf("\n");
    }
    printf("Literals:\n");
    for(n = expression->literals->first; n != NULL; n = n->next)
        printf("\t%f\n", *(double*)(n->ptr));
    printf("\n");
}

void eil_expression_destroy(eil_expression_t *expression)
{
    listnode_t *n;
    for(n = expression->instructions->first; n != NULL; n = n->next)
        free(n->ptr);

    for(n = expression->literals->first; n != NULL; n = n->next)
        free(n->ptr);

    list_Free(expression->instructions);
    list_Free(expression->literals);
}

////

static void *create_executable_code_aarch64_linux(const char *machine_code, int size);

void *compile_function_internal(compiler c, token first_token, int *size)
{
    unsigned char code[MAX_CODE_LEN];

    /*
    eil_expression_t *expr = compile_to_eil(c, first_token);
    eil_expression_dump(expr);
    eil_expression_destroy(expr);
    */

    // f(x) = x * 2
    unsigned char tmp[] = {0x00, 0x28, 0x60, 0x1e, 0xc0, 0x03, 0x5f, 0xd6};
    int n = 8;

    memcpy(code, tmp, n);

    if(size != NULL)
        (*size) = n;

    return create_executable_code_aarch64_linux(code, n);
}

/*
        Attraverso opportune chiamate al kernel alloca un vettore della dimensione adatta che contenga
        il codice appena compilato, vi copia il codice e lo rende eseguibile (avendo cura di renderlo
        non scrivibile, visto che è bene che la memoria non sia mai scrivibile ed eseguibile nello
        stesso momento.
*/
static void *create_executable_code_aarch64_linux(const char *machine_code, int size)
{
        /* Memoria rw- */
        void *mem = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
        if(mem == MAP_FAILED)
                return NULL;
        /* Scrivo il codice */
        memcpy(mem, machine_code, size);
        /* Inibisco la scrittura e consento l'esecuzione (r-x) */
        if(mprotect(mem, size, PROT_READ | PROT_EXEC) < 0)
        {
                munmap(mem, size);
                return NULL;
        }

        return mem;
}

/*
        Floating point support could be verified through
            mrs x0, id_aa64pfr0_el1
        but the register id_aa64pfr0_el1 is not accessible
        from userspace (el0), so this function always returns 1,
        assuming that floating point instructions are supported
*/
int check_system_support(compiler c)
{
        return 1;
}

#include <string.h>
#include <stdint.h>
#include <math.h>
#include <sys/mman.h>
#include "arch/aarch64.h"
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
    EIL_POW,
    EIL_MOD,

    EIL_NEG,        // Change the sign of a number; NEG REG_1
    EIL_CALL        // Call a function; CALL function_ptr, num_params
} eil_opcode_t;
union eil_operand
{
    eil_register_t reg;
    void *mem;
    int imm;
};
union eil_literal
{
    double fp;
    void *ptr;
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
            union eil_literal *value = malloc(sizeof(union eil_literal));
            (*value).fp = t->number_value;
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
                union eil_literal *value = malloc(sizeof(union eil_literal));
                (*value).fp = constant_value;
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
                case OP_PWR:
                    op = malloc(sizeof(eil_instruction_t));
                    op->opcode = EIL_POW;
                    op->op1.reg = EIL_REG_6;
                    op->op2.reg = EIL_REG_7;
                    list_Append(expression->instructions, op);
                    break;
                case OP_MOD:
                    op = malloc(sizeof(eil_instruction_t));
                    op->opcode = EIL_MOD;
                    op->op1.reg = EIL_REG_6;
                    op->op2.reg = EIL_REG_7;
                    list_Append(expression->instructions, op);
                    break;
                default:
                    ERROR(ERROR_UNKNOWN, NULL);
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
                case OP_NEG:
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
            function f = parser_GetFunction(c->p, t->text);

            /* this should never happen, since when a function is not defined
               is not considered a function by the parser */
            if(f == NULL)
                    ERROR(ERROR_UNKNOWN, t);

            if(stack_size < f->num_args)
                ERROR(ERROR_ARGS, t);

            eil_instruction_t *call = malloc(sizeof(eil_instruction_t));
            call->opcode = EIL_CALL;
            call->op1.mem = f->ptr;
            call->op2.imm = f->num_args;
            list_Append(expression->instructions, call);

            /* Calling the function will pop the arguments from the stack and
               push the result */
            stack_size -= (f->num_args - 1);
            // ERROR(ERROR_UNKNOWN, NULL); // TODO implement function calling
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

        const char *vals[] = {"PUSH", "POP", "MOVLIT", "ADD", "SUB", "MUL", "DIV", "POW", "MOD", "NEG", "CALL"};
        const char *regs[] = {"REG_0", "REG_1", "REG_2", "REG_3", "REG_4", "REG_5", "REG_6", "REG_7"};

        if(instruction->opcode == EIL_CALL)
            printf("\t%s %p %d", vals[instruction->opcode], instruction->op1.mem, instruction->op2.imm);
        else
        {
            printf("\t%s %s", vals[instruction->opcode], regs[instruction->op1.reg]);

            switch (instruction->opcode)
            {
                case EIL_ADD:
                case EIL_SUB:
                case EIL_MUL:
                case EIL_DIV:
                case EIL_POW:
                case EIL_MOD:
                    printf(" %s", regs[instruction->op2.reg]);
                    break;
                case EIL_MOVLIT:
                    printf(" %d", instruction->op2.imm);
                    break;
            }
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
    uint32_t code[MAX_CODE_LEN];
    int i = 0;

    eil_expression_t *expr = compile_to_eil(c, first_token);
    // eil_expression_dump(expr);

    listnode_t *n;
    int max_stack_size = 0, current_stack_size = 0;
    int aarch64_inst_count = 0;
    for(n = expr->instructions->first; n != NULL; n = n->next)
    {
        eil_instruction_t *current_instruction = n->ptr;

        switch (current_instruction->opcode)
        {
            case EIL_PUSH:
                current_stack_size++;
                if(current_stack_size > max_stack_size)
                    max_stack_size = current_stack_size;
                aarch64_inst_count++;
                break;
            case EIL_POP:
                current_stack_size--;
                aarch64_inst_count++;
                break;
            case EIL_MOD:
                aarch64_inst_count += 5;
                break;
            case EIL_CALL:
                aarch64_inst_count += 6 + current_instruction->op2.imm + 3 * c->arg_count;
                break;
            default:
                aarch64_inst_count++;
                break;
        }
    }
    max_stack_size *= sizeof(double); // 64 bit per stack slot
    aarch64_inst_count += 4; // alloca stack, pop risultato, libera, ret

    // TODO check max_stack_size <= 4095

    int aarch64_curr_inst_index = 0; // Current instruction index
    // Allocate space in the stack. Note that SP must be 16-byte aligned when accessing memory
    if(max_stack_size & 0xf)
        max_stack_size += 0x10 - (max_stack_size & 0xf);
    // sub sp, sp, #{max_stack_size}
    SUB_imm(SP, SP, max_stack_size);
    aarch64_curr_inst_index++;

    int aarch64_virtual_sp = max_stack_size; // relative to real SP
    for(n = expr->instructions->first; n != NULL; n = n->next)
    {
        eil_instruction_t *current_instruction = n->ptr;

        switch(current_instruction->opcode)
        {
            case EIL_MOVLIT:
                // ldr dN, LITERAL_POINTED_BY_OFFSET
                LDR_fp_lit(current_instruction->op1.reg,
                           (aarch64_inst_count
                                - aarch64_curr_inst_index) * 4
                            + current_instruction->op2.imm * 8);
                aarch64_curr_inst_index++;
                break;

            case EIL_PUSH:
                // str dN, [sp, #{aarch64_virtual_sp}]
                aarch64_virtual_sp -= sizeof(double);
                STR_fp_imm(current_instruction->op1.reg, SP, aarch64_virtual_sp);
                aarch64_curr_inst_index++;
                break;
            case EIL_POP:
                // ldr dN, [sp, #{aarch64_virtual_sp}]
                LDR_fp_imm(current_instruction->op1.reg, SP, aarch64_virtual_sp);
                aarch64_virtual_sp += sizeof(double);
                aarch64_curr_inst_index++;
                break;

            case EIL_ADD:
                FADD(current_instruction->op1.reg,
                     current_instruction->op1.reg,
                     current_instruction->op2.reg);
                aarch64_curr_inst_index++;
                break;
            case EIL_SUB:
                FSUB(current_instruction->op1.reg,
                     current_instruction->op1.reg,
                     current_instruction->op2.reg);
                aarch64_curr_inst_index++;
                break;
            case EIL_MUL:
                FMUL(current_instruction->op1.reg,
                     current_instruction->op1.reg,
                     current_instruction->op2.reg);
                aarch64_curr_inst_index++;
                break;
            case EIL_DIV:
                FDIV(current_instruction->op1.reg,
                     current_instruction->op1.reg,
                     current_instruction->op2.reg);
                aarch64_curr_inst_index++;
                break;

            case EIL_NEG:
                FNEG(current_instruction->op1.reg,
                     current_instruction->op1.reg);
                aarch64_curr_inst_index++;
                break;

            case EIL_MOD:
                // Cast to two 64-bit signed integers
                FCVTZS(X0, current_instruction->op1.reg);
                FCVTZS(X1, current_instruction->op2.reg);
                // Divide
                SDIV(X2, X0, X1);
                // Find remainder
                MSUB(X0, X2, X1, X0); // x0 = x0 - x2 * x1
                // Cast to double precision floating point
                SCVTF(current_instruction->op1.reg, X0);
                break;

            case EIL_CALL:
            {
                /* Now registers from d0 to d3 contain this expression arguments, so we
                   can't modify them. Let's pop the called function args into d4-d7.
                   current_instruction->op2.imm contains the number of arguments taken
                   by the called function */
                int j;
                for(j = current_instruction->op2.imm - 1; j >= 0; j--)
                {
                    // ldr d{j + 4}, [sp, #{aarch64_virtual_sp}]
                    LDR_fp_imm(D0 + 4 + j, SP, aarch64_virtual_sp);
                    aarch64_virtual_sp += sizeof(double);
                    aarch64_curr_inst_index++;
                }

                /* Push into the stack this expression parameters and copy into
                   registers from d0 to d3 the called function arguments */
                for(j = 0; j < c->arg_count; j++)
                {
                    // PUSH
                    aarch64_virtual_sp -= sizeof(double);
                    // ldr d{j}, [sp, #{aarch64_virtual_sp}]
                    STR_fp_imm(D0 + j, SP, aarch64_virtual_sp);
                    aarch64_curr_inst_index++;

                    // fmov d{j}, d{j + 4}
                    FMOV(D0 + j, D0 + j + 4);
                    aarch64_curr_inst_index++;
                }

                // Add to the literal table the function address
                union eil_literal *lit_address = malloc(sizeof(union eil_literal));
                lit_address->ptr = current_instruction->op1.mem;
                list_Append(expr->literals, lit_address);
                int literal_index = expr->literals->count - 1;

                // Loads the literal address in X0
                LDR_lit(X0, (aarch64_inst_count - aarch64_curr_inst_index) * 4
                            + literal_index * 8);
                aarch64_curr_inst_index++;

                STP_pri(X29, X30, SP, -16);
                aarch64_curr_inst_index++;

                // Procedure call
                BLR(X0);
                aarch64_curr_inst_index++;

                LDP_psi(X29, X30, SP, 16);
                aarch64_curr_inst_index++;
                // NOP;

                // Move the result to a temporary register
                // fmov d6, d0
                FMOV(D6, D0);
                aarch64_curr_inst_index++;

                // Restore this expression parameters
                for(j = c->arg_count - 1; j >= 0; j--)
                {
                    // ldr d{j}, [sp, #{aarch64_virtual_sp}]
                    LDR_fp_imm(D0 + j, SP, aarch64_virtual_sp);
                    aarch64_virtual_sp += sizeof(double);
                    aarch64_curr_inst_index++;
                }

                // Push the result value into the stack
                aarch64_virtual_sp -= sizeof(double);
                // ldr d6, [sp, #{aarch64_virtual_sp}]
                STR_fp_imm(D6, SP, aarch64_virtual_sp);
                aarch64_curr_inst_index++;

                break;
            }
        }
    }

    // Pop the result from the stack
    // ldr d0, [sp, #{aarch64_virtual_sp}]
    LDR_fp_imm(D0, SP, aarch64_virtual_sp);
    aarch64_virtual_sp += sizeof(double);

    // Free space
    // add sp, sp, #{max_stack_size}
    ADD_imm(SP, SP, max_stack_size);
    // ret
    RET;

    // Copy literal values
    for(n = expr->literals->first; n != NULL; n = n->next)
    {
        union eil_literal *l = n->ptr;
        memcpy(code + i, l, sizeof(double)); // 8 bytes
        i += 2;
    }

    if(size != NULL)
        (*size) = i * 4;

    eil_expression_destroy(expr);


    // Dump
    int j;
    /*
    for(j = 0; j < i; j++)
        printf("%08x\n", code[j]);
    printf("\n");
    // exit(0);
    FILE *fp = fopen("test.bin", "wb");
    char *cd = (char*)code;
    for(j = 0; j < i * 4; j++)
        fputc(cd[j], fp);
    fclose(fp);
    // exit(0);
    // printf("cos: %p\nme:%p\n", cos, compile_function_internal);
    */

    return create_executable_code_aarch64_linux((const char*)code, i * 4);
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

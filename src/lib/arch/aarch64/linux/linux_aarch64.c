#include <string.h>
#include <stdint.h>
#include <math.h>
#include <sys/mman.h>
#include "arch/aarch64.h"
#include "expreval_internal.h"
#include "list.h"
#include "eil.h"

#define ERROR(a,b)        do { compiler_SetError(c, a, b); return NULL; } while(0)

static void *create_executable_code_aarch64_linux(const char *machine_code, int size);

void *compile_function_internal(compiler c, token first_token, int *size)
{
    uint32_t code[MAX_CODE_LEN];
    int i = 0;

    eil_expression_t *expr = compile_to_eil(c, first_token);
    // eil_expression_dump(expr);

    listnode_t *n;

    // Calculate stack size and number of instructions in advance
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

            case EIL_POW:
                aarch64_inst_count += 7 + 2 * c->arg_count;
                current_stack_size += c->arg_count;
                if(current_stack_size > max_stack_size)
                    max_stack_size = current_stack_size;
                current_stack_size -= c->arg_count;
                break;
            case EIL_CALL: // TODO check
                aarch64_inst_count += 6 + current_instruction->op2.imm + 3 * c->arg_count;
                current_stack_size = current_stack_size
                                     - current_instruction->op2.imm
                                     + c->arg_count;
                if(current_stack_size > max_stack_size)
                    max_stack_size = current_stack_size;
                current_stack_size = current_stack_size - c->arg_count + 1;
                break;

            default:
                aarch64_inst_count++;
                break;
        }
    }
    max_stack_size *= sizeof(double); // 64 bit per stack slot
    aarch64_inst_count += 4; // allocate stack, pop result, free, ret

    // Check that max_stack_size <= 4095
    if(max_stack_size >= 4095)
        ERROR(ERROR_STACKOVERFLOW, NULL);

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

            case EIL_POW:
            {
                int j;

                // Push into the stack this expression parameters
                for(j = 0; j < c->arg_count; j++)
                {
                    // PUSH
                    aarch64_virtual_sp -= sizeof(double);
                    // ldr d{j}, [sp, #{aarch64_virtual_sp}]
                    STR_fp_imm(D0 + j, SP, aarch64_virtual_sp);
                    aarch64_curr_inst_index++;
                }

                // Move "pow" args to d0 and d1
                FMOV(D0, current_instruction->op1.reg);
                FMOV(D1, current_instruction->op2.reg);
                aarch64_curr_inst_index += 2;

                // Add to the literal table "pow()" address
                union eil_literal *lit_address = malloc(sizeof(union eil_literal));
                lit_address->ptr = pow;
                list_Append(expr->literals, lit_address);
                int literal_index = expr->literals->count - 1;

                // Loads the literal address in X0
                LDR_lit(X0, (aarch64_inst_count - aarch64_curr_inst_index) * 4
                            + literal_index * 8);
                aarch64_curr_inst_index++;

                // Store frame pointer (FP/X29) and link register (LR/X30) into the stack
                STP_pri(X29, X30, SP, -16);
                aarch64_curr_inst_index++;

                // Procedure call
                BLR(X0);
                aarch64_curr_inst_index++;

                // Restore frame pointer (FP/X29) and link register (LR/X30) from the stack
                LDP_psi(X29, X30, SP, 16);
                aarch64_curr_inst_index++;

                // Move the result to the final register
                // fmov {?}, d0
                FMOV(current_instruction->op1.reg, D0);
                aarch64_curr_inst_index++;

                // Restore this expression parameters
                for(j = c->arg_count - 1; j >= 0; j--)
                {
                    // ldr d{j}, [sp, #{aarch64_virtual_sp}]
                    LDR_fp_imm(D0 + j, SP, aarch64_virtual_sp);
                    aarch64_virtual_sp += sizeof(double);
                    aarch64_curr_inst_index++;
                }

                break;
            }
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

                // Store frame pointer (FP/X29) and link register (LR/X30) into the stack
                STP_pri(X29, X30, SP, -16);
                aarch64_curr_inst_index++;

                // Procedure call
                BLR(X0);
                aarch64_curr_inst_index++;

                // Restore frame pointer (FP/X29) and link register (LR/X30) from the stack
                LDP_psi(X29, X30, SP, 16);
                aarch64_curr_inst_index++;

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


    /*
    // Dump
    int j;

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

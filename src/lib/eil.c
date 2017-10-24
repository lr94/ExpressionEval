#include <stdlib.h>
#include <stdio.h>
#include "expreval_internal.h"
#include "list.h"
#include "eil.h"

#define ERROR(a,b)        do { compiler_SetError(c, a, b); return NULL; } while(0)

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

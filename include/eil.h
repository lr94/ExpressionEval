#ifndef EIL_H
#define EIL_H
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

eil_expression_t *compile_to_eil(compiler c, token first_token);
void eil_expression_dump(eil_expression_t *expression);
void eil_expression_destroy(eil_expression_t *expression);
#endif

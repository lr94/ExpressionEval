#ifndef AARCH64_H
#define AARCH64_H
// Double precision (64-bit) floating point registers
enum {D0 = 0, D1, D2, D3, D4, D5, D6, D7,
      D8, D9, D10, D11, D12, D13, D14, D15,
      D16, D17, D18, D19, D20, D21, D22, D23,
      D24, D25, D26, D27, D28, D29, D30, D31};

// General purpose 64-bit registers and stack pointer
enum {X0 = 0, X1, X2, X3, X4, X5, X6, X7,
      X8, X9, X10, X11, X12, X13, X14, X15,
      X16, X17, X18, X19, X20, X21, X22, X23,
      X24, X25, X26, X27, X28, X29, X30, SP};

// All the floating point instructions take only fp (double precision) operands
#define FADD(x,y,z)         code[i++] = 0x1e602800 | (x) | ((y) << 5) | ((z) << 16)
#define FSUB(x,y,z)         code[i++] = 0x1e603800 | (x) | ((y) << 5) | ((z) << 16)
#define FMUL(x,y,z)         code[i++] = 0x1e600800 | (x) | ((y) << 5) | ((z) << 16)
#define FDIV(x,y,z)         code[i++] = 0x1e601800 | (x) | ((y) << 5) | ((z) << 16)

#define FNEG(x,y)           code[i++] = 0x1e614000 | (x) | ((y) << 5)

#define FMOV(x,y)           code[i++] = 0x1e604000 | (x) | ((y) << 5)

// x is a general purpose 64-bit register (Xn), y is a double precision floating point register (Dn)
#define FCVTZS(x,y)         code[i++] = 0x9e780000 | (x) | ((y) << 5)

// x is a double precision fp register (Dn) and y is a general purpose 64-bit register (Xn)
#define SCVTF(x,y)          code[i++] = 0x9e620000 | (x) | ((y) << 5)

// x, y and z are general purpose 64-bit registers (Xn)
#define SDIV(x,y,z)         code[i++] = 0x9ac00c00 | (x) | ((y) << 5) | ((z) << 16)

#define MSUB(w,x,y,z)       code[i++] = 0x9b008000 | (w) | ((x) << 5) | ((z) << 10) | \
                            ((y) << 16)

// x and y are registers (Xn|SP) and imm is an unsigned immediate value (0..4095)
//      sub {x}, {y}, #{imm}
#define SUB_imm(x,y,imm)    code[i++] = 0xd1000000 | (x) | ((y) << 5) | \
                            (((imm) & 0xfff) << 10)
#define ADD_imm(x,y,imm)    code[i++] = 0x91000000 | (x) | ((y) << 5) | \
                            (((imm) & 0xfff) << 10)

/*
    x is a floating poing 64 bit register (Dn), y is a general purpose register (Xn|SP)
    and imm is an unsigned immediate value (0..32760) multiple of 8
        str {x}, [{y}, #{imm}]
        ldr {x}, [{y}, #{imm}]
*/
#define STR_fp_imm(x,y,imm) code[i++] = 0xfd000000 | (x) | ((y) << 5) | \
                            ((((imm) >> 3) & 0xfff) << 10)
#define LDR_fp_imm(x,y,imm) code[i++] = 0xfd400000 | (x) | ((y) << 5) | \
                            ((((imm) >> 3) & 0xfff) << 10)

/*
    x is a double precision (64 bit) floating poing register (Dn),
    lit is an offset (+-1 1MiB) relative to the address of this instruction.
    It must be a multiple of 4.
*/
#define LDR_fp_lit(x,lit)  code[i++] = 0x5c000000 | (x) | ((((lit) >> 2) & 0x7ffff) << 5)

/*
    x is a 64 bit general purpose register, lit is an offset (+-1 1MiB) relative
    to the address of this instruction. It must be a multiple of 4.
*/
#define LDR_lit(x,lit)     code[i++] = 0x58000000 | (x) | ((((lit) >> 2) & 0x7ffff) << 5)

/*
    Pre-index STP
    x and y are general purpose 64-bit registers (Xn)
    z is a general purpose register or the stack pointer (Xn|SP)
    imm is a signed offset multiple of 8 (-512..504)
        stp {x}, {y}, [{z}, #{imm}]!
*/
#define STP_pri(x,y,z,imm) code[i++] = 0xa9800000 | (x) | ((z) << 5) | ((y) << 10) | \
                           ((((imm) >> 3) & 0x7f) << 15)

/*
    Post-index LDP
    x and y are general purpose 64-bit registers (Xn)
    z is a general purpose register or the stack pointer (Xn|SP)
    imm is a signed offset multiple of 8 (-512..504)
        ldp {x}, {y}, [{z}], #{imm}
*/
#define LDP_psi(x,y,z,imm) code[i++] = 0xa8c00000 | (x) | ((z) << 5) | ((y) << 10) | \
                           ((((imm) >> 3) & 0x7f) << 15)

/*
    Branch with link to a register. Calls a subroutine at an address in a
    general purpose register (Xn|XZR)
        blr {x}
*/
#define BLR(x)              code[i++] = 0xd63f0000 | ((x) << 5)

#define RET                 code[i++] = 0xd65f03c0

#define NOP                 code[i++] = 0xd503201f
#endif

#ifndef AARCH64_H
#define AARCH64_H
enum {D0 = 0, D1, D2, D3, D4, D5, D6, D7,
      D8, D9, D10, D11, D12, D13, D14, D15,
      D16, D17, D18, D19, D20, D21, D22, D23,
      D24, D25, D26, D27, D28, D29, D30, D31};

enum {X0 = 0, X1, X2, X3, X4, X5, X6, X7,
      X8, X9, X10, X11, X12, X13, X14, X15,
      X16, X17, X18, X19, X20, X21, X22, X23,
      X24, X25, X26, X27, X28, X29, X30, SP};

#define FADD(x,y,z)         code[i++] = 0x1e602800 | (x) | ((y) << 5) | ((z) << 16)
#define FSUB(x,y,z)         code[i++] = 0x1e603800 | (x) | ((y) << 5) | ((z) << 16)
#define FMUL(x,y,z)         code[i++] = 0x1e600800 | (x) | ((y) << 5) | ((z) << 16)
#define FDIV(x,y,z)         code[i++] = 0x1e601800 | (x) | ((y) << 5) | ((z) << 16)

#define FMOV(x,y)           code[i++] = 0x1e604000 | (x) | ((y) << 5)

#define RET                 code[i++] = 0xd65f03c0
#endif

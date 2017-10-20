#ifndef AARCH64_H
#define AARCH64_H
enum {D0 = 0, D1, D2, D3, D4, D5, D6, D7,
      D8, D9, D10, D11, D12, D13, D14, D15,
      D16, D17, D18, D19, D20, D21, D22, D23,
      D24, D25, D26, D27, D28, D29, D30, D31};

#define FADD(x,y,z)         code[i++] = 0x1e602800 | (x) | ((y) << 5) | ((z) << 16)
#define FSUB(x,y,z)         code[i++] = 0x1e603800 | (x) | ((y) << 5) | ((z) << 16)
#define FMUL(x,y,z)         code[i++] = 0x1e600800 | (x) | ((y) << 5) | ((z) << 16)
#define FDIV(x,y,z)         code[i++] = 0x1e601800 | (x) | ((y) << 5) | ((z) << 16)

#define FMOV(x,y)           code[i++] = 0x1e604000 | (x) | ((y) << 5)

#define RET                 code[i++] = 0xd65f03c0
#endif

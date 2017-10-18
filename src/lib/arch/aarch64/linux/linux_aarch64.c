#include <string.h>
#include <math.h>
#include <sys/mman.h>
#include "expreval_internal.h"

static void *create_executable_code_aarch64_linux(const char *machine_code, int size);

void *compile_function_internal(compiler c, token first_token, int *size)
{
    unsigned char code[MAX_CODE_LEN];

    unsigned char tmp[] = {0x1e, 0x60, 0x28, 0x00, 0xd6, 0x5f, 0x03, 0xc0};
    int n = 8;

    memcpy(code, tmp, n);

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

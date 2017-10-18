#if __i386__ // Tutto questo file va compilato solo se siamo su architettura x86_32!

#include <string.h>
#include <math.h>
#include <sys/mman.h>
#include "expreval_internal.h"
#include "arch/x86.h"

typedef unsigned long long int uint64;

extern unsigned int cpu_features();
static void *create_executable_code_x86_linux(char *machine_code, int size);

#define ERROR(a,b)        do { compiler_SetError(c, a, b); return NULL; } while(0)

/* Definisco istruzioni fittizie pushq e popq:
        pushq XMMn:
                sub esp, 8
                movq [esp], XMMn
        popq XMMn:
                movq XMMn, [esp]
                add esp, 8
   I registri XMM sono a 128 bit, ma qui noi siamo interessati
   solo a 64 bit (che sono quelli effettivamente spostati dalla
   movq) quindi modifico esp solo di 8 */
#define PUSH_Q(n)        do { SUB_imm(ESP, 8); MOVQ(_ESP_(0), n); } while(0)
#define  POP_Q(n)        do { MOVQ(n, _ESP_(0)); ADD_imm(ESP, 8); } while(0)

/* Definisco istruzione fittizia per chiamata a funzione (ind. assoluto)
   ATTENZIONE CHE SPORCA EAX
*/
#define CALL_FUN(f)        do { MOV_imm(EAX, f); CALL(EAX); } while(0)

/*
        Questa funzione si occupa della compilazione vera e propria.
        Riceve come parametri:
                -L'oggetto compiler
                -Il primo token della linked list rappresentante l'espressione
                 (già convertita in RPN mediante l'algoritmo shunting-yard)
                -Un puntatore a un valore intero che alla fine conterrà
                 la lunghezza del codice macchina generato (se il puntatore
                 è NULL viene semplicemente ignorato).
*/
void *compile_function_internal(compiler c, token first_token, int *size)
{
        /* Queste 5 variabili sono tutte usate dalle macro di compilazione e non possono essere rinominate */
        unsigned char code[MAX_CODE_LEN], sib = 0;
        int i = 0, disp = 0, flag = 0;
        
        token t;
        
        int stack_size = 0;
        int j;
        
        // Prelevo gli argomenti dallo stack
        for(j = 0; j < c->arg_count; j++)
                MOVQ(XMM(j), _ESP_(4 + 8 * j));                                   // movq xmm{j}, [esp + {4 + 8 * j}]
        // Salvo EBP ci copio lo stack pointer
        PUSH(EBP);                                                                // push ebp
        // Alloco 8 bytes nello stack (li uso per operazioni varie)
        SUB_imm(ESP, 8);                                                          // sub esp, 0x8
        MOV(EBP, ESP);	                                                          // mov ebp, esp
        
        t = first_token;
        do
        {
                /* Se il token è un numero */
                if(t->type == TOKEN_NUMBER)
                {
                        /* Visto che a quanto pare non esiste una mov
                           per inserire direttamente un valore immediato
                           in un registro XMM, faccio questo rigiro
                           strano (uso la 32 bit per volta per inserire
                           il valore double a 64 bit negli 8 byte allocati
                           inizialmente nello stack per poi portarlo
                           nel registro XMM7.
                           Sicuramente è migliorabile.
                        */
			uint64 v;
			v=*((uint64*)&(t->number_value));
			MOV_imm(ECX, (unsigned int)(v & 0xffffffff));             // mov ecx, {v & 0xffffffff}
			MOV(_EBP_(0), ECX);                                       // mov [ebp], ecx
			MOV_imm(ECX, (unsigned int)(v >> 32));                    // mov ecx, {v >> 32}
			MOV(_EBP_(4), ECX);                                       // mov [ebp+0x4], ecx
			MOVQ(XMM7, _EBP_(0));                                     // movq xmm7, [ebp]
                        /* E inserito nello stack */
                        PUSH_Q(XMM7);                                             // pushq xmm7 ; In realtà questa istruzione non esiste (compilata come 2)
                        stack_size++;
                }
                /* Se è una variabile */
                else if(t->type == TOKEN_VARIABLE)
                {
                        /* Se la variabile è mappata come argomento */
                        int arg_index = compiler_GetArgumentIndex(c, t->text);
                        double constant_value;
                        if(arg_index >= 0)
                        {
                                PUSH_Q(XMM(arg_index));                           // pushq xmm{arg_index}
                                stack_size++;
                        }
                        /* Se è una costante faccio come se fosse TOKEN_NUMBER */
                        else if(parser_GetVariable(c->p, t->text, &constant_value))
                        {
                                // Vedi sopra (TOKEN_NUMBER)
				uint64 v;
				v=*((uint64*)&(constant_value));
				MOV_imm(ECX, (unsigned int)(v & 0xffffffff));     // mov ecx, {v & 0xffffffff}
				MOV(_EBP_(0), ECX);                               // mov [ebp], ecx
				MOV_imm(ECX, (unsigned int)(v >> 32));            // mov ecx, {v >> 32}
				MOV(_EBP_(4), ECX);                               // mov [ebp+0x4], ecx
				MOVQ(XMM7, _EBP_(0));                             // movq xmm7, [ebp]
		                /* E inserito nello stack */
		                PUSH_Q(XMM7);                                     // pushq xmm7 ; In realtà questa istruzione non esiste (compilata come 2)
		                stack_size++;
                        }
                        else /* Se non è definita */
                                ERROR(ERROR_UNDEFINED_VAR, t);
                }
                else if(t->type == TOKEN_OPERATOR && op_args[t->op] == 2)
                {
                        if(stack_size < 2)
                                ERROR(ERROR_UNKNOWN, NULL);
                        POP_Q(XMM7);                                              // popq xmm7
                        POP_Q(XMM6);                                              // popq xmm6
                        stack_size -= 2;
                        switch(t->op)
                        {
                                case OP_ADD:
                                        ADDSD(XMM6, XMM7);                        // addsd xmm6, xmm7
                                        break;
                                case OP_SUB:
                                        SUBSD(XMM6, XMM7);                        // subsd xmm6, xmm7
                                        break;
                                case OP_MUL:
                                        MULSD(XMM6, XMM7);                        // mulsd xmm6, xmm7
                                        break;
                                case OP_DIV:
                                        DIVSD(XMM6, XMM7);                        // divsd xmm6, xmm7
                                        break;
                                case OP_MOD:
                                        CVTTSD2SI(ECX, XMM7);                     // cvttsd2si ebx, xmm7
                                        CVTTSD2SI(EAX, XMM6);                     // cvttsd2si eax, xmm6
                                        CDQ;                                      // cdq
                                        IDIV(ECX);                                // idiv ebx           ; Qui intendo proprio ecx, non ecx (anche su x86_64)
                                        CVTSI2SD(XMM6, EDX);                      // cvtsi2sd xmm6, edx
                                        break;
                                case OP_PWR:                                                            
                                        PUSH_Q(XMM7);                             // pushq xmm7
                                        PUSH_Q(XMM6);                             // popq xmm6
                                        CALL_FUN(pow);                            // call pow
                                        ADD_imm(ESP, 0x10);                       // add esp, 0x10      ; 0x10 == 16
                                        FSTP(_EBP_(0));                           // fstp [ebp]
                                        MOVQ(XMM6, _EBP_(0));                     // movq xmm6, [ebp]
                                        break;
                        }
                        PUSH_Q(XMM6);                                             // pushq xmm6
                        stack_size += 1;
                }
                else if(t->type == TOKEN_OPERATOR && op_args[t->op] == 1)
                {
                        /* Nei valori double (standard IEEE 754) il bit 63
                           indica il segno, per cambiare il segno di un
                           double quindi basta fare un bitwise xor un valore
                           avente tutti i bit a 0 e tranne quello più
                           significativo */
                        uint64 v = 1ULL << 63;
                        /* Operatore unario, deve esserci almeno un valore
                           nello stack */
                        if(stack_size < 1)
                                ERROR(ERROR_UNKNOWN, NULL);
                        switch(t->op)
                        {
                                case OP_NEG:
                                        POP_Q(XMM6);                              // popq xmm6
                                        stack_size--;
                                        /* Vedi sopra (TOKEN_NUMBER), l'unica
                                           differenza è che ora v non rappresenta
                                           più un valore double ma 10000[...] */
					MOV_imm(ECX, (unsigned int)(v & 0xffffffff));// mov ecx, {v & 0xffffffff}
					MOV(_EBP_(0), ECX);                       // mov [ebp], ecx
					MOV_imm(ECX, (unsigned int)(v >> 32));    // mov ecx, {v >> 32}
					MOV(_EBP_(4), ECX);                       // mov [ebp+0x4], ecx
					MOVQ(XMM7, _EBP_(0));                     // movq xmm7, [ebp]
                                        XORPD(XMM6, XMM7);                        // xorpd xmm6, xmm7
                                        PUSH_Q(XMM6);                             // pushq xmm6
                                        stack_size++;
                                        break;
                                /* In caso di "+" non bisogna fare proprio
                                   niente, anche per questo ho messo la pop
                                   e la push dentro a OP_NEG (-), qui faremmo
                                   pop e push dello stesso valore */
                                case OP_POS:
                                        break;
                        }
                }
                else if(t->type == TOKEN_FUNCTION)
                {
                        function f = parser_GetFunction(c->p, t->text);
                        /* Non dovrebbe succedere mai: se una funzione non è definita
                           non viene neanche considerata una funzione dal parser e
                           viene trattata come una variabile */
                        if(f == NULL)
                                ERROR(ERROR_UNKNOWN, t);
                        
                        if(stack_size < f->num_args)
                                ERROR(ERROR_ARGS, t);
                        
                        /* xmm{0-3} contengono i parametri
                           dell'espressione, quindi non possiamo toccarli!
                           Faccio POP in altri registri (xmm{4-7}) */
                        for(j = f->num_args - 1; j >= 0; j--)
                                POP_Q(XMM(j + 4));                                // popq xmm{j + 4}
                        stack_size -= f->num_args;
                        
                        /* Salvo nello stack i parametri dell'espressione
                           che potrebbero essere sovrascritti durante la
                           chiamata */
                        for(j = 0; j < c->arg_count; j++)
                                PUSH_Q(XMM(j));                                   // pushq xmm{j}
                        
                        /* Faccio push degli argomenti della funzione:
                           prima erano già nello stack ma in ordine inverso */
                        for(j = f->num_args - 1; j >= 0; j--)
                                PUSH_Q(XMM(j + 4));                               // pushq xmm{j + 4}                        
                        
                        /* Chiamo la funzione e salvo il risultato in XMM6 */
                        CALL_FUN(f->ptr);                                         // call {f->ptr}
                        FSTP(_EBP_(0));                                           // fstp [ebp]
                        MOVQ(XMM6, _EBP_(0));                                     // movq xmm6, [ebp]
                        ADD_imm(ESP, 8 * f->num_args);                            // add esp, {8 * f->num_args}
                        
                         /* Recupero dallo stack i parametri dell'espressione
                            che potrebbero essere stati sovrascritti durante la chiamata */
                        for(j = c->arg_count - 1; j >= 0; j--)
                                POP_Q(XMM(j));                                    // popq xmm{j}     
                        
                        /* Push nello stack del risultato */
                        PUSH_Q(XMM6);                                             // push xmm6        
                        stack_size++;
                }
        } while((t = t->next) != NULL);
        
        /* Lo stack dei valori alla fine deve contenere
           un solo numero. */
        if(stack_size != 1)
                ERROR(ERROR_UNKNOWN, NULL);
        else
                POP_Q(XMM0); /* Valore che viene restituito */                    // popq xmm0
        
        MOVQ(_EBP_(0), XMM0);                                                     // movw [ebp], xmm0
        FLD(_EBP_(0));                                                            // fld [ebp]
        ADD_imm(ESP, 8);                                                          // add esp, 0x8
        POP(EBP);	                                                          // pop ebp
        RET;                                                                      // ret
        
        if(size != NULL)
                *size = i;
        
        void *executable = create_executable_code_x86_linux(code, i);
        
        if(executable == NULL)
                ERROR(ERROR_SYSTEM, NULL);
        
        return executable;
}

/*
        Attraverso opportune chiamate al kernel alloca un vettore della dimensione adatta che contenga
        il codice appena compilato, vi copia il codice e lo rende eseguibile (avendo cura di renderlo
        non scrivibile, visto che è bene che la memoria non sia mai scrivibile ed eseguibile nello
        stesso momento.
*/
static void *create_executable_code_x86_linux(char *machine_code, int size)
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
        Restituisce 1 se la CPU supporta il set di istruzioni SSE2, 0 altrimenti
*/
static int supported_sse2()
{
        /* cpu_features() è definita in cpu_id.asm e funziona eseguendo
           l'istruzione CPUID.
           Il supporto a SSE2 è indicato dal bit 26 del valore che si
           trova in EDX dopo CPUID (che è restituito da cpu_features()
           https://en.wikipedia.org/wiki/CPUID#EAX.3D1:_Processor_Info_and_Feature_Bits
        */
        return (cpu_features() >> 26) & 1;
}


/*
        Verifica il supporto del sistema corrente, nello specifico verificando
        che il set di istruzioni SSE2 sia supportato.
*/
int check_system_support(compiler c)
{
        /* Queste macro verificano il supporto in fase di compilazione della libreria,
           avrei potuto infilarle più o meno ovunque ma qui mi è sembrato più
           pertinente.
        */
        #ifndef __linux__
                #warning Only Linux is supported!
                #ifdef _WIN32
                        #error Microsoft Windows is not supported!
                #endif
        #endif

        if(!supported_sse2())
        {
                compiler_SetError(c, ERROR_UNSUPPORTED_SYS, NULL);
                return 0;
        }
        
        return 1;
}
#endif

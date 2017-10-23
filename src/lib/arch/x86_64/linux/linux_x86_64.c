#if __x86_64__ // Tutto questo file va compilato solo se siamo su architettura x86_64!

#include <string.h>
#include <math.h>
#include <sys/mman.h>
#include "expreval_internal.h"
#include "arch/x86.h"

extern unsigned int cpu_features();
static void *create_executable_code_x86_linux(char *machine_code, int size);

#define ERROR(a,b)        do { compiler_SetError(c, a, b); return NULL; } while(0)

/* Definisco istruzioni fittizie pushq e popq:
        pushq XMMn:
                sub esp, 0x10           ; 0x10 == 16
                movq [esp], XMMn
        popq XMMn:
                movq XMMn, [esp]
                add esp, 0x10
   I registri XMM sono a 128 bit, ma qui noi siamo interessati
   solo a 64 bit (che sono quelli effettivamente spostati dalla
   movq). Modifichiamo esp comunque di 16 altrimenti otteniamo
   segmentation fault (penso per una questione di allineamento
   dello stack) */
#define PUSH_Q(n)        do { SUB_imm(ESP, 16); MOVQ(_ESP_(0), n); } while(0)
#define  POP_Q(n)        do { MOVQ(n, _ESP_(0)); ADD_imm(ESP, 16); } while(0)
/* Definisco istruzione fittizia per chiamata a funzione (ind. assoluto)
   ATTENZIONE CHE SPORCA EAX */
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

        /* Tabella dei valori literal: tutti i valori numerici inseriti direttamente
           nell'espressione vengono inseriti in questa tabella che verrà poi copiata
           nel codice (dopo la RET, così da non creare problemi). L'indirizzo della
           tabella verrà caricato dalla LEA qui sotto (il valore 0x11223344 è
           temporaneo e viene sostituito in fondo, per questo si tiene traccia della
           sua posizione con data_address_position) in RBX, così è possibile recuperare
           quei valori da [RBX+disp] (tutto questo perché non esiste una MOV che
           accetta come primo parametro un registro XMM* e come secondo parametro
           un valore immediato).
           Nella versione a 32 bit per qualche ragione tutto questo non funzionava,
           quindi sono ricordo ad un altro escamotage.
        */
        double literals[MAX_LITERALS_N]; int literals_i = 0;
        int data_address_position;

        int stack_size = 0;

        PUSH(EBP);                                                                // push rbp
        PUSH(EBX);                                                                // push rbx              ; Il registro rbx deve essere preservato
        SUB_imm(ESP, 0x8);                                                        // sub rsp, 0x8          ; Non ho capito bene perché ci voglia, ma a quanto pare
                                                                                  //                       ; senza crasha. Probabilmente è una qualche questione di
                                                                                  //                       ; allineamento dello stack (?)

        LEA(EBX, DISP(0x11223344));                                               // lea rbx, [0x11223344] ; 0x11223344 sarà cambiato dopo, quindi memorizzo la posizione
        data_address_position = i - 4;

        t = first_token;
        do
        {
                /* Se il token è un numero */
                if(t->type == TOKEN_NUMBER)
                {
                        /* Il valore andrà preso dalla lista dei literals */
                        MOVQ(XMM7, _EBX_(literals_i * 8));                        // movq xmm7, [rbx + {literals_i * 8}]
                        /* E inserito nello stack */
                        PUSH_Q(XMM7);                                             // pushq xmm7 ; In realtà questa istruzione non esiste (compilata come 2)
                        stack_size++;
                        /* Lo aggiungo alla tabella dei literals */
                        literals[literals_i++] = t->number_value;
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
                                MOVQ(XMM7, _EBX_(literals_i * 8));                // movq xmm7, [rbx + {literals_i * 8}]
                                PUSH_Q(XMM7);                                     // pushq xmm7
                                stack_size++;
                                literals[literals_i++] = constant_value;
                        }
                        else /* Se non è definita */
                                ERROR(ERROR_UNDEFINED_VAR, t);
                }
                else if(t->type == TOKEN_OPERATOR && op_args[t->op] == 2)
                {
                        int j;
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
                                        IDIV(ECX);                                // idiv ebx           ; Qui intendo proprio ecx, non rcx (anche su x86_64)
                                        CVTSI2SD(XMM6, EDX);                      // cvtsi2sd xmm6, edx
                                        break;
                                case OP_PWR:
                                        /* Salvo nello stack i parametri dell'espressione
                                           che andranno sovrascritti */
                                        for(j = 0; j < c->arg_count; j++)
                                                PUSH_Q(XMM(j));                   // pushq xmm{j}

                                        MOVQ(XMM0, XMM6);                         // movq xmm0, xmm6
                                        MOVQ(XMM1, XMM7);                         // movq xmm1, xmm7
                                        CALL_FUN(pow);                            // call pow
                                        MOVQ(XMM6, XMM0);                         // movq xmm6, xmm0

                                        /* Ripristino i parametri dell'espressione facendo POP
                                           (ovviamente in ordine inverso) */
                                        for(j = c->arg_count - 1; j >= 0; j--)
                                                POP_Q(XMM(j));                    // popq xmm{j}
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
                        unsigned long long int v = 1ULL << 63;
                        /* Operatore unario, deve esserci almeno un valore
                           nello stack */
                        if(stack_size < 1)
                                ERROR(ERROR_UNKNOWN, NULL);
                        switch(t->op)
                        {
                                case OP_NEG:
                                        POP_Q(XMM6);                              // popq xmm6
                                        stack_size--;
                                        MOVQ(XMM7, _EBX_(literals_i * 8));        // movq xmm7, [rbx + {literals_i * 8}]
                                        XORPD(XMM6, XMM7);                        // xorpd xmm6, xmm7
                                        PUSH_Q(XMM6);                             // pushq xmm6
                                        stack_size++;
                                        literals[literals_i++] = *((double*)&v);
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
                        int j;
                        function f = parser_GetFunction(c->p, t->text);
                        /* Non dovrebbe succedere mai: se una funzione non è definita
                           non viene neanche considerata una funzione dal parser e
                           viene trattata come una variabile */
                        if(f == NULL)
                                ERROR(ERROR_UNKNOWN, t);

                        if(stack_size < f->num_args)
                                ERROR(ERROR_ARGS, t);

                        /* In questo momento xmm{0-3} contengono i parametri
                           dell'espressione, quindi non possiamo ancora toccare
                           xmm da 0 a 3!
                           Faccio POP in altri registri (xmm{4-7}) */
                        for(j = f->num_args - 1; j >= 0; j--)
                                POP_Q(XMM(j + 4));                                // popq xmm{j + 4}
                        stack_size -= f->num_args;

                        /* Salvo nello stack i parametri dell'espressione
                           che andranno sovrascritti e inserisco in xmm{0-3}
                           i parametri della funzione chiamata */
                        for(j = 0; j < c->arg_count; j++)
                        {
                                PUSH_Q(XMM(j));                                   // pushq xmm{j}
                                MOVQ(XMM(j), XMM(j + 4));                         // movq xmm{j}, xmm{j + 4}
                        }

                        /* Chiamo la funzione e salvo il risultato in XMM6 */
                        CALL_FUN(f->ptr);                                         // call {f->ptr}
                        MOVQ(XMM6, XMM0);                                         // movq xmm6, xmm0

                        /* Ripristino i parametri dell'espressione facendo POP
                           (ovviamente in ordine inverso) */
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

        ADD_imm(ESP, 0x8);                                                        // add rsp, 0x8
        POP(EBX);                                                                 // pop rbx
        POP(EBP);                                                                 // pop rbp
        RET;                                                                      // ret

        /* A questo punto il codice della funzione è finito
           ed è già stata inserita la RET. Inserisco la tabella
           dei literals, contenente tutti i valori double costanti
           usati finora. */

        int data_address = i - (data_address_position + 4);
        memcpy(code + data_address_position, &data_address, sizeof(data_address));
        memcpy(code + i, literals, literals_i * 8); i+=literals_i * 8;

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
        if(!supported_sse2())
        {
                compiler_SetError(c, ERROR_UNSUPPORTED_SYS, NULL);
                return 0;
        }

        return 1;
}
#endif

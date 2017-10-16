/*
MIT License

Copyright (c) 2016 Luca Robbiano

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#ifndef EXPREVAL_H
#define EXPREVAL_H

#define STACK_LEN       512                     // Dimensione massima dello stack per la valutazione di espressioni RPN (interpretazione)
#define MAX_CODE_LEN    4096                    // Dimensione massima del codice eseguibile generato dal compilatore JIT
#define MAX_LITERALS_N  384                     // Numero massimo di valori literals (anche uguali tra loro) presenti in un'espressione (ha effetto solo su x86_64)


#define LIBEXPREVAL_V   "0.8.6"
/*
        La tabella seguende (composta da un'enumerazione e 3 array) definisce tutte le informazioni necessarie sugli
        operatori (simbolo, priorità, associatività e numero di operandi)
*/
enum {LTR, RTL}; /* LEFT TO RIGHT, RIGHT TO LEFT */
#define OPERATORS                       "+"     "-"     "*"     "/"     "%"     "^"     "("     ")"    "," //   "-"     "+"
enum                                   {OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD, OP_PWR, OP_LBR, OP_RBR, OP_SEP, OP_NEG, OP_POS};
static const int op_prec[] =           {1,      1,      2,      2,      2,      4,      0,      0,      0,      3,      3     };
static const int op_associativity[] =  {LTR,    LTR,    LTR,    LTR,    LTR,    RTL,    0,      0,      0,      LTR,    LTR   };
static const int op_args[] =           {2,      2,      2,      2,      2,      2,      0,      0,      0,      1,      1     };

/* Cose tokenizer.c */

enum {  /* Usati in tokenizer.c */  
        TOKEN_UNDEFINED,                        // Il tipo del token non è noto / non è definito (per esempio per token fittizio di inizio linked list)
        TOKEN_NUMBER,                           // Il token rappresenta un numero
        TOKEN_OPERATOR,                         // Il token rappresenta un operatore
        TOKEN_IDENTIFIER,                       // Il token rappresenta il nome di una variabile, di una costante o di una funzione
        /* Usati in parser.c, dopo  */
        TOKEN_VARIABLE,                         // Il token rappresenta il nome di una variabile o di una costante
        TOKEN_FUNCTION                          // Il token rappresenta il nome di una funzione
     };
typedef struct _token *token;
struct _token {
        int type;                               // Tipo di token (vedi enumerazione sopra)
        char *text;                             // Stringa di testo corrispondente al token
        int position;                           // Posizione nell'espressione del primo carattere del token
        int length;                             // Numero di caratteri che compongon il token
        token next;                             // Token successivo nella linked list
        
        /* Questi usati dopo la creazione dei token, in fase di parsing */
        double number_value;                    // Valore numerico del token (se type == TOKEN_NUMBER)
        int op;                                 // Identificativo (da enumerazione sopra) dell'operatore (se type == TOKEN_OPERATOR)
};

token tokenize(char *string, int *token_count);
token token_New(int type, int pos);
void token_Free(token t, int free_next);
int is_valid_identifier(char *str);

/* Cose parser.c */

enum {  
        ERROR_STACKOVERFLOW = 1,                // Errore di superamento dei limiti dello stack
        ERROR_NOT_IMPLEMENTED,                  // Funzione non ancora implementata
        ERROR_INVALID_NUMBER,                   // Valore numerico non valido (es 3.5.6 viene identificato come token numerico da tokenizer() ma non è valido)
        ERROR_MISSING_BRACKET,                  // Manca una parentesi da qualche parte
        ERROR_UNDEFINED_VAR,                    // Variabile o costante non definita
        ERROR_UNKNOWN,                          // Errore sconosciuto
        ERROR_ARGS,                             // Errore nel numero di argomenti passati ad una funzione
        ERROR_UNSUPPORTED_SYS,                  // Sistema non supportato (in fase di compilazione su x86 se non è supportato l'instruction set SSE2)
        ERROR_SYSTEM                            // Errore del kernel del sistema operativo (per esempio si rifiuta di rendere eseguibile il codice appena compilato)
     };

typedef struct _variable *variable;
struct _variable {
        char *name;                             // Nome della variabile
        double value;                           // Valore della variabile
        variable next;                          // Variabile successiva nella linked list
};

typedef struct _function *function;
struct _function {
        char *name;                             // Nome della funzione
        void *ptr;                              // Puntatore alla funzione
        int num_args;                           // Numero di argomenti della funzione (da 0 a 4)
        function next;                          // Funzione successiva nella linked list
};

typedef struct _parser *parser;
struct _parser {
        int errorcode;                          // Codice di errore (0 se non c'è stato nessun errore)
        int errorpos;                           // Posizione dell'errore (-> del token che l'ha generato) nell'espressione
        char *errorstr;                         // Messaggio di errore
        variable first_var;                     // Prima variabile della linked list di variabili (ultima variabile inserita)
        function first_function;                // Prima funzione nella linked list di funzioni (ultima funzione inserita)
};


typedef struct _expression *expression;
struct _expression {
        token first;                            // Primo token (linked list) dell'espressione
        int num_tokens;                         // Numero di tokens dell'espressione
        int num_vars;                           // Numero di variabili (perché l'ho messo? Serve davvero?)
};

expression expression_Parse(parser p, char *expressionstring);
void expression_Free(expression expr);
parser parser_New();
int parser_SetError(parser p, int errorcode, token t);
char *parser_GetError(parser p, int *errorcode, int *errorpos);
void parser_ClearError(parser p);
void parser_Free(parser p);
int parser_GetVariable(parser p, char *name, double *value);
int parser_SetVariable(parser p, char *name, double value);
function parser_GetFunction(parser p, char *name);
int parser_RegisterFunction(parser p, char *name, void *ptr, int num_args);

/* Cose eval.c */
double expression_Eval(parser p, expression e);

/* Cose x86_compiler.c */
typedef struct _compiler *compiler;
typedef double (*cfunction0)();
typedef double (*cfunction1)(double);
typedef double (*cfunction2)(double, double);
typedef double (*cfunction3)(double, double, double);
typedef double (*cfunction4)(double, double, double, double);
struct _compiler {
        parser p;                               // Parser associato al compilatore
        char *arg_names[4];                     // Permette di associare all'indice di un argomento il suo nome
        int arg_count;                          // Numero di argomenti della funzione/espressione che si vuole compilare
};
compiler compiler_New();
void compiler_Free(compiler c);
int compiler_RegisterFunction(compiler c, char *name, void *ptr, int num_args);
int compiler_SetConstant(compiler c, char *name, double value);
int compiler_MapArgument(compiler c, char *arg_name, int index);
char *compiler_GetError(compiler c, int *errorcode, int *errorpos);
int compiler_SetError(compiler c, int errorcode, token t);
void *compiler_CompileFunction(compiler c, char *expr);
int compiler_GetArgumentIndex(compiler c, char *arg_name);

// Controllo che stiamo compilando per x86_32 o x86_64
#if (!__x86_64__) && (!__i386__)
        #error Unsupported architecture or compiler
#endif
#endif

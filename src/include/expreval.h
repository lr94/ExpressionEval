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

#define LIBEXPREVAL_V   "0.8.5"

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

typedef struct _parser *parser;
typedef struct _expression *expression;
typedef struct _compiler *compiler;
typedef double (*cfunction0)();
typedef double (*cfunction1)(double);
typedef double (*cfunction2)(double, double);
typedef double (*cfunction3)(double, double, double);
typedef double (*cfunction4)(double, double, double, double);

parser parser_New();
void parser_Free(parser p);
char *parser_GetError(parser p, int *errorcode, int *errorpos);
int parser_SetVariable(parser p, char *name, double value);
int parser_RegisterFunction(parser p, char *name, void *ptr, int num_args);
int parser_GetVariable(parser p, char *name, double *value);

expression expression_Parse(parser p, char *expressionstring);
void expression_Free(expression expr);
double expression_Eval(parser p, expression e);

compiler compiler_New();
void compiler_Free(compiler c);
char *compiler_GetError(compiler c, int *errorcode, int *errorpos);
int compiler_SetConstant(compiler c, char *name, double value);
int compiler_RegisterFunction(compiler c, char *name, void *ptr, int num_args);
int compiler_MapArgument(compiler c, char *arg_name, int index);
void *compiler_CompileFunction(compiler c, char *expr);
#endif

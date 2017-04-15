#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "expreval_internal.h"

void *compile_function_internal(compiler c, token first_token, int *size);
int check_system_support(compiler c);

/*
        Crea un nuovo oggetto compiler
*/
compiler compiler_New()
{
        int i;
        compiler c = malloc(sizeof(*c));
        c->p = parser_New();        // Include un oggetto parser
        c->arg_count = 0;
        for(i = 0; i < 4; i++)
                c->arg_names[i] = NULL;
        return c;
}

/*
        Distrugge un oggetto di tipo compiler
*/
void compiler_Free(compiler c)
{
        int i;
        parser_Free(c->p);
        for(i = 0; i < 4; i++)
                if(c->arg_names[i] != NULL)
                        free(c->arg_names[i]);
        free(c);
}

/*
        Permette di definire una funzione.
                name indica il nome della funzione
                ptr indica il puntatore alla funzione
                num_args indica il numero di argomenti
        Restituisce 1 in caso di successo, 0 altrimenti
        es:
                compiler_RegisterFunction(c, "sin", sin, 1);
*/
int compiler_RegisterFunction(compiler c, char *name, void *ptr, int num_args)
{
        // Il nome non deve essere già stato registrato come argomento della funzione
        if(compiler_GetArgumentIndex(c, name) >= 0)
                return 0;
        
        // Registro la funzione nell'oggetto parser
        return parser_RegisterFunction(c->p, name, ptr, num_args);
}

/*
        Permette di definire una costante.
                name indica il nome della costante
                double indica il valore che le si vuole attribuire
        Restituisce 1 in caso di successo e 0 altrimenti (per ora solo
        nel caso in cui il nome scelto sia già stato associato ad un 
        parametro dell'espressione)
*/
int compiler_SetConstant(compiler c, char *name, double value)
{
        if(compiler_GetArgumentIndex(c, name) >= 0)
                return 0;

        return parser_SetVariable(c->p, name, value);
}

/*
        Permette di associare una variabile dell'espressione ad un argomento
        della funzione che si va a compilare.
                arg_name indica il nome della variabile
                index indica l'indice dell'argomento (partendo da 0)
        Poiché il numero massimo di argomenti è 4, il massimo valore di index è 3.
        
        Es:
                Espressione con un solo argomento, cf(x):
                compiler_MapArgument(c, "x", 0);
                cf = compiler_CompileFunction(c, "1/(1+x^2)");
                
                Espressione con due argomenti, cf(x, y)
                compiler_MapArgument(c, "x", 0);
                compiler_MapArgument(c, "y", 1);
                cf = compiler_CompileFunction(c, "(1+y^2)/(1+x^2)");   

                Espressione con due argomenti, cf(y, x)
                compiler_MapArgument(c, "x", 1);
                compiler_MapArgument(c, "y", 0);
                cf = compiler_CompileFunction(c, "(1+y^2)/(1+x^2)");               
*/
int compiler_MapArgument(compiler c, char *arg_name, int index)
{
        if(index < 0 || index > 3)
                return 0;                                // Indice argomento non valido
        if(compiler_GetArgumentIndex(c, arg_name) != -1)
                return 0;                                // Parametro già mappato
        if(parser_GetVariable(c->p, arg_name, NULL))
                return 0;                                // Parametro già definito come costante
        if(!is_valid_identifier(arg_name))
                return 0;                                // Nome non valido
        c->arg_names[index] = strdup(arg_name);
        // Aggiorno il numero di argomenti della funzione che vogliamo compilare
        if(index + 1 > c->arg_count)
                c->arg_count = index + 1;
        return 1;
}

/*
        Restituisce l'indice di un argomento a partire dal nome.
        -1 se al nome indicato non è assoiato alcun argomento. 
*/
int compiler_GetArgumentIndex(compiler c, char *arg_name)
{
        int i;
        for(i = 0; i < 4; i++)
                if(c->arg_names[i] != NULL)
                        if(strcmp(c->arg_names[i], arg_name) == 0)
                                return i;
        return -1;
}

/*
        Imposta l'errore di un oggetto di tipo compiler.
        errorcode indica il codice errore
        token specifica il token che ha generato l'errore (può essere NULL)
        
        Non usa la parser_SetError sull'oggetto parser (anche se poi tutti i
        dati sull'errore sono memorizzati in esso) per poter gestire i vari
        messaggi di errore specifici della compilazione.
*/
int compiler_SetError(compiler c, int errorcode, token t)
{
        parser p = c->p;
        p->errorcode = errorcode;
        char *txt = NULL;
        if(t != NULL)
        {
                p->errorpos = t->position;
                txt = t->text;
        }
        p->errorstr = malloc(256*sizeof(char));
        
        switch(errorcode)
        {
                case ERROR_NOT_IMPLEMENTED:
                        sprintf(p->errorstr, "Compilation error: feature not implemented at \"%s\" (%d)", txt, p->errorpos);
                        break;
                case ERROR_UNDEFINED_VAR:
                        sprintf(p->errorstr, "Compilation error: identifier \"%s\" (%d) was not defined as constant nor mapped as argument", txt, p->errorpos);
                        break;
                case ERROR_SYSTEM:
                        sprintf(p->errorstr, "A system error occurred while compiling");
                        break;
                case ERROR_UNSUPPORTED_SYS:
                        sprintf(p->errorstr, "Your CPU does not seem to support SSE2 instruction set.");
                        break;
                default:
                        if(txt != NULL)
                                sprintf(p->errorstr, "An unknown error occurred while compiling at \"%s\" (%d)", txt, p->errorpos);
                        else
                                sprintf(p->errorstr, "An unknown error occurred while compiling");
        }
        return errorcode;
}

/*
        Restituisce una stringa con un messaggio d'errore se c'è stato un errore
        durante la compilazione dell'espressione.
        Se non si sono verificati errori restiuisce NULL.
        Se diversi da NULL errorcode e errorpos consentono di ottenere il codice
        d'errore e la sua posizione (se definita).
*/
char *compiler_GetError(compiler c, int *errorcode, int *errorpos)
{
        return parser_GetError(c->p, errorcode, errorpos);
}

/*
        Cancella l'eventuale errore rimasto in memoria dalla compilazione precedente.
*/
void compiler_ClearError(compiler c)
{
        parser_ClearError(c->p);
}

/*
        Effettua la compilazione di un'espressione passata come argomento in forma testuale.
        Restituisce NULL in caso di errore, altrimenti viene restituita la funzione.
*/
void *compiler_CompileFunction(compiler c, char *expr)
{
        expression e;
        void *code;

        compiler_ClearError(c);        
        
        if(!check_system_support(c))
                return NULL;
        
        e = expression_Parse(c->p, expr);
        
        if(parser_GetError(c->p, NULL, NULL) != NULL)
                return NULL;

        code = compile_function_internal(c, e->first, NULL);
        expression_Free(e);
        
        return code;
}


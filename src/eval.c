#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "expreval_internal.h"

/*
        Valuta un'espressione interpretandola (niente compilazione).
        L'espressione è già stata convertita in notazione postfissa 
        dalla expression_Parse.
*/
double expression_Eval(parser p, expression e)
{
        // Stack (qui sarebbe da migliorare, possibile stack overflow)
        double stack[STACK_LEN];        int stack_i = 0;
        // Primo elemento della lista di token
        token t = e->first;
        
        // Scorro tutti i token
        do
        {
                // Se il token corrente è un numero lo inserisco semplicemente in cima allo stack
                if(t->type == TOKEN_NUMBER)
                        stack[stack_i++] = t->number_value;
                /* Se è una variabile la cerco tra le variabili definite, se è definita metto
                   il valore sullo stack, altrimenti genero un errore. */
                else if(t->type == TOKEN_VARIABLE)
                {
                        double val;
                        if(!parser_GetVariable(p, t->text, &val))
                        {
                                parser_SetError(p, ERROR_UNDEFINED_VAR, t);
                                return NAN;
                        }
                        stack[stack_i++] = val;
                }
                // Se è un operatore distinguo i casi degli operatori binari dagli unari (+ e -)
                else if(t->type == TOKEN_OPERATOR)
                {
                        double first_operand, second_operand, result;
                        switch(op_args[t->op])
                        {
                                // Caso operatore unario
                                case 1:
                                        /* POP dallo stack dell'unico operando e calcolo del
                                           risultato */
                                        first_operand = stack[--stack_i];
                                        switch(t->op)
                                        {
                                                case OP_NEG:
                                                        result = -first_operand;
                                                        break;
                                                case OP_POS:
                                                        result = first_operand;
                                                        break;
                                        }
                                        break;
                                // Caso operatore binario
                                case 2:
                                        /* POP dallo stack dei due operandi e calcolo del risultato */
                                        second_operand = stack[--stack_i];
                                        first_operand = stack[--stack_i];
                                        switch(t->op)
                                        {
                                                case OP_ADD:
                                                        result = first_operand + second_operand;
                                                        break;
                                                case OP_SUB:
                                                        result = first_operand - second_operand;
                                                        break;
                                                case OP_MUL:
                                                        result = first_operand * second_operand;
                                                        break;
                                                case OP_DIV:
                                                        result = first_operand / second_operand;
                                                        break;
                                                case OP_MOD:
                                                        /* Qui sarebbe meglio usare (long) per scongiurare per quanto
                                                           possibile un overflow, ma per mantenere uniformità con la
                                                           versione compilata ho usato int */
                                                        result = (double)((int)first_operand % (int)second_operand);
                                                        break;
                                                case OP_PWR:
                                                        result = pow(first_operand, second_operand);
                                                        break;
                                        }
                                        break;
                        }
                        // PUSH del risultato sullo stack.
                        stack[stack_i++] = result;
                }
                // Se l'operatore è una funzione
                else if(t->type == TOKEN_FUNCTION)
                {
                        double result = 0.0;
                        /* Recupero l'oggetto function associato alla funzione in questione
                           (dovrebbe esistere per forza o non l'avremmo identificata come
                           funzione) */
                        function f = parser_GetFunction(p, t->text);
                        int num_args = f->num_args;
                        int i, j;
                        // Array degli argomenti della funzione
                        double args[4];
                        
                        /* Se sullo stack non ci sono abbastanza valori la funzione è stata
                           sicuramente chiamata con un numero insufficiente di parametri.
                           Notare però che il fatto che ci siano abbastanza valori non significa
                           che sia stata chiamata col giusto numero di parametri...questo controllo
                           andrebbe fatto prima della conversione in RPN (o durante) */
                        if(stack_i < num_args)
                        {
                                parser_SetError(p, ERROR_ARGS, t);
                                return NAN;
                        }
                        
                        /* Faccio POP di n valori dallo stack e li inserisco in ordine inverso nel
                           vettore degli argomenti (il valore in cima allo stack è l'ultimo argomento) */
                        for(i = j = 0; i < num_args; i++)
                                args[num_args - (1 + j++)] = stack[--stack_i];

                        /* A seconda del numero di argomenti creo un puntatore a funzione adatto,
                           lo inizializzo con f->ptr (che è un generico puntatore void*) e lo chiamo. */
                        if(num_args == 0)
                        {
                                double (*ptr)() = f->ptr;
                                result = ptr();
                        }
                        else if(num_args == 1)
                        {
                                double (*ptr)(double) = f->ptr;
                                result = ptr(args[0]);
                        }
                        else if(num_args == 2)
                        {
                                double (*ptr)(double, double) = f->ptr;
                                result = ptr(args[0], args[1]);
                        }
                        else if(num_args == 3)
                        {
                                double (*ptr)(double, double, double) = f->ptr;
                                result = ptr(args[0], args[1], args[2]);
                        }
                        else if(num_args == 4)
                        {
                                double (*ptr)(double, double, double, double) = f->ptr;
                                result = ptr(args[0], args[1], args[2], args[3]);
                        }
                        // PUSH del risultato sullo stack
                        stack[stack_i++] = result;
                }
        } while((t = t->next) != NULL); // Passo al token successivo
        
        // Se alla fine della procedura sullo stack non è rimasto un solo valore c'è stato un qualche errore
        if(stack_i != 1)
        {
                parser_SetError(p, ERROR_UNKNOWN, NULL);
                return NAN;
        }
        
        // Restituisco l'unico valore rimasto sullo stack
        return stack[0];
}


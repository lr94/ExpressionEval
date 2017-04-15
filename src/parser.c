#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "expreval_internal.h"

static int treat_tokens(parser p, token first);
static expression expression_parse_internal(parser p, token first);
static variable variable_New(char *name, double value);
static void variable_Free(variable v, int free_next);
static function function_New(char *name, void *fun, int num_args);
static void function_Free(function f, int free_next);
static void token_dbg(token t);

/*
        Crea un nuovo oggetto di tipo parser
*/
parser parser_New()
{
        parser p = malloc(sizeof(*p));
        p->errorstr = NULL;
        p->errorcode = 0;
        p->errorpos = -1;
        p->first_var = NULL;
        p->first_function = NULL;

        return p;
}

/*
        Imposta l'errore di un oggetto di tipo parser.
        errorcode indica il codice errore
        token specifica il token che ha generato l'errore (può essere NULL)
*/
int parser_SetError(parser p, int errorcode, token t)
{
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
                case ERROR_STACKOVERFLOW:
                        sprintf(p->errorstr, "Stack overflow at \"%s\" (%d)", txt, p->errorpos);
                        break;
                case ERROR_NOT_IMPLEMENTED:
                        sprintf(p->errorstr, "Not implemented function at \"%s\" (%d)", txt, p->errorpos);
                        break;
                case ERROR_INVALID_NUMBER:
                        sprintf(p->errorstr, "\"%s\" (%d) is not a valid number", txt, p->errorpos);
                        break;
                case ERROR_MISSING_BRACKET:
                        sprintf(p->errorstr, "Missing bracket at \"%s\" (%d)", txt, p->errorpos);
                        break;
                case ERROR_UNDEFINED_VAR:
                        sprintf(p->errorstr, "Unknown identifier \"%s\" (%d)", txt, p->errorpos);
                        break;
                case ERROR_ARGS:
                        sprintf(p->errorstr, "Error calling function \"%s\" (%d)", txt, p->errorpos);
                        break;
                default:
                        if(txt != NULL)
                                sprintf(p->errorstr, "Unknown error at \"%s\" (%d)", txt, p->errorpos);
                        else
                                sprintf(p->errorstr, "Unknown error.");
        }
        return errorcode;
}

/*
        Restituisce una stringa con un messaggio d'errore se c'è stato un errore
        durante il parsing dell'espressione.
        Se non si sono verificati errori restiuisce NULL.
        Se diversi da NULL errorcode e errorpos consentono di ottenere il codice
        d'errore e la sua posizione (se definita).
*/
char *parser_GetError(parser p, int *errorcode, int *errorpos)
{
        if(errorcode != NULL)
                *errorcode = p->errorcode;
        if(p->errorcode != 0)
        {
                if(errorpos != NULL)
                        *errorpos = p->errorpos;
                return p->errorstr;
        }
        return NULL;
}

/*
        Cancella l'eventuale errore rimasto in memoria dall'elaborazione precedente.
*/
void parser_ClearError(parser p)
{
        p->errorcode = 0;
        p->errorpos = -1;
        free(p->errorstr);
        p->errorstr = NULL;
}

/*
        Crea un nuovo oggetto di tipo variable.
*/
static variable variable_New(char *name, double value)
{
        variable v = malloc(sizeof(*v));
        v->name = strdup(name);
        v->value = value;
        v->next = NULL;
        return v;
}

/*
        Distrugge un oggetto di tipo variable.
        free_next indica se distruggere anche l'eventuale altra variabile
        ad esso collegata (e così via ricorsivamente)
*/
static void variable_Free(variable v, int free_next)
{
        if(v->next != NULL && free_next)
                variable_Free(v->next, 1);
        if(v->name != NULL)
                free(v->name);
        free(v);
}

/*
        Crea un nuovo oggetto di tipo function
*/
static function function_New(char *name, void *fun, int num_args)
{
        function f = malloc(sizeof(*f));
        f->name = strdup(name);
        f->ptr = fun;
        f->num_args = num_args;
        f->next = NULL;
        return f;
}

/*
        Distrugge un oggetto di tipo function.
        free_next indica se distruggere anche l'eventuale altra funzione
        ad esso collegata (e così via ricorsivamente)
*/
static void function_Free(function f, int free_next)
{
        if(f->next != NULL && free_next)
                function_Free(f->next, 1);
        if(f->name != NULL)
                free(f->name);
        free(f);
}

/*
        Distrugge un oggetto di tipo parser.
*/
void parser_Free(parser p)
{
        if(p->first_var != NULL)
                variable_Free(p->first_var, 1);
        if(p->first_function != NULL)
                function_Free(p->first_function, 1);
        if(p->errorstr != NULL)
                free(p->errorstr);
        free(p);
}

/*
        Effettua alcune operazioni preliminari sui tokens (restituisce errorcode se necessario)
*/
static int treat_tokens(parser p, token first)
{
        token t = first;
        int i, count, len;
        /* Tiene traccia dell'ultimo token elaborato */
        token latest = NULL;
        do
        {
                switch(t->type)
                {
                        /* Se il token è un numero controllo che non contenga più di un ".", poi converto in double */
                        case TOKEN_NUMBER:
                                len = strlen(t->text);
                                count = 0;
                                for(i = 0; i < len; i++)
                                        if(t->text[i] == '.')
                                                count++;
                                if(count > 1)
                                        return parser_SetError(p, ERROR_INVALID_NUMBER, t);
                                t->number_value = atof(t->text);
                                break;
                        /* Se il token è un operatore assegno al campo op l'id dell'operatore */
                        case TOKEN_OPERATOR:
                                for(i = 0; i < strlen(OPERATORS); i++)
                                        if(t->text[0] == OPERATORS[i])
                                                t->op = i;
                                /*
                                        Questa parte serve per distinguere gli operatori unari + e - dai binari:
                                        se è un - e si trova all'inizio o segue un altro operatore diverso da ")"
                                        allora l'operatore è unario (OP_NEG/OP_POS) e non binario (OP_SUB/OP_ADD)
                                */
                                if(t->op == OP_SUB)
                                {
                                        if(latest == NULL)
                                                t->op = OP_NEG;
                                        else if(latest->type == TOKEN_OPERATOR && latest->op != OP_RBR)
                                                t->op = OP_NEG;
                                }
                                if(t->op == OP_ADD)
                                {
                                        if(latest == NULL)
                                                t->op = OP_POS;
                                        else if(latest->type == TOKEN_OPERATOR && latest->op != OP_RBR)
                                                t->op = OP_POS;        
                                }
                                break;
                        case TOKEN_IDENTIFIER:
                                if(parser_GetFunction(p, t->text) != NULL)
                                        t->type = TOKEN_FUNCTION;
                                else
                                        t->type = TOKEN_VARIABLE;
                                break;
                }
                latest = t;
        } while((t = t->next) != NULL);
        return 0;
}

/*
        Crea un clone del token passato come parametro (e dell'eventuale testo in esso contenut).
        Il clone restituito ha il campo "next" settato a NULL, quindi non è legato a nessun altro token.
*/
token token_clone(token t)
{
        token c = malloc(sizeof(*c));
        memcpy(c, t, sizeof(*c));
        c->next = NULL;
        if(t->text != NULL)
                c->text = strdup(t->text);
        return c;
}

/*
        Crea un oggetto di tipo expression a partire da una stringa
        contenente un'espressione.
        In caso di errore restituisce NULL.
*/
expression expression_Parse(parser p, char *expressionstring)
{
        int token_count;
        /* Divide l'espressione in token. t conterrà il primo token della lista */
        token t = tokenize(expressionstring, &token_count);
        /* Converte la lista concatenata di token in una lista di token
           rappresentante l'espressione in forma postfissa (RPN) */
        expression e = expression_parse_internal(p, t);
        token_Free(t, 1);
        return e;
}

/*
        Implementazione dell'algoritmo shunting-yard di Dijkstra.
        Crea un oggetto expression convertendo l'espressione in forma
        postfissa.
        La descrizione dell'algoritmo qui utilizzato si trova su
        https://en.wikipedia.org/wiki/Shunting-yard_algorithm#The_algorithm_in_detail
*/
static expression expression_parse_internal(parser p, token first)
{
        /* Stack operatori */
        token op_stack[STACK_LEN];        int op_stack_i = 0;
        /* Coda uscita */
        token q_first;
        token q;
        token current_t = first;
        expression expr;
        int queue_count, var_count;
        
        parser_ClearError(p);
        
        if(first == NULL)
                return NULL;
        
        if(treat_tokens(p, first)) /* Parentesi aggiunte solo per non far rompere -Wall -pedantic */
                return NULL;
        
        q_first = token_New(TOKEN_UNDEFINED, -1);
        q = q_first;
        var_count = queue_count = 0;
        
        do
        {
                token t = token_clone(current_t);
                
                if(t->type == TOKEN_NUMBER || t->type == TOKEN_VARIABLE)
                {
                        q->next = t;
                        q = t;
                        queue_count++;
                        if(t->type == TOKEN_VARIABLE)
                                var_count++;
                }
                else if(t->type == TOKEN_FUNCTION)
                        op_stack[op_stack_i++] = t; // PUSH function nello stack
                else if(t->type == TOKEN_OPERATOR)
                {
                        if(t->op == OP_LBR) /* ( */
                                op_stack[op_stack_i++] = t; // PUSH operator nello stack
                        else if(t->op == OP_RBR || t->op == OP_SEP) /* ) , */
                        {
                                int found_left = 0; /* Abbiamo trovato l'aperta parentesi? */
                                while(op_stack_i > 0)
                                {
                                        token o2 = op_stack[--op_stack_i]; // POP operator dallo stack
                                        if(o2->op == OP_LBR)
                                        {
                                                found_left = 1;
                                                if(t->op == OP_RBR)
                                                        token_Free(o2, 0);
                                                else
                                                        op_stack_i++; /*  Se il token che stiamo lavorando è "," l'aperta "(" deve restare sullo stack */
                                                break;
                                        }
                                        q->next = o2;
                                        q = o2;
                                        queue_count++;
                                }
                                if(op_stack == 0 && !found_left)
                                {
                                        parser_SetError(p, ERROR_MISSING_BRACKET, t);
                                        token_Free(q_first, 1);
                                        return NULL;
                                }
                                
                                if(t->op == OP_RBR && op_stack_i > 0)
                                {
                                        if(op_stack[op_stack_i-1]->type == TOKEN_FUNCTION) /* Il token in cima allo stack è il nome di una funzione? */
                                        {
                                                token o2 = op_stack[--op_stack_i]; // POP funzione dallo stack
                                                q->next = o2;
                                                q = o2;
                                                queue_count++;
                                        }
                                }
                                
                                token_Free(t, 0);
                        }
                        else
                        {
                                while(op_stack_i > 0)
                                {
                                        token o2 = op_stack[--op_stack_i]; // POP operator dallo stack
                                        if(((op_associativity[t->op] == LTR) && (op_prec[t->op] <= op_prec[o2->op]))
                                                || ((op_associativity[t->op] == RTL) && (op_prec[t->op] < op_prec[o2->op])))
                                        {
                                                q->next = o2;
                                                q = o2;
                                                queue_count++;
                                        }
                                        else
                                        {
                                                op_stack_i++; /* Rimettiamo l'operatore dov'era nello stack */
                                                break;
                                        }
                                }
                                op_stack[op_stack_i++] = t; // PUSH operator nello stack
                        }
                }
        } while((current_t = current_t->next) != NULL);
        
        while(op_stack_i > 0)
        {
                token o2 = op_stack[--op_stack_i]; // POP operator dallo stack
                if(o2->op == OP_LBR || o2->op == OP_RBR)
                {
                        parser_SetError(p, ERROR_MISSING_BRACKET, o2);
                        token_Free(q_first, 1);
                        return NULL;
                }
                q->next = o2;
                q = o2;
                queue_count++;
        }
        
        /*
        current_t = q_first;
        while(current_t = current_t->next)
                token_dbg(current_t);
       // exit(0); */
        
        
        expr = malloc(sizeof(*expr));
        expr->first = q_first->next;
        expr->num_tokens = queue_count;
        expr->num_vars = var_count;
        token_Free(q_first, 0);
        return expr;
}

/*
        Distrugge un oggetto di tipo expression.
*/
void expression_Free(expression expr)
{
        if(expr->first != NULL)
                token_Free(expr->first, 1);
        free(expr);
}

/*
        Permette di ottenere il valore di una variabile.
        name è il nome della variabile, value è un puntatore ad
        una variabile double in cui salvare il valore.
        Restituisce 1 se la variabile è definita, 0 altrimenti.
*/
int parser_GetVariable(parser p, char *name, double *value)
{
        variable current_var = p->first_var;
        while(current_var != NULL)
        {
                if(strcmp(current_var->name, name) == 0)
                {
                        if(value != NULL)
                                *value = current_var->value;
                        return 1;
                }
                current_var = current_var->next;
        }
        return 0;
}

/*
        Imposta il valore di una variabile.
        name indica il nome della variabile, value il valore
        che si intende assegnare.
        
        Restituisce 1 in caso di successo, 0 altrimenti
*/
int parser_SetVariable(parser p, char *name, double value)
{
        variable new_var;
        // Cerca la variabile tra quelle già definite
        variable current_var = p->first_var;
        
        if(!is_valid_identifier(name))
                return 0;
        
        while(current_var != NULL)
        {
                if(strcmp(current_var->name, name) == 0)
                {
                        // Se la trova imposta il valore ed esce
                        current_var->value = value;
                        return 1;
                }
                current_var = current_var->next;
        }
        // Definisce la variabile nuova e la aggiungo in testa alla lista
        new_var = variable_New(name, value);
        new_var->next = p->first_var;
        p->first_var = new_var;
        return 1;
}

/*
        Permette di ottenere un oggetto function a partire dal nome.
        Restituisce l'oggetto se la funzione è definita, NULL altrimenti.
*/
function parser_GetFunction(parser p, char *name)
{
        function current_fun = p->first_function;        
        while(current_fun != NULL)
        {
                if(strcmp(current_fun->name, name) == 0)
                        return current_fun;
                current_fun = current_fun->next;
        }
        return NULL;
}

/*
        Permette di definire una funzione.
                name indica il nome della funzione
                ptr indica il puntatore alla funzione
                num_args indica il numero di argomenti
        Restituisce 1 in caso di successo, 0 altrimenti
        es:
                parser_RegisterFunction(p, "sin", sin, 1);
*/
int parser_RegisterFunction(parser p, char *name, void *ptr, int num_args)
{
        function new_fun;
        function current_fun = p->first_function;

        if(num_args > 4 || num_args < 0)
                return 0; // Numero di argomenti non valido
        
        if(!is_valid_identifier(name))
                return 0; // Nome della funzione non valido
        
        // Se la funzione è già definita la ridefinisco
        while(current_fun != NULL)
        {
                if(strcmp(current_fun->name, name) == 0)
                {
                        current_fun->ptr = ptr;
                        current_fun->num_args = num_args;
                        return 1; // funzione ridefinita, esco
                }
                current_fun = current_fun->next;
        }
        // La funzione è nuova, creo l'oggetto e lo aggiungo in testa alla lista
        new_fun = function_New(name, ptr, num_args);
        new_fun->next = p->first_function;
        p->first_function = new_fun;
        
        return 1;
}


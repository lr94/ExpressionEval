#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "expreval_internal.h"

/*
        Restituisce 1 se il carattere passato come argomento
        deve essere interpretato come un operatore, 0 altrimenti.
        La lista degli operatori è definita in expreval_internal.h
*/
int isoperator(char c)
{
        int i;
        for(i = 0; i < strlen(OPERATORS) ; i++)
                if(c == OPERATORS[i])
                        return 1;
        return 0;
}

/*
        Inizializza un oggetto di tipo token
*/
token token_New(int type, int pos)
{
        token t = malloc(sizeof(*t));
        t->next = NULL;
        t->text = NULL;
        t->position = pos;
        t->length = 0;
        t->op = 0;
        t->type = type;
        
        return t;
}

/*
        Distrugge un oggetto di tipo token
*/
void token_Free(token t, int free_next)
{
        if(t->next != NULL && free_next)
                token_Free(t->next, 1);
        if(t->text != NULL)
                free(t->text);
        free(t);
}

/*
        Divide un'espressione contenuta in una stringa in tokens.
        string: espressione da dividere in token
        token_count: puntatore a un intero che conterrà il numero di token
        (può essere NULL se non si è interessati).
        Restituisce il primo token della lista (ogni token contiene
        un puntatore al successivo).
        La lista di tokens così generata non è ancora pronta: prima di
        procedere con qualsiasi operazione comunque bisognerà eseguire
        alcune operazioni preliminari con la treat_tokens di parser.c,
        che comunque viene già chiamata dalla funzione che effettua la
        conversione in notazione postfissa.
*/
token tokenize(char *string, int *token_count)
{
        int len, i, j, current_type, token_counter;
        token t, first;
        len = strlen(string);
        
        if(len == 0)
                return NULL;
        
        // Token fittizio di apertura
        t = token_New(TOKEN_UNDEFINED, -1);
        first = t;
        // Stato corrente
        current_type = TOKEN_UNDEFINED;
        
        for(i = j = 0; i < len; i++)
        {
                char c=string[i];
                int newtoken_type = TOKEN_UNDEFINED;
                // Se c'è uno spazio il token corrente termina, al prossimo non-spazio ne inizia un altro
                if(c == ' ')
                {
                        t->length = i-t->position;
                        current_type = TOKEN_UNDEFINED;
                }
                // Se non ci trovavamo in un token numerico (o alfanumerico) e c'è una cifra inizia un numero
                else if((c == '.' || isdigit(c)) && current_type != TOKEN_NUMBER && current_type != TOKEN_IDENTIFIER)
                        newtoken_type = TOKEN_NUMBER;
                // Se è un carattere operatore il token corrente termina in ogni caso, nuovo token operatore */
                else if(isoperator(c))
                        newtoken_type = TOKEN_OPERATOR;
                // Se è un carattere alfabetico (o una cifra fuori da un numero) o un _ e non ci trovavamo già in un identificatore inizia un identificatore
                else if((isalpha(c) || (isdigit(c) && current_type != TOKEN_NUMBER) || c == '_') && current_type != TOKEN_IDENTIFIER)
                        newtoken_type = TOKEN_IDENTIFIER;
                
                // In base a quanto visto, se inizia un nuovo token
                if(newtoken_type != TOKEN_UNDEFINED)
                {
                        // Creo il nuovo token, cambio stato e imposto la lunghezza del vecchio token
                        token new_t = token_New(newtoken_type, i);
                        current_type = newtoken_type;
                        t->next = new_t;
                        // Se la lunghezza è diversa da 0 il token era già stato chiuso da uno spazio
                        if(t->length == 0)
                                t->length = i-t->position;
                        t = new_t;
                }
                
                // j tiene traccia dell'ultimo carattere stampabile
                if(isprint(c))
                        j++;
        }
        /* Lunghezza dell'ultimo token */
        t->length = j-t->position;
        
        // Scorro i token e li conto, inoltre faccio una copia della stringa corrispondente in ciascuno di essi
        token_counter = 0;
        t = first;
        while((t = t->next) != NULL) // Il primo token (fittizio) viene subito saltato
        {
                token_counter++;
                t->text=strndup(&string[t->position], t->length);
        }
        
        if(token_count != NULL)
                *token_count = token_counter;
        
        // Libero il token fittizio e restituisco il primo valido
        t = first->next;
        token_Free(first, 0);
        return t;
}

/*
        Consente di determinare se la stringa str è un identificatore valido
        (deve iniziare con una lettera o un "_" e può contenere solo lettere,
        numeri e "_"
        
        Restituisce 1 in caso di identificatore valido, 0 altrimenti.
        Non è usata direttamente da tokenizer ma da altre funzioni in giro
        per la libreria che possono aver bisogno di verificare la validità
        di un nome.
*/
int is_valid_identifier(char *str)
{
        int i, len;
        len = strlen(str);
        if(str == 0)
                return 0;
        if(!(isalpha(str[0]) || str[0] == '_'))
                return 0;
        for(i = 0; i < len; i++)
                if(!(isalpha(str[i]) || isdigit(str[i]) || str[i] == '_'))
                        return 0;
        return 1;
}


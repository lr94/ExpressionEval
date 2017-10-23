#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>
#include <expreval.h>

#define PI              3.1415926535897932384626
double max(double a, double b) { return (a > b) ? a : b; }

double test_function(double x, double y, double z, double a)
{
    return 1+pow(0.35*cos(1/(PI*x))+sqrt(1+pow(-y+x,2))+sqrt(1+pow(x-y,2)),3)-(2-cos(a))/(2+sin((x+y)/(max(1+x,1+y)+x*x+z*z)))-(int)(a*256*cos(x*y+4*z*z-a))%(int)(102+24.3*sin(z*z*z-x*a+y)); // ||
}
char expr[] = "1+(0.35*cos(1/(pi*x))+sqrt(1+(-y+x)^2)+sqrt(1+(x-y)^2))^3-(2-cos(a))/(2+sin((x+y)/(max(1+x,1+y)+x^2+z^2)))-(a*256*cos(x*y+4*z^2-a))%(102+24.3*sin(z^3-x*a+y))";
#define N       2500000




#define GET_TIME()          (gettimeofday(&tv, 0) , (double)(tv.tv_sec * 1000000 + tv.tv_usec) / 1000000.0)
void print_size(unsigned long long n, int iec);
void print_lw(char *s, int n);
void nomem();

int main()
{
    struct timeval tv;
    int i, j;
    unsigned long long int memb = 0;

    double s, e;

    // Stampa informazioni iniziali

    printf("Espressione di prova:\t\t\t");
    print_lw(expr, 48);
    printf("\n\n");
    printf("Numero di prove:\t\t\t%d\n", N);

    // Inizializza generatore di numeri casuali
    srand((unsigned)time(NULL));
    // Alloca la memoria e genera i valori
    double **values = malloc(N*sizeof(double*));        memb += N*sizeof(double*);
    if(values == NULL)
        nomem();
    for(i = 0; i < N; i++)
    {
        values[i] = malloc(7 * sizeof(double));     memb += 7*sizeof(double);
        if(values[i] == NULL)
            nomem();
        for(j = 0; j < 7; j++)
            values[i][j] = (j < 4) ? (double)rand() / RAND_MAX  * 1000.0 - 500.0 : 0.0;
    }
    #ifdef __GNUC__
        /* La glibc permette statistiche più precise sulla memoria utilizzata
           (comprendenti anche i blocchi "di controllo" usati dalla malloc */
        struct mallinfo mi = mallinfo();
        memb = mi.uordblks + mi.hblkhd;
    #endif
    printf("Memoria utilizzata:\t\t\t"); print_size(memb, 1); printf(" ("); print_size(memb, 0); printf(")\n");

    //////////// TEST INTERPRETE
    s = GET_TIME();
    parser p = parser_New();
    parser_RegisterFunction(p, "sin", sin, 1);
    parser_RegisterFunction(p, "cos", cos, 1);
    parser_RegisterFunction(p, "pow", pow, 2);
    parser_RegisterFunction(p, "max", max, 2);
    parser_RegisterFunction(p, "sqrt", sqrt, 1);
    parser_SetVariable(p, "pi", PI);
    expression ex = expression_Parse(p, expr);
    if(ex == NULL)
    {
        fprintf(stderr, "Errore:\n\t%s\n", parser_GetError(p, NULL, NULL));
        exit(EXIT_FAILURE);
    }
    e = GET_TIME();
    printf("Tempo parsing:\t\t\t\t%f s\n", e - s);
    printf("Tempo interprete:\t\t\t");
    fflush(stdout);
    char *err;
    s = GET_TIME();
    for(i = 0; i < N; i++)
    {
        parser_SetVariable(p, "x", values[i][0]);
        parser_SetVariable(p, "y", values[i][1]);
        parser_SetVariable(p, "z", values[i][2]);
        parser_SetVariable(p, "a", values[i][3]);

        values[i][4] = expression_Eval(p, ex);
    }
    e = GET_TIME();
    expression_Free(ex);
    parser_Free(p);
    printf("%f s\n", e - s);


    //////////// TEST COMPILATORE JIT
    s = GET_TIME();
    // Inizializzo il compilatore
    compiler c = compiler_New();
    compiler_RegisterFunction(c, "sin", sin, 1);
    compiler_RegisterFunction(c, "cos", cos, 1);
    compiler_RegisterFunction(c, "pow", pow, 2);
    compiler_RegisterFunction(c, "max", max, 2);
    compiler_RegisterFunction(c, "sqrt", sqrt, 1);
    compiler_MapArgument(c, "x", 0);
    compiler_MapArgument(c, "y", 1);
    compiler_MapArgument(c, "z", 2);
    compiler_MapArgument(c, "a", 3);
    compiler_SetConstant(c, "pi", PI);
    // Compilo la funzione (cfunction4 perché la funzione ha 4 argomenti)
    cfunction4 cf = compiler_CompileFunction(c, expr);
    // Verifico se ci sono stati errori
    if(cf == NULL)
    {
        fprintf(stderr, "Errore:\n\t%s\n", compiler_GetError(c, NULL, NULL));
        exit(EXIT_FAILURE);
    }
    e = GET_TIME();
    printf("Tempo compilazione JIT:\t\t\t%f s\n", e - s);

    printf("Tempo JIT:\t\t\t\t");
    fflush(stdout);

    // A questo punto la funzione è bella che compilata e la tratto come tale
    s = GET_TIME();
    for(i = 0; i < N; i++)
        values[i][5] = cf(values[i][0], values[i][1], values[i][2], values[i][3]);
    e = GET_TIME();
    printf("%f s\n", e - s);
    compiler_Free(c);

    //////////// TEST GCC
    printf("Tempo GCC:\t\t\t\t");
    fflush(stdout);
    s = GET_TIME();
    for(i = 0; i < N; i++)
        values[i][6] = test_function(values[i][0], values[i][1], values[i][2], values[i][3]);
    e = GET_TIME();
    printf("%f s\n", e - s);

    //////////// VERIFICA DEI RISULTATI
    /*
        In particolare su architettura x86_32 (i686) i risultati delle due funzioni compilate
        (rispettivamente dal compilatore JIT e da GCC) potrebbero essere leggermente diversi,
        in quanto di default su i686 GCC compila usando per i calcoli in virgola mobile le
        istruzioni del set della FPU x87, mentre su x86_64 usa di default le istruzioni del
        set SSE (/versioni successive). Il compilatore JIT di questa libreria usa sempre
        il set SSE2.
        Sono istruzioni diverse che vengono eseguite da circuiti diversi che operano in modo
        diverso, in particolare la FPU x87 adotta dei registri a 80 bit per i passaggi
        intermedi.
        Si può forzare GCC a compilare usando il set SSE2 anche su x86_32 usando i flags:
            -msse2 -mfpmath=sse
    */
    double max_delta = 0.0;
    double max_rel = 0.0;
    double sum_rel = 0.0;
    int count_rel = 0;
    int max_delta_i = -1;
    int max_rel_i = -1;
    for(i = 0; i < N; i++)
    {
        double delta = max(fabs(values[i][4] - values[i][5]), fabs(values[i][4] - values[i][6]));
        delta = max(delta, fabs(values[i][5] - values[i][6]));
        if(delta > max_delta)
        {
            max_delta = delta;
            max_delta_i = i;
        }

        if(values[i][6]!=0.0)
        {
            double rel = delta / values[i][6] * 100.0;
            count_rel++;
            sum_rel += rel;
            if(rel > max_rel)
            {
                max_rel = rel;
                max_rel_i = i;
            }
        }
    }
    printf("Massimo errore assoluto:\t\t%.16f\n", max_delta);
    if(max_delta_i > -1)
    {
        printf("\tErrore relativo:\t\t%.16f %%\n", max_delta / values[max_delta_i][6] * 100.0);
        printf("\tCorrispondente ai parametri:\n");
        printf("\t\t  x   = %f\n", values[max_delta_i][0]);
        printf("\t\t  y   = %f\n", values[max_delta_i][1]);
        printf("\t\t  z   = %f\n", values[max_delta_i][2]);
        printf("\t\t  a   = %f\n", values[max_delta_i][3]);
        printf("\tRisultati:\n");
        printf("\t\tINT   = %.16f\n", values[max_delta_i][4]);
        printf("\t\tJIT   = %.16f\n", values[max_delta_i][5]);
        printf("\t\tGCC   = %.16f\n\n", values[max_delta_i][6]);
    }
    printf("Massimo errore relativo:\t\t%.16f %%\n", max_rel);
    if(max_rel_i > -1)
    {
        double delta = max(fabs(values[max_rel_i][4] - values[max_rel_i][5]), fabs(values[max_rel_i][4] - values[max_rel_i][6]));
        delta = max(delta, fabs(values[max_rel_i][5] - values[max_rel_i][6]));
        printf("\tErrore assoluto:\t\t%.16f\n", delta);
        printf("\tCorrispondente ai parametri:\n");
        printf("\t\t  x   = %f\n", values[max_rel_i][0]);
        printf("\t\t  y   = %f\n", values[max_rel_i][1]);
        printf("\t\t  z   = %f\n", values[max_rel_i][2]);
        printf("\t\t  a   = %f\n", values[max_rel_i][3]);
        printf("\tRisultati:\n");
        printf("\t\tINT   = %.16f\n", values[max_rel_i][4]);
        printf("\t\tJIT   = %.16f\n", values[max_rel_i][5]);
        printf("\t\tGCC   = %.16f\n\n", values[max_rel_i][6]);
    }
    printf("Errore relativo medio:\t\t\t%.16f %%\n", sum_rel / count_rel);

    for(i = 0; i < N; i++)
        free(values[i]);
    free(values);


    exit(EXIT_SUCCESS);
}

void print_size(unsigned long long n, int iec)
{
    static const char *units_iec[] = {"B", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB"};
    static const char *units[] = {"B", "KB", "MB", "GB", "TB", "PB", "EB"};
    int n_units = sizeof(units) / sizeof(*units);
    int r;
    int i = 0;
    int div[] = {1000, 1024};
    if(iec % 2 != iec)
        return;
    while(n >= div[iec] && i < n_units)
    {
        r = n % div[iec];
        n /= div[iec];
        i++;
    }
    printf("%.2f %s", (float)n + (float)r / (float)div[iec], iec ? units_iec[i] : units[i]);
}

void print_lw(char *s, int n)
{
    int i = 0;
    while(s[i])
    {
        if(i % n == 0 && i != 0)
            printf("\n\t\t\t\t\t");
        printf("%c", s[i]);
        i++;
    }
}

void nomem()
{
    fprintf(stderr, "Errore:\n\tMemoria insufficiente.\n");
    exit(-1);
}

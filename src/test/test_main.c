#include <stdio.h>
#include <math.h>
#include <stdlib.h>

#include <expreval.h>

double maxd(double a, double b)
{
        if(a>b)
                return a;
        return b;
}

double boh()
{
        return sqrt(2);
}

double gcc_cf(double x)
{
        return (x*(1+x)-sin(x)+7*(4-3)/(x+3)-(x*x-2*x+17.3)/(pow(x,3.0)+19.4*x+7))/(21.7-47.3*x);
}

int main(int argc, char *argv[])
{
        char buf[256];
        printf("f(x,y) = ");
        fgets(buf, 255, stdin); /* (sin(pi/2)+2*(((2+5*2)/4)^2+2)+7)/3 */

        float xf, yf;
        
        compiler c = compiler_New();
        compiler_SetConstant(c, "pi", 3.14159265358979);
        compiler_SetConstant(c, "phi", (1+sqrt(5.0))/2);
        compiler_MapArgument(c, "x", 0);
        compiler_MapArgument(c, "y", 1);
        compiler_RegisterFunction(c, "sqrt", sqrt, 1);
        compiler_RegisterFunction(c, "sin", sin, 1);
        compiler_RegisterFunction(c, "cos", cos, 1);
        compiler_RegisterFunction(c, "pow", pow, 2);
        compiler_RegisterFunction(c, "max", maxd, 2);

        cfunction2 cf = compiler_CompileFunction(c,  buf);
        
        char *errstr = compiler_GetError(c, NULL, NULL);
        if(errstr != NULL)
        {
                fprintf(stderr, "Errore:\n\t%s\n", errstr);
                compiler_Free(c);
                exit(1);
        }        
        
        compiler_Free(c);
        printf("    x  = ");
        scanf("%f", &xf);
        printf("    y  = ");
        scanf("%f", &yf);
        
        double x = (double)xf;
        double y = (double)yf;
        
        printf("f(x,y) = %f\n", cf(x, y));
        
        return EXIT_SUCCESS;
}


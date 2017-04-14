#include <stdio.h>
#include <math.h>
#include <stdlib.h>

#include <expreval.h>

int main(int argc, char *argv[])
{
        
        compiler c = compiler_New();
        compiler_SetConstant(c, "pi", 3.14159265358979);
        compiler_SetConstant(c, "phi", (1+sqrt(5.0))/2);
        compiler_MapArgument(c, "x", 0);
        compiler_RegisterFunction(c, "sqrt", sqrt, 1);
        compiler_RegisterFunction(c, "sin", sin, 1);
        compiler_RegisterFunction(c, "cos", cos, 1);
        compiler_RegisterFunction(c, "pow", pow, 2);

        // Magia!
        cfunction1 cf = compiler_CompileFunction(c,  "1/(2+cos(x))");
        double result = cf(3.4);
        
        
        printf("%f\n", result);
        
        return EXIT_SUCCESS;
}


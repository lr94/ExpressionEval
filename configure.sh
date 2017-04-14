#!/bin/bash

cc="gcc"                        # Nome del compilatore
as="nasm"                       # Nome dell'assembler
cflags="-msse2 -mfpmath=sse"    # Eventuali opzioni aggiuntive per il compilatore (ho messe queste per forzare SSE2 anche sui i386)

#######

arch=$(uname -m)
os=$(uname -o)
nasm_format="elf64";
if [[ "$arch" == "x86_64" ]] ; then
        echo -e "Architecture:\t\tx86_64"
elif [[ ( "$arch" == "i686" ) || ( "$arch" == "i386" ) || ( "$arch" == "x86_32" ) ]] ; then
        echo -e "Architecture:\t\tx86_32"
        nasm_format="elf32";
fi
echo -e "OS:\t\t\t$os"
echo -ne "Checking for $cc...\t";
hash $cc 2>/dev/null || { echo >&2 "no"; exit 1; }
echo "yes"
echo -ne "Checking for $as...\t";
hash $as 2>/dev/null || { echo >&2 "no"; exit 1; }
echo "yes"
echo -ne "Library type:\t\t"
if [[ $* == *--shared* ]] ; then
        echo "Shared"
else
        echo "Static"
fi
echo -ne "Creating Makefile...\t"

######## QUI INIZIA LA SCRITTURA DEL MAKEFILE VERO E PROPRIO

# Compilatore e assembler
echo "CC = $cc" > Makefile
echo -e "AS = $as\n" >> Makefile

echo -e "ECHO = $(which echo)\n" >> Makefile
# Flags per GCC (usati nella compilazione della libreria e degli esempi)
cflags="-I./src/include/ $cflags"
if [[ $* == *--debug* ]] ; then
        cflags="$cflags -g"
fi
if [[ $* == *--optimize* ]] ; then
        cflags="$cflags -O1"
else
        if [[ $* == *--maxoptimize* ]] ; then
                cflags="$cflags -O3"
        fi
fi
echo "CFLAGS = $cflags" >> Makefile

#Flags aggiuntivi per compilazione shared library e nome della libreria (.a se statica, .so se shared)
add_flags="";
if [[ $* == *--shared* ]] ; then
        echo "LIBOUT = libexpreval.so"
        add_flags="$add_flags -fPIC -Wl,--version-script=map.sym"
else
        echo "LIBOUT = libexpreval.a"
fi >> Makefile
echo "LIB_CFLAGS = $add_flags" >> Makefile # Solo per i sorgenti di libreria e per il linking di libreria

# Lista delle directory da creare (in ./obj deve esserci la stessa struttura che c'è in ./src)
echo -e "DIRS = usr/lib usr/include bin \$(shell find src -type d | awk '{gsub(\"src\",\"obj\",\$\$0); print}')" >> Makefile
# Lista dei .h
echo -e "HEADERS = \$(shell find src -name '*.h')" >> Makefile
# Lista degli eseguibili d'esempio da creare (generata a partire dai file .c)
echo -e "test = \$(shell find ./test -name '*.c' | awk '{gsub(\"./test\",\"./bin\",\$\$0); gsub(\"\\\\\\.c\",\"\",\$\$0); print}')" >> Makefile
# Lista dei file oggetto da creare (generata a partire dai file .c presenti in ./src e sottocartelle)
echo -e "OBJS_C = \$(shell find ./src -name '*.c' | awk '{gsub(\"./src\",\"./obj\",\$\$0); gsub(\"\\\\\\.c\",\".o\",\$\$0); print}')" >> Makefile
# Lista dei file oggetto da creare (generata a partire dai file .asm presenti in ./src e sottocartelle)
echo -e "OBJS_ASM = \$(shell find ./src -name '*.asm' | awk '{gsub(\"./src\",\"./obj\",\$\$0); gsub(\"\\\\\\.asm\",\".o\",\$\$0); print}')" >> Makefile

# Target completo, compila tutto (libreria ed esempi)
echo -e "all: \$(DIRS) \$(OBJS_C) \$(OBJS_ASM) library test\n" >> Makefile

# Target libreria
if [[ $* == *--shared* ]] ; then
        # Primo target: map.sym, lista di tutte le funzioni da esportare nella libreria shared, creata a partire da expreval.h
        echo -e "map.sym: \$(HEADERS)" >> Makefile
        echo -e "\t@\$(ECHO) -e \"{\\\\n\\\\tglobal:\" > map.sym" >> Makefile
        echo -e "\t@awk -F'[ (]' '(/.+\(.*\);/ && !match(\$\$1, /typedef/)){sub(\"*\",\"\",\$\$2);print \"\\\\t\\\\t\"\$\$2\";\";}' src/include/expreval.h >> map.sym" >> Makefile
        echo -e "\t@\$(ECHO) -e \"\\\\tlocal:\\\\n\\\\t\\\\t*;\\\\n};\" >> map.sym\n" >> Makefile
        # Secondo target (dipendente anche da map.sym)
        echo -e "library: \$(DIRS) map.sym \$(OBJS_C) \$(OBJS_ASM)" >> Makefile
        echo -e "\t@\$(ECHO) -e \"CC\\\\tusr/lib/\$(LIBOUT)\"" >> Makefile
        echo -e "\t@\$(CC) \$(LIB_CFLAGS) -shared -o usr/lib/\$(LIBOUT) \$(OBJS_ASM) \$(OBJS_C)" >> Makefile
        # Strip della libreria, eliminiamo tutto quello che non serve (simboli non esportati ecc ecc)
        if [[ $* != *--debug* ]] ; then
                echo -e "\t@strip usr/lib/\$(LIBOUT)" >> Makefile
        fi
else
        # Dipende solo dai file oggetto e dalle cartelle
        echo -e "library: \$(DIRS) \$(OBJS_C) \$(OBJS_ASM)" >> Makefile
        echo -e "\t@\$(ECHO) -e \"AR\\\\tusr/lib/\$(LIBOUT)\"" >> Makefile
        echo -e "\t@ar rcs usr/lib/\$(LIBOUT) \$(OBJS_ASM) \$(OBJS_C)" >> Makefile
fi
# Copia dei .h (rientra nel target "library" in ogni caso)
echo -e "\t@\$(ECHO) -e \"CP\\\\tusr/include/expreval.h\"" >> Makefile
echo -e "\t@cp src/include/expreval.h usr/include/expreval.h" >> Makefile
echo "" >> Makefile

# Target esempi: dipendono da library e dalla lista di file eseguibili
echo -e "test: library \$(test)" >> Makefile

# I file della libreria (.so/.a e .h) da cui dipende l'installazione dipendono da library
echo -e "usr/lib/\$(LIBOUT) usr/include/expreval.h: library\n" >> Makefile

# Installazione (dipende da .so/.a e .h)
echo -e "install: usr/lib/\$(LIBOUT) usr/include/expreval.h" >> Makefile
echo -e "\t@cp usr/lib/\$(LIBOUT) /usr/lib/\$(LIBOUT)" >> Makefile
echo -e "\t@cp usr/include/expreval.h /usr/include/expreval.h" >> Makefile
echo "" >> Makefile

# Disinstallazione
echo -e "uninstall:" >> Makefile
echo -e "\t@rm -f /usr/lib/libexpreval.so" >> Makefile
echo -e "\t@rm -f /usr/lib/libexpreval.a" >> Makefile
echo -e "\t@rm -f /usr/include/expreval.h" >> Makefile
echo "" >> Makefile

# Per ogni file .c degli esempi
for x in $(find test -name '*.c') ; do
        # Determino la directory di output
        dn=$(dirname $x | awk '{gsub("test", "bin", $0); print}')
        # Determino il nome dell'eseguibile
        y="$dn/$(basename $x .c)"
        # Scrivo il target e le dipendenze (tra cui i .h)
        echo -e "$y: $x \$(HEADERS)"
        echo -e "\t@\$(ECHO) -e \"CC\\\\t$x\""
        # Comando di compilazione
        echo -e "\t@\$(CC) \$(CFLAGS) -I./usr/include/ -L./usr/lib/ $x -lexpreval -lm -o $y ;"
        echo ""
done >> Makefile;

# Per ogni file .c della libreria (analogo a sopra)
for x in $(find src -name '*.c') ; do
        dn=$(dirname $x | awk '{gsub("src", "obj", $0); print}')
        y="$dn/$(basename $x .c).o"
        echo -e "$y: $x \$(HEADERS)"
        echo -e "\t@\$(ECHO) -e \"CC\\\\t$x\""
        echo -e "\t@\$(CC) \$(CFLAGS) \$(LIB_CFLAGS) -c $x -o $y"
        echo ""
done >> Makefile;

# Per ogni file .asm della libreria (analogo a sopra)
for x in $(find src -name '*.asm') ; do
        dn=$(dirname $x | awk '{gsub("src", "obj", $0); print}')
        y="$dn/$(basename $x .asm).o"
        echo -e "$y: $x"
        echo -e "\t@\$(ECHO) -e \"AS\\\\t$x\""
        echo -e "\t@\$(AS) -f $nasm_format $x -o $y"
        echo ""
done >> Makefile;

# Creazione delle directory
echo "\$(DIRS):" >> Makefile
echo -e "\t@\$(ECHO) -e \"MK\\\\t\$@\"" >> Makefile
echo -e "\t@mkdir -p \$@\n" >> Makefile

# Pulizia dai file oggetto
echo "clean:" >> Makefile
echo -e "\t@rm -rf obj/*" >> Makefile
echo "" >> Makefile

# Pulizia totale
echo "cleanall: clean" >> Makefile
echo -e "\t@rm -fr bin/ usr/ obj/ Makefile map.sym" >> Makefile
echo "" >> Makefile

# Conteggio righe di codice
echo "lines:" >> Makefile
echo -e "\t@cat \$\$(find src/ \\( -name *.asm -o -name *.h -o -name *.c \\)) | wc -l" >> Makefile


echo "Done."

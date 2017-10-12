# LibExpreval 0.8.8 (12 Ottobre 2018)

## Compatibilità

La libreria al momento è compatibile (almeno per le funzionalità relative alla compilazione)
solo con Linux su architetture x86_64 e i686 (x86_32).

È inoltre necessario il set di istruzioni SSE2, che dovrebbe essere presente su tutti i
processori Intel prodotti a partire dal Pentium 4. In ogni caso la libreria ne verifica la
presenza in fase di compilazione delle espressioni.

## Configurazione e compilazione

Per generare il makefile:

        $ ./configure.sh

Per generare il makefile in modalità debug (aggiunge -g in fase di compilazione):

        $ ./configure.sh --debug

Una volta generato il makefile, per compilare tutto (libreria ed esempi):

        $ make

Per compilare solo la libreria:

        $ make library

Per installare la libreria sul sistema:

        # make install
        
Per compilare e installare sul sistema la libreria come shared library:

        $ make cleanall                 # Da eseguire sempre se in precedenza era stata compilata
                                        # la versione statica della libreria

        $ ./configure.sh --shared
        $ make library
        # make install

Per disinstallare la libreria dal sistema:

        # make uninstall

Per cancellare tutti i file oggetto:

        $ make clean

Per cancellare tutti i file generati in fase di compilazione (e il makefile)

        $ make cleanall

## Modifiche e prove

Se si vogliono effettuare delle prove è sufficiente creare i file C all'interno di "test",
eseguire nuovamente ./configure.sh e poi eseguire make. Gli esempi saranno compilati in "bin".
Se si effettuano modifiche al sorgente la procedura è analoga, ma prima di ./configure.sh
è opportuno eseguire make cleanall

## Libreria shared o static

Di default viene generata una libreria statica (static library): i due file necessari al suo
utilizzo sono

        ./usr/lib/libexpreval.a
        ./usr/lib/include/expreval.h
        
È tuttavia possibile generare una libreria condivisa (shared library) generando nuovamente il
makefile:

        $ ./configure.sh --shared

In tal caso i file generati dalla compilazione della libreria sono

        ./usr/lib/libexpreval.so
        ./usr/lib/include/expreval.h

(questi file vengono copiati nell'opportuna posizione /usr/lib/ in fase di installazione)

Se si compila la libreria come condivisa è necessario installarla prima di poter eseguire
qualsiasi programma (compresi gli esempi) che ne fa utilizzo.

## Uso della libreria installata

Una volta installata la libreria sul sistema, è possibile compilare programmi che ne fanno uso

        gcc file.c -lm -lexpreval -o file

## Valutazione delle espressioni

Gli operatori supportati sono (in ordine di priorità decrescente):

        ^
        + -
        * / %
        + -

La libreria lavora sempre su valori in virgola mobile a doppia precisione (double, 64 bit).
Per l'operatore %, definito solo per interi, opera troncando gli operandi a interi a 32 bit
(attenzione ad overflow):

        14.87 % 11.26 = 14 % 11 = 3

Non è supportata la moltiplicazione implicita:

        5(3+x) NON è un modo valido per scrivere 5*(3+x)

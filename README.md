_Note: Since when I started the project I didn't plan to make it public, all the comments and the documentation are written in Italian. I will eventually translate them, but I don't know when._

# LibExpreval 0.9.2 (23 Ottobre 2017)

## Compatibilità

La libreria al momento è compatibile (almeno per le funzionalità relative alla compilazione)
solo con Linux su architetture x86_64, i686 (x86_32) a aarch64 (ARM a 64 bit).
Sto lavorando al supporto di Windows su x86_64.

È inoltre necessario il set di istruzioni SSE2, che dovrebbe essere presente su tutti i
processori Intel prodotti a partire dal Pentium 4. In ogni caso la libreria ne verifica la
presenza in fase di compilazione delle espressioni.

## Configurazione e compilazione

Per compilare e installare la libreria statica sul sistema

        $ ./configure
        $ make
        # make install

Per compilare e installare sul sistema la libreria anche come shared library:

        $ ./configure --enable-shared
        $ make
        # make install

Per disinstallare la libreria dal sistema:

        # make uninstall

Per cancellare tutti i file generati in fase di compilazione (e il makefile)

        $ make distclean

## Libreria shared o static

Di default viene generata una libreria statica (static library): i due file necessari al suo
utilizzo sono

        libexpreval.a
        include/expreval.h

È tuttavia possibile generare una libreria condivisa (shared library) generando nuovamente il
makefile:

        $ ./configure --enable-shared

In tal caso i file generati dalla compilazione della libreria sono

        libexpreval.a
        libexpreval.so

(questi file vengono copiati nell'opportuna posizione /usr/lib/ in fase di installazione; il prefisso /usr può essere modificato mediante l'opzione --prefix di configure)

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

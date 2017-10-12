/*
        Nota: In tutto il file si usano i nomi dei registri a 32 bit per indicare i registri sia a 32 che a 64 bit:
        tranne qualche eccezione (IDIV, per esempio) 
*/

#ifndef X86_H_INCLUDED
#define X86_H_INCLUDED

enum {FLAG_DISP8  = 1,           // b001,        Indica che l'istruzione corrente è seguita da/contiene un indirizzo (relativo: displacement) a 8 bit
      FLAG_DISP32 = 2,           // b010         Indica che l'istruzione corrente è seguita da/contiene un indirizzo (relativo: displacement) a 32 bit
      FLAG_SIB    = 4};          // b100         Indica che il byte ModR/M dell'istruzione corrente è seguito da un byte SIB

// _64PREFIX aggiunge un prefisso 0x48 all'istruzione, solo se stiamo lavorando sui 64
#if __x86_64__
        #define _64PREFIX        do { code[i++] = 0x48; } while(0)
#else
        #define _64PREFIX        do { } while(0)
#endif

/********************************* MACRO USATE PER DEFINIRE GLI OPERANDI DELLE ISTRUZIONI *******************************/

/*
        Associo ad ogni registro il rispettivo indice. XMMn e EAX/EBX/... hanno gli stessi indici perché vengono
        rappresentati allo stesso modo dentro ogni istruzione: poi a seconda dell'opcode la CPU sa se deve lavorare
        con EAX o XMM0 (per esempio).
        Gli indici di EAX/ECX/EDX/EBX/[...] sarebbero 0/1/2/3/[...] però qui li segno con un offset di 24 sull'indice
        per poter distinguere:
                [EAX]               Accesso in memoria senza displacement               offset:  0
                [EAX+disp8]         Accesso in memoria con displacement a 8 bit         offset:  8
                [EAX+disp32]        Accesso in memoria con displacement a 1 bit         offset: 16
                EAX                 Nessun accesso in memoria                           offset: 24
        (vale lo stesso per tutti gli altri registri, ovviamente)
        
        Poi quando serve effettivamente l'ID di un registro nelle macro scrivo qualcosa come (REG % 8), per eliminare
        l'offset. Allo stesso modo posso sempre sapere se intendo EAX, [EAX], [EAX+disp8] o [EAX+disp32] dividendo per
        8: nei 4 casi viene 3, 0, 1, 2.
        
        I casi _SIB_ e _disp_ sono particolari: non indicano alcun registro e vengono usati da altre macro definite sotto
        (non sono da usare manualmente ma vengono generati rispettivamente dalle macro INDEX e DISP)
                _SIB_ indica che l'istruzione è seguita da un byte SIB che specifica bene dove vogliamo accedere in memoria
                        (usato per casi complessi quali [ecx+esi*4+0x7])
                _disp_ indica semplicemente che si accede a un indirizzo a 32 bit (che segue l'istruzione):
                        (usato per cose come [0xee32f0])
*/
enum { XMM0 = 24,       XMM1,    XMM2,    XMM3,    XMM4,    XMM5,    XMM6,    XMM7  };
enum {_EAX_ =  0,      _ECX_,   _EDX_,   _EBX_,   _SIB_,  _disp_,   _ESI_,   _EDI_ ,                /* [EAX] ... */
        EAX = 24,        ECX,     EDX,     EBX,     ESP,     EBP,     ESI,     EDI  };
enum {_ESP_ = 4};

// Indica "nessun registro" nel senso del byte SIB (???)
#define NONE                    4

/*
        Le macro _EAX_(x) ecc ecc stanno ad indicare [EAX+x].
        x può essere un displacement a 8 o a 32 bit.
        Tutte queste macro hanno una parte consistente in comune, perciò chiamano un'ulteriore macro _REG_DISP:
        _REG_DISP esegue le seguenti operazioni:
                -Stabilisce se il displacement sta o meno su 8 bit e setta di conseguenza i flag 
                 FLAG_DISP8 o FLAG_DISP32
                -Inserisce nella variabile disp il valore del displacement (che è il suo secondo argomento)
                -Restituisce l'indice del registro (il suo primo argomento) con l'offset di 8 o 16 come spiegato
                 sopra. Per determinare l'offset si usa "flag & 3" (ovvero flag ma prendendo solo i 2 bit meno
                 significativi, che corrispondono appunto a FLAG_DISP8 e FLAG_DISP32:
                        flag & 3 == b01         =>     restituisce b + 8
                        flag & 3 == b10         =>     restituisce b + 16
        
        DISP(x) serve invece per indicare un accesso in memoria ad un determinato indirizzo
        (per esempio [0xffe3] si scrive DISP(0xffe3)).
        Esegue le seguenti operazioni:
                -Setta il flag FLAG_DISP32
                -Inserisce nella variabile disp il valore del displacement
                -Restituisce il valore speciale _disp_ spiegato sopra
*/
#define _REG_DISP(b, x)         ((flag |= (x > -129 && x < 128) ? FLAG_DISP8 : FLAG_DISP32) , (disp = x) , (b + 8 * (flag & 3)))
#define _EAX_(x)                _REG_DISP(_EAX_, x)                                                                // [eax]
#define _ECX_(x)                _REG_DISP(_ECX_, x)
#define _EDX_(x)                _REG_DISP(_EDX_, x)
#define _EBX_(x)                _REG_DISP(_EBX_, x)
#define _ESI_(x)                _REG_DISP(_ESI_, x)
#define _EDI_(x)                _REG_DISP(_EDI_, x)
#define DISP(x)                 (flag |= FLAG_DISP32, disp = x, _disp_)                                                // [X]

/*
        Questa macro serve per indicare indirizzamenti quali
                [ecx+esi*4+0x7]         =>      INDEX(_ECX_, _ESI_, 4, 0x7)
                [b+x*s+d]               =>      INDEX(    b,     x, s,   d)
        Per descrivere la terna (b,x,s) si usa un byte apposito (byte SIB) che segue il byte ModR/M descritto sotto.
        Il byte SIB contiene 3 campi (dal bit più significativo al meno significativo):
                SS:     2 bit   Indica lo scalamento s
                                        00:  s = 1
                                        01:  s = 2
                                        10:  s = 4
                                        11:  s = 8
      Scaled index:     3 bit   Indica il registro x
                                        000: EAX
                                        001: ECX
                                        010: EDX
                                        011: EBX
                                        100: (nessuno)
                                        101: EBP
                                        110: ESI
                                        111: EDI
              Base:     3 bit   Indica il registro b
                                        000: EAX
                                        001: ECX
                                        010: EDX
                                        011: EBX
                                        100: ESP
                                        101: ??? Qui penso che dipenda dai due bit del campo Mod del byte ModR/M precedente
                                        110: ESI
                                        111: EDI
                                        
        La macro opera settando FLAG_SIB e calcolando il byte SIB per poi usare _REG_DISP come visto in precedenza coi registri 
*/
#define INDEX(b,x,s,d)          (flag = FLAG_SIB, sib = (("\x0\x0\x1\x0\x2\x0\x0\x0\x3"[s] << 6) | (x << 3) | b), _REG_DISP(_SIB_, d)) // [b+x*s+d]
#define _ESP_(x)                INDEX(4, NONE, 1, x)        // Aggiunto per lavorare sullo stack: [esp+x]
#define _EBP_(x)                INDEX(5, NONE, 1, x)        // [ebp+x]

// Questa serve per indicare un generico registro XMM{n}, così XMM1 si può scrivere sia come XMM1 che come XMM(1) 
#define XMM(x)                  (24 + x % 8 )



/*************************************** MACRO USATE PER CODIFICARE LE ISTRUZIONI ***************************************/

/*
        Il byte ModR/M è usato in molte istruzioni per indicare una coppia di operandi o un solo operando.
        Può essere usato per indicare:
                -Due registri
                -Un registro e una locazione di memoria (in questo caso è seguito da altri byte che la specificano)
                -Un registro e un parametro "mode" (da 0 a 7) per l'istruzione corrente
        Esso è composto da 3 campi (dal bit più significativo al meno significativo):
                Mod:    2 bit   Indica la modalità di accesso alla memoria:
                                        00:  Senza displacement (es: [eax], [0xf342])
                                        01:  Displacement 8 bit (es: [eax+8])
                                        10:  Displacement 32 bit (es: [ebp+892])
                                        11:  Registro
                REG:    3 bit   Indica l'operando registro o la modalità dell'istruzione (a seconda dei casi).
                                Qualora indichi un registro ad ogni possibile valore di REG corrispondono fino a 8
                                diversi registri. Il registro effettivamente utilizzato dipende dall'opcode dell'istruzione:
                                la mov avrà diverse traduzioni in opcode, in una delle quali REG == b000 vorrà dire EAX,
                                in un'altra AX e così via.
                                        000: AL/AX/EAX/MM0/XMM0/ES/CR0/DR0
                                        001: CL/CX/ECX/MM1/XMM1/CS/???/DR1
                                        010: DL/DX/EDX/MM2/XMM2/SS/CR2/DR2
                                        011: BL/BX/EBX/MM3/XMM3/DS/CR3/DR3
                                        100: AH/SP/ESP/MM4/XMM4/FS/CR4/DR4
                                        101: CH/BP/EBP/MM5/XMM5/GS/???/DR5
                                        110: DH/SI/ESI/MM6/XMM6/??/???/DR6
                                        111: BH/DI/EDI/MM7/XMM7/??/???/DR7
                R/M:    3 bit   Indica a quale registro si riferiscono i due bit del campo Mod:
                                        000: EAX
                                        001: ECX
                                        010: EDX
                                        011: EBX
                                        100: (sib)
                                        101: disp32 se Mod è 00, EBP altrimenti
                                        110: ESI
                                        111: EDI
        Quando il campo R/M è b100 (SIB) un ulteriore byte (detto byte SIB, descritto sopra) segue immediatamente il byte R/M.
        
        Questa macro prima di tutto genera il byte ModR/M usando gli operatori bitwise per inserire i valori giusti
        in ogni campo:
                y / 8   =>      Mod     (vedi descrizione delle macro usate per indicare gli operandi)
                x % 8   =>      REG
                y % 8   =>      R/M     (vedi descrizione delle macro...)
        Se dai flag risulta che sia da aggiungere un byte SIB generato nel processare uno degli argomenti (che evidentemente era la macro
        INDEX, che appunto si occupa di generare il byte SIB e settare il flag), quindi aggiungo il byte SIB.
        Se dai flag risulta che sia da aggiungere un displacement ne aggiugno uno della dimensione giusta (prendendo gli 8 bit meno
        significativi nel caso di displacement di 8 bit, copiando l'intero valore con la memcpy nel caso di displacement a 32 bit.
        Infine resetta tutti i flag, il displacement e il byte sib.
*/
#define MODRM(x,y)              do { code[i++] = ( ((y / 8) << 6) | ((x % 8) << 3) | (y % 8) ); \
                                        if(flag & FLAG_SIB) code[i++] = sib;\
                                        if(flag & FLAG_DISP8) code[i++] = disp & 0xff; \
                                        else if(flag & FLAG_DISP32) { memcpy(code + i, &disp, sizeof(disp)); i+=4; }\
                                        flag = disp = 0; sib = 0; } while(0)

/*
        Istruzioni matematiche elementari su valori in virgola mobile a doppia precisione (double).
        Queste istruzioni operano tutte sui registri XMM (che sono registri a 128 bit ma a noi ne interessano
        solo 64) e hanno tutte il prefisso 0xf2. Inoltre i loro opcode iniziano tutti con 0x0f, quindi
        definisco una macro che aggiunge questi due byte.
        Poi ciascuna istruzione ha il suo byte identificativo, seguito da un byte ModR/M (ed eventuale altra roba
        che da esso dipende) usato per indicare gli operandi
*/
#define ___SD                     do { code[i++] = 0xf2 ; code[i++] = 0x0f; } while(0)
#define ADDSD(x,y)                do { ___SD ; code[i++] = 0x58 ; MODRM(x, y); } while(0)
#define MULSD(x,y)                do { ___SD ; code[i++] = 0x59 ; MODRM(x, y); } while(0)
#define SUBSD(x,y)                do { ___SD ; code[i++] = 0x5c ; MODRM(x, y); } while(0)
#define DIVSD(x,y)                do { ___SD ; code[i++] = 0x5e ; MODRM(x, y); } while(0)

/*
        XOR per registri XMM/valori in memoria memoria a 128 bit.
        A quanto pare non esiste XORSD, comunque qui è molto semplice:
        PREFISSO (0x66) OPCODE (0x0f 0x57) ModR/M (...)
*/
#define XORPD(x, y)               do { code[i++] = 0x66; code[i++] = 0x0f ; code[i++] = 0x57; \
                                        MODRM(x, y); } while(0)

/*
        MOV tra valori a 64 bit su registri SSE (la uso per XMM, considero solo i bit meno significativi).
        Non supporta operandi immediati ma solo reg-reg, reg-mem, mem-reg
        L'istruzione viene codificata in modi diversi a seconda della direzione dello spostamento:
                mov REG, MEM: 0xf3 0x0f 0x7e
                mov MEM, REG: 0x66 0x0f 0xd6
        Nel caso di mov tra due registri credo che sia indifferente quale delle due usare.
        La macro definisce dentro al ciclo fittizio do-while una variabile is_2nd_op_mem, che assume
        valore 1 se il secondo parametro della macro è minore di 24 (ovvero se il corrispondente
        operando richiede un accesso in memoria, come indicato sopra), 0 altrimenti.
        A seconda del valore di is_2nd_op_mem vengono emessi i primi tre byte dell'istruzione
        (prefisso 0xf3/0x66 e opcode 0x0f 0x72 o 0x0f 0xd6), poi viene aggiunto un byte ModR/M con gli eventuali
        altri byte.
        Poiché la macro MODRM vuole prima il registro e poi la locazione di memoria, è necessario 
        cambiare l'ordine dei parametri a seconda del valore di is_2nd_op_mem:
                is_2nd_op_mem == 1     =>       MODRM(x, y);
                is_2nd_op_mem == 0     =>       MODRM(y, x);
        
*/
#define MOVQ(x, y)                do { int is_2nd_op_mem = (y < 24); \
                                        code[i++] = is_2nd_op_mem ? 0xf3 : 0x66; \
                                        code[i++] = is_2nd_op_mem ? 0x0f : 0x0f; \
                                        code[i++] = is_2nd_op_mem ? 0x7e : 0xd6; \
                                        MODRM((is_2nd_op_mem ? x : y), (is_2nd_op_mem ? y : x)); } while(0)

/*
        Semplici push e pop per registri a 32 o 64 bit (a seconda del processore su cui gira, credo).
        Chiaramente esistono molti altri tipi di push e pop che qui non ho implementato (per esempio
        push di un valore immediato)
*/
#define PUSH(x)                   do { code[i++] = 0x50 + x - 24; } while(0)
#define POP(x)                    do { code[i++] = 0x58 + x - 24; } while(0)

/*
        MOV per valori a 32 o 64 bit. Dipende dall'architettura (grazie a _64PREFIX):
                Compilando su x86_64
                        MOV(EAX, _EBX_(8))      =>      mov rbx, [rbx+8]
                Compilando su x86_32
                        MOV(EAX, _EBX_(8))      =>      mov ebx, [ebx+8]
        Per il resto è fondamentalmente analoga alla movq
*/
#define MOV(x, y)                 do { _64PREFIX; int is_2nd_op_mem = (y < 24); \
                                        code[i++] = is_2nd_op_mem ? 0x8b : 0x89; \
                                        MODRM((is_2nd_op_mem ? x : y), (is_2nd_op_mem ? y : x)); } while(0)


//LEA (anche questa interpretata diversamente a seconda dell'architettura). Vuole registro e memoria
#define LEA(x, y)                 do { _64PREFIX; code[i++] = 0x8d ; MODRM(x, y); } while(0)

// Banali istruzioni single-byte senza operandi
#define RET                       do { code[i++] = 0xc3; } while(0)
#define NOP                       do { code[i++] = 0x90; } while(0) // Questa l'ho messa solo per "completezza" (insomma, per quel che costava)

/*
        ADD e SUB che supportano solo come primo operando un registro e come secondo operando un valore immediato
        a 8 bit (CON SEGNO):
                Su x86_64
                        ADD_imm(ECX, 2)         =>      add rcx, 2
                Su x86_32
                        ADD_imm(ECX, 2)         =>      add ecx, 2
        Questo è un caso in cui il byte ModR/M non indica un registro e una locazione di memoria ma solo un registro
        e una modalità per l'istruzione: infatti la sub e la add sono codificate con lo stesso opcode (0x83),
        mentre un numero "di modalità" (0 per la ADD e 5 per la SUB) va passato alla MODRM dove di solito sta il primo
        registro.
*/
#define ADD_imm(r, v)             do { _64PREFIX; code[i++] = 0x83; MODRM(0, r); code[i++] = (char)v; } while(0)
#define SUB_imm(r, v)             do { _64PREFIX; code[i++] = 0x83; MODRM(5, r); code[i++] = (char)v; } while(0)

/*
        MOV per registro (32 o 64 bit a seconda dell'architettura su cui si compila, come sempre) e valore immediato a 32 bit.
        Definisce nel ciclo fittizio una variabile intera tmp_val, inizializzandola col valore immediato richiesto.
        Inserisce l'opcode 0xc7 e un byte ModR/M (modalità 0, registro r), per poi inserire i 4 byte relativi al valore immediato
        richiesto.
        
        Usa due opcode diversi: se il valore immediato sta in 32 bit usa l'opcode C7, altrimenti usa l'opcode B8
*/
#define MOV_imm(r, v)             do {  long tmp_val = ((long)v); \
                                        _64PREFIX; \
                                        if(tmp_val >> 32) { \
                                        code[i++] = 0xb8 + r - 24; \
                                        memcpy(code + i, &tmp_val, sizeof(tmp_val)); i+=sizeof(tmp_val); \
                                       } else { \
										int tmp_val_i = (int)tmp_val; code[i++] = 0xc7; \
                                        MODRM(0, r); memcpy(code + i, &tmp_val_i, sizeof(tmp_val_i)); i+=sizeof(tmp_val_i); \
                                       } } while(0)

// CALL che vuole come operando un registro contenente l'indirizzo della procedura da chiamare
#define CALL(r)                   do { code[i++] = 0xff; MODRM(2, r); } while(0)


// Aggiunto per cast float -> int [per float -> long ci vorrebbe _64PREFIX dopo 0xf2; ? ...ma funziona sulle macchine a 32 bit?]
#define CVTTSD2SI(x, y)           do { code[i++] = 0xf2; \
                                        code[i++] = 0x0f; code[i++] = 0x2c; MODRM(x, y); } while(0)

// Aggiunto per cast int -> float
#define CVTSI2SD(x, y)            do { code[i++] = 0xf2; \
                                        code[i++] = 0x0f; code[i++] = 0x2a; MODRM(x, y); } while(0)
                                        
// Aggiunti per operatore modulo
#define CDQ                       do { code[i++] = 0x99; } while(0)
#define IDIV(x)                   do { code[i++] = 0xf7; MODRM(7, x); } while(0)

// FLD carica un valore a virgola mobile sullo stack della FPU x87 [ registro st(0) ]
#define FLD(x)                    do { code[i++] = 0xdd; MODRM(0, x); } while(0)
// FSTP esegue il POP di un valore a virgola mobile dallo stack della FPU x87
#define FSTP(x)                   do { code[i++] = 0xdd; MODRM(3, x); } while(0)
#endif

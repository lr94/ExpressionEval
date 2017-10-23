bits 32             ; Va bene assemblare per i 32 bit, tanto per questo specifico
                    ; listato non cambia niente: un processore a 64 bit lo interpreterà
                    ; allo stesso modo, solo con i registri Rxx al posto dei registri Exx
                    ; in tutte le istruzioni (tranne le due mov, che restano coi registri
                    ; a 32 bit, ma a noi non interessa) 
global cpu_features

cpu_features:
    push edx        ; Salvataggio dei registri
    push ecx
    push ebx
    mov eax, 0x1    ; Eseguo istruzione CPUID (con eax=1)
    cpuid
    mov eax, edx    ; Sposto il risultato (edx) in eax
    pop ebx         ; Ripristino i registri
    pop ecx
    pop edx
    ret             ; Torno alla procedura chiamante

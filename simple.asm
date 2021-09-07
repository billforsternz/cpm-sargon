;
;  A simple CP/M program
;

bdos    equ     5

        org     100h
        ld      sp,8000h
        ld      hl,hello
        call    con_outs
m01:    ld      hl,prompt
        call    con_outs
        call    con_in
        call    con_out
        ld      hl,crlf
        call    con_outs
        cmp     'q'
        jr      z,m02
        cmp     'Q'
        jr      z,m02
        jr      m01
m02:    ld      hl,byebye
        call    con_outs
        call    exit
        ret

exit:   ld      c,0
        call    bdos
        ret

con_in: push    bc
        push    de
        push    hl
c01:    ld      c,6
        ld      e,0ffh
        call    bdos
        cmp     0
        jr      z,c01
        pop     hl
        pop     de
        pop     bc
        ret

con_out:
        push    af
        push    bc
        push    de
        push    hl
        ld      c,6
        ld      e,a
        call    bdos
        pop     hl
        pop     de
        pop     bc
        pop     af
        ret

con_outs:
        push    af
        push    bc
        push    de
        push    hl
c02:    ld      a,(hl)
        inc     hl
        cmp     0
        jr      z,c03
        call    con_out
        jr      c02
c03:    pop     hl
        pop     de
        pop     bc
        pop     af
        ret

hello:  db  'Hello this is our first CP/M program', 0dh, 0ah, 0
prompt: db  'Press q to quit, otherwise I am just going to loop and ask again', 0dh, 0ah, 0
byebye: db  'Bye bye', 0dh, 0ah, 0
crlf:   db  0dh, 0ah, 0

        ORG     100h
        jmp     main
        
bdos    equ     5

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


knight      db      "  =:^  ", 0
            db      "  /N\  ", 0
            db      " /@@@\ ", 0
bishop      db      "  (/)  ", 0
            db      "  /B\  ", 0
            db      " /@@@\ ", 0
rook        db      " |_|_| ", 0
            db      "  |R|  ", 0
            db      " /@@@\ ", 0
queen       db      "  www  ", 0
            db      "  )Q(  ", 0
            db      " /@@@\ ", 0
king        db      "  _+_  ", 0
            db      "  )K(  ", 0
            db      " /@@@\ ", 0
pawn        db      "   ()  ", 0
            db      "   )(  ", 0
            db      "  /@@\ ", 0
spaces      db      "       ", 0

position    db      "rnbqkbnr"
            db      "pppppppp"
            db      "        "
            db      "        "
            db      "        "
            db      "        "
            db      "PPPPPPPP"
            db      "RNBQKBNR"

;Print a two digit integer in reg A
printi:     push    af
            push    bc
            ld      b,0         ;divide by 10 with successive subtraction
pi01:       cp      10          ;done when result < 10
            jr      c,pi02      ;jump if done
            inc     b           ;count tens
            sub     10          ;iterate
            jr      pi01
pi02:       push    af          ;save number of ones
            ld      a,b         ;print number of tens
            or      a           ;= 0 ?
            jr      z,pi03      ;suppress leading zero
            add     a,'0'       ;to ascii
            call    con_out
pi03:       pop     af          ;print number of ones
            add     a,'0'       ;to ascii
            call    con_out
            pop     bc
            pop     af
            ret

;row_col( int row, int col )    ;row is in d, col is in e
;{
;    printf( "\x1b[%d;%dH", row, col );
;}
row_col:    push    af
            push    de
            ld      a, 01bh
            call    con_out
            ld      a,'['
            call    con_out
            ld      a,d
            push    de
            call    printi
            pop     de
            ld      a,';'
            call    con_out
            ld      a,e
            call    printi
            ld      a,'H'
            call    con_out
            pop     de
            pop     af
            ret

;void reverse()
;{
;    printf( "\x1b[7m" );
;}
rev01       db  1bh, "[7m", 0
reverse:    push    hl
            ld      hl, rev01
            call    con_outs
            pop     hl
            ret

;void normal()
;{
;    printf( "\x1b[0m" );
;}
nrm01       db  1bh, "[0m", 0
normal:     push    hl
            ld      hl, nrm01
            call    con_outs
            pop     hl
            ret

;void set_dark()
;{
;    normal();
;}
set_dark:   jmp     normal


;void set_light()
;{
;    reverse();
;}
set_light:  jmp     reverse


;void board()
;{
    ;REGISTER allocation
    ;b => rank
    ;c => line
    ;d => file
    ;e => white
dark        db      0
in_piece    db      0
space       db      0
board:
    
    ;for( int rank=0; rank<8; rank++ )
    ;{
            ld      b,0
brd01:      ld      a,b
            cp      8
            jp      nc,brd02

        ;for( int line=0; line<3; line++ )
        ;{
            ld      c,0
brd04:      ld      a,c
            cp      3
            jp      nc,brd05

            ;bool dark = ((rank&1)==1);
            ld      a,b
            and     1
            ld      (dark),a

            ;row_col( rank*3+line+5, 0 );
            ld      a,b     ;a = rank
            add     a,b     ;a = 2*rank
            add     a,b     ;a = 3*rank
            add     a,c     ;a = 3*rank + line
            add     a,1
            ld      d,a
            ld      e,0
            call    row_col

            ;for( int file=0; file<8; file++ )
            ;{
            ld      d,0
brd03:      ld      a,d
            cp      8
            jp      nc,brd06

                ;if( dark )
                ;    set_dark();
                ;else
                ;    set_light();
            ld      a,(dark)
            or      a
            jr      z,brd07
            call    set_dark
            jr      brd08
brd07:      call    set_light
brd08:

                ;char piece = position[rank][file];
            ld      hl,position
            ld      a,b     ;a = rank
            add     a,a     ;a = rank*2
            add     a,a     ;a = rank*4
            add     a,a     ;a = rank*8
            add     a,d     ;a = rank*8+file
            push    bc
            ld      c,a
            ld      b,0     ;a = rank*8+file
            add     hl,bc   ;hl -> position[]
            pop     bc
            
            ;bc temporarily = 8*line
            push    bc
            ld      a,c     ;a=line
            add     a,a     ;a=2*line
            add     a,a     ;a=4*line
            add     a,a     ;a=8*line
            ld      c,a
            ld      b,0     ;bc=8*line
            
            ;a = piece = position[rank][file]
            ld      a,(hl)  

                ;const char *s = "       ";
            ld      hl,spaces

                ;bool white = true;
            ld      e,1
                ;switch( piece )
                ;{
                ;    case 'p':  white = false;
                ;    case 'P':  s = pawn  [line];    break;
            cp      'p'
            jr      nz,brd09
            ld      e,0
            jr      brd10
brd09:      cp      'P'
            jr      nz,brd11
brd10:      ld      hl,pawn
            add     hl,bc   ;hl = pawn[line]
            jr      brd12

                    ;case 'n':  white = false;
                    ;case 'N':  s = knight[line];    break;
brd11:      cp      'n'
            jr      nz,brd13
            ld      e,0
            jr      brd14
brd13:      cp      'N'
            jr      nz,brd15
brd14:      ld      hl,knight
            add     hl,bc   ;hl = knight[line]
            jr      brd12

                    ;case 'b':  white = false;
                    ;case 'B':  s = bishop[line];    break;
brd15:      cp      'b'
            jr      nz,brd17
            ld      e,0
            jr      brd18
brd17:      cp      'B'
            jr      nz,brd19
brd18:      ld      hl,bishop
            add     hl,bc   ;hl = bishop[line]
            jr      brd12

                    ;case 'r':  white = false;
                    ;case 'R':  s = rook  [line];    break;
brd19:      cp      'r'
            jr      nz,brd21
            ld      e,0
            jr      brd22
brd21:      cp      'R'
            jr      nz,brd23
brd22:      ld      hl,rook
            add     hl,bc   ;hl = rook[line]
            jr      brd12

                    ;case 'q':  white = false;
                    ;case 'Q':  s = queen [line];    break;
brd23:      cp      'q'
            jr      nz,brd25
            ld      e,0
            jr      brd26
brd25:      cp      'Q'
            jr      nz,brd27
brd26:      ld      hl,queen
            add     hl,bc   ;hl = queen[line]
            jr      brd12

                    ;case 'k':  white = false;
                    ;case 'K':  s = king  [line];    break;
brd27:      cp      'k'
            jr      nz,brd29
            ld      e,0
            jr      brd30
brd29:      cp      'K'
            jr      nz,brd12
brd30:      ld      hl,king
            add     hl,bc   ;hl = king[line]
                ;}
brd12:      pop     bc  ;end temp bc = 8*line

                ;bool in_piece=false;
            ld      a,0
            ld      (in_piece),a

                ;if( line == 0 )
                ;    printf(s);
                ;else
                ;{
            ld      a,c
            or      a
            jr      nz,brd31
            call    con_outs
            jr      brd32
               
                    ;while( *s )
                    ;{
                    ;    char c = *s++;
                    ;    bool space = (c==' ');
                    
brd31:      ld      a,(hl)
            or      a
            jr      z,brd32
            ;inc     hl     ;deferred to below *
            cp      20h
            ld      a,0
            jr      nz,brd33
            inc     a
brd33:      ld      (space),a
            
                        ;if( white && c=='@' )
                        ;    c = ' ';
            ld      a,e
            or      a
            ld      a,(hl)
            jr      z,brd34
            cp      '@'
            jr      nz,brd34
            ld      a,' '
brd34:      inc     hl      ;* postponed from above

            ;Save reg a = char c on stack
            push    af
            
                        ;if( !in_piece )
                        ;{
                        ;    if( !space )
                        ;    {
                        ;        in_piece = true;
                        ;        if( white )
                        ;        {
                        ;            if( dark )
                        ;                set_light();
                        ;        }
                        ;        else
                        ;        {
                        ;            if( !dark )
                        ;                set_dark();
                        ;        }
                        ;    }
                        ;}
            ld      a,(in_piece)
            or      a
            jr      nz,brd35
            ld      a,(space)
            or      a
            jr      nz,brd41
            ld      a,1
            ld      (in_piece),a
            ld      a,e
            or      a
            ld      a,(dark)
            jr      z,brd36
            or      a
            jr      z,brd41
            call    set_light
            jr      brd41
brd36:      or      a
            jr      nz,brd41
            call    set_dark
            jmp     brd41

                        ;else
                        ;{
                        ;    if( space )
                        ;    {
                        ;        in_piece = false;
                        ;        if( white )
                        ;        {
                        ;            if( dark )
                        ;                set_dark();
                        ;        }
                        ;        else
                        ;        {
                        ;            if( !dark )
                        ;                set_light();
                        ;        }
                        ;    }
                        ;}
brd35:      ld      a,(space)
            or      a
            jr      z,brd41
            ld      a,0
            ld      (in_piece),a
            ld      a,e
            or      a
            ld      a,(dark)
            jr      z,brd38
            or      a
            jr      z,brd41
            call    set_dark
            jr      brd41
brd38:      or      a
            jr      nz,brd41
            call    set_light

                        ;printf("%c",c);
brd41:      pop     af
            call    con_out

                    ;END while( *s ) {
                    ;}
            jr      brd31

                ;END if( line == 0 ) printf(s); else {
                ;}
brd32:

                ;dark = !dark;
            ld      a,(dark)
            xor     1
            ld      (dark),a
            
            ;end for( int file=0; file<8; file++ )
            ;}
            inc     d
            jp      brd03
brd06:            

        ;end for( int line=0; line<3; line++ )
        ;}
            inc     c
            jp      brd04
brd05:            

    ;end for( int rank=0; rank<8; rank++ )
    ;}
            inc     b
            jp      brd01
brd02:      ret
            
;int main:
;{
;    board();
;    normal();
;    row_col(30,0);
;    return 0;
;}

main:       call    board
            call    normal
            ld      d,30
            ld      e,0
            call    row_col
            call    exit

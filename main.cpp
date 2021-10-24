/*

 Run Z80 programs with an emulator, including Sargon
 Now includes some CP/M emulation

 */

#include <stdio.h>
#include <conio.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <wchar.h>
#include <windows.h>

#include "Z80.h"
#include "util.h"
#include "thc.h"
#include "sargon-asm-interface.h"
#include "sargon-interface.h"

int setup_ansi_escape();
static bool sargon_position_tests( bool quiet );

//#define FIXED_RESPONSE
#define PROGRAM "sargon.com"  // alternatively "experiments.com"
#define CPM_FILE "/users/bill/documents/Github/SargonEmu/asm-out/" PROGRAM

// BEGIN "C" Section for interoperability with Z80 emulator
extern "C" {
static Z80 cpu;
static int dumper;
static byte z80_memory[65536];
static byte z80_io[256];
static unsigned long inst_count;
static bool done=false;
static bool triggered=false;
void WrZ80( word A,byte V )
{
    if( triggered && !done && A==0xc12c )
    {
        printf( "***** %04x: Writing %02x to %04x\n", cpu.PC.W, V, A );
    }
    z80_memory[A] = V;
}

byte RdZ80( word A )
{
    //if( 0<=A && A<0xc000 )
    //    printf( "***** Reading from %04x\n", A );
    return z80_memory[A];
}

byte DebugZ80(register Z80 *R)
{
    return 1;
}

void PatchZ80(Z80 *R)
{
    static bool first=true;
    byte reg_c = (R->BC.W & 0xff);
    byte reg_e = (R->DE.W & 0xff);

    // BDOS functions
    switch( reg_c )
    {
        case 98:  // private logging API hl->txt, b = len
        {
            const unsigned char *p = &z80_memory[R->HL.W];
            byte len = (R->BC.W >> 8) & 0xff;
            FILE *log_file;
            fopen_s( &log_file, "sargon.log", first?"wt":"at" );
            for( byte i=0; i<len; i++ )
                fprintf( log_file, "%c", *p++ );
            fprintf( log_file, "\n" );
            fclose(log_file);
            first = false;
            break;
        }

        case 99:  // private logging API hl->ASCIIZ txt
        {
            const unsigned char *p = &z80_memory[R->HL.W];
            FILE *log_file;
            fopen_s( &log_file, "sargon.log", first?"wt":"at" );
            fprintf( log_file, "%s\n", p );
            fclose(log_file);
            first = false;
            break;
        }
            
        // 0 = quit
        case 0: exit(0);    break;

        // 6 = direct CON I/O
        case 6:
        {

            // Input?
            if( reg_e == 0xff )
            {
                // Return A reg = character if available, else 0
                word c = 0;
                if( _kbhit() )
                {
                    c = _getch();
                }
                R->AF.W = (c<<8) | ((R->AF.W) & 0xff);
            }

            // Output?
            else
            {
                printf( "%c", reg_e );
            }
            break;
        }
    }
}

#define URTDA   0x60    // USART DATA 
#define URTCNT  0x61    // USART CONTROL 
#define CTC2    0x66    // BAUD RATE GENERATOR 


void OutZ80( word A,byte V )
{
    A &= 0xff;
    if( A == URTDA )
    {
        _putch(V);
    }
    else if( A == URTCNT ||  A == CTC2 )
    {
    }
    else
    {
        printf( "OUT %02x -> %02x\n", V, A );
        z80_io[A&0xff] = V;
    }
}

byte InZ80( word A )
{
    A &= 0xff;
    if( A == URTCNT )
    {
        return 0xff;    // UART status bits = 1 = ready
    }
    else if( A == URTDA )
    {
#ifdef FIXED_RESPONSE
        static unsigned int idx;
        const char *playback = "yw1d2 d4e2 e4";
        if( idx < strlen(playback) )
            return playback[idx++];
#endif
        char c;
        for(;;)
        {
            c = _getch();
            if( c != 0 )
                break;
        }
        return c;
    }
    else
    {
        printf( "IN %02x\n", A );
        return z80_io[A&0xff];
    }
}

word LoopZ80(register Z80 *R)
{
/*    if( _kbhit() )
    {
        char c;
        c = _getch();
        if( c == 0x1b )
            dumper = 10;
    } */
    return INT_NONE;
}

// END "C" Section for interoperability with Z80 emulator
};
  
void show_board_position( const unsigned char *p )
{
    printf( "\n" );
    p += 119;
    for( int row=1; row<=12; row++ )
    {
        std::string s;
        for( int col=1; col<=10; col++ )
        {
            if( 2<row && row<11 && 1<col && col<10 )
            {
                unsigned char d = *p;
                char c='.';
                switch( d & 0x07 )
                {
                    case 1: c='P'; break;
                    case 2: c='N'; break;
                    case 3: c='B'; break;
                    case 4: c='R'; break;
                    case 5: c='Q'; break;
                    case 6: c='K'; break;
                }
                if( (d&0x80) != 0 )
                    c += ' ';
                s += c;
            }
            p--;
        }
        char rank[9];
        rank[8] = '\0';
        int idx = 7;
        for( char c: s )
        {
            rank[idx] = c;
            if( idx > 0 )
                idx--;
            else
            {
                printf("%s\n",rank);
                idx = 7;
            }
        }
    }
    printf( "\n" );
}


bool hex( std::string s, unsigned int &out )
{
    unsigned int acc=0;
    unsigned char dig;
    bool ok = s.length()==4 || s.length()==2;
    if( !ok )
        return false;
    for( char c: s )
    {
        if( 'a'<=c && c<='f' )
            dig = c-'a'+10;
        else if( 'A'<=c && c<='F' )
            dig = c-'A'+10;
        else if( '0'<=c && c<='9' )
            dig = c-'0';
        else
        {
            ok = false;
            break;
        }
        acc = (acc<<4) + dig;
    }
    if( ok )
        out = acc;
    return ok;
}

int main()
{
    setup_ansi_escape();
    bool ok=true;
//    ok = sargon_position_tests(false);
//    printf( "sargon_position_tests() %s\n", ok?"Okay":"Fail" );

    z80_memory[0] = 0x3e;   // LD A,0x41  (testing)
    z80_memory[1] = 0x41;
    z80_memory[2] = 0xc3;
    z80_memory[3] = 0x00;   // JP 0x100
    z80_memory[4] = 0x01;
    z80_memory[5] = 0xed;   // BDOS: db 0edh, 0feh = emulator calls PatchZ80()
    z80_memory[6] = 0xfe;   //
    z80_memory[7] = 0xc9;   //       ret

    const char *in = CPM_FILE;
    std::ifstream fin( in, std::ofstream::binary );
    if( !fin )
    {
        printf( "Could not open file %s for reading\n", in );
        return 0;
    }
    fin.read( reinterpret_cast<char *>(&z80_memory[0x100]), 0xff00 );
/*
    std::string line;
    ok=true;
    bool had_last_line=false;
    unsigned int expected_addr = 0xc000, first=0, last=0;
    while(ok)
    {
        if( !std::getline(fin,line) )
            break;
        if( had_last_line )
            ok = false;
        if( line.length() > 80 )
            ok = false;
        if( line.length() < 1+2+4+2+2 )
            ok = false;
        if( line[0] != ':' )
            ok = false;
        if( ok )
        {
            std::string scount = line.substr(1,2);
            std::string saddr  = line.substr(1+2,4);
            std::string szero  = line.substr(1+2+4,2);
            unsigned int count, addr, zero;
            ok = hex(scount,count) && hex(saddr,addr) && hex(szero,zero);
            if( ok && count==0 && addr==0 )
            {
                had_last_line = true;
                continue;
            }
            unsigned int expected_chksum = (addr>>8) & 0xff;
            expected_chksum += (addr & 0xff);
            expected_chksum += count;
            expected_chksum += zero;
            if( ok && addr!=expected_addr )
                printf( "Warning: addr=%04x, expected_addr=%04x\n", addr, expected_addr );
            expected_addr = addr+count;
            if( ok )
            {
                if( line.length() != 1+2+4+2+count*2+2 )
                    ok = false;
            }
            for( unsigned int idx=0; ok && idx<count; idx++ )
            {
                std::string sdat = line.substr(1+2+4+2+idx*2,2);
                unsigned int dat;
                ok = hex(sdat,dat);
                if( ok )
                {
                    expected_chksum += dat;
                    z80_memory[ addr + idx ] = dat;
                    last = addr+idx;
                    if( first == 0 )
                        first = last;
                }
            }
            std::string schksum = line.substr(1+2+4+2+count*2,2);
            unsigned int chksum;
            ok = hex(schksum,chksum);
            expected_chksum = (0-expected_chksum) & 0xff;
            if( ok && expected_chksum!=chksum )
            {
                printf( "Warning, checksum error:\n"
                        "line=%s\n"
                        "expected checksum=%02x, checksum=%02x\n", line.c_str(), expected_chksum, chksum );
            }
        }
        if( !ok )
            printf( "Bad line: %s\n", line.c_str() );
    }
*/
    if( ok )
    {
        // Patches
        //z80_memory[0xde8c] = 0xc9;     // kill delay function
        ResetZ80( &cpu );
        cpu.SP.W = 0xfff0;
        RunZ80( &cpu );
    }
    return 0;
}


struct TEST
{
    const char *fen;
    int plymax_required;
    const char *solution;   // As terse string
    int        centipawns;  // score
    const char *pv;         // As natural (SAN) moves space separated
};

static TEST tests[]=
{
    // rnbqkbnr/ppp1pppp/8/3p4/3PP3/8/PPP2PPP/RNBQKBNR b KQkq e3 0 2
    // Position after 1.d4 d5 2. e4 expect 2...dxe4
    { "rnbqkbnr/ppp1pppp/8/3p4/3PP3/8/PPP2PPP/RNBQKBNR b KQkq e3 0 2", 1, "d5e4",
        0, "" }

#if 0
    // Position after 1.e4 e5 2. Nf3 expect 2...Nc6
    { "rnbqkbnr/pppp1ppp/8/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R b KQkq - 1 2", 1, "b8c6",
        0, "" },

    // 1.e4 e5 2.Nf3 Nc6 3.Bc4 Nf6 4.Ng5 d5 5.exd5
    { "r1bqkb1r/ppp2ppp/2n2n2/3Pp1N1/2B5/8/PPPP1PPP/RNBQK2R b KQkq - 0 5", 2, "c8g4",
        0, "" }
#endif

};

static bool sargon_position_tests( bool quiet )
{
    bool ok = true;

    // A little test of some of our internal machinery
    thc::ChessPosition cp, cp2;
    /*std::string en_passant_fen1 = "r4rk1/pb1pq1pp/5p2/2ppP3/5P2/2Q5/PPP3PP/2KR1B1R w - c6 0 15";
    cp.Forsyth( en_passant_fen1.c_str() );   // en passant provides a good workout on import and export
    sargon_import_position(cp);
    sargon_export_position(cp2);
    std::string en_passant_fen2 = cp2.ForsythPublish();
    if( en_passant_fen2 != en_passant_fen1 )
        printf( "Unexpected internal event, expected en_passant_fen2=%s  to equal en_passant_fen1=%s\n", en_passant_fen2.c_str(), en_passant_fen1.c_str() ); */

    printf( "* Known position tests\n" );
    int nbr_tests = sizeof(tests)/sizeof(tests[0]);
    int nbr_tests_to_run = nbr_tests;
    for( int i=0; i<nbr_tests_to_run; i++ )
    {
        TEST *pt = &tests[i];
        thc::ChessRules cr;
        cr.Forsyth(pt->fen);
        if( !quiet )
        {
            std::string intro = util::sprintf("\nTest position %d is", i+1 );
            std::string s = cr.ToDebugStr( intro.c_str() );
            printf( "%s\n", s.c_str() );
            printf( "Expected PV=%s\n", pt->pv );
        }
        printf( "Test %d of %d: PLYMAX=%d:", i+1, nbr_tests_to_run, pt->plymax_required );
        if( 0 == strcmp(pt->fen,"2rq1r1k/3npp1p/3p1n1Q/pp1P2N1/8/2P4P/1P4P1/R4R1K w - - 0 1") )
            printf( " (sorry this particular test is very slow) :" );
        PV pv;
        sargon_run_engine( cr, pt->plymax_required, pv, false );
        std::vector<thc::Move> &v = pv.variation;
        std::string s_pv;
        std::string spacer;
        for( thc::Move mv: v )
        {
            s_pv += spacer;
            spacer = " ";
            s_pv += mv.NaturalOut(&cr);
            cr.PlayMove(mv);
        }
        std::string sargon_move = sargon_export_move(BESTM);
        bool pass = (s_pv==std::string(pt->pv));
        if( !pass )
            printf( "FAIL\n Fail reason: Expected PV=%s, Calculated PV=%s\n", pt->pv, s_pv.c_str() );
        else
        {
            pass = (sargon_move==std::string(pt->solution));
            if( !pass )
                printf( "FAIL\n Fail reason: Expected move=%s, Calculated move=%s\n", pt->solution, sargon_move.c_str() );
            else if( *pt->pv  )
            {
                pass = (pv.value==pt->centipawns);
                if( !pass )
                    printf( "FAIL\n Fail reason: Expected centipawns=%d, Calculated centipawns=%d\n", pt->centipawns, pv.value );
            }
        }
        if( pass )
            printf( " PASS\n" );
        else
            ok = false;
    }
    return ok;
}

static int attack_count = 0;

extern "C" {
    void callback( uint32_t reg_edi, uint32_t reg_esi, uint32_t reg_ebp, uint32_t reg_esp,
                   uint32_t reg_ebx, uint32_t reg_edx, uint32_t reg_ecx, uint32_t reg_eax,
                   uint32_t reg_eflags )
    {
        uint32_t *sp = &reg_edi;
        sp--;
        static bool triggered = false;
        static bool done = false;
        if( done )
            return;

        // expecting code at return address to be 0xeb = 2 byte opcode, (0xeb + 8 bit relative jump),
        uint32_t ret_addr = *sp;
        const unsigned char *code = (const unsigned char *)ret_addr;
        const char *msg = (const char *)(code+2);   // ASCIIZ text should come after that
#if 0
        std::string s(msg);
        if( s == "*hit POINTS" )
        {
            triggered = true;
            const unsigned char *p = peek(BOARDA);
            show_board_position(p);
        }
        else if( s == "end of POINTS()" )
        {
            printf( "end of POINTS(): al=%02x cx=%04x, dx=%04x, bx=%04x\n",
                        reg_eax&0xff,
                        reg_ecx&0xffff,
                        reg_edx&0xffff,
                        reg_ebx&0xffff
                        );
        }
       else if( triggered && s == "*hit ATTACK" )
        {
            attack_count++;
            if( attack_count > 1 )
                done = true;
        }
        else if( triggered && s == "*after PATH" )
        {
            unsigned char m2 = peekb(M2);
            unsigned char m1 = peekb(M1);
            unsigned char p1 = peekb(P1);
            unsigned char p2 = peekb(P2);
            unsigned char t2 = peekb(T2);
            printf( "M2=%d M1=%02x P1=%02x P2=%02x al=%02x\n",
                        m2,m1,p1,p2,
                        reg_eax&0xff );
        }
        if( *msg == '*' )
            printf( "Callback %s\n", msg );
        if( s == "*hit FNDMOV" )
        {
            attack_count=0;
        }
        if( s == "*hit ATTACK" )
        {
            attack_count++;
        }
        if( (attack_count==8 || attack_count==9) && s=="AT10" )
        {
            printf( "AT10: al=%02x cx=%04x, dx=%04x, bx=%04x\n",
                        reg_eax&0xff,
                        reg_ecx&0xffff,
                        reg_edx&0xffff,
                        reg_ebx&0xffff
                        );
            if( attack_count==9 )
                printf( "debug xlat works\n" );
        }
#endif
    }
};

int setup_ansi_escape()
{
    // Set output mode to handle virtual terminal sequences
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE)
    {
        return false;
    }
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    if (hIn == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    DWORD dwOriginalOutMode = 0;
    DWORD dwOriginalInMode = 0;
    if (!GetConsoleMode(hOut, &dwOriginalOutMode))
    {
        return false;
    }
    if (!GetConsoleMode(hIn, &dwOriginalInMode))
    {
        return false;
    }

    DWORD dwRequestedOutModes = ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN;
    DWORD dwRequestedInModes = ENABLE_VIRTUAL_TERMINAL_INPUT;

    DWORD dwOutMode = dwOriginalOutMode | dwRequestedOutModes;
    if (!SetConsoleMode(hOut, dwOutMode))
    {
        // we failed to set both modes, try to step down mode gracefully.
        dwRequestedOutModes = ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        dwOutMode = dwOriginalOutMode | dwRequestedOutModes;
        if (!SetConsoleMode(hOut, dwOutMode))
        {
            // Failed to set any VT mode, can't do anything here.
            return -1;
        }
    }

//    DWORD dwInMode = dwOriginalInMode | ENABLE_VIRTUAL_TERMINAL_INPUT;
//    if (!SetConsoleMode(hIn, dwInMode))
//    {
//        // Failed to set VT input mode, can't do anything here.
//        return -1;
//    }
//
    return 0;
}


// Some old versions of DebugZ80(), for shows how to trace various issues
#if 0

byte DebugZ80(register Z80 *R)
{
    word pc = R->PC.W;
    if( pc==0xd1aa )
    {
        const unsigned char *p = &z80_memory[0xc0b4];
        show_board_position( p );
        printf( "Points(): %04x>> A=%02x BC=%04x DE=%04x HL=%04x SP=%04x\n",
            R->PC.W,
            (R->AF.W >> 8) & 0xff,
            R->BC.W,
            R->DE.W,
            R->HL.W,
            R->SP.W );
    }
    else if( pc==0xd2f2 )
    {
        done = true;
        printf( "hit end of POINTS(): %04x>> A=%02x BC=%04x DE=%04x HL=%04x SP=%04x\n",
            R->PC.W,
            (R->AF.W >> 8) & 0xff,
            R->BC.W,
            R->DE.W,
            R->HL.W,
            R->SP.W );
    }
    return 1;
}

byte DebugZ80(register Z80 *R)
{
    static bool m2eq32;
    if( done )
        return 1;
    word pc = R->PC.W;
    if( pc==0xd1aa )
    {
        triggered = true;
        const unsigned char *p = &z80_memory[0xc0b4];
        show_board_position( p );
        printf( "Points(): %04x>> A=%02x BC=%04x DE=%04x HL=%04x SP=%04x\n",
            R->PC.W,
            (R->AF.W >> 8) & 0xff,
            R->BC.W,
            R->DE.W,
            R->HL.W,
            R->SP.W );
    }
    else if( pc==0xd231 )
    {
        done = true;
    }
    else if( pc==0xd2f2 )
    {
        done = true;
        printf( "hit end of POINTS(): %04x>> A=%02x BC=%04x DE=%04x HL=%04x SP=%04x\n",
            R->PC.W,
            (R->AF.W >> 8) & 0xff,
            R->BC.W,
            R->DE.W,
            R->HL.W,
            R->SP.W );
    }
    else if( 0xd1aa<=pc && pc<=0xd2f2  )
    {
          printf( "In POINTS(): %04x>> A=%02x BC=%04x DE=%04x HL=%04x SP=%04x\n",
            R->PC.W,
            (R->AF.W >> 8) & 0xff,
            R->BC.W,
            R->DE.W,
            R->HL.W,
            R->SP.W );
    }
    else if( 0xcce5<=pc && pc<=0xcd12 && triggered  )
    {
        printf( "In PATH(): %04x>> A=%02x BC=%04x DE=%04x HL=%04x SP=%04x\n",
            R->PC.W,
            (R->AF.W >> 8) & 0xff,
            R->BC.W,
            R->DE.W,
            R->HL.W,
            R->SP.W );
    }
    else if( 0xcf52<=pc && pc<=0xd005 && triggered && m2eq32 )
    {
        printf( "In ATTACK(): %04x>> A=%02x BC=%04x DE=%04x HL=%04x, IX=%04X, IY=%04X SP=%04x\n",
            R->PC.W,
            (R->AF.W >> 8) & 0xff,
            R->BC.W,
            R->DE.W,
            R->HL.W,
            R->IX.W,
            R->IY.W,
            R->SP.W );
    }
    else if( 0xd008<=pc && pc<=0xd04f && triggered  )
    { 
        printf( "In ATKSAV(): %04x>> A=%02x BC=%04x DE=%04x HL=%04x SP=%04x\n",
            R->PC.W,
            (R->AF.W >> 8) & 0xff,
            R->BC.W,
            R->DE.W,
            R->HL.W,
            R->SP.W );
    }
    else if( pc==0xcf6c && triggered  )
    { 
        /*printf( "EMU: After Path: %04x>> A=%02x BC=%04x DE=%04x HL=%04x SP=%04x\n",
            R->PC.W,
            (R->AF.W >> 8) & 0xff,
            R->BC.W,
            R->DE.W,
            R->HL.W,
            R->SP.W ); */
            unsigned char m1 = z80_memory[0xc003]; // M1 
            unsigned char m2 = z80_memory[0xc005]; // M2 
            unsigned char p1 = z80_memory[0xc025]; // P1
            unsigned char p2 = z80_memory[0xc026]; // P2
            unsigned char t2 = z80_memory[0xc00d]; // T2 
            printf( "M2=%d M1=%02x P1=%02x P2=%02x al=%02x\n",
                        m2,m1,p1,p2,
                        (R->AF.W >> 8) & 0xff );
            m2eq32 = (m2==32);
    }
    return 1;
}

byte DebugZ80(register Z80 *R)
{
    word pc = R->PC.W;
    static int attack=0, total=0, triggered=-1;
    bool check_ok = (z80_memory[0xc006] == 0xc1);
    if( triggered == -1 && !check_ok )
        triggered = 100;
    if( triggered > 0 )
    {
        printf( "(%d) %04x (%c)> A=%02x BC=%04x DE=%04x HL=%04x SP=%04x IX=%04x IY=%04x\n",
            triggered,
            R->PC.W, (check_ok?'Y':'N'),
            (R->AF.W >> 8) & 0xff,
            R->BC.W,
            R->DE.W,
            R->HL.W,
            R->SP.W,
            R->IX.W,
            R->IY.W );
        triggered--;
    }
    if( pc == 0xd451 )
    {
        printf( "emu FNDMOV (%c)\n", (check_ok?'Y':'N') );
        attack = 0;
        total = 0;
    }
    if( pc == 0xcf52 )
        printf( "emu ATTACK %d (%c)\n", ++attack, (check_ok?'Y':'N') );

#define RANGE1 0xd195
#define RANGE2 0xd1a9
    if( RANGE1<=R->PC.W && R->PC.W<=RANGE2 )
    {
        printf( "(%d) %04x (%c)> A=%02x BC=%04x DE=%04x HL=%04x SP=%04x IX=%04x IY=%04x\n",
            triggered,
            R->PC.W, (check_ok?'Y':'N'),
            (R->AF.W >> 8) & 0xff,
            R->BC.W,
            R->DE.W,
            R->HL.W,
            R->SP.W,
            R->IX.W,
            R->IY.W );
    }
    if( pc == 0xcf68 && total<300)
    {
        if( attack == 9 )
        {
            if( triggered == -1 )
            {
                //triggered = 500;
                printf( "debug emu fail point\n" );
            }
        }
        printf( "emu AT10(%d,%d): %04x (%c)> A=%02x BC=%04x DE=%04x HL=%04x SP=%04x\n",
            attack,
            total,
            R->PC.W, (check_ok?'Y':'N'),
            (R->AF.W >> 8) & 0xff,
            R->BC.W,
            R->DE.W,
            R->HL.W,
            R->SP.W );
        total++;
    }
    return 1;
}

byte DebugZ80(register Z80 *R)
{
    word pc = R->PC.W;
    word sp = R->SP.W;
    static int init=20;
    static int kill=258;
    static int attack=0;
    static bool primed=false;
    if( primed && pc==0xcf68 && kill>0 )
    {
        kill--;
        if( kill == 0 )
            printf( "Dumping killed\n" );
    }
    if( kill == 0 )
        return 1;
    if( pc == 0xd76f )
        printf( "hit CPTRMV\n" );
    if( pc == 0xd451 )
    {
        printf( "hit FNDMOV\n" );
        attack = 0;
    }
//#define RANGE1 0xd451
//#define RANGE2 0xd5a0
    if( pc == 0xd1aa )
    {
        printf( "hit POINTS\n" );
    }
//#define RANGE1 0xd1aa
//#define RANGE2 0xd2fc
    if( pc == 0xcf52 )
    {
        printf( "hit ATTACK %d\n", ++attack );
        if( attack == 13 )
            primed = true;
    }
    if( pc==0xcf68 && (attack==13||attack==14) )
    {
        printf( "hit AT10: %04x>> A=%02x BC=%04x DE=%04x HL=%04x SP=%04x\n",
            R->PC.W,
            (R->AF.W >> 8) & 0xff,
            R->BC.W,
            R->DE.W,
            R->HL.W,
            R->SP.W );
    }
#define RANGE1 0xcf52
#define RANGE2 0xd005

    if( primed && RANGE1<=R->PC.W && R->PC.W<=RANGE2 )
    {
        printf( "%04x>> A=%02x BC=%04x DE=%04x HL=%04x SP=%04x\n",
            R->PC.W,
            (R->AF.W >> 8) & 0xff,
            R->BC.W,
            R->DE.W,
            R->HL.W,
            R->SP.W );
    }
 /* if( dumper > 0 )
    {
        dumper--;
        printf( "%04x> A=%02x BC=%04x DE=%04x HL=%04x SP=%04x\n",
            R->PC.W,
            (R->AF.W >> 8) & 0xff,
            R->BC.W,
            R->DE.W,
            R->HL.W,
            R->SP.W );
    } */
    return 1;
}

#endif


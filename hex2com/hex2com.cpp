// hex2com.cpp : This file contains the 'main' function. Program execution begins and ends there.
//


#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include "../util.h"

bool hex2com( const char *nbr, const char *in );

int main( int argc, const char *argv[] )
{
    bool ok = (argc==3);
#ifdef _DEBUG
    ok = hex2com( "/users/bill/documents/Github/SargonEmu/asm-out/simple.hex",
                  "/users/bill/documents/Github/SargonEmu/asm-out/simple.com" );
#else
    if( ok )
        ok = hex2com( argv[1], argv[2] );
    else
    {
        printf( "Usage: hex2com in.hex out.com\n" );
        return -1;
    }
#endif
    if( !ok )
        printf( "Abnormal exit\n" );
    return 0;
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

unsigned char z80_memory[65536];

bool hex2com( const char *in, const char *out )
{
    std::ifstream fin( in );
    if( !fin )
    {
        printf( "Could not open file %s for reading\n", in );
        return false;
    }
    std::ofstream fout( out, std::ofstream::binary );
    if( !fout )
    {
        printf( "Could not open file %s for writing\n", out );
        return false;
    }
    std::string line;
    bool ok=true;
    bool had_last_line=false;
    unsigned int expected_addr = 0x100, lo=0xffff, hi=0;
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
            unsigned int expected_chksum = (addr>>8) & 0xff;
            expected_chksum += (addr & 0xff);
            expected_chksum += count;
            expected_chksum += zero;
            if( ok && zero==0 && addr!=expected_addr )
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
                    if( addr+idx < lo )
                        lo = addr+idx;
                    if( addr+idx > hi )
                        hi = addr+idx;
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
            if( ok && zero==1 && count==0 && addr==0 )
                had_last_line = true;
        }
        if( !ok )
            printf( "Bad line: %s\n", line.c_str() );
    }
    if( lo != 0x100 )
    {
        ok = false;
        printf( "Info: lo=0x%04x, hi=0x%04x\n", lo, hi );
        printf( "Error: CP/M .com files are expected to start at 0x100\n" );
    }
    else
    {

        // CP/M com files are padded out to 128 byte boundaries with 0x1a 
        while( (hi&0x7f) != 0x7f )
        {
            hi++;
            z80_memory[ hi ] = 0x1a;
        }
        hi++;
        fout.write( reinterpret_cast<char *>(&z80_memory[0x100]), hi-0x100 );
    }
    return ok;
}

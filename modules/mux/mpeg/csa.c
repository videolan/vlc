/*****************************************************************************
 * libcsa.c: CSA scrambler/descrambler
 *****************************************************************************
 * Copyright (C) 2004-2005 Laurent Aimar
 * Copyright (C) the deCSA authors
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Jean-Paul Saman <jpsaman #_at_# m2x.nl>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>

#include "csa.h"

struct csa_t
{
    /* odd and even keys */
    uint8_t o_ck[8];
    uint8_t e_ck[8];

    uint8_t o_kk[57];
    uint8_t e_kk[57];

    /* cypher state */
    int     A[11];
    int     B[11];
    int     X, Y, Z;
    int     D, E, F;
    int     p, q, r;

    bool    use_odd;
};

static void csa_ComputeKey( uint8_t kk[57], uint8_t ck[8] );

static void csa_StreamCypher( csa_t *c, int b_init, uint8_t *ck, uint8_t *sb, uint8_t *cb );

static void csa_BlockDecypher( uint8_t kk[57], uint8_t ib[8], uint8_t bd[8] );
static void csa_BlockCypher( uint8_t kk[57], uint8_t bd[8], uint8_t ib[8] );

/*****************************************************************************
 * csa_New:
 *****************************************************************************/
csa_t *csa_New( void )
{
    return calloc( 1, sizeof( csa_t ) );
}

/*****************************************************************************
 * csa_Delete:
 *****************************************************************************/
void csa_Delete( csa_t *c )
{
    free( c );
}

/*****************************************************************************
 * csa_SetCW:
 *****************************************************************************/
int csa_SetCW( vlc_object_t *p_caller, csa_t *c, char *psz_ck, bool set_odd )
{
    if ( !c )
    {
        msg_Dbg( p_caller, "no CSA found" );
        return VLC_ENOOBJ;
    }
    /* skip 0x */
    if( psz_ck[0] == '0' && ( psz_ck[1] == 'x' || psz_ck[1] == 'X' ) )
    {
        psz_ck += 2;
    }
    if( strlen( psz_ck ) != 16 )
    {
        msg_Warn( p_caller, "invalid csa ck (it must be 16 chars long)" );
        return VLC_EBADVAR;
    }
    else
    {
        uint64_t i_ck = strtoull( psz_ck, NULL, 16 );
        uint8_t  ck[8];
        int      i;

        for( i = 0; i < 8; i++ )
        {
            ck[i] = ( i_ck >> ( 56 - 8*i) )&0xff;
        }
#ifndef TS_NO_CSA_CK_MSG
        msg_Dbg( p_caller, "using CSA (de)scrambling with %s "
                 "key=%x:%x:%x:%x:%x:%x:%x:%x", set_odd ? "odd" : "even",
                 ck[0], ck[1], ck[2], ck[3], ck[4], ck[5], ck[6], ck[7] );
#endif
        if( set_odd )
        {
            memcpy( c->o_ck, ck, 8 );
            csa_ComputeKey( c->o_kk, ck );
        }
        else
        {
            memcpy( c->e_ck , ck, 8 );
            csa_ComputeKey( c->e_kk , ck );
        }
        return VLC_SUCCESS;
    }
}

/*****************************************************************************
 * csa_UseKey:
 *****************************************************************************/
int csa_UseKey( vlc_object_t *p_caller, csa_t *c, bool use_odd )
{
    if ( !c ) return VLC_ENOOBJ;
    c->use_odd = use_odd;
#ifndef TS_NO_CSA_CK_MSG
        msg_Dbg( p_caller, "using the %s key for scrambling",
                 use_odd ? "odd" : "even" );
#endif
    return VLC_SUCCESS;
}

/*****************************************************************************
 * csa_Decrypt:
 *****************************************************************************/
void csa_Decrypt( csa_t *c, uint8_t *pkt, int i_pkt_size )
{
    uint8_t *ck;
    uint8_t *kk;

    uint8_t  ib[8], stream[8], block[8];

    int     i_hdr, i_residue;
    int     i, j, n;

    /* transport scrambling control */
    if( (pkt[3]&0x80) == 0 )
    {
        /* not scrambled */
        return;
    }
    if( pkt[3]&0x40 )
    {
        ck = c->o_ck;
        kk = c->o_kk;
    }
    else
    {
        ck = c->e_ck;
        kk = c->e_kk;
    }

    /* clear transport scrambling control */
    pkt[3] &= 0x3f;

    i_hdr = 4;
    if( pkt[3]&0x20 )
    {
        /* skip adaption field */
        i_hdr += pkt[4] + 1;
    }

    if( 188 - i_hdr < 8 )
        return;

    /* init csa state */
    csa_StreamCypher( c, 1, ck, &pkt[i_hdr], ib );

    /* */
    n = (i_pkt_size - i_hdr) / 8;
    if( n < 0 )
        return;
 
    i_residue = (i_pkt_size - i_hdr) % 8;
    for( i = 1; i < n + 1; i++ )
    {
        csa_BlockDecypher( kk, ib, block );
        if( i != n )
        {
            csa_StreamCypher( c, 0, ck, NULL, stream );
            for( j = 0; j < 8; j++ )
            {
                /* xor ib with stream */
                ib[j] = pkt[i_hdr+8*i+j] ^ stream[j];
            }
        }
        else
        {
            /* last block */
            for( j = 0; j < 8; j++ )
            {
                ib[j] = 0;
            }
        }
        /* xor ib with block */
        for( j = 0; j < 8; j++ )
        {
            pkt[i_hdr+8*(i-1)+j] = ib[j] ^ block[j];
        }
    }

    if( i_residue > 0 )
    {
        csa_StreamCypher( c, 0, ck, NULL, stream );
        for( j = 0; j < i_residue; j++ )
        {
            pkt[i_pkt_size - i_residue + j] ^= stream[j];
        }
    }
}

/*****************************************************************************
 * csa_Encrypt:
 *****************************************************************************/
void csa_Encrypt( csa_t *c, uint8_t *pkt, int i_pkt_size )
{
    uint8_t *ck;
    uint8_t *kk;

    int i, j;
    int i_hdr = 4; /* hdr len */
    uint8_t  ib[184/8+2][8], stream[8], block[8];
    int n, i_residue;

    /* set transport scrambling control */
    pkt[3] |= 0x80;

    if( c->use_odd )
    {
        pkt[3] |= 0x40;
        ck = c->o_ck;
        kk = c->o_kk;
    }
    else
    {
        ck = c->e_ck;
        kk = c->e_kk;
    }

    /* hdr len */
    i_hdr = 4;
    if( pkt[3]&0x20 )
    {
        /* skip adaption field */
        i_hdr += pkt[4] + 1;
    }
    n = (i_pkt_size - i_hdr) / 8;
    i_residue = (i_pkt_size - i_hdr) % 8;

    if( n <= 0 )
    {
        pkt[3] &= 0x3f;
        return;
    }

    /* */
    for( i = 0; i < 8; i++ )
    {
        ib[n+1][i] = 0;
    }
    for( i = n; i  > 0; i-- )
    {
        for( j = 0; j < 8; j++ )
        {
            block[j] = pkt[i_hdr+8*(i-1)+j] ^ib[i+1][j];
        }
        csa_BlockCypher( kk, block, ib[i] );
    }

    /* init csa state */
    csa_StreamCypher( c, 1, ck, ib[1], stream );

    for( i = 0; i < 8; i++ )
    {
        pkt[i_hdr+i] = ib[1][i];
    }
    for( i = 2; i < n+1; i++ )
    {
        csa_StreamCypher( c, 0, ck, NULL, stream );
        for( j = 0; j < 8; j++ )
        {
            pkt[i_hdr+8*(i-1)+j] = ib[i][j] ^ stream[j];
        }
    }
    if( i_residue > 0 )
    {
        csa_StreamCypher( c, 0, ck, NULL, stream );
        for( j = 0; j < i_residue; j++ )
        {
            pkt[i_pkt_size - i_residue + j] ^= stream[j];
        }
    }
}

/*****************************************************************************
 * Divers
 *****************************************************************************/
static const uint8_t key_perm[0x40] =
{
    0x12,0x24,0x09,0x07,0x2A,0x31,0x1D,0x15,0x1C,0x36,0x3E,0x32,0x13,0x21,0x3B,0x40,
    0x18,0x14,0x25,0x27,0x02,0x35,0x1B,0x01,0x22,0x04,0x0D,0x0E,0x39,0x28,0x1A,0x29,
    0x33,0x23,0x34,0x0C,0x16,0x30,0x1E,0x3A,0x2D,0x1F,0x08,0x19,0x17,0x2F,0x3D,0x11,
    0x3C,0x05,0x38,0x2B,0x0B,0x06,0x0A,0x2C,0x20,0x3F,0x2E,0x0F,0x03,0x26,0x10,0x37,
};

static void csa_ComputeKey( uint8_t kk[57], uint8_t ck[8] )
{
    int i,j,k;
    int bit[64];
    int newbit[64];
    int kb[8][9];

    /* from a cw create 56 key bytes, here kk[1..56] */

    /* load ck into kb[7][1..8] */
    for( i = 0; i < 8; i++ )
    {
        kb[7][i+1] = ck[i];
    }

    /* calculate all kb[6..1][*] */
    for( i = 0; i < 7; i++ )
    {
        /* do a 64 bit perm on kb */
        for( j = 0; j < 8; j++ )
        {
            for( k = 0; k < 8; k++ )
            {
                bit[j*8+k] = (kb[7-i][1+j] >> (7-k)) & 1;
                newbit[key_perm[j*8+k]-1] = bit[j*8+k];
            }
        }
        for( j = 0; j < 8; j++ )
        {
            kb[6-i][1+j] = 0;
            for( k = 0; k < 8; k++ )
            {
                kb[6-i][1+j] |= newbit[j*8+k] << (7-k);
            }
        }
    }

    /* xor to give kk */
    for( i = 0; i < 7; i++ )
    {
        for( j = 0; j < 8; j++ )
        {
            kk[1+i*8+j] = kb[1+i][1+j] ^ i;
        }
    }
}


static const int sbox1[0x20] = {2,0,1,1,2,3,3,0, 3,2,2,0,1,1,0,3, 0,3,3,0,2,2,1,1, 2,2,0,3,1,1,3,0};
static const int sbox2[0x20] = {3,1,0,2,2,3,3,0, 1,3,2,1,0,0,1,2, 3,1,0,3,3,2,0,2, 0,0,1,2,2,1,3,1};
static const int sbox3[0x20] = {2,0,1,2,2,3,3,1, 1,1,0,3,3,0,2,0, 1,3,0,1,3,0,2,2, 2,0,1,2,0,3,3,1};
static const int sbox4[0x20] = {3,1,2,3,0,2,1,2, 1,2,0,1,3,0,0,3, 1,0,3,1,2,3,0,3, 0,3,2,0,1,2,2,1};
static const int sbox5[0x20] = {2,0,0,1,3,2,3,2, 0,1,3,3,1,0,2,1, 2,3,2,0,0,3,1,1, 1,0,3,2,3,1,0,2};
static const int sbox6[0x20] = {0,1,2,3,1,2,2,0, 0,1,3,0,2,3,1,3, 2,3,0,2,3,0,1,1, 2,1,1,2,0,3,3,0};
static const int sbox7[0x20] = {0,3,2,2,3,0,0,1, 3,0,1,3,1,2,2,1, 1,0,3,3,0,1,1,2, 2,3,1,0,2,3,0,2};

static void csa_StreamCypher( csa_t *c, int b_init, uint8_t *ck, uint8_t *sb, uint8_t *cb )
{
    int i,j, k;
    int extra_B;
    int s1,s2,s3,s4,s5,s6,s7;
    int next_A1;
    int next_B1;
    int next_E;

    if( b_init )
    {
        // load first 32 bits of CK into A[1]..A[8]
        // load last  32 bits of CK into B[1]..B[8]
        // all other regs = 0
        for( i = 0; i < 4; i++ )
        {
            c->A[1+2*i+0] = ( ck[i] >> 4 )&0x0f;
            c->A[1+2*i+1] = ( ck[i] >> 0 )&0x0f;

            c->B[1+2*i+0] = ( ck[4+i] >> 4 )&0x0f;
            c->B[1+2*i+1] = ( ck[4+i] >> 0 )&0x0f;
        }

        c->A[9] = c->A[10] = 0;
        c->B[9] = c->B[10] = 0;

        c->X = c->Y = c->Z = 0;
        c->D = c->E = c->F = 0;
        c->p = c->q = c->r = 0;
    }

    // 8 bytes per operation
    for( i = 0; i < 8; i++ )
    {
        int op = 0;
        int in1 = 0;    /* gcc warn */
        int in2 = 0;

        if( b_init )
        {
            in1 = ( sb[i] >> 4 )&0x0f;
            in2 = ( sb[i] >> 0 )&0x0f;
        }

        // 2 bits per iteration
        for( j = 0; j < 4; j++ )
        {
            // from A[1]..A[10], 35 bits are selected as inputs to 7 s-boxes
            // 5 bits input per s-box, 2 bits output per s-box
            s1 = sbox1[ (((c->A[4]>>0)&1)<<4) | (((c->A[1]>>2)&1)<<3) | (((c->A[6]>>1)&1)<<2) | (((c->A[7]>>3)&1)<<1) | (((c->A[9]>>0)&1)<<0) ];
            s2 = sbox2[ (((c->A[2]>>1)&1)<<4) | (((c->A[3]>>2)&1)<<3) | (((c->A[6]>>3)&1)<<2) | (((c->A[7]>>0)&1)<<1) | (((c->A[9]>>1)&1)<<0) ];
            s3 = sbox3[ (((c->A[1]>>3)&1)<<4) | (((c->A[2]>>0)&1)<<3) | (((c->A[5]>>1)&1)<<2) | (((c->A[5]>>3)&1)<<1) | (((c->A[6]>>2)&1)<<0) ];
            s4 = sbox4[ (((c->A[3]>>3)&1)<<4) | (((c->A[1]>>1)&1)<<3) | (((c->A[2]>>3)&1)<<2) | (((c->A[4]>>2)&1)<<1) | (((c->A[8]>>0)&1)<<0) ];
            s5 = sbox5[ (((c->A[5]>>2)&1)<<4) | (((c->A[4]>>3)&1)<<3) | (((c->A[6]>>0)&1)<<2) | (((c->A[8]>>1)&1)<<1) | (((c->A[9]>>2)&1)<<0) ];
            s6 = sbox6[ (((c->A[3]>>1)&1)<<4) | (((c->A[4]>>1)&1)<<3) | (((c->A[5]>>0)&1)<<2) | (((c->A[7]>>2)&1)<<1) | (((c->A[9]>>3)&1)<<0) ];
            s7 = sbox7[ (((c->A[2]>>2)&1)<<4) | (((c->A[3]>>0)&1)<<3) | (((c->A[7]>>1)&1)<<2) | (((c->A[8]>>2)&1)<<1) | (((c->A[8]>>3)&1)<<0) ];

            /* use 4x4 xor to produce extra nibble for T3 */
            extra_B = ( ((c->B[3]&1)<<3) ^ ((c->B[6]&2)<<2) ^ ((c->B[7]&4)<<1) ^ ((c->B[9]&8)>>0) ) |
                      ( ((c->B[6]&1)<<2) ^ ((c->B[8]&2)<<1) ^ ((c->B[3]&8)>>1) ^ ((c->B[4]&4)>>0) ) |
                      ( ((c->B[5]&8)>>2) ^ ((c->B[8]&4)>>1) ^ ((c->B[4]&1)<<1) ^ ((c->B[5]&2)>>0) ) |
                      ( ((c->B[9]&4)>>2) ^ ((c->B[6]&8)>>3) ^ ((c->B[3]&2)>>1) ^ ((c->B[8]&1)>>0) ) ;

            // T1 = xor all inputs
            // in1,in2, D are only used in T1 during initialisation, not generation
            next_A1 = c->A[10] ^ c->X;
            if( b_init ) next_A1 = next_A1 ^ c->D ^ ((j % 2) ? in2 : in1);

            // T2 =  xor all inputs
            // in1,in2 are only used in T1 during initialisation, not generation
            // if p=0, use this, if p=1, rotate the result left
            next_B1 = c->B[7] ^ c->B[10] ^ c->Y;
            if( b_init) next_B1 = next_B1 ^ ((j % 2) ? in1 : in2);

            // if p=1, rotate left
            if( c->p ) next_B1 = ( (next_B1 << 1) | ((next_B1 >> 3) & 1) ) & 0xf;

            // T3 = xor all inputs
            c->D = c->E ^ c->Z ^ extra_B;

            // T4 = sum, carry of Z + E + r
            next_E = c->F;
            if( c->q )
            {
                c->F = c->Z + c->E + c->r;
                // r is the carry
                c->r = (c->F >> 4) & 1;
                c->F = c->F & 0x0f;
            }
            else
            {
                c->F = c->E;
            }
            c->E = next_E;

            for( k = 10; k > 1; k-- )
            {
                c->A[k] = c->A[k-1];
                c->B[k] = c->B[k-1];
            }
            c->A[1] = next_A1;
            c->B[1] = next_B1;

            c->X = ((s4&1)<<3) | ((s3&1)<<2) | (s2&2) | ((s1&2)>>1);
            c->Y = ((s6&1)<<3) | ((s5&1)<<2) | (s4&2) | ((s3&2)>>1);
            c->Z = ((s2&1)<<3) | ((s1&1)<<2) | (s6&2) | ((s5&2)>>1);
            c->p = (s7&2)>>1;
            c->q = (s7&1);

            // require 4 loops per output byte
            // 2 output bits are a function of the 4 bits of D
            // xor 2 by 2
            op = (op << 2)^ ( (((c->D^(c->D>>1))>>1)&2) | ((c->D^(c->D>>1))&1) );
        }
        // return input data during init
        cb[i] = b_init ? sb[i] : op;
    }
}


// block - sbox
static const uint8_t block_sbox[256] =
{
    0x3A,0xEA,0x68,0xFE,0x33,0xE9,0x88,0x1A,0x83,0xCF,0xE1,0x7F,0xBA,0xE2,0x38,0x12,
    0xE8,0x27,0x61,0x95,0x0C,0x36,0xE5,0x70,0xA2,0x06,0x82,0x7C,0x17,0xA3,0x26,0x49,
    0xBE,0x7A,0x6D,0x47,0xC1,0x51,0x8F,0xF3,0xCC,0x5B,0x67,0xBD,0xCD,0x18,0x08,0xC9,
    0xFF,0x69,0xEF,0x03,0x4E,0x48,0x4A,0x84,0x3F,0xB4,0x10,0x04,0xDC,0xF5,0x5C,0xC6,
    0x16,0xAB,0xAC,0x4C,0xF1,0x6A,0x2F,0x3C,0x3B,0xD4,0xD5,0x94,0xD0,0xC4,0x63,0x62,
    0x71,0xA1,0xF9,0x4F,0x2E,0xAA,0xC5,0x56,0xE3,0x39,0x93,0xCE,0x65,0x64,0xE4,0x58,
    0x6C,0x19,0x42,0x79,0xDD,0xEE,0x96,0xF6,0x8A,0xEC,0x1E,0x85,0x53,0x45,0xDE,0xBB,
    0x7E,0x0A,0x9A,0x13,0x2A,0x9D,0xC2,0x5E,0x5A,0x1F,0x32,0x35,0x9C,0xA8,0x73,0x30,

    0x29,0x3D,0xE7,0x92,0x87,0x1B,0x2B,0x4B,0xA5,0x57,0x97,0x40,0x15,0xE6,0xBC,0x0E,
    0xEB,0xC3,0x34,0x2D,0xB8,0x44,0x25,0xA4,0x1C,0xC7,0x23,0xED,0x90,0x6E,0x50,0x00,
    0x99,0x9E,0x4D,0xD9,0xDA,0x8D,0x6F,0x5F,0x3E,0xD7,0x21,0x74,0x86,0xDF,0x6B,0x05,
    0x8E,0x5D,0x37,0x11,0xD2,0x28,0x75,0xD6,0xA7,0x77,0x24,0xBF,0xF0,0xB0,0x02,0xB7,
    0xF8,0xFC,0x81,0x09,0xB1,0x01,0x76,0x91,0x7D,0x0F,0xC8,0xA0,0xF2,0xCB,0x78,0x60,
    0xD1,0xF7,0xE0,0xB5,0x98,0x22,0xB3,0x20,0x1D,0xA6,0xDB,0x7B,0x59,0x9F,0xAE,0x31,
    0xFB,0xD3,0xB6,0xCA,0x43,0x72,0x07,0xF4,0xD8,0x41,0x14,0x55,0x0D,0x54,0x8B,0xB9,
    0xAD,0x46,0x0B,0xAF,0x80,0x52,0x2C,0xFA,0x8C,0x89,0x66,0xFD,0xB2,0xA9,0x9B,0xC0,
};

// block - perm
static const uint8_t block_perm[256] =
{
    0x00,0x02,0x80,0x82,0x20,0x22,0xA0,0xA2, 0x10,0x12,0x90,0x92,0x30,0x32,0xB0,0xB2,
    0x04,0x06,0x84,0x86,0x24,0x26,0xA4,0xA6, 0x14,0x16,0x94,0x96,0x34,0x36,0xB4,0xB6,
    0x40,0x42,0xC0,0xC2,0x60,0x62,0xE0,0xE2, 0x50,0x52,0xD0,0xD2,0x70,0x72,0xF0,0xF2,
    0x44,0x46,0xC4,0xC6,0x64,0x66,0xE4,0xE6, 0x54,0x56,0xD4,0xD6,0x74,0x76,0xF4,0xF6,
    0x01,0x03,0x81,0x83,0x21,0x23,0xA1,0xA3, 0x11,0x13,0x91,0x93,0x31,0x33,0xB1,0xB3,
    0x05,0x07,0x85,0x87,0x25,0x27,0xA5,0xA7, 0x15,0x17,0x95,0x97,0x35,0x37,0xB5,0xB7,
    0x41,0x43,0xC1,0xC3,0x61,0x63,0xE1,0xE3, 0x51,0x53,0xD1,0xD3,0x71,0x73,0xF1,0xF3,
    0x45,0x47,0xC5,0xC7,0x65,0x67,0xE5,0xE7, 0x55,0x57,0xD5,0xD7,0x75,0x77,0xF5,0xF7,

    0x08,0x0A,0x88,0x8A,0x28,0x2A,0xA8,0xAA, 0x18,0x1A,0x98,0x9A,0x38,0x3A,0xB8,0xBA,
    0x0C,0x0E,0x8C,0x8E,0x2C,0x2E,0xAC,0xAE, 0x1C,0x1E,0x9C,0x9E,0x3C,0x3E,0xBC,0xBE,
    0x48,0x4A,0xC8,0xCA,0x68,0x6A,0xE8,0xEA, 0x58,0x5A,0xD8,0xDA,0x78,0x7A,0xF8,0xFA,
    0x4C,0x4E,0xCC,0xCE,0x6C,0x6E,0xEC,0xEE, 0x5C,0x5E,0xDC,0xDE,0x7C,0x7E,0xFC,0xFE,
    0x09,0x0B,0x89,0x8B,0x29,0x2B,0xA9,0xAB, 0x19,0x1B,0x99,0x9B,0x39,0x3B,0xB9,0xBB,
    0x0D,0x0F,0x8D,0x8F,0x2D,0x2F,0xAD,0xAF, 0x1D,0x1F,0x9D,0x9F,0x3D,0x3F,0xBD,0xBF,
    0x49,0x4B,0xC9,0xCB,0x69,0x6B,0xE9,0xEB, 0x59,0x5B,0xD9,0xDB,0x79,0x7B,0xF9,0xFB,
    0x4D,0x4F,0xCD,0xCF,0x6D,0x6F,0xED,0xEF, 0x5D,0x5F,0xDD,0xDF,0x7D,0x7F,0xFD,0xFF,
};

static void csa_BlockDecypher( uint8_t kk[57], uint8_t ib[8], uint8_t bd[8] )
{
    int i;
    int perm_out;
    int R[9];
    int next_R8;

    for( i = 0; i < 8; i++ )
    {
        R[i+1] = ib[i];
    }

    // loop over kk[56]..kk[1]
    for( i = 56; i > 0; i-- )
    {
        const int sbox_out = block_sbox[ kk[i]^R[7] ];
        perm_out = block_perm[sbox_out];

        next_R8 = R[7];
        R[7] = R[6] ^ perm_out;
        R[6] = R[5];
        R[5] = R[4] ^ R[8] ^ sbox_out;
        R[4] = R[3] ^ R[8] ^ sbox_out;
        R[3] = R[2] ^ R[8] ^ sbox_out;
        R[2] = R[1];
        R[1] = R[8] ^ sbox_out;

        R[8] = next_R8;
    }

    for( i = 0; i < 8; i++ )
    {
        bd[i] = R[i+1];
    }
}

static void csa_BlockCypher( uint8_t kk[57], uint8_t bd[8], uint8_t ib[8] )
{
    int i;
    int perm_out;
    int R[9];
    int next_R1;

    for( i = 0; i < 8; i++ )
    {
        R[i+1] = bd[i];
    }

    // loop over kk[1]..kk[56]
    for( i = 1; i <= 56; i++ )
    {
        const int sbox_out = block_sbox[ kk[i]^R[8] ];
        perm_out = block_perm[sbox_out];

        next_R1 = R[2];
        R[2] = R[3] ^ R[1];
        R[3] = R[4] ^ R[1];
        R[4] = R[5] ^ R[1];
        R[5] = R[6];
        R[6] = R[7] ^ perm_out;
        R[7] = R[8];
        R[8] = R[1] ^ sbox_out;

        R[1] = next_R1;
    }

    for( i = 0; i < 8; i++ )
    {
        ib[i] = R[i+1];
    }
}


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

#include <assert.h>
#include <vlc_common.h>

#include "csa.h"

#ifdef HAVE_DVBCSA
# include <dvbcsa/dvbcsa.h>

struct csa_t
{
    bool    use_odd;
    struct dvbcsa_key_s *keys[2];
};

/*****************************************************************************
 * csa_New:
 *****************************************************************************/
csa_t *csa_New( void )
{
    csa_t *csa = calloc( 1, sizeof( csa_t ) );
    if(csa)
    {
        csa->keys[0] = dvbcsa_key_alloc();
        if(csa->keys[0])
            csa->keys[1] = dvbcsa_key_alloc();
        if(csa->keys[1])
            return csa;
        else
            dvbcsa_key_free(csa->keys[0]);
    }
    free(csa);
    return NULL;
}

/*****************************************************************************
 * csa_Delete:
 *****************************************************************************/
void csa_Delete( csa_t *c )
{
    dvbcsa_key_free( c->keys[0] );
    dvbcsa_key_free( c->keys[1] );
    free( c );
}

/*****************************************************************************
 * csa_SetCW:
 *****************************************************************************/
int csa_SetCW( vlc_object_t *p_caller, csa_t *c, char *psz_ck, bool set_odd )
{
    assert(c != NULL);

    /* skip 0x */
    if( psz_ck[0] == '0' && ( psz_ck[1] == 'x' || psz_ck[1] == 'X' ) )
    {
        psz_ck += 2;
    }
    if( strlen( psz_ck ) != 16 )
    {
        msg_Warn( p_caller, "invalid csa ck (it must be 16 chars long)" );
        return VLC_EINVAL;
    }
    else
    {
        uint64_t i_ck = strtoull( psz_ck, NULL, 16 );
        dvbcsa_cw_t ck;
        int      i;

        for( i = 0; i < 8; i++ )
        {
            ck[i] = ( i_ck >> ( 56 - 8*i) )&0xff;
        }
# ifndef TS_NO_CSA_CK_MSG
        msg_Dbg( p_caller, "using CSA (de)scrambling with %s "
                 "key=%x:%x:%x:%x:%x:%x:%x:%x", set_odd ? "odd" : "even",
                 ck[0], ck[1], ck[2], ck[3], ck[4], ck[5], ck[6], ck[7] );
# endif

        dvbcsa_key_set( ck, c->keys[set_odd ? 1 : 0] );

        return VLC_SUCCESS;
    }
}

/*****************************************************************************
 * csa_UseKey:
 *****************************************************************************/
void csa_UseKey( vlc_object_t *p_caller, csa_t *c, bool use_odd )
{
    assert(c != NULL);
    c->use_odd = use_odd;
# ifndef TS_NO_CSA_CK_MSG
        msg_Dbg( p_caller, "using the %s key for scrambling",
                 use_odd ? "odd" : "even" );
# endif
}

/*****************************************************************************
 * csa_Decrypt:
 *****************************************************************************/
void csa_Decrypt( csa_t *c, uint8_t *pkt, int i_pkt_size )
{
    struct dvbcsa_key_s *key;

    int     i_hdr;

    /* transport scrambling control */
    if( (pkt[3]&0x80) == 0 )
    {
        /* not scrambled */
        return;
    }
    if( pkt[3]&0x40 )
        key = c->keys[1];
    else
        key = c->keys[0];

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

    dvbcsa_decrypt( key, &pkt[i_hdr], i_pkt_size - i_hdr );
}

/*****************************************************************************
 * csa_Encrypt:
 *****************************************************************************/
void csa_Encrypt( csa_t *c, uint8_t *pkt, int i_pkt_size )
{
    int i_hdr = 4; /* hdr len */
    int n;

    /* set transport scrambling control */
    pkt[3] |= 0x80;

    struct dvbcsa_key_s *key;
    if( c->use_odd )
    {
        pkt[3] |= 0x40;
        key = c->keys[1];
    }
    else
    {
        key = c->keys[0];
    }

    /* hdr len */
    i_hdr = 4;
    if( pkt[3]&0x20 )
    {
        /* skip adaption field */
        i_hdr += pkt[4] + 1;
    }
    n = (i_pkt_size - i_hdr) / 8;

    if( n <= 0 )
    {
        pkt[3] &= 0x3f;
        return;
    }

    dvbcsa_encrypt(key, &pkt[i_hdr], i_pkt_size - i_hdr);
}
#else

csa_t *csa_New( void )
{
    return NULL;
}

void csa_Delete( csa_t *c )
{
    VLC_UNUSED(c);
}

int csa_SetCW( vlc_object_t *p_caller, csa_t *c, char *psz_ck, bool set_odd )
{
    VLC_UNUSED(p_caller);
    VLC_UNUSED(c);
    VLC_UNUSED(psz_ck);
    VLC_UNUSED(set_odd);
    return VLC_EGENERIC;
}

void csa_UseKey( vlc_object_t *p_caller, csa_t *c, bool use_odd )
{
    VLC_UNUSED(p_caller);
    VLC_UNUSED(c);
    VLC_UNUSED(use_odd);
}

void csa_Decrypt( csa_t *c, uint8_t *pkt, int i_pkt_size )
{
    VLC_UNUSED(c);
    VLC_UNUSED(pkt);
    VLC_UNUSED(i_pkt_size);
}

void csa_Encrypt( csa_t *c, uint8_t *pkt, int i_pkt_size )
{
    VLC_UNUSED(c);
    VLC_UNUSED(pkt);
    VLC_UNUSED(i_pkt_size);
}

#endif

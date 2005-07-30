/*****************************************************************************
 * acl.c:
 *****************************************************************************
 * Copyright (C) 2005 Rémi Denis-Courmont
 * $Id$
 *
 * Authors: Rémi Denis-Courmont <rem # videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <vlc/vlc.h>

#include "vlc_acl.h"

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include "network.h"

/* FIXME: rwlock on acl, but libvlc doesn't implement rwlock */
typedef struct vlc_acl_entry_t
{
    uint8_t    host[17];
    uint8_t    i_bytes_match;
    uint8_t    i_bits_mask;
    vlc_bool_t b_allow;
} vlc_acl_entry_t;

struct vlc_acl_t
{
    vlc_object_t    *p_owner;
    unsigned         i_size;
    vlc_acl_entry_t *p_entries;
    vlc_bool_t       b_allow_default;
};

static int ACL_Resolve( vlc_object_t *p_this, uint8_t *p_bytes,
                        const char *psz_ip )
{
    struct addrinfo hints = { 0 }, *res;
    int i_family;

    hints.ai_socktype = SOCK_STREAM; /* doesn't matter */
    hints.ai_flags = AI_NUMERICHOST;

    if( vlc_getaddrinfo( p_this, psz_ip, 0, &hints, &res ) )
    {
        msg_Err( p_this, "invalid IP address %s", psz_ip );
        return -1;
    }
    
    p_bytes[16] = 0; /* avoids overflowing when i_bytes_match = 16 */

    i_family = res->ai_addr->sa_family;
    switch( i_family )
    {
        case AF_INET:
        {
            struct sockaddr_in *addr;
                
            addr = (struct sockaddr_in *)res->ai_addr;
            memset( p_bytes, 0, 12 );
            memcpy( p_bytes + 12, &addr->sin_addr, 4 );
            break;
        }

#if defined (HAVE_GETADDRINFO) || defined (WIN32)
        /* unfortunately many people define AF_INET6
           though they don't have struct sockaddr_in6 */
        case AF_INET6:
        {
            struct sockaddr_in6 *addr;

            addr = (struct sockaddr_in6 *)res->ai_addr;
            memcpy( p_bytes, &addr->sin6_addr, 16 );
            break;
        }
#endif

        default:
            msg_Err( p_this, "IMPOSSIBLE: unknown address family!" );
            vlc_freeaddrinfo( res );
            return -1;
    }

    vlc_freeaddrinfo( res );
    return i_family;
}


/*
 * Returns 0 if allowed, 1 if not, -1 on error.
 */
int ACL_Check( vlc_acl_t *p_acl, const char *psz_ip )
{
    const vlc_acl_entry_t *p_cur, *p_end;
    uint8_t host[17];

    if( p_acl == NULL )
        return -1;

    p_cur = p_acl->p_entries;
    p_end = p_cur + p_acl->i_size;

    if( ACL_Resolve( p_acl->p_owner, host, psz_ip ) < 0 )
        return -1;

    while (p_cur < p_end)
    {
        unsigned i;

        i = p_cur->i_bytes_match;
        if( (memcmp( p_cur->host, host, i ) == 0)
         && (((p_cur->host[i] ^ host[i]) & p_cur->i_bits_mask) == 0) )
            return !p_cur->b_allow;

        p_cur++;
    }

    return !p_acl->b_allow_default;
}

int ACL_AddNet( vlc_acl_t *p_acl, const char *psz_ip, int i_len,
                vlc_bool_t b_allow )
{
    vlc_acl_entry_t *p_ent;
    unsigned i_size;
    div_t d;
    int i_family;

    i_size = p_acl->i_size;
    p_ent = (vlc_acl_entry_t *)realloc( p_acl->p_entries,
                                        ++p_acl->i_size * sizeof( *p_ent ) );

    if( p_ent == NULL )
        return -1;

    p_acl->p_entries = p_ent;
    p_ent += i_size;

    i_family = ACL_Resolve( p_acl->p_owner, p_ent->host, psz_ip );
    if( i_family < 0 )
    {
        /*
         * I'm lazy : memory space will be re-used in the next ACL_Add call...
         * or not.
         */
        p_acl->i_size--;
        return -1;
    }

    if( i_len >= 0 )
    {
        if( i_family == AF_INET )
            i_len += 96;

        if( i_len > 128 )
            i_len = 128;
        else
        if( i_len < 0 )
            i_len = 0;
    }
    else
        i_len = 128; /* ACL_AddHost */

    d = div( i_len, 8 );
    p_ent->i_bytes_match = d.quot;
    p_ent->i_bits_mask = 0xff << (8 - d.rem);

    p_ent->b_allow = b_allow;
    return 0;
}


vlc_acl_t *__ACL_Create( vlc_object_t *p_this, vlc_bool_t b_allow )
{
    vlc_acl_t *p_acl;

    p_acl = (vlc_acl_t *)malloc( sizeof( *p_acl ) );
    if( p_acl == NULL )
        return NULL;

    vlc_object_yield( p_this );
    p_acl->p_owner = p_this;
    p_acl->i_size = 0;
    p_acl->p_entries = NULL;
    p_acl->b_allow_default = b_allow;
    
    return p_acl;
}


vlc_acl_t *__ACL_Duplicate( vlc_object_t *p_this, const vlc_acl_t *p_acl )
{
    vlc_acl_t *p_dupacl;

    if( p_acl == NULL )
        return NULL;

    p_dupacl = (vlc_acl_t *)malloc( sizeof( *p_dupacl ) );
    if( p_dupacl == NULL )
        return NULL;

    if( p_acl->i_size )
    {
        p_dupacl->p_entries = (vlc_acl_entry_t *)
            malloc( p_acl->i_size * sizeof( vlc_acl_entry_t ) );

        if( p_dupacl->p_entries == NULL )
        {
            free( p_dupacl );
            return NULL;
        }

        memcpy( p_dupacl->p_entries, p_acl->p_entries,
                p_acl->i_size * sizeof( vlc_acl_entry_t ) );
    }
    else
        p_dupacl->p_entries = NULL;

    vlc_object_yield( p_this );
    p_dupacl->p_owner = p_this;
    p_dupacl->i_size = p_acl->i_size;
    p_dupacl->b_allow_default = p_acl->b_allow_default;

    return p_dupacl;
}


void ACL_Destroy( vlc_acl_t *p_acl )
{
    if( p_acl != NULL )
    {
        if( p_acl->p_entries != NULL )
            free( p_acl->p_entries );

        vlc_object_release( p_acl->p_owner );
        free( p_acl );
    }
}

#ifndef isblank 
#   define isblank(c) ((c) == ' ' || (c) == '\t')
#endif

int ACL_LoadFile( vlc_acl_t *p_acl, const char *psz_path )
{
    FILE *file;
    
    if( p_acl == NULL )
        return -1;

    file = fopen( psz_path, "r" );
    if( file == NULL )
        return -1;

    msg_Dbg( p_acl->p_owner, "find .hosts in dir=%s", psz_path );

    while( !feof( file ) )
    {
        char line[1024], *psz_ip, *ptr;

        if( fgets( line, sizeof( line ), file ) == NULL )
        {
            if( ferror( file ) )
            {
                msg_Err( p_acl->p_owner, "Error reading %s : %s\n", psz_path,
                        strerror( errno ) );
                goto error;
            }
            continue;
        }

        /* fgets() is cool : never overflow, always nul-terminate */
        psz_ip = line;

        /* skips blanks - cannot overflow given '\0' is not space */
        while( isspace( *psz_ip ) )
            psz_ip++;

        ptr = strchr( psz_ip, '\n' );
        if( ptr == NULL )
        {
            msg_Warn( p_acl->p_owner, "Skipping overly long line in %s\n",
                      psz_path);
            do
            {
                fgets( line, sizeof( line ), file );
                if( ferror( file ) || feof( file ) )
                {
                    msg_Err( p_acl->p_owner, "Error reading %s : %s\n",
                             psz_path, strerror( errno ) );
                    goto error;
                }
            }
            while( strchr( line, '\n' ) == NULL);

            continue; /* skip unusable line */
        }

        /* skips comment-only line */
        if( *psz_ip == '#' )
            continue;

        /* looks for first space, CR, LF, etc. or end-of-line comment */
        /* (there is at least a linefeed) */
        for( ptr = psz_ip; ( *ptr != '#' ) && !isspace( *ptr ); ptr++ );

        *ptr = '\0';

        msg_Dbg( p_acl->p_owner, "restricted to %s", psz_ip );

        ptr = strchr( psz_ip, '/' );
        if( ptr != NULL )
            *ptr++ = '\0'; /* separate address from mask length */

        if( (ptr != NULL)
            ? ACL_AddNet( p_acl, psz_ip, atoi( ptr ), VLC_TRUE ) 
            : ACL_AddHost( p_acl, psz_ip, VLC_TRUE ) )
        {
            msg_Err( p_acl->p_owner, "cannot add ACL from %s", psz_path );
            goto error;
        }
    }

    fclose( file );
    return 0;

error:
    fclose( file );
    return -1;
}


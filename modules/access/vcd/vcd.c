/*****************************************************************************
 * vcd.c : VCD input module for vlc
 *****************************************************************************
 * Copyright (C) 2000-2004 the VideoLAN team
 * $Id$
 *
 * Author: Johan Bilien <jobi@via.ecp.fr>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_input.h>
#include <vlc_access.h>
#include <vlc_charset.h>

#include "cdrom.h"

/*****************************************************************************
 * Module descriptior
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

#define CACHING_TEXT N_("Caching value in ms")
#define CACHING_LONGTEXT N_( \
    "Caching value for VCDs. This " \
    "value should be set in milliseconds." )

vlc_module_begin();
    set_shortname( N_("VCD"));
    set_description( N_("VCD input") );
    set_capability( "access", 60 );
    set_callbacks( Open, Close );
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_ACCESS );

    add_usage_hint( N_("[vcd:][device][@[title][,[chapter]]]") );
    add_integer( "vcd-caching", DEFAULT_PTS_DELAY / 1000, NULL, CACHING_TEXT,
                 CACHING_LONGTEXT, true );
    add_shortcut( "vcd" );
    add_shortcut( "svcd" );
vlc_module_end();

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

/* how many blocks VCDRead will read in each loop */
#define VCD_BLOCKS_ONCE 20
#define VCD_DATA_ONCE   (VCD_BLOCKS_ONCE * VCD_DATA_SIZE)

struct access_sys_t
{
    vcddev_t    *vcddev;                            /* vcd device descriptor */

    /* Title infos */
    int           i_titles;
    input_title_t *title[99];            /* No more that 99 track in a vcd ? */


    int         i_sector;                                  /* Current Sector */
    int         *p_sectors;                                 /* Track sectors */
};

static block_t *Block( access_t * );
static int      Seek( access_t *, int64_t );
static int      Control( access_t *, int, va_list );
static int      EntryPoints( access_t * );

/*****************************************************************************
 * VCDOpen: open vcd
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    access_t     *p_access = (access_t *)p_this;
    access_sys_t *p_sys;
    char *psz_dup = ToLocaleDup( p_access->psz_path );
    char *psz;
    int i_title = 0;
    int i_chapter = 0;
    int i;
    vcddev_t *vcddev;

    /* Command line: vcd://[dev_path][@title[,chapter]] */
    if( ( psz = strchr( psz_dup, '@' ) ) )
    {
        *psz++ = '\0';

        i_title = strtol( psz, &psz, 0 );
        if( *psz )
            i_chapter = strtol( psz+1, &psz, 0 );
    }

    if( *psz_dup == '\0' )
    {
        free( psz_dup );

        /* Only when selected */
        if( strcmp( p_access->psz_access, "vcd" ) &&
            strcmp( p_access->psz_access, "svcd" ) )
            return VLC_EGENERIC;

        psz_dup = var_CreateGetString( p_access, "vcd" );
        if( *psz_dup == '\0' )
        {
            free( psz_dup );
            return VLC_EGENERIC;
        }
    }

#ifdef WIN32
    if( psz_dup[0] && psz_dup[1] == ':' &&
        psz_dup[2] == '\\' && psz_dup[3] == '\0' ) psz_dup[2] = '\0';
#endif

    /* Open VCD */
    if( !(vcddev = ioctl_Open( p_this, psz_dup )) )
    {
        free( psz_dup );
        return VLC_EGENERIC;
    }
    free( psz_dup );

    /* Set up p_access */
    p_access->pf_read = NULL;
    p_access->pf_block = Block;
    p_access->pf_control = Control;
    p_access->pf_seek = Seek;
    p_access->info.i_update = 0;
    p_access->info.i_size = 0;
    p_access->info.i_pos = 0;
    p_access->info.b_eof = false;
    p_access->info.i_title = 0;
    p_access->info.i_seekpoint = 0;
    p_access->p_sys = p_sys = malloc( sizeof( access_sys_t ) );
    memset( p_sys, 0, sizeof( access_sys_t ) );
    p_sys->vcddev = vcddev;

    /* We read the Table Of Content information */
    p_sys->i_titles = ioctl_GetTracksMap( VLC_OBJECT(p_access),
                                          p_sys->vcddev, &p_sys->p_sectors );
    if( p_sys->i_titles < 0 )
    {
        msg_Err( p_access, "unable to count tracks" );
        goto error;
    }
    else if( p_sys->i_titles <= 1 )
    {
        msg_Err( p_access, "no movie tracks found" );
        goto error;
    }
    /* The first title isn't usable */
    p_sys->i_titles--;

    /* Build title table */
    for( i = 0; i < p_sys->i_titles; i++ )
    {
        input_title_t *t = p_sys->title[i] = vlc_input_title_New();

        msg_Dbg( p_access, "title[%d] start=%d\n", i, p_sys->p_sectors[1+i] );
        msg_Dbg( p_access, "title[%d] end=%d\n", i, p_sys->p_sectors[i+2] );

        t->i_size = ( p_sys->p_sectors[i+2] - p_sys->p_sectors[i+1] ) *
                    (int64_t)VCD_DATA_SIZE;
    }

    /* Map entry points into chapters */
    if( EntryPoints( p_access ) )
    {
        msg_Warn( p_access, "could not read entry points, will not use them" );
    }

    /* Starting title/chapter and sector */
    if( i_title >= p_sys->i_titles )
        i_title = 0;
    if( i_chapter >= p_sys->title[i_title]->i_seekpoint )
        i_chapter = 0;

    p_sys->i_sector = p_sys->p_sectors[1+i_title];
    if( i_chapter > 0 )
    {
        p_sys->i_sector +=
            ( p_sys->title[i_title]->seekpoint[i_chapter]->i_byte_offset /
              VCD_DATA_SIZE );
    }
    p_access->info.i_title = i_title;
    p_access->info.i_seekpoint = i_chapter;
    p_access->info.i_size = p_sys->title[i_title]->i_size;
    p_access->info.i_pos = ( p_sys->i_sector - p_sys->p_sectors[1+i_title] ) *
        VCD_DATA_SIZE;

    free( p_access->psz_demux );
    p_access->psz_demux = strdup( "ps" );

    return VLC_SUCCESS;

error:
    ioctl_Close( VLC_OBJECT(p_access), p_sys->vcddev );
    free( p_sys );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Close: closes vcd
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    access_t     *p_access = (access_t *)p_this;
    access_sys_t *p_sys = p_access->p_sys;

    ioctl_Close( p_this, p_sys->vcddev );
    free( p_sys );
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( access_t *p_access, int i_query, va_list args )
{
    access_sys_t *p_sys = p_access->p_sys;
    bool   *pb_bool;
    int          *pi_int;
    int64_t      *pi_64;
    input_title_t ***ppp_title;
    int i;

    switch( i_query )
    {
        /* */
        case ACCESS_CAN_SEEK:
        case ACCESS_CAN_FASTSEEK:
        case ACCESS_CAN_PAUSE:
        case ACCESS_CAN_CONTROL_PACE:
            pb_bool = (bool*)va_arg( args, bool* );
            *pb_bool = true;
            break;

        /* */
        case ACCESS_GET_MTU:
            pi_int = (int*)va_arg( args, int * );
            *pi_int = VCD_DATA_ONCE;
            break;

        case ACCESS_GET_PTS_DELAY:
            pi_64 = (int64_t*)va_arg( args, int64_t * );
            *pi_64 = var_GetInteger( p_access, "vcd-caching" ) * 1000;
            break;

        /* */
        case ACCESS_SET_PAUSE_STATE:
            break;

        case ACCESS_GET_TITLE_INFO:
            ppp_title = (input_title_t***)va_arg( args, input_title_t*** );
            pi_int    = (int*)va_arg( args, int* );

            /* Duplicate title infos */
            *pi_int = p_sys->i_titles;
            *ppp_title = malloc( sizeof(input_title_t **) * p_sys->i_titles );
            for( i = 0; i < p_sys->i_titles; i++ )
            {
                (*ppp_title)[i] = vlc_input_title_Duplicate( p_sys->title[i] );
            }
            break;

        case ACCESS_SET_TITLE:
            i = (int)va_arg( args, int );
            if( i != p_access->info.i_title )
            {
                /* Update info */
                p_access->info.i_update |=
                  INPUT_UPDATE_TITLE|INPUT_UPDATE_SEEKPOINT|INPUT_UPDATE_SIZE;
                p_access->info.i_title = i;
                p_access->info.i_seekpoint = 0;
                p_access->info.i_size = p_sys->title[i]->i_size;
                p_access->info.i_pos  = 0;

                /* Next sector to read */
                p_sys->i_sector = p_sys->p_sectors[1+i];
            }
            break;

        case ACCESS_SET_SEEKPOINT:
        {
            input_title_t *t = p_sys->title[p_access->info.i_title];
            i = (int)va_arg( args, int );
            if( t->i_seekpoint > 0 )
            {
                p_access->info.i_update |= INPUT_UPDATE_SEEKPOINT;
                p_access->info.i_seekpoint = i;

                p_sys->i_sector = p_sys->p_sectors[1+p_access->info.i_title] +
                    t->seekpoint[i]->i_byte_offset / VCD_DATA_SIZE;

                p_access->info.i_pos = (int64_t)(p_sys->i_sector -
                    p_sys->p_sectors[1+p_access->info.i_title]) *VCD_DATA_SIZE;
            }
            return VLC_SUCCESS;
        }

        case ACCESS_SET_PRIVATE_ID_STATE:
        case ACCESS_GET_CONTENT_TYPE:
            return VLC_EGENERIC;

        default:
            msg_Warn( p_access, "unimplemented query in control" );
            return VLC_EGENERIC;

    }
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Block:
 *****************************************************************************/
static block_t *Block( access_t *p_access )
{
    access_sys_t *p_sys = p_access->p_sys;
    int i_blocks = VCD_BLOCKS_ONCE;
    block_t *p_block;
    int i_read;

    /* Check end of file */
    if( p_access->info.b_eof ) return NULL;

    /* Check end of title */
    while( p_sys->i_sector >= p_sys->p_sectors[p_access->info.i_title + 2] )
    {
        if( p_access->info.i_title + 2 >= p_sys->i_titles )
        {
            p_access->info.b_eof = true;
            return NULL;
        }

        p_access->info.i_update |=
            INPUT_UPDATE_TITLE | INPUT_UPDATE_SEEKPOINT | INPUT_UPDATE_SIZE;
        p_access->info.i_title++;
        p_access->info.i_seekpoint = 0;
        p_access->info.i_size =
            p_sys->title[p_access->info.i_title]->i_size;
        p_access->info.i_pos = 0;
    }

    /* Don't read after the end of a title */
    if( p_sys->i_sector + i_blocks >=
        p_sys->p_sectors[p_access->info.i_title + 2] )
    {
        i_blocks = p_sys->p_sectors[p_access->info.i_title + 2 ] -
                   p_sys->i_sector;
    }

    /* Do the actual reading */
    if( !( p_block = block_New( p_access, i_blocks * VCD_DATA_SIZE ) ) )
    {
        msg_Err( p_access, "cannot get a new block of size: %i",
                 i_blocks * VCD_DATA_SIZE );
        return NULL;
    }

    if( ioctl_ReadSectors( VLC_OBJECT(p_access), p_sys->vcddev,
            p_sys->i_sector, p_block->p_buffer, i_blocks, VCD_TYPE ) < 0 )
    {
        msg_Err( p_access, "cannot read sector %i", p_sys->i_sector );
        block_Release( p_block );

        /* Try to skip one sector (in case of bad sectors) */
        p_sys->i_sector++;
        p_access->info.i_pos += VCD_DATA_SIZE;
        return NULL;
    }

    /* Update seekpoints */
    for( i_read = 0; i_read < i_blocks; i_read++ )
    {
        input_title_t *t = p_sys->title[p_access->info.i_title];

        if( t->i_seekpoint > 0 &&
            p_access->info.i_seekpoint + 1 < t->i_seekpoint &&
            p_access->info.i_pos + i_read * VCD_DATA_SIZE >=
            t->seekpoint[p_access->info.i_seekpoint+1]->i_byte_offset )
        {
            msg_Dbg( p_access, "seekpoint change" );
            p_access->info.i_update |= INPUT_UPDATE_SEEKPOINT;
            p_access->info.i_seekpoint++;
        }
    }

    /* Update a few values */
    p_sys->i_sector += i_blocks;
    p_access->info.i_pos += p_block->i_buffer;

    return p_block;
}

/*****************************************************************************
 * Seek:
 *****************************************************************************/
static int Seek( access_t *p_access, int64_t i_pos )
{
    access_sys_t *p_sys = p_access->p_sys;
    input_title_t *t = p_sys->title[p_access->info.i_title];
    int i_seekpoint;

    /* Next sector to read */
    p_access->info.i_pos = i_pos;
    p_sys->i_sector = i_pos / VCD_DATA_SIZE +
        p_sys->p_sectors[p_access->info.i_title + 1];

    /* Update current seekpoint */
    for( i_seekpoint = 0; i_seekpoint < t->i_seekpoint; i_seekpoint++ )
    {
        if( i_seekpoint + 1 >= t->i_seekpoint ) break;
        if( i_pos < t->seekpoint[i_seekpoint + 1]->i_byte_offset ) break;
    }

    if( i_seekpoint != p_access->info.i_seekpoint )
    {
        msg_Dbg( p_access, "seekpoint change" );
        p_access->info.i_update |= INPUT_UPDATE_SEEKPOINT;
        p_access->info.i_seekpoint = i_seekpoint;
    }

    /* Reset eof */
    p_access->info.b_eof = false;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * EntryPoints: Reads the information about the entry points on the disc.
 *****************************************************************************/
static int EntryPoints( access_t *p_access )
{
    access_sys_t *p_sys = p_access->p_sys;
    uint8_t      sector[VCD_DATA_SIZE];

    entries_sect_t entries;
    int i_nb, i;

    /* Read the entry point sector */
    if( ioctl_ReadSectors( VLC_OBJECT(p_access), p_sys->vcddev,
        VCD_ENTRIES_SECTOR, sector, 1, VCD_TYPE ) < 0 )
    {
        msg_Err( p_access, "could not read entry points sector" );
        return VLC_EGENERIC;
    }
    memcpy( &entries, sector, CD_SECTOR_SIZE );

    i_nb = GetWBE( &entries.i_entries_nb );
    if( i_nb > 500 )
    {
        msg_Err( p_access, "invalid entry points number" );
        return VLC_EGENERIC;
    }

    if( strncmp( entries.psz_id, "ENTRYVCD", sizeof( entries.psz_id ) ) &&
        strncmp( entries.psz_id, "ENTRYSVD", sizeof( entries.psz_id ) ) )
    {
        msg_Err( p_access, "unrecognized entry points format" );
        return VLC_EGENERIC;
    }

    for( i = 0; i < i_nb; i++ )
    {
        const int i_title = BCD_TO_BIN(entries.entry[i].i_track) - 2;
        const int i_sector =
            (MSF_TO_LBA2( BCD_TO_BIN( entries.entry[i].msf.minute ),
                          BCD_TO_BIN( entries.entry[i].msf.second ),
                          BCD_TO_BIN( entries.entry[i].msf.frame  ) ));
        seekpoint_t *s;

        if( i_title < 0 ) continue;   /* Should not occur */
        if( i_title >= p_sys->i_titles ) continue;

        msg_Dbg( p_access, "Entry[%d] title=%d sector=%d\n",
                 i, i_title, i_sector );

        s = vlc_seekpoint_New();
        s->i_byte_offset = (i_sector - p_sys->p_sectors[i_title+1]) *
            VCD_DATA_SIZE;

        TAB_APPEND( p_sys->title[i_title]->i_seekpoint,
                    p_sys->title[i_title]->seekpoint, s );
    }

    return VLC_SUCCESS;
}


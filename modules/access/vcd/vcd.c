/*****************************************************************************
 * vcd.c : VCD input module for vlc
 *****************************************************************************
 * Copyright (C) 2000-2004 VideoLAN
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>

#include <vlc/vlc.h>
#include <vlc/input.h>

#include "cdrom.h"

/*****************************************************************************
 * Module descriptior
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

#define CACHING_TEXT N_("Caching value in ms")
#define CACHING_LONGTEXT N_( \
    "Allows you to modify the default caching value for cdda streams. This " \
    "value should be set in milliseconds units." )

vlc_module_begin();
    set_description( _("VCD input") );
    set_capability( "access2", 10 );
    set_callbacks( Open, Close );

    add_usage_hint( N_("[vcd:][device][@[title][,[chapter]]]") );
    add_integer( "vcd-caching", DEFAULT_PTS_DELAY / 1000, NULL, CACHING_TEXT,
                 CACHING_LONGTEXT, VLC_TRUE );
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
    char *psz_dup = strdup( p_access->psz_path );
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

    /* Open VCD */
    if( !(vcddev = ioctl_Open( p_this, psz_dup )) )
    {
        msg_Warn( p_access, "could not open %s", psz_dup );
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
    p_access->info.b_eof = VLC_FALSE;
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

        fprintf( stderr, "title[%d] start=%d\n", i, p_sys->p_sectors[1+i] );
        fprintf( stderr, "title[%d] end=%d\n", i, p_sys->p_sectors[i+2] );

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
        int64_t i_off = p_sys->title[i_title]->seekpoint[i_chapter]->i_byte_offset;
        p_sys->i_sector += i_off / VCD_DATA_SIZE;
    }
    p_access->info.i_title = i_title;
    p_access->info.i_seekpoint = i_chapter;
    p_access->info.i_size = p_sys->title[i_title]->i_size;
    p_access->info.i_pos = ( p_sys->i_sector - p_sys->p_sectors[1+i_title] ) * VCD_DATA_SIZE;

    p_access->psz_demux = strdup( "ps2" );

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
    vlc_bool_t   *pb_bool;
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
            pb_bool = (vlc_bool_t*)va_arg( args, vlc_bool_t* );
            *pb_bool = VLC_TRUE;
            break;

        /* */
        case ACCESS_GET_MTU:
            pi_int = (int*)va_arg( args, int * );
            *pi_int = VCD_DATA_ONCE;
            break;

        case ACCESS_GET_PTS_DELAY:
            pi_64 = (int64_t*)va_arg( args, int64_t * );
            *pi_64 = (int64_t)var_GetInteger( p_access, "vcd-caching" ) * I64C(1000);
            break;

        /* */
        case ACCESS_SET_PAUSE_STATE:
            break;

        case ACCESS_GET_TITLE_INFO:
            ppp_title = (input_title_t***)va_arg( args, input_title_t*** );
            pi_int    = (int*)va_arg( args, int* );

            /* Duplicate title infos */
            *pi_int = p_sys->i_titles;
            *ppp_title = malloc( sizeof( input_title_t ** ) * p_sys->i_titles );
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
                p_access->info.i_update |= INPUT_UPDATE_TITLE|INPUT_UPDATE_SEEKPOINT|INPUT_UPDATE_SIZE;
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

                p_access->info.i_pos = (int64_t)(p_sys->i_sector - p_sys->p_sectors[1+p_access->info.i_title] ) *
                                       (int64_t)VCD_DATA_SIZE;
            }
            return VLC_SUCCESS;
        }

        default:
            msg_Err( p_access, "unimplemented query in control" );
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
    int i_skip = p_access->info.i_pos % VCD_DATA_SIZE;
    block_t      *p_block;
    int          i_blocks;
    int          i_read;
    int          i;

    if( p_access->info.b_eof )
        return NULL;

    /* Read raw data */
    i_blocks = VCD_BLOCKS_ONCE;

    /* Don't read after a title */
    if( p_sys->i_sector + i_blocks >= p_sys->p_sectors[p_access->info.i_title + 2] )
    {
        i_blocks = p_sys->p_sectors[p_access->info.i_title + 2 ] - p_sys->i_sector;
        if( i_blocks <= 0 ) i_blocks = 1;   /* Should never occur */
    }

    p_block = block_New( p_access, i_blocks * VCD_DATA_SIZE );

    if( ioctl_ReadSectors( VLC_OBJECT(p_access), p_sys->vcddev, p_sys->i_sector,
                           p_block->p_buffer, i_blocks, VCD_TYPE ) < 0 )
    {
        msg_Err( p_access, "cannot read a sector" );
        block_Release( p_block );

        p_block = NULL;
        i_blocks = 1;   /* Next sector */
    }


    i_read = 0;
    for( i = 0; i < i_blocks; i++ )
    {
        input_title_t *t = p_sys->title[p_access->info.i_title];

        /* A good sector read */
        i_read++;
        p_sys->i_sector++;

        /* Check end of title */
        if( p_sys->i_sector >= p_sys->p_sectors[p_access->info.i_title + 2] )
        {
            if( p_access->info.i_title + 2 >= p_sys->i_titles )
            {
                p_access->info.b_eof = VLC_TRUE;
                break;
            }
            p_access->info.i_update |= INPUT_UPDATE_TITLE|INPUT_UPDATE_SEEKPOINT|INPUT_UPDATE_SIZE;
            p_access->info.i_title++;
            p_access->info.i_seekpoint = 0;
            p_access->info.i_size = p_sys->title[p_access->info.i_title]->i_size;
            p_access->info.i_pos = 0;
        }
        else if( t->i_seekpoint > 0 &&
                 p_access->info.i_seekpoint + 1 < t->i_seekpoint &&
                 p_access->info.i_pos - i_skip + i_read * VCD_DATA_SIZE >= t->seekpoint[p_access->info.i_seekpoint+1]->i_byte_offset )
        {
            fprintf( stderr, "seekpoint change\n" );
            p_access->info.i_update |= INPUT_UPDATE_SEEKPOINT;
            p_access->info.i_seekpoint++;
        }
        /* TODO */
    }

    if( i_read <= 0 )
    {
        block_Release( p_block );
        return NULL;
    }

    if( p_block )
    {

        /* Really read data */
        p_block->i_buffer = i_read * VCD_DATA_SIZE;
        /* */
        p_block->i_buffer -= i_skip;
        p_block->p_buffer += i_skip;

        p_access->info.i_pos += p_block->i_buffer;
    }

    return p_block;
}

/*****************************************************************************
 * Seek:
 *****************************************************************************/
static int Seek( access_t *p_access, int64_t i_pos )
{
    return VLC_EGENERIC;
}

/*****************************************************************************
 * EntryPoints:
 *****************************************************************************/
static int EntryPoints( access_t *p_access )
{
    access_sys_t *p_sys = p_access->p_sys;
    uint8_t      sector[VCD_DATA_SIZE];

    entries_sect_t entries;
    int i_nb;
    int i;


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

        if( i_title < 0 )
            continue;   /* Should not occur */
        if( i_title >= p_sys->i_titles )
            continue;

        fprintf( stderr, "Entry[%d] title=%d sector=%d\n",
                 i, i_title, i_sector );

        s = vlc_seekpoint_New();
        s->i_byte_offset = (i_sector - p_sys->p_sectors[i_title+1]) * VCD_DATA_SIZE;

        TAB_APPEND( p_sys->title[i_title]->i_seekpoint, p_sys->title[i_title]->seekpoint, s );
    }

#if 0
#define i_track BCD_TO_BIN(entries.entry[i].i_track)
    /* Reset the i_part_nb for each track */
    for( i = 0 ; i < i_nb ; i++ )
    {
        if( i_track <= p_input->stream.i_area_nb )
        {
            p_input->stream.pp_areas[i_track-1]->i_part_nb = 0;
        }
    }

    for( i = 0 ; i < i_nb ; i++ )
    {
        if( i_track <= p_input->stream.i_area_nb )
        {
            p_sys->p_entries[i_entry_index] =
                (MSF_TO_LBA2( BCD_TO_BIN( entries.entry[i].msf.minute ),
                              BCD_TO_BIN( entries.entry[i].msf.second ),
                              BCD_TO_BIN( entries.entry[i].msf.frame  ) ));
            p_input->stream.pp_areas[i_track-1]->i_part_nb ++;
            /* if this entry belongs to a new track */
            if( i_track != i_previous_track )
            {
                /* i_plugin_data is used to store the first entry of the area*/
                p_input->stream.pp_areas[i_track-1]->i_plugin_data =
                                                            i_entry_index;
                i_previous_track = i_track;
            }
            msg_Dbg( p_input, "entry point %i begins at LBA: %i",
                     i_entry_index, p_sys->p_entries[i_entry_index] );

            i_entry_index ++;
            p_sys->i_entries_nb ++;
        }
        else
            msg_Warn( p_input, "wrong track number found in entry points" );
    }
#undef i_track
    return 0;
#endif
    return VLC_EGENERIC;
}

#if 0
/*****************************************************************************
 * VCDRead: reads from the VCD into PES packets.
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, otherwise the number of
 * bytes.
 *****************************************************************************/
static int VCDRead( input_thread_t * p_input, byte_t * p_buffer,
                     size_t i_len )
{
    access_sys_t *p_sys;
    int                     i_blocks;
    int                     i_index;
    int                     i_read;
    byte_t                  p_last_sector[ VCD_DATA_SIZE ];

    p_sys = p_input->p_access_data;

    i_read = 0;

    /* Compute the number of blocks we have to read */

    i_blocks = i_len / VCD_DATA_SIZE;

    for ( i_index = 0 ; i_index < i_blocks ; i_index++ )
    {
        if ( ioctl_ReadSectors( VLC_OBJECT(p_input), p_sys->vcddev,
             p_sys->i_sector, p_buffer + i_index * VCD_DATA_SIZE, 1,
             VCD_TYPE ) < 0 )
        {
            msg_Err( p_input, "could not read sector %d", p_sys->i_sector );
            return -1;
        }

        p_sys->i_sector ++;
        if ( p_sys->i_sector == p_sys->p_sectors[p_sys->i_track + 1] )
        {
            input_area_t *p_area;

            if ( p_sys->i_track >= p_sys->i_nb_tracks - 1 )
                return 0; /* EOF */

            vlc_mutex_lock( &p_input->stream.stream_lock );
            p_area = p_input->stream.pp_areas[
                    p_input->stream.p_selected_area->i_id + 1 ];

            msg_Dbg( p_input, "new title" );

            p_area->i_part = 1;
            VCDSetArea( p_input, p_area );
            vlc_mutex_unlock( &p_input->stream.stream_lock );
        }

        /* Update chapter */
        else if( p_sys->b_valid_ep &&
                /* FIXME kludge so that read does not update chapter
                 * when a manual chapter change was requested and not
                 * yet accomplished */
                !p_input->stream.p_new_area )
        {
            int i_entry;

            vlc_mutex_lock( &p_input->stream.stream_lock );
            i_entry = p_input->stream.p_selected_area->i_plugin_data
                /* 1st entry point of the track (area)*/
                        + p_input->stream.p_selected_area->i_part - 1;
            if( i_entry + 1 < p_sys->i_entries_nb &&
                    p_sys->i_sector >= p_sys->p_entries[i_entry + 1] )
            {
                vlc_value_t val;

                msg_Dbg( p_input, "new chapter" );
                p_input->stream.p_selected_area->i_part ++;

                /* Update the navigation variables without triggering
                 * a callback */
                val.i_int = p_input->stream.p_selected_area->i_part;
                var_Change( p_input, "chapter", VLC_VAR_SETVALUE, &val, NULL );
            }
            vlc_mutex_unlock( &p_input->stream.stream_lock );
        }

        i_read += VCD_DATA_SIZE;
    }

    if ( i_len % VCD_DATA_SIZE ) /* this should not happen */
    {
        if ( ioctl_ReadSectors( VLC_OBJECT(p_input), p_sys->vcddev,
             p_sys->i_sector, p_last_sector, 1, VCD_TYPE ) < 0 )
        {
            msg_Err( p_input, "could not read sector %d", p_sys->i_sector );
            return -1;
        }

        p_input->p_vlc->pf_memcpy( p_buffer + i_blocks * VCD_DATA_SIZE,
                                   p_last_sector, i_len % VCD_DATA_SIZE );
        i_read += i_len % VCD_DATA_SIZE;
    }

    return i_read;
}

/*****************************************************************************
 * VCDSetProgram: Does nothing since a VCD is mono_program
 *****************************************************************************/
static int VCDSetProgram( input_thread_t * p_input,
                          pgrm_descriptor_t * p_program)
{
    return 0;
}

/*****************************************************************************
 * VCDSetArea: initialize input data for title x, chapter y.
 * It should be called for each user navigation request.
 ****************************************************************************/
static int VCDSetArea( input_thread_t * p_input, input_area_t * p_area )
{
    access_sys_t *p_sys = p_input->p_access_data;
    vlc_value_t val;

    /* we can't use the interface slider until initilization is complete */
    p_input->stream.b_seekable = 0;

    if( p_area != p_input->stream.p_selected_area )
    {
        unsigned int i;

        /* Reset the Chapter position of the current title */
        p_input->stream.p_selected_area->i_part = 1;
        p_input->stream.p_selected_area->i_tell = 0;

        /* Change the default area */
        p_input->stream.p_selected_area = p_area;

        /* Change the current track */
        /* The first track is not a valid one  */
        p_sys->i_track = p_area->i_id;
        p_sys->i_sector = p_sys->p_sectors[p_sys->i_track];

        /* Update the navigation variables without triggering a callback */
        val.i_int = p_area->i_id;
        var_Change( p_input, "title", VLC_VAR_SETVALUE, &val, NULL );
        var_Change( p_input, "chapter", VLC_VAR_CLEARCHOICES, NULL, NULL );
        for( i = 1; i <= p_area->i_part_nb; i++ )
        {
            val.i_int = i;
            var_Change( p_input, "chapter", VLC_VAR_ADDCHOICE, &val, NULL );
        }
    }

    if( p_sys->b_valid_ep )
    {
        int i_entry = p_area->i_plugin_data /* 1st entry point of
                                               the track (area)*/
                            + p_area->i_part - 1;
        p_sys->i_sector = p_sys->p_entries[i_entry];
    }
    else
        p_sys->i_sector = p_sys->p_sectors[p_sys->i_track];

    p_input->stream.p_selected_area->i_tell =
        (off_t)p_sys->i_sector * (off_t)VCD_DATA_SIZE
         - p_input->stream.p_selected_area->i_start;

    /* warn interface that something has changed */
    p_input->stream.b_seekable = 1;
    p_input->stream.b_changed = 1;

    /* Update the navigation variables without triggering a callback */
    val.i_int = p_area->i_part;
    var_Change( p_input, "chapter", VLC_VAR_SETVALUE, &val, NULL );

    return 0;
}

/****************************************************************************
 * VCDSeek
 ****************************************************************************/
static void VCDSeek( input_thread_t * p_input, off_t i_off )
{
    access_sys_t * p_sys = p_input->p_access_data;
    unsigned int i_index;

    p_sys->i_sector = p_sys->p_sectors[p_sys->i_track]
                       + i_off / (off_t)VCD_DATA_SIZE;

    vlc_mutex_lock( &p_input->stream.stream_lock );
#define p_area p_input->stream.p_selected_area
    /* Find chapter */
    if( p_sys->b_valid_ep )
    {
        for( i_index = 2 ; i_index <= p_area->i_part_nb; i_index ++ )
        {
            if( p_sys->i_sector < p_sys->p_entries[p_area->i_plugin_data
                + i_index - 1] )
            {
                vlc_value_t val;

                p_area->i_part = i_index - 1;

                /* Update the navigation variables without triggering
                 * a callback */
                val.i_int = p_area->i_part;
                var_Change( p_input, "chapter", VLC_VAR_SETVALUE, &val, NULL );
                break;
            }
        }
    }
#undef p_area

    p_input->stream.p_selected_area->i_tell =
        (off_t)p_sys->i_sector * (off_t)VCD_DATA_SIZE
         - p_input->stream.p_selected_area->i_start;
    vlc_mutex_unlock( &p_input->stream.stream_lock );
}

/*****************************************************************************
 * VCDEntryPoints: Reads the information about the entry points on the disc.
 *****************************************************************************/
static int VCDEntryPoints( input_thread_t * p_input )
{
    access_sys_t * p_sys = p_input->p_access_data;
    byte_t *                          p_sector;
    entries_sect_t                    entries;
    uint16_t                          i_nb;
    int                               i, i_entry_index = 0;
    int                               i_previous_track = -1;

    p_sector = malloc( VCD_DATA_SIZE * sizeof( byte_t ) );
    if( p_sector == NULL )
    {
        msg_Err( p_input, "not enough memory for entry points treatment" );
        return -1;
    }

    if( ioctl_ReadSectors( VLC_OBJECT(p_input), p_sys->vcddev,
        VCD_ENTRIES_SECTOR, p_sector, 1, VCD_TYPE ) < 0 )
    {
        msg_Err( p_input, "could not read entry points sector" );
        free( p_sector );
        return( -1 );
    }

    memcpy( &entries, p_sector, CD_SECTOR_SIZE );
    free( p_sector );

    if( (i_nb = U16_AT( &entries.i_entries_nb )) > 500 )
    {
        msg_Err( p_input, "invalid entry points number" );
        return( -1 );
    }

    p_sys->p_entries = malloc( sizeof( int ) * i_nb );
    if( p_sys->p_entries == NULL )
    {
        msg_Err( p_input, "not enough memory for entry points treatment" );
        return -1;
    }

    if( strncmp( entries.psz_id, "ENTRYVCD", sizeof( entries.psz_id ) )
     && strncmp( entries.psz_id, "ENTRYSVD", sizeof( entries.psz_id ) ))
    {
        msg_Err( p_input, "unrecognized entry points format" );
        free( p_sys->p_entries );
        return -1;
    }

    p_sys->i_entries_nb = 0;

#define i_track BCD_TO_BIN(entries.entry[i].i_track)
    /* Reset the i_part_nb for each track */
    for( i = 0 ; i < i_nb ; i++ )
    {
        if( i_track <= p_input->stream.i_area_nb )
        {
            p_input->stream.pp_areas[i_track-1]->i_part_nb = 0;
        }
    }

    for( i = 0 ; i < i_nb ; i++ )
    {
        if( i_track <= p_input->stream.i_area_nb )
        {
            p_sys->p_entries[i_entry_index] =
                (MSF_TO_LBA2( BCD_TO_BIN( entries.entry[i].msf.minute ),
                              BCD_TO_BIN( entries.entry[i].msf.second ),
                              BCD_TO_BIN( entries.entry[i].msf.frame  ) ));
            p_input->stream.pp_areas[i_track-1]->i_part_nb ++;
            /* if this entry belongs to a new track */
            if( i_track != i_previous_track )
            {
                /* i_plugin_data is used to store the first entry of the area*/
                p_input->stream.pp_areas[i_track-1]->i_plugin_data =
                                                            i_entry_index;
                i_previous_track = i_track;
            }
            msg_Dbg( p_input, "entry point %i begins at LBA: %i",
                     i_entry_index, p_sys->p_entries[i_entry_index] );

            i_entry_index ++;
            p_sys->i_entries_nb ++;
        }
        else
            msg_Warn( p_input, "wrong track number found in entry points" );
    }
#undef i_track
    return 0;
}
#endif

/*****************************************************************************
 * vcd.c : VCD input module for vlc
 *****************************************************************************
 * Copyright (C) 2000 VideoLAN
 * $Id: vcd.c,v 1.21 2003/05/18 15:44:03 gbazin Exp $
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
#include <stdio.h>
#include <stdlib.h>

#include <vlc/vlc.h>
#include <vlc/input.h>

#include "../../demux/mpeg/system.h"

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

#include <string.h>

#include "cdrom.h"

/* how many blocks VCDRead will read in each loop */
#define VCD_BLOCKS_ONCE 20
#define VCD_DATA_ONCE   (VCD_BLOCKS_ONCE * VCD_DATA_SIZE)

/*****************************************************************************
 * thread_vcd_data_t: VCD information
 *****************************************************************************/
typedef struct thread_vcd_data_s
{
    vcddev_t    *vcddev;                            /* vcd device descriptor */
    int         i_nb_tracks;                        /* Nb of tracks (titles) */
    int         i_track;                                    /* Current track */
    int         i_sector;                                  /* Current Sector */
    int *       p_sectors;                                  /* Track sectors */
    int         i_entries_nb;                      /* Number of entry points */
    int *       p_entries;                                   /* Entry points */
    vlc_bool_t  b_valid_ep;                       /* Valid entry points flag */
    vlc_bool_t  b_end_of_track;           /* If the end of track was reached */

} thread_vcd_data_t;

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  VCDOpen         ( vlc_object_t * );
static void VCDClose        ( vlc_object_t * );
static int  VCDRead         ( input_thread_t *, byte_t *, size_t );
static void VCDSeek         ( input_thread_t *, off_t );
static int  VCDSetArea      ( input_thread_t *, input_area_t * );
static int  VCDSetProgram   ( input_thread_t *, pgrm_descriptor_t * );
static int  VCDEntryPoints  ( input_thread_t * );

/*****************************************************************************
 * Module descriptior
 *****************************************************************************/
vlc_module_begin();
    set_description( _("VCD input") );
    set_capability( "access", 80 );
    set_callbacks( VCDOpen, VCDClose );
    add_shortcut( "svcd" );
vlc_module_end();

/*
 * Data reading functions
 */

/*****************************************************************************
 * VCDOpen: open vcd
 *****************************************************************************/
static int VCDOpen( vlc_object_t *p_this )
{
    input_thread_t *        p_input = (input_thread_t *)p_this;
    char *                  psz_orig;
    char *                  psz_parser;
    char *                  psz_source;
    char *                  psz_next;
    thread_vcd_data_t *     p_vcd;
    int                     i;
    input_area_t *          p_area;
    int                     i_title = 1;
    int                     i_chapter = 1;
    vcddev_t                *vcddev;

#ifdef WIN32
    /* On Win32 we want the VCD access plugin to be explicitly requested,
     * we end up with lots of problems otherwise */
    if( !p_input->psz_access || !*p_input->psz_access ) return( -1 );
#endif

    /* parse the options passed in command line : */
    psz_orig = psz_parser = psz_source = strdup( p_input->psz_name );

    if( !psz_orig )
    {
        return( -1 );
    }

    while( *psz_parser && *psz_parser != '@' )
    {
        psz_parser++;
    }

    if( *psz_parser == '@' )
    {
        /* Found options */
        *psz_parser = '\0';
        ++psz_parser;

        i_title = (int)strtol( psz_parser, &psz_next, 10 );
        if( *psz_next )
        {
            psz_parser = psz_next + 1;
            i_chapter = (int)strtol( psz_parser, &psz_next, 10 );
        }

        i_title = i_title ? i_title : 1;
        i_chapter = i_chapter ? i_chapter : 1;
    }

    if( !*psz_source )
    {
        if( !p_input->psz_access )
        {
            free( psz_orig );
            return -1;
        }
        psz_source = config_GetPsz( p_input, "vcd" );
        if( !psz_source ) return -1;
    }

    /* Open VCD */
    if( !(vcddev = ioctl_Open( p_this, psz_source )) )
    {
        msg_Warn( p_input, "could not open %s", psz_source );
        free( psz_source );
        return -1;
    }

    p_vcd = malloc( sizeof(thread_vcd_data_t) );
    if( p_vcd == NULL )
    {
        msg_Err( p_input, "out of memory" );
        free( psz_source );
        return -1;
    }
    free( psz_source );

    p_vcd->vcddev = vcddev;
    p_input->p_access_data = (void *)p_vcd;

    p_input->i_mtu = VCD_DATA_ONCE;

    vlc_mutex_lock( &p_input->stream.stream_lock );
    p_input->stream.b_pace_control = 1;
    p_input->stream.b_seekable = 1;
    p_input->stream.p_selected_area->i_size = 0;
    p_input->stream.p_selected_area->i_tell = 0;
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    /* We read the Table Of Content information */
    p_vcd->i_nb_tracks = ioctl_GetTracksMap( VLC_OBJECT(p_input),
                                           p_vcd->vcddev, &p_vcd->p_sectors );
    if( p_vcd->i_nb_tracks < 0 )
        msg_Err( p_input, "unable to count tracks" );
    else if( p_vcd->i_nb_tracks <= 1 )
        msg_Err( p_input, "no movie tracks found" );
    if( p_vcd->i_nb_tracks <= 1)
    {
        ioctl_Close( p_this, p_vcd->vcddev );
        free( p_vcd );
        return -1;
    }

    /* Allocate the entry points table */
    p_vcd->p_entries = malloc( p_vcd->i_nb_tracks * sizeof( int ) );

    if( p_vcd->p_entries == NULL )
    {
        msg_Err( p_input, "not enough memory" );
        ioctl_Close( p_this, p_vcd->vcddev );
        free( p_vcd );
    }

    /* Set stream and area data */
    vlc_mutex_lock( &p_input->stream.stream_lock );

    /* Initialize ES structures */
    input_InitStream( p_input, sizeof( stream_ps_data_t ) );

    /* disc input method */
    p_input->stream.i_method = INPUT_METHOD_VCD;

    p_input->stream.i_area_nb = 1;

#define area p_input->stream.pp_areas
    for( i = 1 ; i <= p_vcd->i_nb_tracks - 1 ; i++ )
    {
        /* Titles are Program Chains */
        input_AddArea( p_input, i, 1 );

        /* Absolute start offset and size */
        area[i]->i_start = (off_t)p_vcd->p_sectors[i] * (off_t)VCD_DATA_SIZE;
        area[i]->i_size = (off_t)(p_vcd->p_sectors[i+1] - p_vcd->p_sectors[i])
                           * (off_t)VCD_DATA_SIZE;

        /* Default Chapter */
        area[i]->i_part = 1;

        /* i_plugin_data is used to store which entry point is the first
         * of the track (area) */
        area[i]->i_plugin_data = 0;
    }
#undef area

    p_area = p_input->stream.pp_areas[i_title];

    p_vcd->b_valid_ep = 1;
    if( VCDEntryPoints( p_input ) < 0 )
    {
        msg_Warn( p_input, "could not read entry points, will not use them" );
        p_vcd->b_valid_ep = 0;
    }

    VCDSetArea( p_input, p_area );

    vlc_mutex_unlock( &p_input->stream.stream_lock );

    if( !p_input->psz_demux || !*p_input->psz_demux )
    {
        p_input->psz_demux = "ps";
    }

    p_input->pf_read = VCDRead;
    p_input->pf_seek = VCDSeek;
    p_input->pf_set_area = VCDSetArea;
    p_input->pf_set_program = VCDSetProgram;

    return 0;
}

/*****************************************************************************
 * VCDClose: closes vcd
 *****************************************************************************/
static void VCDClose( vlc_object_t *p_this )
{
    input_thread_t *   p_input = (input_thread_t *)p_this;
    thread_vcd_data_t *p_vcd = (thread_vcd_data_t *)p_input->p_access_data;

    ioctl_Close( p_this, p_vcd->vcddev );
    free( p_vcd );
}

/*****************************************************************************
 * VCDRead: reads from the VCD into PES packets.
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, otherwise the number of
 * bytes.
 *****************************************************************************/
static int VCDRead( input_thread_t * p_input, byte_t * p_buffer,
                     size_t i_len )
{
    thread_vcd_data_t *     p_vcd;
    int                     i_blocks;
    int                     i_index;
    int                     i_read;
    byte_t                  p_last_sector[ VCD_DATA_SIZE ];

    p_vcd = (thread_vcd_data_t *)p_input->p_access_data;

    i_read = 0;

    /* Compute the number of blocks we have to read */

    i_blocks = i_len / VCD_DATA_SIZE;

    for ( i_index = 0 ; i_index < i_blocks ; i_index++ )
    {
        if ( ioctl_ReadSectors( VLC_OBJECT(p_input), p_vcd->vcddev,
             p_vcd->i_sector, p_buffer + i_index * VCD_DATA_SIZE, 1,
             VCD_TYPE ) < 0 )
        {
            msg_Err( p_input, "could not read sector %d", p_vcd->i_sector );
            return -1;
        }

        p_vcd->i_sector ++;
        if ( p_vcd->i_sector == p_vcd->p_sectors[p_vcd->i_track + 1] )
        {
            input_area_t *p_area;

            if ( p_vcd->i_track >= p_vcd->i_nb_tracks - 1 )
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
        else if( p_vcd->b_valid_ep &&
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
            if( i_entry + 1 < p_vcd->i_entries_nb &&
                    p_vcd->i_sector >= p_vcd->p_entries[i_entry + 1] )
            {
                msg_Dbg( p_input, "new chapter" );
                p_input->stream.p_selected_area->i_part ++;
            }
            vlc_mutex_unlock( &p_input->stream.stream_lock );
        }

        i_read += VCD_DATA_SIZE;
    }

    if ( i_len % VCD_DATA_SIZE ) /* this should not happen */
    {
        if ( ioctl_ReadSectors( VLC_OBJECT(p_input), p_vcd->vcddev,
             p_vcd->i_sector, p_last_sector, 1, VCD_TYPE ) < 0 )
        {
            msg_Err( p_input, "could not read sector %d", p_vcd->i_sector );
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
    thread_vcd_data_t *     p_vcd;
    vlc_value_t val;

    p_vcd = (thread_vcd_data_t*)p_input->p_access_data;

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
        p_vcd->i_track = p_area->i_id;
        p_vcd->i_sector = p_vcd->p_sectors[p_vcd->i_track];

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

    if( p_vcd->b_valid_ep )
    {
        int i_entry = p_area->i_plugin_data /* 1st entry point of
                                               the track (area)*/
                            + p_area->i_part - 1;
        p_vcd->i_sector = p_vcd->p_entries[i_entry];
    }
    else
        p_vcd->i_sector = p_vcd->p_sectors[p_vcd->i_track];

    p_input->stream.p_selected_area->i_tell =
        (off_t)p_vcd->i_sector * (off_t)VCD_DATA_SIZE
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
    thread_vcd_data_t * p_vcd;
    unsigned int i_index;

    p_vcd = (thread_vcd_data_t *) p_input->p_access_data;

    p_vcd->i_sector = p_vcd->p_sectors[p_vcd->i_track]
                       + i_off / (off_t)VCD_DATA_SIZE;

    vlc_mutex_lock( &p_input->stream.stream_lock );
#define p_area p_input->stream.p_selected_area
    /* Find chapter */
    if( p_vcd->b_valid_ep )
    {
        for( i_index = 0 ; i_index < p_area->i_part_nb - 1 ; i_index ++ )
        {
            if( p_vcd->i_sector < p_vcd->p_entries[p_area->i_plugin_data
                + i_index + 1] )
            {
                p_area->i_part = i_index;
                break;
            }
        }
    }
#undef p_area

    p_input->stream.p_selected_area->i_tell =
        (off_t)p_vcd->i_sector * (off_t)VCD_DATA_SIZE
         - p_input->stream.p_selected_area->i_start;
    vlc_mutex_unlock( &p_input->stream.stream_lock );
}

/*****************************************************************************
 * VCDEntryPoints: Reads the information about the entry points on the disc.
 *****************************************************************************/
static int VCDEntryPoints( input_thread_t * p_input )
{
    thread_vcd_data_t *               p_vcd;
    byte_t *                          p_sector;
    entries_sect_t                    entries;
    uint16_t                          i_nb;
    int                               i, i_entry_index = 0;
    int                               i_previous_track = -1;

    p_vcd = (thread_vcd_data_t *) p_input->p_access_data;

    p_sector = malloc( VCD_DATA_SIZE * sizeof( byte_t ) );
    if( p_sector == NULL )
    {
        msg_Err( p_input, "not enough memory for entry points treatment" );
        return -1;
    }

    if( ioctl_ReadSectors( VLC_OBJECT(p_input), p_vcd->vcddev,
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

    p_vcd->p_entries = malloc( sizeof( int ) * i_nb );
    if( p_vcd->p_entries == NULL )
    {
        msg_Err( p_input, "not enough memory for entry points treatment" );
        return -1;
    }

    if( strncmp( entries.psz_id, "ENTRYVCD", sizeof( entries.psz_id ) )
     && strncmp( entries.psz_id, "ENTRYSVD", sizeof( entries.psz_id ) ))
    {
        msg_Err( p_input, "unrecognized entry points format" );
        free( p_vcd->p_entries );
        return -1;
    }

    p_vcd->i_entries_nb = 0;

#define i_track BCD_TO_BIN(entries.entry[i].i_track)
    for( i = 0 ; i < i_nb ; i++ )
    {
        if( i_track <= p_input->stream.i_area_nb )
        {
            p_vcd->p_entries[i_entry_index] =
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
            i_entry_index ++;
            p_vcd->i_entries_nb ++;
        }
        else
            msg_Warn( p_input, "wrong track number found in entry points" );
    }
#undef i_track
    return 0;
}

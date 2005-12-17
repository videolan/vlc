/*****************************************************************************
 * cdda.c : CD digital audio input module for vlc
 *****************************************************************************
 * Copyright (C) 2000, 2003 the VideoLAN team
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Gildas Bazin <gbazin@netcourrier.com>
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

#include "codecs.h"
#include "vcd/cdrom.h"

#include <vlc_playlist.h>

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
    set_shortname( _("Audio CD"));
    set_description( _("Audio CD input") );
    set_capability( "access2", 10 );
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_ACCESS );
    set_callbacks( Open, Close );

    add_usage_hint( N_("[cdda:][device][@[track]]") );
    add_integer( "cdda-caching", DEFAULT_PTS_DELAY / 1000, NULL, CACHING_TEXT,
                 CACHING_LONGTEXT, VLC_TRUE );
    add_bool( "cdda-separate-tracks", VLC_TRUE, NULL, NULL, NULL, VLC_TRUE );
    add_integer( "cdda-track", -1 , NULL, NULL, NULL, VLC_TRUE );
    add_shortcut( "cdda" );
    add_shortcut( "cddasimple" );
vlc_module_end();


/* how many blocks VCDRead will read in each loop */
#define CDDA_BLOCKS_ONCE 20
#define CDDA_DATA_ONCE   (CDDA_BLOCKS_ONCE * CDDA_DATA_SIZE)

/*****************************************************************************
 * Access: local prototypes
 *****************************************************************************/
struct access_sys_t
{
    vcddev_t    *vcddev;                            /* vcd device descriptor */

    /* Title infos */
    int           i_titles;
    input_title_t *title[99];         /* No more that 99 track in a cd-audio */

    /* Current position */
    int         i_sector;                                  /* Current Sector */
    int *       p_sectors;                                  /* Track sectors */

    /* Wave header for the output data */
    WAVEHEADER  waveheader;
    vlc_bool_t  b_header;

    vlc_bool_t  b_separate_items;
    vlc_bool_t  b_single_track;
    int         i_track;
};

static block_t *Block( access_t * );
static int      Seek( access_t *, int64_t );
static int      Control( access_t *, int, va_list );

static int GetTracks( access_t *p_access, vlc_bool_t b_separate,
                      playlist_t *p_playlist, playlist_item_t *p_parent );

/*****************************************************************************
 * Open: open cdda
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    access_t     *p_access = (access_t*)p_this;
    access_sys_t *p_sys;

    vcddev_t *vcddev;
    char *psz_name;

    vlc_bool_t b_separate_requested;
    vlc_bool_t b_play = VLC_FALSE;
    input_thread_t *p_input;
    playlist_item_t *p_item = NULL;
    playlist_t *p_playlist  = NULL;
    int i_ret;
    int i_track;

    if( !p_access->psz_path || !*p_access->psz_path )
    {
        /* Only when selected */
        if( !p_this->b_force ) return VLC_EGENERIC;

        psz_name = var_CreateGetString( p_this, "cd-audio" );
        if( !psz_name || !*psz_name )
        {
            if( psz_name ) free( psz_name );
            return VLC_EGENERIC;
        }
    }
    else psz_name = strdup( p_access->psz_path );

    /* Open CDDA */
    if( (vcddev = ioctl_Open( VLC_OBJECT(p_access), psz_name )) == NULL )
    {
        msg_Warn( p_access, "could not open %s", psz_name );
        free( psz_name );
        return VLC_EGENERIC;
    }
    free( psz_name );

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
    p_sys->b_separate_items = VLC_FALSE;
    p_sys->b_single_track = VLC_FALSE;
    p_sys->b_header = VLC_FALSE;

    b_separate_requested = var_CreateGetBool( p_access,
                                              "cdda-separate-tracks" );

    /* We only do separate items if the whole disc is requested -
     *  Dirty hack we access some private data ! */
    p_input = (input_thread_t *)( p_access->p_parent );
    /* Do we play a single track ? */
    i_track = var_CreateGetInteger( p_access, "cdda-track" );
    if( b_separate_requested && p_input->input.i_title_start == -1 &&
        i_track <= 0 )
    {
        p_sys->b_separate_items = VLC_TRUE;
    }
    if( i_track > 0 )
    {
        p_sys->b_single_track = VLC_TRUE;
        p_sys->i_track = i_track - 1;
    }

    msg_Dbg( p_access, "Separate items : %i - Single track : %i",
                        p_sys->b_separate_items, p_sys->b_single_track );

    if( p_sys->b_separate_items )
    {
        p_playlist = (playlist_t *) vlc_object_find( p_access,
                                       VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );

        if( !p_playlist ) return VLC_EGENERIC;

        /* Let's check if we need to play */
        if( &p_playlist->status.p_item->input ==
             ((input_thread_t *)p_access->p_parent)->input.p_item )
        {
            p_item = p_playlist->status.p_item;
            b_play = VLC_TRUE;
            msg_Dbg( p_access, "starting Audio CD playback");
        }
        else
        {
            input_item_t *p_current = ( (input_thread_t*)p_access->p_parent)->
                                         input.p_item;
            p_item = playlist_LockItemGetByInput( p_playlist, p_current );
            msg_Dbg( p_access, "not starting Audio CD playback");

            if( !p_item )
            {
                msg_Dbg( p_playlist, "unable to find item in playlist");
               return -1;
            }
            b_play = VLC_FALSE;
        }
    }

    /* We read the Table Of Content information */
    if( 1 )
    {
        i_ret = GetTracks( p_access, p_sys->b_separate_items,
                           p_playlist, p_item );
        if( i_ret < 0 )
        {
            goto error;
        }
    }

    /* Build a WAV header for the output data */
    memset( &p_sys->waveheader, 0, sizeof(WAVEHEADER) );
    SetWLE( &p_sys->waveheader.Format, 1 ); /*WAVE_FORMAT_PCM*/
    SetWLE( &p_sys->waveheader.BitsPerSample, 16);
    p_sys->waveheader.MainChunkID = VLC_FOURCC('R', 'I', 'F', 'F');
    p_sys->waveheader.Length = 0;                     /* we just don't know */
    p_sys->waveheader.ChunkTypeID = VLC_FOURCC('W', 'A', 'V', 'E');
    p_sys->waveheader.SubChunkID = VLC_FOURCC('f', 'm', 't', ' ');
    SetDWLE( &p_sys->waveheader.SubChunkLength, 16);
    SetWLE( &p_sys->waveheader.Modus, 2);
    SetDWLE( &p_sys->waveheader.SampleFreq, 44100);
    SetWLE( &p_sys->waveheader.BytesPerSample,
            2 /*Modus*/ * 16 /*BitsPerSample*/ / 8 );
    SetDWLE( &p_sys->waveheader.BytesPerSec,
             2*16/8 /*BytesPerSample*/ * 44100 /*SampleFreq*/ );
    p_sys->waveheader.DataChunkID = VLC_FOURCC('d', 'a', 't', 'a');
    p_sys->waveheader.DataLength = 0;                 /* we just don't know */

    /* Only update META if we are in old mode */
    if( !p_sys->b_separate_items && !p_sys->b_single_track )
    {
        p_access->info.i_update |= INPUT_UPDATE_META;
    }

    /* Position on the right sector and update size */
    if( p_sys->b_single_track )
    {
        p_sys->i_sector = p_sys->p_sectors[ p_sys->i_track ];
        p_access->info.i_size =
            ( p_sys->p_sectors[p_sys->i_track+1] -
              p_sys->p_sectors[p_sys->i_track] ) *
                        (int64_t)CDDA_DATA_SIZE;
    }

    /* PTS delay */
    var_Create( p_access, "cdda-caching", VLC_VAR_INTEGER|VLC_VAR_DOINHERIT );

    if( b_play )
    {
        playlist_Control( p_playlist, PLAYLIST_VIEWPLAY,
                          p_playlist->status.i_view, p_playlist->status.p_item,
                          NULL );
    }

    if( p_playlist ) vlc_object_release( p_playlist );

    return VLC_SUCCESS;

error:
    ioctl_Close( VLC_OBJECT(p_access), p_sys->vcddev );
    free( p_sys );
    if( p_playlist ) vlc_object_release( p_playlist );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Close: closes cdda
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    access_t     *p_access = (access_t *)p_this;
    access_sys_t *p_sys = p_access->p_sys;
    int          i;

    for( i = 0; i < p_sys->i_titles; i++ )
    {
        vlc_input_title_Delete( p_sys->title[i] );
    }

    ioctl_Close( p_this, p_sys->vcddev );
    free( p_sys );
}

/*****************************************************************************
 * Block: read data (CDDA_DATA_ONCE)
 *****************************************************************************/
static block_t *Block( access_t *p_access )
{
    access_sys_t *p_sys = p_access->p_sys;
    int i_blocks = CDDA_BLOCKS_ONCE;
    block_t *p_block;

    if( p_sys->b_separate_items ) p_access->info.b_eof = VLC_TRUE;

    /* Check end of file */
    if( p_access->info.b_eof ) return NULL;

    if( !p_sys->b_header )
    {
        /* Return only the header */
        p_block = block_New( p_access, sizeof( WAVEHEADER ) );
        memcpy( p_block->p_buffer, &p_sys->waveheader, sizeof(WAVEHEADER) );
        p_sys->b_header = VLC_TRUE;
        return p_block;
    }

    /* Check end of title - Single track */
    if( p_sys->b_single_track )
    {
        if( p_sys->i_sector >= p_sys->p_sectors[p_sys->i_track + 1] )
        {
            p_access->info.b_eof = VLC_TRUE;
            return NULL;
        }
        /* Don't read too far */
        if( p_sys->i_sector + i_blocks >=
            p_sys->p_sectors[p_sys->i_track +1] )
        {
            i_blocks = p_sys->p_sectors[p_sys->i_track+1] - p_sys->i_sector;
        }
    }
    else
    {
        /* Check end of title - Normal */
        while( p_sys->i_sector >= p_sys->p_sectors[p_access->info.i_title + 1] )
        {
            if( p_access->info.i_title + 1 >= p_sys->i_titles )
            {
                p_access->info.b_eof = VLC_TRUE;
                return NULL;
            }

            p_access->info.i_update |= INPUT_UPDATE_TITLE | INPUT_UPDATE_SIZE |
                                       INPUT_UPDATE_META;
            p_access->info.i_title++;
            p_access->info.i_size =
                        p_sys->title[p_access->info.i_title]->i_size;
            p_access->info.i_pos = 0;
        }
        /* Don't read too far */
        if( p_sys->i_sector + i_blocks >=                                                       p_sys->p_sectors[p_access->info.i_title + 1] )
        {
            i_blocks = p_sys->p_sectors[ p_access->info.i_title + 1] -
                         p_sys->i_sector;
        }
    }

    /* Do the actual reading */
    if( !( p_block = block_New( p_access, i_blocks * CDDA_DATA_SIZE ) ) )
    {
        msg_Err( p_access, "cannot get a new block of size: %i",
                 i_blocks * CDDA_DATA_SIZE );
        return NULL;
    }

    if( ioctl_ReadSectors( VLC_OBJECT(p_access), p_sys->vcddev,
            p_sys->i_sector, p_block->p_buffer, i_blocks, CDDA_TYPE ) < 0 )
    {
        msg_Err( p_access, "cannot read sector %i", p_sys->i_sector );
        block_Release( p_block );

        /* Try to skip one sector (in case of bad sectors) */
        p_sys->i_sector++;
        p_access->info.i_pos += CDDA_DATA_SIZE;
        return NULL;
    }

    /* Update a few values */
    p_sys->i_sector += i_blocks;
    p_access->info.i_pos += p_block->i_buffer;

    return p_block;
}

/****************************************************************************
 * Seek
 ****************************************************************************/
static int Seek( access_t *p_access, int64_t i_pos )
{
    access_sys_t *p_sys = p_access->p_sys;

    /* Next sector to read */
    p_sys->i_sector = p_sys->p_sectors[
                                        (p_sys->b_single_track ?
                                         p_sys->i_track :
                                         p_access->info.i_title + 1)
                                      ] +
                                i_pos / CDDA_DATA_SIZE;
    p_access->info.i_pos = i_pos;

    return VLC_SUCCESS;
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
    char         *psz_title;
    vlc_meta_t  **pp_meta;

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
            *pi_int = CDDA_DATA_ONCE;
            break;

        case ACCESS_GET_PTS_DELAY:
            pi_64 = (int64_t*)va_arg( args, int64_t * );
            *pi_64 = var_GetInteger( p_access, "cdda-caching" ) * 1000;
            break;

        /* */
        case ACCESS_SET_PAUSE_STATE:
            break;

        case ACCESS_GET_TITLE_INFO:
            if( p_sys->b_single_track )
                return VLC_EGENERIC;
            ppp_title = (input_title_t***)va_arg( args, input_title_t*** );
            pi_int    = (int*)va_arg( args, int* );
            *((int*)va_arg( args, int* )) = 1; /* Title offset */

            /* Duplicate title infos */
            *pi_int = p_sys->i_titles;
            *ppp_title = malloc(sizeof( input_title_t **) * p_sys->i_titles );
            for( i = 0; i < p_sys->i_titles; i++ )
            {
                (*ppp_title)[i] = vlc_input_title_Duplicate( p_sys->title[i] );
            }
            break;

        case ACCESS_SET_TITLE:
            if( p_sys->b_single_track ) return VLC_EGENERIC;
            i = (int)va_arg( args, int );
            if( i != p_access->info.i_title )
            {
                /* Update info */
                p_access->info.i_update |=
                    INPUT_UPDATE_TITLE|INPUT_UPDATE_SIZE|INPUT_UPDATE_META;
                p_access->info.i_title = i;
                p_access->info.i_size = p_sys->title[i]->i_size;
                p_access->info.i_pos = 0;

                /* Next sector to read */
                p_sys->i_sector = p_sys->p_sectors[i];
            }
            break;

        case ACCESS_GET_META:
             if( p_sys->b_single_track ) return VLC_EGENERIC;
             psz_title = malloc( strlen( _("Audio CD - Track ") ) + 5 );
             snprintf( psz_title, 100, _("Audio CD - Track %i" ),
                                        p_access->info.i_title+1 );
             pp_meta = (vlc_meta_t**)va_arg( args, vlc_meta_t** );
             *pp_meta = vlc_meta_New();
             vlc_meta_Add( *pp_meta, VLC_META_TITLE, psz_title );
             free( psz_title );
             break;

        case ACCESS_SET_SEEKPOINT:
        case ACCESS_SET_PRIVATE_ID_STATE:
            return VLC_EGENERIC;

        default:
            msg_Warn( p_access, "unimplemented query in control" );
            return VLC_EGENERIC;

    }
    return VLC_SUCCESS;
}

static int GetTracks( access_t *p_access, vlc_bool_t b_separate,
                      playlist_t *p_playlist, playlist_item_t *p_parent )
{
    access_sys_t *p_sys = p_access->p_sys;
    int i;
    playlist_item_t *p_item;
    p_sys->i_titles = ioctl_GetTracksMap( VLC_OBJECT(p_access),
                                          p_sys->vcddev, &p_sys->p_sectors );
    if( p_sys->i_titles < 0 )
    {
        msg_Err( p_access, "unable to count tracks" );
        return VLC_EGENERIC;;
    }
    else if( p_sys->i_titles <= 0 )
    {
        msg_Err( p_access, "no audio tracks found" );
        return VLC_EGENERIC;
    }

    if( b_separate )
    {
        if( p_parent->i_children == -1 )
        {
            playlist_LockItemToNode( p_playlist, p_parent );
        }
    }

    /* Build title table */
    for( i = 0; i < p_sys->i_titles; i++ )
    {
        msg_Dbg( p_access, "track[%d] start=%d", i, p_sys->p_sectors[i] );
        if( !b_separate )
        {
            input_title_t *t = p_sys->title[i] = vlc_input_title_New();

            asprintf( &t->psz_name, _("Track %i"), i + 1 );
            t->i_size = ( p_sys->p_sectors[i+1] - p_sys->p_sectors[i] ) *
                        (int64_t)CDDA_DATA_SIZE;

            t->i_length = I64C(1000000) * t->i_size / 44100 / 4;
        }
        else
        {
            char *psz_uri;
            int i_path_len = p_access->psz_path ? strlen( p_access->psz_path )
                                                : 0;
            char *psz_name = malloc( strlen( _("Audio CD - Track ") ) + 5 );
            char *psz_opt = (char*)malloc( 14 );

            psz_uri = (char*)malloc( i_path_len + 13 );
            snprintf( psz_uri, i_path_len + 13, "cdda://%s",
                                p_access->psz_path ? p_access->psz_path : "" );
            snprintf( psz_name, 100, _("Audio CD - Track %i" ),
                                     (i+1) );
            snprintf( psz_opt, 14, "cdda-track=%i", i+1 );
            /* Create playlist items */
            p_item = playlist_ItemNewWithType( VLC_OBJECT( p_playlist ),
                                 psz_uri, psz_name, ITEM_TYPE_DISC );
            playlist_ItemAddOption( p_item, psz_opt );
            playlist_NodeAddItem( p_playlist, p_item,
                                  p_parent->pp_parents[0]->i_view,
                                  p_parent, PLAYLIST_APPEND, PLAYLIST_END );
            free( psz_uri ); free( psz_opt );
        }
    }

    p_sys->i_sector = p_sys->p_sectors[0];
    if( p_sys->b_separate_items )
    {
        p_sys->i_titles = 0;
    }

   return VLC_SUCCESS;
}

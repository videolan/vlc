/*****************************************************************************
 * audio_video.c: Audio/Video management : volume, snapshot, OSD
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
 * $Id: vlc.c 10786 2005-04-23 23:19:17Z zorglub $
 *
 * Authors: Olivier Aubert <olivier.aubert@liris.univ-lyon1.fr>
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

#include <vlc/control.h>

#include <vlc/intf.h>
#include <vlc/vout.h>
#include <vlc/aout.h>
#include <vlc_demux.h>

#include <vlc_osd.h>

#define HAS_SNAPSHOT 1

#ifdef HAS_SNAPSHOT
#include <snapshot.h>
#endif

#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>

#include <errno.h>                                                 /* ENOMEM */
#include <stdio.h>
#include <ctype.h>

#ifdef HAVE_UNISTD_H
#    include <unistd.h>
#endif
#ifdef HAVE_SYS_TIME_H
#    include <sys/time.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#    include <sys/types.h>
#endif

#define RAISE( c, m )  exception->code = c; \
                       exception->message = strdup(m);

mediacontrol_RGBPicture *
mediacontrol_snapshot( mediacontrol_Instance *self,
                       const mediacontrol_Position * a_position,
                       mediacontrol_Exception *exception )
{
    mediacontrol_RGBPicture *retval = NULL;
    input_thread_t* p_input = self->p_playlist->p_input;
    vout_thread_t *p_vout = NULL;
    int i_datasize;
    snapshot_t **pointer;
    vlc_value_t val;
    int i_index;
    snapshot_t *p_best_snapshot;
    long searched_date;
#ifdef HAS_SNAPSHOT
    int i_cachesize;
#endif

    exception=mediacontrol_exception_init( exception );

    /*
       if( var_Get( self->p_vlc, "snapshot-id", &val ) == VLC_SUCCESS )
       p_vout = vlc_object_get( self->p_vlc, val.i_int );
    */

    /* FIXME: if in p_libvlc, we cannot have multiple video outputs */
    /* Once corrected, search for snapshot-id to modify all instances */
    if( var_Get( p_input, "snapshot-id", &val ) != VLC_SUCCESS )
    {
        RAISE( mediacontrol_InternalException, "No snapshot-id in p_input" );
        return NULL;
    }
    p_vout = vlc_object_get( self->p_vlc, val.i_int );

    if( ! p_vout )
    {
        RAISE( mediacontrol_InternalException, "No snapshot module" );
        return NULL;
    }

#ifdef HAS_SNAPSHOT
    /* We test if the vout is a snapshot module. We cannot test
       pvout_psz_object_name( which is NULL ). But we can check if
       there are snapshot-specific variables */
    if( var_Get( p_vout, "snapshot-datasize", &val ) != VLC_SUCCESS )
    {
        RAISE( mediacontrol_InternalException, "No snapshot module" );
        vlc_object_release( p_vout );
        return NULL;
    }
    i_datasize = val.i_int;

    /* Handle the a_position parameter */
    if( ! ( a_position->origin == mediacontrol_RelativePosition
            && a_position->value == 0 ) )
    {
        /* The position is not the current one. Go to it. */
        mediacontrol_set_media_position( self,
                                         ( mediacontrol_Position* ) a_position,
                                         exception );
        if( exception->code )
        {
            vlc_object_release( p_vout );
            return NULL;
        }
    }

    /* FIXME: We should not go further until we got past the position
       ( which means that we had the possibility to capture the right
       picture ). */

    vlc_mutex_lock( &p_vout->picture_lock );

    searched_date = mediacontrol_position2microsecond( p_input,
                                                       ( mediacontrol_Position * ) a_position );

    var_Get( p_vout, "snapshot-cache-size", &val );
    i_cachesize = val.i_int  ;

    var_Get( p_vout, "snapshot-list-pointer", &val );
    pointer = ( snapshot_t ** )val.p_address;

    if( ! pointer )
    {
        RAISE( mediacontrol_InternalException, "No available snapshot" );

        vlc_mutex_unlock( &p_vout->picture_lock );
        vlc_object_release( p_vout );
        return NULL;
    }

    /* Find the more appropriate picture, based on date */
    p_best_snapshot = pointer[0];

    for( i_index = 1 ; i_index < i_cachesize ; i_index++ )
    {
        long l_diff = pointer[i_index]->date - searched_date;
        if( l_diff > 0 && l_diff < abs( p_best_snapshot->date - searched_date ))
        {
            /* This one is closer, and _after_ the requested position */
            p_best_snapshot = pointer[i_index];
        }
    }

    /* FIXME: add a test for the case that no picture matched the test
       ( we have p_best_snapshot == pointer[0] */
    retval = _mediacontrol_createRGBPicture( p_best_snapshot->i_width,
                                             p_best_snapshot->i_height,
                                             p_vout->output.i_chroma,
                                             p_best_snapshot->date,
                                             p_best_snapshot->p_data,
                                             i_datasize );

    vlc_mutex_unlock( &p_vout->picture_lock );
    vlc_object_release( p_vout );

#endif

    return retval;
}

mediacontrol_RGBPicture **
mediacontrol_all_snapshots( mediacontrol_Instance *self,
                            mediacontrol_Exception *exception )
{
    mediacontrol_RGBPicture **retval = NULL;
    vout_thread_t *p_vout = NULL;
    int i_datasize;
    int i_cachesize;
    vlc_value_t val;
    int i_index;
#ifdef HAS_SNAPSHOT
    snapshot_t **pointer;
#endif

    exception=mediacontrol_exception_init( exception );

    if( var_Get( self->p_playlist->p_input, "snapshot-id", &val ) == VLC_SUCCESS )
        p_vout = vlc_object_get( self->p_vlc, val.i_int );

    if( ! p_vout )
    {
        RAISE( mediacontrol_InternalException, "No snapshot module" );
        return NULL;
    }
#ifdef HAS_SNAPSHOT
    /* We test if the vout is a snapshot module. We cannot test
       pvout_psz_object_name( which is NULL ). But we can check if
       there are snapshot-specific variables */
    if( var_Get( p_vout, "snapshot-datasize", &val ) != VLC_SUCCESS )
    {
        RAISE( mediacontrol_InternalException, "No snapshot module" );
        vlc_object_release( p_vout );
        return NULL;
    }
    i_datasize = val.i_int;

    vlc_mutex_lock( &p_vout->picture_lock );

    var_Get( p_vout, "snapshot-cache-size", &val );
    i_cachesize = val.i_int  ;

    var_Get( p_vout, "snapshot-list-pointer", &val );
    pointer = ( snapshot_t ** )val.p_address;

    if( ! pointer )
    {
        RAISE( mediacontrol_InternalException, "No available picture" );

        vlc_mutex_unlock( &p_vout->picture_lock );
        vlc_object_release( p_vout );
        return NULL;
    }

    retval = ( mediacontrol_RGBPicture** )malloc( (i_cachesize + 1 ) * sizeof( char* ));

    for( i_index = 0 ; i_index < i_cachesize ; i_index++ )
    {
        snapshot_t *p_s = pointer[i_index];
        mediacontrol_RGBPicture *p_rgb;

        p_rgb = _mediacontrol_createRGBPicture( p_s->i_width,
                                                p_s->i_height,
                                                p_vout->output.i_chroma,
                                                p_s->date,
                                                p_s->p_data,
                                                i_datasize );

        retval[i_index] = p_rgb;
    }

    retval[i_cachesize] = NULL;

    vlc_mutex_unlock( &p_vout->picture_lock );
    vlc_object_release( p_vout );

#endif

    return retval;
}

int mediacontrol_showtext( vout_thread_t *p_vout, int i_channel,
                           char *psz_string, text_style_t *p_style,
                           int i_flags, int i_hmargin, int i_vmargin,
                           mtime_t i_start, mtime_t i_stop )
{
    subpicture_t *p_spu;
    video_format_t fmt;

    if( !psz_string ) return VLC_EGENERIC;

    p_spu = spu_CreateSubpicture( p_vout->p_spu );
    if( !p_spu ) return VLC_EGENERIC;

    /* Create a new subpicture region */
    memset( &fmt, 0, sizeof(video_format_t) );
    fmt.i_chroma = VLC_FOURCC('T','E','X','T');
    fmt.i_aspect = 0;
    fmt.i_width = fmt.i_height = 0;
    fmt.i_x_offset = fmt.i_y_offset = 0;
    p_spu->p_region = p_spu->pf_create_region( VLC_OBJECT(p_vout), &fmt );
    if( !p_spu->p_region )
    {
        msg_Err( p_vout, "cannot allocate SPU region" );
        spu_DestroySubpicture( p_vout->p_spu, p_spu );
        return VLC_EGENERIC;
    }

    p_spu->p_region->psz_text = strdup( psz_string );
    p_spu->i_start = i_start;
    p_spu->i_stop = i_stop;
    p_spu->b_ephemer = VLC_FALSE;
    p_spu->b_absolute = VLC_FALSE;

    p_spu->i_x = i_hmargin;
    p_spu->i_y = i_vmargin;
    p_spu->i_flags = i_flags;
    p_spu->i_channel = i_channel;

    spu_DisplaySubpicture( p_vout->p_spu, p_spu );

    return VLC_SUCCESS;
}


void
mediacontrol_display_text( mediacontrol_Instance *self,
                           const char * message,
                           const mediacontrol_Position * begin,
                           const mediacontrol_Position * end,
                           mediacontrol_Exception *exception )
{
    input_thread_t *p_input = NULL;
    vout_thread_t *p_vout = NULL;
    char* psz_message;

    psz_message = strdup( message );
    if( !psz_message )
    {
        RAISE( mediacontrol_InternalException, "No more memory" );
        return;
    }

    p_vout = vlc_object_find( self->p_playlist, VLC_OBJECT_VOUT, FIND_CHILD );
    if( ! p_vout )
    {
        RAISE( mediacontrol_InternalException, "No video output" );
        return;
    }

    if( begin->origin == mediacontrol_RelativePosition &&
        begin->value == 0 &&
        end->origin == mediacontrol_RelativePosition )
    {
        mtime_t i_duration = 0;
	mtime_t i_now = mdate();

        i_duration = 1000 * mediacontrol_unit_convert( self->p_playlist->p_input,
                                                       end->key,
                                                       mediacontrol_MediaTime,
                                                       end->value );

        mediacontrol_showtext( p_vout, DEFAULT_CHAN, psz_message, NULL,
                               OSD_ALIGN_BOTTOM | OSD_ALIGN_LEFT, 0, 0,
                               i_now, i_now + i_duration );
    }
    else
    {
        mtime_t i_debut, i_fin, i_now;

        p_input = self->p_playlist->p_input;
        if( ! p_input )
        {
            RAISE( mediacontrol_InternalException, "No input" );
            vlc_object_release( p_vout );
            return;
        }

        /* FIXME */
        /* i_now = input_ClockGetTS( p_input, NULL, 0 ); */
        i_now = mdate();
        
        i_debut = mediacontrol_position2microsecond( p_input,
                                                     ( mediacontrol_Position* ) begin );
        i_debut += i_now;

        i_fin = mediacontrol_position2microsecond( p_input,
                                                   ( mediacontrol_Position * ) end );
        i_fin += i_now;

        vout_ShowTextAbsolute( p_vout, DEFAULT_CHAN, psz_message, NULL,
                               OSD_ALIGN_BOTTOM | OSD_ALIGN_LEFT, 0, 0,
                               i_debut, i_fin );
    }

    vlc_object_release( p_vout );
}

unsigned short
mediacontrol_sound_get_volume( mediacontrol_Instance *self,
                               mediacontrol_Exception *exception )
{
    short retval;
    audio_volume_t i_volume;

    if( !self->p_intf )
    {
        RAISE( mediacontrol_InternalException, "No interface module" );
        return 0;
    }
    aout_VolumeGet( self->p_intf, &i_volume );
    retval = i_volume;
    return retval;
}

void
mediacontrol_sound_set_volume( mediacontrol_Instance *self,
                               const unsigned short volume,
                               mediacontrol_Exception *exception )
{
    if( !self->p_intf )
    {
        RAISE( mediacontrol_InternalException, "No interface module" );
        return;
    }
    aout_VolumeSet( self->p_intf,( audio_volume_t )volume );
}

vlc_bool_t mediacontrol_set_visual( mediacontrol_Instance *self,
		 	            WINDOWHANDLE visual_id,
			            mediacontrol_Exception *exception )
{
    vlc_value_t value;
    int ret;

    if( !self->p_vlc )
    {
        RAISE( mediacontrol_InternalException, "No vlc reference" );
        return 0;
    }
    value.i_int=visual_id;
    ret = var_Set(self->p_vlc, "drawable", value);
    
    return (ret == VLC_SUCCESS);
}

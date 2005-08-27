#include "mediacontrol-core.h"

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
#include <sys/types.h>

#define RAISE( c, m )  exception->code = c; \
                       exception->message = strdup(m);

long long mediacontrol_unit_convert( input_thread_t *p_input,
                                     mediacontrol_PositionKey from,
                                     mediacontrol_PositionKey to,
                                     long long value )
{
    if( to == from )
        return value;

    /* For all conversions, we need data from p_input */
    if( !p_input )
        return 0;

    switch( from )
    {
    case mediacontrol_MediaTime:
        if( to == mediacontrol_ByteCount )
        {
            /* FIXME */
            /* vlc < 0.8 API */
            /* return value * 50 * p_input->stream.i_mux_rate / 1000; */
            return 0;
        }
        if( to == mediacontrol_SampleCount )
        {
            double f_fps;

            if( demux2_Control( p_input->input.p_demux, DEMUX_GET_FPS, &f_fps ) || f_fps < 0.1 )
                return 0;
            else
                return( value * f_fps / 1000.0 );
        }
        /* Cannot happen */
        /* See http://catb.org/~esr/jargon/html/entry/can't-happen.html */
        break;

    case mediacontrol_SampleCount:
    {
        double f_fps;

	if( demux2_Control( p_input->input.p_demux, DEMUX_GET_FPS, &f_fps ) || f_fps < 0.1 )
            return 0;

        if( to == mediacontrol_ByteCount )
        {
            /* FIXME */
            /* vlc < 0.8 API */
/*             return ( long long )( value * 50 * p_input->stream.i_mux_rate / f_fps ); */
            return 0;
        }

        if( to == mediacontrol_MediaTime )
            return( long long )( value * 1000.0 / ( double )f_fps );

        /* Cannot happen */
        break;
    }
    case mediacontrol_ByteCount:
        /* FIXME */
        return 0;
/* vlc < 0.8 API: */

//         if( p_input->stream.i_mux_rate == 0 )
//             return 0;
// 
//         /* Convert an offset into milliseconds. Taken from input_ext-intf.c.
//            The 50 hardcoded constant comes from the definition of i_mux_rate :
//            i_mux_rate : the rate we read the stream (in units of 50 bytes/s) ;
//            0 if undef */
//         if( to == mediacontrol_MediaTime )
//             return ( long long )( 1000 * value / 50 / p_input->stream.i_mux_rate );
// 
//         if( to == mediacontrol_SampleCount )
//         {
//             double f_fps;
//             if( demux2_Control( p_input->input.p_demux, DEMUX_GET_FPS, &f_fps ) || f_fps < 0.1 )
//                 return 0;
//             else
//                 return ( long long )( value * f_fps / 50 / p_input->stream.i_mux_rate );
//         }
        /* Cannot happen */
        break;
    }
    /* Cannot happen */
    return 0;
}

/* Converts a mediacontrol_Position into a time in microseconds in
   movie clock time */
long long
mediacontrol_position2microsecond( input_thread_t* p_input, const mediacontrol_Position * pos )
{
    switch( pos->origin )
    {
    case mediacontrol_AbsolutePosition:
        return ( 1000 * mediacontrol_unit_convert( p_input,
                                                   pos->key, /* from */
                                                   mediacontrol_MediaTime,  /* to */
                                                   pos->value ) );
        break;
    case mediacontrol_RelativePosition:
    {
        long long l_pos;
        vlc_value_t val;

        val.i_time = 0;
        if( p_input )
        {
            var_Get( p_input, "time", &val );
        }

        l_pos = 1000 * mediacontrol_unit_convert( p_input,
                                                  pos->key,
                                                  mediacontrol_MediaTime,
                                                  pos->value );
        return val.i_time + l_pos;
        break;
    }
    case mediacontrol_ModuloPosition:
    {
        long long l_pos;
        vlc_value_t val;

        val.i_time = 0;
        if( p_input )
        {
            var_Get( p_input, "length", &val );
        }

        if( val.i_time > 0)
        {
            l_pos = ( 1000 * mediacontrol_unit_convert( p_input,
                                                        pos->key,
                                                        mediacontrol_MediaTime,
                                                        pos->value ) );
        }
        else
            l_pos = 0;

        return l_pos % val.i_time;
        break;
    }
    }
    return 0;
}

mediacontrol_RGBPicture*
mediacontrol_RGBPicture__alloc( int datasize )
{
    mediacontrol_RGBPicture* pic;

    pic = ( mediacontrol_RGBPicture * )malloc( sizeof( mediacontrol_RGBPicture ) );
    if( ! pic )
        return NULL;

    pic->size = datasize;
    pic->data = ( char* )malloc( datasize );
    return pic;
}

void
mediacontrol_RGBPicture__free( mediacontrol_RGBPicture* pic )
{
    if( pic )
        free( pic->data );
    free( pic );
}

mediacontrol_PlaylistSeq*
mediacontrol_PlaylistSeq__alloc( int size )
{
    mediacontrol_PlaylistSeq* ps;

    ps =( mediacontrol_PlaylistSeq* )malloc( sizeof( mediacontrol_PlaylistSeq ) );
    if( ! ps )
        return NULL;

    ps->size = size;
    ps->data = ( char** )malloc( size * sizeof( char* ) );
    return ps;
}

void
mediacontrol_PlaylistSeq__free( mediacontrol_PlaylistSeq* ps )
{
    if( ps )
    {
        int i;
        for( i = 0 ; i < ps->size ; i++ )
            free( ps->data[i] );
    }
    free( ps->data );
    free( ps );
}

mediacontrol_Exception*
mediacontrol_exception_init( mediacontrol_Exception *exception )
{
    if( exception == NULL )
    {
        exception = ( mediacontrol_Exception* )malloc( sizeof( mediacontrol_Exception ) );
    }

    exception->code = 0;
    exception->message = NULL;
    return exception;
}

void
mediacontrol_exception_free( mediacontrol_Exception *exception )
{
    if( ! exception )
        return;

    free( exception->message );
    free( exception );
}

mediacontrol_Instance* mediacontrol_new_from_object( vlc_object_t* p_object,
                                                     mediacontrol_Exception *exception )
{
    mediacontrol_Instance* retval;
    vlc_object_t *p_vlc;

    p_vlc = vlc_object_find( p_object, VLC_OBJECT_ROOT, FIND_PARENT );
    if( ! p_vlc )
    {
        RAISE( mediacontrol_InternalException, "Unable to initialize VLC" );
        return NULL;
    }
    retval = ( mediacontrol_Instance* )malloc( sizeof( mediacontrol_Instance ) );
    retval->p_vlc = p_vlc;
    retval->vlc_object_id = p_vlc->i_object_id;

    /* We can keep references on these, which should not change. Is it true ? */
    retval->p_playlist = vlc_object_find( p_vlc, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
    retval->p_intf = vlc_object_find( p_vlc, VLC_OBJECT_INTF, FIND_ANYWHERE );

    if( ! retval->p_playlist || ! retval->p_intf )
    {
        RAISE( mediacontrol_InternalException, "No available interface" );
        return NULL;
    }
    return retval;
};

/* Returns the current position in the stream. The returned value can
   be relative or absolute( according to PositionOrigin ) and the unit
   is set by PositionKey */
mediacontrol_Position*
mediacontrol_get_media_position( mediacontrol_Instance *self,
                                 const mediacontrol_PositionOrigin an_origin,
                                 const mediacontrol_PositionKey a_key,
                                 mediacontrol_Exception *exception )
{
    mediacontrol_Position* retval;
    vlc_value_t val;
    input_thread_t * p_input = self->p_playlist->p_input;

    exception = mediacontrol_exception_init( exception );

    retval = ( mediacontrol_Position* )malloc( sizeof( mediacontrol_Position ) );
    retval->origin = an_origin;
    retval->key = a_key;

    if( ! p_input )
    {
        /*
           RAISE( mediacontrol_InternalException, "No input thread." );
           return( NULL );
        */
        retval->value = 0;
        return retval;
    }

    if(  an_origin == mediacontrol_RelativePosition
         || an_origin == mediacontrol_ModuloPosition )
    {
        /* Relative or ModuloPosition make no sense */
        retval->value = 0;
        return retval;
    }

    /* We are asked for an AbsolutePosition. */
    val.i_time = 0;
    var_Get( p_input, "time", &val );
    /* FIXME: check val.i_time > 0 */

    retval->value = mediacontrol_unit_convert( p_input,
                                               mediacontrol_MediaTime,
                                               a_key,
                                               val.i_time / 1000 );
    return retval;
}

/* Sets the media position */
void
mediacontrol_set_media_position( mediacontrol_Instance *self,
                                 const mediacontrol_Position * a_position,
                                 mediacontrol_Exception *exception )
{
    vlc_value_t val;
    input_thread_t * p_input = self->p_playlist->p_input;

    exception=mediacontrol_exception_init( exception );
    if( ! p_input )
    {
        RAISE( mediacontrol_InternalException, "No input thread." );
        return;
    }

    if( !var_GetBool( p_input, "seekable" ) )
    {
        RAISE( mediacontrol_InvalidPosition, "Stream not seekable" );
        return;
    }

    val.i_time = mediacontrol_position2microsecond( p_input, a_position );
    var_Set( p_input, "time", val );
    return;
}

/* Starts playing a stream */
void
mediacontrol_start( mediacontrol_Instance *self,
                    const mediacontrol_Position * a_position,
                    mediacontrol_Exception *exception )
{
    playlist_t * p_playlist = self->p_playlist;

    exception = mediacontrol_exception_init( exception );
    if( ! p_playlist )
    {
        RAISE( mediacontrol_PlaylistException, "No available playlist" );
        return;
    }

    vlc_mutex_lock( &p_playlist->object_lock );
    if( p_playlist->i_size )
    {
        vlc_value_t val;

        vlc_mutex_unlock( &p_playlist->object_lock );

        /* Set start time */
        val.i_int = mediacontrol_position2microsecond( p_playlist->p_input, a_position ) / 1000000;
        var_Set( p_playlist, "start-time", val );

        playlist_Play( p_playlist );
    }
    else
    {
        RAISE( mediacontrol_PlaylistException, "Empty playlist." );
        vlc_mutex_unlock( &p_playlist->object_lock );
        return;
    }

    return;
}

void
mediacontrol_pause( mediacontrol_Instance *self,
                    const mediacontrol_Position * a_position,
                    mediacontrol_Exception *exception )
{
    input_thread_t *p_input = self->p_playlist->p_input;;

    /* FIXME: use the a_position parameter */
    exception=mediacontrol_exception_init( exception );
    if( p_input != NULL )
    {
        var_SetInteger( p_input, "state", PAUSE_S );
    }
    else
    {
        RAISE( mediacontrol_InternalException, "No input" );
    }

    return;
}

void
mediacontrol_resume( mediacontrol_Instance *self,
                     const mediacontrol_Position * a_position,
                     mediacontrol_Exception *exception )
{
    input_thread_t *p_input = self->p_playlist->p_input;

    /* FIXME: use the a_position parameter */
    exception=mediacontrol_exception_init( exception );
    if( p_input != NULL )
    {
        var_SetInteger( p_input, "state", PAUSE_S );
    }
    else
    {
        RAISE( mediacontrol_InternalException, "No input" );
    }
}

void
mediacontrol_stop( mediacontrol_Instance *self,
                   const mediacontrol_Position * a_position,
                   mediacontrol_Exception *exception )
{
    /* FIXME: use the a_position parameter */
    exception=mediacontrol_exception_init( exception );
    if( !self->p_playlist )
    {
        RAISE( mediacontrol_PlaylistException, "No playlist" );
        return;
    }

    playlist_Stop( self->p_playlist );
}

void
mediacontrol_playlist_add_item( mediacontrol_Instance *self,
                                const char * psz_file,
                                mediacontrol_Exception *exception )
{
    exception=mediacontrol_exception_init( exception );
    if( !self->p_playlist )
    {
        RAISE( mediacontrol_InternalException, "No playlist" );
        return;
    }

    playlist_Add( self->p_playlist, psz_file, psz_file , PLAYLIST_REPLACE, 0 );
}

void
mediacontrol_playlist_clear( mediacontrol_Instance *self,
                             mediacontrol_Exception *exception )
{
    exception=mediacontrol_exception_init( exception );
    if( !self->p_playlist )
    {
        RAISE( mediacontrol_PlaylistException, "No playlist" );
        return;
    }

    playlist_Clear( self->p_playlist );

    return;
}

mediacontrol_PlaylistSeq *
mediacontrol_playlist_get_list( mediacontrol_Instance *self,
                                mediacontrol_Exception *exception )
{
    mediacontrol_PlaylistSeq *retval;
    int i_index;
    playlist_t * p_playlist = self->p_playlist;;
    int i_playlist_size;

    exception=mediacontrol_exception_init( exception );
    if( !p_playlist )
    {
        RAISE( mediacontrol_PlaylistException, "No playlist" );
        return NULL;
    }

    vlc_mutex_lock( &p_playlist->object_lock );
    i_playlist_size = p_playlist->i_size;

    retval = mediacontrol_PlaylistSeq__alloc( i_playlist_size );

    for( i_index = 0 ; i_index < i_playlist_size ; i_index++ )
    {
        retval->data[i_index] = strdup( p_playlist->pp_items[i_index]->input.psz_uri );
    }
    vlc_mutex_unlock( &p_playlist->object_lock );

    return retval;
}

mediacontrol_RGBPicture*
_mediacontrol_createRGBPicture( int i_width, int i_height, long i_chroma, long long l_date,
                                char* p_data, int i_datasize )
{
    mediacontrol_RGBPicture *retval;

    retval = mediacontrol_RGBPicture__alloc( i_datasize );
    if( retval )
    {
        retval->width  = i_width;
        retval->height = i_height;
        retval->type   = i_chroma;
        retval->date   = l_date;
        retval->size   = i_datasize;
        memcpy( retval->data, p_data, i_datasize );
    }
    return retval;
}

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

void
mediacontrol_display_text( mediacontrol_Instance *self,
                           const char * message,
                           const mediacontrol_Position * begin,
                           const mediacontrol_Position * end,
                           mediacontrol_Exception *exception )
{
    input_thread_t *p_input = NULL;
    vout_thread_t *p_vout = NULL;

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

        i_duration = 1000 * mediacontrol_unit_convert( self->p_playlist->p_input,
                                                       end->key,
                                                       mediacontrol_MediaTime,
                                                       end->value );

        vout_ShowTextRelative( p_vout, DEFAULT_CHAN, ( char* ) message, NULL,
                               OSD_ALIGN_BOTTOM | OSD_ALIGN_LEFT, 20, 20,
                               i_duration );
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
        i_now = 0;
        
        i_debut = mediacontrol_position2microsecond( p_input,
                                                     ( mediacontrol_Position* ) begin );
        i_debut += i_now;

        i_fin = mediacontrol_position2microsecond( p_input,
                                                   ( mediacontrol_Position * ) end );
        i_fin += i_now;

        vout_ShowTextAbsolute( p_vout, DEFAULT_CHAN, ( char* ) message, NULL,
                               OSD_ALIGN_BOTTOM | OSD_ALIGN_LEFT, 20, 20,
                               i_debut, i_fin );
    }

    vlc_object_release( p_vout );
}

mediacontrol_StreamInformation *
mediacontrol_get_stream_information( mediacontrol_Instance *self,
                                     mediacontrol_PositionKey a_key,
                                     mediacontrol_Exception *exception )
{
    mediacontrol_StreamInformation *retval;
    input_thread_t *p_input = self->p_playlist->p_input;
    vlc_value_t val;

    retval = ( mediacontrol_StreamInformation* )malloc( sizeof( mediacontrol_StreamInformation ) );
    if( ! retval )
    {
        RAISE( mediacontrol_InternalException, "Out of memory" );
        return NULL;
    }

    if( ! p_input )
    {
        /* No p_input defined */
        retval->streamstatus = mediacontrol_UndefinedStatus;
        retval->url          = strdup( "None" );
        retval->position     = 0;
        retval->length       = 0;
    }
    else
    {
        switch( var_GetInteger( p_input, "state" ) )
        {
        case PLAYING_S     :
            retval->streamstatus = mediacontrol_PlayingStatus;
            break;
        case PAUSE_S       :
            retval->streamstatus = mediacontrol_PauseStatus;
            break;
        case INIT_S        :
            retval->streamstatus = mediacontrol_InitStatus;
            break;
        case END_S         :
            retval->streamstatus = mediacontrol_EndStatus;
            break;
        default :
            retval->streamstatus = mediacontrol_UndefinedStatus;
            break;
        }

        retval->url = strdup( p_input->input.p_item->psz_uri );

        /* TIME and LENGTH are in microseconds. We want them in ms */
        var_Get( p_input, "time", &val);
        retval->position = val.i_time / 1000;

        var_Get( p_input, "length", &val);
        retval->length = val.i_time / 1000;

        retval->position = mediacontrol_unit_convert( p_input,
                                                      mediacontrol_MediaTime, a_key,
                                                      retval->position );
        retval->length   = mediacontrol_unit_convert( p_input,
                                                      mediacontrol_MediaTime, a_key,
                                                      retval->length );
    }
    return retval;
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

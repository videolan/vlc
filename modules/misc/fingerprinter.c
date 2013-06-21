/*****************************************************************************
 * fingerprinter.c: Audio fingerprinter module
 *****************************************************************************
 * Copyright (C) 2012 VLC authors and VideoLAN
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
#include <vlc_plugin.h>
#include <vlc_stream.h>
#include <vlc_modules.h>
#include <vlc_meta.h>
#include <vlc_url.h>

#include <vlc/vlc.h>
#include <vlc_input.h>
#include <vlc_fingerprinter.h>
#include <webservices/acoustid.h>
#include <../stream_out/chromaprint_data.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

struct fingerprinter_sys_t
{
    vlc_thread_t thread;

    struct
    {
        vlc_array_t         *queue;
        vlc_mutex_t         lock;
    } incoming, processing, results;

    vlc_cond_t              incoming_queue_filled;

    struct
    {
        vlc_mutex_t         lock;
        vlc_cond_t          wait;
        int                 i_input_state;
    } condwait;

    /* tracked in sys for cancelability */
    input_item_t            *p_item;
    input_thread_t          *p_input;
    chromaprint_fingerprint_t chroma_fingerprint;
    char                    *psz_uri;

    /* clobberable by cleanups */
    int                     i_cancel_state;
    int                     i;
};

static int  Open            (vlc_object_t *);
static void Close           (vlc_object_t *);
static void Run             (fingerprinter_thread_t *);

/*****************************************************************************
 * Module descriptor
 ****************************************************************************/
vlc_module_begin ()
    set_category(CAT_ADVANCED)
    set_subcategory(SUBCAT_ADVANCED_MISC)
    set_shortname(N_("acoustid"))
    set_description(N_("Track fingerprinter (based on Acoustid)"))
    set_capability("fingerprinter", 10)
    set_callbacks(Open, Close)
vlc_module_end ()

/*****************************************************************************
 * Requests lifecycle
 *****************************************************************************/

static void EnqueueRequest( fingerprinter_thread_t *f, fingerprint_request_t *r )
{
    fingerprinter_sys_t *p_sys = f->p_sys;
    vlc_mutex_lock( &p_sys->incoming.lock );
    vlc_array_append( p_sys->incoming.queue, r );
    vlc_mutex_unlock( &p_sys->incoming.lock );
    vlc_cond_signal( &p_sys->incoming_queue_filled );
}

static void QueueIncomingRequests( fingerprinter_sys_t *p_sys )
{
    vlc_mutex_lock( &p_sys->incoming.lock );
    int i = vlc_array_count( p_sys->incoming.queue );
    if ( i == 0 ) goto end;
    vlc_mutex_lock( &p_sys->processing.lock );
    while( i )
        vlc_array_append( p_sys->processing.queue,
                          vlc_array_item_at_index( p_sys->incoming.queue, --i ) );
    vlc_array_clear( p_sys->incoming.queue );
    vlc_mutex_unlock( &p_sys->processing.lock );
end:
    vlc_mutex_unlock(&p_sys->incoming.lock);
}

static fingerprint_request_t * GetResult( fingerprinter_thread_t *f )
{
    fingerprint_request_t *r = NULL;
    fingerprinter_sys_t *p_sys = f->p_sys;
    vlc_mutex_lock( &p_sys->results.lock );
    if ( vlc_array_count( p_sys->results.queue ) )
    {
        r = vlc_array_item_at_index( p_sys->results.queue, 0 );
        vlc_array_remove( p_sys->results.queue, 0 );
    }
    vlc_mutex_unlock( &p_sys->results.lock );
    return r;
}

static void ApplyResult( fingerprint_request_t *p_r, int i_resultid )
{
    if ( i_resultid >= vlc_array_count( & p_r->results.metas_array ) ) return;

    vlc_meta_t *p_meta = (vlc_meta_t *)
            vlc_array_item_at_index( & p_r->results.metas_array, i_resultid );
    input_item_t *p_item = p_r->p_item;
    vlc_mutex_lock( &p_item->lock );
    vlc_meta_Merge( p_item->p_meta, p_meta );
    vlc_mutex_unlock( &p_item->lock );
}

static void cancelDoFingerprint( void *p_arg )
{
    fingerprinter_sys_t *p_sys = ( fingerprinter_sys_t * ) p_arg;
    if ( p_sys->p_input )
    {
        input_Stop( p_sys->p_input, true );
        input_Close( p_sys->p_input );
    }
    /* cleanup temporary result */
    if ( p_sys->chroma_fingerprint.psz_fingerprint )
        FREENULL( p_sys->chroma_fingerprint.psz_fingerprint );
    if ( p_sys->p_item )
        input_item_Release( p_sys->p_item );
}

static int inputStateCallback( vlc_object_t *obj, const char *var,
                               vlc_value_t old, vlc_value_t cur, void *p_data )
{
    VLC_UNUSED(obj);VLC_UNUSED(var);VLC_UNUSED(old);
    fingerprinter_sys_t *p_sys = (fingerprinter_sys_t *) p_data;
    if ( cur.i_int != INPUT_EVENT_STATE ) return VLC_SUCCESS;
    p_sys->condwait.i_input_state = var_GetInteger( p_sys->p_input, "state" );
    vlc_cond_signal( & p_sys->condwait.wait );
    return VLC_SUCCESS;
}

static void DoFingerprint( vlc_object_t *p_this, fingerprinter_sys_t *p_sys, acoustid_fingerprint_t *fp )
{
    p_sys->p_input = NULL;
    p_sys->p_item = NULL;
    p_sys->chroma_fingerprint.psz_fingerprint = NULL;
    vlc_cleanup_push( cancelDoFingerprint, p_sys );

    p_sys->p_item = input_item_New( NULL, NULL );
    if ( ! p_sys->p_item ) goto end;

    char *psz_sout_option;
    /* Note: need at -max- 2 channels, but we can't guess it before playing */
    /* the stereo upmix could make the mono tracks fingerprint to differ :/ */
    if ( asprintf( &psz_sout_option,
                   "sout=#transcode{acodec=%s,channels=2}:chromaprint",
                   ( VLC_CODEC_S16L == VLC_CODEC_S16N ) ? "s16l" : "s16b" )
         == -1 ) goto end;
    input_item_AddOption( p_sys->p_item, psz_sout_option, VLC_INPUT_OPTION_TRUSTED );
    free( psz_sout_option );
    input_item_AddOption( p_sys->p_item, "vout=dummy", VLC_INPUT_OPTION_TRUSTED );
    input_item_AddOption( p_sys->p_item, "aout=dummy", VLC_INPUT_OPTION_TRUSTED );
    if ( fp->i_duration )
    {
        if ( asprintf( &psz_sout_option, "stop-time=%u", fp->i_duration ) == -1 ) goto end;
        input_item_AddOption( p_sys->p_item, psz_sout_option, VLC_INPUT_OPTION_TRUSTED );
        free( psz_sout_option );
    }
    input_item_SetURI( p_sys->p_item, p_sys->psz_uri ) ;

    p_sys->p_input = input_Create( p_this, p_sys->p_item, "fingerprinter", NULL );
    if ( p_sys->p_input )
    {
        p_sys->chroma_fingerprint.i_duration = fp->i_duration;
        var_Create( p_sys->p_input, "fingerprint-data", VLC_VAR_ADDRESS );
        var_SetAddress( p_sys->p_input, "fingerprint-data", & p_sys->chroma_fingerprint );

        input_Start( p_sys->p_input );

        /* Wait for input to start && end */
        p_sys->condwait.i_input_state = var_GetInteger( p_sys->p_input, "state" );

        if ( likely( var_AddCallback( p_sys->p_input, "intf-event",
                            inputStateCallback, p_sys ) == VLC_SUCCESS ) )
        {
            while( p_sys->condwait.i_input_state <= PAUSE_S )
            {
                vlc_mutex_lock( &p_sys->condwait.lock );
                mutex_cleanup_push( &p_sys->condwait.lock );
                vlc_cond_wait( &p_sys->condwait.wait, &p_sys->condwait.lock );
                vlc_cleanup_run();
            }
            var_DelCallback( p_sys->p_input, "intf-event", inputStateCallback, p_sys );
        }
        input_Stop( p_sys->p_input, true );
        input_Close( p_sys->p_input );
        p_sys->p_input = NULL;

        if ( p_sys->chroma_fingerprint.psz_fingerprint )
        {
            fp->psz_fingerprint = strdup( p_sys->chroma_fingerprint.psz_fingerprint );
            if ( ! fp->i_duration ) /* had not given hint */
                fp->i_duration = p_sys->chroma_fingerprint.i_duration;
        }
    }
end:
    vlc_cleanup_run( );
}

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open(vlc_object_t *p_this)
{
    fingerprinter_thread_t *p_fingerprinter = (fingerprinter_thread_t*) p_this;
    fingerprinter_sys_t *p_sys = calloc(1, sizeof(fingerprinter_sys_t));

    if ( !p_sys )
        return VLC_ENOMEM;

    p_fingerprinter->p_sys = p_sys;

    p_sys->incoming.queue = vlc_array_new();
    vlc_mutex_init( &p_sys->incoming.lock );
    vlc_cond_init( &p_sys->incoming_queue_filled );

    p_sys->processing.queue = vlc_array_new();
    vlc_mutex_init( &p_sys->processing.lock );

    p_sys->results.queue = vlc_array_new();
    vlc_mutex_init( &p_sys->results.lock );

    vlc_mutex_init( &p_sys->condwait.lock );
    vlc_cond_init( &p_sys->condwait.wait );

    p_sys->psz_uri = NULL;

    p_fingerprinter->pf_run = Run;
    p_fingerprinter->pf_enqueue = EnqueueRequest;
    p_fingerprinter->pf_getresults = GetResult;
    p_fingerprinter->pf_apply = ApplyResult;

    var_Create( p_fingerprinter, "results-available", VLC_VAR_BOOL );
    if( p_fingerprinter->pf_run
     && vlc_clone( &p_sys->thread,
                   (void *(*) (void *)) p_fingerprinter->pf_run,
                   p_fingerprinter, VLC_THREAD_PRIORITY_LOW ) )
    {
        msg_Err( p_fingerprinter, "cannot spawn fingerprinter thread" );
        goto error;
    }

    return VLC_SUCCESS;

error:
    free( p_sys );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close(vlc_object_t *p_this)
{
    fingerprinter_thread_t   *p_fingerprinter = (fingerprinter_thread_t*) p_this;
    fingerprinter_sys_t *p_sys = p_fingerprinter->p_sys;

    vlc_cancel( p_sys->thread );
    vlc_join( p_sys->thread, NULL );

    vlc_mutex_destroy( &p_sys->condwait.lock );
    vlc_cond_destroy( &p_sys->condwait.wait );

    for ( int i = 0; i < vlc_array_count( p_sys->incoming.queue ); i++ )
        fingerprint_request_Delete( vlc_array_item_at_index( p_sys->incoming.queue, i ) );
    vlc_array_destroy( p_sys->incoming.queue );
    vlc_mutex_destroy( &p_sys->incoming.lock );
    vlc_cond_destroy( &p_sys->incoming_queue_filled );

    for ( int i = 0; i < vlc_array_count( p_sys->processing.queue ); i++ )
        fingerprint_request_Delete( vlc_array_item_at_index( p_sys->processing.queue, i ) );
    vlc_array_destroy( p_sys->processing.queue );
    vlc_mutex_destroy( &p_sys->processing.lock );

    for ( int i = 0; i < vlc_array_count( p_sys->results.queue ); i++ )
        fingerprint_request_Delete( vlc_array_item_at_index( p_sys->results.queue, i ) );
    vlc_array_destroy( p_sys->results.queue );
    vlc_mutex_destroy( &p_sys->results.lock );

    free( p_sys );
}

static void fill_metas_with_results( fingerprint_request_t *p_r, acoustid_fingerprint_t *p_f )
{
    for( unsigned int i=0 ; i < p_f->results.count; i++ )
    {
        acoustid_result_t *p_result = & p_f->results.p_results[ i ];
        for ( unsigned int j=0 ; j < p_result->recordings.count; j++ )
        {
            musicbrainz_recording_t *p_record = & p_result->recordings.p_recordings[ j ];
            vlc_meta_t *p_meta = vlc_meta_New();
            if ( p_meta )
            {
                vlc_meta_Set( p_meta, vlc_meta_Title, p_record->psz_title );
                vlc_meta_Set( p_meta, vlc_meta_Artist, p_record->psz_artist );
                vlc_meta_AddExtra( p_meta, "musicbrainz-id", p_record->sz_musicbrainz_id );
                vlc_array_append( & p_r->results.metas_array, p_meta );
            }
        }
    }
}

/*****************************************************************************
 * Run :
 *****************************************************************************/
static void cancelRun( void * p_arg )
{
    fingerprinter_sys_t *p_sys = ( fingerprinter_sys_t * ) p_arg;
    if ( vlc_array_count( p_sys->processing.queue ) )
        vlc_array_clear( p_sys->processing.queue );
    if ( p_sys->psz_uri )
        free( p_sys->psz_uri );
}

static void clearPrint( void * p_arg )
{
    acoustid_fingerprint_t *acoustid_print = ( acoustid_fingerprint_t * ) p_arg;
    for( unsigned int j=0 ; j < acoustid_print->results.count; j++ )
        free_acoustid_result_t( &acoustid_print->results.p_results[j] );
    if ( acoustid_print->results.count )
        free( acoustid_print->results.p_results );
    if ( acoustid_print->psz_fingerprint )
        free( acoustid_print->psz_fingerprint );
}

static void Run( fingerprinter_thread_t *p_fingerprinter )
{
    fingerprinter_sys_t *p_sys = p_fingerprinter->p_sys;

    /* main loop */
    for (;;)
    {
        vlc_mutex_lock( &p_sys->processing.lock );
        mutex_cleanup_push( &p_sys->processing.lock );
        vlc_cond_timedwait( &p_sys->incoming_queue_filled, &p_sys->processing.lock, mdate() + 1000000 );
        vlc_cleanup_run();

        QueueIncomingRequests( p_sys );

        vlc_mutex_lock( &p_sys->processing.lock ); // L0
        mutex_cleanup_push( &p_sys->processing.lock );
        vlc_cleanup_push( cancelRun, p_sys ); // C1
//**
        for ( p_sys->i = 0 ; p_sys->i < vlc_array_count( p_sys->processing.queue ); p_sys->i++ )
        {
            fingerprint_request_t *p_data = vlc_array_item_at_index( p_sys->processing.queue, p_sys->i );
            acoustid_fingerprint_t acoustid_print;
            memset( &acoustid_print , 0, sizeof(acoustid_fingerprint_t) );
            vlc_cleanup_push( clearPrint, &acoustid_print ); // C2
            p_sys->psz_uri = input_item_GetURI( p_data->p_item );
            if ( p_sys->psz_uri )
            {
                /* overwrite with hint, as in this case, fingerprint's session will be truncated */
                if ( p_data->i_duration ) acoustid_print.i_duration = p_data->i_duration;

                DoFingerprint( VLC_OBJECT(p_fingerprinter), p_sys, &acoustid_print );

                DoAcoustIdWebRequest( VLC_OBJECT(p_fingerprinter), &acoustid_print );
                fill_metas_with_results( p_data, &acoustid_print );
                FREENULL( p_sys->psz_uri );
            }
            vlc_cleanup_run( ); // C2

            /* copy results */
            vlc_mutex_lock( &p_sys->results.lock );
            vlc_array_append( p_sys->results.queue, p_data );
            vlc_mutex_unlock( &p_sys->results.lock );

            vlc_testcancel();
        }

        if ( vlc_array_count( p_sys->processing.queue ) )
        {
            var_TriggerCallback( p_fingerprinter, "results-available" );
            vlc_array_clear( p_sys->processing.queue );
        }
        vlc_cleanup_pop( ); // C1
//**
        vlc_cleanup_run(); // L0
    }
}

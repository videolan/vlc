/*****************************************************************************
 * audioscrobbler.c : audioscrobbler submission plugin
 *****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id$
 *
 * Authors: Rafaël Carré <funman at videolan org>
 *          Kenneth Ostby <kenneo -at- idi -dot- ntnu -dot- no>
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

#define _GNU_SOURCE
#include <string.h>

#if defined( WIN32 )
#include <time.h>
#endif
/*
 * TODO :
 * implement musicbrainz unique track identifier in p_meta
 * check meta_engine's state, and remove delaying of metadata reading
 * check md5 operations on BIGENDIAN and 64 bits architectures
 */
#include <vlc/vlc.h>
#include <vlc/intf.h>
#include <vlc_meta.h>
#include <vlc_md5.h>
#include <vlc_block.h>
#include <vlc_stream.h>
#include <vlc_url.h>
#include <network.h>
#include <vlc_interaction.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

/* Keeps track of metadata to be submitted, and if song has been submitted */
typedef struct audioscrobbler_song_t
{
    char        *psz_a;                /* track artist     */
    char        *psz_t;                /* track title      */
    char        *psz_b;                /* track album      */
    int         i_l;                   /* track length     */
    char        *psz_m;                /* musicbrainz id   */
    char        *psz_i;                /* date             */
    time_t      time_playing;          /* date (epoch)     */
} audioscrobbler_song_t;


/* Queue to be submitted to server, 10 songs max */
typedef struct audioscrobbler_queue_t
{
    audioscrobbler_song_t   **p_queue;      /* contains up to 10 songs        */
    int                     i_songs_nb;     /* number of songs                */
    void                    *p_next_queue;  /* if queue full, pointer to next */
} audioscrobbler_queue_t;

struct intf_sys_t
{
    audioscrobbler_queue_t  *p_first_queue;     /* 1st queue              */
    vlc_mutex_t             lock;               /* p_sys mutex            */

/* data about audioscrobbler session */
    int                     i_interval;         /* last interval recorded */
    time_t                  time_last_interval; /* when was it recorded ? */
    char                    *psz_submit_host;   /* where to submit data ? */
    int                     i_submit_port;      /* at which port ?        */
    char                    *psz_submit_file;   /* in which file ?        */
    char                    *psz_username;      /* last.fm username       */
    vlc_bool_t              b_handshaked;       /* did we handshake ?     */
    int                     i_post_socket;      /* socket for submission  */
    char                    *psz_response_md5;  /* md5 response to use    */

/* data about input elements */
    audioscrobbler_song_t   *p_current_song;    /* song being played      */
    time_t                  time_pause;         /* time when vlc paused   */
    time_t                  time_total_pauses;  /* sum of time in pause   */
    vlc_bool_t              b_queued;           /* has it been queud ?    */
    vlc_bool_t              b_metadata_read;    /* did we read metadata ? */
    vlc_bool_t              b_paused;           /* are we playing ?       */
    vlc_bool_t              b_waiting_meta;     /* we need fetched data?  */
};

intf_sys_t *p_sys_global;     /* to retrieve p_sys in Run() thread */

static int  Open    ( vlc_object_t * );
static void Close   ( vlc_object_t * );
static void Run     ( intf_thread_t * );
static int ItemChange   ( vlc_object_t *, const char *,
                        vlc_value_t, vlc_value_t, void * );
static int PlayingChange( vlc_object_t *, const char *,
                        vlc_value_t, vlc_value_t, void * );
static int AddToQueue   ( intf_thread_t *p_this );
static int Handshake    ( intf_thread_t *p_sd );
static int ReadMetaData ( intf_thread_t *p_this );
static int ReadLocalMetaData( intf_thread_t *p_this, input_thread_t  *p_input );
void DeleteQueue( audioscrobbler_queue_t *p_queue );

/*****************************************************************************
 * Module descriptor
 ****************************************************************************/


#define APPLICATION_NAME "VLC media player"
#define USERNAME_TEXT N_("Username")
#define USERNAME_LONGTEXT N_("The username of your last.fm account")
#define PASSWORD_TEXT N_("Password")
#define PASSWORD_LONGTEXT N_("The password of your last.fm account")

/* if something goes wrong, we wait at least one minute before trying again */
#define DEFAULT_INTERVAL 60
/* last.fm client identifier */
#define CLIENT_NAME     PACKAGE
#define CLIENT_VERSION  VERSION

/* HTTP POST request : to submit data */
#define    POST_REQUEST "POST /%s HTTP/1.1\n"                               \
                        "Accept-Encoding: identity\n"                       \
                        "Content-length: %d\n"                              \
                        "Connection: close\n"                               \
                        "Content-type: application/x-www-form-urlencoded\n" \
                        "Host: %s\n"                                        \
                        "User-agent: VLC Media Player/%s\r\n"               \
                        "\r\n"                                              \
                        "%s\r\n"                                            \
                        "\r\n"

/* data to submit */
#define POST_DATA "u=%s&s=%s&a%%5B%d%%5D=%s&t%%5B%d%%5D=%s" \
                  "&b%%5B%d%%5D=%s&m%%5B%d%%5D=%s&l%%5B%d%%5D=%d&i%%5B%d%%5D=%s"

vlc_module_begin();
    set_category( CAT_INTERFACE );
    set_subcategory( SUBCAT_INTERFACE_CONTROL );
    set_shortname( N_( "Audioscrobbler" ) );
    set_description( N_("Audioscrobbler submission Plugin") );
    add_string( "lastfm-username", "", NULL,
                USERNAME_TEXT, USERNAME_LONGTEXT, VLC_FALSE );
    add_string( "lastfm-password", "", NULL,
                PASSWORD_TEXT, PASSWORD_LONGTEXT, VLC_FALSE );
    set_capability( "interface", 0 );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Open: initialize and create stuff
 *****************************************************************************/

static int Open( vlc_object_t *p_this )
{
    playlist_t          *p_playlist;

    intf_thread_t *p_intf = ( intf_thread_t* ) p_this;
    intf_sys_t *p_sys = malloc( sizeof( intf_sys_t ) );
    if( !p_sys )
    {
      goto error;
    }
    vlc_mutex_init( p_this, &p_sys->lock );

    p_sys_global = p_sys;
    p_sys->psz_submit_host = NULL;
    p_sys->psz_submit_file = NULL;
    p_sys->b_handshaked = VLC_FALSE;
    p_sys->i_interval = 0;
    p_sys->time_last_interval = time( NULL );
    p_sys->psz_username = NULL;
    p_sys->b_paused = VLC_FALSE;

    /* md5 response is 32 chars, + final \0 */
    p_sys->psz_response_md5 = malloc( sizeof( char ) * 33 );
    if( !p_sys->psz_response_md5 )
    {
        vlc_mutex_destroy ( &p_sys->lock );
        goto error;
   }

    p_sys->p_first_queue = malloc( sizeof( audioscrobbler_queue_t ) );
    if( !p_sys->p_first_queue )
    {
        vlc_mutex_destroy( &p_sys->lock );
        goto error;
    }

    p_sys->p_current_song = malloc( sizeof( audioscrobbler_song_t ) );
    if( !p_sys->p_current_song )
    {
        vlc_mutex_destroy( &p_sys->lock );
        goto error;
    }

    /* queues can't contain more than 10 songs */
    p_sys->p_first_queue->p_queue =
        malloc( 10 * sizeof( audioscrobbler_song_t ) );
    if( !p_sys->p_current_song )
    {
        vlc_mutex_destroy( &p_sys->lock );
        goto error;
    }

    p_sys->p_first_queue->i_songs_nb = 0;
    p_sys->p_first_queue->p_next_queue = NULL;

    p_playlist = pl_Yield( p_intf );
    var_AddCallback( p_playlist, "playlist-current", ItemChange, p_intf );
    pl_Release( p_playlist );

    p_intf->pf_run = Run;

    return VLC_SUCCESS;

error:
    free( p_sys->p_current_song );
    free( p_sys->p_first_queue );
    free( p_sys->psz_response_md5 );
    free( p_sys );

    return VLC_ENOMEM;
}

/*****************************************************************************
 * Close: destroy interface stuff
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    audioscrobbler_queue_t      *p_current_queue, *p_next_queue;
    playlist_t                  *p_playlist;
    input_thread_t              *p_input;

    intf_thread_t *p_intf = ( intf_thread_t* ) p_this;
    intf_sys_t *p_sys = p_intf->p_sys;

    p_playlist = pl_Yield( p_intf );
    PL_LOCK;
    p_input = p_playlist->p_input;
    var_DelCallback( p_playlist, "playlist-current", ItemChange, p_intf );

    if ( p_input )
    {
        vlc_object_yield( p_input );
        var_DelCallback( p_input, "state", PlayingChange, p_intf );
        vlc_object_release( p_input );
    }

    PL_UNLOCK;
    pl_Release( p_playlist );

    vlc_mutex_lock ( &p_sys->lock );
    p_current_queue = p_sys->p_first_queue;
    vlc_mutex_unlock ( &p_sys->lock );

    while( ( p_current_queue->i_songs_nb == 10 ) &&
        ( p_current_queue->p_next_queue != NULL ) )
    {
        p_next_queue = p_current_queue->p_next_queue;
        DeleteQueue( p_current_queue );
        free( p_current_queue );
        p_current_queue = p_next_queue;
    }

    DeleteQueue( p_current_queue );
    free( p_current_queue );

    vlc_mutex_lock ( &p_sys->lock );
    free( p_sys->psz_username );
    free( p_sys->p_current_song );
    vlc_mutex_unlock ( &p_sys->lock );
    vlc_mutex_destroy( &p_sys->lock );
    free( p_sys );
}

/*****************************************************************************
 * Run : Handshake with audioscrobbler, then submit songs
 *****************************************************************************/
static void Run( intf_thread_t *p_this )
{
    char                    *psz_submit_string = NULL;
    int                     i_net_ret;
    int                     i_song;
    playlist_t              *p_playlist;
    uint8_t                 *p_buffer = NULL;
    char                    *p_buffer_pos = NULL;
    audioscrobbler_queue_t  *p_first_queue;
    /* TODO: remove when meta_engine works */
    time_t                  played_time;

    p_this->p_sys = p_sys_global;
    intf_sys_t *p_sys = p_this->p_sys;

    /* main loop */
    while( !p_this->b_die )
    {
        /* verify if there is data to submit 
         * and if waiting interval is finished */
        if ( ( p_sys->p_first_queue->i_songs_nb > 0 ) &&
            ( time( NULL ) >=
            ( p_sys->time_last_interval + p_sys->i_interval )  ) )
        {
            /* handshake if needed */
            if( p_sys->b_handshaked == VLC_FALSE )
            {
                msg_Dbg( p_this, "Handshaking with last.fm ..." );
 
                switch( Handshake( p_this ) )
                {
                    case VLC_ENOMEM:
                        msg_Err( p_this, "Out of memory" );
                        return;
                        break;

                    case VLC_ENOVAR:
                        /* username not set */
                        vlc_mutex_unlock ( &p_sys->lock );
                        intf_UserFatal( p_this, VLC_FALSE,
                            _("last.fm username not set"),
                            _("You have to set a username,"
                            " and then restart VLC.\n"
                            "Visit https://www.last.fm/join/"
                            " if you don't have one.")
                        );
                        return;
                        break;

                    case VLC_SUCCESS:
                        msg_Dbg( p_this, "Handshake successfull :)" );
                        vlc_mutex_lock ( &p_sys->lock );
                        p_sys->b_handshaked = VLC_TRUE;
                        vlc_mutex_unlock ( &p_sys->lock );
                        break;

                    case VLC_EGENERIC:
                    default:
                        /* VLC_EGENERIC : we'll try later */
                        vlc_mutex_lock ( &p_sys->lock );
                        p_sys->i_interval = DEFAULT_INTERVAL;
                        time( &p_sys->time_last_interval );
                        vlc_mutex_unlock ( &p_sys->lock );
                        break;
                }
            }

            msg_Dbg( p_this, "Going to submit some data..." );
            vlc_mutex_lock ( &p_sys->lock );
            psz_submit_string = malloc( 2048 * sizeof( char ) );

            if (!psz_submit_string)
            {
                msg_Err( p_this, "Out of memory" );
                vlc_mutex_unlock ( &p_sys->lock );
                return;
            }

            /* forge the HTTP POST request */
            for (i_song = 0; i_song < p_sys->p_first_queue->i_songs_nb ;
                i_song++ )
            {
                snprintf( psz_submit_string, 2048, POST_DATA,
                    p_sys->psz_username, p_sys->psz_response_md5,
                    i_song, p_sys->p_first_queue->p_queue[i_song]->psz_a,
                    i_song, p_sys->p_first_queue->p_queue[i_song]->psz_t,
                    i_song, p_sys->p_first_queue->p_queue[i_song]->psz_b,
                    i_song, p_sys->p_first_queue->p_queue[i_song]->psz_m,
                    i_song, p_sys->p_first_queue->p_queue[i_song]->i_l,
                    i_song, p_sys->p_first_queue->p_queue[i_song]->psz_i
                );
            }

            p_sys->i_post_socket = net_ConnectTCP( p_this,
                p_sys->psz_submit_host, p_sys->i_submit_port);

            /* we transmit the data */
            i_net_ret = net_Printf(
                VLC_OBJECT(p_this), p_sys->i_post_socket, NULL,
                POST_REQUEST, p_sys->psz_submit_file,
                strlen( psz_submit_string), p_sys->psz_submit_file,
                VERSION, psz_submit_string
            );

            if ( i_net_ret == -1 )
            {
                /* If connection fails, we assume we must handshake again */
                p_sys->i_interval = DEFAULT_INTERVAL;
                time( &p_sys->time_last_interval );
                p_sys->b_handshaked = VLC_FALSE;
                vlc_mutex_unlock( &p_sys->lock );
                continue;
            }

            p_buffer = ( uint8_t* ) calloc( 1, 1024 );
            if ( !p_buffer )
            {
                msg_Err( p_this, "Out of memory" );
                vlc_mutex_unlock ( &p_sys->lock );
                return;
            }

            i_net_ret = net_Read( p_this, p_sys->i_post_socket, NULL,
                        p_buffer, 1024, VLC_FALSE );
            if ( i_net_ret <= 0 )
            {
                /* if we get no answer, something went wrong : try again */
                vlc_mutex_unlock( &p_sys->lock );
                continue;
            }

            net_Close( p_sys->i_post_socket );

            /* record interval */
            p_buffer_pos = strstr( ( char * ) p_buffer, "INTERVAL" );
            if ( p_buffer_pos )
            {
                p_sys->i_interval = atoi( p_buffer_pos +
                                            strlen( "INTERVAL " ) );
                time( &p_sys->time_last_interval );
            }

            p_buffer_pos = strstr( ( char * ) p_buffer, "FAILED" );
            if ( p_buffer_pos )
            {
                /* woops, something failed */
                msg_Dbg( p_this, p_buffer_pos );
                vlc_mutex_unlock ( &p_sys->lock );
                continue;
            }

            p_buffer_pos = strstr( ( char * ) p_buffer, "BADAUTH" );
            if ( p_buffer_pos )
            {
                /* too much time elapsed after last handshake? */
                msg_Dbg( p_this, "Authentification failed, handshaking again" );
                p_sys->b_handshaked = VLC_FALSE;
                vlc_mutex_unlock ( &p_sys->lock );
                continue;
            }

            p_buffer_pos = strstr( ( char * ) p_buffer, "OK" );
            if ( p_buffer_pos )
            {
                if ( p_sys->p_first_queue->i_songs_nb == 10 )
                {
                    /* if there are more than one queue, delete the 1st */
                    p_first_queue = p_sys->p_first_queue->p_next_queue;
                    DeleteQueue( p_sys->p_first_queue );
                    free( p_sys->p_first_queue );
                    p_sys->p_first_queue = p_first_queue;
                }
                else
                {
                    DeleteQueue( p_sys->p_first_queue );
                    p_sys->p_first_queue->i_songs_nb = 0;
                }
                msg_Dbg( p_this, "Submission successfull!" );
            }
            vlc_mutex_unlock ( &p_sys->lock );
        } /* data transmission finished or skipped */

        msleep( INTF_IDLE_SLEEP );

        p_playlist = pl_Yield( p_this );
        PL_LOCK;
        if( p_playlist->request.i_status == PLAYLIST_STOPPED )
        {
            PL_UNLOCK;
            pl_Release( p_playlist );
            /* if we stopped, we won't submit playing song */
            vlc_mutex_lock( &p_sys->lock );
            p_sys->b_queued = VLC_TRUE;
            p_sys->b_metadata_read = VLC_TRUE;
            vlc_mutex_unlock( &p_sys->lock );
        }
        else
        {
            PL_UNLOCK;
            pl_Release( p_playlist );
        }

        vlc_mutex_lock( &p_sys->lock );
        if( p_sys->b_metadata_read == VLC_FALSE )
        {
            /* we read the metadata of the playing song */
            /* TODO: remove when meta_engine works */
            time( &played_time );
            played_time -= p_sys->p_current_song->time_playing;
            played_time -= p_sys->time_total_pauses;

            vlc_mutex_unlock( &p_sys->lock );

            /* TODO: remove when meta_engine works */
            if( played_time > 10 )
            {
                if ( ReadMetaData( p_this ) == VLC_ENOMEM )
                {
                    msg_Err( p_this, "Out of memory" );
                    return;
                }
            }
        }
        else
        {
            /* we add the playing song into the queue */
            if( ( p_sys->b_queued == VLC_FALSE )
                && ( p_sys->b_paused == VLC_FALSE ) )
            {
                vlc_mutex_unlock( &p_sys->lock );
                if( AddToQueue( p_this ) == VLC_ENOMEM )
                {
                    msg_Err( p_this, "Out of memory" );
                    return;
                }
            }
            else
            {
                vlc_mutex_unlock( &p_sys->lock );
            }
        }
    }
}

/*****************************************************************************
 * PlayingChange: Playing status change callback
 *****************************************************************************/
static int PlayingChange( vlc_object_t *p_this, const char *psz_var,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    intf_thread_t *p_intf = ( intf_thread_t* ) p_data;
    intf_sys_t *p_sys = p_intf->p_sys;

    (void)p_this; (void)psz_var; (void)oldval;

    vlc_mutex_lock( &p_sys->lock );

    if( newval.i_int == PAUSE_S )
    {
        time( &p_sys->time_pause );
        p_sys->b_paused = VLC_TRUE;
    }

    if( newval.i_int == PLAYING_S )
    {
        p_sys->time_total_pauses += time( NULL ) - p_sys->time_pause;
        p_sys->b_paused = VLC_FALSE;
    }

    vlc_mutex_unlock( &p_sys->lock );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * ItemChange: Playlist item change callback
 *****************************************************************************/
static int ItemChange( vlc_object_t *p_this, const char *psz_var,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    playlist_t          *p_playlist;
    input_thread_t      *p_input = NULL;
    time_t              epoch;
    struct tm           *epoch_tm;
    char                psz_date[20];

    (void)p_this; (void)psz_var; (void)oldval; (void)newval;

    intf_thread_t *p_intf = ( intf_thread_t* ) p_data;
    intf_sys_t *p_sys = p_intf->p_sys;

    p_playlist = pl_Yield( p_intf );
    PL_LOCK;
    p_input = p_playlist->p_input;

    if( !p_input )
    {
        PL_UNLOCK;
        pl_Release( p_playlist );
        vlc_mutex_lock( &p_sys->lock );

        /* we won't read p_input */
        p_sys->b_queued = VLC_TRUE;
        p_sys->b_metadata_read = VLC_TRUE;

        vlc_mutex_unlock( &p_sys->lock );
        return VLC_SUCCESS;
    }

    vlc_object_yield( p_input );
    PL_UNLOCK;
    pl_Release( p_playlist );

    var_AddCallback( p_input, "state", PlayingChange, p_intf );

    vlc_mutex_lock ( &p_sys->lock );

    /* reset pause counter */
    p_sys->time_total_pauses = 0;

    /* we'll read metadata when it's present */
    p_sys->b_metadata_read = VLC_FALSE;
    p_sys->b_waiting_meta = VLC_FALSE;

    time( &epoch );
    epoch_tm = gmtime( &epoch );
    snprintf( psz_date, 20, "%.4d-%.2d-%.2d %.2d:%.2d:%.2d",
        epoch_tm->tm_year+1900, epoch_tm->tm_mon+1, epoch_tm->tm_mday,
        epoch_tm->tm_hour, epoch_tm->tm_min, epoch_tm->tm_sec );

    p_sys->p_current_song->psz_i = encode_URI_component( psz_date );
    p_sys->p_current_song->time_playing = epoch;

    p_sys->b_paused = ( p_input->b_dead || !p_input->input.p_item->psz_name )
                      ? VLC_TRUE : VLC_FALSE;

    vlc_mutex_unlock( &p_sys->lock );

    vlc_object_release( p_input );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * AddToQueue: Add the played song to the queue to be submitted
 *****************************************************************************/
static int AddToQueue ( intf_thread_t *p_this )
{
    int                         i_songs_nb;
    time_t                      played_time;
    audioscrobbler_queue_t      *p_queue = NULL, *p_next_queue = NULL;

    intf_sys_t *p_sys = p_this->p_sys;

    /* wait for the user to listen enough before submitting */
    time ( &played_time );
    played_time -= p_sys->p_current_song->time_playing;
    played_time -= p_sys->time_total_pauses;

    vlc_mutex_lock( &p_sys->lock );
    if( ( played_time < 240 )
        && ( played_time < ( p_sys->p_current_song->i_l / 2 ) ) )
    {
        vlc_mutex_unlock ( &p_sys->lock );
        return VLC_SUCCESS;
    }

    if( p_sys->p_current_song->i_l < 30 )
    {
        msg_Dbg( p_this, "Song too short (< 30s) -> not submitting" );
        p_sys->b_queued = VLC_TRUE;
        vlc_mutex_unlock ( &p_sys->lock );
        return VLC_SUCCESS;
    }

    if( !*p_sys->p_current_song->psz_a || !*p_sys->p_current_song->psz_t )
    {
        msg_Dbg( p_this, "Missing artist or title -> not submitting" );
        p_sys->b_queued = VLC_TRUE;
        vlc_mutex_unlock ( &p_sys->lock );
        return VLC_SUCCESS;
    }

    msg_Dbg( p_this, "Ok. We'll put it in the queue for submission" );

    /* go to last queue */
    p_queue = p_sys->p_first_queue;
    while( ( p_queue->i_songs_nb == 10 ) && ( p_queue->p_next_queue != NULL ) )
    {
        p_queue = p_queue->p_next_queue;
    }

    i_songs_nb = p_queue->i_songs_nb;

    if( i_songs_nb == 10 )
    {
        p_next_queue = malloc( sizeof( audioscrobbler_queue_t ) );
        if( !p_next_queue )
        {
            vlc_mutex_unlock ( &p_sys->lock );
            return VLC_ENOMEM;
        }
        p_queue->p_next_queue = p_next_queue;
        i_songs_nb = 0;
        p_queue = p_next_queue;
        p_queue->i_songs_nb = i_songs_nb;
    }

    p_queue->p_queue[i_songs_nb] = malloc( sizeof( audioscrobbler_song_t ) );

    p_queue->p_queue[i_songs_nb]->i_l = p_sys->p_current_song->i_l;

    p_queue->p_queue[i_songs_nb]->psz_a =
        strdup( p_sys->p_current_song->psz_a );

    p_queue->p_queue[i_songs_nb]->psz_t =
        strdup( p_sys->p_current_song->psz_t );

    p_queue->p_queue[i_songs_nb]->psz_b =
        strdup( p_sys->p_current_song->psz_b );

    p_queue->p_queue[i_songs_nb]->psz_m =
        strdup( p_sys->p_current_song->psz_m );

    p_queue->p_queue[i_songs_nb]->psz_i =
        strdup( p_sys->p_current_song->psz_i );

    p_queue->i_songs_nb++;
    p_sys->b_queued = VLC_TRUE;

    vlc_mutex_unlock( &p_sys->lock );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Handshake : Init audioscrobbler connection
 *****************************************************************************/
static int Handshake( intf_thread_t *p_this )
{
    char                *psz_password = NULL;
    struct md5_s        *p_struct_md5 = NULL;
    char                *psz_password_md5 = NULL;
    char                *ps_challenge_md5 = NULL;

    stream_t            *p_stream;
    char                *psz_handshake_url = NULL;

    uint8_t             *p_buffer = NULL;
    char                *p_buffer_pos = NULL;
    char                *psz_buffer_substring = NULL;
    char                *psz_url_parser = NULL;
    int                 i_url_pos, i;

    intf_thread_t *p_intf = ( intf_thread_t* ) p_this;
    intf_sys_t *p_sys = p_this->p_sys;

    vlc_mutex_lock ( &p_sys->lock );

    p_sys->psz_username = config_GetPsz(p_this, "lastfm-username");
    if ( !p_sys->psz_username )
    {
        goto memerror;
    }

    if ( !*p_sys->psz_username )
    {
        return VLC_ENOVAR;
    }

    psz_handshake_url = malloc( 1024 );
    if ( !psz_handshake_url )
    {
        goto memerror;
    }

    snprintf( psz_handshake_url, 1024,
        "http://post.audioscrobbler.com/?hs=true&p=1.1&c=%s&v=%s&u=%s",
        CLIENT_NAME, CLIENT_VERSION, p_sys->psz_username );

    /* send the http handshake request */
    p_stream = stream_UrlNew( p_intf, psz_handshake_url);

    free( psz_handshake_url );

    if( !p_stream )
    {
        vlc_mutex_unlock ( &p_sys->lock );
        return VLC_EGENERIC;
    }

    p_buffer = ( uint8_t* ) calloc( 1, 1024 );
    if ( !p_buffer )
    {
        stream_Delete( p_stream );
        goto memerror;
    }

    /* read answer */
    if ( stream_Read( p_stream, p_buffer, 1024 ) == 0 )
    {
        stream_Delete( p_stream );
        goto generic_error;
    }

    stream_Delete( p_stream );

    /* record interval before next submission */
    p_buffer_pos = strstr( ( char * ) p_buffer, "INTERVAL" );
    if ( p_buffer_pos )
    {
        p_sys->i_interval = atoi( p_buffer_pos + strlen( "INTERVAL " ) );
        time( &p_sys->time_last_interval );
    }

    p_buffer_pos = strstr( ( char * ) p_buffer, "FAILED" );
    if ( p_buffer_pos )
    {
        /* handshake request failed */
        msg_Dbg( p_this, p_buffer_pos );
        goto generic_error;
    }

    p_buffer_pos = strstr( ( char * ) p_buffer, "BADUSER" );
    if ( p_buffer_pos )
    {
        /* username does not exist */
        intf_UserFatal( p_this, VLC_FALSE, _("Bad last.fm Username"),
            _("last.fm username is incorrect, please verify your settings")
        );
        goto generic_error;
    }

    p_buffer_pos = strstr( ( char * ) p_buffer, "UPDATE" );
    if ( p_buffer_pos )
    {
        /* protocol has been updated, developers need to work :) */
        msg_Dbg( p_intf, "Protocol updated" );
        msg_Dbg( p_intf, p_buffer_pos );
    }

    else
    {
        p_buffer_pos = strstr( ( char * ) p_buffer, "UPTODATE" );
        if ( !p_buffer_pos )
        {
            msg_Dbg( p_intf, "Protocol error" );
            goto generic_error;
        }
    }

    psz_buffer_substring = strndup( strstr( p_buffer_pos, "\n" ) + 1, 32 );
    if ( !psz_buffer_substring )
    {
        goto memerror;
    }
    else
    {
        ps_challenge_md5 = malloc( sizeof( char ) * 32 );
        if ( !ps_challenge_md5 )
        {
            goto memerror;
        }
        memcpy( ps_challenge_md5, psz_buffer_substring, 32 );
        free( psz_buffer_substring );
    }

    p_buffer_pos = ( void* ) strstr( ( char* ) p_buffer, "http://" );

    free( p_sys->psz_submit_host );
    free( p_sys->psz_submit_file );

    psz_url_parser = p_buffer_pos + strlen( "http://" );

    i_url_pos = strcspn( psz_url_parser, ":" );
    p_sys->psz_submit_host = strndup( psz_url_parser, i_url_pos );

    p_sys->i_submit_port = atoi( psz_url_parser + i_url_pos + 1 );

    psz_url_parser += strcspn( psz_url_parser , "/" ) + 1;
    i_url_pos = strcspn( psz_url_parser, "\n" );
    p_sys->psz_submit_file = strndup( psz_url_parser, i_url_pos );

    free(p_buffer);

    p_struct_md5 = malloc( sizeof( struct md5_s ) );
    if( !p_struct_md5 )
    {
        goto memerror;
    }

    psz_password = config_GetPsz(p_this, "lastfm-password");
    if ( !psz_password )
    {
         goto memerror;
    }

    /* generates a md5 hash of the password */
    InitMD5( p_struct_md5 );
    AddMD5( p_struct_md5, ( uint8_t* ) psz_password, strlen( psz_password ) );
    EndMD5( p_struct_md5 );

    free( psz_password );

    psz_password_md5 = malloc ( 33 * sizeof( char ) );
    if ( !psz_password_md5 )
    {
        goto memerror;
    }

    for ( i = 0; i < 4; i++ )
    {
        sprintf( &psz_password_md5[8*i], "%02x%02x%02x%02x",
            p_struct_md5->p_digest[i] & 0xff,
            ( p_struct_md5->p_digest[i] >> 8 ) & 0xff,
            ( p_struct_md5->p_digest[i] >> 16 ) & 0xff,
            p_struct_md5->p_digest[i] >> 24
        );
    }

    /* generates a md5 hash of :
     * - md5 hash of the password
     * - md5 challenge sent by last.fm server
     */
    InitMD5( p_struct_md5 );
    AddMD5( p_struct_md5, ( uint8_t* ) psz_password_md5, 32 );
    AddMD5( p_struct_md5, ( uint8_t* ) ps_challenge_md5, 32 );
    EndMD5( p_struct_md5 );

    free( ps_challenge_md5 );
    free( psz_password_md5 );

    for ( i = 0; i < 4; i++ )
    {
        sprintf( &p_sys->psz_response_md5[8*i], "%02x%02x%02x%02x",
            p_struct_md5->p_digest[i] & 0xff,
            ( p_struct_md5->p_digest[i] >> 8 ) & 0xff,
            ( p_struct_md5->p_digest[i] >> 16 ) & 0xff,
            p_struct_md5->p_digest[i] >> 24
        );
    }

    p_sys->psz_response_md5[32] = 0;

    vlc_mutex_unlock ( &p_sys->lock );

    return VLC_SUCCESS;

generic_error:
    free( p_buffer );
    vlc_mutex_unlock( &p_sys->lock );
    return VLC_EGENERIC;

memerror:
    free( p_buffer );
    free( p_struct_md5 );
    free( psz_buffer_substring );

    vlc_mutex_unlock( &p_sys->lock );
    return VLC_ENOMEM;
}

/*****************************************************************************
 * DeleteQueue : Free all songs from an audioscrobbler_queue_t
 *****************************************************************************/
void DeleteQueue( audioscrobbler_queue_t *p_queue )
{
    int     i;

    for( i = 0; i < p_queue->i_songs_nb; i++ )
    {
        free( p_queue->p_queue[i]->psz_a );
        free( p_queue->p_queue[i]->psz_b );
        free( p_queue->p_queue[i]->psz_t );
        free( p_queue->p_queue[i]->psz_i );
        free( p_queue->p_queue[i] );
    }
}

/*****************************************************************************
 * ReadMetaData : Read meta data when parsed by vlc
 * or wait for fetching if unavailable
 *****************************************************************************/
static int ReadMetaData( intf_thread_t *p_this )
{
    playlist_t          *p_playlist;
    input_thread_t      *p_input = NULL;
    vlc_value_t         video_val;

    intf_sys_t *p_sys = p_this->p_sys;

    p_playlist = pl_Yield( p_this );
    PL_LOCK;
    p_input = p_playlist->p_input;

    if( !p_input )
    {
        PL_UNLOCK;
        pl_Release( p_playlist );
        return( VLC_SUCCESS );
    }

    vlc_object_yield( p_input );
    PL_UNLOCK;
    pl_Release( p_playlist );

    var_Change( p_input, "video-es", VLC_VAR_CHOICESCOUNT, &video_val, NULL );
    if( ( video_val.i_int > 0 ) || \
        ( p_input->input.p_item->i_type == ITEM_TYPE_NET ) )
    {
        msg_Dbg( p_this, "Not an audio file -> no submission");
        vlc_object_release( p_input );

        vlc_mutex_lock( &p_sys->lock );
        p_sys->b_queued = VLC_TRUE;
        p_sys->b_metadata_read = VLC_TRUE;
        vlc_mutex_unlock( &p_sys->lock );

        return VLC_SUCCESS;
    }

    return ReadLocalMetaData( p_this, p_input );    
}

/*****************************************************************************
 * ReadLocalMetaData : Puts current song's meta data in p_sys->p_current_song
 *****************************************************************************/
static int ReadLocalMetaData( intf_thread_t *p_this, input_thread_t  *p_input )
{
    char                *psz_title = NULL;
    char                *psz_artist = NULL;
    char                *psz_album = NULL;
    char                *psz_trackid = NULL;
    int                 i_length = -1;
    vlc_bool_t          b_waiting;
    int                 i_status;

    intf_sys_t *p_sys = p_this->p_sys;

    i_status = p_input->input.p_item->p_meta->i_status;

    vlc_mutex_lock( &p_sys->lock );
    b_waiting = p_sys->b_waiting_meta;
    vlc_mutex_unlock( &p_sys->lock );

    /* TODO : remove if (1) when meta_engine works */
    if ( (1/*( i_status & ITEM_PREPARSED )*/&& ( b_waiting == VLC_FALSE ) ) || \
        ( ( i_status & ITEM_META_FETCHED ) && ( b_waiting == VLC_TRUE ) ) )
    {
        if ( p_input->input.p_item->p_meta->psz_artist )
        {
            psz_artist = encode_URI_component(
                p_input->input.p_item->p_meta->psz_artist );
            if ( !psz_artist )
            {
                goto error;
            }
        }
        else
        {
            msg_Dbg( p_this, "No artist.." );
            if ( b_waiting == VLC_TRUE )
            {
                psz_artist = calloc( 1, sizeof( char ) );
            }
            else
            {
                goto waiting_meta_data_fetching;
            }
        }
        if ( p_input->input.p_item->psz_name )
        {
            psz_title = encode_URI_component( p_input->input.p_item->psz_name );
            if ( !psz_title )
            {
                goto error;
            }
        }
        else
        {
            msg_Dbg( p_this, "No track name.." );
            if ( b_waiting == VLC_TRUE )
            {
                psz_title = calloc( 1, sizeof( char ) );
            }
            else
            {
                goto waiting_meta_data_fetching;
            }
        }

        if ( p_input->input.p_item->p_meta->psz_trackid )
        {
            psz_trackid = strdup( p_input->input.p_item->p_meta->psz_trackid );
            if ( !psz_trackid )
            {
                goto error;
            }
        }
        else
        {
            psz_trackid = calloc( 1, sizeof( char ) );
        }

        if ( p_input->input.p_item->p_meta->psz_album )
        {
            psz_album = encode_URI_component(
                p_input->input.p_item->p_meta->psz_album );
            if ( !psz_album )
            {
                goto error;
            }
        }
        else
        {
            psz_album = calloc( 1, sizeof( char ) );
        }

        i_length = p_input->input.p_item->i_duration / 1000000;

        vlc_object_release( p_input );

        vlc_mutex_lock ( &p_sys->lock );

        p_sys->p_current_song->psz_a = strdup( psz_artist );
        p_sys->p_current_song->psz_t = strdup( psz_title );
        p_sys->p_current_song->psz_b = strdup( psz_album );
        p_sys->p_current_song->psz_m = strdup( psz_trackid );
        p_sys->p_current_song->i_l = i_length;
        p_sys->b_queued = VLC_FALSE;
        p_sys->b_metadata_read = VLC_TRUE;

        vlc_mutex_unlock ( &p_sys->lock );

        msg_Dbg( p_this, "Meta data registered, waiting to be queued" );

        free( psz_title );
        free( psz_artist );
        free( psz_album );
        free( psz_trackid );

        return VLC_SUCCESS;
    }
    
    return VLC_SUCCESS;

waiting_meta_data_fetching:
    vlc_object_release( p_input );

    vlc_mutex_lock( &p_sys->lock );
    p_sys->b_waiting_meta = VLC_TRUE;
    vlc_mutex_unlock( &p_sys->lock );

    free( psz_artist );
    free( psz_album );
    free( psz_title );

    return VLC_SUCCESS;

error:
    vlc_object_release( p_input );

    free( psz_artist );
    free( psz_album );
    free( psz_title );
    free( psz_trackid );
    return VLC_ENOMEM;
}

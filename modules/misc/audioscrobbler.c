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

/* audioscrobbler protocol version: 1.1 
 * http://audioscrobbler.net/wiki/Protocol1.1
 * */

/*****************************************************************************
 * Preamble
 *****************************************************************************/


#if defined( WIN32 )
#include <time.h>
#endif

#include <vlc/vlc.h>
#include <vlc_interface.h>
#include <vlc_meta.h>
#include <vlc_md5.h>
#include <vlc_block.h>
#include <vlc_stream.h>
#include <vlc_url.h>
#include <vlc_network.h>
#include <vlc_interface.h>
#include <vlc_playlist.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

/* Keeps track of metadata to be submitted */
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
    time_t                  time_next_exchange; /* when can we send data? */
    char                    *psz_submit_host;   /* where to submit data ? */
    int                     i_submit_port;      /* at which port ?        */
    char                    *psz_submit_file;   /* in which file ?        */
    char                    *psz_username;      /* last.fm username       */
    vlc_bool_t              b_handshaked;       /* did we handshake ?     */
    char                    *psz_response_md5;  /* md5 response to use    */

    /* data about song currently playing */
    audioscrobbler_song_t   *p_current_song;    /* song being played      */
    time_t                  time_pause;         /* time when vlc paused   */
    time_t                  time_total_pauses;  /* sum of time in pause   */
    vlc_bool_t              b_queued;           /* has it been queud ?    */
    vlc_bool_t              b_metadata_read;    /* did we read metadata ? */
    vlc_bool_t              b_paused;           /* is vlc paused ?        */
    vlc_bool_t              b_waiting_meta;     /* we need fetched data?  */
};

intf_sys_t *p_sys_global;     /* to retrieve p_sys in Run() thread */

static int  Open        ( vlc_object_t * );
static void Close       ( vlc_object_t * );
static void Run         ( intf_thread_t * );
static void Main        ( intf_thread_t * );
static int ItemChange   ( vlc_object_t *, const char *, vlc_value_t,
                                vlc_value_t, void * );
static int PlayingChange( vlc_object_t *, const char *, vlc_value_t,
                                vlc_value_t, void * );
static int AddToQueue   ( intf_thread_t *p_this );
static int Handshake    ( intf_thread_t *p_sd );
static int ReadMetaData ( intf_thread_t *p_this );
void DeleteQueue        ( audioscrobbler_queue_t *p_queue );

/*****************************************************************************
 * Module descriptor
 ****************************************************************************/

#define USERNAME_TEXT       N_("Username")
#define USERNAME_LONGTEXT   N_("The username of your last.fm account")
#define PASSWORD_TEXT       N_("Password")
#define PASSWORD_LONGTEXT   N_("The password of your last.fm account")

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
#define POST_DATA       "&a%%5B%d%%5D=%s&t%%5B%d%%5D=%s&b%%5B%d%%5D=%s" \
                        "&m%%5B%d%%5D=%s&l%%5B%d%%5D=%d&i%%5B%d%%5D=%s"
#define HTTPPOST_MAXLEN 2048

vlc_module_begin();
    set_category( CAT_INTERFACE );
    set_subcategory( SUBCAT_INTERFACE_CONTROL );
    set_shortname( N_( "Audioscrobbler" ) );
    set_description( N_("Audioscrobbler submission Plugin") );
    add_string( "lastfm-username", "", NULL,
                USERNAME_TEXT, USERNAME_LONGTEXT, VLC_FALSE );
    add_password( "lastfm-password", "", NULL,
                PASSWORD_TEXT, PASSWORD_LONGTEXT, VLC_FALSE );
    set_capability( "interface", 0 );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Open: initialize and create stuff
 *****************************************************************************/

static int Open( vlc_object_t *p_this )
{
    playlist_t      *p_playlist;
    intf_thread_t   *p_intf     = ( intf_thread_t* ) p_this;
    intf_sys_t      *p_sys      = malloc( sizeof( intf_sys_t ) );

#define MEM_ERROR \
    free( p_sys->p_current_song ); \
    free( p_sys->p_first_queue ); \
    free( p_sys->psz_response_md5 ); \
    free( p_sys ); \
    return VLC_ENOMEM;

    if( !p_sys )
    {
        MEM_ERROR
    }

    p_intf->p_sys = p_sys;

    vlc_mutex_init( p_this, &p_sys->lock );

    p_sys_global = p_sys;
    p_sys->psz_submit_host = NULL;
    p_sys->psz_submit_file = NULL;
    p_sys->b_handshaked = VLC_FALSE;
    p_sys->time_next_exchange = time( NULL );
    p_sys->psz_username = NULL;
    p_sys->b_paused = VLC_FALSE;

#define MALLOC_CHECK( a ) \
    if( !a ) { \
        vlc_mutex_destroy( &p_sys->lock ); \
        MEM_ERROR \
    }

    /* md5 response is 32 chars, + final \0 */
    p_sys->psz_response_md5 = malloc( 33 );
    MALLOC_CHECK( p_sys->psz_response_md5 )

    p_sys->p_first_queue = malloc( sizeof( audioscrobbler_queue_t ) );
    MALLOC_CHECK( p_sys->p_first_queue )

    p_sys->p_current_song = malloc( sizeof( audioscrobbler_song_t ) );
    MALLOC_CHECK( p_sys->p_current_song )
    time( &p_sys->p_current_song->time_playing );

    /* queues can't contain more than 10 songs */
    p_sys->p_first_queue->p_queue =
        malloc( 10 * sizeof( audioscrobbler_song_t ) );
    MALLOC_CHECK( p_sys->p_current_song )

    p_sys->p_first_queue->i_songs_nb = 0;
    p_sys->p_first_queue->p_next_queue = NULL;

    p_playlist = pl_Yield( p_intf );
    PL_LOCK;
    var_AddCallback( p_playlist, "playlist-current", ItemChange, p_intf );
    PL_UNLOCK;
    pl_Release( p_playlist );

    p_intf->pf_run = Run;

    return VLC_SUCCESS;
#undef MEM_ERROR
#undef MALLOC_CHECK
}

/*****************************************************************************
 * Close: destroy interface stuff
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    audioscrobbler_queue_t      *p_current_queue, *p_next_queue;
    playlist_t                  *p_playlist;
    input_thread_t              *p_input;
    intf_thread_t               *p_intf = ( intf_thread_t* ) p_this;
    intf_sys_t                  *p_sys  = p_intf->p_sys;

    p_playlist = pl_Yield( p_intf );
    PL_LOCK;
    var_DelCallback( p_playlist, "playlist-current", ItemChange, p_intf );

    p_input = p_playlist->p_input;
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
    free( p_sys->psz_submit_host );
    free( p_sys->psz_submit_file );
    free( p_sys->psz_response_md5 );
    vlc_mutex_unlock ( &p_sys->lock );
    vlc_mutex_destroy( &p_sys->lock );
    free( p_sys );
}

/****************************************************************************
 * Run : create Main() thread
 * **************************************************************************/
static void Run( intf_thread_t *p_intf )
{
    if( vlc_thread_create( p_intf, "Audioscrobbler", Main, 0, VLC_TRUE ) )
        msg_Err( p_intf, "failed to create Audioscrobbler thread" );
}

/*****************************************************************************
 * Main : call Handshake() then submit songs
 *****************************************************************************/
static void Main( intf_thread_t *p_this )
{
    char                    *psz_submit         = NULL;
    char                    *psz_submit_song    = NULL;
    int                     i_net_ret;
    int                     i_song;
    playlist_t              *p_playlist;
    uint8_t                 *p_buffer           = NULL;
    char                    *p_buffer_pos       = NULL;
    audioscrobbler_queue_t  *p_first_queue;
    int                     i_post_socket;
    time_t                  played_time;

    vlc_thread_ready( p_this );

    p_this->p_sys = p_sys_global;
    intf_sys_t *p_sys = p_this->p_sys;

    #define MEM_ERROR \
        free( psz_submit ); \
        free( psz_submit_song ); \
        free( p_buffer ); \
        msg_Err( p_this, "Out of memory" ); \
        return;

    psz_submit = malloc( HTTPPOST_MAXLEN );
    psz_submit_song = malloc( HTTPPOST_MAXLEN );
    p_buffer = ( uint8_t* ) malloc( 1024 );

    if( !psz_submit || !psz_submit_song || !p_buffer )
    {
        MEM_ERROR
    }

    /* main loop */
    while( !p_this->b_die )
    {
        /* verify if there is data to submit 
         * and if waiting interval is elapsed */
        if ( ( p_sys->p_first_queue->i_songs_nb > 0 ) &&
            ( time( NULL ) >= p_sys->time_next_exchange ) )
        {
            /* handshake if needed */
            if( p_sys->b_handshaked == VLC_FALSE )
            {
                msg_Dbg( p_this, "Handshaking with last.fm ..." );

                switch( Handshake( p_this ) )
                {
                    case VLC_ENOMEM:
                        MEM_ERROR
                        break;

                    case VLC_ENOVAR:
                        /* username not set */
                        vlc_mutex_unlock ( &p_sys->lock );
                        intf_UserFatal( p_this, VLC_FALSE,
                            _("Last.fm username not set"),
                            _("Please set an username or disable "
                            "audioscrobbler plugin, and then restart VLC.\n"
                            "Visit https://www.last.fm/join/ to get an account")
                        );
                        free( psz_submit );
                        free( psz_submit_song );
                        free( p_buffer );
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
                        /* protocol error : we'll try later */
                        vlc_mutex_lock ( &p_sys->lock );
                        time( &p_sys->time_next_exchange );
                        p_sys->time_next_exchange += DEFAULT_INTERVAL;
                        vlc_mutex_unlock ( &p_sys->lock );
                        break;
                }
                /* handshake is done or failed, lets start from 
                 * beginning to check it out and wait INTERVAL if needed
                 */
                continue;
            }

            msg_Dbg( p_this, "Going to submit some data..." );
            vlc_mutex_lock ( &p_sys->lock );

            snprintf( psz_submit, HTTPPOST_MAXLEN, "u=%s&s=%s",
                p_sys->psz_username, p_sys->psz_response_md5 );

            /* forge the HTTP POST request */
            for (i_song = 0 ; i_song < p_sys->p_first_queue->i_songs_nb ;
                i_song++ )
            {
                snprintf( psz_submit_song, HTTPPOST_MAXLEN -1, POST_DATA,
                    i_song, p_sys->p_first_queue->p_queue[i_song]->psz_a,
                    i_song, p_sys->p_first_queue->p_queue[i_song]->psz_t,
                    i_song, p_sys->p_first_queue->p_queue[i_song]->psz_b,
                    i_song, p_sys->p_first_queue->p_queue[i_song]->psz_m,
                    i_song, p_sys->p_first_queue->p_queue[i_song]->i_l,
                    i_song, p_sys->p_first_queue->p_queue[i_song]->psz_i
                );
                strncat( psz_submit, psz_submit_song, HTTPPOST_MAXLEN - 1 );
            }

            i_post_socket = net_ConnectTCP( p_this,
                p_sys->psz_submit_host, p_sys->i_submit_port);

            if ( i_post_socket == -1 )
            {
                /* If connection fails, we assume we must handshake again */
                time( &p_sys->time_next_exchange );
                p_sys->time_next_exchange += DEFAULT_INTERVAL;
                p_sys->b_handshaked = VLC_FALSE;
                vlc_mutex_unlock( &p_sys->lock );
                continue;
            }

            /* we transmit the data */
            i_net_ret = net_Printf(
                VLC_OBJECT(p_this), i_post_socket, NULL,
                POST_REQUEST, p_sys->psz_submit_file,
                strlen( psz_submit ), p_sys->psz_submit_file,
                VERSION, psz_submit
            );

            if ( i_net_ret == -1 )
            {
                /* If connection fails, we assume we must handshake again */
                time( &p_sys->time_next_exchange );
                p_sys->time_next_exchange += DEFAULT_INTERVAL;
                p_sys->b_handshaked = VLC_FALSE;
                vlc_mutex_unlock( &p_sys->lock );
                continue;
            }

            memset( p_buffer, '\0', 1024 );

            i_net_ret = net_Read( p_this, i_post_socket, NULL,
                        p_buffer, 1024, VLC_FALSE );
            if ( i_net_ret <= 0 )
            {
                /* if we get no answer, something went wrong : try again */
                vlc_mutex_unlock( &p_sys->lock );
                continue;
            }

            net_Close( i_post_socket );

            /* record interval */
            p_buffer_pos = strstr( ( char * ) p_buffer, "INTERVAL" );
            if ( p_buffer_pos )
            {
                time( &p_sys->time_next_exchange );
                p_sys->time_next_exchange += atoi( p_buffer_pos +
                                            strlen( "INTERVAL " ) );
            }

            p_buffer_pos = strstr( ( char * ) p_buffer, "FAILED" );
            if ( p_buffer_pos )
            {
                /* woops, submission failed */
                msg_Dbg( p_this, "%s", p_buffer_pos );
                vlc_mutex_unlock ( &p_sys->lock );
                continue;
            }

            p_buffer_pos = strstr( ( char * ) p_buffer, "BADAUTH" );
            if ( p_buffer_pos )
            {
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
            /* if we stopped, we won't submit playing song */
            vlc_mutex_lock( &p_sys->lock );
            p_sys->b_queued = VLC_TRUE;
            p_sys->b_metadata_read = VLC_TRUE;
            vlc_mutex_unlock( &p_sys->lock );
        }
        PL_UNLOCK;
        pl_Release( p_playlist );

        vlc_mutex_lock( &p_sys->lock );
        if( p_sys->b_metadata_read == VLC_FALSE )
        {
            /* we read the metadata of the playing song */
            time( &played_time );
            played_time -= p_sys->p_current_song->time_playing;
            played_time -= p_sys->time_total_pauses;

            vlc_mutex_unlock( &p_sys->lock );

            if( played_time > 20 )
            /* wait at least 20 secondes before reading the meta data
             * since the songs must be at least 30s */
            {
                if ( ReadMetaData( p_this ) == VLC_ENOMEM )
                {
                    MEM_ERROR
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
                    MEM_ERROR
                }
            }
            else
            {
                vlc_mutex_unlock( &p_sys->lock );
            }
        }
    }
#undef MEM_ERROR
}

/*****************************************************************************
 * PlayingChange: Playing status change callback
 *****************************************************************************/
static int PlayingChange( vlc_object_t *p_this, const char *psz_var,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    intf_thread_t   *p_intf = ( intf_thread_t* ) p_data;
    intf_sys_t      *p_sys  = p_intf->p_sys;

    (void)p_this; (void)psz_var; (void)oldval;

    /* don't bother if song has already been queued */
    if( p_sys->b_queued == VLC_TRUE )
        return VLC_SUCCESS;

    vlc_mutex_lock( &p_sys->lock );

    if( newval.i_int == PAUSE_S )
    {
        time( &p_sys->time_pause );
        p_sys->b_paused = VLC_TRUE;
    }

    else if( newval.i_int == PLAYING_S )
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
    input_thread_t      *p_input    = NULL;
    time_t              epoch;
    struct tm           *epoch_tm;
    char                psz_date[20];
    intf_thread_t       *p_intf     = ( intf_thread_t* ) p_data;
    intf_sys_t          *p_sys      = p_intf->p_sys;
    (void)p_this; (void)psz_var; (void)oldval; (void)newval;

    p_playlist = pl_Yield( p_intf );
    PL_LOCK;
    p_input = p_playlist->p_input;

    if( !p_input )
    {
        PL_UNLOCK;
        pl_Release( p_playlist );

        vlc_mutex_lock( &p_sys->lock );

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

    char *psz_name = input_item_GetName( input_GetItem( p_input ) );
    p_sys->b_paused = ( p_input->b_dead || !psz_name )
                      ? VLC_TRUE : VLC_FALSE;
    free( psz_name );

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
    audioscrobbler_queue_t      *p_queue        = NULL,
                                *p_next_queue   = NULL;
    intf_sys_t                  *p_sys          = p_this->p_sys;

    /* wait for the user to listen enough before submitting */
    time ( &played_time );
    vlc_mutex_lock( &p_sys->lock );

    played_time -= p_sys->p_current_song->time_playing;
    played_time -= p_sys->time_total_pauses;

    #define NO_SUBMISSION \
        p_sys->b_queued = VLC_TRUE; \
        vlc_mutex_unlock( &p_sys->lock ); \
        return VLC_SUCCESS;

    if( ( played_time < 240 ) &&
        ( played_time < ( p_sys->p_current_song->i_l / 2 ) ) )
    {
        //msg_Dbg( p_this, "Song not listened long enough -> waiting" );
        vlc_mutex_unlock( &p_sys->lock );
        return VLC_SUCCESS;
    }

    if( p_sys->p_current_song->i_l < 30 )
    {
        msg_Dbg( p_this, "Song too short (< 30s) -> not submitting" );
        NO_SUBMISSION
    }

    if( !*p_sys->p_current_song->psz_a || !*p_sys->p_current_song->psz_t )
    {
        msg_Dbg( p_this, "Missing artist or title -> not submitting" );
        NO_SUBMISSION
    }

    msg_Dbg( p_this, "Ok. We'll put it in the queue for submission" );

    /* go to last queue */
    p_queue = p_sys->p_first_queue;
    while( ( p_queue->i_songs_nb == 10 ) && ( p_queue->p_next_queue != NULL ) )
        p_queue = p_queue->p_next_queue;

    i_songs_nb = p_queue->i_songs_nb;

    #define MALLOC_CHECK( a ) \
        if( !a ) { \
            vlc_mutex_unlock( &p_sys->lock ); \
            return VLC_ENOMEM; \
        }

    if( i_songs_nb == 10 )
    {
        p_next_queue = malloc( sizeof( audioscrobbler_queue_t ) );
        MALLOC_CHECK( p_next_queue );
        p_queue->p_next_queue = p_next_queue;
        p_queue = p_next_queue;
        i_songs_nb = 0;
        p_queue->i_songs_nb = i_songs_nb;
    }

    p_queue->p_queue[i_songs_nb] = malloc( sizeof( audioscrobbler_song_t ) );
    MALLOC_CHECK( p_queue->p_queue[i_songs_nb] );

    #define QUEUE_COPY( a ) \
        p_queue->p_queue[i_songs_nb]->a = p_sys->p_current_song->a

    QUEUE_COPY(i_l);
    QUEUE_COPY(psz_a);
    QUEUE_COPY(psz_t);
    QUEUE_COPY(psz_b);
    QUEUE_COPY(psz_m);
    QUEUE_COPY(psz_i);

    p_queue->i_songs_nb++;
    p_sys->b_queued = VLC_TRUE;

    vlc_mutex_unlock( &p_sys->lock );

    return VLC_SUCCESS;
#undef QUEUE_COPY
#undef MALLOC_CHECK
#undef NO_SUBMISSION
}

/*****************************************************************************
 * Handshake : Init audioscrobbler connection
 *****************************************************************************/
static int Handshake( intf_thread_t *p_this )
{
    char                *psz_password           = NULL;
    struct md5_s        *p_struct_md5           = NULL;
    char                *psz_password_md5       = NULL;
    char                *ps_challenge_md5       = NULL;
    stream_t            *p_stream;
    char                *psz_handshake_url      = NULL;
    uint8_t             *p_buffer               = NULL;
    char                *p_buffer_pos           = NULL; 
    char                *psz_url_parser         = NULL;
    char                *psz_buffer_substring;
    int                 i_url_pos, i;

    intf_thread_t       *p_intf                 = ( intf_thread_t* ) p_this;
    intf_sys_t          *p_sys                  = p_this->p_sys;

    vlc_mutex_lock ( &p_sys->lock );

    #define MEM_ERROR \
        free( p_buffer ); \
        free( p_struct_md5 ); \
        free( ps_challenge_md5 ); \
        vlc_mutex_unlock( &p_sys->lock ); \
        return VLC_ENOMEM;

    #define PROTOCOL_ERROR \
        free( p_buffer ); \
        vlc_mutex_unlock( &p_sys->lock ); \
        return VLC_EGENERIC;

    #define MALLOC_CHECK( a ) \
        if( !a ) \
        { \
            MEM_ERROR \
        }

    p_sys->psz_username = config_GetPsz(p_this, "lastfm-username");
    MALLOC_CHECK( p_sys->psz_username )

    /* username has not been setup, ignoring */
    if ( !*p_sys->psz_username )
        return VLC_ENOVAR;

    psz_handshake_url = malloc( 1024 );
    MALLOC_CHECK( p_sys->psz_username )

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
        MEM_ERROR
    }

    /* read answer */
    if ( stream_Read( p_stream, p_buffer, 1024 ) == 0 )
    {
        stream_Delete( p_stream );
        PROTOCOL_ERROR
    }

    stream_Delete( p_stream );

    /* record interval before next submission */
    p_buffer_pos = strstr( ( char* ) p_buffer, "INTERVAL" );
    if ( p_buffer_pos )
    {
        time( &p_sys->time_next_exchange );
        p_sys->time_next_exchange +=
                atoi( p_buffer_pos + strlen( "INTERVAL " ) );
    }

    p_buffer_pos = strstr( ( char* ) p_buffer, "FAILED" );
    if ( p_buffer_pos )
    {
        /* handshake request failed, sorry */
        msg_Dbg( p_this, "%s", p_buffer_pos );
        PROTOCOL_ERROR
    }

    p_buffer_pos = strstr( ( char* ) p_buffer, "BADUSER" );
    if ( p_buffer_pos )
    {
        /* username does not exist on the server */
        intf_UserFatal( p_this, VLC_FALSE, _("Bad last.fm Username"),
            _("last.fm username is incorrect, please verify your settings")
        );
        PROTOCOL_ERROR
    }

    p_buffer_pos = strstr( ( char* ) p_buffer, "UPDATE" );
    if ( p_buffer_pos )
    {
        /* protocol has been updated, time to update the code */
        msg_Dbg( p_intf, "Protocol updated : plugin may be outdated" );
        msg_Dbg( p_intf, "%s", p_buffer_pos );
    }

    else
    {
        p_buffer_pos = strstr( ( char* ) p_buffer, "UPTODATE" );
        if ( !p_buffer_pos )
        {
            msg_Dbg( p_intf, "Can't recognize server protocol" );
            PROTOCOL_ERROR
        }
    }

    psz_buffer_substring = strstr( p_buffer_pos, "\n" );
    if( ( psz_buffer_substring == NULL ) || \
            ( strlen( psz_buffer_substring + 1 ) < 32 ) )
    {
        msg_Dbg( p_intf, "Can't recognize server protocol" );
        PROTOCOL_ERROR
    }
    else
    {
        ps_challenge_md5 = malloc( 32 );
        MALLOC_CHECK( ps_challenge_md5 )
        memcpy( ps_challenge_md5, psz_buffer_substring + 1, 32 );
    }

    p_buffer_pos = ( void* ) strstr( ( char* ) p_buffer, "http://" );

    /* free old information */
    free( p_sys->psz_submit_host );
    free( p_sys->psz_submit_file );

    psz_url_parser = p_buffer_pos + strlen( "http://" );
    i_url_pos = strcspn( psz_url_parser, ":" );

    p_sys->psz_submit_host = strndup( psz_url_parser, i_url_pos );
    MALLOC_CHECK( p_sys->psz_submit_host )

    p_sys->i_submit_port = atoi( psz_url_parser + i_url_pos + 1 );

    psz_url_parser += strcspn( psz_url_parser , "/" ) + 1;
    i_url_pos = strcspn( psz_url_parser, "\n" );
    p_sys->psz_submit_file = strndup( psz_url_parser, i_url_pos );
    MALLOC_CHECK( p_sys->psz_submit_file )

    free(p_buffer);

    p_struct_md5 = malloc( sizeof( struct md5_s ) );
    MALLOC_CHECK( p_struct_md5 )

    psz_password = config_GetPsz(p_this, "lastfm-password");
    MALLOC_CHECK( psz_password )

    /* generates a md5 hash of the password */
    InitMD5( p_struct_md5 );
    AddMD5( p_struct_md5, ( uint8_t* ) psz_password, strlen( psz_password ) );
    EndMD5( p_struct_md5 );

    free( psz_password );

    psz_password_md5 = malloc ( 33 );
    MALLOC_CHECK( psz_password_md5 )

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
     * - md5 hash of the password, plus
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

    p_sys->psz_response_md5[32] = '\0';

    vlc_mutex_unlock ( &p_sys->lock );

    return VLC_SUCCESS;
#undef MEM_ERROR
#undef PROTOCOL_ERROR
#undef MALLOC_CHECK
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
    input_thread_t      *p_input        = NULL;
    vlc_value_t         video_val;

    char                *psz_title      = NULL;
    char                *psz_artist     = NULL;
    char                *psz_album      = NULL;
    char                *psz_trackid    = NULL;
    int                 i_length        = -1;
    vlc_bool_t          b_waiting;
    intf_sys_t          *p_sys          = p_this->p_sys;
    int                 i_status;

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
        ( input_GetItem( p_input )->i_type == ITEM_TYPE_NET ) )
    {
        msg_Dbg( p_this, "Not an audio only local file -> no submission");
        vlc_object_release( p_input );

        vlc_mutex_lock( &p_sys->lock );
        p_sys->b_queued = VLC_TRUE;
        p_sys->b_metadata_read = VLC_TRUE;
        vlc_mutex_unlock( &p_sys->lock );

        return VLC_SUCCESS;
    }

    #define FREE_INPUT_AND_CHARS \
        vlc_object_release( p_input ); \
        free( psz_title ); \
        free( psz_artist ); \
        free( psz_album ); \
        free( psz_trackid );

    #define WAIT_METADATA_FETCHING( a ) \
        if ( b_waiting == VLC_TRUE ) \
        { \
            a = calloc( 1, 1 ); \
        } \
        else \
        { \
            vlc_object_release( p_input ); \
            vlc_mutex_lock( &p_sys->lock ); \
            p_sys->b_waiting_meta = VLC_TRUE; \
            vlc_mutex_unlock( &p_sys->lock ); \
            free( psz_artist ); \
            return VLC_SUCCESS; \
        }

    char *psz_meta;
    #define ALLOC_ITEM_META( a, b ) \
        psz_meta = input_item_Get##b( input_GetItem( p_input ) ); \
        if( psz_meta ) \
        { \
            a = encode_URI_component( psz_meta ); \
            if( !a ) \
            { \
                free( psz_meta ); \
                FREE_INPUT_AND_CHARS \
                return VLC_ENOMEM; \
            } \
            free( psz_meta ); \
        }

    i_status = input_GetItem(p_input)->p_meta->i_status;

    vlc_mutex_lock( &p_sys->lock );
    b_waiting = p_sys->b_waiting_meta;
    vlc_mutex_unlock( &p_sys->lock );

    if( i_status & ( !b_waiting ? ITEM_PREPARSED : ITEM_META_FETCHED ) )
    {
        ALLOC_ITEM_META( psz_artist, Artist )
        else
        {
            msg_Dbg( p_this, "No artist.." );
            WAIT_METADATA_FETCHING( psz_artist )
        }
        psz_meta = input_item_GetTitle( input_GetItem( p_input ) );
        if( psz_meta )
        {
            psz_title = encode_URI_component( psz_meta );
            if( !psz_title )
            {
                free( psz_meta );
                FREE_INPUT_AND_CHARS
                return VLC_ENOMEM;
            }
            free( psz_meta );
        }
        else
        {
            msg_Dbg( p_this, "No track name.." );
            WAIT_METADATA_FETCHING( psz_title );
        }

        ALLOC_ITEM_META( psz_album, Album )
        else
            psz_album = calloc( 1, 1 );

        ALLOC_ITEM_META( psz_trackid, TrackID )
        else
            psz_trackid = calloc( 1, 1 );

        i_length = input_item_GetDuration( input_GetItem( p_input ) ) / 1000000;

        vlc_mutex_lock ( &p_sys->lock );

        p_sys->p_current_song->psz_a    = strdup( psz_artist );
        p_sys->p_current_song->psz_t    = strdup( psz_title );
        p_sys->p_current_song->psz_b    = strdup( psz_album );
        p_sys->p_current_song->psz_m    = strdup( psz_trackid );
        p_sys->p_current_song->i_l      = i_length;
        p_sys->b_queued                 = VLC_FALSE;
        p_sys->b_metadata_read          = VLC_TRUE;

        vlc_mutex_unlock( &p_sys->lock );

        msg_Dbg( p_this, "Meta data registered, waiting to be queued" );
    }
   
    FREE_INPUT_AND_CHARS 
    return VLC_SUCCESS;
#undef FREE_INPUT_AND_CHARS
#undef ALLOC_ITEM_META
#undef WAIT_METADATA_FETCHING
}

/*****************************************************************************
 * audioscrobbler.c : audioscrobbler submission plugin
 *****************************************************************************
 * Copyright © 2006-2011 the VideoLAN team
 * $Id$
 *
 * Author: Rafaël Carré <funman at videolanorg>
 *         Ilkka Ollakka <ileoo at videolan org>
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

/* audioscrobbler protocol version: 1.2
 * http://www.audioscrobbler.net/development/protocol/
 *
 * TODO:    "Now Playing" feature (not mandatory)
 *          Update to new API? http://www.lastfm.fr/api
 */
/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <time.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_interface.h>
#include <vlc_dialog.h>
#include <vlc_meta.h>
#include <vlc_md5.h>
#include <vlc_stream.h>
#include <vlc_url.h>
#include <vlc_network.h>
#include <vlc_playlist.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

#define QUEUE_MAX 50

/* Keeps track of metadata to be submitted */
typedef struct audioscrobbler_song_t
{
    char        *psz_a;             /**< track artist     */
    char        *psz_t;             /**< track title      */
    char        *psz_b;             /**< track album      */
    char        *psz_n;             /**< track number     */
    int         i_l;                /**< track length     */
    char        *psz_m;             /**< musicbrainz id   */
    time_t      date;               /**< date since epoch */
    mtime_t     i_start;            /**< playing start    */
} audioscrobbler_song_t;

struct intf_sys_t
{
    audioscrobbler_song_t   p_queue[QUEUE_MAX]; /**< songs not submitted yet*/
    int                     i_songs;            /**< number of songs        */

    vlc_mutex_t             lock;               /**< p_sys mutex            */
    vlc_cond_t              wait;               /**< song to submit event   */
    vlc_thread_t            thread;             /**< thread to submit song  */

    /* submission of played songs */
    vlc_url_t               p_submit_url;       /**< where to submit data   */

    /* submission of playing song */
#if 0 //NOT USED
    char                    *psz_nowp_host;     /**< where to submit data   */
    int                     i_nowp_port;        /**< port to which submit   */
    char                    *psz_nowp_file;     /**< file to which submit   */
#endif
    char                    psz_auth_token[33]; /**< Authentication token */

    /* data about song currently playing */
    audioscrobbler_song_t   p_current_song;     /**< song being played      */

    mtime_t                 time_pause;         /**< time when vlc paused   */
    mtime_t                 time_total_pauses;  /**< total time in pause    */

    bool                    b_submit;           /**< do we have to submit ? */

    bool                    b_state_cb;         /**< if we registered the
                                                 * "state" callback         */

    bool                    b_meta_read;        /**< if we read the song's
                                                 * metadata already         */
};

static int  Open            (vlc_object_t *);
static void Close           (vlc_object_t *);
static void *Run            (void *);

/*****************************************************************************
 * Module descriptor
 ****************************************************************************/

#define USERNAME_TEXT       N_("Username")
#define USERNAME_LONGTEXT   N_("The username of your last.fm account")
#define PASSWORD_TEXT       N_("Password")
#define PASSWORD_LONGTEXT   N_("The password of your last.fm account")
#define URL_TEXT            N_("Scrobbler URL")
#define URL_LONGTEXT        N_("The URL set for an alternative scrobbler engine")

/* This error value is used when last.fm plugin has to be unloaded. */
#define VLC_AUDIOSCROBBLER_EFATAL -69

/* last.fm client identifier */
#define CLIENT_NAME     PACKAGE
#define CLIENT_VERSION  VERSION

vlc_module_begin ()
    set_category(CAT_INTERFACE)
    set_subcategory(SUBCAT_INTERFACE_CONTROL)
    set_shortname(N_("Audioscrobbler"))
    set_description(N_("Submission of played songs to last.fm"))
    add_string("lastfm-username", "",
                USERNAME_TEXT, USERNAME_LONGTEXT, false)
    add_password("lastfm-password", "",
                PASSWORD_TEXT, PASSWORD_LONGTEXT, false)
    add_string("scrobbler-url", "post.audioscrobbler.com",
                URL_TEXT, URL_LONGTEXT, false)
    set_capability("interface", 0)
    set_callbacks(Open, Close)
vlc_module_end ()

/*****************************************************************************
 * DeleteSong : Delete the char pointers in a song
 *****************************************************************************/
static void DeleteSong(audioscrobbler_song_t* p_song)
{
    FREENULL(p_song->psz_a);
    FREENULL(p_song->psz_b);
    FREENULL(p_song->psz_t);
    FREENULL(p_song->psz_m);
    FREENULL(p_song->psz_n);
}

/*****************************************************************************
 * ReadMetaData : Read meta data when parsed by vlc
 *****************************************************************************/
static void ReadMetaData(intf_thread_t *p_this)
{
    input_thread_t      *p_input;
    input_item_t        *p_item;

    intf_sys_t          *p_sys = p_this->p_sys;

    p_input = playlist_CurrentInput(pl_Get(p_this));
    if (!p_input)
        return;

    p_item = input_GetItem(p_input);
    if (!p_item)
    {
        vlc_object_release(p_input);
        return;
    }

#define ALLOC_ITEM_META(a, b) do { \
        char *psz_meta = input_item_Get##b(p_item); \
        if (psz_meta && *psz_meta) \
            a = encode_URI_component(psz_meta); \
        free(psz_meta); \
    } while (0)

    vlc_mutex_lock(&p_sys->lock);

    p_sys->b_meta_read = true;

    ALLOC_ITEM_META(p_sys->p_current_song.psz_a, Artist);
    if (!p_sys->p_current_song.psz_a)
    {
        msg_Dbg(p_this, "No artist..");
        DeleteSong(&p_sys->p_current_song);
        goto end;
    }

    ALLOC_ITEM_META(p_sys->p_current_song.psz_t, Title);
    if (!p_sys->p_current_song.psz_t)
    {
        msg_Dbg(p_this, "No track name..");
        DeleteSong(&p_sys->p_current_song);
        goto end;
    }

    /* Now we have read the mandatory meta data, so we can submit that info */
    p_sys->b_submit = true;

    ALLOC_ITEM_META(p_sys->p_current_song.psz_b, Album);
    if (!p_sys->p_current_song.psz_b)
        p_sys->p_current_song.psz_b = calloc(1, 1);

    ALLOC_ITEM_META(p_sys->p_current_song.psz_m, TrackID);
    if (!p_sys->p_current_song.psz_m)
        p_sys->p_current_song.psz_m = calloc(1, 1);

    p_sys->p_current_song.i_l = input_item_GetDuration(p_item) / 1000000;

    ALLOC_ITEM_META(p_sys->p_current_song.psz_n, TrackNum);
    if (!p_sys->p_current_song.psz_n)
        p_sys->p_current_song.psz_n = calloc(1, 1);
#undef ALLOC_ITEM_META

    msg_Dbg(p_this, "Meta data registered");

end:
    vlc_mutex_unlock(&p_sys->lock);
    vlc_object_release(p_input);
}

/*****************************************************************************
 * AddToQueue: Add the played song to the queue to be submitted
 *****************************************************************************/
static void AddToQueue (intf_thread_t *p_this)
{
    mtime_t                     played_time;
    intf_sys_t                  *p_sys = p_this->p_sys;

    vlc_mutex_lock(&p_sys->lock);
    if (!p_sys->b_submit)
        goto end;

    /* wait for the user to listen enough before submitting */
    played_time = mdate() - p_sys->p_current_song.i_start -
                            p_sys->time_total_pauses;
    played_time /= 1000000; /* µs → s */

    /*HACK: it seam that the preparsing sometime fail,
            so use the playing time as the song length */
    if (p_sys->p_current_song.i_l == 0)
        p_sys->p_current_song.i_l = played_time;

    /* Don't send song shorter than 30s */
    if (p_sys->p_current_song.i_l < 30)
    {
        msg_Dbg(p_this, "Song too short (< 30s), not submitting");
        goto end;
    }

    /* Send if the user had listen more than 240s OR half the track length */
    if ((played_time < 240) &&
        (played_time < (p_sys->p_current_song.i_l / 2)))
    {
        msg_Dbg(p_this, "Song not listened long enough, not submitting");
        goto end;
    }

    /* Check that all meta are present */
    if (!p_sys->p_current_song.psz_a || !*p_sys->p_current_song.psz_a ||
        !p_sys->p_current_song.psz_t || !*p_sys->p_current_song.psz_t)
    {
        msg_Dbg(p_this, "Missing artist or title, not submitting");
        goto end;
    }

    if (p_sys->i_songs >= QUEUE_MAX)
    {
        msg_Warn(p_this, "Submission queue is full, not submitting");
        goto end;
    }

    msg_Dbg(p_this, "Song will be submitted.");

#define QUEUE_COPY(a) \
    p_sys->p_queue[p_sys->i_songs].a = p_sys->p_current_song.a

#define QUEUE_COPY_NULL(a) \
    QUEUE_COPY(a); \
    p_sys->p_current_song.a = NULL

    QUEUE_COPY(i_l);
    QUEUE_COPY_NULL(psz_n);
    QUEUE_COPY_NULL(psz_a);
    QUEUE_COPY_NULL(psz_t);
    QUEUE_COPY_NULL(psz_b);
    QUEUE_COPY_NULL(psz_m);
    QUEUE_COPY(date);
#undef QUEUE_COPY_NULL
#undef QUEUE_COPY

    p_sys->i_songs++;

    /* signal the main loop we have something to submit */
    vlc_cond_signal(&p_sys->wait);

end:
    DeleteSong(&p_sys->p_current_song);
    p_sys->b_submit = false;
    vlc_mutex_unlock(&p_sys->lock);
}

/*****************************************************************************
 * PlayingChange: Playing status change callback
 *****************************************************************************/
static int PlayingChange(vlc_object_t *p_this, const char *psz_var,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data)
{
    VLC_UNUSED(oldval);

    intf_thread_t   *p_intf = (intf_thread_t*) p_data;
    intf_sys_t      *p_sys  = p_intf->p_sys;
    input_thread_t  *p_input = (input_thread_t*)p_this;
    int             state;

    VLC_UNUSED(psz_var);

    if (newval.i_int != INPUT_EVENT_STATE) return VLC_SUCCESS;

    if (var_CountChoices(p_input, "video-es"))
    {
        msg_Dbg(p_this, "Not an audio-only input, not submitting");
        return VLC_SUCCESS;
    }

    state = var_GetInteger(p_input, "state");

    if (!p_sys->b_meta_read && state >= PLAYING_S)
    {
        ReadMetaData(p_intf);
        return VLC_SUCCESS;
    }


    if (state >= END_S)
        AddToQueue(p_intf);
    else if (state == PAUSE_S)
        p_sys->time_pause = mdate();
    else if (p_sys->time_pause > 0 && state == PLAYING_S)
    {
        p_sys->time_total_pauses += (mdate() - p_sys->time_pause);
        p_sys->time_pause = 0;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * ItemChange: Playlist item change callback
 *****************************************************************************/
static int ItemChange(vlc_object_t *p_this, const char *psz_var,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data)
{
    input_thread_t      *p_input;
    intf_thread_t       *p_intf     = (intf_thread_t*) p_data;
    intf_sys_t          *p_sys      = p_intf->p_sys;
    input_item_t        *p_item;

    VLC_UNUSED(p_this); VLC_UNUSED(psz_var);
    VLC_UNUSED(oldval); VLC_UNUSED(newval);

    p_sys->b_state_cb       = false;
    p_sys->b_meta_read      = false;
    p_sys->b_submit         = false;

    p_input = playlist_CurrentInput(pl_Get(p_intf));

    if (!p_input || p_input->b_dead)
        return VLC_SUCCESS;

    p_item = input_GetItem(p_input);
    if (!p_item)
    {
        vlc_object_release(p_input);
        return VLC_SUCCESS;
    }

    if (var_CountChoices(p_input, "video-es"))
    {
        msg_Dbg(p_this, "Not an audio-only input, not submitting");
        vlc_object_release(p_input);
        return VLC_SUCCESS;
    }

    p_sys->time_total_pauses = 0;
    time(&p_sys->p_current_song.date);        /* to be sent to last.fm */
    p_sys->p_current_song.i_start = mdate();    /* only used locally */

    var_AddCallback(p_input, "intf-event", PlayingChange, p_intf);
    p_sys->b_state_cb = true;

    if (input_item_IsPreparsed(p_item))
        ReadMetaData(p_intf);
    /* if the input item was not preparsed, we'll do it in PlayingChange()
     * callback, when "state" == PLAYING_S */

    vlc_object_release(p_input);
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Open: initialize and create stuff
 *****************************************************************************/
static int Open(vlc_object_t *p_this)
{
    intf_thread_t   *p_intf     = (intf_thread_t*) p_this;
    intf_sys_t      *p_sys      = calloc(1, sizeof(intf_sys_t));

    if (!p_sys)
        return VLC_ENOMEM;

    p_intf->p_sys = p_sys;

    vlc_mutex_init(&p_sys->lock);
    vlc_cond_init(&p_sys->wait);

    if (vlc_clone(&p_sys->thread, Run, p_intf, VLC_THREAD_PRIORITY_LOW))
    {
        vlc_cond_destroy(&p_sys->wait);
        vlc_mutex_destroy(&p_sys->lock);
        free(p_sys);
        return VLC_ENOMEM;
    }

    var_AddCallback(pl_Get(p_intf), "activity", ItemChange, p_intf);

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: destroy interface stuff
 *****************************************************************************/
static void Close(vlc_object_t *p_this)
{
    playlist_t                  *p_playlist = pl_Get(p_this);
    input_thread_t              *p_input;
    intf_thread_t               *p_intf = (intf_thread_t*) p_this;
    intf_sys_t                  *p_sys  = p_intf->p_sys;

    var_DelCallback(p_playlist, "activity", ItemChange, p_intf);

    vlc_cancel(p_sys->thread);
    vlc_join(p_sys->thread, NULL);

    p_input = playlist_CurrentInput(p_playlist);
    if (p_input)
    {
        if (p_sys->b_state_cb)
            var_DelCallback(p_input, "intf-event", PlayingChange, p_intf);
        vlc_object_release(p_input);
    }

    int i;
    for (i = 0; i < p_sys->i_songs; i++)
        DeleteSong(&p_sys->p_queue[i]);
    vlc_UrlClean(&p_sys->p_submit_url);
#if 0 //NOT USED
    free(p_sys->psz_nowp_host);
    free(p_sys->psz_nowp_file);
#endif
    vlc_cond_destroy(&p_sys->wait);
    vlc_mutex_destroy(&p_sys->lock);
    free(p_sys);
}

/*****************************************************************************
 * Handshake : Init audioscrobbler connection
 *****************************************************************************/
static int Handshake(intf_thread_t *p_this)
{
    char                *psz_username, *psz_password;
    char                *psz_scrobbler_url;
    time_t              timestamp;
    char                psz_timestamp[21];

    struct md5_s        p_struct_md5;

    stream_t            *p_stream;
    char                *psz_handshake_url;
    uint8_t             p_buffer[1024];
    char                *p_buffer_pos;

    int                 i_ret;
    char                *psz_url;

    intf_thread_t       *p_intf                 = (intf_thread_t*) p_this;
    intf_sys_t          *p_sys                  = p_this->p_sys;

    psz_username = var_InheritString(p_this, "lastfm-username");
    if (!psz_username)
        return VLC_ENOMEM;

    psz_password = var_InheritString(p_this, "lastfm-password");
    if (!psz_password)
    {
        free(psz_username);
        return VLC_ENOMEM;
    }

    /* username or password have not been setup */
    if (!*psz_username || !*psz_password)
    {
        free(psz_username);
        free(psz_password);
        return VLC_ENOVAR;
    }

    time(&timestamp);

    /* generates a md5 hash of the password */
    InitMD5(&p_struct_md5);
    AddMD5(&p_struct_md5, (uint8_t*) psz_password, strlen(psz_password));
    EndMD5(&p_struct_md5);

    free(psz_password);

    char *psz_password_md5 = psz_md5_hash(&p_struct_md5);
    if (!psz_password_md5)
    {
        free(psz_username);
        return VLC_ENOMEM;
    }

    snprintf(psz_timestamp, sizeof(psz_timestamp), "%"PRIu64,
              (uint64_t)timestamp);

    /* generates a md5 hash of :
     * - md5 hash of the password, plus
     * - timestamp in clear text
     */
    InitMD5(&p_struct_md5);
    AddMD5(&p_struct_md5, (uint8_t*) psz_password_md5, 32);
    AddMD5(&p_struct_md5, (uint8_t*) psz_timestamp, strlen(psz_timestamp));
    EndMD5(&p_struct_md5);
    free(psz_password_md5);

    char *psz_auth_token = psz_md5_hash(&p_struct_md5);
    if (!psz_auth_token)
    {
        free(psz_username);
        return VLC_ENOMEM;
    }

    psz_scrobbler_url = var_InheritString(p_this, "scrobbler-url");
    if (!psz_scrobbler_url)
    {
        free(psz_username);
        return VLC_ENOMEM;
    }

    i_ret = asprintf(&psz_handshake_url,
    "http://%s/?hs=true&p=1.2&c="CLIENT_NAME"&v="CLIENT_VERSION"&u=%s&t=%s&a=%s"
    , psz_scrobbler_url, psz_username, psz_timestamp, psz_auth_token);

    free(psz_scrobbler_url);
    free(psz_username);
    if (i_ret == -1)
        return VLC_ENOMEM;

    /* send the http handshake request */
    p_stream = stream_UrlNew(p_intf, psz_handshake_url);
    free(psz_handshake_url);

    if (!p_stream)
        return VLC_EGENERIC;

    /* read answer */
    i_ret = stream_Read(p_stream, p_buffer, sizeof(p_buffer) - 1);
    if (i_ret == 0)
    {
        stream_Delete(p_stream);
        return VLC_EGENERIC;
    }
    p_buffer[i_ret] = '\0';
    stream_Delete(p_stream);

    p_buffer_pos = strstr((char*) p_buffer, "FAILED ");
    if (p_buffer_pos)
    {
        /* handshake request failed, sorry */
        msg_Err(p_this, "last.fm handshake failed: %s", p_buffer_pos + 7);
        return VLC_EGENERIC;
    }

    if (strstr((char*) p_buffer, "BADAUTH"))
    {
        /* authentication failed, bad username/password combination */
        dialog_Fatal(p_this,
            _("last.fm: Authentication failed"),
            "%s", _("last.fm username or password is incorrect. "
              "Please verify your settings and relaunch VLC."));
        return VLC_AUDIOSCROBBLER_EFATAL;
    }

    if (strstr((char*) p_buffer, "BANNED"))
    {
        /* oops, our version of vlc has been banned by last.fm servers */
        msg_Err(p_intf, "This version of VLC has been banned by last.fm. "
                         "You should upgrade VLC, or disable the last.fm plugin.");
        return VLC_AUDIOSCROBBLER_EFATAL;
    }

    if (strstr((char*) p_buffer, "BADTIME"))
    {
        /* The system clock isn't good */
        msg_Err(p_intf, "last.fm handshake failed because your clock is too "
                         "much shifted. Please correct it, and relaunch VLC.");
        return VLC_AUDIOSCROBBLER_EFATAL;
    }

    p_buffer_pos = strstr((char*) p_buffer, "OK");
    if (!p_buffer_pos)
        goto proto;

    p_buffer_pos = strstr(p_buffer_pos, "\n");
    if (!p_buffer_pos || strlen(p_buffer_pos) < 33)
        goto proto;
    p_buffer_pos++; /* we skip the '\n' */

    /* save the session ID */
    memcpy(p_sys->psz_auth_token, p_buffer_pos, 32);
    p_sys->psz_auth_token[32] = '\0';

    p_buffer_pos = strstr(p_buffer_pos, "http://");
    if (!p_buffer_pos || strlen(p_buffer_pos) == 7)
        goto proto;

    /* We need to read the nowplaying url */
    p_buffer_pos += 7; /* we skip "http://" */
#if 0 //NOT USED
    psz_url = strndup(p_buffer_pos, strcspn(p_buffer_pos, "\n"));
    if (!psz_url)
        goto oom;

    switch(ParseURL(psz_url, &p_sys->psz_nowp_host,
                &p_sys->psz_nowp_file, &p_sys->i_nowp_port))
    {
        case VLC_ENOMEM:
            goto oom;
        case VLC_EGENERIC:
            goto proto;
        case VLC_SUCCESS:
        default:
            break;
    }
#endif
    p_buffer_pos = strstr(p_buffer_pos, "http://");
    if (!p_buffer_pos || strlen(p_buffer_pos) == 7)
        goto proto;

    /* We need to read the submission url */
    p_buffer_pos += 7; /* we skip "http://" */
    psz_url = strndup(p_buffer_pos, strcspn(p_buffer_pos, "\n"));
    if (!psz_url)
        goto oom;

    /* parse the submission url */
    vlc_UrlParse(&p_sys->p_submit_url, psz_url, 0);
    free(psz_url);

    return VLC_SUCCESS;

oom:
    return VLC_ENOMEM;

proto:
    msg_Err(p_intf, "Handshake: can't recognize server protocol");
    return VLC_EGENERIC;
}

static void HandleInterval(mtime_t *next, unsigned int *i_interval)
{
    if (*i_interval == 0)
    {
        /* first interval is 1 minute */
        *i_interval = 1;
    }
    else
    {
        /* else we double the previous interval, up to 120 minutes */
        *i_interval <<= 1;
        if (*i_interval > 120)
            *i_interval = 120;
    }
    *next = mdate() + (*i_interval * 1000000 * 60);
}

/*****************************************************************************
 * Run : call Handshake() then submit songs
 *****************************************************************************/
static void *Run(void *data)
{
    intf_thread_t          *p_intf = data;
    uint8_t                 p_buffer[1024];
    int                     canc = vlc_savecancel();
    bool                    b_handshaked = false;

    /* data about audioscrobbler session */
    mtime_t                 next_exchange = -1; /**< when can we send data  */
    unsigned int            i_interval;         /**< waiting interval (secs)*/

    intf_sys_t *p_sys = p_intf->p_sys;

    /* main loop */
    for (;;)
    {
        vlc_restorecancel(canc);
        vlc_mutex_lock(&p_sys->lock);
        mutex_cleanup_push(&p_sys->lock);

        do
            vlc_cond_wait(&p_sys->wait, &p_sys->lock);
        while (mdate() < next_exchange);

        vlc_cleanup_run();
        canc = vlc_savecancel();

        /* handshake if needed */
        if (!b_handshaked)
        {
            msg_Dbg(p_intf, "Handshaking with last.fm ...");

            switch(Handshake(p_intf))
            {
                case VLC_ENOMEM:
                    goto out;

                case VLC_ENOVAR:
                    /* username not set */
                    dialog_Fatal(p_intf,
                        _("Last.fm username not set"),
                        "%s", _("Please set a username or disable the "
                        "audioscrobbler plugin, and restart VLC.\n"
                        "Visit http://www.last.fm/join/ to get an account."));
                    goto out;

                case VLC_SUCCESS:
                    msg_Dbg(p_intf, "Handshake successful :)");
                    b_handshaked = true;
                    i_interval = 0;
                    next_exchange = mdate();
                    break;

                case VLC_AUDIOSCROBBLER_EFATAL:
                    msg_Warn(p_intf, "Exiting...");
                    goto out;

                case VLC_EGENERIC:
                default:
                    /* protocol error : we'll try later */
                    HandleInterval(&next_exchange, &i_interval);
                    break;
            }
            /* if handshake failed let's restart the loop */
            if (!b_handshaked)
                continue;
        }

        msg_Dbg(p_intf, "Going to submit some data...");
        char *psz_submit;
        if (asprintf(&psz_submit, "s=%s", p_sys->psz_auth_token) == -1)
            break;

        /* forge the HTTP POST request */
        vlc_mutex_lock(&p_sys->lock);
        audioscrobbler_song_t *p_song;
        for (int i_song = 0 ; i_song < p_sys->i_songs ; i_song++)
        {
            char *psz_submit_song, *psz_submit_tmp;
            p_song = &p_sys->p_queue[i_song];
            if (asprintf(&psz_submit_song,
                    "&a%%5B%d%%5D=%s"
                    "&t%%5B%d%%5D=%s"
                    "&i%%5B%d%%5D=%u"
                    "&o%%5B%d%%5D=P"
                    "&r%%5B%d%%5D="
                    "&l%%5B%d%%5D=%d"
                    "&b%%5B%d%%5D=%s"
                    "&n%%5B%d%%5D=%s"
                    "&m%%5B%d%%5D=%s",
                    i_song, p_song->psz_a,
                    i_song, p_song->psz_t,
                    i_song, (unsigned)p_song->date, /* HACK: %ju (uintmax_t) unsupported on Windows */
                    i_song,
                    i_song,
                    i_song, p_song->i_l,
                    i_song, p_song->psz_b,
                    i_song, p_song->psz_n,
                    i_song, p_song->psz_m
           ) == -1)
            {   /* Out of memory */
                vlc_mutex_unlock(&p_sys->lock);
                goto out;
            }
            psz_submit_tmp = psz_submit;
            if (asprintf(&psz_submit, "%s%s",
                    psz_submit_tmp, psz_submit_song) == -1)
            {   /* Out of memory */
                free(psz_submit_tmp);
                free(psz_submit_song);
                vlc_mutex_unlock(&p_sys->lock);
                goto out;
            }
            free(psz_submit_song);
            free(psz_submit_tmp);
        }
        vlc_mutex_unlock(&p_sys->lock);

        int i_post_socket = net_ConnectTCP(p_intf, p_sys->p_submit_url.psz_host,
                                        p_sys->p_submit_url.i_port);

        if (i_post_socket == -1)
        {
            /* If connection fails, we assume we must handshake again */
            HandleInterval(&next_exchange, &i_interval);
            b_handshaked = false;
            free(psz_submit);
            continue;
        }

        /* we transmit the data */
        int i_net_ret = net_Printf(p_intf, i_post_socket, NULL,
            "POST %s HTTP/1.1\n"
            "Accept-Encoding: identity\n"
            "Content-length: %zu\n"
            "Connection: close\n"
            "Content-type: application/x-www-form-urlencoded\n"
            "Host: %s\n"
            "User-agent: VLC media player/"VERSION"\r\n"
            "\r\n"
            "%s\r\n"
            "\r\n",
            p_sys->p_submit_url.psz_path, strlen(psz_submit),
            p_sys->p_submit_url.psz_host, psz_submit
       );

        free(psz_submit);
        if (i_net_ret == -1)
        {
            /* If connection fails, we assume we must handshake again */
            HandleInterval(&next_exchange, &i_interval);
            b_handshaked = false;
            continue;
        }

        i_net_ret = net_Read(p_intf, i_post_socket, NULL,
                    p_buffer, sizeof(p_buffer) - 1, false);
        if (i_net_ret <= 0)
        {
            /* if we get no answer, something went wrong : try again */
            continue;
        }

        net_Close(i_post_socket);
        p_buffer[i_net_ret] = '\0';

        char *failed = strstr((char *) p_buffer, "FAILED");
        if (failed)
        {
            msg_Warn(p_intf, "%s", failed);
            HandleInterval(&next_exchange, &i_interval);
            continue;
        }

        if (strstr((char *) p_buffer, "BADSESSION"))
        {
            msg_Err(p_intf, "Authentication failed (BADSESSION), are you connected to last.fm with another program ?");
            b_handshaked = false;
            HandleInterval(&next_exchange, &i_interval);
            continue;
        }

        if (strstr((char *) p_buffer, "OK"))
        {
            for (int i = 0; i < p_sys->i_songs; i++)
                DeleteSong(&p_sys->p_queue[i]);
            p_sys->i_songs = 0;
            i_interval = 0;
            next_exchange = mdate();
            msg_Dbg(p_intf, "Submission successful!");
        }
        else
        {
            msg_Err(p_intf, "Authentication failed, handshaking again (%s)",
                             p_buffer);
            b_handshaked = false;
            HandleInterval(&next_exchange, &i_interval);
        }
    }
out:
    vlc_restorecancel(canc);
    return NULL;
}

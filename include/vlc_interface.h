/*****************************************************************************
 * vlc_interface.h: interface access for other threads
 * This library provides basic functions for threads to interact with user
 * interface, such as message output.
 *****************************************************************************
 * Copyright (C) 1999, 2000 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
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

#ifndef VLC_INTF_H_
#define VLC_INTF_H_

# ifdef __cplusplus
extern "C" {
# endif

typedef struct intf_dialog_args_t intf_dialog_args_t;

/**
 * \defgroup interface Interface
 * VLC user interfaces
 * @{
 * \file
 * VLC user interface modules
 */

typedef struct intf_sys_t intf_sys_t;

/** Describe all interface-specific data of the interface thread */
typedef struct intf_thread_t
{
    VLC_COMMON_MEMBERS

    struct intf_thread_t *p_next; /** LibVLC interfaces book keeping */

    /* Specific interfaces */
    intf_sys_t *        p_sys;                          /** system interface */

    /** Interface module */
    module_t *   p_module;

    /** Specific for dialogs providers */
    void ( *pf_show_dialog ) ( struct intf_thread_t *, int, int,
                               intf_dialog_args_t * );

    config_chain_t *p_cfg;
} intf_thread_t;

/** \brief Arguments passed to a dialogs provider
 *  This describes the arguments passed to the dialogs provider. They are
 *  mainly used with INTF_DIALOG_FILE_GENERIC.
 */
struct intf_dialog_args_t
{
    intf_thread_t *p_intf;
    char *psz_title;

    char **psz_results;
    int  i_results;

    void (*pf_callback) ( intf_dialog_args_t * );
    void *p_arg;

    /* Specifically for INTF_DIALOG_FILE_GENERIC */
    char *psz_extensions;
    bool b_save;
    bool b_multiple;

    /* Specific to INTF_DIALOG_INTERACTION */
    struct interaction_dialog_t *p_dialog;
};

VLC_API int intf_Create( playlist_t *, const char * );

VLC_API void libvlc_Quit( libvlc_int_t * );

static inline playlist_t *pl_Get( struct intf_thread_t *intf )
{
    return (playlist_t *)(intf->obj.parent);
}

/**
 * Retrieves the current input thread from the playlist.
 * @note The returned object must be released with vlc_object_release().
 */
#define pl_CurrentInput(intf) (playlist_CurrentInput(pl_Get(intf)))

/**
 * @ingroup messages
 * @{
 */

VLC_API void vlc_LogSet(libvlc_int_t *, vlc_log_cb cb, void *data);

/*@}*/

/* Interface dialog ids for dialog providers */
typedef enum vlc_intf_dialog {
    INTF_DIALOG_FILE_SIMPLE = 1,
    INTF_DIALOG_FILE,
    INTF_DIALOG_DISC,
    INTF_DIALOG_NET,
    INTF_DIALOG_CAPTURE,
    INTF_DIALOG_SAT,
    INTF_DIALOG_DIRECTORY,

    INTF_DIALOG_STREAMWIZARD,
    INTF_DIALOG_WIZARD,

    INTF_DIALOG_PLAYLIST,
    INTF_DIALOG_MESSAGES,
    INTF_DIALOG_FILEINFO,
    INTF_DIALOG_PREFS,
    INTF_DIALOG_BOOKMARKS,
    INTF_DIALOG_EXTENDED,
    INTF_DIALOG_RENDERER,

    INTF_DIALOG_POPUPMENU = 20,
    INTF_DIALOG_AUDIOPOPUPMENU,
    INTF_DIALOG_VIDEOPOPUPMENU,
    INTF_DIALOG_MISCPOPUPMENU,

    INTF_DIALOG_FILE_GENERIC = 30,
    INTF_DIALOG_INTERACTION = 50,
    INTF_DIALOG_SENDKEY = 51,

    INTF_DIALOG_UPDATEVLC = 90,
    INTF_DIALOG_VLM,

    INTF_DIALOG_EXIT = 99
} vlc_intf_dialog;

/* Useful text messages shared by interfaces */
#define INTF_ABOUT_MSG LICENSE_MSG

#define EXTENSIONS_AUDIO_CSV "3ga", "669", "a52", "aac", "ac3", "adt", "adts", "aif", "aifc", "aiff", \
                         "amb", "amr", "aob", "ape", "au", "awb", "caf", "dts", "flac", "it", "kar", \
                         "m4a", "m4b", "m4p", "m5p", "mka", "mlp", "mod", "mpa", "mp1", "mp2", "mp3", "mpc", "mpga", "mus", \
                         "oga", "ogg", "oma", "opus", "qcp", "ra", "rmi", "s3m", "sid", "spx", "tak", "thd", "tta", \
                         "voc", "vqf", "w64", "wav", "wma", "wv", "xa", "xm"

#define EXTENSIONS_VIDEO_CSV "3g2", "3gp", "3gp2", "3gpp", "amv", "asf", "avi", "bik", "crf", "dav", "divx", "drc", "dv", "dvr-ms" \
                             "evo", "f4v", "flv", "gvi", "gxf", "iso", \
                             "m1v", "m2v", "m2t", "m2ts", "m4v", "mkv", "mov",\
                             "mp2", "mp2v", "mp4", "mp4v", "mpe", "mpeg", "mpeg1", \
                             "mpeg2", "mpeg4", "mpg", "mpv2", "mts", "mtv", "mxf", "mxg", "nsv", "nuv", \
                             "ogg", "ogm", "ogv", "ogx", "ps", \
                             "rec", "rm", "rmvb", "rpl", "thp", "tod", "ts", "tts", "txd", "vob", "vro", \
                             "webm", "wm", "wmv", "wtv", "xesc"

#define EXTENSIONS_AUDIO \
    "*.3ga;" \
    "*.669;" \
    "*.a52;" \
    "*.aac;" \
    "*.ac3;" \
    "*.adt;" \
    "*.adts;" \
    "*.aif;"\
    "*.aifc;"\
    "*.aiff;"\
    "*.amb;" \
    "*.amr;" \
    "*.aob;" \
    "*.ape;" \
    "*.au;" \
    "*.awb;" \
    "*.caf;" \
    "*.dts;" \
    "*.flac;"\
    "*.it;"  \
    "*.kar;" \
    "*.m4a;" \
    "*.m4b;" \
    "*.m4p;" \
    "*.m5p;" \
    "*.mid;" \
    "*.mka;" \
    "*.mlp;" \
    "*.mod;" \
    "*.mpa;" \
    "*.mp1;" \
    "*.mp2;" \
    "*.mp3;" \
    "*.mpc;" \
    "*.mpga;" \
    "*.mus;" \
    "*.oga;" \
    "*.ogg;" \
    "*.oma;" \
    "*.opus;" \
    "*.qcp;" \
    "*.ra;" \
    "*.rmi;" \
    "*.s3m;" \
    "*.sid;" \
    "*.spx;" \
    "*.tak;" \
    "*.thd;" \
    "*.tta;" \
    "*.voc;" \
    "*.vqf;" \
    "*.w64;" \
    "*.wav;" \
    "*.wma;" \
    "*.wv;"  \
    "*.xa;"  \
    "*.xm"

#define EXTENSIONS_VIDEO "*.3g2;*.3gp;*.3gp2;*.3gpp;*.amv;*.asf;*.avi;*.bik;*.bin;*.crf;*.dav;*.divx;*.drc;*.dv;*.dvr-ms;*.evo;*.f4v;*.flv;*.gvi;*.gxf;*.iso;*.m1v;*.m2v;" \
                         "*.m2t;*.m2ts;*.m4v;*.mkv;*.mov;*.mp2;*.mp2v;*.mp4;*.mp4v;*.mpe;*.mpeg;*.mpeg1;" \
                         "*.mpeg2;*.mpeg4;*.mpg;*.mpv2;*.mts;*.mtv;*.mxf;*.mxg;*.nsv;*.nuv;" \
                         "*.ogg;*.ogm;*.ogv;*.ogx;*.ps;" \
                         "*.rec;*.rm;*.rmvb;*.rpl;*.thp;*.tod;*.tp;*.ts;*.tts;*.txd;*.vob;*.vro;*.webm;*.wm;*.wmv;*.wtv;*.xesc"

#define EXTENSIONS_PLAYLIST "*.asx;*.b4s;*.cue;*.ifo;*.m3u;*.m3u8;*.pls;*.ram;*.rar;*.sdp;*.vlc;*.xspf;*.wax;*.wvx;*.zip;*.conf"

#define EXTENSIONS_MEDIA EXTENSIONS_VIDEO ";" EXTENSIONS_AUDIO ";" \
                          EXTENSIONS_PLAYLIST

#define EXTENSIONS_SUBTITLE "*.cdg;*.idx;*.srt;" \
                            "*.sub;*.utf;*.ass;" \
                            "*.ssa;*.aqt;" \
                            "*.jss;*.psb;" \
                            "*.rt;*.sami;*.smi;*.txt;" \
                            "*.smil;*.stl;*.usf;" \
                            "*.dks;*.pjs;*.mpl2;*.mks;" \
                            "*.vtt;*.tt;*.ttml;*.dfxp;" \
                            "*.scc"

/** \defgroup interaction Interaction
 * \ingroup interface
 * Interaction between user and modules
 * @{
 */

/**
 * This structure describes a piece of interaction with the user
 */
typedef struct interaction_dialog_t
{
    int             i_type;             ///< Type identifier
    char           *psz_title;          ///< Title
    char           *psz_description;    ///< Descriptor string
    char           *psz_default_button;  ///< default button title (~OK)
    char           *psz_alternate_button;///< alternate button title (~NO)
    /// other button title (optional,~Cancel)
    char           *psz_other_button;

    char           *psz_returned[1];    ///< returned responses from the user

    vlc_value_t     val;                ///< value coming from core for dialogue
    int             i_timeToGo;         ///< time (in sec) until shown progress is finished
    bool      b_cancelled;        ///< was the dialogue cancelled ?

    void *          p_private;          ///< Private interface data

    int             i_status;           ///< Dialog status;
    int             i_action;           ///< Action to perform;
    int             i_flags;            ///< Misc flags
    int             i_return;           ///< Return status

    vlc_object_t   *p_parent;           ///< The vlc object that asked
                                        //for interaction
    intf_thread_t  *p_interface;
    vlc_mutex_t    *p_lock;
} interaction_dialog_t;

/**
 * Possible flags . Dialog types
 */
#define DIALOG_GOT_ANSWER           0x01
#define DIALOG_YES_NO_CANCEL        0x02
#define DIALOG_LOGIN_PW_OK_CANCEL   0x04
#define DIALOG_PSZ_INPUT_OK_CANCEL  0x08
#define DIALOG_BLOCKING_ERROR       0x10
#define DIALOG_NONBLOCKING_ERROR    0x20
#define DIALOG_USER_PROGRESS        0x80
#define DIALOG_INTF_PROGRESS        0x100

/** Possible return codes */
enum
{
    DIALOG_OK_YES,
    DIALOG_NO,
    DIALOG_CANCELLED
};

/** Possible status  */
enum
{
    ANSWERED_DIALOG,            ///< Got "answer"
    DESTROYED_DIALOG,           ///< Interface has destroyed it
};

/** Possible actions */
enum
{
    INTERACT_NEW,
    INTERACT_UPDATE,
    INTERACT_HIDE,
    INTERACT_DESTROY
};

#define intf_UserStringInput( a, b, c, d ) (VLC_OBJECT(a),b,c,d, VLC_EGENERIC)
#define interaction_Register( t ) (t, VLC_EGENERIC)
#define interaction_Unregister( t ) (t, VLC_EGENERIC)


/** @} */
/** @} */

# ifdef __cplusplus
}
# endif
#endif

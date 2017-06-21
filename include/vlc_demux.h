/*****************************************************************************
 * vlc_demux.h: Demuxer descriptor, queries and methods
 *****************************************************************************
 * Copyright (C) 1999-2005 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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

#ifndef VLC_DEMUX_H
#define VLC_DEMUX_H 1

#include <stdlib.h>
#include <string.h>

#include <vlc_es.h>
#include <vlc_stream.h>
#include <vlc_es_out.h>

/**
 * \defgroup demux Demultiplexer
 * \ingroup input
 * Demultiplexers (file format parsers)
 * @{
 * \file
 * Demultiplexer modules interface
 */

struct demux_t
{
    VLC_COMMON_MEMBERS

    /* Module properties */
    module_t    *p_module;

    /* eg informative but needed (we can have access+demux) */
    char        *psz_access;
    char        *psz_demux;
    char        *psz_location;
    char        *psz_file;

    union {
        /**
         * Input stream
         *
         * Depending on the module capability:
         * - "demux": input byte stream (not NULL)
         * - "access_demux": a NULL pointer
         * - "demux_filter": undefined
         */
        stream_t *s;
        /**
         * Input demuxer
         *
         * If the module capability is "demux_filter", this is the upstream
         * demuxer or demux filter. Otherwise, this is undefined.
         */
        demux_t *p_next;
    };

    /* es output */
    es_out_t    *out;   /* our p_es_out */

    bool         b_preparsing; /* True if the demux is used to preparse */

    /* set by demuxer */
    int (*pf_demux)  ( demux_t * );   /* demux one frame only */
    int (*pf_control)( demux_t *, int i_query, va_list args);

    /* Demux has to maintain them uptodate
     * when it is responsible of seekpoint/title */
    struct
    {
        unsigned int i_update;  /* Demux sets them on change,
                                   Input removes them once take into account*/
        /* Seekpoint/Title at demux level */
        int          i_title;       /* idem, start from 0 (could be menu) */
        int          i_seekpoint;   /* idem, start from 0 */
    } info;
    demux_sys_t *p_sys;

    /* Weak link to parent input */
    input_thread_t *p_input;
};

/* pf_demux return values */
#define VLC_DEMUXER_EOF       0
#define VLC_DEMUXER_EGENERIC -1
#define VLC_DEMUXER_SUCCESS   1

/* demux_t.info.i_update field */
#define INPUT_UPDATE_TITLE      0x0010
#define INPUT_UPDATE_SEEKPOINT  0x0020
#define INPUT_UPDATE_META       0x0040
#define INPUT_UPDATE_TITLE_LIST 0x0100

/* demux_meta_t is returned by "meta reader" module to the demuxer */
typedef struct demux_meta_t
{
    VLC_COMMON_MEMBERS
    input_item_t *p_item; /***< the input item that is being read */

    vlc_meta_t *p_meta;                 /**< meta data */

    int i_attachments;                  /**< number of attachments */
    input_attachment_t **attachments;    /**< array of attachments */
} demux_meta_t;

/**
 * Control query identifiers for use with demux_t.pf_control
 *
 * In the individual identifier description, the input stream refers to
 * demux_t.s if non-NULL, and the output refers to demux_t.out.
 *
 * A demuxer is synchronous if it only accesses its input stream and the
 * output from within its demux_t callbacks, i.e. demux.pf_demux and
 * demux_t.pf_control.
 *
 * A demuxer is threaded if it accesses either or both input and output
 * asynchronously.
 *
 * An access-demuxer is a demuxer without input, i.e. demux_t.s == NULL).
 */
enum demux_query_e
{
    /** Checks whether the stream supports seeking.
     * Can fail if seeking is not supported (same as returning false).
     * \bug Failing should not be allowed.
     *
     * arg1 = bool * */
    DEMUX_CAN_SEEK,

    /** Checks whether (long) pause then stream resumption is supported.
     * Can fail only if synchronous and <b>not</b> an access-demuxer. The
     * underlying input stream then determines if pause is supported.
     * \bug Failing should not be allowed.
     *
     * arg1= bool * */
    DEMUX_CAN_PAUSE = 0x002,

    /** Whether the stream can be read at an arbitrary pace.
     * Cannot fail.
     *
     * arg1= bool * */
    DEMUX_CAN_CONTROL_PACE,

    /** Retrieves the PTS delay (roughly the default buffer duration).
     * Can fail only if synchronous and <b>not</b> an access-demuxer. The
     * underlying input stream then determines the PTS delay.
     *
     * arg1= int64_t * */
    DEMUX_GET_PTS_DELAY = 0x101,

    /** Retrieves stream meta-data.
     * Should fail if no meta-data were retrieved.
     *
     * arg1= vlc_meta_t * */
    DEMUX_GET_META = 0x105,

    /** Retrieves an estimate of signal quality and strength.
     * Can fail.
     *
     * arg1=double *quality, arg2=double *strength */
    DEMUX_GET_SIGNAL = 0x107,

    /** Sets the paused or playing/resumed state.
     *
     * Streams are initially in playing state. The control always specifies a
     * change from paused to playing (false) or from playing to paused (true)
     * and streams are initially playing; a no-op cannot be requested.
     *
     * The control is never used if DEMUX_CAN_PAUSE fails.
     * Can fail.
     *
     * arg1= bool */
    DEMUX_SET_PAUSE_STATE = 0x200,

    /** Seeks to the beginning of a title.
     *
     * The control is never used if DEMUX_GET_TITLE_INFO fails.
     * Can fail.
     *
     * arg1= int */
    DEMUX_SET_TITLE,

    /** Seeks to the beginning of a chapter of the current title.
     *
     * The control is never used if DEMUX_GET_TITLE_INFO fails.
     * Can fail.
     *
     * arg1= int */
    DEMUX_SET_SEEKPOINT,        /* arg1= int            can fail */

    /** Check which INPUT_UPDATE_XXX flag is set and reset the ones set.
     *
     * The unsigned* argument is set with the flags needed to be checked,
     * on return it contains the values that were reset during the call
     *
     * This can can fail, in which case flags from demux_t.info.i_update
     * are read/reset
     *
     * arg1= unsigned * */
    DEMUX_TEST_AND_CLEAR_FLAGS, /* arg1= unsigned*      can fail */

    /** Read the title number currently playing
     *
     * Can fail, in which case demux_t.info.i_title is used
     *
     * arg1= int * */
    DEMUX_GET_TITLE,            /* arg1= int*           can fail */

    /* Read the seekpoint/chapter currently playing
     *
     * Can fail, in which case demux_t.info.i_seekpoint is used
     *
     * arg1= int * */
    DEMUX_GET_SEEKPOINT,        /* arg1= int*           can fail */

    /* I. Common queries to access_demux and demux */
    /* POSITION double between 0.0 and 1.0 */
    DEMUX_GET_POSITION = 0x300, /* arg1= double *       res=    */
    DEMUX_SET_POSITION,         /* arg1= double arg2= bool b_precise    res=can fail    */

    /* LENGTH/TIME in microsecond, 0 if unknown */
    DEMUX_GET_LENGTH,           /* arg1= int64_t *      res=    */
    DEMUX_GET_TIME,             /* arg1= int64_t *      res=    */
    DEMUX_SET_TIME,             /* arg1= int64_t arg2= bool b_precise   res=can fail    */

    /**
     * \todo Document
     *
     * \warning The prototype is different from STREAM_GET_TITLE_INFO
     *
     * Can fail, meaning there is only one title and one chapter.
     *
     * arg1= input_title_t ***, arg2=int *, arg3=int *pi_title_offset(0),
     * arg4= int *pi_seekpoint_offset(0) */
    DEMUX_GET_TITLE_INFO,

    /* DEMUX_SET_GROUP/SET_ES only a hint for demuxer (mainly DVB) to allow not
     * reading everything (you should not use this to call es_out_Control)
     * if you don't know what to do with it, just IGNORE it, it is safe(r)
     * -1 means all group, 0 default group (first es added) */
    DEMUX_SET_GROUP,            /* arg1= int, arg2=const vlc_list_t *   can fail */
    DEMUX_SET_ES,               /* arg1= int                            can fail */

    /* Ask the demux to demux until the given date at the next pf_demux call
     * but not more (and not less, at the precision available of course).
     * XXX: not mandatory (except for subtitle demux) but will help a lot
     * for multi-input
     */
    DEMUX_SET_NEXT_DEMUX_TIME,  /* arg1= int64_t        can fail */
    /* FPS for correct subtitles handling */
    DEMUX_GET_FPS,              /* arg1= double *       res=can fail    */

    /* Meta data */
    DEMUX_HAS_UNSUPPORTED_META, /* arg1= bool *   res can fail    */

    /* Attachments */
    DEMUX_GET_ATTACHMENTS,      /* arg1=input_attachment_t***, int* res=can fail */

    /* RECORD you are ensured that it is never called twice with the same state
     * you should accept it only if the stream can be recorded without
     * any modification or header addition. */
    DEMUX_CAN_RECORD,           /* arg1=bool*   res=can fail(assume false) */
    /**
     * \todo Document
     *
     * \warning The prototype is different from STREAM_SET_RECORD_STATE
     *
     * The control is never used if DEMUX_CAN_RECORD fails or returns false.
     * Can fail.
     *
     * arg1= bool */
    DEMUX_SET_RECORD_STATE,

    /* II. Specific access_demux queries */

    /* DEMUX_CAN_CONTROL_RATE is called only if DEMUX_CAN_CONTROL_PACE has
     *  returned false. *pb_rate should be true when the rate can be changed
     * (using DEMUX_SET_RATE). */
    DEMUX_CAN_CONTROL_RATE,     /* arg1= bool*pb_rate */
    /* DEMUX_SET_RATE is called only if DEMUX_CAN_CONTROL_RATE has returned true.
     * It should return the value really used in *pi_rate */
    DEMUX_SET_RATE,             /* arg1= int*pi_rate                                        can fail */

    /** Checks whether the stream is actually a playlist, rather than a real
     * stream.
     *
     * \warning The prototype is different from STREAM_IS_DIRECTORY.
     *
     * Can fail if the stream is not a playlist (same as returning false).
     *
     * arg1= bool * */
    DEMUX_IS_PLAYLIST,

    /* Menu (VCD/DVD/BD) Navigation */
    /** Activate the navigation item selected. Can fail */
    DEMUX_NAV_ACTIVATE,
    /** Use the up arrow to select a navigation item above. Can fail */
    DEMUX_NAV_UP,
    /** Use the down arrow to select a navigation item under. Can fail */
    DEMUX_NAV_DOWN,
    /** Use the left arrow to select a navigation item on the left. Can fail */
    DEMUX_NAV_LEFT,
    /** Use the right arrow to select a navigation item on the right. Can fail */
    DEMUX_NAV_RIGHT,
    /** Activate the popup Menu (for BD). Can fail */
    DEMUX_NAV_POPUP,
    /** Activate disc Root Menu. Can fail */
    DEMUX_NAV_MENU,            /* res=can fail */
    /** Enable/Disable a demux filter
     * \warning This has limited support, and is likely to break if more than
     * a single demux_filter is present in the chain. This is not guaranteed to
     * work in future VLC versions, nor with all demux filters
     */
    DEMUX_FILTER_ENABLE,
    DEMUX_FILTER_DISABLE
};

/*************************************************************************
 * Main Demux
 *************************************************************************/

/* stream_t *s could be null and then it mean a access+demux in one */
VLC_API demux_t *demux_New( vlc_object_t *p_obj, const char *psz_name,
                            const char *psz_path, stream_t *s, es_out_t *out );

VLC_API void demux_Delete( demux_t * );


VLC_API int demux_vaControlHelper( stream_t *, int64_t i_start, int64_t i_end,
                                   int64_t i_bitrate, int i_align, int i_query, va_list args );

VLC_USED static inline int demux_Demux( demux_t *p_demux )
{
    if( !p_demux->pf_demux )
        return VLC_DEMUXER_SUCCESS;

    return p_demux->pf_demux( p_demux );
}

VLC_API int demux_vaControl( demux_t *p_demux, int i_query, va_list args );

static inline int demux_Control( demux_t *p_demux, int i_query, ... )
{
    va_list args;
    int     i_result;

    va_start( args, i_query );
    i_result = demux_vaControl( p_demux, i_query, args );
    va_end( args );
    return i_result;
}

/*************************************************************************
 * Miscellaneous helpers for demuxers
 *************************************************************************/

static inline void demux_UpdateTitleFromStream( demux_t *demux )
{
    stream_t *s = demux->s;
    unsigned title, seekpoint;

    if( vlc_stream_Control( s, STREAM_GET_TITLE, &title ) == VLC_SUCCESS
     && title != (unsigned)demux->info.i_title )
    {
        demux->info.i_title = title;
        demux->info.i_update |= INPUT_UPDATE_TITLE;
    }

    if( vlc_stream_Control( s, STREAM_GET_SEEKPOINT,
                            &seekpoint ) == VLC_SUCCESS
     && seekpoint != (unsigned)demux->info.i_seekpoint )
    {
        demux->info.i_seekpoint = seekpoint;
        demux->info.i_update |= INPUT_UPDATE_SEEKPOINT;
    }
}

VLC_USED
static inline bool demux_IsPathExtension( demux_t *p_demux, const char *psz_extension )
{
    const char *name = (p_demux->psz_file != NULL) ? p_demux->psz_file
                                                   : p_demux->psz_location;
    const char *psz_ext = strrchr ( name, '.' );
    if( !psz_ext || strcasecmp( psz_ext, psz_extension ) )
        return false;
    return true;
}

VLC_USED
static inline bool demux_IsContentType(demux_t *demux, const char *type)
{
    return stream_IsMimeType(demux->s, type);
}

VLC_USED
static inline bool demux_IsForced( demux_t *p_demux, const char *psz_name )
{
   if( !p_demux->psz_demux || strcmp( p_demux->psz_demux, psz_name ) )
        return false;
    return true;
}

/**
 * This function will create a packetizer suitable for a demuxer that parses
 * elementary stream.
 *
 * The provided es_format_t will be cleaned on error or by
 * demux_PacketizerDestroy.
 */
VLC_API decoder_t * demux_PacketizerNew( demux_t *p_demux, es_format_t *p_fmt, const char *psz_msg ) VLC_USED;

/**
 * This function will destroy a packetizer create by demux_PacketizerNew.
 */
VLC_API void demux_PacketizerDestroy( decoder_t *p_packetizer );

/* */
#define DEMUX_INIT_COMMON() do {            \
    p_demux->pf_control = Control;          \
    p_demux->pf_demux = Demux;              \
    p_demux->p_sys = calloc( 1, sizeof( demux_sys_t ) ); \
    if( !p_demux->p_sys ) return VLC_ENOMEM;\
    } while(0)

/**
 * \defgroup chained_demux Chained demultiplexer
 * Demultiplexers wrapped by another demultiplexer
 * @{
 */

typedef struct vlc_demux_chained_t vlc_demux_chained_t;

/**
 * Creates a chained demuxer.
 *
 * This creates a thread running a demuxer whose input stream is generated
 * directly by the caller. This typically handles some sort of stream within a
 * stream, e.g. MPEG-TS within something else.
 *
 * \note There are a number of limitations to this approach. The chained
 * demuxer is run asynchronously in a separate thread. Most demuxer controls
 * are synchronous and therefore unavailable in this case. Also the input
 * stream is a simple FIFO, so the chained demuxer cannot perform seeks.
 * Lastly, most errors cannot be detected.
 *
 * \param parent parent VLC object
 * \param name chained demux module name (e.g. "ts")
 * \param out elementary stream output for the chained demux
 * \return a non-NULL pointer on success, NULL on failure.
 */
VLC_API vlc_demux_chained_t *vlc_demux_chained_New(vlc_object_t *parent,
                                                   const char *name,
                                                   es_out_t *out);

/**
 * Destroys a chained demuxer.
 *
 * Sends an end-of-stream to the chained demuxer, and releases all underlying
 * allocated resources.
 */
VLC_API void vlc_demux_chained_Delete(vlc_demux_chained_t *);

/**
 * Sends data to a chained demuxer.
 *
 * This queues data for a chained demuxer to consume.
 *
 * \param block data block to queue
 */
VLC_API void vlc_demux_chained_Send(vlc_demux_chained_t *, block_t *block);

/**
 * Controls a chained demuxer.
 *
 * This performs a <b>demux</b> (i.e. DEMUX_...) control request on a chained
 * demux.
 *
 * \note In most cases, vlc_demux_chained_Control() should be used instead.
 * \warning As per vlc_demux_chained_New(), most demux controls are not, and
 * cannot be, supported; VLC_EGENERIC is returned.
 *
 * \param query demux control (see \ref demux_query_e)
 * \param args variable arguments (depending on the query)
 */
VLC_API int vlc_demux_chained_ControlVa(vlc_demux_chained_t *, int query,
                                        va_list args);

static inline int vlc_demux_chained_Control(vlc_demux_chained_t *dc, int query,
                                            ...)
{
    va_list ap;
    int ret;

    va_start(ap, query);
    ret = vlc_demux_chained_ControlVa(dc, query, ap);
    va_end(ap);
    return ret;
}

/**
 * @}
 */

/**
 * @}
 */

#endif

/*****************************************************************************
 * input_ext-intf.h: structures of the input exported to the interface
 * This header provides structures to read the stream descriptors and
 * control the pace of reading. 
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: input_ext-intf.h,v 1.78 2002/11/11 14:39:11 sam Exp $
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
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

#ifndef _VLC_INPUT_EXT_INTF_H
#define _VLC_INPUT_EXT_INTF_H 1

/*
 * Communication input -> interface
 */
/* FIXME ! */
#define REQUESTED_MPEG         1
#define REQUESTED_A52          2
#define REQUESTED_LPCM         3
#define REQUESTED_NOAUDIO    255

#define OFFSETTOTIME_MAX_SIZE       10

/*****************************************************************************
 * es_descriptor_t: elementary stream descriptor
 *****************************************************************************
 * Describes an elementary stream, and includes fields required to handle and
 * demultiplex this elementary stream.
 *****************************************************************************/
struct es_descriptor_t
{
    uint16_t                i_id;            /* stream ID for PS, PID for TS */
    uint8_t                 i_stream_id;     /* stream ID defined in the PES */
    vlc_fourcc_t            i_fourcc;                         /* stream type */
    uint8_t                 i_cat;    /* stream category (audio, video, spu) */
    int                     i_demux_fd;   /* used to store demux device
                                             file handle */
    char                    psz_desc[20]; /* description of ES: audio language
                                           * for instance ; NULL if not
                                           *  available */

    /* Demultiplexer information */
    es_sys_t *              p_demux_data;
    pgrm_descriptor_t *     p_pgrm;  /* very convenient in the demultiplexer */

    /* PES parser information */
    pes_packet_t *          p_pes;                            /* Current PES */
    int                     i_pes_real_size;   /* as indicated by the header */

    /* Decoder information */
    decoder_fifo_t *        p_decoder_fifo;

    count_t                 c_packets;                 /* total packets read */
    count_t                 c_invalid_packets;       /* invalid packets read */
};

/* Special PID values - note that the PID is only on 13 bits, and that values
 * greater than 0x1fff have no meaning in a stream */
#define PROGRAM_ASSOCIATION_TABLE_PID   0x0000
#define CONDITIONNAL_ACCESS_TABLE_PID   0x0001                   /* not used */
#define EMPTY_ID                        0xffff    /* empty record in a table */
 

/* ES Categories to be used by interface plugins */
#define UNKNOWN_ES      0x00
#define VIDEO_ES        0x01
#define AUDIO_ES        0x02
#define SPU_ES          0x03
#define NAV_ES          0x04

/*****************************************************************************
 * pgrm_descriptor_t
 *****************************************************************************
 * Describes a program and list associated elementary streams. It is build by
 * the PSI decoder upon the informations carried in program map sections
 *****************************************************************************/
struct pgrm_descriptor_t
{
    /* Program characteristics */
    uint16_t                i_number;                      /* program number */
    uint8_t                 i_version;                     /* version number */
    vlc_bool_t              b_is_ok;      /* Is the description up to date ? */

    /* Service Descriptor (program name) - DVB extension */
    uint8_t                 i_srv_type;
    char *                  psz_srv_name;

    /* Synchronization information */
    mtime_t                 delta_cr;
    mtime_t                 cr_ref, sysdate_ref;
    mtime_t                 last_cr; /* reference to detect unexpected stream
                                      * discontinuities                      */
    mtime_t                 last_syscr;
    count_t                 c_average_count;
                           /* counter used to compute dynamic average values */
    int                     i_synchro_state;

    /* Demultiplexer data */
    pgrm_sys_t *            p_demux_data;

    int                     i_es_number;      /* size of the following array */
    es_descriptor_t **      pp_es;                /* array of pointers to ES */
};

/* Synchro states */
#define SYNCHRO_OK          0
#define SYNCHRO_START       1
#define SYNCHRO_REINIT      2

/*****************************************************************************
 * input_area_t
 *****************************************************************************
 * Attributes for current area (title for DVD)
 *****************************************************************************/
struct input_area_t
{
    /* selected area attributes */
    int                     i_id;        /* identificator for area */
    off_t                   i_start;     /* start offset of area */
    off_t                   i_size;      /* total size of the area
                                          * (in arbitrary units) */

    /* navigation parameters */
    off_t                   i_tell;      /* actual location in the area
                                          * (in arbitrary units) */
    off_t                   i_seek;      /* next requested location
                                          * (changed by the interface thread */

    /* area subdivision */
    int                     i_part_nb;   /* number of parts (chapter for DVD)*/
    int                     i_part;      /* currently selected part */


    /* offset to plugin related data */
    off_t                   i_plugin_data;
};

/**
 * \brief A list of info items.
 *
 * Composes a linked list of name/value pairs intended to inform the
 * user about the current stream
 * \see input_AddInfo
 */
struct input_info_t {
    /**
     * Name of this item
     */
    char *         psz_name;

    /**
     * Value of this item
     */
    char *         psz_value;
    
    /**
     * Pointer to next item in list, or NULL it at end of list
     */
    input_info_t * p_next;
};

/**
 * \brief A list of info categories.
 *
 * Composes a NULL terminated linked list of categories in which to
 * place info about the stream.
 *
 * \see input_InfoCategory
 */
struct input_info_category_t {
    /**
     * The name of this category
     */
    char *                  psz_name;
    
    /**
     * first element of a linked list containing info items
     */
    input_info_t *          p_info;
    
    /**
     * Pointer to next element in this list, or NULL if at end of list
     */
    input_info_category_t * p_next;
};

/*****************************************************************************
* stream_descriptor_t
 *****************************************************************************
 * Describes a stream and list its associated programs. Build upon
 * the information carried in program association sections (for instance)
 *****************************************************************************/
struct stream_descriptor_t
{
    uint16_t                i_stream_id;                        /* stream id */
    vlc_bool_t              b_changed;    /* if stream has been changed,
                                             we have to inform the interface */
    vlc_mutex_t             stream_lock;  /* to be taken every time you read
                                           * or modify stream, pgrm or es    */

    /* Input method data */
    int                     i_method;       /* input method for stream: file,
                                               disc or network */
    vlc_bool_t              b_pace_control;    /* can we read when we want ? */
    vlc_bool_t              b_seekable;               /* can we do lseek() ? */

    /* if (b_seekable) : */
    int                     i_area_nb;
    input_area_t **         pp_areas;    /* list of areas in stream == offset
                                          * interval with own properties */
    input_area_t *          p_selected_area;
    input_area_t *          p_new_area;  /* Newly selected area from
                                          * the interface */

    uint32_t                i_mux_rate; /* the rate we read the stream (in
                                         * units of 50 bytes/s) ; 0 if undef */

    /* New status and rate requested by the interface */
    int                     i_new_status, i_new_rate;
    int                     b_new_mute;          /* int because it can be -1 */
    vlc_cond_t              stream_wait; /* interface -> input in case of a
                                          * status change request            */

    /* Demultiplexer data */
    stream_sys_t *          p_demux_data;

    /* Programs descriptions */
    int                     i_pgrm_number;    /* size of the following array */
    pgrm_descriptor_t **    pp_programs;        /* array of pointers to pgrm */
    pgrm_descriptor_t *     p_selected_program;   /* currently 
                                                 selected program */
    pgrm_descriptor_t *     p_new_program;        /* Newly selected program */
    /* ES descriptions */
    int                     i_es_number;
    es_descriptor_t **      pp_es;             /* carried elementary streams */
    int                     i_selected_es_number;
    es_descriptor_t **      pp_selected_es;             /* ES with a decoder */
    es_descriptor_t *       p_newly_selected_es;   /* ES selected from
                                                    * the interface */
    es_descriptor_t *       p_removed_es;   /* ES removed from the interface */
    
    /* Stream control */
    stream_ctrl_t           control;

    /* Optional stream output */
    sout_instance_t *       p_sout;

    /* Input info */
    input_info_category_t * p_info;
    
    /* Statistics */
    count_t                 c_packets_read;                  /* packets read */
    count_t                 c_packets_trashed;            /* trashed packets */
};

/*****************************************************************************
 * stream_position_t
 *****************************************************************************
 * Describes the current position in the stream.
 *****************************************************************************/
struct stream_position_t
{
    off_t    i_tell;     /* actual location in the area (in arbitrary units) */
    off_t    i_size;          /* total size of the area (in arbitrary units) */

    uint32_t i_mux_rate;                /* the rate we read the stream (in
                                         * units of 50 bytes/s) ; 0 if undef */
};

#define MUTE_NO_CHANGE      -1

/*****************************************************************************
 * input_thread_t
 *****************************************************************************
 * This structure includes all the local static variables of an input thread
 *****************************************************************************/
struct input_thread_t
{
    VLC_COMMON_MEMBERS

    /* Thread properties */
    vlc_bool_t              b_eof;

    /* Access module */
    module_t *       p_access;
    ssize_t       (* pf_read ) ( input_thread_t *, byte_t *, size_t );
    int           (* pf_set_program )( input_thread_t *, pgrm_descriptor_t * );
    int           (* pf_set_area )( input_thread_t *, input_area_t * );
    void          (* pf_seek ) ( input_thread_t *, off_t );
    access_sys_t *   p_access_data;
    size_t           i_mtu;

    /* Demux module */
    module_t *       p_demux;
    int           (* pf_demux )  ( input_thread_t * );
    int           (* pf_rewind ) ( input_thread_t * );
                                           /* NULL if we don't support going *
                                            * backwards (it's gonna be fun)  */
    demux_sys_t *    p_demux_data;                      /* data of the demux */

    /* Buffer manager */
    input_buffers_t *p_method_data;     /* data of the packet manager */
    data_buffer_t *  p_data_buffer;
    byte_t *         p_current_data;
    byte_t *         p_last_data;
    size_t           i_bufsize;

    /* General stream description */
    stream_descriptor_t     stream;

    /* Playlist item */
    char *  psz_source;
    char *  psz_access;
    char *  psz_demux;
    char *  psz_name;

    count_t c_loops;
};

/* Input methods */
/* The first figure is a general method that can be used in interface plugins ;
 * The second figure is a detailed sub-method */
#define INPUT_METHOD_NONE         0x0            /* input thread is inactive */
#define INPUT_METHOD_FILE        0x10   /* stream is read from file p_source */
#define INPUT_METHOD_DISC        0x20   /* stream is read directly from disc */
#define INPUT_METHOD_DVD         0x21             /* stream is read from DVD */
#define INPUT_METHOD_VCD         0x22             /* stream is read from VCD */
#define INPUT_METHOD_NETWORK     0x30         /* stream is read from network */
#define INPUT_METHOD_UCAST       0x31                         /* UDP unicast */
#define INPUT_METHOD_MCAST       0x32                       /* UDP multicast */
#define INPUT_METHOD_BCAST       0x33                       /* UDP broadcast */
#define INPUT_METHOD_VLAN_BCAST  0x34            /* UDP broadcast with VLANs */
#define INPUT_METHOD_SATELLITE   0x40               /* stream is read from a */
                                                           /* satellite card */

/* Status changing methods */
#define INPUT_STATUS_END            0
#define INPUT_STATUS_PLAY           1
#define INPUT_STATUS_PAUSE          2
#define INPUT_STATUS_FASTER         3
#define INPUT_STATUS_SLOWER         4

/* Seek modes */
#define INPUT_SEEK_SET       0x00
#define INPUT_SEEK_CUR       0x01
#define INPUT_SEEK_END       0x02
#define INPUT_SEEK_BYTES     0x00
#define INPUT_SEEK_SECONDS   0x10
#define INPUT_SEEK_PERCENT   0x20

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
#define input_CreateThread(a,b,c) __input_CreateThread(VLC_OBJECT(a),b,c)
input_thread_t * __input_CreateThread ( vlc_object_t *,
                                        playlist_item_t *, int * );
void   input_StopThread     ( input_thread_t * );
void   input_DestroyThread  ( input_thread_t * );

#define input_SetStatus(a,b) __input_SetStatus(VLC_OBJECT(a),b)
VLC_EXPORT( void, __input_SetStatus, ( vlc_object_t *, int ) );

#define input_Seek(a,b,c) __input_Seek(VLC_OBJECT(a),b,c)
VLC_EXPORT( void, __input_Seek, ( vlc_object_t *, off_t, int ) );

#define input_Tell(a,b) __input_Tell(VLC_OBJECT(a),b)
VLC_EXPORT( void, __input_Tell, ( vlc_object_t *, stream_position_t * ) );

VLC_EXPORT( void, input_DumpStream, ( input_thread_t * ) );
VLC_EXPORT( char *, input_OffsetToTime, ( input_thread_t *, char *, off_t ) );
VLC_EXPORT( int, input_ToggleES, ( input_thread_t *, es_descriptor_t *, vlc_bool_t ) );
VLC_EXPORT( int, input_ChangeArea, ( input_thread_t *, input_area_t * ) );
VLC_EXPORT( int, input_ChangeProgram, ( input_thread_t *, uint16_t ) );

int    input_ToggleGrayscale( input_thread_t * );
int    input_ToggleMute     ( input_thread_t * );

#endif /* "input_ext-intf.h" */

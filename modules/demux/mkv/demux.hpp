/*****************************************************************************
 * demux.hpp : matroska demuxer
 *****************************************************************************
 * Copyright (C) 2003-2004 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Steve Lhomme <steve.lhomme@free.fr>
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

#ifndef _DEMUX_SYS_H
#define _DEMUX_SYS_H

#include "mkv.hpp"

#include "chapter_command.hpp"
#include "virtual_segment.hpp"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#undef ATTRIBUTE_PACKED
#undef PRAGMA_PACK_BEGIN
#undef PRAGMA_PACK_END

#if defined(__GNUC__)
#define ATTRIBUTE_PACKED __attribute__ ((packed))
#define PRAGMA_PACK 0
#endif

#if !defined(ATTRIBUTE_PACKED)
#define ATTRIBUTE_PACKED
#define PRAGMA_PACK 1
#endif

#if PRAGMA_PACK
#pragma pack(1)
#endif

/*************************************
*  taken from libdvdnav / libdvdread
**************************************/

/**
 * DVD Time Information.
 */
typedef struct {
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
  uint8_t frame_u; /* The two high bits are the frame rate. */
} ATTRIBUTE_PACKED dvd_time_t;

/**
 * User Operations.
 */
typedef struct {
#ifdef WORDS_BIGENDIAN
  unsigned char zero                           : 7; /* 25-31 */
  unsigned char video_pres_mode_change         : 1; /* 24 */
 
  unsigned char karaoke_audio_pres_mode_change : 1; /* 23 */
  unsigned char angle_change                   : 1;
  unsigned char subpic_stream_change           : 1;
  unsigned char audio_stream_change            : 1;
  unsigned char pause_on                       : 1;
  unsigned char still_off                      : 1;
  unsigned char button_select_or_activate      : 1;
  unsigned char resume                         : 1; /* 16 */
 
  unsigned char chapter_menu_call              : 1; /* 15 */
  unsigned char angle_menu_call                : 1;
  unsigned char audio_menu_call                : 1;
  unsigned char subpic_menu_call               : 1;
  unsigned char root_menu_call                 : 1;
  unsigned char title_menu_call                : 1;
  unsigned char backward_scan                  : 1;
  unsigned char forward_scan                   : 1; /* 8 */
 
  unsigned char next_pg_search                 : 1; /* 7 */
  unsigned char prev_or_top_pg_search          : 1;
  unsigned char time_or_chapter_search         : 1;
  unsigned char go_up                          : 1;
  unsigned char stop                           : 1;
  unsigned char title_play                     : 1;
  unsigned char chapter_search_or_play         : 1;
  unsigned char title_or_time_play             : 1; /* 0 */
#else
  unsigned char video_pres_mode_change         : 1; /* 24 */
  unsigned char zero                           : 7; /* 25-31 */
 
  unsigned char resume                         : 1; /* 16 */
  unsigned char button_select_or_activate      : 1;
  unsigned char still_off                      : 1;
  unsigned char pause_on                       : 1;
  unsigned char audio_stream_change            : 1;
  unsigned char subpic_stream_change           : 1;
  unsigned char angle_change                   : 1;
  unsigned char karaoke_audio_pres_mode_change : 1; /* 23 */
 
  unsigned char forward_scan                   : 1; /* 8 */
  unsigned char backward_scan                  : 1;
  unsigned char title_menu_call                : 1;
  unsigned char root_menu_call                 : 1;
  unsigned char subpic_menu_call               : 1;
  unsigned char audio_menu_call                : 1;
  unsigned char angle_menu_call                : 1;
  unsigned char chapter_menu_call              : 1; /* 15 */
 
  unsigned char title_or_time_play             : 1; /* 0 */
  unsigned char chapter_search_or_play         : 1;
  unsigned char title_play                     : 1;
  unsigned char stop                           : 1;
  unsigned char go_up                          : 1;
  unsigned char time_or_chapter_search         : 1;
  unsigned char prev_or_top_pg_search          : 1;
  unsigned char next_pg_search                 : 1; /* 7 */
#endif
} ATTRIBUTE_PACKED user_ops_t;

/**
 * Type to store per-command data.
 */
typedef struct {
  uint8_t bytes[8];
} ATTRIBUTE_PACKED vm_cmd_t;
#define COMMAND_DATA_SIZE 8

/**
 * PCI General Information
 */
typedef struct {
  uint32_t nv_pck_lbn;      /**< sector address of this nav pack */
  uint16_t vobu_cat;        /**< 'category' of vobu */
  uint16_t zero1;           /**< reserved */
  user_ops_t vobu_uop_ctl;  /**< UOP of vobu */
  uint32_t vobu_s_ptm;      /**< start presentation time of vobu */
  uint32_t vobu_e_ptm;      /**< end presentation time of vobu */
  uint32_t vobu_se_e_ptm;   /**< end ptm of sequence end in vobu */
  dvd_time_t e_eltm;        /**< Cell elapsed time */
  char vobu_isrc[32];
} ATTRIBUTE_PACKED pci_gi_t;

/**
 * Non Seamless Angle Information
 */
typedef struct {
  uint32_t nsml_agl_dsta[9];  /**< address of destination vobu in AGL_C#n */
} ATTRIBUTE_PACKED nsml_agli_t;

/**
 * Highlight General Information
 *
 * For btngrX_dsp_ty the bits have the following meaning:
 * 000b: normal 4/3 only buttons
 * XX1b: wide (16/9) buttons
 * X1Xb: letterbox buttons
 * 1XXb: pan&scan buttons
 */
typedef struct {
  uint16_t hli_ss; /**< status, only low 2 bits 0: no buttons, 1: different 2: equal 3: eual except for button cmds */
  uint32_t hli_s_ptm;              /**< start ptm of hli */
  uint32_t hli_e_ptm;              /**< end ptm of hli */
  uint32_t btn_se_e_ptm;           /**< end ptm of button select */
#ifdef WORDS_BIGENDIAN
  unsigned char zero1 : 2;          /**< reserved */
  unsigned char btngr_ns : 2;       /**< number of button groups 1, 2 or 3 with 36/18/12 buttons */
  unsigned char zero2 : 1;          /**< reserved */
  unsigned char btngr1_dsp_ty : 3;  /**< display type of subpic stream for button group 1 */
  unsigned char zero3 : 1;          /**< reserved */
  unsigned char btngr2_dsp_ty : 3;  /**< display type of subpic stream for button group 2 */
  unsigned char zero4 : 1;          /**< reserved */
  unsigned char btngr3_dsp_ty : 3;  /**< display type of subpic stream for button group 3 */
#else
  unsigned char btngr1_dsp_ty : 3;
  unsigned char zero2 : 1;
  unsigned char btngr_ns : 2;
  unsigned char zero1 : 2;
  unsigned char btngr3_dsp_ty : 3;
  unsigned char zero4 : 1;
  unsigned char btngr2_dsp_ty : 3;
  unsigned char zero3 : 1;
#endif
  uint8_t btn_ofn;     /**< button offset number range 0-255 */
  uint8_t btn_ns;      /**< number of valid buttons  <= 36/18/12 (low 6 bits) */
  uint8_t nsl_btn_ns;  /**< number of buttons selectable by U_BTNNi (low 6 bits)   nsl_btn_ns <= btn_ns */
  uint8_t zero5;       /**< reserved */
  uint8_t fosl_btnn;   /**< forcedly selected button  (low 6 bits) */
  uint8_t foac_btnn;   /**< forcedly activated button (low 6 bits) */
} ATTRIBUTE_PACKED hl_gi_t;


/**
 * Button Color Information Table
 * Each entry beeing a 32bit word that contains the color indexs and alpha
 * values to use.  They are all represented by 4 bit number and stored
 * like this [Ci3, Ci2, Ci1, Ci0, A3, A2, A1, A0].   The actual palette
 * that the indexes reference is in the PGC.
 * \todo split the uint32_t into a struct
 */
typedef struct {
  uint32_t btn_coli[3][2];  /**< [button color number-1][select:0/action:1] */
} ATTRIBUTE_PACKED btn_colit_t;

/**
 * Button Information
 *
 * NOTE: I've had to change the structure from the disk layout to get
 * the packing to work with Sun's Forte C compiler.
 * The 4 and 7 bytes are 'rotated' was: ABC DEF GHIJ  is: ABCG DEFH IJ
 */
typedef struct {
#ifdef WORDS_BIGENDIAN
  uint32        btn_coln         : 2;  /**< button color number */
  uint32        x_start          : 10; /**< x start offset within the overlay */
  uint32        zero1            : 2;  /**< reserved */
  uint32        x_end            : 10; /**< x end offset within the overlay */

  uint32        zero3            : 2;  /**< reserved */
  uint32        up               : 6;  /**< button index when pressing up */

  uint32        auto_action_mode : 2;  /**< 0: no, 1: activated if selected */
  uint32        y_start          : 10; /**< y start offset within the overlay */
  uint32        zero2            : 2;  /**< reserved */
  uint32        y_end            : 10; /**< y end offset within the overlay */

  uint32        zero4            : 2;  /**< reserved */
  uint32        down             : 6;  /**< button index when pressing down */
  unsigned char zero5            : 2;  /**< reserved */
  unsigned char left             : 6;  /**< button index when pressing left */
  unsigned char zero6            : 2;  /**< reserved */
  unsigned char right            : 6;  /**< button index when pressing right */
#else
  uint32        x_end            : 10;
  uint32        zero1            : 2;
  uint32        x_start          : 10;
  uint32        btn_coln         : 2;

  uint32        up               : 6;
  uint32        zero3            : 2;

  uint32        y_end            : 10;
  uint32        zero2            : 2;
  uint32        y_start          : 10;
  uint32        auto_action_mode : 2;

  uint32        down             : 6;
  uint32        zero4            : 2;
  unsigned char left             : 6;
  unsigned char zero5            : 2;
  unsigned char right            : 6;
  unsigned char zero6            : 2;
#endif
  vm_cmd_t cmd;
} ATTRIBUTE_PACKED btni_t;

/**
 * Highlight Information
 */
typedef struct {
  hl_gi_t     hl_gi;
  btn_colit_t btn_colit;
  btni_t      btnit[36];
} ATTRIBUTE_PACKED hli_t;

/**
 * PCI packet
 */
typedef struct {
  pci_gi_t    pci_gi;
  nsml_agli_t nsml_agli;
  hli_t       hli;
  uint8_t     zero1[189];
} ATTRIBUTE_PACKED pci_t;

#if PRAGMA_PACK
#pragma pack()
#endif
////////////////////////////////////////

class virtual_segment_c;
class chapter_item_c;

class event_thread_t
{
public:
    event_thread_t(demux_t *);
    virtual ~event_thread_t();

    void SetPci(const pci_t *data);
    void ResetPci();

private:
    void EventThread();
    static void *EventThread(void *);

    static int EventMouse( vlc_object_t *, char const *, vlc_value_t, vlc_value_t, void * );
    static int EventKey( vlc_object_t *, char const *, vlc_value_t, vlc_value_t, void * );
    static int EventInput( vlc_object_t *, char const *, vlc_value_t, vlc_value_t, void * );

    demux_t      *p_demux;

    bool         is_running;
    vlc_thread_t thread;

    vlc_mutex_t  lock;
    vlc_cond_t   wait;
    bool         b_abort;
    bool         b_moved;
    bool         b_clicked;
    int          i_key_action;
    bool         b_vout;
    pci_t        pci_packet;
};


class demux_sys_t
{
public:
    demux_sys_t( demux_t & demux )
        :demuxer(demux)
        ,i_pts(0)
        ,i_pcr(0)
        ,i_start_pts(0)
        ,i_chapter_time(0)
        ,meta(NULL)
        ,i_current_title(0)
        ,p_current_segment(NULL)
        ,dvd_interpretor( *this )
        ,f_duration(-1.0)
        ,p_input(NULL)
        ,p_ev(NULL)
    {
        vlc_mutex_init( &lock_demuxer );
    }

    virtual ~demux_sys_t();

    /* current data */
    demux_t                 & demuxer;

    mtime_t                 i_pts;
    mtime_t                 i_pcr;
    mtime_t                 i_start_pts;
    mtime_t                 i_chapter_time;

    vlc_meta_t              *meta;

    std::vector<input_title_t*>      titles; // matroska editions
    size_t                           i_current_title;

    std::vector<matroska_stream_c*>  streams;
    std::vector<attachment_c*>       stored_attachments;
    std::vector<matroska_segment_c*> opened_segments;
    std::vector<virtual_segment_c*>  used_segments;
    virtual_segment_c                *p_current_segment;

    dvd_command_interpretor_c        dvd_interpretor;

    /* duration of the stream */
    float                   f_duration;

    matroska_segment_c *FindSegment( const EbmlBinary & uid ) const;
    virtual_chapter_c *BrowseCodecPrivate( unsigned int codec_id,
                                        bool (*match)(const chapter_codec_cmds_c &data, const void *p_cookie, size_t i_cookie_size ),
                                        const void *p_cookie,
                                        size_t i_cookie_size,
                                        virtual_segment_c * & p_segment_found );
    virtual_chapter_c *FindChapter( int64_t i_find_uid, virtual_segment_c * & p_segment_found );

    void PreloadFamily( const matroska_segment_c & of_segment );
    bool PreloadLinked();
    void FreeUnused();
    bool PreparePlayback( virtual_segment_c *p_new_segment );
    matroska_stream_c *AnalyseAllSegmentsFound( demux_t *p_demux, EbmlStream *p_estream, bool b_initial = false );
    void JumpTo( virtual_segment_c & p_segment, virtual_chapter_c * p_chapter );

    void InitUi();
    void CleanUi();

    /* for spu variables */
    input_thread_t *p_input;
    uint8_t        palette[4][4];
    vlc_mutex_t    lock_demuxer;

    /* event */
    event_thread_t *p_ev;

protected:
    virtual_segment_c *VirtualFromSegments( std::vector<matroska_segment_c*> *p_segments ) const;
};


#endif

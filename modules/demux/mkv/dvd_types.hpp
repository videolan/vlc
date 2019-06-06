/*****************************************************************************
 * dvd_types.hpp : matroska demuxer
 *****************************************************************************
 * Copyright (C) 2003-2004 VLC authors and VideoLAN
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

#ifndef VLC_MKV_DVD_TYPES_HPP
#define VLC_MKV_DVD_TYPES_HPP

namespace mkv {
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

} // namespace

#endif

/*****************************************************************************
 * dvd_ifo.h: Structures for ifo parsing
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: dvd_ifo.h,v 1.7 2001/02/20 02:53:13 stef Exp $
 *
 * Author: Stéphane Borel <stef@via.ecp.fr>
 *
 * based on:
 *  - libifo by Thomas Mirlacher <dent@cosy.sbg.ac.at>
 *  - IFO structure documentation by Thomas Mirlacher, Björn Englund,
 *  Håkan Hjort
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

/*****************************************************************************
 * Common structures for Video Management and Video Title sets
 *****************************************************************************/

/*
 * Program Chain structures
 */

/* Ifo vitual machine Commands */
typedef struct ifo_command_s
{
    u8              i_type      :3;
    u8              i_direct    :1;
    u8              i_cmd       :4;
    u8              i_dir_cmp   :1;
    u8              i_cmp       :3;
    u8              i_sub_cmd   :4;
    union
    {
		u8          pi_8[6];
		u16         pi_16[3];
    } data;
} ifo_command_t;

/* Program Chain Command Table
  - start at i_pgc_com_tab_sbyte */
typedef struct pgc_com_tab_s
{
    u16             i_pre_com_nb;               // 2 bytes
    u16             i_post_com_nb;              // 2 bytes
    u16             i_cell_com_nb;              // 2 bytes
//    char[2]         ???
    ifo_command_t*  p_pre_com;                  // i_pre_com_nb * 8 bytes
    ifo_command_t*  p_post_com;                 // i_post_com_nb * 8 bytes
    ifo_command_t*  p_cell_com;                 // i_cell_com_nb * 8 bytes
} pgc_com_tab_t;

/* Program Chain Map Table
 * - start at "i_pgc_prg_map_sbyte" */
typedef struct pgc_prg_map_s
{
    u8*             pi_entry_cell;              // i_prg_nb * 1 byte 
} pgc_prg_map_t;

/* Cell Playback Information Table
 * we have a pointer to such a structure for each cell  
 * - first start at "i_cell_play_inf_sbyte" */
typedef struct cell_play_inf_s
{
    /* This information concerns the currently selected cell */
    u16             i_cat;                      // 2 bytes
    u8              i_still_time;               // 1 byte; in seconds; ff=inf
    u8              i_com_nb;                   // 1 byte; 0 = no com
    u32             i_play_time;                // 4 bytes
    u32             i_entry_sector;             // 4 bytes
    u32             i_first_ilvu_vobu_esector;  // 4 bytes; ???
    u32             i_lvobu_ssector;            // 4 bytes
    u32             i_lsector;                  // 4 bytes
} cell_play_inf_t;

/* Cell Position Information Table
 * we have a pointer to such a structure for each cell 
 * - first start at "i_cell_pos_inf_sbyte" */
typedef struct cell_pos_inf_s
{
    /* This information concerns the currently selected cell */
    u16             i_vob_id;                   // 2 bytes
//    char            ???
    u8              i_cell_id;                  // 1 byte
} cell_pos_inf_t;

/* Main structure for Program Chain
 * - start at i_fp_pgc_sbyte
 * - or at i_vmgm_pgci_sbyte in vmgm_pgci_srp_t */
typedef struct pgc_s
{
    /* Global features of program chain */
//    char[2]         ???
    u8              i_prg_nb;                   // 1 byte
    u8              i_cell_nb;                  // 1 byte
    u32             i_play_time;                // 4 bytes
    u32             i_prohibited_user_op;       // 4 bytes
    u16             pi_audio_status[8];         // 8*2 bytes
    u32             pi_subpic_status[32];       // 32*4 bytes
    u16             i_next_pgc_nb;              // 2 bytes
    u16             i_prev_pgc_nb;              // 2 bytes
    u16             i_goup_pgc_nb;              // 2 bytes
    u8              i_still_time;               // 1 byte ; in seconds
    u8              i_play_mode;                // 1 byte
    /* In video_ts.ifo, the 3 significant bytes of each color are
     * preceded by 1 unsignificant byte */
    u32             pi_yuv_color[16];           // 16*3 bytes
    /* Here come the start bytes of the following structures */
    u16             i_com_tab_sbyte;            // 2 bytes
    u16             i_prg_map_sbyte;            // 2 bytes
    u16             i_cell_play_inf_sbyte;      // 2 bytes
    u16             i_cell_pos_inf_sbyte;       // 2 bytes
    /* Predefined structures */
    pgc_com_tab_t   com_tab;
    pgc_prg_map_t   prg_map;
    cell_play_inf_t* p_cell_play_inf;           // i_cell_nb * 24 bytes
    cell_pos_inf_t* p_cell_pos_inf;             // i_cell_nb * 4 bytes
} pgc_t;

/*
 * Menu PGCI Unit Table
 */

/* Menu PGCI Language unit Descriptor */
typedef struct pgci_lu_s
{
    char            ps_lang_code[2];            // 2 bytes (ISO-xx)
//    char            ???
    u8              i_existence_mask;           // 1 byte
    u32             i_lu_sbyte;                 // 4 bytes
} pgci_lu_t;

typedef struct pgci_srp_s
{
    u8              i_pgc_cat_mask;             // 1 byte
    u8              i_pgc_cat;                  // 1 byte
    u16             i_par_mask;                 // 2 bytes
    u32             i_pgci_sbyte;               // 4 bytes
    pgc_t           pgc;
} pgci_srp_t;

/* Menu PGCI Language Unit Table 
 * - start at i_lu_sbyte */
typedef struct pgci_inf_s
{
    u16             i_srp_nb;                   // 2 bytes
//    char[2]         ???
    u32             i_lu_ebyte;                 // 4 bytes
    pgci_srp_t*     p_srp;                      // i_srp_nb * 8 bytes
} pgci_inf_t;

/* Main Struct for Menu PGCI
 * - start at i_*_pgci_ut_ssector */
typedef struct pgci_ut_s
{
    u16             i_lu_nb;                    // 2 bytes; ???
//    char[2]         ???
    u32             i_ebyte;                    // 4 bytes
    pgci_lu_t*      p_lu;                       // i_lu_nb * 8 bytes
    pgci_inf_t*     p_pgci_inf;                 // i_lu_nb * 8 bytes
} pgci_ut_t;

/*
 * Cell Adress Table Information
 */
typedef struct cell_inf_s
{
    u16             i_vob_id;                   // 2 bytes
    u8              i_cell_id;                  // 1 byte
//    char            ???
    u32             i_ssector;                  // 4 bytes
    u32             i_esector;                  // 4 bytes
} cell_inf_t;

typedef struct c_adt_s
{
    u16             i_vob_nb;                   // 2 bytes
//    char[2]         ???
    u32             i_ebyte;                    // 4 bytes
    u16             i_cell_nb;                  // not in ifo; computed
                                                // with e_byte
    cell_inf_t*     p_cell_inf;
} c_adt_t;


/*
 * VOBU Adress Map Table
 */
typedef struct vobu_admap_s
{
    u32             i_ebyte;                    // 4 bytes
    u32*            pi_vobu_ssector;            // (nb of vobu) * 4 bytes
} vobu_admap_t;

/*****************************************************************************
 * Structures for Video Management (cf video_ts.ifo)
 *****************************************************************************/

/* 
 * Video Manager Information Management Table
 */ 
typedef struct vmgi_mat_s
{
    char            psz_id[13];                 // 12 bytes (DVDVIDEO-VMG)
    u32             i_lsector;                  // 4 bytes
//    char[12]        ???
    u32             i_i_lsector;                // 4 bytes
//    char            ???
    u8              i_spec_ver;                 // 1 byte
    u32             i_cat;                      // 4 bytes
    u16             i_vol_nb;                   // 2 bytes
    u16             i_vol;                      // 2 bytes
    u8              i_disc_side;                // 1 bytes
//    char[20]        ???
    u16             i_tts_nb;                   // 2 bytes
    char            ps_provider_id[32];         // 32 bytes
    u64             i_pos_code;                 // 8 bytes
//    char[24]        ???
    u32             i_i_mat_ebyte;              // 4 bytes
    u32             i_fp_pgc_sbyte;             // 4 bytes
//    char[56]        ???
    u32             i_vobs_ssector;             // 4 bytes
    u32             i_ptt_srpt_ssector;         // 4 bytes
    u32             i_pgci_ut_ssector;          // 4 bytes
    u32             i_ptl_mait_ssector;         // 4 bytes
    u32             i_vts_atrt_ssector;         // 4 bytes
    u32             i_txtdt_mg_ssector;         // 4 bytes
    u32             i_c_adt_ssector;            // 4 bytes
    u32             i_vobu_admap_ssector;       // 4 bytes
//    char[2]         ???
    u16             i_video_atrt;               // 2 bytes
//    char            ???
    u8              i_audio_nb;                 // 1 byte
    u64             pi_audio_atrt[8];           // i_vmgm_audio_nb * 8 bytes
//    char[16]        ???
    u8              i_subpic_nb;                // 1 byte
    u64             pi_subpic_atrt[32];         // i_subpic_nb * 6 bytes
} vmgi_mat_t;


/* 
 * Part Of Title Search Pointer Table Information
 */

/* Title sets structure
 * we have a pointer to this structure for each tts */
typedef struct tts_s
{
    u8              i_play_type;                // 1 byte
    u8              i_angle_nb;                 // 1 byte
    u16             i_ptt_nb;                   // 2 bytes; Chapters/PGs
    u16             i_parental_id;              // 2 bytes
    u8              i_tts_nb;                   // 1 byte (VTS#)
    u8              i_vts_ttn;                  // 1 byte ???
    u32             i_ssector;                  // 4 bytes
} tts_t;

/* Main struct for tts
 * - start at "i_vmg_ptt_srpt_ssector" */
typedef struct vmg_ptt_srpt_s
{
    u16             i_ttu_nb;                   // 2 bytes
//    char[2]         ???
    u32             i_ebyte;                    // 4 bytes
    tts_t*          p_tts;                      // i_ttu_nb * 12 bytes
} vmg_ptt_srpt_t;

/*
 * Parental Management Information Table
 */
typedef struct vmg_ptl_mai_desc_s
{
    char            ps_country_code[2];         // 2 bytes
//    char[2]         ???
    u16             i_ptl_mai_sbyte;            // 2 bytes
//    char[2]         ???
} vmg_ptl_mai_desc_t;

typedef struct vmg_ptl_mask_s
{
    u16*            ppi_ptl_mask[8];            // (i_vts_nb +1) * 8 * 2 bytes
} vmg_ptl_mask_t;

/* Main struct for parental management
 * - start at i_vmg_ptl_mait_ssector */
typedef struct vmg_ptl_mait_s
{
    u16             i_country_nb;               // 2 bytes
    u16             i_vts_nb;                   // 2 bytes
    u32             i_ebyte;                    // 4 bytes
    vmg_ptl_mai_desc_t* p_ptl_desc;             // i_country_nb * 8 bytes
    vmg_ptl_mask_t* p_ptl_mask;        // i_country_nb * sizeof(vmg_ptl_mask_t)
} vmg_ptl_mait_t;

/*
 * Video Title Set Attribute Table
 */

/* Attribute structure : one for each vts
 * - start at pi_atrt_sbyte */
typedef struct vts_atrt_s
{
    u32             i_ebyte;                    // 4 bytes
    u32             i_cat_app_type;             // 4 bytes
    u16             i_vtsm_video_atrt;          // 2 bytes
//    char            ???
    u8              i_vtsm_audio_nb;            // 1 byte
    u64             pi_vtsm_audio_atrt[8];      // 8 * 8 bytes
//    char[17]        ???
    u8              i_vtsm_subpic_nb;           // 1 byte
    u64             pi_vtsm_subpic_atrt[28];    // i_vtsm_subpic_nb * 6 bytes
//    char[2]         ???
    u16             i_vtstt_video_atrt;         // 2 bytes
//    char            ???
    u8              i_vtstt_audio_nb;           // 1 byte
    u64             pi_vtstt_audio_atrt[8];     // 8 * 8 bytes
//    char[17]        ???
    u8              i_vtstt_subpic_nb;          // 1 byte
    u64             pi_vtstt_subpic_atrt[28];   // i_vtstt_subpic_nb * 6 bytes
} vts_atrt_t;

/* Main struct for vts attributes
 * - start at i_vmg_vts_atrt_ssector */
typedef struct vmg_vts_atrt_s
{
    u16             i_vts_nb;                   // 2 bytes
//    char[2]         ???
    u32             i_ebyte;                    // 4 bytes
    u32*            pi_vts_atrt_sbyte;          // i_vts_nb * 4 bytes
    vts_atrt_t*     p_vts_atrt;
} vmg_vts_atrt_t;

/* 
 * Global Structure for Video Manager
 */
typedef struct vmg_s 
{
    vmgi_mat_t      mat;
    pgc_t           pgc;
    vmg_ptt_srpt_t  ptt_srpt;
    pgci_ut_t       pgci_ut;
    vmg_ptl_mait_t  ptl_mait;
    vmg_vts_atrt_t  vts_atrt;
    c_adt_t         c_adt;
    vobu_admap_t    vobu_admap;
} vmg_t;

/*****************************************************************************
 * Structures for Video Title Sets (cf vts_*.ifo)
 ****************************************************************************/

/* 
 * Video Title Sets Information Management Table
 */ 
typedef struct vtsi_mat_s
{
    char            psz_id[13];                 // 12 bytes (DVDVIDEO-VTS)
    u32             i_lsector;                  // 4 bytes
//    char[12]        ???
    u32             i_i_lsector;                // 4 bytes
//    char            ???
    u8              i_spec_ver;                 // 1 byte
    u32             i_cat;                      // 4 bytes
//    char[90]        ???
    u32             i_mat_ebyte;                // 4 bytes
//    char[60]        ???
    u32             i_m_vobs_ssector;           // 4 bytes
    u32             i_tt_vobs_ssector;          // 4 bytes
    u32             i_ptt_srpt_ssector;         // 4 bytes
    u32             i_pgcit_ssector;            // 4 bytes
    u32             i_m_pgci_ut_ssector;        // 4 bytes
    u32             i_tmap_ti_ssector;          // 4 bytes
    u32             i_m_c_adt_ssector;          // 4 bytes
    u32             i_m_vobu_admap_ssector;     // 4 bytes
    u32             i_c_adt_ssector;            // 4 bytes
    u32             i_vobu_admap_ssector;       // 4 bytes
//    char[24]        ???
    u16             i_m_video_atrt;             // 2 bytes
//    char            ???
    u8              i_m_audio_nb;               // 1 byte
    u64             pi_m_audio_atrt[8];         // i_vmgm_audio_nb * 8 bytes
//    char[16]        ???
    u8              i_m_subpic_nb;              // 1 byte
    u64             pi_m_subpic_atrt[32];       // i_subpic_nb * 6 bytes
                                                // !!! only 28 subpics ???
//    char[2]         ???
    u16             i_video_atrt;               // 2 bytes
//    char            ???
    u8              i_audio_nb;                 // 1 byte
    u64             pi_audio_atrt[8];           // i_vmgm_audio_nb * 8 bytes
//    char[16]        ???
    u8              i_subpic_nb;                // 1 byte
    u64             pi_subpic_atrt[32];         // i_subpic_nb * 6 bytes
} vtsi_mat_t;

/* 
 * Part Of Title Search Pointer Table Information
 */

/* Title sets structure
 * we have a pointer to this structure for each tts */
typedef struct ttu_s
{
    u16             i_pgc_nb;                   // 2 bytes; Chapters/PGs
    u16             i_prg_nb;                   // 2 bytes
} ttu_t;

/* Main struct for tts
 * - start at "i_vts_ptt_srpt_ssector" */
typedef struct vts_ptt_srpt_s
{
    u16             i_ttu_nb;                   // 2 bytes
//    char[2]         ???
    u32             i_ebyte;                    // 4 bytes
    u32*            pi_ttu_sbyte;
    ttu_t*          p_ttu;                      // i_ttu_nb * 4 bytes
} vts_ptt_srpt_t;

/*
 * Time Map table information
 */

/* Time Map structure */
typedef struct tmap_s
{
    u8              i_time_unit;                // 1 byte
//    char            ???
    u16             i_entry_nb;                 // 2 bytes
    u32*            pi_sector;                  // i_entry_nb * 4 bytes
} tmap_t;

/* Main structure for tmap_ti
 * - start at "i_tmap_ti_ssector" */
typedef struct vts_tmap_ti_s
{
    u16             i_nb;                       // 2 bytes
//    char[2]         ???
    u32             i_ebyte;                    // 4 bytes
    u32*            pi_sbyte;                   // i_tmap_nb * 4 bytes
    tmap_t*         p_tmap;
} vts_tmap_ti_t;

/*
 * Video Title Set 
 */
typedef struct vts_s
{
    off_t           i_pos;
    vtsi_mat_t      mat;
    /* Part Of Title Search Pointer Table Info */
    vts_ptt_srpt_t  ptt_srpt;
    /* Video Title Set Menu PGCI Unit Table */
    pgci_ut_t       pgci_ut;
    /* Video Title Set Program Chain Info Table */
    pgci_inf_t      pgci_ti;
    /* Video Title Set Time Map Table */
    vts_tmap_ti_t   tmap_ti;
    /* VTSM Cell Adress Table Information */
    c_adt_t         m_c_adt;
    /* VTSM VOBU Adress Map Table */
    vobu_admap_t    m_vobu_admap;
    /* VTS Cell Adress Table Information */
    c_adt_t         c_adt;
    /* VTS VOBU Adress Map Table */
    vobu_admap_t    vobu_admap;
} vts_t;

/*
 *  Global Ifo Structure
 */
typedef struct ifo_s
{
    /* File descriptor for the device */
    int             i_fd;
    /* Offset to video_ts.ifo on the device */
    off_t           i_off;
    /* Position of stream pointer */
    off_t           i_pos;
    /* Error Management */
    boolean_t       b_error;
    /* Current title set number */
    int             i_title;
    /* Structure described in video_ts */
    vmg_t           vmg;
    /* Vts ifo for current title set */
    vts_t           vts;
} ifo_t;


/*****************************************************************************
 * dvd_ifo.h: Structures for ifo parsing
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: dvd_ifo.h,v 1.9 2001/04/01 07:31:38 stef Exp $
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
typedef struct ifo_video_s
{
    u8      i_compression         ;// 2;
    u8      i_system              ;// 2;
    u8      i_ratio               ;// 2;
    u8      i_perm_displ          ;// 2;

    u8      i_line21_1            ;// 1;
    u8      i_line21_2            ;// 1;
    u8      i_source_res          ;// 2;
    u8      i_letterboxed         ;// 1;
    u8      i_mode                ;// 1;
} ifo_video_t;

/* Audio type information */
typedef struct ifo_audio_s
{
    u8      i_coding_mode         ;// 3;
    u8      i_multichannel_extension  ;// 1;
    u8      i_type                ;// 2;
    u8      i_appl_mode           ;// 2;

    u8      i_quantization        ;// 2;
    u8      i_sample_freq         ;// 2;
//    u8                            ;// 1;
    u8      i_num_channels        ;// 3;
    u16     i_lang_code           ;// 16;   // <char> description
    u8      i_foo                 ;// 8;    // 0x00000000 ?
    u8      i_caption             ;// 8;
    u8      i_bar                 ;// 8;    // 0x00000000 ?
} ifo_audio_t;

typedef struct ifo_spu_t
{
    u16     i_prefix              ;// 16;   // 0x0100 ?
    u16     i_lang_code           ;// 16;   // <char> description
    u8      i_foo                 ;// 8;    // dont know
    u8      i_caption             ;// 8;    // 0x00 ?
} ifo_spu_t;



/* Ifo vitual machine Commands */
typedef struct command_desc_s
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
} command_desc_t;

/* Program Chain Command Table
  - start at i_pgc_com_tab_sbyte */
typedef struct command_s
{
    u16             i_pre_command_nb;               // 2 bytes
    u16             i_post_command_nb;              // 2 bytes
    u16             i_cell_command_nb;              // 2 bytes
//    char[2]         ???
    command_desc_t* p_pre_command;                  // i_pre_com_nb * 8 bytes
    command_desc_t* p_post_command;                 // i_post_com_nb * 8 bytes
    command_desc_t* p_cell_command;                 // i_cell_com_nb * 8 bytes
} command_t;

/* Program Chain Map Table
 * - start at "i_pgc_prg_map_sbyte" */
typedef struct chapter_map_s
{
    u8*             pi_start_cell;              // i_prg_nb * 1 byte 
} chapter_map_t;

/* Cell Playback Information Table
 * we have a pointer to such a structure for each cell  
 * - first start at "i_cell_play_inf_sbyte" */
typedef struct cell_play_s
{
    /* This information concerns the currently selected cell */
    u16             i_category;                      // 2 bytes
    u8              i_still_time;               // 1 byte; in seconds; ff=inf
    u8              i_command_nb;                   // 1 byte; 0 = no com
    u32             i_play_time;                // 4 bytes
    u32             i_start_sector;             // 4 bytes
    u32             i_first_ilvu_vobu_esector;  // 4 bytes; ???
    u32             i_last_vobu_start_sector;            // 4 bytes
    u32             i_end_sector;                  // 4 bytes
} cell_play_t;

/* Cell Position Information Table
 * we have a pointer to such a structure for each cell 
 * - first start at "i_cell_pos_inf_sbyte" */
typedef struct cell_pos_s
{
    /* This information concerns the currently selected cell */
    u16             i_vob_id;                   // 2 bytes
//    char            ???
    u8              i_cell_id;                  // 1 byte
} cell_pos_t;

/* Main structure for Program Chain
 * - start at i_fp_pgc_sbyte
 * - or at i_vmgm_pgci_sbyte in vmgm_pgci_srp_t */
typedef struct title_s
{
    /* Global features of program chain */
//    char[2]         ???
    u8              i_chapter_nb;                   // 1 byte
    u8              i_cell_nb;                  // 1 byte
    u32             i_play_time;                // 4 bytes
    u32             i_prohibited_user_op;       // 4 bytes
    u16             pi_audio_status[8];         // 8*2 bytes
    u32             pi_subpic_status[32];       // 32*4 bytes
    u16             i_next_title_num;              // 2 bytes
    u16             i_prev_title_num;              // 2 bytes
    u16             i_go_up_title_num;              // 2 bytes
    u8              i_still_time;               // 1 byte ; in seconds
    u8              i_play_mode;                // 1 byte
    /* In video_ts.ifo, the 3 significant bytes of each color are
     * preceded by 1 unsignificant byte */
    u32             pi_yuv_color[16];           // 16*3 bytes
    /* Here come the start bytes of the following structures */
    u16             i_command_start_byte;            // 2 bytes
    u16             i_chapter_map_start_byte;            // 2 bytes
    u16             i_cell_play_start_byte;      // 2 bytes
    u16             i_cell_pos_start_byte;       // 2 bytes
    /* Predefined structures */
    command_t       command;
    chapter_map_t   chapter_map;
    cell_play_t*    p_cell_play;           // i_cell_nb * 24 bytes
    cell_pos_t*     p_cell_pos;             // i_cell_nb * 4 bytes
} title_t;

/*
 * Menu PGCI Unit Table
 */

/* Menu PGCI Language unit Descriptor */
typedef struct unit_s
{
    char            ps_lang_code[2];            // 2 bytes (ISO-xx)
//    char            ???
    u8              i_existence_mask;           // 1 byte
    u32             i_unit_inf_start_byte;                 // 4 bytes
} unit_t;

typedef struct unit_title_s
{
    u8              i_category_mask;             // 1 byte
    u8              i_category;                  // 1 byte
    u16             i_parental_mask;                 // 2 bytes
    u32             i_title_start_byte;               // 4 bytes
    title_t         title;
} unit_title_t;

/* Menu PGCI Language Unit Table 
 * - start at i_lu_sbyte */
typedef struct unit_inf_s
{
    u16             i_title_nb;                   // 2 bytes
//    char[2]         ???
    u32             i_end_byte;                 // 4 bytes
    unit_title_t *  p_title;                      // i_srp_nb * 8 bytes
} unit_inf_t;

/* Main Struct for Menu PGCI
 * - start at i_*_pgci_ut_ssector */
typedef struct title_unit_s
{
    u16             i_unit_nb;                    // 2 bytes; ???
//    char[2]         ???
    u32             i_end_byte;                    // 4 bytes
    unit_t*         p_unit;                       // i_lu_nb * 8 bytes
    unit_inf_t*     p_unit_inf;                 // i_lu_nb * 8 bytes
} title_unit_t;

/*
 * Cell Adress Table Information
 */
typedef struct cell_map_s
{
    u16             i_vob_id;                   // 2 bytes
    u8              i_cell_id;                  // 1 byte
//    char            ???
    u32             i_start_sector;                  // 4 bytes
    u32             i_end_sector;                  // 4 bytes
} cell_map_t;

typedef struct cell_inf_s
{
    u16             i_vob_nb;                   // 2 bytes
//    char[2]         ???
    u32             i_end_byte;                    // 4 bytes
    u16             i_cell_nb;                  // not in ifo; computed
                                                // with e_byte
    cell_map_t*     p_cell_map;
} cell_inf_t;


/*
 * VOBU Adress Map Table
 */
typedef struct vobu_map_s
{
    u32             i_end_byte;                    // 4 bytes
    u32*            pi_vobu_start_sector;            // (nb of vobu) * 4 bytes
} vobu_map_t;

/*****************************************************************************
 * Structures for Video Management (cf video_ts.ifo)
 *****************************************************************************/

/* 
 * Video Manager Information Management Table
 */ 
typedef struct manager_inf_s
{
    char            psz_id[13];                 // 12 bytes (DVDVIDEO-VMG)
    u32             i_vmg_end_sector;                  // 4 bytes
//    char[12]        ???
    u32             i_vmg_inf_end_sector;                // 4 bytes
//    char            ???
    u8              i_spec_ver;                 // 1 byte
    u32             i_cat;                      // 4 bytes
    u16             i_volume_nb;                   // 2 bytes
    u16             i_volume;                      // 2 bytes
    u8              i_disc_side;                // 1 bytes
//    char[20]        ???
    u16             i_title_set_nb;                   // 2 bytes
    char            ps_provider_id[32];         // 32 bytes
    u64             i_pos_code;                 // 8 bytes
//    char[24]        ???
    u32             i_vmg_inf_end_byte;              // 4 bytes
    u32             i_first_play_title_start_byte;             // 4 bytes
//    char[56]        ???
    u32             i_vob_start_sector;             // 4 bytes
    u32             i_title_inf_start_sector;         // 4 bytes
    u32             i_title_unit_start_sector;          // 4 bytes
    u32             i_parental_inf_start_sector;         // 4 bytes
    u32             i_vts_inf_start_sector;         // 4 bytes
    u32             i_text_data_start_sector;         // 4 bytes
    u32             i_cell_inf_start_sector;            // 4 bytes
    u32             i_vobu_map_start_sector;       // 4 bytes
//    char[2]         ???
    ifo_video_t     video_attr;                 // 2 bytes
//    char            ???
    u8              i_audio_nb;                 // 1 byte
    ifo_audio_t     p_audio_attr[8];            // i_vmgm_audio_nb * 8 bytes
//    char[16]        ???
    u8              i_spu_nb;                // 1 byte
    ifo_spu_t       p_spu_attr[32];          // i_subpic_nb * 6 bytes
} manager_inf_t;


/* 
 * Part Of Title Search Pointer Table Information
 */

/* Title sets structure
 * we have a pointer to this structure for each tts */
typedef struct title_attr_s
{
    u8              i_play_type;                // 1 byte
    u8              i_angle_nb;                 // 1 byte
    u16             i_chapter_nb;                  // 2 bytes; Chapters/PGs
    u16             i_parental_id;              // 2 bytes
    u8              i_title_set_num;            // 1 byte (VTS#)
    u8              i_title_num;                 // 1 byte ???
    u32             i_start_sector;              // 4 bytes
} title_attr_t;

/* Main struct for tts
 * - start at "i_vmg_ptt_srpt_ssector" */
typedef struct title_inf_s
{
    u16             i_title_nb;                   // 2 bytes
//    char[2]         ???
    u32             i_end_byte;                    // 4 bytes
    title_attr_t *  p_attr;                     // i_ttu_nb * 12 bytes
} title_inf_t;

/*
 * Parental Management Information Table
 */
typedef struct parental_desc_s
{
    char            ps_country_code[2];         // 2 bytes
//    char[2]         ???
    u16             i_parental_mask_start_byte;            // 2 bytes
//    char[2]         ???
} parental_desc_t;

typedef struct parental_mask_s
{
    u16*            ppi_mask[8];            // (i_vts_nb +1) * 8 * 2 bytes
} parental_mask_t;

/* Main struct for parental management
 * - start at i_vmg_ptl_mait_ssector */
typedef struct parental_inf_s
{
    u16             i_country_nb;               // 2 bytes
    u16             i_vts_nb;                   // 2 bytes
    u32             i_end_byte;                    // 4 bytes
    parental_desc_t* p_parental_desc;             // i_country_nb * 8 bytes
    parental_mask_t* p_parental_mask;        // i_country_nb * sizeof(vmg_ptl_mask_t)
} parental_inf_t;

/*
 * Video Title Set Attribute Table
 */

/* Attribute structure : one for each vts
 * - start at pi_atrt_sbyte */
typedef struct vts_attr_s
{
    u32             i_end_byte;                    // 4 bytes
    u32             i_cat_app_type;             // 4 bytes
    ifo_video_t     vts_menu_video_attr;          // 2 bytes
//    char            ???
    u8              i_vts_menu_audio_nb;            // 1 byte
    ifo_audio_t     p_vts_menu_audio_attr[8];       // 8 * 8 bytes
//    char[17]        ???
    u8              i_vts_menu_spu_nb;           // 1 byte
    ifo_spu_t       p_vts_menu_spu_attr[28];     // i_vtsm_subpic_nb * 6 bytes
//    char[2]         ???
    ifo_video_t     vts_title_video_attr;         // 2 bytes
//    char            ???
    u8              i_vts_title_audio_nb;           // 1 byte
    ifo_audio_t     p_vts_title_audio_attr[8];      // 8 * 8 bytes
//    char[17]        ???
    u8              i_vts_title_spu_nb;          // 1 byte
    ifo_spu_t       p_vts_title_spu_attr[28];    // i_vtstt_subpic_nb * 6 bytes
} vts_attr_t;

/* Main struct for vts attributes
 * - start at i_vmg_vts_atrt_ssector */
typedef struct vts_inf_s
{
    u16             i_vts_nb;                   // 2 bytes
//    char[2]         ???
    u32             i_end_byte;                    // 4 bytes
    u32*            pi_vts_attr_start_byte;          // i_vts_nb * 4 bytes
    vts_attr_t*     p_vts_attr;
} vts_inf_t;

/* 
 * Global Structure for Video Manager
 */
typedef struct vmg_s 
{
    manager_inf_t       manager_inf;
    title_t             title;
    title_inf_t         title_inf;
    title_unit_t        title_unit;
    parental_inf_t      parental_inf;
    vts_inf_t           vts_inf;
    cell_inf_t          cell_inf;
    vobu_map_t          vobu_map;
} vmg_t;

/*****************************************************************************
 * Structures for Video Title Sets (cf vts_*.ifo)
 ****************************************************************************/

/* 
 * Video Title Sets Information Management Table
 */ 
typedef struct vts_manager_s
{
    char            psz_id[13];                 // 12 bytes (DVDVIDEO-VTS)
    u32             i_end_sector;                  // 4 bytes
//    char[12]        ???
    u32             i_inf_end_sector;                // 4 bytes
//    char            ???
    u8              i_spec_ver;                 // 1 byte
    u32             i_cat;                      // 4 bytes
//    char[90]        ???
    u32             i_inf_end_byte;                // 4 bytes
//    char[60]        ???
    u32             i_menu_vob_start_sector;           // 4 bytes
    u32             i_title_vob_start_sector;          // 4 bytes
    u32             i_title_inf_start_sector;         // 4 bytes
    u32             i_title_unit_start_sector;            // 4 bytes
    u32             i_menu_unit_start_sector;        // 4 bytes
    u32             i_time_inf_start_sector;          // 4 bytes
    u32             i_menu_cell_inf_start_sector;          // 4 bytes
    u32             i_menu_vobu_map_start_sector;     // 4 bytes
    u32             i_cell_inf_start_sector;            // 4 bytes
    u32             i_vobu_map_start_sector;       // 4 bytes
//    char[24]        ???
    ifo_video_t     menu_video_attr;               // 2 bytes
//    char            ???
    u8              i_menu_audio_nb;               // 1 byte
    ifo_audio_t     p_menu_audio_attr[8];          // i_vmgm_audio_nb * 8 bytes
//    char[16]        ???
    u8              i_menu_spu_nb;              // 1 byte
    ifo_spu_t       p_menu_spu_attr[32];        // i_subpic_nb * 6 bytes
                                                // !!! only 28 subpics ???
//    char[2]         ???
    ifo_video_t     video_attr;                 // 2 bytes
//    char            ???
    u8              i_audio_nb;                 // 1 byte
    ifo_audio_t     p_audio_attr[8];            // i_vmgm_audio_nb * 8 bytes
//    char[16]        ???
    u8              i_spu_nb;                // 1 byte
    ifo_spu_t       p_spu_attr[32];          // i_subpic_nb * 6 bytes
} vts_manager_t;

/* 
 * Part Of Title Search Pointer Table Information
 */

/* Title sets structure
 * we have a pointer to this structure for each tts */
typedef struct title_start_s
{
    u16             i_program_chain_num;                   // 2 bytes; Chapters/PGs
    u16             i_program_num;                   // 2 bytes
} title_start_t;

/* Main struct for tts
 * - start at "i_vts_ptt_srpt_ssector" */
typedef struct vts_title_s
{
    u16             i_title_nb;                   // 2 bytes
//    char[2]         ???
    u32             i_end_byte;                    // 4 bytes
    u32*            pi_start_byte;
    title_start_t * p_title_start;                      // i_ttu_nb * 4 bytes
} vts_title_t;

/*
 * Time Map table information
 */

/* Time Map structure */
typedef struct time_map_s
{
    u8              i_time_unit;                // 1 byte
//    char            ???
    u16             i_entry_nb;                 // 2 bytes
    u32*            pi_sector;                  // i_entry_nb * 4 bytes
} time_map_t;

/* Main structure for tmap_ti
 * - start at "i_tmap_ti_ssector" */
typedef struct time_inf_s
{
    u16             i_nb;                       // 2 bytes
//    char[2]         ???
    u32             i_end_byte;                    // 4 bytes
    u32*            pi_start_byte;                   // i_tmap_nb * 4 bytes
    time_map_t*     p_time_map;
} time_inf_t;

/*
 * Video Title Set 
 */
typedef struct vts_s
{
    boolean_t       b_initialized;
    off_t           i_pos;
    vts_manager_t   manager_inf;
    vts_title_t     title_inf;
    title_unit_t    menu_unit;
    unit_inf_t      title_unit;
    time_inf_t      time_inf;
    cell_inf_t      menu_cell_inf;
    vobu_map_t      menu_vobu_map;
    cell_inf_t      cell_inf;
    vobu_map_t      vobu_map;
} vts_t;

/*
 *  Global Ifo Structure
 */
typedef struct ifo_s
{
    int             i_fd;           /* File descriptor for the device */
    off_t           i_off;          /* Offset to video_ts.ifo on the device */
    off_t           i_pos;          /* Position of stream pointer */
    boolean_t       b_error;        /* Error Management */
    vmg_t           vmg;            /* Structure described in video_ts */
    int             i_title;        /* Current title number */
    vts_t           vts;            /* Vts ifo for current title set */
} ifo_t;


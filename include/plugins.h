/*****************************************************************************
 * plugins.h : Dynamic plugin management functions
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 *
 * Authors:
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

#ifdef SYS_BEOS
typedef int plugin_id_t;
#define GET_PLUGIN( p_func, plugin_id, psz_name ) \
    get_image_symbol( plugin_id, psz_name, B_SYMBOL_TYPE_TEXT, &p_func );
#else
typedef void *plugin_id_t;
#define GET_PLUGIN( p_func, plugin_id, psz_name ) \
    p_func = dlsym( plugin_id, psz_name );
#endif

typedef struct plugin_info_s
{
    plugin_id_t plugin_id;

    char *      psz_filename;
    char *      psz_name;
    char *      psz_version;
    char *      psz_author;

    void *      aout_GetPlugin;
    void *      vout_GetPlugin;
    void *      intf_GetPlugin;
    void *      yuv_GetPlugin;

    int         i_score;
} plugin_info_t;

typedef struct plugin_bank_s
{
    int               i_plugin_count;
    plugin_info_t *   p_info[ MAX_PLUGIN_COUNT ];
} plugin_bank_t;

plugin_bank_t *  bank_Create       ( void );
void             bank_Init         ( plugin_bank_t * p_bank );
void             bank_Destroy      ( plugin_bank_t * p_bank );


/*****************************************************************************
 * dynamicoverlay_commands.c : dynamic overlay plugin commands
 *****************************************************************************
 * Copyright (C) 2008 VLC authors and VideoLAN
 *
 * Author: Søren Bøg <avacore@videolan.org>
 *         Jean-Paul Saman <jpsaman@videolan.org>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_arrays.h>
#include <vlc_vout.h>
#include <vlc_filter.h>

#include <limits.h>
#include <string.h>
#include <ctype.h>

#if defined(HAVE_SYS_SHM_H)
#include <sys/shm.h>
#endif

#include "dynamicoverlay.h"


/*****************************************************************************
 * overlay_t: Overlay descriptor
 *****************************************************************************/

overlay_t *OverlayCreate( void )
{
    overlay_t *p_ovl = calloc( 1, sizeof( overlay_t ) );
    if( p_ovl == NULL )
       return NULL;

    p_ovl->i_x = p_ovl->i_y = 0;
    p_ovl->i_alpha = 0xFF;
    p_ovl->b_active = false;
    video_format_Setup( &p_ovl->format, VLC_FOURCC( '\0','\0','\0','\0') , 0, 0,
                        0, 0, 1, 1 );
    p_ovl->p_fontstyle = text_style_Create( STYLE_NO_DEFAULTS );
    p_ovl->data.p_text = NULL;

    return p_ovl;
}

int OverlayDestroy( overlay_t *p_ovl )
{
    free( p_ovl->data.p_text );
    text_style_Delete( p_ovl->p_fontstyle );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Command parsers
 *****************************************************************************/
static int skip_space( char **psz_command )
{
    char *psz_temp = *psz_command;

    while( isspace( (unsigned char)*psz_temp ) )
    {
        ++psz_temp;
    }
    if( psz_temp == *psz_command )
    {
        return VLC_EGENERIC;
    }
    *psz_command = psz_temp;
    return VLC_SUCCESS;
}

static int parse_digit( char **psz_command, int32_t *value )
{
    char *psz_temp;
    long l = strtol( *psz_command, &psz_temp, 10 );

    if( psz_temp == *psz_command )
    {
        return VLC_EGENERIC;
    }
#if LONG_MAX > INT32_MAX
    if( l > INT32_MAX || l < INT32_MIN )
        return VLC_EGENERIC;
#endif
    *value = l;
    *psz_command = psz_temp;
    return VLC_SUCCESS;
}

static int parse_char( char **psz_command, char **psz_end,
                       int count, char *psz_value )
{
    if( *psz_end - *psz_command < count )
    {
        return VLC_EGENERIC;
    }
    memcpy( psz_value, *psz_command, count );
    *psz_command += count;
    return VLC_SUCCESS;
}

static int parser_DataSharedMem( char *psz_command,
                                 char *psz_end,
                                 commandparams_t *p_params )
{
    /* Parse: 0 128 128 RGBA 9404459 */
    skip_space( &psz_command );
    if( isdigit( (unsigned char)*psz_command ) )
    {
        if( parse_digit( &psz_command, &p_params->i_id ) == VLC_EGENERIC )
            return VLC_EGENERIC;
    }
    skip_space( &psz_command );
    if( isdigit( (unsigned char)*psz_command ) )
    {
        if( parse_digit( &psz_command, &p_params->i_width ) == VLC_EGENERIC )
            return VLC_EGENERIC;
    }
    skip_space( &psz_command );
    if( isdigit( (unsigned char)*psz_command ) )
    {
        if( parse_digit( &psz_command, &p_params->i_height ) == VLC_EGENERIC )
            return VLC_EGENERIC;
    }
    skip_space( &psz_command );
    if( isascii( (unsigned char)*psz_command ) )
    {
        if( parse_char( &psz_command, &psz_end, 4, (char*)&p_params->fourcc )
            == VLC_EGENERIC )
            return VLC_EGENERIC;
    }
    skip_space( &psz_command );
    if( isdigit( (unsigned char)*psz_command ) )
    {
        if( parse_digit( &psz_command, &p_params->i_shmid ) == VLC_EGENERIC )
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

static int parser_Id( char *psz_command, char *psz_end,
                      commandparams_t *p_params )
{
    VLC_UNUSED(psz_end);
    skip_space( &psz_command );
    if( isdigit( (unsigned char)*psz_command ) )
    {
        if( parse_digit( &psz_command, &p_params->i_id ) == VLC_EGENERIC )
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

static int parser_None( char *psz_command, char *psz_end,
                        commandparams_t *p_params )
{
    VLC_UNUSED(psz_command);
    VLC_UNUSED(psz_end);
    VLC_UNUSED(p_params);
    return VLC_SUCCESS;
}

static int parser_SetAlpha( char *psz_command, char *psz_end,
                            commandparams_t *p_params )
{
    VLC_UNUSED(psz_end);
    skip_space( &psz_command );
    if( isdigit( (unsigned char)*psz_command ) )
    {
        if( parse_digit( &psz_command, &p_params->i_id ) == VLC_EGENERIC  )
            return VLC_EGENERIC;
    }
    skip_space( &psz_command );
    if( isdigit( (unsigned char)*psz_command ) )
    {
        if( parse_digit( &psz_command, &p_params->i_alpha ) == VLC_EGENERIC )
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

static int parser_SetPosition( char *psz_command, char *psz_end,
                               commandparams_t *p_params )
{
    VLC_UNUSED(psz_end);
    skip_space( &psz_command );
    if( isdigit( (unsigned char)*psz_command ) )
    {
        if( parse_digit( &psz_command, &p_params->i_id ) == VLC_EGENERIC )
            return VLC_EGENERIC;
    }
    skip_space( &psz_command );
    if( isdigit( (unsigned char)*psz_command ) )
    {
        if( parse_digit( &psz_command, &p_params->i_x ) == VLC_EGENERIC )
            return VLC_EGENERIC;
    }
    skip_space( &psz_command );
    if( isdigit( (unsigned char)*psz_command ) )
    {
        if( parse_digit( &psz_command, &p_params->i_y ) == VLC_EGENERIC )
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

static int parser_SetTextAlpha( char *psz_command, char *psz_end,
                                commandparams_t *p_params )
{
    VLC_UNUSED(psz_end);
    skip_space( &psz_command );
    if( isdigit( (unsigned char)*psz_command ) )
    {
        if( parse_digit( &psz_command, &p_params->i_id ) == VLC_EGENERIC )
            return VLC_EGENERIC;
    }
    skip_space( &psz_command );
    if( isdigit( (unsigned char)*psz_command ) )
    {
        int32_t value;

        if( parse_digit( &psz_command, &value ) == VLC_EGENERIC )
            return VLC_EGENERIC;

        p_params->fontstyle.i_font_alpha = value;
    }
    return VLC_SUCCESS;
}

static int parser_SetTextColor( char *psz_command, char *psz_end,
                                commandparams_t *p_params )
{
    int r = 0, g = 0, b = 0;
    VLC_UNUSED(psz_end);

    skip_space( &psz_command );
    if( isdigit( (unsigned char)*psz_command ) )
    {
        if( parse_digit( &psz_command, &p_params->i_id ) == VLC_EGENERIC )
            return VLC_EGENERIC;
    }
    skip_space( &psz_command );
    if( isdigit( (unsigned char)*psz_command ) )
    {
        if( parse_digit( &psz_command, &r ) == VLC_EGENERIC )
            return VLC_EGENERIC;
    }
    skip_space( &psz_command );
    if( isdigit( (unsigned char)*psz_command ) )
    {
        if( parse_digit( &psz_command, &g ) == VLC_EGENERIC )
            return VLC_EGENERIC;
    }
    skip_space( &psz_command );
    if( isdigit( (unsigned char)*psz_command ) )
    {
        if( parse_digit( &psz_command, &b ) == VLC_EGENERIC )
            return VLC_EGENERIC;
    }
    p_params->fontstyle.i_font_color = (r<<16) | (g<<8) | (b<<0);
    return VLC_SUCCESS;
}

static int parser_SetTextSize( char *psz_command, char *psz_end,
                               commandparams_t *p_params )
{
    VLC_UNUSED(psz_end);
    skip_space( &psz_command );
    if( isdigit( (unsigned char)*psz_command ) )
    {
        if( parse_digit( &psz_command, &p_params->i_id ) == VLC_EGENERIC )
            return VLC_EGENERIC;
    }
    skip_space( &psz_command );
    if( isdigit( (unsigned char)*psz_command ) )
    {
        if( parse_digit( &psz_command, &p_params->fontstyle.i_font_size ) == VLC_EGENERIC )
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

static int parser_SetVisibility( char *psz_command, char *psz_end,
                                 commandparams_t *p_params )
{
    VLC_UNUSED(psz_end);
    skip_space( &psz_command );
    if( isdigit( (unsigned char)*psz_command ) )
    {
        if( parse_digit( &psz_command, &p_params->i_id ) == VLC_EGENERIC )
            return VLC_EGENERIC;
    }
    skip_space( &psz_command );
    if( isdigit( (unsigned char)*psz_command ) )
    {
        int32_t i_vis = 0;
        if( parse_digit( &psz_command, &i_vis ) == VLC_EGENERIC )
            return VLC_EGENERIC;
        p_params->b_visible = (i_vis == 1) ? true : false;
    }
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Command unparser functions
 *****************************************************************************/

static int unparse_default( const commandparams_t *p_results,
                            buffer_t *p_output )
{
    VLC_UNUSED(p_results);
    VLC_UNUSED(p_output);
    return VLC_SUCCESS;
}

static int unparse_GenImage( const commandparams_t *p_results,
                             buffer_t *p_output )
{
    int ret = BufferPrintf( p_output, " %d", p_results->i_id );
    if( ret != VLC_SUCCESS )
        return ret;

    return VLC_SUCCESS;
}

static int unparse_GetAlpha( const commandparams_t *p_results,
                             buffer_t *p_output )
{
    int ret = BufferPrintf( p_output, " %d", p_results->i_alpha );
    if( ret != VLC_SUCCESS )
        return ret;

    return VLC_SUCCESS;
}

static int unparse_GetPosition( const commandparams_t *p_results,
                                buffer_t *p_output )
{
    int ret = BufferPrintf( p_output, " %d", p_results->i_x );
    if( ret != VLC_SUCCESS )
        return ret;

    ret = BufferPrintf( p_output, " %d", p_results->i_y );
    if( ret != VLC_SUCCESS )
        return ret;

    return VLC_SUCCESS;
}

static int unparse_GetTextAlpha( const commandparams_t *p_results,
                                 buffer_t *p_output )
{
    int ret = BufferPrintf( p_output, " %d", p_results->fontstyle.i_font_alpha );
    if( ret != VLC_SUCCESS )
        return ret;

    return VLC_SUCCESS;
}

static int unparse_GetTextColor( const commandparams_t *p_results,
                                 buffer_t *p_output )
{
    int ret = BufferPrintf( p_output, " %d", (p_results->fontstyle.i_font_color & 0xff0000)>>16 );
    if( ret != VLC_SUCCESS )
        return ret;

    ret = BufferPrintf( p_output, " %d", (p_results->fontstyle.i_font_color & 0x00ff00)>>8 );
    if( ret != VLC_SUCCESS )
        return ret;

    ret = BufferPrintf( p_output, " %d", (p_results->fontstyle.i_font_color & 0x0000ff) );
    if( ret != VLC_SUCCESS )
        return ret;

    return VLC_SUCCESS;
}

static int unparse_GetTextSize( const commandparams_t *p_results,
                                buffer_t *p_output )
{
    int ret = BufferPrintf( p_output, " %d", p_results->fontstyle.i_font_size );
    if( ret != VLC_SUCCESS )
        return ret;

    return VLC_SUCCESS;
}

static int unparse_GetVisibility( const commandparams_t *p_results,
                             buffer_t *p_output )
{
    int ret = BufferPrintf( p_output, " %d", (p_results->b_visible ? 1 : 0) );
    if( ret != VLC_SUCCESS ) {
        return ret;
    }
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Command functions
 *****************************************************************************/
static int exec_DataSharedMem( filter_t *p_filter,
                               const commandparams_t *p_params,
                               commandparams_t *p_results )
{
#if defined(HAVE_SYS_SHM_H)
    filter_sys_t *p_sys = p_filter->p_sys;
    struct shmid_ds shminfo;
    overlay_t *p_ovl;
    size_t i_size;

    VLC_UNUSED(p_results);

    p_ovl = ListGet( &p_sys->overlays, p_params->i_id );
    if( p_ovl == NULL )
    {
        msg_Err( p_filter, "Invalid overlay: %d", p_params->i_id );
        return VLC_EGENERIC;
    }

    if( shmctl( p_params->i_shmid, IPC_STAT, &shminfo ) == -1 )
    {
        msg_Err( p_filter, "Unable to access shared memory" );
        return VLC_EGENERIC;
    }
    i_size = shminfo.shm_segsz;

    if( p_params->fourcc == VLC_CODEC_TEXT )
    {
        char *p_data;

        if( (p_params->i_height != 1) || (p_params->i_width < 1) )
        {
            msg_Err( p_filter,
                     "Invalid width and/or height. when specifying text height "
                     "must be 1 and width the number of bytes in the string, "
                     "including the null terminator" );
            return VLC_EGENERIC;
        }

        if( (size_t)p_params->i_width > i_size )
        {
            msg_Err( p_filter,
                     "Insufficient data in shared memory. need %d, got %zu",
                     p_params->i_width, i_size );
            return VLC_EGENERIC;
        }

        p_ovl->data.p_text = malloc( p_params->i_width );
        if( p_ovl->data.p_text == NULL )
        {
            msg_Err( p_filter, "Unable to allocate string storage" );
            return VLC_ENOMEM;
        }

        video_format_Setup( &p_ovl->format, VLC_CODEC_TEXT,
                            0, 0, 0, 0, 0, 1 );

        p_data = shmat( p_params->i_shmid, NULL, SHM_RDONLY );
        if( p_data == NULL )
        {
            msg_Err( p_filter, "Unable to attach to shared memory" );
            free( p_ovl->data.p_text );
            p_ovl->data.p_text = NULL;
            return VLC_ENOMEM;
        }
        memcpy( p_ovl->data.p_text, p_data, p_params->i_width );

        shmdt( p_data );
    }
    else
    {
        uint8_t *p_data, *p_in;
        size_t i_neededsize = 0;

        p_ovl->data.p_pic = picture_New( p_params->fourcc,
                                         p_params->i_width, p_params->i_height,
                                         1, 1 );
        if( p_ovl->data.p_pic == NULL )
            return VLC_ENOMEM;

        p_ovl->format = p_ovl->data.p_pic->format;

        for( size_t i_plane = 0; i_plane < (size_t)p_ovl->data.p_pic->i_planes;
             ++i_plane )
        {
            i_neededsize += p_ovl->data.p_pic->p[i_plane].i_visible_lines *
                            p_ovl->data.p_pic->p[i_plane].i_visible_pitch;
        }

        if( i_neededsize > i_size )
        {
            msg_Err( p_filter,
                     "Insufficient data in shared memory. need %zu, got %zu",
                     i_neededsize, i_size );
            picture_Release( p_ovl->data.p_pic );
            p_ovl->data.p_pic = NULL;
            return VLC_EGENERIC;
        }

        p_data = shmat( p_params->i_shmid, NULL, SHM_RDONLY );
        if( p_data == NULL )
        {
            msg_Err( p_filter, "Unable to attach to shared memory" );
            picture_Release( p_ovl->data.p_pic );
            p_ovl->data.p_pic = NULL;
            return VLC_ENOMEM;
        }

        p_in = p_data;
        for( size_t i_plane = 0; i_plane < (size_t)p_ovl->data.p_pic->i_planes;
             ++i_plane )
        {
            uint8_t *p_out = p_ovl->data.p_pic->p[i_plane].p_pixels;
            for( size_t i_line = 0;
                 i_line < (size_t)p_ovl->data.p_pic->p[i_plane].i_visible_lines;
                 ++i_line )
            {
                memcpy( p_out, p_in,
                            p_ovl->data.p_pic->p[i_plane].i_visible_pitch );
                p_out += p_ovl->data.p_pic->p[i_plane].i_pitch;
                p_in += p_ovl->data.p_pic->p[i_plane].i_visible_pitch;
            }
        }
        shmdt( p_data );
    }
    p_sys->b_updated = p_ovl->b_active;

    return VLC_SUCCESS;
#else
    VLC_UNUSED(p_params);
    VLC_UNUSED(p_results);

    msg_Err( p_filter, "system doesn't support shared memory" );
    return VLC_EGENERIC;
#endif
}

static int exec_DeleteImage( filter_t *p_filter,
                             const commandparams_t *p_params,
                             commandparams_t *p_results )
{
    VLC_UNUSED(p_results);
    filter_sys_t *p_sys = p_filter->p_sys;
    p_sys->b_updated = true;

    return ListRemove( &p_sys->overlays, p_params->i_id );
}

static int exec_EndAtomic( filter_t *p_filter,
                           const commandparams_t *p_params,
                           commandparams_t *p_results )
{
    VLC_UNUSED(p_params);
    VLC_UNUSED(p_results);
    filter_sys_t *p_sys = p_filter->p_sys;
    QueueTransfer( &p_sys->pending, &p_sys->atomic );
    p_sys->b_atomic = false;
    return VLC_SUCCESS;
}

static int exec_GenImage( filter_t *p_filter,
                          const commandparams_t *p_params,
                          commandparams_t *p_results )
{
    VLC_UNUSED(p_params);
    filter_sys_t *p_sys = p_filter->p_sys;

    overlay_t *p_ovl = OverlayCreate();
    if( p_ovl == NULL )
        return VLC_ENOMEM;

    ssize_t i_idx = ListAdd( &p_sys->overlays, p_ovl );
    if( i_idx < 0 )
        return i_idx;

    p_results->i_id = i_idx;
    return VLC_SUCCESS;
}

static int exec_GetAlpha( filter_t *p_filter,
                          const commandparams_t *p_params,
                          commandparams_t *p_results )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    overlay_t *p_ovl = ListGet( &p_sys->overlays, p_params->i_id );
    if( p_ovl == NULL )
        return VLC_EGENERIC;

    p_results->i_alpha = p_ovl->i_alpha;
    return VLC_SUCCESS;
}

static int exec_GetPosition( filter_t *p_filter,
                             const commandparams_t *p_params,
                             commandparams_t *p_results )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    overlay_t *p_ovl = ListGet( &p_sys->overlays, p_params->i_id );
    if( p_ovl == NULL )
        return VLC_EGENERIC;

    p_results->i_x = p_ovl->i_x;
    p_results->i_y = p_ovl->i_y;
    return VLC_SUCCESS;
}

static int exec_GetTextAlpha( filter_t *p_filter,
                              const commandparams_t *p_params,
                              commandparams_t *p_results )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    overlay_t *p_ovl = ListGet( &p_sys->overlays, p_params->i_id );
    if( p_ovl == NULL )
        return VLC_EGENERIC;

    p_results->fontstyle.i_font_alpha = p_ovl->p_fontstyle->i_font_alpha;
    p_results->fontstyle.i_features |= STYLE_HAS_FONT_ALPHA;
    return VLC_SUCCESS;
}

static int exec_GetTextColor( filter_t *p_filter,
                              const commandparams_t *p_params,
                              commandparams_t *p_results )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    overlay_t *p_ovl = ListGet( &p_sys->overlays, p_params->i_id );
    if( p_ovl == NULL )
        return VLC_EGENERIC;

    p_results->fontstyle.i_font_color = p_ovl->p_fontstyle->i_font_color;
    p_results->fontstyle.i_features |= STYLE_HAS_FONT_COLOR;
    return VLC_SUCCESS;
}

static int exec_GetTextSize( filter_t *p_filter,
                             const commandparams_t *p_params,
                             commandparams_t *p_results )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    overlay_t *p_ovl = ListGet( &p_sys->overlays, p_params->i_id );
    if( p_ovl == NULL )
        return VLC_EGENERIC;

    p_results->fontstyle.i_font_size = p_ovl->p_fontstyle->i_font_size;
    return VLC_SUCCESS;
}

static int exec_GetVisibility( filter_t *p_filter,
                               const commandparams_t *p_params,
                               commandparams_t *p_results )
{
    filter_sys_t *p_sys = p_filter->p_sys;

    overlay_t *p_ovl = ListGet( &p_sys->overlays, p_params->i_id );
    if( p_ovl == NULL )
        return VLC_EGENERIC;

    p_results->b_visible = p_ovl->b_active ? 1 : 0;
    return VLC_SUCCESS;
}

static int exec_SetAlpha( filter_t *p_filter,
                          const commandparams_t *p_params,
                          commandparams_t *p_results )
{
    VLC_UNUSED(p_results);
    filter_sys_t *p_sys = p_filter->p_sys;

    overlay_t *p_ovl = ListGet( &p_sys->overlays, p_params->i_id );
    if( p_ovl == NULL )
        return VLC_EGENERIC;

    p_ovl->i_alpha = p_params->i_alpha;
    p_sys->b_updated = p_ovl->b_active;
    return VLC_SUCCESS;
}

static int exec_SetPosition( filter_t *p_filter,
                             const commandparams_t *p_params,
                             commandparams_t *p_results )
{
    VLC_UNUSED(p_results);
    filter_sys_t *p_sys = p_filter->p_sys;

    overlay_t *p_ovl = ListGet( &p_sys->overlays, p_params->i_id );
    if( p_ovl == NULL )
        return VLC_EGENERIC;

    p_ovl->i_x = p_params->i_x;
    p_ovl->i_y = p_params->i_y;

    p_sys->b_updated = p_ovl->b_active;
    return VLC_SUCCESS;
}

static int exec_SetTextAlpha( filter_t *p_filter,
                              const commandparams_t *p_params,
                              commandparams_t *p_results )
{
    VLC_UNUSED(p_results);
    filter_sys_t *p_sys = p_filter->p_sys;

    overlay_t *p_ovl = ListGet( &p_sys->overlays, p_params->i_id );
    if( p_ovl == NULL )
        return VLC_EGENERIC;

    p_ovl->p_fontstyle->i_font_alpha = p_params->fontstyle.i_font_alpha;
    p_ovl->p_fontstyle->i_features |= STYLE_HAS_FONT_ALPHA;
    p_sys->b_updated = p_ovl->b_active;
    return VLC_SUCCESS;
}

static int exec_SetTextColor( filter_t *p_filter,
                              const commandparams_t *p_params,
                              commandparams_t *p_results )
{
    VLC_UNUSED(p_results);
    filter_sys_t *p_sys = p_filter->p_sys;

    overlay_t *p_ovl = ListGet( &p_sys->overlays, p_params->i_id );
    if( p_ovl == NULL )
        return VLC_EGENERIC;

    p_ovl->p_fontstyle->i_font_color = p_params->fontstyle.i_font_color;
    p_ovl->p_fontstyle->i_features |= STYLE_HAS_FONT_COLOR;
    p_sys->b_updated = p_ovl->b_active;
    return VLC_SUCCESS;
}

static int exec_SetTextSize( filter_t *p_filter,
                              const commandparams_t *p_params,
                              commandparams_t *p_results )
{
    VLC_UNUSED(p_results);
    filter_sys_t *p_sys = p_filter->p_sys;

    overlay_t *p_ovl = ListGet( &p_sys->overlays, p_params->i_id );
    if( p_ovl == NULL )
        return VLC_EGENERIC;

    p_ovl->p_fontstyle->i_font_size = p_params->fontstyle.i_font_size;
    p_sys->b_updated = p_ovl->b_active;
    return VLC_SUCCESS;
}

static int exec_SetVisibility( filter_t *p_filter,
                               const commandparams_t *p_params,
                               commandparams_t *p_results )
{
    VLC_UNUSED(p_results);
    filter_sys_t *p_sys = p_filter->p_sys;

    overlay_t *p_ovl = ListGet( &p_sys->overlays, p_params->i_id );
    if( p_ovl == NULL )
        return VLC_EGENERIC;

    p_ovl->b_active = p_params->b_visible;// ? false : true;
    p_sys->b_updated = true;
    return VLC_SUCCESS;
}

static int exec_StartAtomic( filter_t *p_filter,
                             const commandparams_t *p_params,
                             commandparams_t *p_results )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    VLC_UNUSED(p_params);
    VLC_UNUSED(p_results);

    p_sys->b_atomic = true;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Command functions
 *****************************************************************************/
static const commanddesc_static_t p_commands[] =
{
    {   .psz_command = "DataSharedMem",
        .b_atomic = true,
        .pf_parser = parser_DataSharedMem,
        .pf_execute = exec_DataSharedMem,
        .pf_unparse = unparse_default,
    },
    {   .psz_command = "DeleteImage",
        .b_atomic = true,
        .pf_parser = parser_Id,
        .pf_execute = exec_DeleteImage,
        .pf_unparse = unparse_default,
    },
    {   .psz_command = "EndAtomic",
        .b_atomic = false,
        .pf_parser = parser_None,
        .pf_execute = exec_EndAtomic,
        .pf_unparse = unparse_default,
    },
    {   .psz_command = "GenImage",
        .b_atomic = false,
        .pf_parser = parser_None,
        .pf_execute = exec_GenImage,
        .pf_unparse = unparse_GenImage,
    },
    {   .psz_command = "GetAlpha",
        .b_atomic = false,
        .pf_parser = parser_Id,
        .pf_execute = exec_GetAlpha,
        .pf_unparse = unparse_GetAlpha,
    },
    {   .psz_command = "GetPosition",
        .b_atomic = false,
        .pf_parser = parser_Id,
        .pf_execute = exec_GetPosition,
        .pf_unparse = unparse_GetPosition,
    },
    {   .psz_command = "GetTextAlpha",
        .b_atomic = false,
        .pf_parser = parser_Id,
        .pf_execute = exec_GetTextAlpha,
        .pf_unparse = unparse_GetTextAlpha,
    },
    {   .psz_command = "GetTextColor",
        .b_atomic = false,
        .pf_parser = parser_Id,
        .pf_execute = exec_GetTextColor,
        .pf_unparse = unparse_GetTextColor,
    },
    {   .psz_command = "GetTextSize",
        .b_atomic = true,
        .pf_parser = parser_Id,
        .pf_execute = exec_GetTextSize,
        .pf_unparse = unparse_GetTextSize,
    },
    {   .psz_command = "GetVisibility",
        .b_atomic = false,
        .pf_parser = parser_Id,
        .pf_execute = exec_GetVisibility,
        .pf_unparse = unparse_GetVisibility,
    },
    {   .psz_command = "SetAlpha",
        .b_atomic = true,
        .pf_parser = parser_SetAlpha,
        .pf_execute = exec_SetAlpha,
        .pf_unparse = unparse_default,
    },
    {   .psz_command = "SetPosition",
        .b_atomic = true,
        .pf_parser = parser_SetPosition,
        .pf_execute = exec_SetPosition,
        .pf_unparse = unparse_default,
    },
    {   .psz_command = "SetTextAlpha",
        .b_atomic = true,
        .pf_parser = parser_SetTextAlpha,
        .pf_execute = exec_SetTextAlpha,
        .pf_unparse = unparse_default,
    },
    {   .psz_command = "SetTextColor",
        .b_atomic = true,
        .pf_parser = parser_SetTextColor,
        .pf_execute = exec_SetTextColor,
        .pf_unparse = unparse_default,
    },
    {   .psz_command = "SetTextSize",
        .b_atomic = true,
        .pf_parser = parser_SetTextSize,
        .pf_execute = exec_SetTextSize,
        .pf_unparse = unparse_default,
    },
    {   .psz_command = "SetVisibility",
        .b_atomic = true,
        .pf_parser = parser_SetVisibility,
        .pf_execute = exec_SetVisibility,
        .pf_unparse = unparse_default,
    },
    {   .psz_command = "StartAtomic",
        .b_atomic = true,
        .pf_parser = parser_None,
        .pf_execute = exec_StartAtomic,
        .pf_unparse = unparse_default,
    }
};

void RegisterCommand( filter_t *p_filter )
{
    filter_sys_t *p_sys = p_filter->p_sys;

    p_sys->i_commands = ARRAY_SIZE(p_commands);
    p_sys->pp_commands = (commanddesc_t **) calloc( p_sys->i_commands, sizeof(commanddesc_t*) );
    if( !p_sys->pp_commands ) return;
    for( size_t i_index = 0; i_index < p_sys->i_commands; i_index ++ )
    {
        p_sys->pp_commands[i_index] = (commanddesc_t *) malloc( sizeof(commanddesc_t) );
        if( !p_sys->pp_commands[i_index] ) return;
        p_sys->pp_commands[i_index]->psz_command = strdup( p_commands[i_index].psz_command );
        p_sys->pp_commands[i_index]->b_atomic = p_commands[i_index].b_atomic;
        p_sys->pp_commands[i_index]->pf_parser = p_commands[i_index].pf_parser;
        p_sys->pp_commands[i_index]->pf_execute = p_commands[i_index].pf_execute;
        p_sys->pp_commands[i_index]->pf_unparse = p_commands[i_index].pf_unparse;
    }

    msg_Dbg( p_filter, "%zu commands are available", p_sys->i_commands );
    for( size_t i_index = 0; i_index < p_sys->i_commands; i_index++ )
        msg_Dbg( p_filter, "    %s", p_sys->pp_commands[i_index]->psz_command );
}

void UnregisterCommand( filter_t *p_filter )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    size_t i_index = 0;

    for( i_index = 0; i_index < p_sys->i_commands; i_index++ )
    {
        free( p_sys->pp_commands[i_index]->psz_command );
        free( p_sys->pp_commands[i_index] );
    }
    free( p_sys->pp_commands );
    p_sys->pp_commands = NULL;
    p_sys->i_commands = 0;
}

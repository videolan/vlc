/*****************************************************************************
 * osd_text.c : Filter to put text on the video, using freetype2
 *****************************************************************************
 * Copyright (C) 2002, 2003 VideoLAN
 * $Id: osd_text.c,v 1.4 2003/05/18 12:18:46 gbazin Exp $
 *
 * Authors: Sigmund Augdal <sigmunau@idi.ntnu.no>
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
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>

#include <vlc/vlc.h>
#include <vlc/vout.h>
#include <osd.h>
#include <math.h>

#include "filter_common.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H

#ifdef WIN32
#define FT_RENDER_MODE_NORMAL 0
#endif

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create    ( vlc_object_t * );
static void Destroy   ( vlc_object_t * );

static int  Init      ( vout_thread_t * );
static void End       ( vout_thread_t * );
static void Render    ( vout_thread_t *, picture_t * );

static int  SendEvents( vlc_object_t *, char const *,
                        vlc_value_t, vlc_value_t, void * );
static int  SetMargin ( vlc_object_t *, char const *,
                        vlc_value_t, vlc_value_t, void * );
static int  AddText   ( vlc_object_t *, char const *,
                        vlc_value_t, vlc_value_t, void * );


/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define FONT_TEXT N_("Font")
#define FONT_LONGTEXT N_("Filename of Font")
#define FONTSIZE_TEXT N_("Font size")
#define FONTSIZE_LONGTEXT N_("The size of the fonts used by the osd module" )

vlc_module_begin();
    add_category_hint( N_("OSD"), NULL, VLC_FALSE );
    add_file( "osd-font", "", NULL, FONT_TEXT, FONT_LONGTEXT, VLC_FALSE );
    add_integer( "osd-fontsize", 16, NULL, FONTSIZE_TEXT, FONTSIZE_LONGTEXT, VLC_FALSE );
    set_description( _("osd text filter") );
    set_capability( "video filter", 0 );
    add_shortcut( "text" );
    set_callbacks( Create, Destroy );
vlc_module_end();

/**
 Describes a string to be displayed on the video, or a linked list of
 such
*/
typedef struct string_info_s string_info_t;
struct string_info_s
{
    string_info_t *p_next;
    int            i_x_margin;
    int            i_y_margin;
    int            i_width;
    int            i_height;
    int            i_flags;
    mtime_t        i_start_date;
    mtime_t        i_end_date;
    char          *psz_text;
    FT_Glyph      *pp_glyphs;
    FT_Vector     *p_glyph_pos;
};

/*****************************************************************************
 * vout_sys_t: osd_text local data
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the osd-text specific properties of an output thread.
 *****************************************************************************/
struct vout_sys_t
{
    int            i_clones;
    vout_thread_t *p_vout;
    FT_Library     p_library;   /* handle to library     */
    FT_Face        p_face;      /* handle to face object */
    string_info_t *p_strings;
    int            i_x_margin;
    int            i_y_margin;
    int            i_flags;
    int            i_duration;
    mtime_t        i_start_date;
    mtime_t        i_end_date;
    vlc_mutex_t   *lock;
    vlc_bool_t     i_use_kerning;
    uint8_t        pi_gamma[256];
};
/* more prototypes */
static void ComputeBoundingBox( string_info_t * );
static void FreeString( string_info_t * );

/*****************************************************************************
 * Create: allocates osd-text video thread output method
 *****************************************************************************
 * This function allocates and initializes a Clone vout method.
 *****************************************************************************/
#define gamma_value 2.0
static int Create( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    char *psz_fontfile;
    int i, i_error;
    vlc_value_t val;
    double gamma_inv = 1.0f / gamma_value;
    
    /* Allocate structure */
    p_vout->p_sys = malloc( sizeof( vout_sys_t ) );
    if( p_vout->p_sys == NULL )
    {
        msg_Err( p_vout, "out of memory" );
        return VLC_ENOMEM;
    }
    p_vout->p_sys->p_strings = NULL;
    p_vout->p_sys->i_x_margin = 50;
    p_vout->p_sys->i_y_margin = 50;
    p_vout->p_sys->i_flags = 0;
    
    p_vout->p_sys->i_duration = 2000000;
    p_vout->p_sys->i_start_date = 0;
    p_vout->p_sys->i_end_date = 0;
    p_vout->p_sys->lock = malloc( sizeof(vlc_mutex_t));
    if( p_vout->p_sys->lock == NULL )
    {
        msg_Err( p_vout, "out of memory" );
        return VLC_ENOMEM;
    }
    vlc_mutex_init( p_vout, p_vout->p_sys->lock);

    for (i = 0; i < 256; i++) {
        p_vout->p_sys->pi_gamma[i] =
            (uint8_t)( pow( (double)i / 255.0f, gamma_inv) * 255.0f );
        msg_Dbg( p_vout, "%d", p_vout->p_sys->pi_gamma[i]);
    }
    p_vout->pf_init = Init;
    p_vout->pf_end = End;
    p_vout->pf_manage = NULL;
    p_vout->pf_render = Render;
    p_vout->pf_display = NULL;

    /* Look what method was requested */
    psz_fontfile = config_GetPsz( p_vout, "osd-font" );
    i_error = FT_Init_FreeType( &p_vout->p_sys->p_library );
    if( i_error )
    {
        msg_Err( p_vout, "couldn't initialize freetype" );
        free( p_vout->p_sys );
        return VLC_EGENERIC;
    }
    i_error = FT_New_Face( p_vout->p_sys->p_library, psz_fontfile, 0,
                           &p_vout->p_sys->p_face );
    if( i_error == FT_Err_Unknown_File_Format )
    {
        msg_Err( p_vout, "file %s have unknown format", psz_fontfile );
        free( p_vout->p_sys );
        return VLC_EGENERIC;
    }
    else if( i_error )
    {
        msg_Err( p_vout, "failed to load font file" );
        free( p_vout->p_sys );
        return VLC_EGENERIC;
    }
    p_vout->p_sys->i_use_kerning = FT_HAS_KERNING(p_vout->p_sys->p_face);
    
    i_error = FT_Set_Pixel_Sizes( p_vout->p_sys->p_face, 0, config_GetInt( p_vout, "osd-fontsize" ) );    
    if( i_error )
    {
        msg_Err( p_vout, "couldn't set font size to %d",
                 config_GetInt( p_vout, "osd-fontsize" ) );
        free( p_vout->p_sys );
        return VLC_EGENERIC;
    }
    var_Create( p_vout, "lock", VLC_VAR_MUTEX );
    var_Create( p_vout, "flags", VLC_VAR_INTEGER | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_vout, "flags", SetMargin, NULL );
    var_Create( p_vout, "x-margin", VLC_VAR_INTEGER | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_vout, "x-margin", SetMargin, NULL );
    var_Create( p_vout, "y-margin", VLC_VAR_INTEGER | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_vout, "y-margin", SetMargin, NULL );
    var_Create( p_vout, "duration", VLC_VAR_INTEGER | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_vout, "duration", SetMargin, NULL );
    var_Create( p_vout, "start-date", VLC_VAR_TIME | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_vout, "start-date", SetMargin, NULL );
    var_Create( p_vout, "end-date", VLC_VAR_TIME | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_vout, "end-date", SetMargin, NULL );
    var_Create( p_vout, "string", VLC_VAR_STRING | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_vout, "string", AddText, NULL );

    val.psz_string = "Videolan";
//    var_Set( p_vout, "string", val );
//    p_vout->p_sys->p_strings->i_end_date = 0xFFFFFFFFFFFFFFF;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Init: initialize the video thread output method
 *****************************************************************************/
static int Init( vout_thread_t *p_vout )
{
    int i_index;
    picture_t *p_pic;

    I_OUTPUTPICTURES = 0;

    /* Initialize the output structure */
    p_vout->output.i_chroma = p_vout->render.i_chroma;
    p_vout->output.i_width  = p_vout->render.i_width;
    p_vout->output.i_height = p_vout->render.i_height;
    p_vout->output.i_aspect = p_vout->render.i_aspect;

    /* Try to open the real video output */
    msg_Dbg( p_vout, "spawning the real video output" );

    p_vout->p_sys->p_vout = vout_Create( p_vout,
                            p_vout->render.i_width, p_vout->render.i_height,
                            p_vout->render.i_chroma, p_vout->render.i_aspect );
    if( p_vout->p_sys->p_vout == NULL )
    {
        msg_Err( p_vout, "failed to start vout" );
        return VLC_EGENERIC;
    }

    ADD_CALLBACKS( p_vout->p_sys->p_vout, SendEvents );

    ALLOCATE_DIRECTBUFFERS( VOUT_MAX_PICTURES );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * End: terminate Clone video thread output method
 *****************************************************************************/
static void End( vout_thread_t *p_vout )
{
    int i_index;

    /* Free the fake output buffers we allocated */
    for( i_index = I_OUTPUTPICTURES ; i_index ; )
    {
        i_index--;
        free( PP_OUTPUTPICTURE[ i_index ]->p_data_orig );
    }
}

/*****************************************************************************
 * Destroy: destroy Clone video thread output method
 *****************************************************************************
 * Terminate an output method created by CloneCreateOutputMethod
 *****************************************************************************/
static void Destroy( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    string_info_t *p_string1, *p_string2;
    DEL_CALLBACKS( p_vout->p_sys->p_vout, SendEvents );
    vlc_object_detach( p_vout->p_sys->p_vout );
    vout_Destroy( p_vout->p_sys->p_vout );
    vlc_mutex_destroy( p_vout->p_sys->lock );
    p_string1 = p_vout->p_sys->p_strings;
    while( p_string1 )
    {
        p_string2 = p_string1->p_next;
        FreeString( p_string1 );
        p_string1 = p_string2;
    }
    FT_Done_Face( p_vout->p_sys->p_face );
    FT_Done_FreeType( p_vout->p_sys->p_library );
    free( p_vout->p_sys );
}

/*****************************************************************************
 * SendEvents: forward mouse and keyboard events to the parent p_vout
 *****************************************************************************/
static int SendEvents( vlc_object_t *p_this, char const *psz_var,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    var_Set( (vlc_object_t *)p_data, psz_var, newval );

    return VLC_SUCCESS;
}


/*****************************************************************************
 * Render: displays previously rendered output
 *****************************************************************************
 * This function send the currently rendered image to Clone image, waits
 * until it is displayed and switch the two rendering buffers, preparing next
 * frame.
 *****************************************************************************/
static void Render( vout_thread_t *p_vout, picture_t *p_pic )
{
    picture_t *p_outpic = NULL;
    int i_plane, i_error,x,y,pen_x, pen_y;
    unsigned int i;
    string_info_t *p_string;
    mtime_t date;
    
    while( ( p_outpic =
             vout_CreatePicture( p_vout->p_sys->p_vout, 0, 0, 0 )
               ) == NULL )
    {
        if( p_vout->b_die || p_vout->b_error )
        {
            vout_DestroyPicture(
                p_vout->p_sys->p_vout, p_outpic );
            return;
        }

        msleep( VOUT_OUTMEM_SLEEP );
    }
    vout_DatePicture( p_vout->p_sys->p_vout,
                      p_outpic, p_pic->date );
    if( p_vout->i_changes )
    {
        p_vout->p_sys->p_vout->i_changes = p_vout->i_changes;
        p_vout->i_changes = 0;
    }

    date = mdate();
    /* trash old strings */
    while( p_vout->p_sys->p_strings &&
           p_vout->p_sys->p_strings->i_end_date < date )
    {
        p_string = p_vout->p_sys->p_strings;
        p_vout->p_sys->p_strings = p_string->p_next;
        msg_Dbg( p_vout, "trashing string "I64Fd" < "I64Fd, p_string->i_end_date, date );
        FreeString( p_string );
    }


    vlc_mutex_lock( p_vout->p_sys->lock );

    for( i_plane = 0 ; i_plane < p_pic->i_planes ; i_plane++ )
    {
        uint8_t *p_in, *p_in_end, *p_out;
        int i_in_pitch = p_pic->p[i_plane].i_pitch;
        const int i_out_pitch = p_outpic->p[i_plane].i_pitch;
        const int i_copy_pitch = p_outpic->p[i_plane].i_visible_pitch;

        p_in = p_pic->p[i_plane].p_pixels;
        p_out = p_outpic->p[i_plane].p_pixels;

        if( i_in_pitch == i_copy_pitch
            && i_out_pitch == i_copy_pitch )
        {
            p_vout->p_vlc->pf_memcpy( p_out, p_in, i_in_pitch
                                      * p_outpic->p[i_plane].i_lines );
        }
        else
        {
            p_in_end = p_in + i_in_pitch * p_outpic->p[i_plane].i_lines;

            while( p_in < p_in_end )
            {
                p_vout->p_vlc->pf_memcpy( p_out, p_in, i_copy_pitch );
                p_in += i_in_pitch;
                p_out += i_out_pitch;
            }
        }
/*        pen_x = 20;
          pen_y = 100;*/
        if ( i_plane == 0 )
        {
            for( p_string = p_vout->p_sys->p_strings; p_string != NULL;
                 p_string = p_string->p_next )
            {
                if( p_string->i_start_date > date )
                {
                    continue;
                }
                if ( p_string->i_flags & OSD_ALIGN_BOTTOM )
                {
                    pen_y = p_outpic->p[i_plane].i_lines - p_string->i_height -
                        p_string->i_y_margin;
                }
                else
                {
                    pen_y = p_string->i_y_margin;
                }
                if ( p_string->i_flags & OSD_ALIGN_RIGHT )
                {
                    pen_x = i_out_pitch - p_string->i_width
                        - p_string->i_x_margin;
                }
                else
                {
                    pen_x = p_string->i_x_margin;
                }

                for( i = 0; i < strlen( p_string->psz_text ); i++ )
                {
                    if( p_string->pp_glyphs[i] )
                    {
                        FT_Glyph p_glyph = p_string->pp_glyphs[i];
                        FT_BitmapGlyph p_image;
                        i_error = FT_Glyph_To_Bitmap( &p_glyph,
                                                      FT_RENDER_MODE_NORMAL,
                                                      &p_string->p_glyph_pos[i],
                                                      0 );
                        if ( i_error ) continue;
                        p_image = (FT_BitmapGlyph)p_glyph;
#define alpha p_vout->p_sys->pi_gamma[p_image->bitmap.buffer[x+ y*p_image->bitmap.width]]
#define pixel p_out[(p_string->p_glyph_pos[i].y + pen_y + y - p_image->top)*i_out_pitch+x+pen_x+p_string->p_glyph_pos[i].x+p_image->left]
                        for(y = 0; y < p_image->bitmap.rows; y++ )
                        {
                            for( x = 0; x < p_image->bitmap.width; x++ )
                            {
//                                pixel = alpha;
//                                pixel = (pixel^alpha)^pixel;
                                pixel = ((pixel*(255-alpha))>>8) + (255*alpha>>8);
                            }
                        }
                        FT_Done_Glyph( p_glyph );
                    }
                }
            }
        }
    }
    vlc_mutex_unlock( p_vout->p_sys->lock );

    vout_DisplayPicture( p_vout->p_sys->p_vout, p_outpic );
}

static int  SetMargin ( vlc_object_t *p_this, char const *psz_command,
                        vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    vout_thread_t *p_vout = (vout_thread_t*)p_this;
    if( !strcmp( psz_command, "x-margin" ) )
    {
        p_vout->p_sys->i_x_margin = newval.i_int;
    }
    else if( !strcmp( psz_command, "y-margin" ) )
    {
        p_vout->p_sys->i_y_margin = newval.i_int;
    }
    else if( !strcmp( psz_command, "duration" ) )
    {
        p_vout->p_sys->i_duration = newval.i_int;
        msg_Dbg( p_vout, "setting duration %d", p_vout->p_sys->i_duration );
    }
    else if( !strcmp( psz_command, "start-date" ) )
    {
        p_vout->p_sys->i_start_date = ( (mtime_t) newval.time.i_high << 32 )
            + newval.time.i_low;
    }
    else if( !strcmp( psz_command, "end-date" ) )
    {
        p_vout->p_sys->i_end_date = ( (mtime_t) newval.time.i_high << 32 )
            + newval.time.i_low;
    }
    else if( !strcmp( psz_command, "flags" ) )
    {
        p_vout->p_sys->i_flags = newval.i_int;
    }
    else
    {
        msg_Err( p_vout, "Invalid command" );
        return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

static int  AddText ( vlc_object_t *p_this, char const *psz_command,
                      vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    vout_thread_t *p_vout = (vout_thread_t*)p_this;
    string_info_t **pp_string;
    string_info_t *p_string;
    char *psz_string;
    int i, i_pen_x, i_error, i_glyph_index, i_previous;
    
    p_string = malloc( sizeof(string_info_t) );
    p_string->i_flags = p_vout->p_sys->i_flags;
    p_string->i_x_margin = p_vout->p_sys->i_x_margin;
    p_string->i_y_margin = p_vout->p_sys->i_y_margin;
    if( p_vout->p_sys->i_start_date && p_vout->p_sys->i_end_date )
    {
        p_string->i_end_date = p_vout->p_sys->i_end_date;
        p_string->i_start_date = p_vout->p_sys->i_start_date;
    }
    else
    {
        p_string->i_end_date = mdate() + p_vout->p_sys->i_duration;
        p_string->i_start_date = 0;
    }
    p_vout->p_sys->i_end_date = 0;
    p_vout->p_sys->i_start_date = 0;
    p_string->psz_text = strdup( newval.psz_string );
    p_string->pp_glyphs = malloc( sizeof(FT_GlyphSlot)
                                  * strlen( p_string->psz_text ) );
    if( p_string->pp_glyphs == NULL )
    {
        msg_Err( p_this, "Out of memory" );
        return VLC_ENOMEM;
    }
    p_string->p_glyph_pos = malloc( sizeof( FT_Vector )
                                  * strlen( p_string->psz_text ) );
    if( p_string->p_glyph_pos == NULL )
    {
        msg_Err( p_this, "Out of memory" );
        return VLC_ENOMEM;
    }

    /* Calculate relative glyph positions and a bounding box for the
     * entire string */
    i_pen_x = 0;
    i_previous = 0;
    psz_string = p_string->psz_text;
    for( i = 0; *psz_string; i++, psz_string++ )
    {
#define face p_vout->p_sys->p_face
#define glyph face->glyph
        if ( *psz_string == '\n' )
        {
            i_pen_x = 0;
            p_string->pp_glyphs[ i ] = NULL;
            continue;
        }
        i_glyph_index = FT_Get_Char_Index( face, *psz_string);
        if ( p_vout->p_sys->i_use_kerning && i_glyph_index
            && i_previous )
        {
            FT_Vector delta;
            FT_Get_Kerning( face, i_previous, i_glyph_index,
                            ft_kerning_default, &delta );
            i_pen_x += delta.x >> 6;
            
        }
        p_string->p_glyph_pos[ i ].x = i_pen_x;
        p_string->p_glyph_pos[ i ].y = 0;
        i_error = FT_Load_Glyph( face, i_glyph_index, FT_LOAD_DEFAULT );
        if ( i_error )
        {
            msg_Err( p_this, "FT_Load_Glyph returned %d", i_error );
            return VLC_EGENERIC;
        }
        i_error = FT_Get_Glyph( glyph, &p_string->pp_glyphs[ i ] );
        if ( i_error )
        {
            msg_Err( p_this, "FT_Get_Glyph returned %d", i_error );
            return VLC_EGENERIC;
        }
        
        i_previous = i_glyph_index;
        i_pen_x += glyph->advance.x >> 6;
    }

    ComputeBoundingBox( p_string );
    msg_Dbg( p_this, "string height is %d width is %d", p_string->i_height, p_string->i_width );
    p_string->p_next = NULL;
    msg_Dbg( p_this, "adding string \"%s\" at (%d,%d) start_date "I64Fd
             " end_date" I64Fd, p_string->psz_text, p_string->i_x_margin,
             p_string->i_y_margin, p_string->i_start_date,
             p_string->i_end_date );
    vlc_mutex_lock( p_vout->p_sys->lock );
    pp_string  = &p_vout->p_sys->p_strings;
    while( *pp_string && (*pp_string)->i_end_date < p_string->i_end_date )
    {        
        pp_string = &(*pp_string)->p_next;
    }
    p_string->p_next = (*pp_string);
    *pp_string = p_string;
    vlc_mutex_unlock( p_vout->p_sys->lock );
    return VLC_SUCCESS;
}

static void ComputeBoundingBox( string_info_t *p_string )
{
    unsigned int i;
    int i_pen_y = 0;
    int i_firstline_height = 0;
    FT_Vector result;
    FT_BBox line;
    FT_BBox glyph_size;

    result.x = 0;
    result.y = 0;
    line.xMin = 0;
    line.xMax = 0;
    line.yMin = 0;
    line.yMax = 0;
    for ( i = 0; i < strlen( p_string->psz_text ); i++ )
    {
        if ( p_string->psz_text[i] == '\n' )
        {
            result.x = __MAX( result.x, line.xMax );
            result.y += line.yMax - line.yMin;
            if ( !i_firstline_height )
            {
                i_firstline_height = result.y;
            }
            line.xMin = 0;
            line.xMax = 0;
            line.yMin = 0;
            line.yMax = 0;
            i_pen_y = result.y + 1;
            continue;
        }
        p_string->p_glyph_pos[ i ].y = i_pen_y;
        FT_Glyph_Get_CBox( p_string->pp_glyphs[i],
                           ft_glyph_bbox_pixels, &glyph_size );
        /* Do rest */
        line.xMax = p_string->p_glyph_pos[i].x + glyph_size.xMax - glyph_size.xMin;
        line.yMax = __MAX( line.yMax, glyph_size.yMax );
        line.yMin = __MIN( line.yMin, glyph_size.yMin );
    }
    result.x = __MAX( result.x, line.xMax );
    result.y += line.yMax - line.yMin;
    p_string->i_height = result.y;
    p_string->i_width = result.x;
    if ( !i_firstline_height )
    {
        i_firstline_height = result.y;
    }
    for ( i = 0; i < strlen( p_string->psz_text ); i++ )
    {
        p_string->p_glyph_pos[ i ].y += i_firstline_height;
    }
    return;    
}

static void FreeString( string_info_t *p_string )
{
    unsigned int i;
    for ( i = 0; i < strlen( p_string->psz_text ); i++ )
    {
        if ( p_string->pp_glyphs[ i ] )
        {
            FT_Done_Glyph( p_string->pp_glyphs[ i ] );
        }
    }
    free( p_string->psz_text );
    free( p_string->p_glyph_pos );
    free( p_string->pp_glyphs );
    free( p_string );
}


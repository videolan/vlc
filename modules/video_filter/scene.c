/*****************************************************************************
 * scene.c : scene video filter (based on modules/video_output/image.c)
 *****************************************************************************
 * Copyright (C) 2004-2008 VLC authors and VideoLAN
 *
 * Authors: Jean-Paul Saman <jpsaman@videolan.org>
 *          Clément Stenac <zorglub@videolan.org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <limits.h>
#include <errno.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_filter.h>
#include <vlc_picture.h>
#include "filter_picture.h"
#include <vlc_image.h>
#include <vlc_strings.h>
#include <vlc_fs.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create      ( vlc_object_t * );
static void Destroy     ( vlc_object_t * );

static picture_t *Filter( filter_t *, picture_t * );

static void SnapshotRatio( filter_t *p_filter, picture_t *p_pic );
static void SavePicture( filter_t *, picture_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define FORMAT_TEXT N_( "Image format" )
#define FORMAT_LONGTEXT N_( "Format of the output images (png, jpeg, ...)." )

#define WIDTH_TEXT N_( "Image width" )
#define WIDTH_LONGTEXT N_( "You can enforce the image width. By default " \
                            "(-1) VLC will adapt to the video " \
                            "characteristics.")

#define HEIGHT_TEXT N_( "Image height" )
#define HEIGHT_LONGTEXT N_( "You can enforce the image height. By default " \
                            "(-1) VLC will adapt to the video " \
                            "characteristics.")

#define RATIO_TEXT N_( "Recording ratio" )
#define RATIO_LONGTEXT N_( "Ratio of images to record. "\
                           "3 means that one image out of three is recorded." )

#define PREFIX_TEXT N_( "Filename prefix" )
#define PREFIX_LONGTEXT N_( "Prefix of the output images filenames. Output " \
                            "filenames will have the \"prefixNUMBER.format\" "\
                            "form if replace is not true." )

#define PATH_TEXT N_( "Directory path prefix" )
#define PATH_LONGTEXT N_( "Directory path where images files should be saved. " \
                          "If not set, then images will be automatically saved in " \
                          "users homedir." )

#define REPLACE_TEXT N_( "Always write to the same file" )
#define REPLACE_LONGTEXT N_( "Always write to the same file instead of " \
                            "creating one file per image. In this case, " \
                             "the number is not appended to the filename." )

#define SCENE_HELP N_("Send your video to picture files")
#define CFG_PREFIX "scene-"

vlc_module_begin ()
    set_shortname( N_( "Scene filter" ) )
    set_description( N_( "Scene video filter" ) )
    set_help(SCENE_HELP)
    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_VFILTER )
    set_capability( "video filter", 0 )

    /* General options */
    add_string(  CFG_PREFIX "format", "png",
                 FORMAT_TEXT, FORMAT_LONGTEXT, false )
    add_integer( CFG_PREFIX "width", -1,
                 WIDTH_TEXT, WIDTH_LONGTEXT, true )
    add_integer( CFG_PREFIX "height", -1,
                 HEIGHT_TEXT, HEIGHT_LONGTEXT, true )
    add_string(  CFG_PREFIX "prefix", "scene",
                 PREFIX_TEXT, PREFIX_LONGTEXT, false )
    add_string(  CFG_PREFIX "path", NULL,
                 PATH_TEXT, PATH_LONGTEXT, false )
    add_bool(    CFG_PREFIX "replace", false,
                 REPLACE_TEXT, REPLACE_LONGTEXT, false )

    /* Snapshot method */
    add_integer_with_range( CFG_PREFIX "ratio", 50, 1, INT_MAX,
                            RATIO_TEXT, RATIO_LONGTEXT, false )

    set_callbacks( Create, Destroy )
vlc_module_end ()

static const char *const ppsz_vfilter_options[] = {
    "format", "width", "height", "ratio", "prefix", "path", "replace", NULL
};

typedef struct scene_t {
    picture_t       *p_pic;
    video_format_t  format;
} scene_t;

/*****************************************************************************
 * filter_sys_t: private data
 *****************************************************************************/
typedef struct
{
    image_handler_t *p_image;
    scene_t scene;

    char *psz_path;
    char *psz_prefix;
    char *psz_format;
    vlc_fourcc_t i_format;
    int32_t i_width;
    int32_t i_height;
    int32_t i_ratio;  /* save every n-th frame */
    int32_t i_frames; /* frames count */
    bool  b_replace;
} filter_sys_t;

/*****************************************************************************
 * Create: initialize and set pf_video_filter()
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys;

    const vlc_chroma_description_t *p_chroma =
        vlc_fourcc_GetChromaDescription( p_filter->fmt_in.video.i_chroma );
    if( p_chroma == NULL || p_chroma->plane_count == 0 )
        return VLC_EGENERIC;

    config_ChainParse( p_filter, CFG_PREFIX, ppsz_vfilter_options,
                       p_filter->p_cfg );

    p_filter->p_sys = p_sys = calloc( 1, sizeof( filter_sys_t ) );
    if( p_filter->p_sys == NULL )
        return VLC_ENOMEM;

    p_sys->p_image = image_HandlerCreate( p_this );
    if( !p_sys->p_image )
    {
        msg_Err( p_this, "Couldn't get handle to image conversion routines." );
        free( p_sys );
        return VLC_EGENERIC;
    }

    p_sys->psz_format = var_CreateGetString( p_this, CFG_PREFIX "format" );
    p_sys->i_format = image_Type2Fourcc( p_sys->psz_format );
    if( !p_sys->i_format )
    {
        msg_Err( p_filter, "Could not find FOURCC for image type '%s'",
                 p_sys->psz_format );
        image_HandlerDelete( p_sys->p_image );
        free( p_sys->psz_format );
        free( p_sys );
        return VLC_EGENERIC;
    }
    p_sys->i_width = var_CreateGetInteger( p_this, CFG_PREFIX "width" );
    p_sys->i_height = var_CreateGetInteger( p_this, CFG_PREFIX "height" );
    p_sys->i_ratio = var_CreateGetInteger( p_this, CFG_PREFIX "ratio" );
    if( p_sys->i_ratio <= 0)
        p_sys->i_ratio = 1;
    p_sys->b_replace = var_CreateGetBool( p_this, CFG_PREFIX "replace" );
    p_sys->psz_prefix = var_CreateGetString( p_this, CFG_PREFIX "prefix" );
    p_sys->psz_path = var_GetNonEmptyString( p_this, CFG_PREFIX "path" );
    if( p_sys->psz_path == NULL )
        p_sys->psz_path = config_GetUserDir( VLC_PICTURES_DIR );

    p_filter->pf_video_filter = Filter;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Destroy: destroy video filter method
 *****************************************************************************/
static void Destroy( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys = p_filter->p_sys;

    image_HandlerDelete( p_sys->p_image );

    if( p_sys->scene.p_pic )
        picture_Release( p_sys->scene.p_pic );
    free( p_sys->psz_format );
    free( p_sys->psz_prefix );
    free( p_sys->psz_path );
    free( p_sys );
}

/*****************************************************************************
 * Filter: Apply filtering logic to picture.
 *****************************************************************************/
static picture_t *Filter( filter_t *p_filter, picture_t *p_pic )
{
    /* TODO: think of some funky algorithm to detect scene changes. */
    SnapshotRatio( p_filter, p_pic );
    return p_pic;
}

static void SnapshotRatio( filter_t *p_filter, picture_t *p_pic )
{
    filter_sys_t *p_sys = (filter_sys_t *)p_filter->p_sys;

    if( !p_pic ) return;

    if( p_sys->i_frames % p_sys->i_ratio != 0 )
    {
        p_sys->i_frames++;
        return;
    }
    p_sys->i_frames++;

    if( p_sys->scene.p_pic )
        picture_Release( p_sys->scene.p_pic );

    if( (p_sys->i_width <= 0) && (p_sys->i_height > 0) )
    {
        p_sys->i_width = (p_pic->format.i_width * p_sys->i_height) / p_pic->format.i_height;
    }
    else if( (p_sys->i_height <= 0) && (p_sys->i_width > 0) )
    {
        p_sys->i_height = (p_pic->format.i_height * p_sys->i_width) / p_pic->format.i_width;
    }
    else if( (p_sys->i_width <= 0) && (p_sys->i_height <= 0) )
    {
        p_sys->i_width = p_pic->format.i_width;
        p_sys->i_height = p_pic->format.i_height;
    }

    p_sys->scene.p_pic = picture_NewFromFormat( &p_pic->format );
    if( p_sys->scene.p_pic )
    {
        picture_Copy( p_sys->scene.p_pic, p_pic );
        SavePicture( p_filter, p_sys->scene.p_pic );
    }
}

/*****************************************************************************
 * Save Picture to disk
 *****************************************************************************/
static void SavePicture( filter_t *p_filter, picture_t *p_pic )
{
    filter_sys_t *p_sys = (filter_sys_t *)p_filter->p_sys;
    video_format_t fmt_in, fmt_out;
    char *psz_filename = NULL;
    char *psz_temp = NULL;
    int i_ret;

    video_format_Init( &fmt_out, p_sys->i_format );

    /* Save snapshot psz_format to a memory zone */
    fmt_in = p_pic->format;
    fmt_out.i_sar_num = fmt_out.i_sar_den = 1;
    fmt_out.i_width = p_sys->i_width;
    fmt_out.i_height = p_sys->i_height;

    /*
     * Save the snapshot to a temporary file and
     * switch it to the real name afterwards.
     */
    if( p_sys->b_replace )
        i_ret = asprintf( &psz_filename, "%s" DIR_SEP "%s.%s",
                          p_sys->psz_path, p_sys->psz_prefix,
                          p_sys->psz_format );
    else
        i_ret = asprintf( &psz_filename, "%s" DIR_SEP "%s%05d.%s",
                          p_sys->psz_path, p_sys->psz_prefix,
                          p_sys->i_frames, p_sys->psz_format );

    if( i_ret == -1 )
    {
        msg_Err( p_filter, "could not create snapshot %s", psz_filename );
        goto error;
    }

    i_ret = asprintf( &psz_temp, "%s.swp", psz_filename );
    if( i_ret == -1 )
    {
        msg_Err( p_filter, "could not create snapshot temporarily file %s", psz_temp );
        goto error;
    }

    /* Save the image */
    i_ret = image_WriteUrl( p_sys->p_image, p_pic, &fmt_in, &fmt_out,
                            psz_temp );
    if( i_ret != VLC_SUCCESS )
    {
        msg_Err( p_filter, "could not create snapshot %s", psz_temp );
    }
    else
    {
        /* switch to the final destination */
#if defined (_WIN32) || defined(__OS2__)
        vlc_unlink( psz_filename );
#endif
        i_ret = vlc_rename( psz_temp, psz_filename );
        if( i_ret == -1 )
        {
            msg_Err( p_filter, "could not rename snapshot %s: %s",
                     psz_filename, vlc_strerror_c(errno) );
            goto error;
        }
    }

error:
    free( psz_temp );
    free( psz_filename );
}

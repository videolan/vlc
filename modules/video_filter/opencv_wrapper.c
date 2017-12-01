/*****************************************************************************
 * opencv_wrapper.c : OpenCV wrapper video filter
 *****************************************************************************
 * Copyright (C) 2006-2012 VLC authors and VideoLAN
 * Copyright (C) 2012 Edward Wang
 *
 * Authors: Dugal Harris <dugalh@protoclea.co.za>
 *          Edward Wang <edward.c.wang@compdigitec.com>
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

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout.h>
#include <vlc_modules.h>
#include <vlc_picture.h>
#include <vlc_filter.h>
#include <vlc_image.h>
#include "filter_picture.h"

#include <opencv2/core/core_c.h>
#include <opencv2/core/types_c.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create    ( vlc_object_t * );
static void Destroy   ( vlc_object_t * );

static picture_t* Filter( filter_t*, picture_t* );

static void ReleaseImages( filter_t* p_filter );
static void VlcPictureToIplImage( filter_t* p_filter, picture_t* p_in );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

static const char *const chroma_list[] = { "input", "I420", "RGB32"};
static const char *const chroma_list_text[] = { N_("Use input chroma unaltered"),
  N_("I420 - first plane is grayscale"), N_("RGB32")};

static const char *const output_list[] = { "none", "input", "processed"};
static const char *const output_list_text[] = { N_("Don't display any video"),
  N_("Display the input video"), N_("Display the processed video")};

static const char *const verbosity_list[] = { "error", "warning", "debug"};
static const char *const verbosity_list_text[] = { N_("Show only errors"),
  N_("Show errors and warnings"), N_("Show everything including debug messages")};

vlc_module_begin ()
    set_description( N_("OpenCV video filter wrapper") )
    set_shortname( N_("OpenCV" ))
    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_VFILTER )
    set_capability( "video filter", 0 )
    add_shortcut( "opencv_wrapper" )
    set_callbacks( Create, Destroy )
    add_float_with_range( "opencv-scale", 1.0, 0.1, 2.0,
                          N_("Scale factor (0.1-2.0)"),
                          N_("Amount by which to scale the picture before sending it to the internal OpenCV filter"),
                          false )
    add_string( "opencv-chroma", "input",
                          N_("OpenCV filter chroma"),
                          N_("Chroma to convert picture to before sending it to the internal OpenCV filter"), false);
        change_string_list( chroma_list, chroma_list_text )
    add_string( "opencv-output", "input",
                          N_("Wrapper filter output"),
                          N_("Determines what (if any) video is displayed by the wrapper filter"), false);
        change_string_list( output_list, output_list_text )
    add_string( "opencv-filter-name", "none",
                          N_("OpenCV internal filter name"),
                          N_("Name of internal OpenCV plugin filter to use"), false);
vlc_module_end ()


/*****************************************************************************
 * wrapper_output_t: what video is output
 *****************************************************************************/
enum wrapper_output_t
{
   NONE,
   VINPUT,
   PROCESSED
};

/*****************************************************************************
 * internal_chroma_t: what chroma is sent to the internal opencv filter
 *****************************************************************************/
enum internal_chroma_t
{
   CINPUT,
   GREY,
   RGB
};

/*****************************************************************************
 * filter_sys_t: opencv_wrapper video output method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the opencv_wrapper specific properties of an output thread.
 *****************************************************************************/
struct filter_sys_t
{
    image_handler_t *p_image;

    int i_cv_image_size;

    picture_t *p_proc_image;
    picture_t *p_to_be_freed;

    float f_scale;

    int i_wrapper_output;
    int i_internal_chroma;

    IplImage *p_cv_image[VOUT_MAX_PLANES];

    filter_t *p_opencv;
    char* psz_inner_name;

    picture_t hacked_pic;
};

/*****************************************************************************
 * Create: allocates opencv_wrapper video thread output method
 *****************************************************************************
 * This function allocates and initializes a opencv_wrapper vout method.
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    filter_t* p_filter = (filter_t*)p_this;
    char *psz_chroma, *psz_output;

    /* Allocate structure */
    p_filter->p_sys = malloc( sizeof( filter_sys_t ) );
    if( p_filter->p_sys == NULL )
        return VLC_ENOMEM;

    /* Load the internal OpenCV filter.
     *
     * This filter object is needed to call the internal OpenCV filter
     * for processing, the wrapper just converts into an IplImage* for
     * the other filter.
     *
     * We don't need to set up video formats for this filter as it not
     * actually using a picture_t.
     */
    p_filter->p_sys->p_opencv = vlc_object_create( p_filter, sizeof(filter_t) );
    if( !p_filter->p_sys->p_opencv ) {
        free( p_filter->p_sys );
        return VLC_ENOMEM;
    }

    p_filter->p_sys->psz_inner_name = var_InheritString( p_filter, "opencv-filter-name" );
    if( p_filter->p_sys->psz_inner_name )
        p_filter->p_sys->p_opencv->p_module =
            module_need( p_filter->p_sys->p_opencv,
                         "opencv internal filter",
                         p_filter->p_sys->psz_inner_name,
                         true );

    if( !p_filter->p_sys->p_opencv->p_module )
    {
        msg_Err( p_filter, "can't open internal opencv filter: %s", p_filter->p_sys->psz_inner_name );
        free( p_filter->p_sys->psz_inner_name );
        p_filter->p_sys->psz_inner_name = NULL;
        vlc_object_release( p_filter->p_sys->p_opencv );
        free( p_filter->p_sys );

        return VLC_ENOMOD;
    }


    /* Init structure */
    p_filter->p_sys->p_image = image_HandlerCreate( p_filter );
    for( int i = 0; i < VOUT_MAX_PLANES; i++ )
        p_filter->p_sys->p_cv_image[i] = NULL;
    p_filter->p_sys->p_proc_image = NULL;
    p_filter->p_sys->p_to_be_freed = NULL;
    p_filter->p_sys->i_cv_image_size = 0;

    /* Retrieve and apply config */
    psz_chroma = var_InheritString( p_filter, "opencv-chroma" );
    if( psz_chroma == NULL )
    {
        msg_Err( p_filter, "configuration variable %s empty, using 'grey'",
                         "opencv-chroma" );
        p_filter->p_sys->i_internal_chroma = GREY;
    } else if( !strcmp( psz_chroma, "input" ) )
        p_filter->p_sys->i_internal_chroma = CINPUT;
    else if( !strcmp( psz_chroma, "I420" ) )
        p_filter->p_sys->i_internal_chroma = GREY;
    else if( !strcmp( psz_chroma, "RGB32" ) )
        p_filter->p_sys->i_internal_chroma = RGB;
    else {
        msg_Err( p_filter, "no valid opencv-chroma provided, using 'grey'" );
        p_filter->p_sys->i_internal_chroma = GREY;
    }

    free( psz_chroma );

    psz_output = var_InheritString( p_filter, "opencv-output" );
    if( psz_output == NULL )
    {
        msg_Err( p_filter, "configuration variable %s empty, using 'input'",
                         "opencv-output" );
        p_filter->p_sys->i_wrapper_output = VINPUT;
    } else if( !strcmp( psz_output, "none" ) )
        p_filter->p_sys->i_wrapper_output = NONE;
    else if( !strcmp( psz_output, "input" ) )
        p_filter->p_sys->i_wrapper_output = VINPUT;
    else if( !strcmp( psz_output, "processed" ) )
        p_filter->p_sys->i_wrapper_output = PROCESSED;
    else {
        msg_Err( p_filter, "no valid opencv-output provided, using 'input'" );
        p_filter->p_sys->i_wrapper_output = VINPUT;
    }
    free( psz_output );

    p_filter->p_sys->f_scale =
        var_InheritFloat( p_filter, "opencv-scale" );

    msg_Info(p_filter, "Configuration: opencv-scale: %f, opencv-chroma: %d, "
        "opencv-output: %d, opencv-filter %s",
        p_filter->p_sys->f_scale,
        p_filter->p_sys->i_internal_chroma,
        p_filter->p_sys->i_wrapper_output,
        p_filter->p_sys->psz_inner_name);

#ifndef NDEBUG
    msg_Dbg( p_filter, "opencv_wrapper successfully started" );
#endif

    p_filter->pf_video_filter = Filter;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Destroy: destroy opencv_wrapper video thread output method
 *****************************************************************************
 * Terminate an output method created by opencv_wrapperCreateOutputMethod
 *****************************************************************************/
static void Destroy( vlc_object_t *p_this )
{
    filter_t* p_filter = (filter_t*)p_this;
    ReleaseImages( p_filter );

    // Release the internal OpenCV filter.
    module_unneed( p_filter->p_sys->p_opencv, p_filter->p_sys->p_opencv->p_module );
    vlc_object_release( p_filter->p_sys->p_opencv );
    p_filter->p_sys->p_opencv = NULL;

    free( p_filter->p_sys );
}

/*****************************************************************************
 * ReleaseImages: Release OpenCV images in filter_sys_t.
 *****************************************************************************/
static void ReleaseImages( filter_t* p_filter )
{
    filter_sys_t* p_sys = p_filter->p_sys;

    for( int i = 0; i < VOUT_MAX_PLANES; i++ )
    {
        if (p_sys->p_cv_image[i] != NULL)
        {
            cvReleaseImageHeader(&(p_sys->p_cv_image[i]));
            p_sys->p_cv_image[i] = NULL;
        }
    }
    p_sys->i_cv_image_size = 0;

    /* Release temp picture_t if it exists */
    if (p_sys->p_to_be_freed)
    {
        picture_Release( p_sys->p_to_be_freed );
        p_sys->p_to_be_freed = NULL;
    }

#ifndef NDEBUG
    msg_Dbg( p_filter, "images released" );
#endif
}

/*****************************************************************************
 * VlcPictureToIplImage: Convert picture_t to IplImage
 *****************************************************************************
 * Converts given picture_t into IplImage(s) according to module config.
 * IplImage(s) are stored in vout_sys_t.
 *****************************************************************************/
static void VlcPictureToIplImage( filter_t* p_filter, picture_t* p_in )
{
    int planes = p_in->i_planes;    //num input video planes
    // input video size
    CvSize sz = cvSize(abs(p_in->format.i_width), abs(p_in->format.i_height));
    video_format_t fmt_out;
    filter_sys_t* p_sys = p_filter->p_sys;

    memset( &fmt_out, 0, sizeof(video_format_t) );

    //do scale / color conversion according to p_sys config
    if ((p_sys->f_scale != 1) || (p_sys->i_internal_chroma != CINPUT))
    {
        fmt_out = p_in->format;

        //calc the scaled video size
        fmt_out.i_width = p_in->format.i_width * p_sys->f_scale;
        fmt_out.i_height = p_in->format.i_height * p_sys->f_scale;

        if (p_sys->i_internal_chroma == RGB)
        {
            //rgb2 gives 3 separate planes, this gives 1 interleaved plane
            //rv24 gives is about 20% faster but gives r&b the wrong way round
            //and I can't think of an easy way to fix this
            fmt_out.i_chroma = VLC_CODEC_RGB24;
        }
        else if (p_sys->i_internal_chroma == GREY)
        {
            //take the I (gray) plane (video seems to commonly be in this fmt so usually the
            //conversion does nothing)
            fmt_out.i_chroma = VLC_CODEC_I420;
        }

        //convert from the input image
        p_sys->p_proc_image = image_Convert( p_sys->p_image, p_in,
                                     &(p_in->format), &fmt_out );

        if (!p_sys->p_proc_image)
        {
            msg_Err(p_filter, "can't convert (unsupported formats?), aborting...");
            return;
        }

        p_sys->p_to_be_freed = p_sys->p_proc_image;    //remember this so we can free it later

    }
    else    //((p_sys->f_scale != 1) || (p_sys->i_internal_chroma != CINPUT))
    {
        // In theory, you could use the input image without conversion,
        // but it seems to cause weird picture effects (like repeated
        // image filtering) and picture leaking.
        p_sys->p_proc_image = filter_NewPicture( p_filter ); //p_in
        picture_Copy( p_sys->p_proc_image, p_in );
        p_sys->p_to_be_freed = p_sys->p_proc_image;
    }

    //Convert to the IplImage array that is to be processed.
    //If there are multiple planes in p_sys->p_proc_image, then 1 IplImage
    //is created for each plane.
    planes = p_sys->p_proc_image->i_planes;
    p_sys->i_cv_image_size = planes;
    for( int i = 0; i < planes; i++ )
    {
        sz = cvSize(abs(p_sys->p_proc_image->p[i].i_visible_pitch /
            p_sys->p_proc_image->p[i].i_pixel_pitch),
            abs(p_sys->p_proc_image->p[i].i_visible_lines));

        p_sys->p_cv_image[i] = cvCreateImageHeader(sz, IPL_DEPTH_8U,
            p_sys->p_proc_image->p[i].i_pixel_pitch);

        cvSetData( p_sys->p_cv_image[i],
            (char*)(p_sys->p_proc_image->p[i].p_pixels), p_sys->p_proc_image->p[i].i_pitch );
    }

    //Hack the above opencv image array into a picture_t so that it can be sent to
    //another video filter
    p_sys->hacked_pic.i_planes = planes;
    p_sys->hacked_pic.format.i_chroma = fmt_out.i_chroma;

#ifndef NDEBUG
    msg_Dbg( p_filter, "VlcPictureToIplImageRgb() completed" );
#endif
}

/*****************************************************************************
 * Filter: displays previously rendered output
 *****************************************************************************
 * This function send the currently rendered image to the internal opencv
 * filter for processing.
 *****************************************************************************/
static picture_t* Filter( filter_t* p_filter, picture_t* p_pic )
{
    picture_t* p_outpic = filter_NewPicture( p_filter );
    if( p_outpic == NULL ) {
        msg_Err( p_filter, "couldn't get a p_outpic!" );
        picture_Release( p_pic );
        return NULL;
    }

    video_format_t fmt_out;

    // Make a copy if we want to show the original input
    if (p_filter->p_sys->i_wrapper_output == VINPUT)
        picture_Copy( p_outpic, p_pic );

    VlcPictureToIplImage( p_filter, p_pic );
    // Pass the image (as a pointer to the first IplImage*) to the
    // internal OpenCV filter for processing.
    p_filter->p_sys->p_opencv->pf_video_filter( p_filter->p_sys->p_opencv, (picture_t*)&(p_filter->p_sys->p_cv_image[0]) );

    if(p_filter->p_sys->i_wrapper_output == PROCESSED) {
        // Processed video
        if( (p_filter->p_sys->p_proc_image) &&
            (p_filter->p_sys->p_proc_image->i_planes > 0) &&
            (p_filter->p_sys->i_internal_chroma != CINPUT) ) {
            //p_filter->p_sys->p_proc_image->format.i_chroma = VLC_CODEC_RGB24;

            memset( &fmt_out, 0, sizeof(video_format_t) );
            fmt_out = p_pic->format;
            //picture_Release( p_outpic );

            /*
             * We have to copy out the image from image_Convert(), otherwise
             * you leak pictures for some reason:
             * main video output error: pictures leaked, trying to workaround
             */
            picture_t* p_outpic_tmp = image_Convert(
                        p_filter->p_sys->p_image,
                        p_filter->p_sys->p_proc_image,
                        &(p_filter->p_sys->p_proc_image->format),
                        &fmt_out );

            picture_CopyPixels( p_outpic, p_outpic_tmp );
            CopyInfoAndRelease( p_outpic, p_outpic_tmp );
        } else if( p_filter->p_sys->i_internal_chroma == CINPUT ) {
            picture_CopyPixels( p_outpic, p_filter->p_sys->p_proc_image );
            picture_CopyProperties( p_outpic, p_filter->p_sys->p_proc_image );
        }
    }

    ReleaseImages( p_filter );
    picture_Release( p_pic );

#ifndef NDEBUG
    msg_Dbg( p_filter, "Filter() done" );
#endif

    if( p_filter->p_sys->i_wrapper_output != NONE ) {
        return p_outpic;
    } else { // NONE
        picture_Release( p_outpic );
        return NULL;
    }
}


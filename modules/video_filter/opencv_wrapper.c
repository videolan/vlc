/*****************************************************************************
 * opencv_wrapper.c : OpenCV wrapper video filter
 *****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 *
 * Authors: Dugal Harris <dugalh@protoclea.co.za>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
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

#include <math.h>
#include <time.h>

#include <vlc_filter.h>
#include "filter_common.h"
#include <vlc_image.h>
#include <vlc_input.h>
#include <vlc_playlist.h>

#include <cxcore.h>
#include <cv.h>


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create    ( vlc_object_t * );
static void Destroy   ( vlc_object_t * );

static int  Init      ( vout_thread_t * );
static void End       ( vout_thread_t * );
static void Render    ( vout_thread_t *, picture_t * );

static void ReleaseImages( vout_thread_t *p_vout );
static void VlcPictureToIplImage( vout_thread_t *p_vout, picture_t *p_in );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

static const char *const chroma_list[] = { "input", "I420", "RGB32"};
static const char *const chroma_list_text[] = { N_("Use input chroma unaltered"),
  N_("I420 - first plane is greyscale"), N_("RGB32")};

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
    add_float_with_range( "opencv-scale", 1.0, 0.1, 2.0, NULL,
                          N_("Scale factor (0.1-2.0)"),
                          N_("Ammount by which to scale the picture before sending it to the internal OpenCV filter"),
                          false )
    add_string( "opencv-chroma", "input", NULL,
                          N_("OpenCV filter chroma"),
                          N_("Chroma to convert picture to before sending it to the internal OpenCV filter"), false);
        change_string_list( chroma_list, chroma_list_text, 0);
    add_string( "opencv-output", "input", NULL,
                          N_("Wrapper filter output"),
                          N_("Determines what (if any) video is displayed by the wrapper filter"), false);
        change_string_list( output_list, output_list_text, 0);
    add_string( "opencv-verbosity", "error", NULL,
                          N_("Wrapper filter verbosity"),
                          N_("Determines wrapper filter verbosity level"), false);
        change_string_list( verbosity_list, verbosity_list_text, 0);
    add_string( "opencv-filter-name", "none", NULL,
                          N_("OpenCV internal filter name"),
                          N_("Name of internal OpenCV plugin filter to use"), false);
vlc_module_end ()


/*****************************************************************************
 * wrapper_output_t: what video is output
 *****************************************************************************/
enum wrapper_output_t
{
   NONE,    //not working yet
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
 * verbosity_t:
 *****************************************************************************/
enum verbosity_t
{
   VERB_ERROR,
   VERB_WARN,
   VERB_DEBUG
};

/*****************************************************************************
 * vout_sys_t: opencv_wrapper video output method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the opencv_wrapper specific properties of an output thread.
 *****************************************************************************/
struct vout_sys_t
{
    vout_thread_t *p_vout;

    image_handler_t *p_image;

    int i_cv_image_size;

    picture_t *p_proc_image;
    picture_t *p_to_be_freed;

    float f_scale;

    int i_wrapper_output;
    int i_internal_chroma;
    int i_verbosity;

    IplImage *p_cv_image[VOUT_MAX_PLANES];

    filter_t *p_opencv;
    char* psz_inner_name;

    picture_t hacked_pic;
};

/*****************************************************************************
 * Control: control facility for the vout (forwards to child vout)
 *****************************************************************************/
static int Control( vout_thread_t *p_vout, int i_query, va_list args )
{
    return vout_vaControl( p_vout->p_sys->p_vout, i_query, args );
}

/*****************************************************************************
 * Create: allocates opencv_wrapper video thread output method
 *****************************************************************************
 * This function allocates and initializes a opencv_wrapper vout method.
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    char *psz_chroma, *psz_output, *psz_verbosity;
    int i = 0;

    /* Allocate structure */
    p_vout->p_sys = malloc( sizeof( vout_sys_t ) );
    if( p_vout->p_sys == NULL )
        return VLC_ENOMEM;

    /* Init structure */
    p_vout->p_sys->p_image = image_HandlerCreate( p_vout );
    for (i = 0; i < VOUT_MAX_PLANES; i++)
        p_vout->p_sys->p_cv_image[i] = NULL;
    p_vout->p_sys->p_proc_image = NULL;
    p_vout->p_sys->p_to_be_freed = NULL;
    p_vout->p_sys->i_cv_image_size = 0;

    p_vout->pf_init = Init;
    p_vout->pf_end = End;
    p_vout->pf_manage = NULL;
    p_vout->pf_render = Render;
    p_vout->pf_display = NULL;
    p_vout->pf_control = Control;

    /* Retrieve and apply config */
    psz_chroma = var_InheritString( p_vout, "opencv-chroma" );
    if( psz_chroma == NULL )
    {
        msg_Err( p_vout, "configuration variable %s empty, using 'grey'",
                         "opencv-chroma" );
        p_vout->p_sys->i_internal_chroma = GREY;
    }
    else
    {
        if( !strcmp( psz_chroma, "input" ) )
            p_vout->p_sys->i_internal_chroma = CINPUT;
        else if( !strcmp( psz_chroma, "I420" ) )
            p_vout->p_sys->i_internal_chroma = GREY;
        else if( !strcmp( psz_chroma, "RGB32" ) )
            p_vout->p_sys->i_internal_chroma = RGB;
        else
        {
            msg_Err( p_vout, "no valid opencv-chroma provided, using 'grey'" );
            p_vout->p_sys->i_internal_chroma = GREY;
        }
    }
    free( psz_chroma);

    psz_output = var_InheritString( p_vout, "opencv-output" );
    if( psz_output == NULL )
    {
        msg_Err( p_vout, "configuration variable %s empty, using 'input'",
                         "opencv-output" );
        p_vout->p_sys->i_wrapper_output = VINPUT;
    }
    else
    {
        if( !strcmp( psz_output, "none" ) )
            p_vout->p_sys->i_wrapper_output = NONE;
        else if( !strcmp( psz_output, "input" ) )
            p_vout->p_sys->i_wrapper_output = VINPUT;
        else if( !strcmp( psz_output, "processed" ) )
            p_vout->p_sys->i_wrapper_output = PROCESSED;
        else
        {
            msg_Err( p_vout, "no valid opencv-output provided, using 'input'" );
            p_vout->p_sys->i_wrapper_output = VINPUT;
        }
    }
    free( psz_output);

    psz_verbosity = var_InheritString( p_vout, "opencv-verbosity" );
    if( psz_verbosity == NULL )
    {
        msg_Err( p_vout, "configuration variable %s empty, using 'input'",
                         "opencv-verbosity" );
        p_vout->p_sys->i_verbosity = VERB_ERROR;
    }
    else
    {
        if( !strcmp( psz_verbosity, "error" ) )
            p_vout->p_sys->i_verbosity = VERB_ERROR;
        else if( !strcmp( psz_verbosity, "warning" ) )
            p_vout->p_sys->i_verbosity = VERB_WARN;
        else if( !strcmp( psz_verbosity, "debug" ) )
            p_vout->p_sys->i_verbosity = VERB_DEBUG;
        else
        {
            msg_Err( p_vout, "no valid opencv-verbosity provided, using 'error'" );
            p_vout->p_sys->i_verbosity = VERB_ERROR;
        }
    }
    free( psz_verbosity);

    p_vout->p_sys->psz_inner_name =
        var_InheritString( p_vout, "opencv-filter-name" );
    p_vout->p_sys->f_scale =
        var_InheritFloat( p_vout, "opencv-scale" );

    if (p_vout->p_sys->i_verbosity > VERB_WARN)
        msg_Info(p_vout, "Configuration: opencv-scale: %f, opencv-chroma: %d, "
            "opencv-output: %d, opencv-verbosity %d, opencv-filter %s",
            p_vout->p_sys->f_scale,
            p_vout->p_sys->i_internal_chroma,
            p_vout->p_sys->i_wrapper_output,
            p_vout->p_sys->i_verbosity,
            p_vout->p_sys->psz_inner_name);

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Init: initialize opencv_wrapper video thread output method
 *****************************************************************************/
static int Init( vout_thread_t *p_vout )
{
    video_format_t fmt;
    vout_sys_t *p_sys = p_vout->p_sys;
    I_OUTPUTPICTURES = 0;

    /* Initialize the output video format */
    memset( &fmt, 0, sizeof(video_format_t) );
    p_vout->output.i_chroma = p_vout->render.i_chroma;
    p_vout->output.i_width  = p_vout->render.i_width;
    p_vout->output.i_height = p_vout->render.i_height;
    p_vout->output.i_aspect = p_vout->render.i_aspect;
    p_vout->fmt_out = p_vout->fmt_in;           //set to input video format

    fmt = p_vout->fmt_out;
    if (p_sys->i_wrapper_output == PROCESSED)   //set to processed video format
    {
        fmt.i_width = fmt.i_width * p_sys->f_scale;
        fmt.i_height = fmt.i_height * p_sys->f_scale;
        fmt.i_visible_width = fmt.i_visible_width * p_sys->f_scale;
        fmt.i_visible_height = fmt.i_visible_height * p_sys->f_scale;
        fmt.i_x_offset = fmt.i_x_offset * p_sys->f_scale;
        fmt.i_y_offset = fmt.i_y_offset * p_sys->f_scale;

        if (p_sys->i_internal_chroma == GREY)
            fmt.i_chroma = VLC_CODEC_I420;
        else if (p_sys->i_internal_chroma == RGB)
            fmt.i_chroma = VLC_CODEC_RGB32;
    }

    /* Load the internal opencv filter */
    /* We don't need to set up video formats for this filter as it not actually using a picture_t */
    p_sys->p_opencv = vlc_object_create( p_vout, sizeof(filter_t) );
    vlc_object_attach( p_sys->p_opencv, p_vout );

    if (p_vout->p_sys->psz_inner_name)
        p_sys->p_opencv->p_module =
            module_need( p_sys->p_opencv, p_sys->psz_inner_name, NULL, false );

    if( !p_sys->p_opencv->p_module )
    {
        msg_Err( p_vout, "can't open internal opencv filter: %s", p_vout->p_sys->psz_inner_name );
        p_vout->p_sys->psz_inner_name = NULL;
        vlc_object_release( p_sys->p_opencv );
        p_sys->p_opencv = NULL;
    }

    /* Try to open the real video output */
    if (p_sys->i_verbosity > VERB_WARN)
        msg_Dbg( p_vout, "spawning the real video output" );

    p_vout->p_sys->p_vout = vout_Create( p_vout, &fmt );

    /* Everything failed */
    if( p_vout->p_sys->p_vout == NULL )
    {
        msg_Err( p_vout, "can't open vout, aborting" );
        return VLC_EGENERIC;
    }

    vout_filter_AllocateDirectBuffers( p_vout, VOUT_MAX_PICTURES );

    vout_filter_AddChild( p_vout, p_vout->p_sys->p_vout, NULL );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * End: terminate opencv_wrapper video thread output method
 *****************************************************************************/
static void End( vout_thread_t *p_vout )
{
    vout_sys_t *p_sys = p_vout->p_sys;

    vout_filter_DelChild( p_vout, p_sys->p_vout, NULL );
    vout_CloseAndRelease( p_sys->p_vout );

    vout_filter_ReleaseDirectBuffers( p_vout );

    if( p_sys->p_opencv )
    {
        //release the internal opencv filter
        if( p_sys->p_opencv->p_module )
            module_unneed( p_sys->p_opencv, p_sys->p_opencv->p_module );
        vlc_object_release( p_sys->p_opencv );
        p_sys->p_opencv = NULL;
    }
}

/*****************************************************************************
 * Destroy: destroy opencv_wrapper video thread output method
 *****************************************************************************
 * Terminate an output method created by opencv_wrapperCreateOutputMethod
 *****************************************************************************/
static void Destroy( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;

    ReleaseImages(p_vout);

    if( p_vout->p_sys->p_image )
        image_HandlerDelete( p_vout->p_sys->p_image );

    free( p_vout->p_sys );
}

/*****************************************************************************
 * ReleaseImages: Release OpenCV images in vout_sys_t.
 *****************************************************************************/
static void ReleaseImages(vout_thread_t *p_vout)
{
    int i = 0;
    if (p_vout->p_sys->p_cv_image)
    {
        for (i = 0; i < VOUT_MAX_PLANES; i++)
        {
            if (p_vout->p_sys->p_cv_image[i])
                cvReleaseImageHeader(&(p_vout->p_sys->p_cv_image[i]));
            p_vout->p_sys->p_cv_image[i] = NULL;
        }
    }
    p_vout->p_sys->i_cv_image_size = 0;

    /* Release temp picture_t if it exists */
    if (p_vout->p_sys->p_to_be_freed)
    {
        picture_Release( p_vout->p_sys->p_to_be_freed );
        p_vout->p_sys->p_to_be_freed = NULL;
    }
    if (p_vout->p_sys->i_verbosity > VERB_WARN)
        msg_Dbg( p_vout, "images released" );
}

/*****************************************************************************
 * VlcPictureToIplImage: Convert picture_t to IplImage
 *****************************************************************************
 * Converts given picture_t into IplImage(s) according to module config.
 * IplImage(s) are stored in vout_sys_t.
 *****************************************************************************/
static void VlcPictureToIplImage( vout_thread_t *p_vout, picture_t *p_in )
{
    int planes = p_in->i_planes;    //num input video planes
    // input video size
    CvSize sz = cvSize(abs(p_in->format.i_width), abs(p_in->format.i_height));
    video_format_t fmt_out;
    clock_t start, finish;  //performance measures
    double  duration;
    int i = 0;
    vout_sys_t* p_sys = p_vout->p_sys;

    memset( &fmt_out, 0, sizeof(video_format_t) );

    start = clock();

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
            //and I cant think of an easy way to fix this
            fmt_out.i_chroma = VLC_CODEC_RGB32;
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
            msg_Err(p_vout, "can't convert (unsupported formats?), aborting...");
            return;
        }

        p_sys->p_to_be_freed = p_sys->p_proc_image;    //remember this so we can free it later

    }
    else    //((p_sys->f_scale != 1) || (p_sys->i_internal_chroma != CINPUT))
    {
        //use the input image without conversion
        p_sys->p_proc_image = p_in;
    }

    //Convert to the IplImage array that is to be processed.
    //If there are multiple planes in p_sys->p_proc_image, then 1 IplImage
    //is created for each plane.
    planes = p_sys->p_proc_image->i_planes;
    p_sys->i_cv_image_size = planes;
    for ( i = 0; i < planes; i++ )
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
    p_sys->hacked_pic.p_data_orig = p_sys->p_cv_image;
    p_sys->hacked_pic.i_planes = planes;
    p_sys->hacked_pic.format.i_chroma = fmt_out.i_chroma;

    //calculate duration of conversion
    finish = clock();
    duration = (double)(finish - start) / CLOCKS_PER_SEC;
    if (p_sys->i_verbosity > VERB_WARN)
        msg_Dbg( p_vout, "VlcPictureToIplImageRgb took %2.4f seconds", duration );
}

/*****************************************************************************
 * Render: displays previously rendered output
 *****************************************************************************
 * This function send the currently rendered image to the internal opencv
 * filter for processing.
 *****************************************************************************/
static void Render( vout_thread_t *p_vout, picture_t *p_pic )
{
    picture_t *p_outpic = NULL;
    clock_t start, finish;
    double  duration;

    while( ( p_outpic = vout_CreatePicture( p_vout->p_sys->p_vout, 0, 0, 0 ) )
              == NULL )
    {
        if( !vlc_object_alive (p_vout) || p_vout->b_error )
        {   return; }
        msleep( VOUT_OUTMEM_SLEEP );
    }

    vout_LinkPicture( p_vout->p_sys->p_vout, p_outpic );

    start = clock();

    if (p_vout->p_sys->i_wrapper_output == VINPUT)  //output = input video
    {
        //This copy is a bit unfortunate but image_Convert can't write into an existing image so it is better to copy the
        //(say) 16bit YUV image here than a 32bit RGB image somehwere else.
        //It is also not that expensive in time.
        picture_Copy( p_outpic, p_pic );
        VlcPictureToIplImage( p_vout, p_pic);
        //pass the image to the internal opencv filter for processing
        if ((p_vout->p_sys->p_opencv) && (p_vout->p_sys->p_opencv->p_module))
            p_vout->p_sys->p_opencv->pf_video_filter( p_vout->p_sys->p_opencv, &(p_vout->p_sys->hacked_pic));
    }
    else    //output = processed video (NONE option not working yet)
    {
        VlcPictureToIplImage( p_vout, p_pic);
        //pass the image to the internal opencv filter for processing
        if ((p_vout->p_sys->p_opencv) && (p_vout->p_sys->p_opencv->p_module))
            p_vout->p_sys->p_opencv->pf_video_filter( p_vout->p_sys->p_opencv, &(p_vout->p_sys->hacked_pic));
        //copy the processed image into the output image
        if ((p_vout->p_sys->p_proc_image) && (p_vout->p_sys->p_proc_image->p_data))
            picture_Copy( p_outpic, p_vout->p_sys->p_proc_image );
    }

    //calculate duration
    finish = clock();
    duration = (double)(finish - start) / CLOCKS_PER_SEC;
    if (p_vout->p_sys->i_verbosity > VERB_WARN)
        msg_Dbg( p_vout, "Render took %2.4f seconds", duration );

    ReleaseImages(p_vout);
    p_outpic->date  = p_pic->date;
    
    vout_UnlinkPicture( p_vout->p_sys->p_vout, p_outpic );
    vout_DisplayPicture( p_vout->p_sys->p_vout, p_outpic );
}


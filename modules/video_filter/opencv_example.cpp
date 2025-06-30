/*****************************************************************************
 * opencv_example.cpp : Example OpenCV internal video filter
 * (performs face identification).  Mostly taken from the facedetect.c
 * OpenCV sample.
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
#include <vlc_filter.h>
#include <vlc_picture.h>
#include <vlc_vout.h>
#include <vlc_image.h>
#include "filter_event_info.h"

#include <opencv2/core/core_c.h>
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc_c.h>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/objdetect/objdetect.hpp>

#define CV_HAAR_DO_CANNY_PRUNING 1

/*****************************************************************************
 * filter_sys_t : filter descriptor
 *****************************************************************************/

namespace {

class filter_sys_t
{
public:
    cv::CascadeClassifier cascade;
    std::vector<video_filter_region_info_t> event_info_storage;
    video_filter_event_info_t event_info;
    int i_id;
};

} // namespace

/****************************************************************************
 * Local prototypes
 ****************************************************************************/
static int  OpenFilter ( vlc_object_t * );
static void CloseFilter( filter_t * );

static picture_t *Filter( filter_t *, picture_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_description( N_("OpenCV face detection example filter") )
    set_shortname( N_( "OpenCV example" ))
    set_capability( "opencv internal filter", 1 )
    add_shortcut( "opencv_example" )

    set_subcategory( SUBCAT_VIDEO_VFILTER )
    set_callback( OpenFilter )

    add_string( "opencv-haarcascade-file", "c:\\haarcascade_frontalface_alt.xml",
                          N_("Haar cascade filename"),
                          N_("Name of XML file containing Haar cascade description"))
vlc_module_end ()

/*****************************************************************************
 * OpenFilter: probe the filter and return score
 *****************************************************************************/
static int OpenFilter( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t*)p_this;
    filter_sys_t *p_sys;

    /* Allocate the memory needed to store the decoder's structure */
    p_filter->p_sys = p_sys = new filter_sys_t;
    if(p_sys == nullptr)
    {
        return VLC_ENOMEM;
    }

    //init the video_filter_event_info_t struct
    p_sys->event_info.i_region_size = 0;
    p_sys->event_info.p_region = nullptr;
    p_sys->i_id = 0;

    static const struct FilterOperationInitializer {
        struct vlc_filter_operations ops {};
        FilterOperationInitializer()
        {
            ops.filter_video = Filter;
            ops.close        = CloseFilter;
        };
    } filter_ops;
    p_filter->ops = &filter_ops.ops;

    //create the VIDEO_FILTER_EVENT_VARIABLE
    vlc_value_t val;
    if (var_Create( vlc_object_instance(p_filter), VIDEO_FILTER_EVENT_VARIABLE, VLC_VAR_ADDRESS | VLC_VAR_DOINHERIT ) != VLC_SUCCESS)
        msg_Err( p_filter, "Could not create %s", VIDEO_FILTER_EVENT_VARIABLE);

    val.p_address = &(p_sys->event_info);
    if (var_Set( vlc_object_instance(p_filter), VIDEO_FILTER_EVENT_VARIABLE, val )!=VLC_SUCCESS)
        msg_Err( p_filter, "Could not set %s", VIDEO_FILTER_EVENT_VARIABLE);

    //OpenCV init specific to this example
    char* filename = var_InheritString( p_filter, "opencv-haarcascade-file" );
    if (!p_sys->cascade.load(filename)) {
        msg_Err( p_filter, "Could not load %s", filename);
    }
    free( filename );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * CloseFilter: clean up the filter
 *****************************************************************************/
static void CloseFilter( filter_t *p_filter )
{
    filter_sys_t *p_sys = static_cast<filter_sys_t *>(p_filter->p_sys);

    delete p_sys;

    var_Destroy( vlc_object_instance(p_filter), VIDEO_FILTER_EVENT_VARIABLE);
}

/****************************************************************************
 * Filter: Check for faces and raises an event when one is found.
 ****************************************************************************/
static picture_t *Filter( filter_t *p_filter, picture_t *p_pic )
{
    IplImage** p_img = nullptr;
	
    filter_sys_t *p_sys = static_cast<filter_sys_t *>(p_filter->p_sys);

    if ((!p_pic) )
    {
        msg_Err( p_filter, "no image array" );
        return nullptr;
    }
    //(hack) cast the picture_t to array of IplImage*
    p_img = (IplImage**) p_pic;

    //check the image array for validity
    if ((!p_img[0]))    //1st plane is 'I' i.e. greyscale
    {
        msg_Err( p_filter, "no image" );
        return nullptr;
    }

    // Wrapper for IplImage* -> cv::Mat (reference, not copy)
    cv::Mat gray = cv::cvarrToMat(p_img[0]);

    //perform face detection
    if (p_sys->cascade.empty())
        msg_Err( p_filter, "No cascade - is opencv-haarcascade-file valid?" );

    //we should make some of these params config variables
    std::vector<cv::Rect> faces;
    p_sys->cascade.detectMultiScale(gray, faces,
                                    1.15, 5,
                                    CV_HAAR_DO_CANNY_PRUNING,
                                    cv::Size(20, 20));
    //create the video_filter_region_info_t struct
    if (faces.empty())
        return p_pic;

    //msg_Dbg( p_filter, "Found %d face(s)", faces.size());
    p_sys->event_info_storage.resize(faces.size());
    p_sys->event_info.i_region_size = faces.size();
    p_sys->event_info.p_region = p_sys->event_info_storage.data();
    if( !p_sys->event_info.p_region )
        return nullptr;

    //populate the video_filter_region_info_t struct
    for( size_t i = 0; i < faces.size(); i++ )
     {		
        const cv::Rect& r = faces[i];
        cv::rectangle(gray, r, CV_RGB(0,0,0), 3);

        p_sys->event_info.p_region[i].i_x = r.x;
        p_sys->event_info.p_region[i].i_y = r.y;
        p_sys->event_info.p_region[i].i_width = r.width;
        p_sys->event_info.p_region[i].i_height = r.height;
    }
    
    var_TriggerCallback( vlc_object_instance(p_filter), VIDEO_FILTER_EVENT_VARIABLE );

    return p_pic;
}

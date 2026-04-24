/*****************************************************************************
* sam3.cpp : Segment Anything Model 3 (SAM3) interactive filter
*****************************************************************************
* Copyright (C) 2025 VideoLAN
*
* Authors: Brandon Li <brandonli2006ma@gmail.com>
*
* This program is free software; you can redistribute it and/or modify it
* under the terms of the GNU Lesser General Public License as published by
* the Free Software Foundation; either version 2.1 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
* Lesser General Public License for more details.
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
#include <vlc_mouse.h>
#include <vlc_fs.h>

#include <opencv2/opencv.hpp>

#include <thread>
#include <mutex>
#include <atomic>
#include <vector>
#include <map>
#include <ctime>
#include <condition_variable>
#include <future>
#include <sstream>

#include "sam3.h"

/*****************************************************************************
* Local prototypes
*****************************************************************************/

struct segmented_object_t
{
    int i_id{};
    cv::Mat mat_mask;
    cv::Scalar scalar_color;
    std::vector<cv::Point> vec_positive_clicks;
    std::vector<cv::Point> vec_negative_clicks;
    std::vector<std::vector<cv::Point>> vec_contours;
    std::vector<std::vector<cv::Point>> vec_scaled_contours;
    cv::Size sz_scaled_for;
};

struct click_event_t
{
    cv::Point pt_click;
    bool b_positive{};
    int i_target_id{};
    bool b_pending{};
};

struct filter_sys_t
{
    std::shared_ptr<sam3_model> ptr_sam_model;
    sam3_state_ptr ptr_sam_state;
    cv::Size sz_input_size;
    cv::Mat mat_current_frame;
    cv::Mat mat_original_frame;
    std::mutex mtx_frame_mutex;
    std::map<int, segmented_object_t> map_objects;
    std::mutex mtx_objects_mutex;
    int i_next_object_id{};
    click_event_t evt_current_click;
    std::thread thrd_processing;
    std::atomic<bool> atom_active{};
    std::atomic<bool> atom_busy{};
    std::atomic<bool> atom_paused{};
    std::atomic<bool> atom_was_paused{};
    int i_outline_thickness{};
    cv::Scalar scalar_outline_color;
    vlc_tick_t i_last_pic_date{};
    char* psz_export_path{};
    std::condition_variable cv_work;
    std::mutex mtx_work;
    std::atomic<bool> atom_preprocessed{};
    char* psz_text_prompt{};
    std::atomic<bool> atom_run_text_segment{};
    std::atomic<bool> atom_frame_captured{};
};

static picture_t* Filter( filter_t* p_filter, picture_t* p_pic );
static int Mouse( filter_t* p_filter, vlc_mouse_t* p_mouse, const vlc_mouse_t* p_oldmouse );
static int Open( filter_t* p_filter );
static void Close( filter_t* p_filter );
static void Process( filter_t* p_filter );
static void Export( filter_t* p_filter, const segmented_object_t& obj, const cv::Mat& mat_frame );

static constexpr vlc_filter_operations st_filter_ops = {
    .filter_video = Filter,
    .drain_audio = nullptr,
    .flush = nullptr,
    .change_viewpoint = nullptr,
    .video_mouse = Mouse,
    .close = Close
};

/*****************************************************************************
* Module descriptor
*****************************************************************************/

vlc_module_begin( )
    set_description( N_ ( "Segment Anything Model 3 (SAM3) interactive filter" ) )
    add_shortcut( "sam3" )
    set_shortname( N_ ( "SAM Filter " ) )
    set_help( N_ ( "Left-click to add positive point (mask inclusion), right-click to remove excess (mask exclusion)." ) )
    set_subcategory( SUBCAT_VIDEO_VFILTER )
    set_callback_video_filter( Open )
    add_string( "sam-model", nullptr, N_ ( "SAM model path" ), N_ ( "Path to the SAM3 GGML model" ) )
    add_bool( "sam-use-cpu", true, N_ ( "CPU inference" ),
               N_ ( "Whether CPU inference should be used or not" ) )
    add_integer_with_range( "sam-outline-thickness", 3, 1, 10, N_ ( "Outline thickness" ),
                           N_ ( "Thickness of the segmentation outline" ) )
    add_string( "sam-outline-color", "255,0,0", N_ ( "Outline color" ),
               N_ ( "Color of the segmentation outline (R,G,B format)" ) )
    add_directory( "sam-export-path", nullptr, N_ ( "Export path for masks" ),
                  N_ ( "Directory where masks will be automatically exported" ) )
    add_string( "sam-text-prompt", nullptr, N_ ( "SAM3 text prompt" ),
               N_ ( "Natural-language concept to segment" ) )
vlc_module_end( )

/*****************************************************************************
* Export: Export a single mask to file
*****************************************************************************/
static void Export( filter_t* p_filter, const segmented_object_t& obj, const cv::Mat& mat_frame )
{
    const auto* p_sys = static_cast<filter_sys_t*>( p_filter->p_sys );
    if ( !p_sys->psz_export_path || obj.mat_mask.empty( ) || mat_frame.empty( ) )
        return;

    // get timestamp
    const time_t t_now = time( nullptr );
    tm tm_now_buf{};
    const tm* tm_now = localtime_r( &t_now, &tm_now_buf );
    if ( !tm_now )
    {
        msg_Err( p_filter, "Failed to get local time" );
        return;
    }
    char psz_timestamp[ 32 ];
    strftime( psz_timestamp, sizeof ( psz_timestamp ), "%Y%m%d_%H%M%S", tm_now );

    // resize mask to match frame size
    cv::Mat mat_mask_scaled;
    cv::resize( obj.mat_mask, mat_mask_scaled, mat_frame.size( ), 0, 0, cv::INTER_LINEAR );
    cv::threshold( mat_mask_scaled, mat_mask_scaled, 128, 255, cv::THRESH_BINARY );

    // create file name
    std::string str_filename = std::string( p_sys->psz_export_path ) + "/mask_" +
        psz_timestamp + "_id" + std::to_string( obj.i_id ) + ".png";

    // export mask onto Mat - optimized version
    std::vector<cv::Mat> vec_channels;
    cv::split( mat_frame, vec_channels );
    vec_channels.push_back( mat_mask_scaled );
    cv::Mat mat_export;
    cv::merge( vec_channels, mat_export );

    // write image
    if ( !cv::imwrite( str_filename, mat_export ) )
    {
        msg_Err( p_filter, "Failed to export mask to %s", str_filename.c_str( ) );
        return;
    }
    msg_Info( p_filter, "Exported mask %d to %s", obj.i_id, str_filename.c_str( ) );

    // create file name
    std::string vis_filename = std::string( p_sys->psz_export_path ) + "/visualization_" +
        psz_timestamp + "_id" + std::to_string( obj.i_id ) + ".jpg";

    // clone image, add mask overlay
    cv::Mat mat_visualization = mat_frame.clone( );
    cv::Mat mat_overlay = cv::Mat::zeros( mat_visualization.size( ), CV_8UC3 );
    mat_overlay.setTo( obj.scalar_color, mat_mask_scaled );
    cv::addWeighted( mat_visualization, 0.7, mat_overlay, 0.3, 0, mat_visualization );

    // draw contours on the visualization
    std::vector<std::vector<cv::Point>> vec_scaled_contours;
    double d_scale_x = static_cast<double>( mat_frame.cols ) / p_sys->sz_input_size.width;
    double d_scale_y = static_cast<double>( mat_frame.rows ) / p_sys->sz_input_size.height;
    for ( const auto& vec_contour : obj.vec_contours )
    {
        std::vector<cv::Point> vec_scaled;
        for ( const auto& pt : vec_contour )
        {
            vec_scaled.emplace_back(
                static_cast<int>( pt.x * d_scale_x ),
                static_cast<int>( pt.y * d_scale_y )
            );
        }
        vec_scaled_contours.push_back( vec_scaled );
    }
    cv::drawContours( mat_visualization, vec_scaled_contours, -1, obj.scalar_color, 2, cv::LINE_AA );

    // write image
    if ( !cv::imwrite( vis_filename, mat_visualization ) )
    {
        msg_Err( p_filter, "Failed to export visualization to %s", vis_filename.c_str( ) );
        return;
    }
    msg_Info( p_filter, "Exported visualization to %s", vis_filename.c_str( ) );
}

/*****************************************************************************
* Process: Processing thread for segmentation
*****************************************************************************/
static void Process( filter_t* p_filter )
{
    auto* p_sys = static_cast<filter_sys_t*>( p_filter->p_sys );

    while ( p_sys->atom_active ) // processing active
    {
        std::unique_lock lck_lock( p_sys->mtx_work );
        p_sys->cv_work.wait( lck_lock, [ p_sys ]
        {
            return !p_sys->atom_active || p_sys->evt_current_click.b_pending ||
                   p_sys->atom_run_text_segment;
        } );

        if ( !p_sys->atom_active )
        {
            p_sys->atom_busy = false;
            break;
        }

        click_event_t evt_click; // get click point
        bool b_has_click = false;
        if ( p_sys->evt_current_click.b_pending ) {
            evt_click = p_sys->evt_current_click;
            p_sys->evt_current_click.b_pending = false;
            b_has_click = true;
        }
        bool b_text_mode = p_sys->atom_run_text_segment.exchange( false );
        lck_lock.unlock( );

        if ( ( !b_has_click && !b_text_mode ) || !p_sys->atom_paused )
        {
            continue;
        }

        if ( !p_sys->atom_active )
        {
            p_sys->atom_busy = false;
            break;
        }

        // clone frames
        p_sys->atom_busy = true;
        cv::Mat mat_frame_copy;
        cv::Mat mat_original_copy;
        {
            std::lock_guard lck_lock1( p_sys->mtx_frame_mutex );
            if ( !p_sys->mat_current_frame.empty( ) )
            {
                mat_frame_copy = p_sys->mat_current_frame;      // shallow
                mat_original_copy = p_sys->mat_original_frame;   // shallow
            }
        }

        if ( mat_frame_copy.empty( ) || mat_original_copy.empty( ) )
        {
            p_sys->atom_busy = false;
            continue;
        }

        if ( !p_sys->atom_active || !p_sys->ptr_sam_model || !p_sys->ptr_sam_state )
        {
            p_sys->atom_busy = false;
            break;
        }

        if ( !p_sys->atom_preprocessed )
        {
            sam3_image img_frame;
            img_frame.width = mat_frame_copy.cols;
            img_frame.height = mat_frame_copy.rows;
            img_frame.channels = 3;
            cv::Mat mat_rgb;
            cv::cvtColor( mat_frame_copy, mat_rgb, cv::COLOR_BGR2RGB );
            img_frame.data.assign( mat_rgb.datastart, mat_rgb.dataend );

            if ( !sam3_encode_image( *p_sys->ptr_sam_state, *p_sys->ptr_sam_model, img_frame ) )
            {
                msg_Err( p_filter, "Failed to preprocess image" );
                p_sys->atom_busy = false;
                continue;
            }
            p_sys->atom_preprocessed = true;
        }

        if ( b_text_mode )
        {
            sam3_pcs_params pcs_params;
            pcs_params.text_prompt = p_sys->psz_text_prompt;
            auto [detections] = sam3_segment_pcs( *p_sys->ptr_sam_state,
                                                  *p_sys->ptr_sam_model, pcs_params );

            std::lock_guard lck_obj( p_sys->mtx_objects_mutex );
            for ( const auto& det : detections )
            {
                if ( det.mask.data.empty( ) || det.mask.width <= 0 || det.mask.height <= 0 )
                {
                    continue;
                }

                cv::Mat mat_mask( det.mask.height, det.mask.width, CV_8UC1,
                                  const_cast<uint8_t*>( det.mask.data.data( ) ) );
                if ( mat_mask.size( ) != mat_frame_copy.size( ) )
                {
                    cv::Mat mat_resized;
                    cv::resize( mat_mask, mat_resized, mat_frame_copy.size( ), 0, 0, cv::INTER_NEAREST );
                    mat_mask = mat_resized;
                }
                else
                {
                    mat_mask = mat_mask.clone( );
                }

                std::vector<std::vector<cv::Point>> vec_contours;
                cv::findContours( mat_mask, vec_contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE );
                if ( vec_contours.empty( ) )
                {
                    continue;
                }
                for ( auto& vec_contour : vec_contours )
                {
                    cv::approxPolyDP( vec_contour, vec_contour, 2.0, true );
                }

                int i_object_id = p_sys->i_next_object_id++;
                segmented_object_t& obj_new = p_sys->map_objects[ i_object_id ];
                obj_new.i_id = i_object_id;
                obj_new.mat_mask = mat_mask;
                obj_new.vec_contours = vec_contours;
                obj_new.scalar_color = p_sys->scalar_outline_color;
            }

            p_sys->atom_busy = false;
            continue;
        }

        // add positive/negative click points
        sam3_pvs_params pvs_params;
        pvs_params.multimask = true;

        if ( !p_sys->atom_active )
        {
            p_sys->atom_busy = false;
            break;
        }

        int i_target_id;
        {
            std::lock_guard lck_obj( p_sys->mtx_objects_mutex );

            // get segmented points and update objects under a single lock
            segmented_object_t* p_obj = nullptr;
            if ( evt_click.i_target_id > 0 && p_sys->map_objects.find( evt_click.i_target_id ) != p_sys->map_objects.end( ) )
            {
                p_obj = &p_sys->map_objects[ evt_click.i_target_id ];
            }
            else
            {
                int i_object_id = p_sys->i_next_object_id++;
                p_sys->map_objects[ i_object_id ] = segmented_object_t( );
                p_obj = &p_sys->map_objects[ i_object_id ];
                p_obj->i_id = i_object_id;
                p_obj->scalar_color = p_sys->scalar_outline_color;
            }

            // Check click limits
            if ( p_obj->vec_positive_clicks.size( ) + p_obj->vec_negative_clicks.size( ) >= 20 )
            {
                msg_Warn( p_filter, "Maximum number of clicks reached for object %d", p_obj->i_id );
                p_sys->atom_busy = false;
                continue;
            }

            // add positive/negative click feedback with bounds checking
            if ( evt_click.b_positive )
            {
                if ( ( evt_click.pt_click.x != 0 || evt_click.pt_click.y != 0 ) &&
                    evt_click.pt_click.x >= 0 && evt_click.pt_click.x < p_sys->sz_input_size.width &&
                    evt_click.pt_click.y >= 0 && evt_click.pt_click.y < p_sys->sz_input_size.height )
                {
                    p_obj->vec_positive_clicks.push_back( evt_click.pt_click );
                }
            }
            else
            {
                if ( ( evt_click.pt_click.x != 0 || evt_click.pt_click.y != 0 ) &&
                    evt_click.pt_click.x >= 0 && evt_click.pt_click.x < p_sys->sz_input_size.width &&
                    evt_click.pt_click.y >= 0 && evt_click.pt_click.y < p_sys->sz_input_size.height )
                {
                    p_obj->vec_negative_clicks.push_back( evt_click.pt_click );
                }
            }

            for ( const auto& pt : p_obj->vec_positive_clicks )
            {
                pvs_params.pos_points.push_back( { static_cast<float>( pt.x ), static_cast<float>( pt.y ) } );
            }
            for ( const auto& pt : p_obj->vec_negative_clicks )
            {
                pvs_params.neg_points.push_back( { static_cast<float>( pt.x ), static_cast<float>( pt.y ) } );
            }

            if ( p_obj->vec_positive_clicks.empty( ) )
            {
                msg_Warn( p_filter, "Need at least one positive click to create a mask" );
                p_sys->atom_busy = false;
                continue;
            }

            i_target_id = p_obj->i_id;
        }

        auto [detections] = sam3_segment_pvs( *p_sys->ptr_sam_state,
                                              *p_sys->ptr_sam_model, pvs_params );
        cv::Mat mat_best_mask;
        double d_best_score = -1.0;
        for ( const auto& det : detections )
        {
            if ( det.mask.data.empty( ) || det.mask.width <= 0 || det.mask.height <= 0 )
            {
                continue;
            }

            cv::Mat mat_mask( det.mask.height, det.mask.width, CV_8UC1,
                              const_cast<uint8_t*>( det.mask.data.data( ) ) );
            if ( mat_mask.size( ) != mat_frame_copy.size( ) )
            {
                cv::Mat mat_resized;
                cv::resize( mat_mask, mat_resized, mat_frame_copy.size( ), 0, 0, cv::INTER_NEAREST );
                mat_mask = mat_resized;
            }
            else
            {
                mat_mask = mat_mask.clone( );
            }

            std::vector<std::vector<cv::Point>> vec_temp_contours;
            cv::findContours( mat_mask, vec_temp_contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE );
            if ( vec_temp_contours.empty( ) )
            {
                continue;
            }

            double d_area = cv::contourArea( vec_temp_contours[ 0 ] );
            double d_perimeter = cv::arcLength( vec_temp_contours[ 0 ], true );
            auto f_compactness = static_cast<float>( 4.0 * CV_PI * d_area / ( d_perimeter * d_perimeter ) );
            if ( const double d_score = f_compactness * d_area / ( mat_mask.rows * mat_mask.cols );
                d_score > d_best_score )
            {
                d_best_score = d_score;
                mat_best_mask = mat_mask;
            }
        }

        if ( mat_best_mask.empty( ) )
        {
            p_sys->atom_busy = false;
            continue;
        }

        int i_kernel_size = 9;
        cv::Mat mat_kernel = cv::getStructuringElement( cv::MORPH_ELLIPSE, cv::Size( i_kernel_size, i_kernel_size ) );
        cv::morphologyEx( mat_best_mask, mat_best_mask, cv::MORPH_CLOSE, mat_kernel );
        cv::morphologyEx( mat_best_mask, mat_best_mask, cv::MORPH_OPEN, mat_kernel );

        std::vector<std::vector<cv::Point>> vec_contours;
        cv::findContours( mat_best_mask, vec_contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE );
        if ( vec_contours.empty( ) )
        {
            p_sys->atom_busy = false;
            continue;
        }
        for ( auto& vec_contour : vec_contours )
        {
            cv::approxPolyDP( vec_contour, vec_contour, 2.0, true );
        }

        cv::Mat mat_dilated_mask;
        cv::dilate( mat_best_mask, mat_dilated_mask, mat_kernel );

        int i_exported_id;
        {
            std::lock_guard lck_obj( p_sys->mtx_objects_mutex );

            auto it_target = p_sys->map_objects.find( i_target_id );
            if ( it_target == p_sys->map_objects.end( ) )
            {
                // Unpause cleared the target between phases; drop the result.
                p_sys->atom_busy = false;
                continue;
            }
            segmented_object_t* p_obj = &it_target->second;
            p_obj->mat_mask = mat_best_mask.clone( );
            p_obj->vec_contours = vec_contours;
            p_obj->vec_scaled_contours.clear( );

            std::vector<int> vec_merge_ids;
            for ( auto& [ i_other_id, obj_other ] : p_sys->map_objects )
            {
                if ( i_other_id == p_obj->i_id || obj_other.mat_mask.empty( ) )
                {
                    continue;
                }

                cv::Mat mat_intersection;
                cv::bitwise_and( mat_dilated_mask, obj_other.mat_mask, mat_intersection );

                int i_intersection_pixels = cv::countNonZero( mat_intersection );
                if ( int i_other_pixels = cv::countNonZero( obj_other.mat_mask ); i_intersection_pixels > 0 &&
                    static_cast<double>( i_intersection_pixels ) / i_other_pixels > 0.1 )
                {
                    vec_merge_ids.push_back( i_other_id );
                }
            }

            for ( int i_merge_id : vec_merge_ids )
            {
                auto& obj_other = p_sys->map_objects[ i_merge_id ];
                cv::bitwise_or( p_obj->mat_mask, obj_other.mat_mask, p_obj->mat_mask );
                p_obj->vec_positive_clicks.insert( p_obj->vec_positive_clicks.end( ),
                                                  obj_other.vec_positive_clicks.begin( ),
                                                  obj_other.vec_positive_clicks.end( ) );
                p_obj->vec_negative_clicks.insert( p_obj->vec_negative_clicks.end( ),
                                                  obj_other.vec_negative_clicks.begin( ),
                                                  obj_other.vec_negative_clicks.end( ) );
                p_sys->map_objects.erase( i_merge_id );
            }

            if ( !vec_merge_ids.empty( ) )
            {
                cv::findContours( p_obj->mat_mask, p_obj->vec_contours,
                                 cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE );
                for ( auto& vec_contour : p_obj->vec_contours )
                {
                    cv::approxPolyDP( vec_contour, vec_contour, 2.0, true );
                }
                p_obj->vec_scaled_contours.clear( );
            }

            i_exported_id = p_obj->i_id;
        }

        segmented_object_t obj_snap;
        {
            std::lock_guard lck_guard( p_sys->mtx_objects_mutex );
            if ( auto it_map = p_sys->map_objects.find( i_exported_id ); it_map != p_sys->map_objects.end( ) )
            {
                obj_snap.i_id = it_map->second.i_id;
                obj_snap.scalar_color = it_map->second.scalar_color;
                obj_snap.mat_mask = it_map->second.mat_mask.clone( );
                obj_snap.vec_contours = it_map->second.vec_contours;
            }
        }

        Export( p_filter, obj_snap, mat_original_copy );

        p_sys->atom_busy = false;
    }
}

/*****************************************************************************
* Filter: Segmentation frame handling
*****************************************************************************/
static picture_t* Filter( filter_t* p_filter, picture_t* p_pic )
{
    auto* p_sys = static_cast<filter_sys_t*>( p_filter->p_sys );

    // only segment objects if paused
    bool b_is_paused = ( p_pic->date == p_sys->i_last_pic_date );
    p_sys->atom_paused = b_is_paused;
    if ( p_sys->atom_was_paused && !b_is_paused )
    {
        std::lock_guard lck_lock( p_sys->mtx_objects_mutex );
        p_sys->map_objects.clear( );
        p_sys->i_next_object_id = 1;

        {
            std::lock_guard lck_frame( p_sys->mtx_frame_mutex );
            p_sys->mat_original_frame.release( );
        }
        // allow the next pause
        p_sys->atom_frame_captured = false;
    }
    if ( !p_sys->atom_was_paused && b_is_paused &&
         p_sys->psz_text_prompt && p_sys->psz_text_prompt[ 0 ] != '\0' )
    {
        std::lock_guard lck_lock( p_sys->mtx_work );
        p_sys->atom_run_text_segment = true;
        p_sys->cv_work.notify_one( );
    }
    p_sys->atom_was_paused = b_is_paused;
    p_sys->i_last_pic_date = p_pic->date;

    // reuse same resized frame
    if ( p_sys->atom_paused && !p_sys->atom_busy && !p_sys->atom_frame_captured )
    {
        video_format_t fmt_in = p_filter->fmt_in.video;
        cv::Mat mat_temp( static_cast<int>( fmt_in.i_visible_height ),
                         static_cast<int>( fmt_in.i_visible_width ),
                         CV_8UC3, p_pic->p[ 0 ].p_pixels, p_pic->p[ 0 ].i_pitch );
        std::lock_guard lck_lock( p_sys->mtx_frame_mutex );
        p_sys->mat_original_frame = mat_temp.clone( );
        cv::resize( mat_temp, p_sys->mat_current_frame, p_sys->sz_input_size );
        p_sys->atom_preprocessed = false;
        p_sys->atom_frame_captured = true;
    }

    // need at least one object to segment
    bool b_has_objects = false;
    {
        std::lock_guard<std::mutex> lck_lock( p_sys->mtx_objects_mutex );
        b_has_objects = !p_sys->map_objects.empty( );
    }

    if ( !b_has_objects )
    {
        return p_pic;
    }

    // clone picture
    picture_t* p_pic_out = filter_NewPicture( p_filter );
    if ( !p_pic_out )
    {
        picture_Release( p_pic );
        return nullptr;
    }
    picture_CopyPixels( p_pic_out, p_pic );
    picture_CopyProperties( p_pic_out, p_pic );

    // create Mat image
    video_format_t fmt_out = p_filter->fmt_out.video;
    const cv::Mat mat_out( static_cast<int>( fmt_out.i_visible_height ),
                          static_cast<int>( fmt_out.i_visible_width ),
                          CV_8UC3, p_pic_out->p[ 0 ].p_pixels, p_pic_out->p[ 0 ].i_pitch );

    {
        std::lock_guard lck_lock( p_sys->mtx_objects_mutex );

        // draw all segmented objects
        for ( auto& [ i_id, obj ] : p_sys->map_objects )
        {
            double d_scale_x = static_cast<double>( fmt_out.i_visible_width ) / p_sys->sz_input_size.width;
            double d_scale_y = static_cast<double>( fmt_out.i_visible_height ) / p_sys->sz_input_size.height;
            const cv::Size sz_out( static_cast<int>( fmt_out.i_visible_width ),
                                  static_cast<int>( fmt_out.i_visible_height ) );

            // rebuild the cache
            if ( obj.vec_scaled_contours.empty( ) || obj.sz_scaled_for != sz_out )
            {
                obj.vec_scaled_contours.clear( );
                for ( const auto& vec_contour : obj.vec_contours )
                {
                    std::vector<cv::Point> vec_scaled;
                    for ( const auto& pt : vec_contour )
                    {
                        vec_scaled.emplace_back(
                            static_cast<int>( pt.x * d_scale_x ),
                            static_cast<int>( pt.y * d_scale_y )
                        );
                    }
                    obj.vec_scaled_contours.push_back( vec_scaled );
                }
                obj.sz_scaled_for = sz_out;
            }

            const auto& vec_scaled_contours = obj.vec_scaled_contours;
            cv::drawContours( mat_out, vec_scaled_contours, -1, obj.scalar_color,
                             p_sys->i_outline_thickness, cv::LINE_AA );

            // draw id and box
            if ( !vec_scaled_contours.empty( ) && !vec_scaled_contours[ 0 ].empty( ) )
            {
                cv::Rect rect_bbox = cv::boundingRect( vec_scaled_contours[ 0 ] );
                cv::putText( mat_out, std::to_string( obj.i_id ),
                            cv::Point( rect_bbox.x, rect_bbox.y - 5 ),
                            cv::FONT_HERSHEY_SIMPLEX, 0.7,
                            obj.scalar_color, 2, cv::LINE_AA );
            }

            // draw current positive clicks
            for ( const auto& pt : obj.vec_positive_clicks )
            {
                cv::Point pt_scaled( static_cast<int>( pt.x * d_scale_x ),
                                    static_cast<int>( pt.y * d_scale_y ) );
                cv::circle( mat_out, pt_scaled, 8,
                           cv::Scalar( 0, 255, 0 ), -1, cv::LINE_AA );
                cv::circle( mat_out, pt_scaled, 9,
                           cv::Scalar( 255, 255, 255 ), 2, cv::LINE_AA );
            }

            // draw current negative clicks
            for ( const auto& pt : obj.vec_negative_clicks )
            {
                cv::Point pt_scaled( static_cast<int>( pt.x * d_scale_x ),
                                    static_cast<int>( pt.y * d_scale_y ) );
                cv::circle( mat_out, pt_scaled, 8,
                           cv::Scalar( 0, 0, 255 ), -1, cv::LINE_AA );
                cv::circle( mat_out, pt_scaled, 9,
                           cv::Scalar( 255, 255, 255 ), 2, cv::LINE_AA );
            }
        }
    }
    picture_Release( p_pic );

    return p_pic_out;
}

/*****************************************************************************
* Mouse: Handles mouse clicks
*****************************************************************************/
static int Mouse( filter_t* p_filter, vlc_mouse_t* p_mouse, const vlc_mouse_t* p_oldmouse )
{
    auto* p_sys = static_cast<filter_sys_t*>( p_filter->p_sys );

    // Left click handling
    const unsigned i_old_pressed = p_oldmouse ? p_oldmouse->i_pressed : 0;
    if ( ( p_mouse->i_pressed & 1 ) && !( i_old_pressed & 1 ) )
    {
        if ( !p_sys->atom_paused )
        {
            return VLC_SUCCESS;
        }

        const video_format_t fmt_out = p_filter->fmt_out.video;
        unsigned int i_frame_x = p_mouse->i_x * fmt_out.i_visible_width / fmt_out.i_width;
        unsigned int i_frame_y = p_mouse->i_y * fmt_out.i_visible_height / fmt_out.i_height;
        unsigned int i_input_x = i_frame_x * p_sys->sz_input_size.width / fmt_out.i_visible_width;
        unsigned int i_input_y = i_frame_y * p_sys->sz_input_size.height / fmt_out.i_visible_height;

        bool b_removed_point = false;
        bool b_post_event = false;
        click_event_t evt_staged{};
        {
            std::lock_guard lck_lock( p_sys->mtx_objects_mutex );
            for ( auto& [ i_id, obj ] : p_sys->map_objects )
            {
                // Check positive clicks
                for ( auto it_map = obj.vec_positive_clicks.begin( ); it_map != obj.vec_positive_clicks.end( ); )
                {
                    int i_dist_x = static_cast<int>( i_input_x ) - it_map->x;
                    int i_dist_y = static_cast<int>( i_input_y ) - it_map->y;
                    if ( int i_dist_sq = i_dist_x * i_dist_x + i_dist_y * i_dist_y; i_dist_sq > 256 )
                    {
                        ++it_map;
                        continue;
                    }

                    it_map = obj.vec_positive_clicks.erase( it_map );
                    b_removed_point = true;

                    evt_staged.pt_click = cv::Point( 0, 0 );
                    evt_staged.b_positive = true;
                    evt_staged.i_target_id = i_id;
                    evt_staged.b_pending = true;
                    b_post_event = true;
                    break;
                }

                if ( b_removed_point ) break;

                // Check negative clicks
                for ( auto it_map = obj.vec_negative_clicks.begin( ); it_map != obj.vec_negative_clicks.end( ); )
                {
                    int i_dist_x = static_cast<int>( i_input_x ) - it_map->x;
                    int i_dist_y = static_cast<int>( i_input_y ) - it_map->y;
                    if ( int i_dist_sq = i_dist_x * i_dist_x + i_dist_y * i_dist_y; i_dist_sq > 256 )
                    {
                        ++it_map;
                        continue;
                    }

                    it_map = obj.vec_negative_clicks.erase( it_map );
                    b_removed_point = true;

                    evt_staged.pt_click = cv::Point( 0, 0 );
                    evt_staged.b_positive = false;
                    evt_staged.i_target_id = i_id;
                    evt_staged.b_pending = true;
                    b_post_event = true;
                    break;
                }

                if ( b_removed_point ) break;
            }
        }

        if ( b_removed_point )
        {
            if ( b_post_event ) {
                std::lock_guard<std::mutex> lck_guard( p_sys->mtx_work );
                p_sys->evt_current_click = evt_staged;
            }
            if ( b_post_event )
                p_sys->cv_work.notify_one( );
            return VLC_EGENERIC;
        }

        // Find target object
        int i_target_id = -1;
        {
            std::lock_guard<std::mutex> lck_lock( p_sys->mtx_objects_mutex );
            cv::Point pt_click( static_cast<int>( i_input_x ), static_cast<int>( i_input_y ) );

            // check if click is in any mask
            for ( const auto& [ i_id, obj ] : p_sys->map_objects )
            {
                if ( obj.mat_mask.empty( ) ) continue;
                if ( pt_click.x < 0 || pt_click.x >= obj.mat_mask.cols ) continue;
                if ( pt_click.y < 0 || pt_click.y >= obj.mat_mask.rows ) continue;
                if ( obj.mat_mask.at<uchar>( pt_click ) == 0 ) continue;

                i_target_id = i_id;
                break;
            }

            // check contours too
            if ( i_target_id == -1 )
            {
                double d_min_distance = 30.0;
                for ( const auto& [ i_id, obj ] : p_sys->map_objects )
                {
                    if ( obj.vec_contours.empty( ) ) continue;

                    for ( const auto& vec_contour : obj.vec_contours )
                    {
                        double d_distance = cv::pointPolygonTest( vec_contour, pt_click, true );
                        if ( std::abs( d_distance ) >= d_min_distance ) continue;

                        d_min_distance = std::abs( d_distance );
                        i_target_id = i_id;
                    }
                }
            }
        }

        // Set the click event
        {
            std::lock_guard lck_lock( p_sys->mtx_work );
            p_sys->evt_current_click.pt_click = cv::Point( static_cast<int>( i_input_x ),
                                                          static_cast<int>( i_input_y ) );
            p_sys->evt_current_click.b_positive = true;
            p_sys->evt_current_click.i_target_id = i_target_id;
            p_sys->evt_current_click.b_pending = true;
        }
        p_sys->cv_work.notify_one( );

        return VLC_EGENERIC; // don't open menu
    }

    // Right click handling
    if ( ( p_mouse->i_pressed & 4 ) && !( i_old_pressed & 4 ) )
    {
        if ( !p_sys->atom_paused )
        {
            msg_Info( p_filter, "Segmentation only works when video is paused" );
            return VLC_SUCCESS;
        }

        const video_format_t fmt_out = p_filter->fmt_out.video;
        unsigned int i_frame_x = p_mouse->i_x * fmt_out.i_visible_width / fmt_out.i_width;
        unsigned int i_frame_y = p_mouse->i_y * fmt_out.i_visible_height / fmt_out.i_height;
        unsigned int i_input_x = i_frame_x * p_sys->sz_input_size.width / fmt_out.i_visible_width;
        unsigned int i_input_y = i_frame_y * p_sys->sz_input_size.height / fmt_out.i_visible_height;

        int i_target_id = -1;
        {
            std::lock_guard lck_lock( p_sys->mtx_objects_mutex );
            cv::Point pt_click( static_cast<int>( i_input_x ), static_cast<int>( i_input_y ) );

            // same logic as above
            for ( const auto& [ i_id, obj ] : p_sys->map_objects )
            {
                if ( obj.mat_mask.empty( ) ) continue;
                if ( pt_click.x < 0 || pt_click.x >= obj.mat_mask.cols ) continue;
                if ( pt_click.y < 0 || pt_click.y >= obj.mat_mask.rows ) continue;
                if ( obj.mat_mask.at<uchar>( pt_click ) == 0 ) continue;

                i_target_id = i_id;
                break;
            }

            // same logic as above
            if ( i_target_id == -1 )
            {
                double d_min_distance = 30.0;
                for ( const auto& [ i_id, obj ] : p_sys->map_objects )
                {
                    if ( obj.vec_contours.empty( ) ) continue;

                    for ( const auto& vec_contour : obj.vec_contours )
                    {
                        double d_distance = cv::pointPolygonTest( vec_contour, pt_click, true );
                        if ( std::abs( d_distance ) >= d_min_distance ) continue;

                        d_min_distance = std::abs( d_distance );
                        i_target_id = i_id;
                    }
                }
            }
        }

        if ( i_target_id <= 0 )
        {
            msg_Info( p_filter, "Right-click on an existing mask to add negative points" );
            return VLC_EGENERIC;
        }

        {
            std::lock_guard lck_lock( p_sys->mtx_work );
            p_sys->evt_current_click.pt_click = cv::Point( static_cast<int>( i_input_x ),
                                                          static_cast<int>( i_input_y ) );
            p_sys->evt_current_click.b_positive = false;
            p_sys->evt_current_click.i_target_id = i_target_id;
            p_sys->evt_current_click.b_pending = true;
        }
        p_sys->cv_work.notify_one( );


        return VLC_EGENERIC;
    }

    // Middle click handling
    if ( ( p_mouse->i_pressed & 2 ) && !( i_old_pressed & 2 ) )
    {
        const video_format_t fmt_out = p_filter->fmt_out.video;
        unsigned int i_frame_x = p_mouse->i_x * fmt_out.i_visible_width / fmt_out.i_width;
        unsigned int i_frame_y = p_mouse->i_y * fmt_out.i_visible_height / fmt_out.i_height;
        unsigned int i_input_x = i_frame_x * p_sys->sz_input_size.width / fmt_out.i_visible_width;
        unsigned int i_input_y = i_frame_y * p_sys->sz_input_size.height / fmt_out.i_visible_height;

        std::lock_guard<std::mutex> lck_lock( p_sys->mtx_objects_mutex );

        int i_removed_id = -1;
        cv::Point pt_click( static_cast<int>( i_input_x ), static_cast<int>( i_input_y ) );

        for ( auto it_map = p_sys->map_objects.begin( ); it_map != p_sys->map_objects.end( ); ++it_map )
        {
            if ( it_map->second.mat_mask.empty( ) ) continue;
            if ( pt_click.x < 0 || pt_click.x >= it_map->second.mat_mask.cols ) continue;
            if ( pt_click.y < 0 || pt_click.y >= it_map->second.mat_mask.rows ) continue;
            if ( it_map->second.mat_mask.at<uchar>( pt_click ) == 0 ) continue;

            i_removed_id = it_map->first;
            p_sys->map_objects.erase( it_map );
            break;
        }

        if ( i_removed_id > 0 )
        {
            msg_Info( p_filter, "Removed object ID: %d", i_removed_id );
        }

        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
* Open: Opens segmentation module
*****************************************************************************/
static int Open( filter_t* p_filter )
{

    msg_Info( p_filter, "Open SAM segmentation filter" );

    // Only support BGR24 (fast)
    if ( const vlc_fourcc_t i_fourcc = p_filter->fmt_in.video.i_chroma; i_fourcc != VLC_CODEC_BGR24 )
    {
        return VLC_EGENERIC;
    }

    // Argument sanitization
    bool b_use_cpu = var_InheritBool( p_filter, "sam-use-cpu" );
    char* psz_sam_model = var_InheritString( p_filter, "sam-model" );
    char* psz_export_path = var_InheritString( p_filter, "sam-export-path" );
    char* psz_color = var_InheritString( p_filter, "sam-outline-color" );
    char* psz_text_prompt = var_InheritString( p_filter, "sam-text-prompt" );

    if ( !psz_export_path )
    {
        msg_Warn( p_filter, "Export path not configured" );
    }

    // Parse and validate outline color
    int i_r = 255, i_g = 0, i_b = 0;
    if ( psz_color )
    {
        std::string str_color( psz_color );
        std::replace( str_color.begin( ), str_color.end( ), ',', ' ' );
        if ( std::istringstream iss_color( str_color ); !( iss_color >> i_r >> i_g >> i_b ) )
        {
            msg_Warn( p_filter, "Invalid color format '%s', using default (255,0,0)", psz_color );
            i_r = 255;
            i_g = 0;
            i_b = 0;
        }
        else
        {
            i_r = std::clamp( i_r, 0, 255 );
            i_g = std::clamp( i_g, 0, 255 );
            i_b = std::clamp( i_b, 0, 255 );
        }

        free( psz_color );
    }

    // Create and initialize filter system
    auto* p_sys = new filter_sys_t( );
    p_filter->p_sys = p_sys;
    p_sys->i_outline_thickness = var_InheritInteger( p_filter, "sam-outline-thickness" );
    p_sys->psz_export_path = psz_export_path;
    p_sys->psz_text_prompt = psz_text_prompt;
    p_sys->atom_run_text_segment = false;
    p_sys->scalar_outline_color = cv::Scalar( i_b, i_g, i_r );
    p_sys->i_next_object_id = 1;
    p_sys->atom_active = true;
    p_sys->atom_busy = false;
    p_sys->atom_paused = false;
    p_sys->atom_was_paused = false;
    p_sys->atom_frame_captured = false;
    p_sys->i_last_pic_date = 0;
    p_sys->evt_current_click.b_pending = false;

    // Initialize SAM model
    try
    {
        if ( !psz_sam_model )
        {
            msg_Err( p_filter, "sam-model path is required" );
            free( psz_sam_model );
            free( psz_export_path );
            free( psz_text_prompt );
            delete p_sys;
            return VLC_EGENERIC;
        }

        sam3_params sam_params;
        sam_params.model_path = psz_sam_model;
        sam_params.n_threads = static_cast<int>( std::thread::hardware_concurrency( ) );
        sam_params.use_gpu = !b_use_cpu;
        // force grid mismatch at inference, automatic size
        sam_params.encode_img_size = 0;

        p_sys->ptr_sam_model = sam3_load_model( sam_params );
        if ( !p_sys->ptr_sam_model )
        {
            msg_Err( p_filter, "Failed to load SAM model" );
            free( psz_sam_model );
            free( psz_export_path );
            free( psz_text_prompt );
            delete p_sys;
            return VLC_EGENERIC;
        }

        p_sys->ptr_sam_state = sam3_create_state( *p_sys->ptr_sam_model, sam_params );
        if ( !p_sys->ptr_sam_state )
        {
            msg_Err( p_filter, "Failed to create SAM state" );
            free( psz_sam_model );
            free( psz_export_path );
            free( psz_text_prompt );
            delete p_sys;
            return VLC_EGENERIC;
        }
        p_sys->sz_input_size = cv::Size( 1008, 1008 );
    }
    catch ( const std::exception& ex_e )
    {
        msg_Err( p_filter, "SAM initialization failed: %s", ex_e.what( ) );
        free( psz_sam_model );
        free( psz_export_path );
        free( psz_text_prompt );
        delete p_sys;
        return VLC_EGENERIC;
    }
    catch ( ... )
    {
        msg_Err( p_filter, "SAM initialization failed with unknown error" );
        free( psz_sam_model );
        free( psz_export_path );
        free( psz_text_prompt );
        delete p_sys;
        return VLC_EGENERIC;
    }
    free( psz_sam_model );

    // set filter operations
    p_filter->ops = &st_filter_ops;
    p_sys->thrd_processing = std::thread( Process, p_filter );

    return VLC_SUCCESS;
}

/*****************************************************************************
* Close: Releases all resources
*****************************************************************************/
static void Close( filter_t* p_filter )
{
    auto* p_sys = static_cast<filter_sys_t*>( p_filter->p_sys );

    if (!p_sys) return;

    // Signal thread
    p_sys->atom_active = false;

    // Wake up the thread
    {
        std::lock_guard lock(p_sys->mtx_work);
        p_sys->cv_work.notify_all();
    }

    // Wait for processing to complete
    if ( p_sys->thrd_processing.joinable( ) )
    {
        p_sys->thrd_processing.join( );
    }

    {
        std::lock_guard lck_lock( p_sys->mtx_frame_mutex );
        p_sys->mat_current_frame.release( );
        p_sys->mat_original_frame.release( );
    }

    {
        std::lock_guard lck_lock( p_sys->mtx_objects_mutex );
        p_sys->map_objects.clear( );
    }

    free( p_sys->psz_export_path );
    free( p_sys->psz_text_prompt );
    delete p_sys;
}
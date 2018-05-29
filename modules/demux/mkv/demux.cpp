
/*****************************************************************************
 * mkv.cpp : matroska demuxer
 *****************************************************************************
 * Copyright (C) 2003-2004 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Steve Lhomme <steve.lhomme@free.fr>
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

#include "demux.hpp"
#include "stream_io_callback.hpp"
#include "Ebml_parser.hpp"

#include <vlc_actions.h>

event_thread_t::event_thread_t(demux_t *p_demux) : p_demux(p_demux)
{
    vlc_mutex_init( &lock );
    vlc_cond_init( &wait );
    is_running = false;
}
event_thread_t::~event_thread_t()
{
    ResetPci();
    vlc_cond_destroy( &wait );
    vlc_mutex_destroy( &lock );
}

void event_thread_t::SetPci(const pci_t *data)
{
    vlc_mutex_locker l(&lock);

    pci_packet = *data;

#ifndef WORDS_BIGENDIAN
    for( uint8_t button = 1; button <= pci_packet.hli.hl_gi.btn_ns; button++) {
        btni_t *button_ptr = &(pci_packet.hli.btnit[button-1]);
        binary *p_data = (binary*) button_ptr;

        uint16 i_x_start = ((p_data[0] & 0x3F) << 4 ) + ( p_data[1] >> 4 );
        uint16 i_x_end   = ((p_data[1] & 0x03) << 8 ) + p_data[2];
        uint16 i_y_start = ((p_data[3] & 0x3F) << 4 ) + ( p_data[4] >> 4 );
        uint16 i_y_end   = ((p_data[4] & 0x03) << 8 ) + p_data[5];
        button_ptr->x_start = i_x_start;
        button_ptr->x_end   = i_x_end;
        button_ptr->y_start = i_y_start;
        button_ptr->y_end   = i_y_end;

    }
    for ( uint8_t i = 0; i<3; i++ )
        for ( uint8_t j = 0; j<2; j++ )
            pci_packet.hli.btn_colit.btn_coli[i][j] = U32_AT( &pci_packet.hli.btn_colit.btn_coli[i][j] );
#endif
    if( !is_running )
    {
        b_abort = false;
        is_running = !vlc_clone( &thread, EventThread, this, VLC_THREAD_PRIORITY_LOW );
    }
}
void event_thread_t::ResetPci()
{
    if( !is_running )
        return;

    vlc_mutex_lock( &lock );
    b_abort = true;
    vlc_cond_signal( &wait );
    vlc_mutex_unlock( &lock );

    vlc_join( thread, NULL );
    is_running = false;
}
int event_thread_t::EventMouse( vlc_object_t *p_this, char const *psz_var,
                                vlc_value_t, vlc_value_t, void *p_data )
{
    event_thread_t *p_ev = (event_thread_t *) p_data;
    vlc_mutex_lock( &p_ev->lock );
    if( psz_var[6] == 'c' )
    {
        p_ev->b_clicked = true;
        msg_Dbg( p_this, "Event Mouse: clicked");
    }
    else if( psz_var[6] == 'm' )
        p_ev->b_moved = true;
    vlc_cond_signal( &p_ev->wait );
    vlc_mutex_unlock( &p_ev->lock );

    return VLC_SUCCESS;
}

int event_thread_t::EventKey( vlc_object_t *p_this, char const *,
                              vlc_value_t, vlc_value_t newval, void *p_data )
{
    event_thread_t *p_ev = (event_thread_t *) p_data;
    vlc_mutex_lock( &p_ev->lock );
    p_ev->i_key_action = newval.i_int;
    vlc_cond_signal( &p_ev->wait );
    vlc_mutex_unlock( &p_ev->lock );
    msg_Dbg( p_this, "Event Key");

    return VLC_SUCCESS;
}

int event_thread_t::EventInput( vlc_object_t *p_this, char const *,
                                vlc_value_t, vlc_value_t newval, void *p_data )
{
    VLC_UNUSED( p_this );
    event_thread_t *p_ev = (event_thread_t *) p_data;
    vlc_mutex_lock( &p_ev->lock );
    if( newval.i_int == INPUT_EVENT_VOUT )
    {
        p_ev->b_vout |= true;
        vlc_cond_signal( &p_ev->wait );
    }
    vlc_mutex_unlock( &p_ev->lock );

    return VLC_SUCCESS;
}

void event_thread_t::EventThread()
{
    demux_sys_t    *p_sys = p_demux->p_sys;
    vlc_object_t   *p_vout = NULL;
    int canc = vlc_savecancel ();

    b_moved      = false;
    b_clicked    = false;
    i_key_action = 0;
    b_vout       = true;

    /* catch all key event */
    var_AddCallback( p_demux->obj.libvlc, "key-action", EventKey, this );
    /* catch input event */
    var_AddCallback( p_demux->p_input, "intf-event", EventInput, this );

    /* main loop */
    for( ;; )
    {
        vlc_mutex_lock( &lock );
        while( !b_abort && !i_key_action && !b_moved && !b_clicked && !b_vout)
            vlc_cond_wait( &wait, &lock );

        if( b_abort )
        {
            vlc_mutex_unlock( &lock );
            break;
        }

        /* KEY part */
        if( i_key_action )
        {
            msg_Dbg( p_demux, "Handle Key Event");

            pci_t *pci = &pci_packet;

            uint16 i_curr_button = p_sys->dvd_interpretor.GetSPRM( 0x88 );

            switch( i_key_action )
            {
            case ACTIONID_NAV_LEFT:
                if ( i_curr_button > 0 && i_curr_button <= pci->hli.hl_gi.btn_ns )
                {
                    btni_t *p_button_ptr = &(pci->hli.btnit[i_curr_button-1]);
                    if ( p_button_ptr->left > 0 && p_button_ptr->left <= pci->hli.hl_gi.btn_ns )
                    {
                        i_curr_button = p_button_ptr->left;
                        p_sys->dvd_interpretor.SetSPRM( 0x88, i_curr_button );
                        btni_t button_ptr = pci->hli.btnit[i_curr_button-1];
                        if ( button_ptr.auto_action_mode )
                        {
                            vlc_mutex_unlock( &lock );
                            vlc_mutex_lock( &p_sys->lock_demuxer );

                            // process the button action
                            p_sys->dvd_interpretor.Interpret( button_ptr.cmd.bytes, 8 );

                            vlc_mutex_unlock( &p_sys->lock_demuxer );
                            vlc_mutex_lock( &lock );
                        }
                    }
                }
                break;
            case ACTIONID_NAV_RIGHT:
                if ( i_curr_button > 0 && i_curr_button <= pci->hli.hl_gi.btn_ns )
                {
                    btni_t *p_button_ptr = &(pci->hli.btnit[i_curr_button-1]);
                    if ( p_button_ptr->right > 0 && p_button_ptr->right <= pci->hli.hl_gi.btn_ns )
                    {
                        i_curr_button = p_button_ptr->right;
                        p_sys->dvd_interpretor.SetSPRM( 0x88, i_curr_button );
                        btni_t button_ptr = pci->hli.btnit[i_curr_button-1];
                        if ( button_ptr.auto_action_mode )
                        {
                            vlc_mutex_unlock( &lock );
                            vlc_mutex_lock( &p_sys->lock_demuxer );

                            // process the button action
                            p_sys->dvd_interpretor.Interpret( button_ptr.cmd.bytes, 8 );

                            vlc_mutex_unlock( &p_sys->lock_demuxer );
                            vlc_mutex_lock( &lock );
                        }
                    }
                }
                break;
            case ACTIONID_NAV_UP:
                if ( i_curr_button > 0 && i_curr_button <= pci->hli.hl_gi.btn_ns )
                {
                    btni_t *p_button_ptr = &(pci->hli.btnit[i_curr_button-1]);
                    if ( p_button_ptr->up > 0 && p_button_ptr->up <= pci->hli.hl_gi.btn_ns )
                    {
                        i_curr_button = p_button_ptr->up;
                        p_sys->dvd_interpretor.SetSPRM( 0x88, i_curr_button );
                        btni_t button_ptr = pci->hli.btnit[i_curr_button-1];
                        if ( button_ptr.auto_action_mode )
                        {
                            vlc_mutex_unlock( &lock );
                            vlc_mutex_lock( &p_sys->lock_demuxer );

                            // process the button action
                            p_sys->dvd_interpretor.Interpret( button_ptr.cmd.bytes, 8 );

                            vlc_mutex_unlock( &p_sys->lock_demuxer );
                            vlc_mutex_lock( &lock );
                        }
                    }
                }
                break;
            case ACTIONID_NAV_DOWN:
                if ( i_curr_button > 0 && i_curr_button <= pci->hli.hl_gi.btn_ns )
                {
                    btni_t *p_button_ptr = &(pci->hli.btnit[i_curr_button-1]);
                    if ( p_button_ptr->down > 0 && p_button_ptr->down <= pci->hli.hl_gi.btn_ns )
                    {
                        i_curr_button = p_button_ptr->down;
                        p_sys->dvd_interpretor.SetSPRM( 0x88, i_curr_button );
                        btni_t button_ptr = pci->hli.btnit[i_curr_button-1];
                        if ( button_ptr.auto_action_mode )
                        {
                            vlc_mutex_unlock( &lock );
                            vlc_mutex_lock( &p_sys->lock_demuxer );

                            // process the button action
                            p_sys->dvd_interpretor.Interpret( button_ptr.cmd.bytes, 8 );

                            vlc_mutex_unlock( &p_sys->lock_demuxer );
                            vlc_mutex_lock( &lock );
                        }
                    }
                }
                break;
            case ACTIONID_NAV_ACTIVATE:
                if ( i_curr_button > 0 && i_curr_button <= pci->hli.hl_gi.btn_ns )
                {
                    btni_t button_ptr = pci->hli.btnit[i_curr_button-1];

                    vlc_mutex_unlock( &lock );
                    vlc_mutex_lock( &p_sys->lock_demuxer );

                    // process the button action
                    p_sys->dvd_interpretor.Interpret( button_ptr.cmd.bytes, 8 );

                    vlc_mutex_unlock( &p_sys->lock_demuxer );
                    vlc_mutex_lock( &lock );
                }
                break;
            default:
                break;
            }
            i_key_action = 0;
        }

        /* MOUSE part */
        if( p_vout && ( b_moved || b_clicked ) )
        {
            int x, y;

            var_GetCoords( p_vout, "mouse-moved", &x, &y );
            pci_t *pci = &pci_packet;

            if( b_clicked )
            {
                int32_t button;
                int32_t best,dist,d;
                int32_t mx,my,dx,dy;

                msg_Dbg( p_demux, "Handle Mouse Event: Mouse clicked x(%d)*y(%d)", x, y);

                // get current button
                best = 0;
                dist = 0x08000000; /* >> than  (720*720)+(567*567); */
                for(button = 1; button <= pci->hli.hl_gi.btn_ns; button++)
                {
                    btni_t *button_ptr = &(pci->hli.btnit[button-1]);

                    if(((unsigned)x >= button_ptr->x_start)
                     && ((unsigned)x <= button_ptr->x_end)
                     && ((unsigned)y >= button_ptr->y_start)
                     && ((unsigned)y <= button_ptr->y_end))
                    {
                        mx = (button_ptr->x_start + button_ptr->x_end)/2;
                        my = (button_ptr->y_start + button_ptr->y_end)/2;
                        dx = mx - x;
                        dy = my - y;
                        d = (dx*dx) + (dy*dy);
                        /* If the mouse is within the button and the mouse is closer
                        * to the center of this button then it is the best choice. */
                        if(d < dist) {
                            dist = d;
                            best = button;
                        }
                    }
                }

                if ( best != 0)
                {
                    btni_t button_ptr = pci->hli.btnit[best-1];
                    uint16 i_curr_button = p_sys->dvd_interpretor.GetSPRM( 0x88 );

                    msg_Dbg( &p_sys->demuxer, "Clicked button %d", best );
                    vlc_mutex_unlock( &lock );
                    vlc_mutex_lock( &p_sys->lock_demuxer );

                    // process the button action
                    p_sys->dvd_interpretor.SetSPRM( 0x88, best );
                    p_sys->dvd_interpretor.Interpret( button_ptr.cmd.bytes, 8 );

                    msg_Dbg( &p_sys->demuxer, "Processed button %d", best );

                    // select new button
                    if ( best != i_curr_button )
                    {
                        uint32_t i_palette;

                        if(button_ptr.btn_coln != 0) {
                            i_palette = pci->hli.btn_colit.btn_coli[button_ptr.btn_coln-1][1];
                        } else {
                            i_palette = 0;
                        }

                        for( int i = 0; i < 4; i++ )
                        {
                            uint32_t i_yuv = 0xFF;//p_sys->clut[(hl.palette>>(16+i*4))&0x0f];
                            uint8_t i_alpha = (i_palette>>(i*4))&0x0f;
                            i_alpha = i_alpha == 0xf ? 0xff : i_alpha << 4;

                            p_sys->palette[i][0] = (i_yuv >> 16) & 0xff;
                            p_sys->palette[i][1] = (i_yuv >> 0) & 0xff;
                            p_sys->palette[i][2] = (i_yuv >> 8) & 0xff;
                            p_sys->palette[i][3] = i_alpha;
                        }

                        vlc_global_lock( VLC_HIGHLIGHT_MUTEX );
                        var_SetInteger( p_demux->p_input, "x-start",
                                        button_ptr.x_start );
                        var_SetInteger( p_demux->p_input, "x-end",
                                        button_ptr.x_end );
                        var_SetInteger( p_demux->p_input, "y-start",
                                        button_ptr.y_start );
                        var_SetInteger( p_demux->p_input, "y-end",
                                        button_ptr.y_end );
                        var_SetAddress( p_demux->p_input, "menu-palette",
                                        p_sys->palette );
                        var_SetBool( p_demux->p_input, "highlight", true );
                        vlc_global_unlock( VLC_HIGHLIGHT_MUTEX );
                    }
                    vlc_mutex_unlock( &p_sys->lock_demuxer );
                    vlc_mutex_lock( &lock );
                }
            }
            else if( b_moved )
            {
//                dvdnav_mouse_select( NULL, pci, x, y );
            }

            b_moved = false;
            b_clicked = false;
        }

        b_vout = false;
        vlc_mutex_unlock( &lock );

        /* Always check vout */
        if( p_vout == NULL )
        {
            p_vout = (vlc_object_t*) input_GetVout(p_demux->p_input);
            if( p_vout)
            {
                var_AddCallback( p_vout, "mouse-moved", EventMouse, this );
                var_AddCallback( p_vout, "mouse-clicked", EventMouse, this );
            }
        }
    }

    /* Release callback */
    if( p_vout )
    {
        var_DelCallback( p_vout, "mouse-moved", EventMouse, this );
        var_DelCallback( p_vout, "mouse-clicked", EventMouse, this );
        vlc_object_release( p_vout );
    }
    var_DelCallback( p_demux->p_input, "intf-event", EventInput, this );
    var_DelCallback( p_demux->obj.libvlc, "key-action", EventKey, this );

    vlc_restorecancel (canc);
}

void *event_thread_t::EventThread(void *data)
{
    static_cast<event_thread_t*>(data)->EventThread();
    return NULL;
}


demux_sys_t::~demux_sys_t()
{
    CleanUi();
    size_t i;
    for ( i=0; i<streams.size(); i++ )
        delete streams[i];
    for ( i=0; i<opened_segments.size(); i++ )
        delete opened_segments[i];
    for ( i=0; i<used_vsegments.size(); i++ )
        delete used_vsegments[i];
    for ( i=0; i<stored_attachments.size(); i++ )
        delete stored_attachments[i];
    if( meta ) vlc_meta_Delete( meta );

    while( titles.size() )
    { vlc_input_title_Delete( titles.back() ); titles.pop_back();}

    vlc_mutex_destroy( &lock_demuxer );
}


bool demux_sys_t::AnalyseAllSegmentsFound( demux_t *p_demux, matroska_stream_c *p_stream1, bool b_initial )
{
    int i_upper_lvl = 0;
    EbmlElement *p_l0;
    bool b_keep_stream = false, b_keep_segment = false;

    /* verify the EBML Header... it shouldn't be bigger than 1kB */
    p_l0 = p_stream1->estream.FindNextID(EBML_INFO(EbmlHead), 1024);
    if (p_l0 == NULL)
    {
        msg_Err( p_demux, "No EBML header found" );
        return false;
    }

    /* verify we can read this Segment */
    try
    {
        p_l0->Read( p_stream1->estream, EBML_CLASS_CONTEXT(EbmlHead), i_upper_lvl, p_l0, true);
    }
    catch(...)
    {
        msg_Err(p_demux, "EBML Header Read failed");
        return false;
    }

    EDocType doc_type = GetChild<EDocType>(*static_cast<EbmlHead*>(p_l0));
    if (std::string(doc_type) != "matroska" && std::string(doc_type) != "webm" )
    {
        msg_Err( p_demux, "Not a Matroska file : DocType = %s ", std::string(doc_type).c_str());
        return false;
    }

    EDocTypeReadVersion doc_read_version = GetChild<EDocTypeReadVersion>(*static_cast<EbmlHead*>(p_l0));
    if (uint64(doc_read_version) > 2)
    {
        msg_Err( p_demux, "matroska file needs version %" PRId64 " but only versions 1 & 2 supported", uint64(doc_read_version));
        return false;
    }

    delete p_l0;


    // find all segments in this file
    p_l0 = p_stream1->estream.FindNextID(EBML_INFO(KaxSegment), UINT64_MAX);
    if (p_l0 == NULL)
    {
        msg_Err( p_demux, "No segment found" );
        return false;
    }

    while (p_l0 != 0)
    {
        bool b_l0_handled = false;

        if ( MKV_IS_ID( p_l0, KaxSegment) )
        {
            matroska_segment_c *p_segment1 = new matroska_segment_c( *this, p_stream1->estream, (KaxSegment*)p_l0 );

            p_segment1->Preload();

            if ( !p_segment1->p_segment_uid ||
                 (b_keep_segment = (FindSegment( *p_segment1->p_segment_uid ) == NULL)))
            {
                opened_segments.push_back( p_segment1 );
                b_keep_stream = true;
                p_stream1->segments.push_back( p_segment1 );
            }
            else
            {
                p_segment1->segment = NULL;
                delete p_segment1;
            }

            b_l0_handled = true;
        }

        EbmlElement* p_l0_prev = p_l0;

        bool b_seekable;
        vlc_stream_Control( demuxer.s, STREAM_CAN_SEEK, &b_seekable );

        if (p_l0->IsFiniteSize() && b_seekable )
        {
            p_l0->SkipData(p_stream1->estream, KaxMatroska_Context);
            p_l0 = p_stream1->estream.FindNextID(EBML_INFO(KaxSegment), UINT64_MAX);
        }
        else
        {
            p_l0 = NULL;
        }

        if( b_l0_handled == false )
            delete p_l0_prev;
    }

    if ( !b_keep_stream )
        return false;

    return true;
}

void demux_sys_t::InitUi()
{
    msg_Dbg( &demuxer, "Starting the UI Hook" );

    /* FIXME hack hack hack hack FIXME */
    /* Get p_input and create variable */
    p_input = demuxer.p_input;
    if( p_input )
    {
        var_Create( p_input, "x-start", VLC_VAR_INTEGER );
        var_Create( p_input, "y-start", VLC_VAR_INTEGER );
        var_Create( p_input, "x-end", VLC_VAR_INTEGER );
        var_Create( p_input, "y-end", VLC_VAR_INTEGER );
        var_Create( p_input, "color", VLC_VAR_ADDRESS );
        var_Create( p_input, "menu-palette", VLC_VAR_ADDRESS );
        var_Create( p_input, "highlight", VLC_VAR_BOOL );
    }

    /* Now create our event thread catcher */
    p_ev = new event_thread_t(&demuxer);
}

void demux_sys_t::CleanUi()
{
    delete p_ev;
    p_ev = NULL;

    if( p_input )
    {
        var_Destroy( p_input, "highlight" );
        var_Destroy( p_input, "x-start" );
        var_Destroy( p_input, "x-end" );
        var_Destroy( p_input, "y-start" );
        var_Destroy( p_input, "y-end" );
        var_Destroy( p_input, "color" );
        var_Destroy( p_input, "menu-palette" );
    }

    msg_Dbg( &demuxer, "Stopping the UI Hook" );
}

void demux_sys_t::PreloadFamily( const matroska_segment_c & of_segment )
{
    for (size_t i=0; i<opened_segments.size(); i++)
    {
        opened_segments[i]->PreloadFamily( of_segment );
    }
}

// preload all the linked segments for all preloaded segments
bool demux_sys_t::PreloadLinked()
{
    size_t i, j, ij = 0;
    virtual_segment_c *p_vseg;

    if ( unlikely(opened_segments.size() == 0) )
        return false;

    p_current_vsegment = new (std::nothrow) virtual_segment_c( *(opened_segments[0]), opened_segments );
    if ( !p_current_vsegment )
        return false;

    if ( unlikely(p_current_vsegment->CurrentEdition() == NULL) )
        return false;

    /* Set current chapter */
    p_current_vsegment->p_current_vchapter = p_current_vsegment->CurrentEdition()->getChapterbyTimecode(0);
    msg_Dbg( &demuxer, "NEW START CHAPTER uid=%" PRId64, p_current_vsegment->p_current_vchapter && p_current_vsegment->p_current_vchapter->p_chapter ?
                 p_current_vsegment->p_current_vchapter->p_chapter->i_uid : 0 );

    used_vsegments.push_back( p_current_vsegment );

    for ( i=1; i< opened_segments.size(); i++ )
    {
        /* add segments from the same family to used_segments */
        if ( opened_segments[0]->SameFamily( *(opened_segments[i]) ) )
        {
            virtual_segment_c *p_vsegment = new (std::nothrow) virtual_segment_c( *(opened_segments[i]), opened_segments );
            if ( likely(p_vsegment != NULL) )
                used_vsegments.push_back( p_vsegment );
        }
    }

    // publish all editions of all usable segment
    for ( i=0; i< used_vsegments.size(); i++ )
    {
        p_vseg = used_vsegments[i];
        if ( p_vseg->Editions() != NULL )
        {
            for ( j=0; j<p_vseg->Editions()->size(); j++ )
            {
                virtual_edition_c * p_ved = (*p_vseg->Editions())[j];
                input_title_t *p_title = vlc_input_title_New();
                int i_chapters;

                // TODO use a name for each edition, let the TITLE deal with a codec name
                if ( p_title->psz_name == NULL )
                {
                    if( p_ved->GetMainName().length() )
                        p_title->psz_name = strdup( p_ved->GetMainName().c_str() );
                    else
                    {
                        /* Check in tags if the edition has a name */

                        /* We use only the tags of the first segment as it contains the edition */
                        matroska_segment_c::tags_t const& tags = opened_segments[0]->tags;
                        uint64_t i_ed_uid = 0;
                        if( p_ved->p_edition )
                            i_ed_uid = (uint64_t) p_ved->p_edition->i_uid;

                        for( size_t k = 0; k < tags.size(); k++ )
                        {
                            if( tags[k].i_tag_type == EDITION_UID && tags[k].i_uid == i_ed_uid )
                                for( size_t l = 0; l < tags[k].simple_tags.size(); l++ )
                                {
                                    SimpleTag const& st = tags[k].simple_tags[l];
                                    if ( st.tag_name == "TITLE" )
                                    {
                                        msg_Dbg( &demuxer, "Using title \"%s\" from tag for edition %" PRIu64, st.value.c_str (), i_ed_uid );
                                        p_title->psz_name = strdup( st.value.c_str () );
                                        break;
                                    }
                                }
                        }

                        if( !p_title->psz_name &&
                            asprintf(&(p_title->psz_name), "%s %d", "Segment", (int)ij) == -1 )
                            p_title->psz_name = NULL;
                    }
                }

                ij++;
                i_chapters = 0;
                p_ved->PublishChapters( *p_title, i_chapters, 0 );

                // Input duration into i_length
                p_title->i_length = p_ved->i_duration;

                titles.push_back( p_title );
            }
        }
        p_vseg->i_sys_title = p_vseg->i_current_edition;
    }

    // TODO decide which segment should be first used (VMG for DVD)

    return true;
}

void demux_sys_t::FreeUnused()
{
    size_t i;
    for( i = 0; i < streams.size(); i++ )
    {
        struct matroska_stream_c *p_s = streams[i];
        if( !p_s->isUsed() )
        {
            streams[i] = NULL;
            delete p_s;
        }
    }
    for( i = 0; i < opened_segments.size(); i++)
    {
        if( !opened_segments[i]->b_preloaded )
        {
            delete opened_segments[i];
            opened_segments[i] = NULL;
        }
    }
}

bool demux_sys_t::PreparePlayback( virtual_segment_c & new_vsegment, mtime_t i_mk_date )
{
    if ( p_current_vsegment != &new_vsegment )
    {
        if ( p_current_vsegment->CurrentSegment() != NULL )
            p_current_vsegment->CurrentSegment()->ESDestroy();

        p_current_vsegment = &new_vsegment;
        p_current_vsegment->CurrentSegment()->ESCreate();
        i_current_title = p_current_vsegment->i_sys_title;
    }
    if( !p_current_vsegment->CurrentSegment() )
        return false;
    if( !p_current_vsegment->CurrentSegment()->b_cues )
        msg_Warn( &p_current_vsegment->CurrentSegment()->sys.demuxer, "no cues/empty cues found->seek won't be precise" );

    f_duration = p_current_vsegment->Duration();

    /* add information */
    p_current_vsegment->CurrentSegment()->InformationCreate( );
    p_current_vsegment->CurrentSegment()->ESCreate( );

    /* Seek to the beginning */
    p_current_vsegment->Seek(p_current_vsegment->CurrentSegment()->sys.demuxer,
                             i_mk_date, p_current_vsegment->p_current_vchapter );

    return true;
}

void demux_sys_t::JumpTo( virtual_segment_c & vsegment, virtual_chapter_c & vchapter )
{
    if ( !vchapter.p_chapter || !vchapter.p_chapter->Enter( true ) )
    {
        // jump to the location in the found segment
        vsegment.Seek( demuxer, vchapter.i_mk_virtual_start_time, &vchapter );
    }
}

matroska_segment_c *demux_sys_t::FindSegment( const EbmlBinary & uid ) const
{
    for (size_t i=0; i<opened_segments.size(); i++)
    {
        if ( opened_segments[i]->p_segment_uid && *opened_segments[i]->p_segment_uid == uid )
            return opened_segments[i];
    }
    return NULL;
}

virtual_chapter_c *demux_sys_t::BrowseCodecPrivate( unsigned int codec_id,
                                        bool (*match)(const chapter_codec_cmds_c &data, const void *p_cookie, size_t i_cookie_size ),
                                        const void *p_cookie,
                                        size_t i_cookie_size,
                                        virtual_segment_c * &p_vsegment_found )
{
    virtual_chapter_c *p_result = NULL;
    for (size_t i=0; i<used_vsegments.size(); i++)
    {
        p_result = used_vsegments[i]->BrowseCodecPrivate( codec_id, match, p_cookie, i_cookie_size );
        if ( p_result != NULL )
        {
            p_vsegment_found = used_vsegments[i];
            break;
        }
    }
    return p_result;
}

virtual_chapter_c *demux_sys_t::FindChapter( int64_t i_find_uid, virtual_segment_c * & p_vsegment_found )
{
    virtual_chapter_c *p_result = NULL;
    for (size_t i=0; i<used_vsegments.size(); i++)
    {
        p_result = used_vsegments[i]->FindChapter( i_find_uid );
        if ( p_result != NULL )
        {
            p_vsegment_found = used_vsegments[i];
            break;
        }
    }
    return p_result;
}


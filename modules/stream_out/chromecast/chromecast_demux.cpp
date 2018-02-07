/*****************************************************************************
 * chromecast_demux.cpp: Chromecast demux filter module for vlc
 *****************************************************************************
 * Copyright © 2015-2016 VideoLAN
 *
 * Authors: Steve Lhomme <robux4@videolabs.io>
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
#include <vlc_demux.h>

#include "chromecast_common.h"

#include <new>

static void on_paused_changed_cb(void *data, bool paused);

struct demux_sys_t
{
    demux_sys_t(demux_t * const demux, chromecast_common * const renderer)
        :p_demux(demux)
        ,p_renderer(renderer)
        ,i_length(-1)
        ,m_enabled( true )
        ,m_startTime( VLC_TS_INVALID )
    {
        vlc_meta_t *p_meta = vlc_meta_New();
        if( likely(p_meta != NULL) )
        {
            input_item_t *p_item = demux->p_next->p_input ?
                                   input_GetItem( demux->p_next->p_input ) : NULL;
            if( p_item )
            {
                /* Favor Meta from the input item of the input_thread since
                 * it's pre-processed by the meta fetcher */
                for( int i = 0; i < VLC_META_TYPE_COUNT; ++i )
                {
                    char *psz_meta = input_item_GetMeta( p_item, (vlc_meta_type_t)i );
                    if( psz_meta )
                    {
                        vlc_meta_Set( p_meta, (vlc_meta_type_t)i, psz_meta );
                        free( psz_meta );
                    }
                }
                if( vlc_meta_Get( p_meta, vlc_meta_Title ) == NULL )
                {
                    char *psz_meta = input_item_GetName( p_item );
                    if( psz_meta )
                    {
                        vlc_meta_Set( p_meta, vlc_meta_Title, psz_meta );
                        free( psz_meta );
                    }
                }
                p_renderer->pf_set_meta( p_renderer->p_opaque, p_meta );
            }
            else if (demux_Control( demux->p_next, DEMUX_GET_META, p_meta) == VLC_SUCCESS)
                p_renderer->pf_set_meta( p_renderer->p_opaque, p_meta );
            else
                vlc_meta_Delete( p_meta );
        }
        if (demux_Control( demux->p_next, DEMUX_CAN_SEEK, &canSeek ) != VLC_SUCCESS)
            canSeek = false;

        int i_current_title;
        if( demux_Control( p_demux->p_next, DEMUX_GET_TITLE,
                           &i_current_title ) == VLC_SUCCESS )
        {
            input_title_t** pp_titles;
            int i_nb_titles, i_title_offset, i_chapter_offset;
            if( demux_Control( demux->p_next, DEMUX_GET_TITLE_INFO, &pp_titles,
                              &i_nb_titles, &i_title_offset,
                              &i_chapter_offset ) == VLC_SUCCESS )
            {
                int64_t i_longest_duration = 0;
                int i_longest_title = 0;
                bool b_is_interactive = false;
                for( int i = 0 ; i < i_nb_titles; ++i )
                {
                    if( pp_titles[i]->i_length > i_longest_duration )
                    {
                        i_longest_duration = pp_titles[i]->i_length;
                        i_longest_title = i;
                    }
                    if( i_current_title == i &&
                            pp_titles[i]->i_flags & INPUT_TITLE_INTERACTIVE )
                    {
                        b_is_interactive = true;
                    }
                    vlc_input_title_Delete( pp_titles[i] );
                }
                free( pp_titles );

                if( b_is_interactive == true )
                {
                    demux_Control( p_demux->p_next, DEMUX_SET_TITLE,
                                   i_longest_title );
                    p_demux->info.i_update = p_demux->p_next->info.i_update;
                }
            }
        }

        p_renderer->pf_set_on_paused_changed_cb(p_renderer->p_opaque,
                                                on_paused_changed_cb, demux);
    }

    ~demux_sys_t()
    {
        if( p_renderer )
        {
            p_renderer->pf_set_meta( p_renderer->p_opaque, NULL );
            p_renderer->pf_set_on_paused_changed_cb( p_renderer->p_opaque,
                                                     NULL, NULL );
        }
    }

    void setPauseState(bool paused)
    {
        p_renderer->pf_set_pause_state( p_renderer->p_opaque, paused );
    }

    /**
     * @brief getPlaybackTime
     * @return the current playback time on the device or VLC_TS_INVALID if unknown
     */
    mtime_t getPlaybackTime()
    {
        return p_renderer->pf_get_time( p_renderer->p_opaque );
    }

    double getPlaybackPosition()
    {
        return p_renderer->pf_get_position( p_renderer->p_opaque );
    }

    void setCanSeek( bool canSeek )
    {
        this->canSeek = canSeek;
    }

    void setLength( mtime_t length )
    {
        this->i_length = length;
        p_renderer->pf_set_length( p_renderer->p_opaque, length );
    }

    int Demux()
    {
        if ( !m_enabled )
            return demux_Demux( p_demux->p_next );

        if( !p_renderer->pf_pace( p_renderer->p_opaque ) )
        {
            // Still pacing, but we return now in order to let the input thread
            // do some controls.
            return VLC_DEMUXER_SUCCESS;
        }

        if( m_startTime == VLC_TS_INVALID )
        {
            if( demux_Control( p_demux->p_next, DEMUX_GET_TIME,
                               &m_startTime ) == VLC_SUCCESS )
                p_renderer->pf_set_initial_time( p_renderer->p_opaque,
                                                 m_startTime );
        }

        return demux_Demux( p_demux->p_next );
    }

    int Control( demux_t *p_demux_filter, int i_query, va_list args )
    {
        if( !m_enabled && i_query != DEMUX_FILTER_ENABLE )
            return demux_vaControl( p_demux_filter->p_next, i_query, args );

        switch (i_query)
        {
        case DEMUX_GET_POSITION:
            *va_arg( args, double * ) = getPlaybackPosition();
            return VLC_SUCCESS;

        case DEMUX_GET_TIME:
            *va_arg(args, int64_t *) = getPlaybackTime();
            return VLC_SUCCESS;

        case DEMUX_GET_LENGTH:
        {
            int ret;
            va_list ap;

            va_copy( ap, args );
            ret = demux_vaControl( p_demux_filter->p_next, i_query, args );
            if( ret == VLC_SUCCESS )
                setLength( *va_arg( ap, int64_t * ) );
            va_end( ap );
            return ret;
        }

        case DEMUX_CAN_SEEK:
        {
            int ret;
            va_list ap;

            va_copy( ap, args );
            ret = demux_vaControl( p_demux_filter->p_next, i_query, args );
            if( ret == VLC_SUCCESS )
                setCanSeek( *va_arg( ap, bool* ) );
            va_end( ap );
            return ret;
        }

        case DEMUX_SET_POSITION:
        {
            m_startTime = VLC_TS_INVALID;
            break;
        }

        case DEMUX_SET_TIME:
        {
            m_startTime = VLC_TS_INVALID;
            break;
        }
        case DEMUX_SET_PAUSE_STATE:
        {
            va_list ap;

            va_copy( ap, args );
            int paused = va_arg( ap, int );
            va_end( ap );

            setPauseState( paused != 0 );
            break;
        }
        case DEMUX_FILTER_ENABLE:
            p_renderer = static_cast<chromecast_common *>(
                        var_InheritAddress( p_demux, CC_SHARED_VAR_NAME ) );
            m_enabled = true;
            return VLC_SUCCESS;

        case DEMUX_FILTER_DISABLE:
            m_enabled = false;
            p_renderer = NULL;
            m_startTime = VLC_TS_INVALID;
            return VLC_SUCCESS;
        case DEMUX_CAN_PAUSE:
        case DEMUX_CAN_CONTROL_PACE:
        {
            int ret;
            va_list ap;

            va_copy( ap, args );
            ret = demux_vaControl( p_demux_filter->p_next, i_query, args );
            if( ret != VLC_SUCCESS )
                *va_arg( ap, bool* ) = false;
            va_end( ap );
            return VLC_SUCCESS;
        }
        case DEMUX_GET_PTS_DELAY:
        {
            int ret;
            va_list ap;

            va_copy( ap, args );
            ret = demux_vaControl( p_demux_filter->p_next, i_query, args );
            if( ret != VLC_SUCCESS )
                *va_arg( ap, int64_t* ) = 0;
            va_end( ap );
            return VLC_SUCCESS;
        }
        }

        return demux_vaControl( p_demux_filter->p_next, i_query, args );
    }

protected:
    demux_t     * const p_demux;
    chromecast_common  * p_renderer;
    mtime_t       i_length;
    bool          canSeek;
    bool          m_enabled;
    mtime_t       m_startTime;
};

static void on_paused_changed_cb( void *data, bool paused )
{
    demux_t *p_demux = reinterpret_cast<demux_t*>(data);

    input_thread_t *p_input = p_demux->p_next->p_input;
    if( p_input )
        input_Control( p_input, INPUT_SET_STATE, paused ? PAUSE_S : PLAYING_S );
}

static int Demux( demux_t *p_demux_filter )
{
    demux_sys_t *p_sys = p_demux_filter->p_sys;

    return p_sys->Demux();
}

static int Control( demux_t *p_demux_filter, int i_query, va_list args)
{
    demux_sys_t *p_sys = p_demux_filter->p_sys;

    return p_sys->Control( p_demux_filter, i_query, args );
}

int Open(vlc_object_t *p_this)
{
    demux_t *p_demux = reinterpret_cast<demux_t*>(p_this);
    chromecast_common *p_renderer = static_cast<chromecast_common *>(
                var_InheritAddress( p_demux, CC_SHARED_VAR_NAME ) );
    if ( p_renderer == NULL )
    {
        msg_Warn( p_this, "using Chromecast demuxer with no sout" );
        return VLC_ENOOBJ;
    }

    demux_sys_t *p_sys = new(std::nothrow) demux_sys_t( p_demux, p_renderer );
    if (unlikely(p_sys == NULL))
        return VLC_ENOMEM;

    p_demux->p_sys = p_sys;
    p_demux->pf_demux = Demux;
    p_demux->pf_control = Control;

    return VLC_SUCCESS;
}

void Close(vlc_object_t *p_this)
{
    demux_t *p_demux = reinterpret_cast<demux_t*>(p_this);
    demux_sys_t *p_sys = p_demux->p_sys;

    delete p_sys;
}

vlc_module_begin ()
    set_shortname( "cc_demux" )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_DEMUX )
    set_description( N_( "Chromecast demux wrapper" ) )
    set_capability( "demux_filter", 0 )
    add_shortcut( "cc_demux" )
    set_callbacks( Open, Close )
vlc_module_end ()

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

struct demux_sys_t
{
    demux_sys_t(demux_t * const demux, chromecast_common * const renderer)
        :p_demux(demux)
        ,p_renderer(renderer)
        ,i_length(-1)
        ,demuxReady(false)
        ,m_seektime( VLC_TS_INVALID )
        ,m_enabled( true )
        ,m_startTime( VLC_TS_INVALID )
    {
        vlc_meta_t *p_meta = vlc_meta_New();
        if( likely(p_meta != NULL) )
        {
            if (demux_Control( demux->p_next, DEMUX_GET_META, p_meta) == VLC_SUCCESS)
            {
                const char *meta = vlc_meta_Get( p_meta, vlc_meta_Title );
                if ( meta != NULL )
                    p_renderer->pf_set_title( p_renderer->p_opaque, meta );
                meta = vlc_meta_Get( p_meta, vlc_meta_ArtworkURL );
                if ( meta != NULL )
                    p_renderer->pf_set_artwork( p_renderer->p_opaque, meta );
            }
            vlc_meta_Delete(p_meta);
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
    }

    ~demux_sys_t()
    {
        if( p_renderer )
        {
            p_renderer->pf_set_title( p_renderer->p_opaque, NULL );
            p_renderer->pf_set_artwork( p_renderer->p_opaque, NULL );
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

    bool seekTo( double pos )
    {
        if (i_length == -1)
            return false;
        return seekTo( mtime_t( i_length * pos ) );
    }

    bool seekTo( mtime_t i_pos )
    {
        if ( !canSeek )
            return false;

        /* seeking will be handled with the Chromecast */
        m_seektime = i_pos;
        p_renderer->pf_request_seek( p_renderer->p_opaque, i_pos );

        return true;
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

        if (!demuxReady)
        {
            msg_Dbg(p_demux, "wait to demux");
            p_renderer->pf_wait_app_started( p_renderer->p_opaque );
            demuxReady = true;
            msg_Dbg(p_demux, "ready to demux");
        }
        if( m_startTime == VLC_TS_INVALID )
        {
            if( demux_Control( p_demux->p_next, DEMUX_GET_TIME,
                               &m_startTime ) == VLC_SUCCESS )
                p_renderer->pf_set_initial_time( p_renderer->p_opaque,
                                                 m_startTime );
        }

        /* hold the data while seeking */
        /* wait until the device is buffering for data after the seek command */
        if ( m_seektime != VLC_TS_INVALID )
        {
            p_renderer->pf_wait_seek_done( p_renderer->p_opaque );
            m_seektime = VLC_TS_INVALID;
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
            va_list ap;

            va_copy( ap, args );
            double pos = va_arg( ap, double );
            va_end( ap );

            if ( getPlaybackTime() == VLC_TS_INVALID )
            {
                msg_Dbg( p_demux_filter, "internal seek to %f when the playback didn't start", pos );
                break; // seek before device started, likely on-the-fly restart
            }

            if ( !seekTo( pos ) )
            {
                msg_Err( p_demux_filter, "failed to seek to %f", pos );
                return VLC_EGENERIC;
            }
            break;
        }

        case DEMUX_SET_TIME:
        {
            va_list ap;

            va_copy( ap, args );
            mtime_t pos = va_arg( ap, mtime_t );
            va_end( ap );

            if ( getPlaybackTime() == VLC_TS_INVALID )
            {
                msg_Dbg( p_demux_filter, "internal seek to %" PRId64 " when the playback didn't start", pos );
                break; // seek before device started, likely on-the-fly restart
            }

            if ( !seekTo( pos ) )
            {
                msg_Err( p_demux_filter, "failed to seek to time %" PRId64, pos );
                return VLC_EGENERIC;
            }
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
        }

        return demux_vaControl( p_demux_filter->p_next, i_query, args );
    }

protected:
    demux_t     * const p_demux;
    chromecast_common  * p_renderer;
    mtime_t       i_length;
    bool          demuxReady;
    bool          canSeek;
    /* seek time kept while waiting for the chromecast to "seek" */
    mtime_t       m_seektime;
    bool          m_enabled;
    mtime_t       m_startTime;
};

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

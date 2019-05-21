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

#include <assert.h>
#include <new>

static void on_paused_changed_cb(void *data, bool paused);

struct demux_cc
{
    demux_cc(demux_t * const demux, chromecast_common * const renderer)
        :p_demux(demux)
        ,p_renderer(renderer)
        ,m_enabled( true )
    {
        init();
    }

    void init()
    {
        resetDemuxEof();

        vlc_meta_t *p_meta = vlc_meta_New();
        if( likely(p_meta != NULL) )
        {
            input_item_t *p_item = p_demux->p_next->p_input_item;
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
            else if (demux_Control( p_demux->p_next, DEMUX_GET_META, p_meta) == VLC_SUCCESS)
                p_renderer->pf_set_meta( p_renderer->p_opaque, p_meta );
            else
                vlc_meta_Delete( p_meta );
        }

        if (demux_Control( p_demux->p_next, DEMUX_CAN_SEEK, &m_can_seek ) != VLC_SUCCESS)
            m_can_seek = false;
        if (demux_Control( p_demux->p_next, DEMUX_GET_LENGTH, &m_length ) != VLC_SUCCESS)
            m_length = -1;

        int i_current_title;
        if( demux_Control( p_demux->p_next, DEMUX_GET_TITLE,
                           &i_current_title ) == VLC_SUCCESS )
        {
            input_title_t** pp_titles;
            int i_nb_titles, i_title_offset, i_chapter_offset;
            if( demux_Control( p_demux->p_next, DEMUX_GET_TITLE_INFO, &pp_titles,
                              &i_nb_titles, &i_title_offset,
                              &i_chapter_offset ) == VLC_SUCCESS )
            {
                vlc_tick_t i_longest_duration = 0;
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
                }
            }
        }

        es_out_Control( p_demux->p_next->out, ES_OUT_RESET_PCR );

        p_renderer->pf_set_demux_enabled(p_renderer->p_opaque, true,
                                         on_paused_changed_cb, p_demux);

        resetTimes();
    }

    void deinit()
    {
        assert(p_renderer);
        p_renderer->pf_set_meta( p_renderer->p_opaque, NULL );
        p_renderer->pf_set_demux_enabled(p_renderer->p_opaque, false, NULL, NULL);
    }

    void resetTimes()
    {
        m_start_time = m_last_time = -1;
        m_start_pos = m_last_pos = -1.0f;
    }

    void initTimes()
    {
        if( demux_Control( p_demux->p_next, DEMUX_GET_TIME, &m_start_time ) != VLC_SUCCESS )
            m_start_time = -1;

        if( demux_Control( p_demux->p_next, DEMUX_GET_POSITION, &m_start_pos ) != VLC_SUCCESS )
            m_start_pos = -1.0f;

        m_last_time = m_start_time;
        m_last_pos = m_start_pos;
    }

    ~demux_cc()
    {
        if( p_renderer )
            deinit();
    }

    void resetDemuxEof()
    {
        m_demux_eof = false;
        p_renderer->pf_send_input_event( p_renderer->p_opaque, CC_INPUT_EVENT_EOF,
                                         cc_input_arg { false } );
    }

    void setPauseState(bool paused)
    {
        p_renderer->pf_set_pause_state( p_renderer->p_opaque, paused );
    }

    vlc_tick_t getCCTime()
    {
        return p_renderer->pf_get_time( p_renderer->p_opaque );
    }

    vlc_tick_t getTime()
    {
        if( m_start_time < 0 )
            return -1;

        vlc_tick_t time = m_start_time;
        vlc_tick_t cc_time = getCCTime();

        if( cc_time != VLC_TICK_INVALID )
            time += cc_time;
        m_last_time = time;
        return time;
    }

    double getPosition()
    {
        if( m_length > 0 && m_start_pos >= 0 )
        {
            m_last_pos = ( getCCTime() / double( m_length ) ) + m_start_pos;
            return m_last_pos;
        }
        else
            return -1;
    }

    void seekBack( vlc_tick_t time, double pos )
    {
        es_out_Control( p_demux->p_next->out, ES_OUT_RESET_PCR );

        if( m_can_seek )
        {
            int ret = VLC_EGENERIC;
            if( time >= 0 )
                ret = demux_Control( p_demux->p_next, DEMUX_SET_TIME, time, false );

            if( ret != VLC_SUCCESS && pos >= 0 )
                demux_Control( p_demux->p_next, DEMUX_SET_POSITION, pos, false );
        }
    }

    int Demux()
    {
        if ( !m_enabled )
            return demux_Demux( p_demux->p_next );

        /* The CC sout is not pacing, so we pace here */
        int pace = p_renderer->pf_pace( p_renderer->p_opaque );
        switch (pace)
        {
            case CC_PACE_ERR:
                return VLC_DEMUXER_EGENERIC;
            case CC_PACE_ERR_RETRY:
            {
                /* Seek back to started position */
                seekBack(m_start_time, m_start_pos);

                resetDemuxEof();
                p_renderer->pf_send_input_event( p_renderer->p_opaque,
                                                 CC_INPUT_EVENT_RETRY,
                                                 cc_input_arg{false} );
                break;
            }
            case CC_PACE_OK_WAIT:
                /* Yeld: return to let the input thread doing controls  */
                return VLC_DEMUXER_SUCCESS;
            case CC_PACE_OK:
            case CC_PACE_OK_ENDED:
                break;
            default:
                vlc_assert_unreachable();
        }

        int ret = VLC_DEMUXER_SUCCESS;
        if( !m_demux_eof )
        {
            ret = demux_Demux( p_demux->p_next );
            if( ret != VLC_DEMUXER_EGENERIC
             && ( m_start_time < 0 || m_start_pos < 0.0f ) )
                initTimes();
            if( ret == VLC_DEMUXER_EOF )
                m_demux_eof = true;
        }

        if( m_demux_eof )
        {
            /* Signal EOF to the sout when the es_out is empty (so when the
             * DecoderThread fifo are empty) */
            bool b_empty;
            es_out_Control( p_demux->p_next->out, ES_OUT_GET_EMPTY, &b_empty );
            if( b_empty )
                p_renderer->pf_send_input_event( p_renderer->p_opaque,
                                                 CC_INPUT_EVENT_EOF,
                                                 cc_input_arg{ true } );

            /* Don't return EOF until the chromecast is not EOF. This allows
             * this demux filter to have more controls over the sout. Indeed,
             * we still can seek or change tracks when the input is EOF and we
             * should continue to handle CC errors. */
            ret = pace == CC_PACE_OK ? VLC_DEMUXER_SUCCESS : VLC_DEMUXER_EOF;
        }

        return ret;
    }

    int Control( demux_t *p_demux_filter, int i_query, va_list args )
    {
        if( !m_enabled && i_query != DEMUX_FILTER_ENABLE )
            return demux_vaControl( p_demux_filter->p_next, i_query, args );

        switch (i_query)
        {
        case DEMUX_GET_POSITION:
        {
            double pos = getPosition();
            if( pos >= 0 )
            {
                *va_arg( args, double * ) = pos;
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;
        }
        case DEMUX_GET_TIME:
        {
            vlc_tick_t time = getTime();
            if( time >= 0 )
            {
                *va_arg(args, vlc_tick_t *) = time;
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;
        }
        case DEMUX_GET_LENGTH:
        {
            int ret;
            va_list ap;

            va_copy( ap, args );
            ret = demux_vaControl( p_demux_filter->p_next, i_query, args );
            if( ret == VLC_SUCCESS )
                m_length = *va_arg( ap, vlc_tick_t * );
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
                m_can_seek = *va_arg( ap, bool* );
            va_end( ap );
            return ret;
        }

        case DEMUX_SET_POSITION:
        {
            double pos = va_arg( args, double );
            /* Force unprecise seek */
            int ret = demux_Control( p_demux->p_next, DEMUX_SET_POSITION, pos, false );
            if( ret != VLC_SUCCESS )
                return ret;

            resetTimes();
            resetDemuxEof();
            return VLC_SUCCESS;
        }
        case DEMUX_SET_TIME:
        {
            vlc_tick_t time = va_arg( args, vlc_tick_t );
            /* Force unprecise seek */
            int ret = demux_Control( p_demux->p_next, DEMUX_SET_TIME, time, false );
            if( ret != VLC_SUCCESS )
                return ret;

            resetTimes();
            resetDemuxEof();
            return VLC_SUCCESS;
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
        case DEMUX_SET_ES:
        case DEMUX_SET_ES_LIST:
            /* Seek back to the last known pos when changing tracks. This will
             * flush sout streams, make sout del/add called right away and
             * clear CC buffers. */
            seekBack(m_last_time, m_last_pos);
            resetTimes();
            resetDemuxEof();
            break;
        case DEMUX_FILTER_ENABLE:
            p_renderer = static_cast<chromecast_common *>(
                        var_InheritAddress( p_demux, CC_SHARED_VAR_NAME ) );
            assert(p_renderer != NULL);
            m_enabled = true;
            init();
            return VLC_SUCCESS;

        case DEMUX_FILTER_DISABLE:

            deinit();

            /* Seek back to last known position. Indeed we don't want to resume
             * from the input position that can be more than 1 minutes forward
             * (depending on the CC buffering policy). */
            seekBack(m_last_time, m_last_pos);

            m_enabled = false;
            p_renderer = NULL;

            return VLC_SUCCESS;
        }

        return demux_vaControl( p_demux_filter->p_next, i_query, args );
    }

protected:
    demux_t     * const p_demux;
    chromecast_common  * p_renderer;
    vlc_tick_t    m_length;
    bool          m_can_seek;
    bool          m_enabled;
    bool          m_demux_eof;
    double        m_start_pos;
    double        m_last_pos;
    vlc_tick_t    m_start_time;
    vlc_tick_t    m_last_time;
};

static void on_paused_changed_cb( void *data, bool paused )
{
    demux_t *p_demux = reinterpret_cast<demux_t*>(data);
    vlc_object_t *obj = vlc_object_parent(p_demux->p_next);

    /* XXX: Ugly: Notify the parent of the input_thread_t that the corks state
     * changed */
    while( obj != NULL )
    {
        /* Try to find the playlist or the mediaplayer that handle the corks
         * state */
        if( var_Type( obj, "corks" ) != 0 )
        {
            ( paused ? var_IncInteger : var_DecInteger )( obj, "corks" );
            return;
        }
        obj = vlc_object_parent(obj);
    }
}

static int Demux( demux_t *p_demux_filter )
{
    demux_cc *p_sys = reinterpret_cast<demux_cc*>(p_demux_filter->p_sys);

    return p_sys->Demux();
}

static int Control( demux_t *p_demux_filter, int i_query, va_list args)
{
    demux_cc *p_sys = reinterpret_cast<demux_cc*>(p_demux_filter->p_sys);

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

    demux_cc *p_sys = new(std::nothrow) demux_cc( p_demux, p_renderer );
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
    demux_cc *p_sys = reinterpret_cast<demux_cc*>(p_demux->p_sys);

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

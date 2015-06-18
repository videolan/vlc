/*****************************************************************************
 * hls.cpp: HTTP Live Streaming module
 *****************************************************************************
 * Copyright © 2015 - VideoLAN and VLC Authors
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
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

#include <stdint.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_demux.h>
#include <vlc_meta.h>

#include <errno.h>

#include "../adaptative/logic/AbstractAdaptationLogic.h"
#include "HLSManager.hpp"

#include "playlist/Parser.hpp"
#include "playlist/M3U8.hpp"
#include "hls.hpp"

using namespace adaptative;
using namespace adaptative::logic;
using namespace adaptative::playlist;
using namespace hls;
using namespace hls::playlist;

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open    (vlc_object_t *);
static void Close   (vlc_object_t *);

#define HLS_WIDTH_TEXT N_("Preferred Width")
#define HLS_WIDTH_LONGTEXT N_("Preferred Width")

#define HLS_HEIGHT_TEXT N_("Preferred Height")
#define HLS_HEIGHT_LONGTEXT N_("Preferred Height")

#define HLS_BW_TEXT N_("Fixed Bandwidth in KiB/s")
#define HLS_BW_LONGTEXT N_("Preferred bandwidth for non adaptative streams")

#define HLS_LOGIC_TEXT N_("Adaptation Logic")

static const int pi_logics[] = {AbstractAdaptationLogic::RateBased,
                                AbstractAdaptationLogic::FixedRate,
                                AbstractAdaptationLogic::AlwaysLowest,
                                AbstractAdaptationLogic::AlwaysBest};

static const char *const ppsz_logics[] = { N_("Bandwidth Adaptive"),
                                           N_("Fixed Bandwidth"),
                                           N_("Lowest Bandwidth/Quality"),
                                           N_("Highest Bandwith/Quality")};

vlc_module_begin ()
        set_shortname( N_("hls"))
        set_description( N_("HTTP Live Streaming") )
        set_capability( "demux", 12 )
        set_category( CAT_INPUT )
        set_subcategory( SUBCAT_INPUT_DEMUX )
        add_integer( "hls-logic", AbstractAdaptationLogic::Default,
                                             HLS_LOGIC_TEXT, NULL, false )
            change_integer_list( pi_logics, ppsz_logics )
        add_integer( "hls-prefwidth",  480, HLS_WIDTH_TEXT,  HLS_WIDTH_LONGTEXT,  true )
        add_integer( "hls-prefheight", 360, HLS_HEIGHT_TEXT, HLS_HEIGHT_LONGTEXT, true )
        add_integer( "hls-prefbw",     250, HLS_BW_TEXT,     HLS_BW_LONGTEXT,     false )
        set_callbacks( Open, Close )
vlc_module_end ()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

static int  Demux( demux_t * );
static int  Control         (demux_t *p_demux, int i_query, va_list args);

/*****************************************************************************
 * Open:
 *****************************************************************************/
static bool isHTTPLiveStreaming(stream_t *s)
{
    const uint8_t *peek;

    int size = stream_Peek(s, &peek, 46);
    if (size < 7)
        return false;

    if (memcmp(peek, "#EXTM3U", 7) != 0)
        return false;

    peek += 7;
    size -= 7;

    /* Parse stream and search for
     * EXT-X-TARGETDURATION or EXT-X-STREAM-INF tag, see
     * http://tools.ietf.org/html/draft-pantos-http-live-streaming-04#page-8 */
    while (size--)
    {
        static const char *const ext[] = {
            "TARGETDURATION",
            "MEDIA-SEQUENCE",
            "KEY",
            "ALLOW-CACHE",
            "ENDLIST",
            "STREAM-INF",
            "DISCONTINUITY",
            "VERSION"
        };

        if (*peek++ != '#')
            continue;

        if (size < 6)
            continue;

        if (memcmp(peek, "EXT-X-", 6))
            continue;

        peek += 6;
        size -= 6;

        for (size_t i = 0; i < ARRAY_SIZE(ext); i++)
        {
            size_t len = strlen(ext[i]);
            if (size < 0 || (size_t)size < len)
                continue;
            if (!memcmp(peek, ext[i], len))
                return true;
        }
    }

    return false;
}

static int Open(vlc_object_t *p_obj)
{
    demux_t *p_demux = (demux_t*) p_obj;

    if(!isHTTPLiveStreaming(p_demux->s))
        return VLC_EGENERIC;

    demux_sys_t *p_sys = (demux_sys_t *) malloc(sizeof(demux_sys_t));
    if (unlikely(p_sys == NULL))
        return VLC_ENOMEM;

    Parser parser(p_demux->s);
    p_sys->p_playlist = parser.parse(std::string());
    if(!p_sys->p_playlist)
    {
        free(p_sys);
        return VLC_EGENERIC;
    }

    int logic = var_InheritInteger(p_obj, "hls-logic");

    HLSManager *p_manager =
            new (std::nothrow) HLSManager(p_sys->p_playlist,
            static_cast<AbstractAdaptationLogic::LogicType>(logic),
            p_demux->s);

    BasePeriod *period = p_sys->p_playlist->getFirstPeriod();
    if(period && !p_manager->start(p_demux))
    {
        delete p_manager;
        free( p_sys );
        return VLC_EGENERIC;
    }
    p_sys->p_manager       = p_manager;
    p_demux->p_sys         = p_sys;
    p_demux->pf_demux      = Demux;
    p_demux->pf_control    = Control;

    p_sys->i_nzpcr = VLC_TS_INVALID;

    msg_Dbg(p_obj,"opening mpd file (%s)", p_demux->s->psz_path);

    return VLC_SUCCESS;
}
/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close(vlc_object_t *p_obj)
{
    demux_t                            *p_demux       = (demux_t*) p_obj;
    demux_sys_t                        *p_sys          = (demux_sys_t *) p_demux->p_sys;

    delete p_sys->p_manager;
    free(p_sys);
}
/*****************************************************************************
 * Callbacks:
 *****************************************************************************/
#define DEMUX_INCREMENT (CLOCK_FREQ / 20)
static int Demux(demux_t *p_demux)
{
    demux_sys_t *p_sys = p_demux->p_sys;

    if(p_sys->i_nzpcr == VLC_TS_INVALID)
    {
        if( Stream::status_eof ==
            p_sys->p_manager->demux(p_sys->i_nzpcr + DEMUX_INCREMENT, false) )
        {
            return VLC_DEMUXER_EOF;
        }
        mtime_t i_dts = p_sys->p_manager->getFirstDTS();
        p_sys->i_nzpcr = i_dts;
    }

    Stream::status status =
            p_sys->p_manager->demux(p_sys->i_nzpcr + DEMUX_INCREMENT, true);
    switch(status)
    {
    case Stream::status_eof:
        return VLC_DEMUXER_EOF;
    case Stream::status_buffering:
        break;
    case Stream::status_demuxed:
        if( p_sys->i_nzpcr != VLC_TS_INVALID )
        {
            p_sys->i_nzpcr += DEMUX_INCREMENT;
            int group = p_sys->p_manager->getGroup();
            es_out_Control(p_demux->out, ES_OUT_SET_GROUP_PCR, group, VLC_TS_0 + p_sys->i_nzpcr);
        }
        break;
    }

    if( !p_sys->p_manager->updatePlaylist() )
        msg_Warn(p_demux, "Can't update playlist");

    return VLC_DEMUXER_SUCCESS;
}

static int  Control         (demux_t *p_demux, int i_query, va_list args)
{
    demux_sys_t *p_sys = p_demux->p_sys;

    switch (i_query)
    {
        case DEMUX_CAN_SEEK:
            *(va_arg (args, bool *)) = p_sys->p_manager->seekAble();
            break;

        case DEMUX_CAN_CONTROL_PACE:
            *(va_arg (args, bool *)) = true;
            break;

        case DEMUX_CAN_PAUSE:
            *(va_arg (args, bool *)) = p_sys->p_playlist->isLive();
            break;

        case DEMUX_GET_TIME:
            *(va_arg (args, int64_t *)) = p_sys->i_nzpcr;
            break;

        case DEMUX_GET_LENGTH:
            *(va_arg (args, int64_t *)) = p_sys->p_manager->getDuration();
            break;

        case DEMUX_GET_POSITION:
            if(!p_sys->p_manager->getDuration())
                return VLC_EGENERIC;

            *(va_arg (args, double *)) = (double) p_sys->i_nzpcr
                                         / p_sys->p_manager->getDuration();
            break;

        case DEMUX_SET_POSITION:
        {
            int64_t time = p_sys->p_manager->getDuration() * va_arg(args, double);
            if(p_sys->p_playlist->isLive() ||
               !p_sys->p_manager->getDuration() ||
               !p_sys->p_manager->setPosition(time))
                return VLC_EGENERIC;
            p_sys->i_nzpcr = VLC_TS_INVALID;
            break;
        }

        case DEMUX_SET_TIME:
        {
            int64_t time = va_arg(args, int64_t);
            if(p_sys->p_playlist->isLive() ||
               !p_sys->p_manager->setPosition(time))
                return VLC_EGENERIC;
            p_sys->i_nzpcr = VLC_TS_INVALID;
            break;
        }

        case DEMUX_GET_PTS_DELAY:
            *va_arg (args, int64_t *) = INT64_C(1000) *
                var_InheritInteger(p_demux, "network-caching");
             break;

        default:
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

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

#include "../adaptative/logic/AbstractAdaptationLogic.h"
#include "HLSManager.hpp"
#include "HLSStreams.hpp"

#include "playlist/Parser.hpp"
#include "playlist/M3U8.hpp"

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

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open(vlc_object_t *p_obj)
{
    demux_t *p_demux = (demux_t*) p_obj;

    if(!HLSManager::isHTTPLiveStreaming(p_demux->s))
        return VLC_EGENERIC;

    Parser parser(p_demux->s);
    M3U8 *p_playlist = parser.parse(std::string());
    if(!p_playlist)
        return VLC_EGENERIC;

    int logic = var_InheritInteger(p_obj, "hls-logic");

    HLSManager *p_manager =
            new (std::nothrow) HLSManager(p_demux, p_playlist,
            new (std::nothrow) HLSStreamOutputFactory,
            static_cast<AbstractAdaptationLogic::LogicType>(logic));

    BasePeriod *period = p_playlist->getFirstPeriod();
    if(period && !p_manager->start())
    {
        delete p_manager;
        return VLC_EGENERIC;
    }

    p_demux->p_sys         = reinterpret_cast<demux_sys_t *>(p_manager);
    p_demux->pf_demux      = p_manager->demux_callback;
    p_demux->pf_control    = p_manager->control_callback;

    msg_Dbg(p_obj,"opening mpd file (%s)", p_demux->s->psz_path);

    return VLC_SUCCESS;
}
/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close(vlc_object_t *p_obj)
{
    demux_t         *p_demux       = (demux_t*) p_obj;
    PlaylistManager *p_manager  = reinterpret_cast<PlaylistManager *>(p_demux->p_sys);

    delete p_manager;
}

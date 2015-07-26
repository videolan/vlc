/*****************************************************************************
 * adaptative.cpp: Adaptative streaming module
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

#include "playlist/BasePeriod.h"

#include "../dash/xml/DOMParser.h"
#include "../dash/mpd/MPDFactory.h"
#include "../dash/DASHManager.h"

#include "../hls/HLSManager.hpp"
#include "../hls/HLSStreams.hpp"
#include "../hls/playlist/Parser.hpp"
#include "../hls/playlist/M3U8.hpp"


using namespace adaptative::logic;
using namespace adaptative::playlist;
using namespace dash::mpd;
using namespace dash::xml;
using namespace dash;
using namespace hls;
using namespace hls::playlist;

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open    (vlc_object_t *);
static void Close   (vlc_object_t *);

#define ADAPT_WIDTH_TEXT N_("Preferred Width")

#define ADAPT_HEIGHT_TEXT N_("Preferred Height")

#define ADAPT_BW_TEXT N_("Fixed Bandwidth in KiB/s")
#define ADAPT_BW_LONGTEXT N_("Preferred bandwidth for non adaptative streams")

#define ADAPT_LOGIC_TEXT N_("Adaptation Logic")

static const int pi_logics[] = {AbstractAdaptationLogic::RateBased,
                                AbstractAdaptationLogic::FixedRate,
                                AbstractAdaptationLogic::AlwaysLowest,
                                AbstractAdaptationLogic::AlwaysBest};

static const char *const ppsz_logics[] = { N_("Bandwidth Adaptive"),
                                           N_("Fixed Bandwidth"),
                                           N_("Lowest Bandwidth/Quality"),
                                           N_("Highest Bandwith/Quality")};

vlc_module_begin ()
        set_shortname( N_("Adaptative"))
        set_description( N_("Unified adaptative streaming for DASH/HLS") )
        set_capability( "demux", 12 )
        set_category( CAT_INPUT )
        set_subcategory( SUBCAT_INPUT_DEMUX )
        add_integer( "adaptative-logic",  AbstractAdaptationLogic::Default,
                                          ADAPT_LOGIC_TEXT, NULL, false )
            change_integer_list( pi_logics, ppsz_logics )
        add_integer( "adaptative-width",  480, ADAPT_WIDTH_TEXT,  ADAPT_WIDTH_TEXT,  true )
        add_integer( "adaptative-height", 360, ADAPT_HEIGHT_TEXT, ADAPT_HEIGHT_TEXT, true )
        add_integer( "adaptative-bw",     250, ADAPT_BW_TEXT,     ADAPT_BW_LONGTEXT,     false )
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

    bool b_mimematched = false;
    char *psz_mime = stream_ContentType(p_demux->s);
    if(psz_mime)
    {
        b_mimematched = !strcmp(psz_mime, "application/dash+xml");
        free(psz_mime);
    }

    PlaylistManager *p_manager = NULL;
    int logic = var_InheritInteger(p_obj, "adaptative-logic");

    std::string playlisturl(p_demux->psz_access);
    playlisturl.append("://");
    playlisturl.append(p_demux->psz_location);

    if(b_mimematched || DASHManager::isDASH(p_demux->s))
    {
        //Build a XML tree
        DOMParser parser(p_demux->s);
        if( !parser.parse() )
        {
            msg_Err( p_demux, "Could not parse MPD" );
            return VLC_EGENERIC;
        }

        //Begin the actual MPD parsing:
        MPD *p_playlist = MPDFactory::create(parser.getRootNode(), p_demux->s,
                                             playlisturl, parser.getProfile());
        if(p_playlist == NULL)
        {
            msg_Err( p_demux, "Cannot create/unknown MPD for profile");
            return VLC_EGENERIC;
        }

        p_manager = new DASHManager( p_demux, p_playlist,
                                     new (std::nothrow) DASHStreamOutputFactory,
                                     static_cast<AbstractAdaptationLogic::LogicType>(logic) );
    }
    else if(HLSManager::isHTTPLiveStreaming(p_demux->s))
    {
        Parser parser(p_demux->s);
        M3U8 *p_playlist = parser.parse(playlisturl);
        if(!p_playlist)
        {
            msg_Err( p_demux, "Could not parse MPD" );
            return VLC_EGENERIC;
        }

        p_manager =
                new (std::nothrow) HLSManager(p_demux, p_playlist,
                                              new (std::nothrow) HLSStreamOutputFactory,
                                              static_cast<AbstractAdaptationLogic::LogicType>(logic));
    }

    if(!p_manager || !p_manager->start())
    {
        delete p_manager;
        return VLC_EGENERIC;
    }

    p_demux->p_sys         = reinterpret_cast<demux_sys_t *>(p_manager);
    p_demux->pf_demux      = p_manager->demux_callback;
    p_demux->pf_control    = p_manager->control_callback;

    msg_Dbg(p_obj,"opening playlist file (%s)", p_demux->psz_location);

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

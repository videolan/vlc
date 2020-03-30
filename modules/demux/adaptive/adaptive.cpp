/*****************************************************************************
 * adaptive.cpp: Adaptive streaming module
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

#include "SharedResources.hpp"
#include "playlist/BasePeriod.h"
#include "logic/BufferingLogic.hpp"
#include "xml/DOMParser.h"

#include "../dash/DASHManager.h"
#include "../dash/DASHStream.hpp"
#include "../dash/mpd/IsoffMainParser.h"

#include "../hls/HLSManager.hpp"
#include "../hls/HLSStreams.hpp"
#include "../hls/playlist/Parser.hpp"
#include "../hls/playlist/M3U8.hpp"
#include "../smooth/SmoothManager.hpp"
#include "../smooth/SmoothStream.hpp"
#include "../smooth/playlist/Parser.hpp"

using namespace adaptive::http;
using namespace adaptive::logic;
using namespace adaptive::playlist;
using namespace adaptive::xml;
using namespace dash::mpd;
using namespace dash;
using namespace hls;
using namespace hls::playlist;
using namespace smooth;
using namespace smooth::playlist;

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open    (vlc_object_t *);
static void Close   (vlc_object_t *);

#define ADAPT_WIDTH_TEXT N_("Maximum device width")
#define ADAPT_HEIGHT_TEXT N_("Maximum device height")

#define ADAPT_BW_TEXT N_("Fixed Bandwidth in KiB/s")
#define ADAPT_BW_LONGTEXT N_("Preferred bandwidth for non adaptive streams")

#define ADAPT_BUFFER_TEXT N_("Live Playback delay (ms)")
#define ADAPT_BUFFER_LONGTEXT N_("Tradeoff between stability and real time")

#define ADAPT_MAXBUFFER_TEXT N_("Max buffering (ms)")

#define ADAPT_LOGIC_TEXT N_("Adaptive Logic")

#define ADAPT_ACCESS_TEXT N_("Use regular HTTP modules")
#define ADAPT_ACCESS_LONGTEXT N_("Connect using HTTP access instead of custom HTTP code")

#define ADAPT_LOWLATENCY_TEXT N_("Low latency")
#define ADAPT_LOWLATENCY_LONGTEXT N_("Overrides low latency parameters")

static const AbstractAdaptationLogic::LogicType pi_logics[] = {
                                AbstractAdaptationLogic::Default,
                                AbstractAdaptationLogic::Predictive,
                                AbstractAdaptationLogic::NearOptimal,
                                AbstractAdaptationLogic::RateBased,
                                AbstractAdaptationLogic::FixedRate,
                                AbstractAdaptationLogic::AlwaysLowest,
                                AbstractAdaptationLogic::AlwaysBest};

static const char *const ppsz_logics_values[] = {
                                "",
                                "predictive",
                                "nearoptimal",
                                "rate",
                                "fixedrate",
                                "lowest",
                                "highest"};

static const char *const ppsz_logics[] = { N_("Default"),
                                           N_("Predictive"),
                                           N_("Near Optimal"),
                                           N_("Bandwidth Adaptive"),
                                           N_("Fixed Bandwidth"),
                                           N_("Lowest Bandwidth/Quality"),
                                           N_("Highest Bandwidth/Quality")};

static_assert( ARRAY_SIZE( pi_logics ) == ARRAY_SIZE( ppsz_logics ),
    "pi_logics and ppsz_logics shall have the same number of elements" );

static_assert( ARRAY_SIZE( pi_logics ) == ARRAY_SIZE( ppsz_logics_values ),
    "pi_logics and ppsz_logics_values shall have the same number of elements" );

static const int rgi_latency[] = { -1, 1, 0 };

static const char *const ppsz_latency[] = { N_("Auto"),
                                            N_("Force"),
                                            N_("Disable") };

vlc_module_begin ()
        set_shortname( N_("Adaptive"))
        set_description( N_("Unified adaptive streaming for DASH/HLS") )
        set_capability( "demux", 12 )
        set_category( CAT_INPUT )
        set_subcategory( SUBCAT_INPUT_DEMUX )
        add_string( "adaptive-logic",  "", ADAPT_LOGIC_TEXT, NULL, false )
            change_string_list( ppsz_logics_values, ppsz_logics )
        add_integer( "adaptive-maxwidth",  0,
                     ADAPT_WIDTH_TEXT,  ADAPT_WIDTH_TEXT,  false )
        add_integer( "adaptive-maxheight", 0,
                     ADAPT_HEIGHT_TEXT, ADAPT_HEIGHT_TEXT, false )
        add_integer( "adaptive-bw",     250, ADAPT_BW_TEXT,     ADAPT_BW_LONGTEXT,     false )
        add_bool   ( "adaptive-use-access", false, ADAPT_ACCESS_TEXT, ADAPT_ACCESS_LONGTEXT, true );
        add_integer( "adaptive-livedelay",
                     MS_FROM_VLC_TICK(AbstractBufferingLogic::DEFAULT_LIVE_BUFFERING),
                     ADAPT_BUFFER_TEXT, ADAPT_BUFFER_LONGTEXT, true );
        add_integer( "adaptive-maxbuffer",
                     MS_FROM_VLC_TICK(AbstractBufferingLogic::DEFAULT_MAX_BUFFERING),
                     ADAPT_MAXBUFFER_TEXT, NULL, true );
        add_integer( "adaptive-lowlatency", -1, ADAPT_LOWLATENCY_TEXT, ADAPT_LOWLATENCY_LONGTEXT, true );
            change_integer_list(rgi_latency, ppsz_latency)
        set_callbacks( Open, Close )
vlc_module_end ()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static PlaylistManager * HandleDash(demux_t *, DOMParser &,
                                    const std::string &, AbstractAdaptationLogic::LogicType);
static PlaylistManager * HandleSmooth(demux_t *, DOMParser &,
                                      const std::string &, AbstractAdaptationLogic::LogicType);
static PlaylistManager * HandleHLS(demux_t *,
                                   const std::string &, AbstractAdaptationLogic::LogicType);

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open(vlc_object_t *p_obj)
{
    demux_t *p_demux = (demux_t*) p_obj;

    if(!p_demux->s->psz_url || p_demux->s->b_preparsing)
        return VLC_EGENERIC;

    std::string mimeType;

    char *psz_mime = stream_ContentType(p_demux->s);
    if(psz_mime)
    {
        mimeType = std::string(psz_mime);
        free(psz_mime);
    }

    PlaylistManager *p_manager = NULL;

    char *psz_logic = var_InheritString(p_obj, "adaptive-logic");
    AbstractAdaptationLogic::LogicType logic = AbstractAdaptationLogic::Default;
    if( psz_logic )
    {
        bool b_found = false;
        for(size_t i=0;i<ARRAY_SIZE(pi_logics); i++)
        {
            if(!strcmp(psz_logic, ppsz_logics_values[i]))
            {
                logic = pi_logics[i];
                b_found = true;
                break;
            }
        }
        if(!b_found)
            msg_Err(p_demux, "Unknown adaptive-logic value '%s'", psz_logic);
        free( psz_logic );
    }

    std::string playlisturl(p_demux->s->psz_url);

    bool dashmime = DASHManager::mimeMatched(mimeType);
    bool smoothmime = SmoothManager::mimeMatched(mimeType);

    if(!dashmime && !smoothmime && HLSManager::isHTTPLiveStreaming(p_demux->s))
    {
        p_manager = HandleHLS(p_demux, playlisturl, logic);
    }
    else
    {
        /* Handle XML Based ones */
        DOMParser xmlParser; /* Share that xml reader */
        if(dashmime)
        {
            p_manager = HandleDash(p_demux, xmlParser, playlisturl, logic);
        }
        else if(smoothmime)
        {
            p_manager = HandleSmooth(p_demux, xmlParser, playlisturl, logic);
        }
        else
        {
            /* We need to probe content */
            const uint8_t *p_peek;
            const ssize_t i_peek = vlc_stream_Peek(p_demux->s, &p_peek, 2048);
            if(i_peek > 0)
            {
                stream_t *peekstream = vlc_stream_MemoryNew(p_demux, const_cast<uint8_t *>(p_peek), (size_t)i_peek, true);
                if(peekstream)
                {
                    if(xmlParser.reset(peekstream) && xmlParser.parse(false))
                    {
                        if(DASHManager::isDASH(xmlParser.getRootNode()))
                        {
                            p_manager = HandleDash(p_demux, xmlParser, playlisturl, logic);
                        }
                        else if(SmoothManager::isSmoothStreaming(xmlParser.getRootNode()))
                        {
                            p_manager = HandleSmooth(p_demux, xmlParser, playlisturl, logic);
                        }
                    }
                    vlc_stream_Delete(peekstream);
                }
            }
        }
    }

    if(!p_manager || !p_manager->init())
    {
        delete p_manager;
        return VLC_EGENERIC;
    }

    /* disable annoying stuff */
    if(VLC_SUCCESS == var_Create( p_demux, "lua", VLC_VAR_BOOL))
        var_SetBool(p_demux, "lua", false);

    p_demux->p_sys         = p_manager;
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

    p_manager->stop();
    delete p_manager;
}

/*****************************************************************************
 *
 *****************************************************************************/
static bool IsLocalResource(const std::string & url)
{
    ConnectionParams params(url);
    return params.isLocal();
}


static PlaylistManager * HandleDash(demux_t *p_demux, DOMParser &xmlParser,
                                    const std::string & playlisturl,
                                    AbstractAdaptationLogic::LogicType logic)
{
    if(!xmlParser.reset(p_demux->s) || !xmlParser.parse(true))
    {
        msg_Err(p_demux, "Cannot parse MPD");
        return NULL;
    }
    IsoffMainParser mpdparser(xmlParser.getRootNode(), VLC_OBJECT(p_demux),
                              p_demux->s, playlisturl);
    MPD *p_playlist = mpdparser.parse();
    if(p_playlist == NULL)
    {
        msg_Err( p_demux, "Cannot create/unknown MPD for profile");
        return NULL;
    }

    SharedResources *resources = new (std::nothrow) SharedResources(VLC_OBJECT(p_demux),
                                                                    IsLocalResource(playlisturl));
    DASHStreamFactory *factory = new (std::nothrow) DASHStreamFactory;
    DASHManager *manager = NULL;
    if(!resources || !factory ||
       !(manager = new (std::nothrow) DASHManager(p_demux, resources,
                                                  p_playlist, factory, logic)))
    {
        delete resources;
        delete factory;
        delete p_playlist;
    }
    return manager;
}

static PlaylistManager * HandleSmooth(demux_t *p_demux, DOMParser &xmlParser,
                                    const std::string & playlisturl,
                                    AbstractAdaptationLogic::LogicType logic)
{
    if(!xmlParser.reset(p_demux->s) || !xmlParser.parse(true))
    {
        msg_Err(p_demux, "Cannot parse Manifest");
        return NULL;
    }
    ManifestParser mparser(xmlParser.getRootNode(), VLC_OBJECT(p_demux),
                           p_demux->s, playlisturl);
    Manifest *p_playlist = mparser.parse();
    if(p_playlist == NULL)
    {
        msg_Err( p_demux, "Cannot create Manifest");
        return NULL;
    }

    SharedResources *resources = new (std::nothrow) SharedResources(VLC_OBJECT(p_demux),
                                                                    IsLocalResource(playlisturl));
    SmoothStreamFactory *factory = new (std::nothrow) SmoothStreamFactory;
    SmoothManager *manager = NULL;
    if(!resources || !factory ||
       !(manager = new (std::nothrow) SmoothManager(p_demux, resources,
                                                    p_playlist, factory, logic)))
    {
        delete resources;
        delete factory;
        delete p_playlist;
    }
    return manager;
}

static PlaylistManager * HandleHLS(demux_t *p_demux,
                                   const std::string & playlisturl,
                                   AbstractAdaptationLogic::LogicType logic)
{
    SharedResources *resources = new SharedResources(VLC_OBJECT(p_demux),
                                                     IsLocalResource(playlisturl));
    if(!resources)
        return NULL;

    M3U8Parser parser(resources);
    M3U8 *p_playlist = parser.parse(VLC_OBJECT(p_demux),p_demux->s, playlisturl);
    if(!p_playlist)
    {
        msg_Err( p_demux, "Could not parse playlist" );
        delete resources;
        return NULL;
    }

    HLSStreamFactory *factory = new (std::nothrow) HLSStreamFactory;
    HLSManager *manager = NULL;
    if(!factory ||
       !(manager = new (std::nothrow) HLSManager(p_demux, resources,
                                                 p_playlist, factory, logic)))
    {
        delete p_playlist;
        delete factory;
        delete resources;
    }
    return manager;
}

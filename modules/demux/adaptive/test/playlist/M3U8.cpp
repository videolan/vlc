/*****************************************************************************
 *
 *****************************************************************************
 * Copyright (C) 2020 VideoLabs, VideoLAN and VLC Authors
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "../../playlist/Segment.h"
#include "../../playlist/SegmentList.h"
#include "../../playlist/BasePeriod.h"
#include "../../playlist/BaseAdaptationSet.h"
#include "../../playlist/BaseRepresentation.h"
#include "../../logic/BufferingLogic.hpp"
#include "../../SegmentTracker.hpp"
#include "../../../hls/playlist/Parser.hpp"
#include "../../../hls/playlist/M3U8.hpp"
#include "../../../hls/playlist/HLSSegment.hpp"
#include "../../../hls/playlist/HLSRepresentation.hpp"

#include "../test.hpp"

#include <vlc_stream.h>

#include <limits>
#include <algorithm>

using namespace adaptive;
using namespace adaptive::playlist;
using namespace hls::playlist;

static M3U8 * ParseM3U8(vlc_object_t *obj, const char *psz, size_t isz)
{
    M3U8Parser parser(nullptr);
    stream_t *substream = vlc_stream_MemoryNew(obj, ((uint8_t *)psz), isz, true);
    if(!substream)
        return nullptr;
    M3U8 *m3u = parser.parse(obj, substream, std::string("stdin://"));
    vlc_stream_Delete(substream);
    return m3u;
}

int M3U8MasterPlaylist_test()
{
    vlc_object_t *obj = static_cast<vlc_object_t*>(nullptr);

    const char manifest0[] =
    "#EXTM3U\n"
    "#EXT-X-STREAM-INF:BANDWIDTH=1280000\n"
    "http://example.com/low.m3u8\n"
    "#EXT-X-STREAM-INF:BANDWIDTH=2560000\n"
    "http://example.com/mid.m3u8\n"
    "#EXT-X-STREAM-INF:BANDWIDTH=7680000\n"
    "http://example.com/hi.m3u8\n"
    "#EXT-X-STREAM-INF:BANDWIDTH=65000,CODECS=\"mp4a.40.5\"\n"
    "http://example.com/audio-only.m3u8\n";

    M3U8 *m3u = ParseM3U8(obj, manifest0, sizeof(manifest0));
    try
    {
        Expect(m3u);
        Expect(m3u->getUrlSegment().toString() == "stdin://");
        Expect(m3u->getPeriods().size() == 1);
        Expect(m3u->getFirstPeriod()->getAdaptationSets().size() == 1);
        Expect(m3u->getFirstPeriod()->getAdaptationSets().front()->getRepresentations().size() == 4);
        BaseAdaptationSet *set = m3u->getFirstPeriod()->getAdaptationSets().front();
        const std::vector<BaseRepresentation *> &reps = set->getRepresentations();
        auto it = std::find_if(reps.cbegin(), reps.cend(),
                          [](BaseRepresentation *r) { return r->getBandwidth() == 1280000; });
        Expect(it != reps.end());
        Expect(static_cast<HLSRepresentation *>(*it)->getPlaylistUrl().toString() == "http://example.com/low.m3u8");
        it = std::find_if(reps.cbegin(), reps.cend(),
                      [](BaseRepresentation *r) { return r->getBandwidth() == 65000; });
        Expect(it != reps.end());
        Expect(static_cast<HLSRepresentation *>(*it)->getPlaylistUrl().toString() == "http://example.com/audio-only.m3u8");
        Expect((*it)->getUrlSegment().toString() == "http://example.com/");
        const std::list<std::string> &codecs = (*it)->getCodecs();
        Expect(!codecs.empty());
        Expect(codecs.front() == "mp4a.40.5");
        Expect((*it)->needsUpdate(1));
        delete m3u;
    }
    catch(...)
    {
        delete m3u;
        return 1;
    }

    const char manifest1[] =
    "#EXTM3U\n"
    "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"aac\",NAME=\"English\", "
    " DEFAULT=YES,AUTOSELECT=YES,LANGUAGE=\"en\", "
    " URI=\"main/english-audio.m3u8\"\n"
    "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"aac\",NAME=\"Deutsch\", "
    " DEFAULT=NO,AUTOSELECT=YES,LANGUAGE=\"de\", "
    " URI=\"main/german-audio.m3u8\"\n"
    "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"aac\",NAME=\"Commentary\", "
    " DEFAULT=NO,AUTOSELECT=NO,LANGUAGE=\"en\", "
    " URI=\"commentary/audio-only.m3u8\"\n"
    "#EXT-X-STREAM-INF:BANDWIDTH=7680000,CODECS=\"avc1, mp4a.40.5\",AUDIO=\"aac\"\n"
    "hi/video-only.m3u8\n"
    "#EXT-X-STREAM-INF:BANDWIDTH=65000,CODECS=\"mp4a.40.5\",AUDIO=\"aac\"\n"
    "main/english-audio.m3u8\n";

    m3u = ParseM3U8(obj, manifest1, sizeof(manifest1));
    try
    {
        Expect(m3u);
        Expect(m3u->getPeriods().size() == 1);
        Expect(m3u->getFirstPeriod()->getAdaptationSets().size() == 4);
        BaseAdaptationSet *set = m3u->getFirstPeriod()->getAdaptationSetByID(ID("aac English"));
        Expect(set);
        Expect(set->getRepresentations().size() == 1);
        Expect(set->getLang() == "en");
        Expect(set->getRole().autoSelectable());
        Expect(set->getRole().isDefault());
        delete m3u;
    }
    catch(...)
    {
        delete m3u;
        return 1;
    }

    return 0;
}

int M3U8Playlist_test()
{
    vlc_object_t *obj = static_cast<vlc_object_t*>(nullptr);
    DefaultBufferingLogic bufferingLogic;

    /* Manifest 0 */
    const char manifest0[] =
    "#EXTM3U\n"
    "#EXT-X-MEDIA-SEQUENCE:10\n"
    "#EXTINF:8,\n"
    "foobar.ts\n"
    "#EXT-X-ENDLIST\n";

    M3U8 *m3u = ParseM3U8(obj, manifest0, sizeof(manifest0));
    try
    {
        Expect(m3u);
        Expect(m3u->isLive() == false);
        Expect(m3u->isLowLatency() == false);
        Expect(m3u->getPeriods().size() == 1);
        Expect(m3u->getFirstPeriod()->getAdaptationSets().size() == 1);
        Expect(m3u->getFirstPeriod()->getAdaptationSets().front()->getRepresentations().size() == 1);
        BaseRepresentation *rep = m3u->getFirstPeriod()->getAdaptationSets().front()->getRepresentations().front();
        Expect(rep->getProfile()->getStartSegmentNumber() == 10);

        uint64_t number = bufferingLogic.getStartSegmentNumber(rep);
        Expect(number == 10);
        vlc_tick_t mediatime, duration;
        Expect(rep->getPlaybackTimeDurationBySegmentNumber(std::numeric_limits<uint64_t>::max(),
                                                           &mediatime, &duration) == false);
        Expect(rep->getPlaybackTimeDurationBySegmentNumber(number + 1, &mediatime, &duration) == false);
        Expect(rep->getPlaybackTimeDurationBySegmentNumber(number - 1, &mediatime, &duration) == false);
        Expect(rep->getPlaybackTimeDurationBySegmentNumber(number, &mediatime, &duration) == true);
        Expect(mediatime == vlc_tick_from_sec(0));
        Expect(duration == vlc_tick_from_sec(8));
        Expect(rep->getSegmentNumberByTime(vlc_tick_from_sec(0), &number));
        Expect(number == 10);
        Segment *seg = rep->getMediaSegment(number);
        Expect(seg);
        Expect(seg->getSequenceNumber() == 10);
        Expect(seg->startTime.Get() == (stime_t) 0);

        vlc_tick_t begin, end;
        Expect(rep->getMediaPlaybackRange(&begin, &end, &duration));
        Expect(begin == vlc_tick_from_sec(0));
        Expect(end == vlc_tick_from_sec(8));
        Expect(duration == vlc_tick_from_sec(8));
        delete m3u;
    }
    catch(...)
    {
        delete m3u;
        return 1;
    }

    /* Manifest 1 */
    const char manifest1[] =
    "#EXTM3U\n"
    "#EXT-X-MEDIA-SEQUENCE:10\n"
    "#EXTINF:8,\n"
    "foobar.ts\n"
    "#EXTINF:12.0,\n" /* Broken format test: non integral notation */
    "foobar2.ts\n"
    "#EXTINF:10\n" /* Broken format test: no mandatory comma */
    "foobar3.ts\n"
    "#EXT-X-ENDLIST\n";

    m3u = ParseM3U8(obj, manifest1, sizeof(manifest1));
    try
    {
        Expect(m3u);
        BaseRepresentation *rep = m3u->getFirstPeriod()->getAdaptationSets().front()->
                                  getRepresentations().front();
        Expect(rep->getProfile()->getStartSegmentNumber() == 10);
        Expect(m3u->duration.Get());

        Timescale timescale = rep->inheritTimescale();
        Expect(timescale.isValid());

        uint64_t number;
        bool discont;
        Segment *seg = rep->getNextMediaSegment(12, &number, &discont);
        Expect(seg);
        Expect(number == 12);
        Expect(!discont);
        Expect(seg->getSequenceNumber() == 12);
        Expect(seg->startTime.Get() == timescale.ToScaled(vlc_tick_from_sec(20)));

        vlc_tick_t begin, end, duration;
        Expect(rep->getMediaPlaybackRange(&begin, &end, &duration));
        Expect(begin == vlc_tick_from_sec(0));
        Expect(end == vlc_tick_from_sec(30));
        Expect(duration == vlc_tick_from_sec(30));

        delete m3u;
    }
    catch (...)
    {
        delete m3u;
        return 1;
    }

    /* Manifest 2 */
    const char manifest2[] =
    "#EXTM3U\n"
    "#EXT-X-MEDIA-SEQUENCE:10\n"
    "#EXTINF:8\n"
    "foobar.ts\n"
    "#EXTINF:5\n"
    "foobar2.ts\n"
    "#EXTINF:8\n"
    "foobar3.ts\n"
    "#EXTINF:5\n"
    "foobar4.ts\n"
    "#EXTINF:8\n"
    "foobar5.ts\n";

    m3u = ParseM3U8(obj, manifest2, sizeof(manifest2));
    try
    {
        Expect(m3u);
        Expect(m3u->isLive() == true);
        BaseRepresentation *rep = m3u->getFirstPeriod()->getAdaptationSets().front()->
                                  getRepresentations().front();
        uint64_t number;
        Expect(rep->getSegmentNumberByTime(vlc_tick_from_sec(2), &number));
        Expect(number == 10);
        Expect(rep->getSegmentNumberByTime(vlc_tick_from_sec(8), &number));
        Expect(number == 11);
        Expect(rep->getSegmentNumberByTime(vlc_tick_from_sec(100), &number));
        Expect(number == 14);
        Expect(bufferingLogic.getStartSegmentNumber(rep) < 13);

        delete m3u;
    }
    catch (...)
    {
        delete m3u;
        return 1;
    }

    /* Manifest 3 */
    const char manifest3[] =
    "#EXTM3U\n"
    "#EXT-X-MEDIA-SEQUENCE:10\n"
    "#EXT-X-PROGRAM-DATE-TIME:1970-01-01T00:00:10.000+00:00\n"
    "#EXTINF:8\n"
    "foobar.ts\n"
    "#EXTINF:5\n"
    "foobar2.ts\n"
    "#EXTINF:8\n"
    "foobar3.ts\n"
    "#EXT-X-DISCONTINUITY\n"
    "#EXT-X-PROGRAM-DATE-TIME:1970-01-01T02:00:00.000+00:00\n"
    "#EXT-X-MEDIA-SEQUENCE:20\n"
    "#EXTINF:5\n"
    "foobar4.ts\n"
    "#EXTINF:8\n"
    "foobar5.ts\n";

    m3u = ParseM3U8(obj, manifest3, sizeof(manifest3));
    try
    {
        Expect(m3u);
        BaseRepresentation *rep = m3u->getFirstPeriod()->getAdaptationSets().front()->
                                  getRepresentations().front();
        /* media sequence and discontinuity handling */
        uint64_t number;
        bool discont;
        Segment *seg = rep->getNextMediaSegment(13, &number, &discont);
        Expect(seg);
        Expect(number == 20);
        Expect(discont);

        /* mapping past discontinuity */
        Expect(rep->getSegmentNumberByTime(vlc_tick_from_sec(23), &number));
        Expect(number == 20);

        /* date set and incremented */
        seg = rep->getMediaSegment(10);
        Expect(seg);
        Expect(static_cast<HLSSegment *>(seg)->getUTCTime() == VLC_TICK_0 + vlc_tick_from_sec(10));
        seg = rep->getMediaSegment(11);
        Expect(seg);
        Expect(static_cast<HLSSegment *>(seg)->getUTCTime() == VLC_TICK_0 + vlc_tick_from_sec(10 + 8));

        /* date change after discontinuity */
        seg = rep->getMediaSegment(20);
        Expect(seg);
        Expect(static_cast<HLSSegment *>(seg)->getUTCTime() == VLC_TICK_0 + vlc_tick_from_sec(7200));

        vlc_tick_t begin, end, duration;
        Expect(rep->getMediaPlaybackRange(&begin, &end, &duration));
        Expect(begin == vlc_tick_from_sec(0));
        Expect(end == vlc_tick_from_sec(34));
        Expect(duration == vlc_tick_from_sec(34));

        delete m3u;
    }
    catch (...)
    {
        delete m3u;
        return 1;
    }

    /* Manifest 4 */
    const char manifest4[] =
    "#EXTM3U\n"
    "#EXT-X-MEDIA-SEQUENCE:10\n"
    "#EXT-X-START:TIME-OFFSET=-11.5,PRECISE=NO\n"
    "#EXTINF:10\n"
    "foobar.ts\n"
    "#EXTINF:10\n"
    "foobar.ts\n"
    "#EXTINF:10\n"
    "foobar.ts\n"
    "#EXTINF:10\n"
    "foobar.ts\n"
    "#EXTINF:10\n"
    "foobar.ts\n"
    "#EXT-X-ENDLIST\n";

    m3u = ParseM3U8(obj, manifest4, sizeof(manifest4));
    try
    {
        Expect(m3u);
        Expect(m3u->isLive() == false);
        Expect(m3u->presentationStartOffset.Get() == ((50 - 11.5) * CLOCK_FREQ));
        BaseRepresentation *rep = m3u->getFirstPeriod()->getAdaptationSets().front()->
                                  getRepresentations().front();
        Expect(bufferingLogic.getStartSegmentNumber(rep) == 13);
        m3u->presentationStartOffset.Set(11.5 * CLOCK_FREQ);
        Expect(bufferingLogic.getStartSegmentNumber(rep) == 11);

        delete m3u;
    }
    catch (...)
    {
        delete m3u;
        return 1;
    }


    return 0;
}

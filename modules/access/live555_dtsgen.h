/*****************************************************************************
 * live555_dtsgen.h : DTS rebuilder for pts only streams
 *****************************************************************************
 * Copyright (C) 2018 VideoLabs, VLC authors and VideoLAN
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
#define DTSGEN_REORDER_MAX   4 /* should be enough */
#define DTSGEN_HISTORY_COUNT (DTSGEN_REORDER_MAX + 2)
//#define DTSGEN_DEBUG

struct dtsgen_t
{
    vlc_tick_t history[DTSGEN_HISTORY_COUNT];
    vlc_tick_t ordereddts[DTSGEN_HISTORY_COUNT];
    vlc_tick_t i_startingdts;
    vlc_tick_t i_startingdiff;
    unsigned reorderdepth;
    unsigned count;
};

static int cmpvlctickp(const void *p1, const void *p2)
{
    if(*((vlc_tick_t *)p1) >= *((vlc_tick_t *)p2))
        return *((vlc_tick_t *)p1) > *((vlc_tick_t *)p2) ? 1 : 0;
    else
        return -1;
}

static void dtsgen_Init(struct dtsgen_t *d)
{
    d->count = 0;
    d->reorderdepth = 0;
}

static void dtsgen_Resync(struct dtsgen_t *d)
{
    d->count = 0;
    d->reorderdepth = 0;
}

#define dtsgen_Clean(d)

/*
 * RTSP sends in decode order, but only provides PTS as timestamp
 * P0 P2 P3 P1 P5 P7 P8 P6
 * D0 D2 D3 D1 D5 D7 D8 D6 <- wrong !
 * creating a non monotonical sequence when used as DTS, then PCR
 *
 * We need to have a suitable DTS for proper PCR and transcoding
 * with the usual requirements DTS0 < DTS1 and DTSN < PTSN
 *
 * So we want to find the closest DTS matching those conditions
 *  P0 P2 P3[P1]P5 P7 P8 P6
 * [D0]D1 D2 D3 D4 D5 D6 D7
 *
 * Which means that within a reorder window,
 * we want the PTS time index after reorder as DTS
 * [P0 P2 P3 P1]P5 P7 P8 P6
 * [P0 P1 P2 P3] reordered
 * [D0 D1 D2 D3]D4 D5 D6 D7
 * we need to pick then N frames before in reordered order (== reorder depth)
 *  P0 P2 P3[P1]P5 P7 P8 P6
 * [D0]D1 D2 D3 D4 D5 D6 D7
 * so D0 < P1 (we can also pick D1 if we want DTSN <= PTSN)
 *
 * Since it would create big delays with low fps streams we need
 * - to avoid buffering packets
 * - to detect real reorder depth (low fps usually have no reorder)
 *
 * We need then to:
 * - Detect reorder depth
 * - Keep track of last of N past timestamps, > maximum possible reorder
 * - Make sure a suitable dts is still created while detecting reorder depth
 *
 * While receiving the N first packets (N>max reorder):
 * - check if it needs reorder, or increase depth
 * - create slow increments in DTS while taking any frame as a start,
 *   substracting the total difference between first and last packet,
 *   and removing the possible offset after reorder,
 *   divided by max possible frames.
 *
 * Once reorder depth is fully known,
 * - use N previous frames reordered PTS as DTS for current PTS.
 *  (with mandatory gap/increase in DTS caused by previous step)
 */

static void dtsgen_AddNextPTS(struct dtsgen_t *d, vlc_tick_t i_pts)
{
    /* Check saved pts in reception order to find reordering depth */
    if(d->count > 0 && d->count < DTSGEN_HISTORY_COUNT)
    {
        unsigned i;
        if(d->count > (1 + d->reorderdepth))
            i = d->count - (1 + d->reorderdepth);
        else
            i = 0;

        for(; i < d->count; i++)
        {
            if(d->history[i] > i_pts)
            {
                if(d->reorderdepth < DTSGEN_REORDER_MAX)
                    d->reorderdepth++;
            }
            break;
        }
    }

    /* insert current */
    if(d->count == DTSGEN_HISTORY_COUNT)
    {
        d->ordereddts[0] = i_pts; /* remove lowest */
        memmove(d->history, &d->history[1],
                sizeof(d->history[0]) * (d->count - 1));
    }
    else
    {
        d->history[d->count] = i_pts;
        d->ordereddts[d->count++] = i_pts;
    }

    /* order pts in second list, will be used as dts */
    qsort(&d->ordereddts, d->count, sizeof(d->ordereddts[0]), cmpvlctickp);
}

static vlc_tick_t dtsgen_GetDTS(struct dtsgen_t *d)
{
    vlc_tick_t i_dts = VLC_TICK_INVALID;

    /* When we have inspected enough packets,
     * use the reorderdepth th packet as dts offset */
    if(d->count > DTSGEN_REORDER_MAX)
    {
        i_dts = d->ordereddts[d->count - d->reorderdepth - 1];
    }
    /* When starting, we craft a slow incrementing DTS to ensure
       we can't go backward due to reorder need */
    else if(d->count == 1)
    {
        d->i_startingdts =
        i_dts = __MAX(d->history[0] - VLC_TICK_FROM_MS(150), VLC_TICK_0);
        d->i_startingdiff = d->history[0] - i_dts;
    }
    else if(d->count > 1)
    {
        vlc_tick_t i_diff = d->ordereddts[d->count - 1] -
                            d->ordereddts[0];
        i_diff = __MIN(d->i_startingdiff, i_diff);
        d->i_startingdts += i_diff / DTSGEN_REORDER_MAX;
        i_dts = d->i_startingdts;
    }

    return i_dts;
}

#ifdef DTSGEN_DEBUG
static void dtsgen_Debug(vlc_object_t *p_demux, struct dtsgen_t *d,
                         vlc_tick_t dts, vlc_tick_t pts)
{
    if(pts == VLC_TICK_INVALID)
        return;
    msg_Dbg(p_demux, "dtsgen %" PRId64 " / pts %" PRId64 " diff %" PRId64 ", "
                     "pkt count %u, reorder %u",
            dts % (10 * CLOCK_FREQ),
            pts % (10 * CLOCK_FREQ),
            (pts - dts) % (10 * CLOCK_FREQ),
            d->count, d->reorderdepth);
}
#else
    #define dtsgen_Debug(a,b,c,d)
#endif

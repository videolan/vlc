/*****************************************************************************
 * m3u.c: M3U unit testing
 *****************************************************************************
 * Copyright (C) 2021 VideoLabs, VideoLAN and VLC Authors
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc/vlc.h>
#include "../../../../lib/libvlc_internal.h"
#include "../../../libvlc/test.h"

#include <vlc_common.h>
#include <vlc_modules.h>
#include <vlc_demux.h>
#include <vlc_meta.h>
#include <vlc_input_item.h>

#define BAILOUT(run) { fprintf(stderr, "failed %s line %d\n", run, __LINE__); \
                        return 1; }
#define EXPECT(foo) if(!(foo)) BAILOUT(run)
#define NOPFIL(n) "bar"#n
#define NOPURI(n) INPUT_ITEM_URI_NOP "/" NOPFIL(n)

static int runtest(const char *run,
                   libvlc_instance_t *vlc,
                   const char *data, size_t datasz,
                   int(*checkfunc)(const char *, const input_item_node_t *))
{
    stream_t *s = vlc_stream_MemoryNew(vlc->p_libvlc_int, (uint8_t *)data, datasz, true);
    if(!s)
        BAILOUT(run);

    demux_t *pl = demux_New(VLC_OBJECT(vlc->p_libvlc_int), "m3u", INPUT_ITEM_URI_NOP, s, NULL);
    if(!pl || !pl->pf_readdir)
    {
        vlc_stream_Delete(s);
        BAILOUT(run);
    }

    int ret = 0;
    input_item_t *p_item = input_item_New(NULL, NULL);
    if(p_item)
    {
        input_item_node_t *p_node = input_item_node_Create(p_item);
        if(p_node)
        {
            pl->pf_readdir(pl, p_node);
            ret = checkfunc(run, p_node);
            input_item_node_Delete(p_node);
        }
        else
        {
            ret = 1;
        }
        input_item_Release(p_item);
    }
    else
    {
        ret = 1;
    }

    demux_Delete(pl);

    return ret;
}

const char m3uplaylist0[] =
"#EXTM3U\n"
"#JUNK\n"
NOPFIL(0) "\n"
NOPURI(1) "\n";

static int check0(const char *run, const input_item_node_t *p_node)
{
    EXPECT(p_node->i_children == 2);

    const input_item_t *p_item = p_node->pp_children[0]->p_item;
    EXPECT(p_item->psz_name && p_item->psz_uri);
    EXPECT(!strcmp(NOPURI(0), p_item->psz_uri));
    EXPECT(!strcmp(NOPFIL(0), p_item->psz_name));
    EXPECT(p_item->i_duration == INPUT_DURATION_INDEFINITE);

    p_item = p_node->pp_children[1]->p_item;
    EXPECT(p_item->psz_name && p_item->psz_uri);
    EXPECT(!strcmp(p_item->psz_name, p_item->psz_uri));
    EXPECT(!strcmp(NOPURI(1), p_item->psz_name));

    return 0;
}

const char m3uplaylist1[] =
"#EXTM3U\n"
"#EXTINF: 1\n"
NOPFIL(0) "\n"
"#EXTINF: 1.11\n"
NOPURI(1) "\n"
"#EXTINF: -2,\n"
"#JUNK:foo\n"
NOPURI(2) "\n"
"#EXTINF: 3,artist3 - name3\n"
NOPURI(3) "\n"
"#EXTINF: 4,,name4\n"
NOPURI(4) "\n"
"#EXTINF: 5,artist5,name5\n"
NOPURI(5) "\n";

static int check1(const char *run, const input_item_node_t *p_node)
{
    EXPECT(p_node->i_children == 6);

    const input_item_t *p_item = p_node->pp_children[0]->p_item;
    EXPECT(p_item->psz_name && p_item->psz_uri);
    EXPECT(!strcmp(NOPFIL(0), p_item->psz_name));
    EXPECT(p_item->i_duration == vlc_tick_from_sec(1));

    p_item = p_node->pp_children[1]->p_item;
    EXPECT(p_item->psz_name && p_item->psz_uri);
    EXPECT(!strcmp(NOPURI(1), p_item->psz_name));
    EXPECT(p_item->i_duration == vlc_tick_from_sec(1.11));

    p_item = p_node->pp_children[2]->p_item;
    EXPECT(p_item->psz_name && p_item->psz_uri);
    EXPECT(!strcmp(NOPURI(2), p_item->psz_uri));
    EXPECT(!strcmp(p_item->psz_name, p_item->psz_uri));
    EXPECT(p_item->i_duration == INPUT_DURATION_INDEFINITE);

    p_item = p_node->pp_children[3]->p_item;
    EXPECT(p_item->psz_name && p_item->psz_uri);
    EXPECT(!strcmp(NOPURI(3), p_item->psz_uri));
    EXPECT(!strcmp("name3", p_item->psz_name));
    const char *p = vlc_meta_Get(p_item->p_meta, vlc_meta_Artist);
    EXPECT(p && !strcmp("artist3", p));

    p_item = p_node->pp_children[4]->p_item;
    EXPECT(p_item->psz_name && p_item->psz_uri);
    EXPECT(!strcmp("name4", p_item->psz_name));
    EXPECT(vlc_meta_Get(p_item->p_meta, vlc_meta_Artist) == NULL);

    p_item = p_node->pp_children[5]->p_item;
    EXPECT(p_item->psz_name && p_item->psz_uri);
    EXPECT(!strcmp("name5", p_item->psz_name));
    p = vlc_meta_Get(p_item->p_meta, vlc_meta_Artist);
    EXPECT(p && !strcmp("artist5", p));

    return 0;
}

const char m3uplaylist2[] =
"#EXTM3U\n"
"#PLAYLIST:playlist0\n"
"#EXTINF:-1 tvg-id=\"id0\" tvg-logo=\"logo0\" group-title=\"group0\",name0\n"
NOPURI(0)"\n"
"#EXTGRP:group1\n"
"#EXTINF:-1,name1\n"
NOPURI(1)"\n"
"#EXTINF:-1,name2\n"
NOPURI(2)"\n";

static int check2(const char *run, const input_item_node_t *p_node)
{
    EXPECT(p_node->i_children == 3);
    const char *p = vlc_meta_Get(p_node->p_item->p_meta, vlc_meta_Title);
    EXPECT(p && !strcmp(p, "playlist0"));

    const input_item_t *p_item = p_node->pp_children[0]->p_item;
    EXPECT(p_item->psz_name && p_item->psz_uri);
    EXPECT(!strcmp(NOPURI(0), p_item->psz_uri));
    EXPECT(!strcmp("name0", p_item->psz_name));
    p = vlc_meta_Get(p_item->p_meta, vlc_meta_Publisher);
    EXPECT(p && !strcmp("group0", p));
    p = vlc_meta_Get(p_item->p_meta, vlc_meta_ArtworkURL);

    p_item = p_node->pp_children[1]->p_item;
    EXPECT(p_item->psz_name && p_item->psz_uri);
    EXPECT(!strcmp(NOPURI(1), p_item->psz_uri));
    EXPECT(!strcmp("name1", p_item->psz_name));
    p = vlc_meta_Get(p_item->p_meta, vlc_meta_Publisher);
    EXPECT(p && !strcmp("group1", p));

    p_item = p_node->pp_children[2]->p_item;
    EXPECT(p_item->psz_name && p_item->psz_uri);
    EXPECT(!strcmp(NOPURI(2), p_item->psz_uri));
    EXPECT(!strcmp("name2", p_item->psz_name));
    p = vlc_meta_Get(p_item->p_meta, vlc_meta_Publisher);
    EXPECT(p && !strcmp("group1", p));

    return 0;
}


int main(void)
{
    test_init();

    libvlc_instance_t *vlc = libvlc_new(0, NULL);
    if(!vlc)
        return 1;

    int ret = runtest("run0", vlc, m3uplaylist0, sizeof(m3uplaylist0), check0);
    if(!ret)
        ret = runtest("run1", vlc, m3uplaylist1, sizeof(m3uplaylist1), check1);
    if(!ret)
        ret = runtest("run2", vlc, m3uplaylist2, sizeof(m3uplaylist2), check2);

    libvlc_release(vlc);
    return ret;
}

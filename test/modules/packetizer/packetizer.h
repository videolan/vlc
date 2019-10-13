/*****************************************************************************
 * packetizer.h: packetizer unit testing
 *****************************************************************************
 * Copyright (C) 2019 VideoLabs, VideoLAN and VLC Authors
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
#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_modules.h>
#include <vlc_demux.h>
#include <vlc_codec.h>
#include <vlc_meta.h>

enum
{
    OK = VLC_SUCCESS,
    FAIL = VLC_EGENERIC,
};

struct params_s
{
    libvlc_instance_t *vlc;
    vlc_object_t *obj;
    vlc_fourcc_t codec;
    unsigned i_rate_num;
    unsigned i_rate_den;
    unsigned i_read_size;
    unsigned i_frame_count;
    bool b_extra;
};

#define BAILOUT(run) { fprintf(stderr, "failed %s line %d\n", run, __LINE__); \
                        return 1; }
#define RUN(run, test, a, b, res) \
    if(test(#test " " run, a, b, &params) != res) BAILOUT(#test " " run)
#define EXPECT(foo) if(!(foo)) BAILOUT(run)


static void delete_packetizer(decoder_t *p_pack)
{
    if(p_pack->p_module)
        module_unneed(p_pack, p_pack->p_module);
    es_format_Clean(&p_pack->fmt_in);
    es_format_Clean(&p_pack->fmt_out);
    if(p_pack->p_description)
        vlc_meta_Delete(p_pack->p_description);
    vlc_object_delete(p_pack);
}

static decoder_t *create_packetizer(libvlc_instance_t *vlc,
                                    unsigned num, unsigned den,
                                    vlc_fourcc_t codec)
{
    decoder_t *p_pack = vlc_object_create(vlc->p_libvlc_int,
                                          sizeof(*p_pack));
    if(!p_pack)
        return NULL;
    p_pack->pf_decode = NULL;
    p_pack->pf_packetize = NULL;

    es_format_Init(&p_pack->fmt_in, VIDEO_ES, codec);
    es_format_Init(&p_pack->fmt_out, VIDEO_ES, 0);
    p_pack->fmt_in.video.i_frame_rate = num;
    p_pack->fmt_in.video.i_frame_rate_base = den;
    p_pack->fmt_in.b_packetized = false;

    p_pack->p_module = module_need( p_pack, "packetizer", NULL, false );
    if(!p_pack->p_module)
        delete_packetizer(p_pack);
    return p_pack;
}

static int test_packetize(const char *run,
                          const uint8_t *p_data, size_t i_data,
                          const struct params_s *params)
{
    decoder_t *p = create_packetizer(params->vlc,
                                     params->i_rate_num,
                                     params->i_rate_den,
                                     params->codec);
    EXPECT(p != NULL);

    stream_t *s = vlc_stream_MemoryNew(params->obj,
                                       (uint8_t *)p_data, i_data, true);
    EXPECT(s != NULL);
    block_t *outchain = NULL;
    block_t **outappend = &outchain;
    block_t *p_block;
    unsigned i_count = 0;
    do
    {
        p_block = vlc_stream_Block(s, params->i_read_size);
        block_t *in = p_block;
        if(in && outchain == NULL)
            in->i_dts = VLC_TICK_0;
        block_t *out;
        do
        {
            out = p->pf_packetize(p, in ? &in : NULL);
            if(out)
            {
                fprintf(stderr, "block #%u dts %"PRId64" sz %"PRId64
                                " flags %x sz %"PRId64"\n",
                        i_count, out->i_dts, out->i_buffer,
                        out->i_flags, out->i_buffer );
                block_ChainLastAppend(&outappend, out);
                ++i_count;
            }
        } while(out);
    } while(p_block);

    EXPECT(i_count == params->i_frame_count);

    if(params->i_rate_num && params->i_rate_den)
    {
        EXPECT(p->fmt_out.video.i_frame_rate == params->i_rate_num);
        EXPECT(p->fmt_out.video.i_frame_rate_base == params->i_rate_den);
        EXPECT(p->fmt_out.video.i_visible_width);
        EXPECT(p->fmt_out.video.i_visible_height);
    }

    if(params->i_frame_count)
    {
        EXPECT(outchain != NULL);
    }
    block_ChainRelease(outchain);

    EXPECT(!!params->b_extra == !!p->fmt_out.i_extra);

    delete_packetizer(p);

    vlc_stream_Delete(s);

    return OK;
}

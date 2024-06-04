/*****************************************************************************
 * h266_nal.c tests NAL conversions
 *****************************************************************************
 * Copyright (C) 2024 VideoLabs, VLC authors and VideoLAN
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

#ifdef NDEBUG
 #undef NDEBUG
#endif

#include <assert.h>
#include <vlc_common.h>
#include "../modules/packetizer/h266_nal.h"
#include "../modules/packetizer/hxxx_nal.h"

#define runtest(dataset, name, testfunction) \
    printf("\nTEST %d %s\n", number, name);\
    testfunction( test##dataset, sizeof(test##dataset), \
                  p_res, rgi_res )

static inline void hexdump(const void *p, size_t sz)
{
    for(size_t i=0; i<sz; i++)
        fprintf(stderr,"%2.2x ", ((const uint8_t *)p)[i]);
    fprintf(stderr,"\n");
}


static const uint8_t test_dcr_qpa0_qp20[] = {
    0xff, 0x00, 0x65, 0x5f, 0x0c, 0x02, 0x40, 0x80, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x80, 0x04,
    0x38, 0x00, 0x00, 0x02, 0x8f, 0x00, 0x01, 0x00, 0xf1, 0x00, 0x79, 0x00,
    0xab, 0x02, 0x40, 0x80, 0x00, 0x00, 0x80, 0x0f, 0x02, 0x00, 0x43, 0x91,
    0xa8, 0x01, 0xcd, 0xe8, 0x84, 0xd1, 0x49, 0xef, 0x28, 0x4d, 0x95, 0x8c,
    0x10, 0x20, 0x94, 0x00, 0x98, 0x80, 0x82, 0x08, 0x21, 0x03, 0x08, 0x42,
    0x16, 0x22, 0x10, 0xb2, 0x42, 0x16, 0xa1, 0x0b, 0xd1, 0xea, 0xd4, 0x97,
    0x92, 0x4d, 0x49, 0x64, 0x88, 0xb5, 0x11, 0x78, 0x89, 0x35, 0x11, 0x22,
    0x92, 0x22, 0x4c, 0x91, 0x12, 0x6a, 0x32, 0xc4, 0x42, 0x16, 0x48, 0x42,
    0xd4, 0x21, 0x78, 0x42, 0x4d, 0x42, 0x12, 0x29, 0x21, 0x09, 0x32, 0x42,
    0x12, 0xea, 0x48, 0x42, 0x42, 0x44, 0x42, 0x12, 0x28, 0x88, 0x42, 0x4c,
    0x44, 0x21, 0x2e, 0xa2, 0x21, 0x09, 0x14, 0x64, 0x21, 0x26, 0x32, 0x10,
    0x97, 0x51, 0x90, 0x85, 0x02, 0x0b, 0x08, 0x41, 0x01, 0x88, 0x84, 0x0c,
    0x91, 0x06, 0xa4, 0x02, 0x66, 0x08, 0x20, 0x82, 0xc2, 0x00, 0x41, 0x62,
    0x01, 0x01, 0x08, 0x10, 0x08, 0x0a, 0x84, 0x08, 0x04, 0x06, 0x90, 0x81,
    0x00, 0x80, 0xb1, 0x02, 0x01, 0x01, 0x10, 0x40, 0x20, 0x2c, 0x82, 0x01,
    0x01, 0x21, 0x00, 0x81, 0x90, 0x10, 0x11, 0x08, 0x08, 0x0b, 0x21, 0x01,
    0x01, 0x22, 0x02, 0x06, 0x81, 0x01, 0x24, 0x08, 0x1c, 0x10, 0x31, 0x00,
    0x85, 0x90, 0x20, 0x44, 0x20, 0x40, 0xb2, 0x10, 0x20, 0x48, 0x81, 0x06,
    0x82, 0x04, 0x90, 0x41, 0xc2, 0x0c, 0x81, 0x16, 0x84, 0x12, 0x42, 0x1c,
    0x43, 0x42, 0x5c, 0x8e, 0x54, 0x08, 0x2c, 0x20, 0x04, 0x16, 0x20, 0x10,
    0x10, 0x81, 0x00, 0x80, 0xa8, 0x40, 0x80, 0x40, 0xfb, 0xee, 0xc4, 0x67,
    0x24, 0x00, 0x00, 0x0f, 0xa4, 0x00, 0x01, 0x77, 0x00, 0x62, 0x90, 0x00,
    0x01, 0x00, 0x12, 0x00, 0x81, 0x00, 0x00, 0x07, 0x81, 0x00, 0x21, 0xc8,
    0x89, 0x00, 0xd7, 0x18, 0x68, 0xd3, 0x9c, 0xe7, 0x10
};

static void test_dcr( void )
{
    struct h266_dcr_values values = {0}, newvalues = {0};
    struct h266_dcr_params params = {0}, newparams = {0};
    params.p_values = &values;
    newparams.p_values = &newvalues;
    assert(h266_parse_DecoderConfigurationRecord(test_dcr_qpa0_qp20,
                                                 sizeof(test_dcr_qpa0_qp20),
                                                 &params));
    assert(values.ptl_present_flag);
    assert(values.ptl.constraints_info.gci_num_additional_bits);

    /* Rebuild and check DCR */
    size_t i_dcr = 0;
    uint8_t *p_dcr = h266_create_DecoderConfigurationRecord(&params, 1, &i_dcr);
    assert(p_dcr);
    fprintf(stderr,"Recreated DCR: ");
    hexdump(p_dcr, i_dcr);
    assert(h266_parse_DecoderConfigurationRecord(p_dcr, i_dcr, &newparams));
    assert(!memcmp(&values, &newvalues, sizeof(values)));
    free(p_dcr);

    /* Compare as AnnexB serializerd, because it's easier */
    uint8_t *annexb0, *annexb1;
    size_t szannexb0, szannexb1;
    annexb0 = h266_create_AnnexbExtradataFromParams(&params, &szannexb0);
    annexb1 = h266_create_AnnexbExtradataFromParams(&params, &szannexb1);
    assert(szannexb0);
    fprintf(stderr,"AnnexB: ");
    hexdump(annexb0, szannexb0);
    assert(szannexb0 == szannexb1);
    assert(!memcmp(annexb0, annexb1, szannexb0));

    /* Regenerate from AnnexB, refetching values from SPS */
    memset(&params, 0, sizeof(params));

    const uint8_t *nal; size_t nalsz;
    hxxx_iterator_ctx_t ctx;
    hxxx_iterator_init(&ctx, annexb0, szannexb0, 0);

    while(hxxx_annexb_iterate_next(&ctx, &nal, &nalsz))
        h266_add_NALtoParams(nal, nalsz, &params);
    p_dcr = h266_create_DecoderConfigurationRecord(&params, 1, &i_dcr);
    assert(p_dcr);
    fprintf(stderr,"DCR from AnnexB: ");
    hexdump(p_dcr, i_dcr);

    memset(&params, 0x00, sizeof(params));
    memset(&values, 0x00, sizeof(values));
    params.p_values = &values;

    assert(h266_parse_DecoderConfigurationRecord(p_dcr, i_dcr, &params));

    /* FIXME: get a sample with same constraint bytes in SPS & DCR */
    /* Our sample has only constraint bytes in DCR, so it no longer exists as AnnexB */
    values.ptl.constraints_info = newvalues.ptl.constraints_info; // patch comparison for those

    assert(!memcmp(&values, &newvalues, sizeof(values)));
    free(p_dcr);

    free(annexb0);
    free(annexb1);
}

int main( void )
{
    test_dcr();

    return 0;
}

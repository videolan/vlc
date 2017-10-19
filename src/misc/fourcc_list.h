/*****************************************************************************
 * fourcc.c: fourcc helpers functions
 *****************************************************************************
 * Copyright © 2009-2011 Laurent Aimar
 *
 * Authors: Laurent Aimar <fenrir@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
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

static const staticentry_t p_list_video[] = {

    B(VLC_CODEC_MP1V, "MPEG-1 Video"),
        A("mp1v"),
        A("m1v "),
        A("mpg1"),
        A("BW10"),
        E("XMPG", "Xing MPEG-1 Intra"),

    B(VLC_CODEC_MPGV, "MPEG-1/2 Video"),
    B(VLC_CODEC_MP2V, "MPEG-2 Video"),
        A("mpeg"),
        A("mp2v"),
        A("MPEG"),
        A("mpg2"),
        A("MPG2"),
        A("H262"),

        E("PIM1", "Pinnacle DC1000 (MPEG-1 Video)"),

        E("hdv1", "HDV 720p30 (MPEG-2 Video)"),
        E("hdv2", "Sony HDV 1080i60 (MPEG-2 Video)"),
        E("hdv3", "FCP HDV 1080i50 (MPEG-2 Video)"),
        E("hdv4", "HDV 720p24 (MPEG-2 Video)"),
        E("hdv5", "HDV 720p25 (MPEG-2 Video)"),
        E("hdv6", "HDV 1080p24 (MPEG-2 Video)"),
        E("hdv7", "HDV 1080p25 (MPEG-2 Video)"),
        E("hdv8", "HDV 1080p30 (MPEG-2 Video)"),
        E("hdv9", "HDV 720p60 JVC (MPEG-2 Video)"),
        E("hdva", "HDV 720p50 (MPEG-2 Video)"),

        E("mx5n", "MPEG2 IMX NTSC 525/60 50Mb/s (FCP)"),
        E("mx5p", "MPEG2 IMX PAL 625/60 50Mb/s (FCP)"),
        E("mx4n", "MPEG2 IMX NTSC 525/60 40Mb/s (FCP)"),
        E("mx4p", "MPEG2 IMX PAL 625/50 40Mb/s (FCP)"),
        E("mx3n", "MPEG2 IMX NTSC 525/60 30Mb/s (FCP)"),
        E("mx3p", "MPEG2 IMX NTSC 625/50 30Mb/s (FCP)"),

        E("xdv1", "XDCAM HD 720p30 35Mb/s"),
        E("xdv2", "XDCAM HD 1080i60 35Mb/s"),
        E("xdv3", "XDCAM HD 1080i50 35Mb/s"),
        E("xdv4", "XDCAM HD 720p24 35Mb/s"),
        E("xdv5", "XDCAM HD 720p25 35Mb/s"),
        E("xdv6", "XDCAM HD 1080p24 35Mb/s"),
        E("xdv7", "XDCAM HD 1080p25 35Mb/s"),
        E("xdv8", "XDCAM HD 1080p30 35Mb/s"),
        E("xdv9", "XDCAM HD 720p60 35Mb/s"),
        E("xdva", "XDCAM HD 720p50 35Mb/s"),

        E("xdvb", "XDCAM EX 1080i60 50Mb/s CBR"),
        E("xdvc", "XDCAM EX 1080i50 50Mb/s CBR"),
        E("xdvd", "XDCAM EX 1080p24 50Mb/s CBR"),
        E("xdve", "XDCAM EX 1080p25 50Mb/s CBR"),
        E("xdvf", "XDCAM EX 1080p30 50Mb/s CBR"),

        E("xd51", "XDCAM HD422 720p30 50Mb/s CBR"),
        E("xd54", "XDCAM HD422 720p24 50Mb/s CBR"),
        E("xd55", "XDCAM HD422 720p25 50Mb/s CBR"),
        E("xd59", "XDCAM HD422 720p60 50Mb/s CBR"),
        E("xd5a", "XDCAM HD422 720p50 50Mb/s CBR"),
        E("xd5b", "XDCAM HD422 1080i60 50Mb/s CBR"),
        E("xd5c", "XDCAM HD422 1080i50 50Mb/s CBR"),
        E("xd5d", "XDCAM HD422 1080p24 50Mb/s CBR"),
        E("xd5e", "XDCAM HD422 1080p25 50Mb/s CBR"),
        E("xd5f", "XDCAM HD422 1080p30 50Mb/s CBR"),

        E("xdhd", "XDCAM HD 540p"),
        E("xdh2", "XDCAM HD422 540p"),

        E("AVmp", "AVID IMX PAL"),
        E("MMES", "Matrox MPEG-2"),
        E("mmes", "Matrox MPEG-2"),
        E("PIM2", "Pinnacle MPEG-2"),
        E("LMP2", "Lead MPEG-2"),

        E("VCR2", "ATI VCR-2"),

    B(VLC_CODEC_MP4V, "MPEG-4 Video"),
        A("mp4v"),
        A("DIVX"),
        A("divx"),
        A("MP4S"),
        A("mp4s"),
        A("M4S2"),
        A("m4s2"),
        A("MP4V"),
        A("\x04\x00\x00\x00"),
        A("m4cc"),
        A("M4CC"),
        A("FMP4"),
        A("fmp4"),
        A("DCOD"),
        A("MVXM"),
        A("PM4V"),
        A("M4T3"),
        A("GEOX"),
        A("GEOV"),
        A("DMK2"),
        A("WV1F"),
        A("DIGI"),
        A("INMC"),
        A("SN40"),
        A("EPHV"),
        A("DM4V"),
        A("SM4V"),
        A("DYM4"),
        /* XVID flavours */
        E("xvid", "Xvid MPEG-4 Video"),
        E("XVID", "Xvid MPEG-4 Video"),
        E("XviD", "Xvid MPEG-4 Video"),
        E("XVIX", "Xvid MPEG-4 Video"),
        E("xvix", "Xvid MPEG-4 Video"),
        /* DX50 */
        E("DX50", "DivX MPEG-4 Video"),
        E("dx50", "DivX MPEG-4 Video"),
        E("BLZ0", "Blizzard MPEG-4 Video"),
        E("DXGM", "Electronic Arts Game MPEG-4 Video"),
        E("DreX", "DreX Mpeg-4"),
        /* 3ivx delta 4 */
        E("3IV2", "3ivx MPEG-4 Video"),
        E("3iv2", "3ivx MPEG-4 Video"),
        /* Various */
        E("UMP4", "UB MPEG-4 Video"),
        E("SEDG", "Samsung MPEG-4 Video"),
        E("RMP4", "REALmagic MPEG-4 Video"),
        E("LMP4", "Lead MPEG-4 Video"),
        E("HDX4", "Jomigo HDX4 (MPEG-4 Video)"),
        E("hdx4", "Jomigo HDX4 (MPEG-4 Video)"),
        E("SMP4", "Samsung SMP4 (MPEG-4 Video)"),
        E("smp4", "Samsung SMP4 (MPEG-4 Video)"),
        E("fvfw", "libavcodec MPEG-4"),
        E("FVFW", "libavcodec MPEG-4"),
        E("FFDS", "FFDShow MPEG-4"),
        E("VIDM", "vidm 4.01 codec"),
        E("DP02", "DynaPel MPEG-4 codec"),
        E("PLV1", "Pelco DVR MPEG-4"),
        E("QMP4", "QNAP Systems MPEG-4"),
        E("qMP4", "QNAP Systems MPEG-4"),
        A("wMP4"), /* Seems QNAP too */
        /* 3ivx delta 3.5 Unsupported
         * putting it here gives extreme distorted images */
        //E("3IV1", "3ivx delta 3.5 MPEG-4 Video"),
        //E("3iv1", "3ivx delta 3.5 MPEG-4 Video"),

    /* MSMPEG4 v1 */
    B(VLC_CODEC_DIV1, "MS MPEG-4 Video v1"),
        A("DIV1"),
        A("div1"),
        A("MPG4"),
        A("mpg4"),
        A("mp41"),

    /* MSMPEG4 v2 */
    B(VLC_CODEC_DIV2, "MS MPEG-4 Video v2"),
        A("DIV2"),
        A("div2"),
        A("MP42"),
        A("mp42"),

    /* MSMPEG4 v3 / M$ mpeg4 v3 */
    B(VLC_CODEC_DIV3, "MS MPEG-4 Video v3"),
        A("DIV3"),
        A("MPG3"),
        A("mpg3"),
        A("div3"),
        A("MP43"),
        A("mp43"),
        /* DivX 3.20 */
        A("DIV4"),
        A("div4"),
        A("DIV5"),
        A("div5"),
        A("DIV6"),
        A("div6"),
        E("divf", "DivX 4.12"),
        E("DIVF", "DivX 4.12"),
        /* Cool Codec */
        A("COL1"),
        A("col1"),
        A("COL0"),
        A("col0"),
        /* AngelPotion stuff */
        A("AP41"),
        /* 3ivx doctered divx files */
        A("3IVD"),
        A("3ivd"),
        /* who knows? */
        A("3VID"),
        A("3vid"),
        A("DVX1"),
        A("DVX3"),

    /* Sorenson v1 */
    B(VLC_CODEC_SVQ1, "SVQ-1 (Sorenson Video v1)"),
        A("SVQ1"),
        A("svq1"),
        A("svqi"),

    /* Sorenson v3 */
    B(VLC_CODEC_SVQ3, "SVQ-3 (Sorenson Video v3)"),
        A("SVQ3"),

    /* HEVC / H.265 */
    B(VLC_CODEC_HEVC, "MPEG-H Part2/HEVC (H.265)"),
        A("hevc"),
        A("HEVC"),
        A("h265"),
        A("H265"),
        A("x265"),
        A("hev1"),
        A("hvc1"),
        A("HM10"),
        E("dvhe", "Dolby Vision HEVC (H.265)"),
        /* E("dvh1", "Dolby Vision HEVC (H.265)"), Collides with DV */

    /* h264 */
    B(VLC_CODEC_H264, "H264 - MPEG-4 AVC (part 10)"),
        A("H264"),
        A("h264"),
        A("x264"),
        A("X264"),
        A("V264"),
        /* avc1: special case h264 */
        A("avc1"),
        A("AVC1"),
        A("AVCB"), /* smooth streaming alias */
        A("avc3"),
        E("ai5p", "AVC-Intra  50M 720p24/30/60"),
        E("ai5q", "AVC-Intra  50M 720p25/50"),
        E("ai52", "AVC-Intra  50M 1080p25/50"),
        E("ai53", "AVC-Intra  50M 1080p24/30/60"),
        E("ai55", "AVC-Intra  50M 1080i50"),
        E("ai56", "AVC-Intra  50M 1080i60"),
        E("ai1p", "AVC-Intra 100M 720p24/30/60"),
        E("ai1q", "AVC-Intra 100M 720p25/50"),
        E("ai12", "AVC-Intra 100M 1080p25/50"),
        E("ai13", "AVC-Intra 100M 1080p24/30/60"),
        E("ai15", "AVC-Intra 100M 1080i50"),
        E("ai16", "AVC-Intra 100M 1080i60"),
        E("dvav", "Dolby Vision H264"),
        E("dva1", "Dolby Vision H264"),
        E("VSSH", "Vanguard VSS H264"),
        E("VSSW", "Vanguard VSS H264"),
        E("vssh", "Vanguard VSS H264"),
        E("DAVC", "Dicas MPEGable H.264/MPEG-4 AVC"),
        E("davc", "Dicas MPEGable H.264/MPEG-4 AVC"),
        E("x3eV", "DreX H.264"),
        E("GAVC", "GeoVision MPEG-4 AVC"),
        E("Q264", "QNAP H.264/MPEG-4 AVC"),
        E("q264", "QNAP H.264/MPEG-4 AVC"),
        A("UMSV"),
        A("SMV2"),
        A("tshd"),
        A("rv64"),

    /* H263 and H263i */
    /* H263(+) is also known as Real Video 1.0 */

    /* H263 */
    B(VLC_CODEC_H263, "H263"),
        A("H263"),
        A("h263"),
        A("VX1K"),
        A("s263"),
        A("S263"),
        A("u263"),
        A("lsvm"),
        E("D263", "DEC H263"),
        E("d263", "DEC H263"),
        E("L263", "LEAD H263"),
        E("M263", "Microsoft H263"),
        E("X263", "Xirlink H263"),
        /* Zygo (partial) */
        E("ZyGo", "ITU H263+"),

    /* H263i */
    B(VLC_CODEC_H263I, "I263.I"),
        A("I263"),
        A("i263"),

    /* H263P */
    B(VLC_CODEC_H263P, "ITU H263+"),
        E("ILVR", "ITU H263+"),
        E("viv1", "H263+"),
        E("vivO", "H263+"),
        E("viv2", "H263+"),
        E("VIVO", "H263+"),
        E("U263", "UB H263+"),

    /* Flash (H263) variant */
    B(VLC_CODEC_FLV1, "Flash Video"),
        A("FLV1"),
        A("flv "),

    /* H261 */
    B(VLC_CODEC_H261, "H.261"),
        A("H261"),
        A("h261"),

    B(VLC_CODEC_FLIC, "Flic Video"),
        A("FLIC"),
        A("AFLC"),

    /* MJPEG */
    B(VLC_CODEC_MJPG, "Motion JPEG Video"),
        A("MJPG"),
        A("MJPx"),
        A("mjpg"),
        A("mJPG"),
        A("mjpa"),
        A("JFIF"),
        A("JPGL"),
        A("MMJP"),
        A("FLJP"),
        A("FMJP"),
        A("SJPG"),
        A("QIVG"), /* Probably QNAP */
        A("qIVG"), /* Probably QNAP */
        A("wIVG"), /* Probably QNAP */
        E("AVRn", "Avid Motion JPEG"),
        E("AVDJ", "Avid Motion JPEG"),
        E("ADJV", "Avid Motion JPEG"),
        E("dmb1", "Motion JPEG OpenDML Video"),
        E("DMB1", "Motion JPEG OpenDML Video"),
        E("ijpg", "Intergraph JPEG Video"),
        E("IJPG", "Intergraph JPEG Video"),
        E("ACDV", "ACD Systems Digital"),
        E("SLMJ", "SL M-JPEG"),

    B(VLC_CODEC_MJPGB, "Motion JPEG B Video"),
        A("mjpb"),

    B(VLC_CODEC_LJPG, "Lead Motion JPEG Video"),
        E("Ljpg", "Lead Motion JPEG"),

    /* SP5x */
    B(VLC_CODEC_SP5X, "Sunplus Motion JPEG Video"),
        A("SP5X"),
        A("SP53"),
        A("SP54"),
        A("SP55"),
        A("SP56"),
        A("SP57"),
        A("SP58"),

    /* DV */
    B(VLC_CODEC_DV, "DV Video"),
        A("dv  "),
        A("dvsl"),
        A("DVSD"),
        A("dvsd"),
        A("DVCS"),
        A("dvcs"),
        A("dvhd"),
        A("dvhq"),
        A("dvh1"),
        E("dvh2", "DV Video 720p24"),
        E("dvh3", "DV Video 720p25"),
        E("dvh4", "DV Video 720p30"),
        A("dv25"),
        A("dc25"),
        A("dvs1"),
        A("dvis"),
        A("CDV2"),
        A("CDVP"),
        A("PDVC"),
        A("IPDV"),
        A("ipdv"),
        A("pdvc"),
        A("SL25"),
        E("dvcp", "DV Video PAL"),
        E("dvc ", "DV Video NTSC" ),
        E("dvp ", "DV Video Pro"),
        E("dvpp", "DV Video Pro PAL"),
        E("dv50", "DV Video C Pro 50"),
        E("dv5p", "DV Video C Pro 50 PAL"),
        E("dv5n", "DV Video C Pro 50 NTSC"),
        E("dv1p", "DV Video C Pro 100 PAL" ),
        E("dv1n", "DV Video C Pro 100 NTSC" ),
        E("dvhp", "DV Video C Pro HD 720p" ),
        E("dvh5", "DV Video C Pro HD 1080i50" ),
        E("dvh6", "DV Video C Pro HD 1080i60" ),
        E("AVdv", "AVID DV"),
        E("AVd1", "AVID DV"),
        E("CDVC", "Canopus DV Video"),
        E("cdvc", "Canopus DV Video"),
        E("CDVH", "Canopus DV Video"),
        E("cdvh", "Canopus DV Video"),
        E("CDV5", "Canopus DV Video"),
        E("SLDV", "SoftLab DVCAM codec"),

    /* Windows Media Video */
    B(VLC_CODEC_WMV1, "Windows Media Video 7"),
        A("WMV1"),
        A("wmv1"),

    B(VLC_CODEC_WMV2, "Windows Media Video 8"),
        A("WMV2"),
        A("wmv2"),
        A("GXVE"),

    B(VLC_CODEC_WMV3, "Windows Media Video 9"),
        A("WMV3"),
        A("wmv3"),

    /* WMVA is the VC-1 codec before the standardization process,
     * it is not bitstream compatible and deprecated  */
    B(VLC_CODEC_WMVA, "Windows Media Video Advanced Profile"),
        A("WMVA"),
        A("wmva"),

    B(VLC_CODEC_VC1, "Windows Media Video VC1"),
        A("WVC1"),
        A("wvc1"),
        A("vc-1"),
        A("VC-1"),

    B(VLC_CODEC_WMVP, "Windows Media Video Presentation"),
        A("WMVP"),
        A("wmvp"),

    B(VLC_CODEC_WMVP2, "Windows Media Video Presentation, v2"),
        A("WVP2"),
        A("wvp2"),

    /* Microsoft Video 1 */
    B(VLC_CODEC_MSVIDEO1, "Microsoft Video 1"),
        A("MSVC"),
        A("msvc"),
        A("CRAM"),
        A("cram"),
        A("WHAM"),
        A("wham"),

    /* Microsoft RLE */
    B(VLC_CODEC_MSRLE, "Microsoft RLE Video"),
        A("mrle"),
        A("WRLE"),
        A("\x01\x00\x00\x00"),
        A("\x02\x00\x00\x00"),

    /* Indeo Video Codecs (Quality of this decoder on ppc is not good) */
    B(VLC_CODEC_INDEO3, "Indeo Video v3"),
        A("IV31"),
        A("iv31"),
        A("IV32"),
        A("iv32"),

    /* Huff YUV */
    B(VLC_CODEC_HUFFYUV, "Huff YUV Video"),
        A("HFYU"),

    B(VLC_CODEC_FFVHUFF, "Huff YUV Video"),
        A("FFVH"),

    /* On2 VP3 Video Codecs */
    B(VLC_CODEC_VP3, "On2's VP3 Video"),
        A("VP3 "),
        A("VP30"),
        A("vp30"),
        A("VP31"),
        A("vp31"),

    /* On2  VP5, VP6 codecs */
    B(VLC_CODEC_VP5, "On2's VP5 Video"),
        A("VP5 "),
        A("VP50"),

    B(VLC_CODEC_VP6, "On2's VP6.2 Video"),
        A("VP62"),
        A("vp62"),
        E("VP60", "On2's VP6.0 Video"),
        E("VP61", "On2's VP6.1 Video"),

    B(VLC_CODEC_VP6F, "On2's VP6.2 Video (Flash)"),
        A("VP6F"),
        A("FLV4"),

    B(VLC_CODEC_VP6A, "On2's VP6 A Video"),
        A("VP6A"),

    B(VLC_CODEC_VP7, "Google/On2's VP7 Video"),
        A("VP70"),
        A("VP71"),

    B(VLC_CODEC_VP8, "Google/On2's VP8 Video"),
        A("VP80"),

    B(VLC_CODEC_VP9, "Google/On2's VP9 Video"),
        A("VP90"),

    B(VLC_CODEC_AV1, "AOMedia's AV1 Video"),
        A("av10"),

    /* Xiph.org theora */
    B(VLC_CODEC_THEORA, "Xiph.org's Theora Video"),
        A("theo"),
        A("THEO"),
        A("Thra"),

    /* Xiph.org tarkin */
    B(VLC_CODEC_TARKIN, "Xiph.org's Tarkin Video"),
        A("tark"),

    /* Asus Video (Another thing that doesn't work on PPC) */
    B(VLC_CODEC_ASV1, "Asus V1 Video"),
        A("ASV1"),
    B(VLC_CODEC_ASV2, "Asus V2 Video"),
        A("ASV2"),

    /* FF video codec 1 (lossless codec) */
    B(VLC_CODEC_FFV1, "FF video codec 1"),
        A("FFV1"),

    /* ATI VCR1 */
    B(VLC_CODEC_VCR1, "ATI VCR1 Video"),
        A("VCR1"),

    /* Cirrus Logic AccuPak */
    B(VLC_CODEC_CLJR, "Creative Logic AccuPak"),
        A("CLJR"),

    /* Real Video */
    B(VLC_CODEC_RV10, "RealVideo 1.0"),
        A("RV10"),
        A("rv10"),

    B(VLC_CODEC_RV13, "RealVideo 1.3"),
        A("RV13"),
        A("rv13"),

    B(VLC_CODEC_RV20, "RealVideo G2 (2.0)"),
        A("RV20"),
        A("rv20"),

    B(VLC_CODEC_RV30, "RealVideo 8 (3.0)"),
        A("RV30"),
        A("rv30"),

    B(VLC_CODEC_RV40, "RealVideo 9/10 (4.0)"),
        A("RV40"),
        A("rv40"),

    /* Apple Video */
    B(VLC_CODEC_RPZA, "Apple Video"),
        A("rpza"),
        A("azpr"),
        A("RPZA"),
        A("AZPR"),

    B(VLC_CODEC_SMC, "Apple graphics"),
        A("smc "),

    B(VLC_CODEC_CINEPAK, "Cinepak Video"),
        A("CVID"),
        A("cvid"),

    /* Screen Capture Video Codecs */
    B(VLC_CODEC_TSCC, "TechSmith Camtasia Screen Capture"),
        A("TSCC"),
        A("tscc"),

    B(VLC_CODEC_CSCD, "CamStudio Screen Codec"),
        A("CSCD"),
        A("cscd"),

    B(VLC_CODEC_ZMBV, "DosBox Capture Codec"),
        A("ZMBV"),

    B(VLC_CODEC_VMNC, "VMware Video"),
        A("VMnc"),

    B(VLC_CODEC_FMVC, "FM Screen Capture Codec"),
        A("FMVC"),

    B(VLC_CODEC_FRAPS, "FRAPS: Realtime Video Capture"),
        A("FPS1"),
        A("fps1"),

    /* Duck TrueMotion */
    B(VLC_CODEC_TRUEMOTION1, "Duck TrueMotion v1 Video"),
        A("DUCK"),
        A("PVEZ"),
    B(VLC_CODEC_TRUEMOTION2, "Duck TrueMotion v2.0 Video"),
        A("TM20"),

    B(VLC_CODEC_QTRLE, "Apple QuickTime RLE Video"),
        A("rle "),

    B(VLC_CODEC_QDRAW, "Apple QuickDraw Video"),
        A("qdrw"),

    B(VLC_CODEC_QPEG, "QPEG Video"),
        A("QPEG"),
        A("Q1.0"),
        A("Q1.1"),

    B(VLC_CODEC_ULTI, "IBM Ultimotion Video"),
        A("ULTI"),

    B(VLC_CODEC_VIXL, "Miro/Pinnacle VideoXL Video"),
        A("VIXL"),
        A("XIXL"),
        E("PIXL", "Pinnacle VideoXL Video"),

    B(VLC_CODEC_LOCO, "LOCO Video"),
        A("LOCO"),

    B(VLC_CODEC_WNV1, "Winnov WNV1 Video"),
        A("WNV1"),
        A("YUV8"),

    B(VLC_CODEC_AASC, "Autodesc RLE Video"),
        A("AASC"),
        E("AAS4", "Autodesc RLE Video 24bit"),

    B(VLC_CODEC_INDEO2, "Indeo Video v2"),
        A("IV20"),
        E("RT21", "Indeo Video 2.1" ),

    /* Flash Screen Video */
    B(VLC_CODEC_FLASHSV, "Flash Screen Video"),
        A("FSV1"),
    B(VLC_CODEC_FLASHSV2, "Flash Screen Video 2"),
        A("FSV2"),

    B(VLC_CODEC_KMVC, "Karl Morton's Video Codec (Worms)"),
        A("KMVC"),

    B(VLC_CODEC_NUV, "Nuppel Video"),
        A("RJPG"),
        A("NUV1"),

    /* CODEC_ID_SMACKVIDEO */
    B(VLC_CODEC_SMACKVIDEO, "Smacker Video"),
        A("SMK2"),
        A("SMK4"),

    /* Chinese AVS - Untested */
    B(VLC_CODEC_CAVS, "Chinese AVS"),
        A("CAVS"),
        A("AVs2"),
        A("avs2"),

    B(VLC_CODEC_AMV, "AMV"),

    B(VLC_CODEC_BINKVIDEO, "Bink Video"),

    B(VLC_CODEC_BINKAUDIO_DCT, "Bink Audio (DCT)"),

    B(VLC_CODEC_BINKAUDIO_RDFT, "Bink Audio (RDFT)"),

    /* */
    B(VLC_CODEC_DNXHD, "DNxHD"),
        A("AVdn"),
        E("AVdh", "DNxHR"),
    B(VLC_CODEC_8BPS, "8BPS"),
        A("8BPS"),
    B(VLC_CODEC_MIMIC, "Mimic"),
        A("ML2O"),

    B(VLC_CODEC_CDG, "CD-G Video"),
        A("CDG "),

    B(VLC_CODEC_FRWU, "Forward Uncompressed" ),
        A("FRWU"),

    B(VLC_CODEC_INDEO4, "Indeo Video v4"),
        A("IV41"),
        A("iv41"),

    B(VLC_CODEC_INDEO5, "Indeo Video v5"),
        A("IV50"),
        A("iv50"),

    B(VLC_CODEC_PRORES, "Apple ProRes"),
        E("apcn", "Apple ProRes 422 Standard"),
        E("apch", "Apple ProRes 422 HQ"),
        E("apcs", "Apple ProRes 422 LT"),
        E("apco", "Apple ProRes 422 Proxy"),
        E("ap4c", "Apple ProRes 4444"),
        E("ap4h", "Apple ProRes 4444"),

    B(VLC_CODEC_ICOD, "Apple Intermediate Codec"),
        A("icod"),

    B(VLC_CODEC_G2M2, "GoTo Meeting Codec 2"),
        A("G2M2"),

    B(VLC_CODEC_G2M3, "GoTo Meeting Codec 3"),
        A("G2M3"),

    B(VLC_CODEC_G2M4, "GoTo Meeting Codec 4"),
        A("G2M4"),
        A("G2M5"),

    B(VLC_CODEC_FIC, "Mirillis FIC video"),
        A("FICV"),

    B(VLC_CODEC_TDSC, "TDSC"),

    B(VLC_CODEC_HQX, "Canopus HQX"),

    B(VLC_CODEC_HQ_HQA, "Canopus HQ"),

    B(VLC_CODEC_HAP, "Vidvox Hap"),
        A("Hap1"),
        E("Hap5", "Vidvox Hap Alpha"),
        E("HapY", "Vidvox Hap Q"),

    B(VLC_CODEC_DXV, "Resolume DXV"),
        A("DXDI"),
        E("DXD3", "Resolume DXV version 3"),

    /* */
    B(VLC_CODEC_YV12, "Planar 4:2:0 YVU"),
        A("YV12"),
        A("yv12"),
    B(VLC_CODEC_YV9,  "Planar 4:1:0 YVU"),
        A("YVU9"),
    B(VLC_CODEC_I410, "Planar 4:1:0 YUV"),
        A("I410"),
    B(VLC_CODEC_I411, "Planar 4:1:1 YUV"),
        A("I411"),
        A("Y41B"),
    B(VLC_CODEC_I420, "Planar 4:2:0 YUV"),
        A("I420"),
        A("IYUV"),
    B(VLC_CODEC_I422, "Planar 4:2:2 YUV"),
        A("I422"),
        A("Y42B"),
    B(VLC_CODEC_I440, "Planar 4:4:0 YUV"),
        A("I440"),
    B(VLC_CODEC_I444, "Planar 4:4:4 YUV"),
        A("I444"),

    B(VLC_CODEC_J420, "Planar 4:2:0 YUV full scale"),
        A("J420"),
    B(VLC_CODEC_J422, "Planar 4:2:2 YUV full scale"),
        A("J422"),
    B(VLC_CODEC_J440, "Planar 4:4:0 YUV full scale"),
        A("J440"),
    B(VLC_CODEC_J444, "Planar 4:4:4 YUV full scale"),
        A("J444"),

    B(VLC_CODEC_YUVP, "Palettized YUV with palette element Y:U:V:A"),
        A("YUVP"),

    B(VLC_CODEC_YUVA, "Planar YUV 4:4:4 Y:U:V:A"),
        A("YUVA"),
    B(VLC_CODEC_YUV420A, "Planar YUV 4:2:0 Y:U:V:A"),
        A("I40A"),
    B(VLC_CODEC_YUV422A, "Planar YUV 4:2:2 Y:U:V:A"),
        A("I42A"),
    B(VLC_CODEC_YUVA_444_10L, "Planar YUV 4:4:4 Y:U:V:A 10bits"),

    B(VLC_CODEC_RGBP, "Palettized RGB with palette element R:G:B"),
        A("RGBP"),

    B(VLC_CODEC_RGB8, "8 bits RGB"),
        A("RGB2"),
    B(VLC_CODEC_RGB12, "12 bits RGB"),
        A("RV12"),
    B(VLC_CODEC_RGB15, "15 bits RGB"),
        A("RV15"),
    B(VLC_CODEC_RGB16, "16 bits RGB"),
        A("RV16"),
    B(VLC_CODEC_RGB24, "24 bits RGB"),
        A("RV24"),
    B(VLC_CODEC_RGB32, "32 bits RGB"),
        A("RV32"),
    B(VLC_CODEC_RGBA, "32 bits RGBA"),
        A("RGBA"),
    B(VLC_CODEC_ARGB, "32 bits ARGB"),
        A("ARGB"),
        A("AV32"),
    B(VLC_CODEC_BGRA, "32 bits BGRA"),
        A("BGRA"),

    B(VLC_CODEC_GREY, "8 bits greyscale"),
        A("GREY"),
        A("Y800"),
        A("Y8  "),

    B(VLC_CODEC_UYVY, "Packed YUV 4:2:2, U:Y:V:Y"),
        A("UYVY"),
        A("UYNV"),
        A("UYNY"),
        A("Y422"),
        A("HDYC"),
        A("AVUI"),
        A("uyv1"),
        A("2vuy"),
        A("2Vuy"),
        A("2Vu1"),
    B(VLC_CODEC_VYUY, "Packed YUV 4:2:2, V:Y:U:Y"),
        A("VYUY"),
    B(VLC_CODEC_YUYV, "Packed YUV 4:2:2, Y:U:Y:V"),
        A("YUY2"),
        A("YUYV"),
        A("YUNV"),
        A("V422"),
    B(VLC_CODEC_YVYU, "Packed YUV 4:2:2, Y:V:Y:U"),
        A("YVYU"),

    B(VLC_CODEC_Y211, "Packed YUV 2:1:1, Y:U:Y:V "),
        A("Y211"),
    B(VLC_CODEC_CYUV, "Creative Packed YUV 4:2:2, U:Y:V:Y, reverted"),
        A("cyuv"),
        A("CYUV"),

    B(VLC_CODEC_V210, "10-bit 4:2:2 Component YCbCr"),
        A("v210"),

    B(VLC_CODEC_NV12, "Biplanar 4:2:0 Y/UV"),
        A("NV12"),
    B(VLC_CODEC_NV21, "Biplanar 4:2:0 Y/VU"),
        A("NV21"),
    B(VLC_CODEC_NV16, "Biplanar 4:2:2 Y/UV"),
        A("NV16"),
    B(VLC_CODEC_NV61, "Biplanar 4:2:2 Y/VU"),
        A("NV61"),
    B(VLC_CODEC_NV24, "Biplanar 4:4:4 Y/UV"),
        A("NV24"),
    B(VLC_CODEC_NV42, "Biplanar 4:4:4 Y/VU"),
        A("NV42"),

    B(VLC_CODEC_I420_9L, "Planar 4:2:0 YUV 9-bit LE"),
        A("I09L"),
    B(VLC_CODEC_I420_9B, "Planar 4:2:0 YUV 9-bit BE"),
        A("I09B"),
    B(VLC_CODEC_I422_9L, "Planar 4:2:2 YUV 9-bit LE"),
        A("I29L"),
    B(VLC_CODEC_I422_9B, "Planar 4:2:2 YUV 9-bit BE"),
        A("I29B"),
    B(VLC_CODEC_I444_9L, "Planar 4:4:4 YUV 9-bit LE"),
        A("I49L"),
    B(VLC_CODEC_I444_9B, "Planar 4:4:4 YUV 9-bit BE"),
        A("I49B"),

    B(VLC_CODEC_I420_10L, "Planar 4:2:0 YUV 10-bit LE"),
        A("I0AL"),
    B(VLC_CODEC_I420_10B, "Planar 4:2:0 YUV 10-bit BE"),
        A("I0AB"),
    B(VLC_CODEC_I422_10L, "Planar 4:2:2 YUV 10-bit LE"),
        A("I2AL"),
    B(VLC_CODEC_I422_10B, "Planar 4:2:2 YUV 10-bit BE"),
        A("I2AB"),
    B(VLC_CODEC_I444_10L, "Planar 4:4:4 YUV 10-bit LE"),
        A("I4AL"),
    B(VLC_CODEC_I444_10B, "Planar 4:4:4 YUV 10-bit BE"),
        A("I4AB"),

    B(VLC_CODEC_I420_12L, "Planar 4:2:0 YUV 12-bit LE"),
        A("I0CL"),
    B(VLC_CODEC_I420_12B, "Planar 4:2:0 YUV 12-bit BE"),
        A("I0CB"),
    B(VLC_CODEC_I422_12L, "Planar 4:2:2 YUV 12-bit LE"),
        A("I2CL"),
    B(VLC_CODEC_I422_12B, "Planar 4:2:2 YUV 12-bit BE"),
        A("I2CB"),
    B(VLC_CODEC_I444_12L, "Planar 4:4:4 YUV 12-bit LE"),
        A("I4CL"),
    B(VLC_CODEC_I444_12B, "Planar 4:4:4 YUV 12-bit BE"),
        A("I4CB"),

    B(VLC_CODEC_I420_16L, "Planar 4:2:0 YUV 16-bit LE"),
        A("I0FL"),
    B(VLC_CODEC_I420_16B, "Planar 4:2:0 YUV 16-bit BE"),
        A("I0FB"),
    B(VLC_CODEC_I444_16L, "Planar 4:4:4 YUV 16-bit LE"),
        A("I4FL"),
    B(VLC_CODEC_I444_16B, "Planar 4:4:4 YUV 16-bit BE"),
        A("I4FB"),


    /* XYZ color space */
    B(VLC_CODEC_XYZ12, "Packed XYZ 12-bit BE"),
        A("XY12"),

    /* Videogames Codecs */

    /* Interplay MVE */
    B(VLC_CODEC_INTERPLAY, "Interplay MVE Video"),
        A("imve"),
        A("INPV"),

    /* Id Quake II CIN */
    B(VLC_CODEC_IDCIN, "Id Quake II CIN Video"),
        A("IDCI"),

    /* 4X Technologies */
    B(VLC_CODEC_4XM, "4X Technologies Video"),
        A("4XMV"),
        A("4xmv"),

    /* Id RoQ */
    B(VLC_CODEC_ROQ, "Id RoQ Video"),
        A("RoQv"),

    /* Sony Playstation MDEC */
    B(VLC_CODEC_MDEC, "PSX MDEC Video"),
        A("MDEC"),

    /* Sierra VMD */
    B(VLC_CODEC_VMDVIDEO, "Sierra VMD Video"),
        A("VMDV"),
        A("vmdv"),

    B(VLC_CODEC_DIRAC, "Dirac" ),
        A("drac"),

    /* Image */
    B(VLC_CODEC_PNG, "PNG Image"),
        A("png "),

    B(VLC_CODEC_PPM, "PPM Image"),
        A("ppm "),

    B(VLC_CODEC_PGM, "PGM Image"),
        A("pgm "),

    B(VLC_CODEC_PGMYUV, "PGM YUV Image"),
        A("pgmy"),

    B(VLC_CODEC_PAM, "PAM Image"),
        A("pam "),

    B(VLC_CODEC_JPEGLS, "JPEG-LS"),
        A("MJLS"),

    B(VLC_CODEC_JPEG, "JPEG"),
        A("jpeg"),
        A("JPEG"),

    B(VLC_CODEC_BPG, "BPG Image"),
        A("BPG "),

    B(VLC_CODEC_BMP, "BMP Image"),
        A("bmp "),

    B(VLC_CODEC_TIFF, "TIFF Image"),
        A("tiff"),

    B(VLC_CODEC_GIF, "GIF Image"),
        A("gif "),


    B(VLC_CODEC_TARGA, "Truevision Targa Image"),
        A("tga "),
        A("mtga"),
        A("MTGA"),

    B(VLC_CODEC_SGI, "SGI Image"),
        A("sgi "),

    B(VLC_CODEC_SVG, "SVG Scalable Vector Graphics Image"),
        A("svg "),

    B(VLC_CODEC_PNM, "Portable Anymap Image"),
        A("pnm "),

    B(VLC_CODEC_PCX, "Personal Computer Exchange Image"),
        A("pcx "),

    B(VLC_CODEC_XWD, "X Window system raster image"),

    B(VLC_CODEC_JPEG2000, "JPEG 2000 Image"),
        A("JP2K"),
        A("mjp2"),
        A("MJP2"),
        A("MJ2C"),
        A("LJ2C"),
        A("LJ2K"),

    B(VLC_CODEC_LAGARITH, "Lagarith Lossless"),
        A("LAGS"),

    B(VLC_CODEC_MXPEG, "Mxpeg"),
        A("MXPG"),

    B(VLC_CODEC_CDXL, "Commodore CDXL video format"),
        A("CDXL"),

    B(VLC_CODEC_BMVVIDEO, "Discworld II BMV video"),
        A("BMVV"),

    B(VLC_CODEC_UTVIDEO, "Ut Video"),
        A("ULRA"),
        A("ULRG"),
        A("ULY0"),
        A("ULY2"),
        A("ULH0"),
        A("ULH2"),

    B(VLC_CODEC_VBLE, "VBLE Lossless"),
        A("VBLE"),

    B(VLC_CODEC_DXTORY, "Dxtory capture format"),
        A("xtor"),

    B(VLC_CODEC_MSS1, "Windows Media Video 7 Screen"),
        A("MSS1"),
        A("mss1"),

    B(VLC_CODEC_MSS2, "Windows Media Video 9 Screen"),
        A("MSS2"),
        A("mss2"),

    B(VLC_CODEC_MSA1, "Microsoft Application Screen Decoder 1"),
        A("MSA1"),

    B(VLC_CODEC_TSC2, "TechSmith Screen Codec 2"),
        A("tsc2"),

    B(VLC_CODEC_MTS2, "Microsoft Expression Encoder Screen"),
        A("MTS2"),

    B(VLC_CODEC_XAN_WC4, "Wing Commander IV Xan video"),
        A("Xxan"),

    B(VLC_CODEC_LCL_MSZH, "Loss-Less Codec Library AVImszh"),
        A("MSZH"),

    B(VLC_CODEC_LCL_ZLIB, "Loss-Less Codec Library AVIzlib"),
        A("ZLIB"),

    B(VLC_CODEC_THP, "GameCube THP video"),

    B(VLC_CODEC_TXD, "RenderWare TXD"),

    B(VLC_CODEC_ESCAPE124, "Escape 124 video"),

    B(VLC_CODEC_KGV1, "Kega Game Video (KGV1)"),
        A("KGV1"),

    B(VLC_CODEC_CLLC, "Canopus Lossless"),
        A("CLLC"),

    B(VLC_CODEC_AURA, "Auravision Aura"),
        A("AURA"),

    B(VLC_CODEC_TMV, "8088flex TMV"),

    B(VLC_CODEC_XAN_WC3, "Wing Commander III video"),

    B(VLC_CODEC_WS_VQA, "Westwood Studios VQA"),

    B(VLC_CODEC_MMVIDEO, "American Laser Games MM Video"),

    B(VLC_CODEC_AVS, "Creature Shock AVS"),

    B(VLC_CODEC_DSICINVIDEO, "Delphine CIN video"),

    B(VLC_CODEC_TIERTEXSEQVIDEO, "Tiertex Limited SEQ video"),

    B(VLC_CODEC_DXA, "Feeble Files/ScummVM DXA"),

    B(VLC_CODEC_C93, "Interplay C93"),

    B(VLC_CODEC_BETHSOFTVID, "Bethesda VID"),

    B(VLC_CODEC_VB, "Beam Software VB"),

    B(VLC_CODEC_RL2, "RL2 video"),

    B(VLC_CODEC_BFI, "Brute Force & Ignorance (BFI) video"),

    B(VLC_CODEC_CMV, "Electronic Arts CMV"),

    B(VLC_CODEC_MOTIONPIXELS, "Sirius Publishing Motion Pixels"),

    B(VLC_CODEC_TGV, "Electronic Arts TGV"),

    B(VLC_CODEC_TGQ, "Electronic Arts TGQ"),

    B(VLC_CODEC_TQI, "Electronic Arts TQI"),

    B(VLC_CODEC_MAD, "Electronic Arts MAD"),

    B(VLC_CODEC_ANM, "DeluxePaint animation"),

    B(VLC_CODEC_YOP, "Psygnosis YOP"),

    B(VLC_CODEC_JV, "Bitmap Brothers JV"),

    B(VLC_CODEC_DFA, "Chronomaster DFA"),

    B(VLC_CODEC_HNM4_VIDEO, "Cryo Interactive Entertainment HNM4"),

    B(VLC_CODEC_CINEFORM, "CineForm" ),

    B(VLC_CODEC_SPEEDHQ, "NewTek SpeedHQ" ),
        A("SHQ0"),
        A("SHQ1"),
        A("SHQ2"),
        A("SHQ3"),
        A("SHQ4"),
        A("SHQ5"),
        A("SHQ7"),
        A("SHQ9"),

    B(VLC_CODEC_PIXLET, "Apple Pixlet" ),
        A("pxlt"),
};

static const staticentry_t p_list_audio[] = {

    /* Windows Media Audio 1 */
    B(VLC_CODEC_WMA1, "Windows Media Audio 1"),
        A("WMA1"),
        A("wma1"),

    /* Windows Media Audio 2 */
    B(VLC_CODEC_WMA2, "Windows Media Audio 2"),
        A("WMA2"),
        A("wma2"),
        A("wma "),

    /* Windows Media Audio Professional */
    B(VLC_CODEC_WMAP, "Windows Media Audio Professional"),
        A("WMAP"),
        A("wmap"),

    /* Windows Media Audio Lossless */
    B(VLC_CODEC_WMAL, "Windows Media Audio Lossless"),
        A("WMAL"),
        A("wmal"),

    /* Windows Media Audio Speech */
    B(VLC_CODEC_WMAS, "Windows Media Audio Voice (Speech)"),
        A("WMAS"),
        A("wmas"),

    /* DV Audio */
    B(VLC_CODEC_DVAUDIO, "DV Audio"),
        A("dvau"),
        A("vdva"),
        A("dvca"),
        A("RADV"),

    /* MACE-3 Audio */
    B(VLC_CODEC_MACE3, "MACE-3 Audio"),
        A("MAC3"),

    /* MACE-6 Audio */
    B(VLC_CODEC_MACE6, "MACE-6 Audio"),
        A("MAC6"),

    /* MUSEPACK7 Audio */
    B(VLC_CODEC_MUSEPACK7, "MUSEPACK7 Audio"),
        A("MPC "),

    /* MUSEPACK8 Audio */
    B(VLC_CODEC_MUSEPACK8, "MUSEPACK8 Audio"),
        A("MPCK"),
        A("MPC8"),

    /* RealAudio 1.0 */
    B(VLC_CODEC_RA_144, "RealAudio 1.0"),
        A("14_4"),
        A("lpcJ"),

    /* RealAudio 2.0 */
    B(VLC_CODEC_RA_288, "RealAudio 2.0"),
        A("28_8"),

    B(VLC_CODEC_SIPR, "RealAudio Sipr"),
        A("sipr"),

    /* MPEG Audio layer 1/2 */
    B(VLC_CODEC_MPGA, "MPEG Audio layer 1/2"),
        A("mpga"),
        A("mp2a"),
        A(".mp1"),
        A(".mp2"),
        A("LAME"),
        A("ms\x00\x50"),
        A("ms\x00\x55"),

    /* MPEG Audio layer 3 */
    B(VLC_CODEC_MP3, "MPEG Audio layer 3"),
        A("mp3 "),
        A(".mp3"),
        A("MP3 "),

    /* A52 Audio (aka AC3) */
    B(VLC_CODEC_A52, "A52 Audio (aka AC3)"),
        A("a52 "),
        A("a52b"),
        A("ac-3"),
        A("sac3"),
        A("ms\x20\x00"),

    B(VLC_CODEC_EAC3, "A/52 B Audio (aka E-AC3)"),
        A("eac3"),

    /* DTS Audio */
    B(VLC_CODEC_DTS, "DTS Audio"),
        A("dts "),
        A("DTS "),
        A("dtsb"),
        A("dtsc"),
        E("dtse", "DTS Express"),
        E("dtsh", "DTS-HD High Resolution Audio"),
        E("dtsl", "DTS-HD Lossless"),
        A("ms\x20\x01"),

    /* AAC audio */
    B(VLC_CODEC_MP4A, "MPEG AAC Audio"),
        A("mp4a"),
        A("aac "),
        A("AACL"),
        A("AACH"),
        A("AACP"), /* smooth streaming alias */

    /* ALS audio */
    B(VLC_CODEC_ALS, "MPEG-4 Audio Lossless (ALS)"),
        A("als "),

    /* 4X Technologies */
    B(VLC_CODEC_ADPCM_4XM, "4X Technologies Audio"),
        A("4xma"),

    /* EA ADPCM */
    B(VLC_CODEC_ADPCM_EA, "EA ADPCM Audio"),
        A("ADEA"),

    /* Interplay DPCM */
    B(VLC_CODEC_INTERPLAY_DPCM, "Interplay DPCM Audio"),
        A("idpc"),

    /* Id RoQ */
    B(VLC_CODEC_ROQ_DPCM, "Id RoQ DPCM Audio"),
        A("RoQa"),

    /* DCIN Audio */
    B(VLC_CODEC_DSICINAUDIO, "Delphine CIN Audio"),
        A("DCIA"),

    /* Sony Playstation XA ADPCM */
    B(VLC_CODEC_ADPCM_XA, "PSX XA ADPCM Audio"),
        A("xa  "),

    /* ADX ADPCM */
    B(VLC_CODEC_ADPCM_ADX, "ADX ADPCM Audio"),
        A("adx "),

    /* Westwood ADPCM */
    B(VLC_CODEC_ADPCM_IMA_WS, "Westwood IMA ADPCM audio"),
        A("AIWS"),

    /* MS ADPCM */
    B(VLC_CODEC_ADPCM_MS, "MS ADPCM audio"),
        A("ms\x00\x02"),

    /* Sierra VMD */
    B(VLC_CODEC_VMDAUDIO, "Sierra VMD Audio"),
        A("vmda"),

    /* G.726 ADPCM */
    B(VLC_CODEC_ADPCM_G726, "G.726 ADPCM Audio"),
        A("g726"),

    /* G.722 ADPCM */
    B(VLC_CODEC_ADPCM_G722, "G.722 ADPCM Audio"),
        A("g722"),

    /* Flash ADPCM */
    B(VLC_CODEC_ADPCM_SWF, "Flash ADPCM Audio"),
        A("SWFa"),

    B(VLC_CODEC_ADPCM_IMA_WAV, "IMA WAV ADPCM Audio"),
        A("ms\x00\x11"),

    B(VLC_CODEC_ADPCM_IMA_AMV, "IMA AMV ADPCM Audio"),
        A("imav"),

    B(VLC_CODEC_ADPCM_IMA_QT, "IMA QT ADPCM Audio"),
        A("ima4"),

    B(VLC_CODEC_ADPCM_YAMAHA, "Yamaha ADPCM Audio" ),
        A("ms\x00\x20"),

    B(VLC_CODEC_ADPCM_DK3, "Duck DK3 ADPCM"),
        A("ms\x00\x62"),

    B(VLC_CODEC_ADPCM_DK4, "Duck DK4 ADPCM"),
        A("ms\x00\x61"),

    B(VLC_CODEC_ADPCM_THP, "GameCube THP ADPCM"),

    B(VLC_CODEC_ADPCM_XA_EA, "EA-XA ADPCM"),
        A("XAJ\x00"),

    /* AMR */
    B(VLC_CODEC_AMR_NB, "AMR narrow band"),
        A("samr"),

    B(VLC_CODEC_AMR_WB, "AMR wide band"),
        A("sawb"),

    /* FLAC */
    B(VLC_CODEC_FLAC, "FLAC (Free Lossless Audio Codec)"),
        A("flac"),

    /* ALAC */
    B(VLC_CODEC_ALAC, "Apple Lossless Audio Codec"),
        A("alac"),

    /* QDM2 */
    B(VLC_CODEC_QDM2, "QDM2 Audio"),
        A("QDM2"),

    /* QDMC */
    B(VLC_CODEC_QDMC, "QDMC Audio"),
        A("QDMC"),

    /* COOK */
    B(VLC_CODEC_COOK, "Cook Audio"),
        A("cook"),

    /* TTA: The Lossless True Audio */
    B(VLC_CODEC_TTA, "The Lossless True Audio"),
        A("TTA1"),

    /* Shorten */
    B(VLC_CODEC_SHORTEN, "Shorten Lossless Audio"),
        A("shn "),
        A("shrn"),

    B(VLC_CODEC_WAVPACK, "WavPack"),
        A("WVPK"),
        A("wvpk"),

    B(VLC_CODEC_GSM, "GSM Audio"),
        A("gsm "),

    B(VLC_CODEC_GSM_MS, "Microsoft GSM Audio"),
        A("agsm"),

    B(VLC_CODEC_ATRAC1, "atrac 1"),
        A("atr1"),

    B(VLC_CODEC_ATRAC3, "atrac 3"),
        A("atrc"),
        A("\x70\x02\x00\x00"),

    B(VLC_CODEC_ATRAC3P, "atrac 3+"),
        A("atrp"),

    B(VLC_CODEC_IMC, "IMC" ),
        A("\x01\x04\x00\x00"),

    B(VLC_CODEC_TRUESPEECH,"TrueSpeech"),
        A("\x22\x00\x00\x00"),

    B(VLC_CODEC_NELLYMOSER, "NellyMoser ASAO"),
        A("NELL"),
        A("nmos"),

    B(VLC_CODEC_APE, "Monkey's Audio"),
        A("APE "),

    B(VLC_CODEC_MLP, "MLP/TrueHD Audio"),
        A("mlp "),

    B(VLC_CODEC_TRUEHD, "TrueHD Audio"),
        A("trhd"),

    B(VLC_CODEC_QCELP, "QCELP Audio"),
        A("Qclp"),
        A("Qclq"),
        A("sqcp"),

    B(VLC_CODEC_SPEEX, "Speex Audio"),
        A("spx "),
        A("spxr"),

    B(VLC_CODEC_VORBIS, "Vorbis Audio"),
        A("vorb"),
        A("vor1"),

    B(VLC_CODEC_OPUS, "Opus Audio"),
        A("Opus"),
        A("opus"),

    B(VLC_CODEC_302M, "302M Audio"),
        A("302m"),

    B(VLC_CODEC_DVD_LPCM, "DVD LPCM Audio"),
        A("lpcm"),

    B(VLC_CODEC_DVDA_LPCM, "DVD-Audio LPCM Audio"),
        A("apcm"),

    B(VLC_CODEC_BD_LPCM, "BD LPCM Audio"),
        A("bpcm"),

    B(VLC_CODEC_SDDS, "SDDS Audio"),
        A("sdds"),
        A("sddb"),

    B(VLC_CODEC_MIDI, "MIDI Audio"),
        A("MIDI"),

    B(VLC_CODEC_RALF, "RealAudio Lossless"),
        A("LSD:"),

    /* G.723.1 */
    B(VLC_CODEC_G723_1, "G.723.1 Audio"),
        A("g72\x31"),

    /* PCM */
    B(VLC_CODEC_S8, "PCM S8"),
        A("s8  "),

    B(VLC_CODEC_U8, "PCM U8"),
        A("u8  "),

    B(VLC_CODEC_S16L, "PCM S16 LE"),
        A("s16l"),

    B(VLC_CODEC_S16L_PLANAR, "PCM S16 LE planar"),

    B(VLC_CODEC_S16B, "PCM S16 BE"),
        A("s16b"),

    B(VLC_CODEC_U16L, "PCM U16 LE"),
        A("u16l"),

    B(VLC_CODEC_U16B, "PCM U16 BE"),
        A("u16b"),

    B(VLC_CODEC_S20B, "PCM S20 BE"),

    B(VLC_CODEC_S24L, "PCM S24 LE"),
        A("s24l"),
        A("42ni"),  /* Quicktime */

    B(VLC_CODEC_S24B, "PCM S24 BE"),
        A("s24b"),
        A("in24"),  /* Quicktime */

    B(VLC_CODEC_U24L, "PCM U24 LE"),
        A("u24l"),

    B(VLC_CODEC_U24B, "PCM U24 BE"),
        A("u24b"),

    B(VLC_CODEC_S24L32, "PCM S24 in 32 LE"),

    B(VLC_CODEC_S24B32, "PCM S24 in 32 BE"),

    B(VLC_CODEC_S32L, "PCM S32 LE"),
        A("s32l"),
        A("23ni"),  /* Quicktime */

    B(VLC_CODEC_S32B, "PCM S32 BE"),
        A("s32b"),
        A("in32"),  /* Quicktime */

    B(VLC_CODEC_U32L, "PCM U32 LE"),
        A("u32l"),

    B(VLC_CODEC_U32B, "PCM U32 BE"),
        A("u32b"),

    B(VLC_CODEC_ALAW, "PCM ALAW"),
        A("alaw"),

    B(VLC_CODEC_MULAW, "PCM MU-LAW"),
        A("mlaw"),
        A("ulaw"),

    B(VLC_CODEC_DAT12, "12 bits DAT audio"),

    B(VLC_CODEC_S24DAUD, "PCM DAUD"),
        A("daud"),

    B(VLC_CODEC_F32L, "32 bits float LE"),
        A("f32l"),
        A("fl32"),

    B(VLC_CODEC_F32B, "32 bits float BE"),
        A("f32b"),

    B(VLC_CODEC_F64L, "64 bits float LE"),
        A("f64l"),

    B(VLC_CODEC_F64B, "64 bits float BE"),
        A("f64b"),

    B(VLC_CODEC_TWINVQ, "TwinVQ"),
        A("TWIN"),

    B(VLC_CODEC_BMVAUDIO, "Discworld II BMV audio"),
        A("BMVA"),

    B(VLC_CODEC_ULEAD_DV_AUDIO_NTSC, "Ulead DV audio NTSC"),
        A("ms\x02\x15"),
    B(VLC_CODEC_ULEAD_DV_AUDIO_PAL, "Ulead DV audio PAL"),
        A("ms\x02\x16"),

    B(VLC_CODEC_INDEO_AUDIO, "Indeo Audio Coder"),
        A("ms\x04\x02"),

    B(VLC_CODEC_TAK, "TAK (Tom's lossless Audio Kompressor)"),

    B(VLC_CODEC_SMACKAUDIO, "Smacker audio"),

    B(VLC_CODEC_ADPCM_IMA_EA_SEAD, "ADPCM IMA Electronic Arts SEAD"),

    B(VLC_CODEC_ADPCM_EA_R1, "ADPCM Electronic Arts R1"),

    B(VLC_CODEC_ADPCM_IMA_APC, "ADPCM APC"),
};

static const staticentry_t p_list_spu[] = {

    B(VLC_CODEC_SPU, "DVD Subtitles"),
        A("spu "),
        A("spub"),

    B(VLC_CODEC_DVBS, "DVB Subtitles"),
        A("dvbs"),

    B(VLC_CODEC_SUBT, "Text subtitles with various tags"),
        A("subt"),

    B(VLC_CODEC_XSUB, "DivX XSUB subtitles"),
        A("XSUB"),
        A("xsub"),
        A("DXSB"),
        A("DXSA"),

    B(VLC_CODEC_SSA, "SubStation Alpha subtitles"),
        A("ssa "),

    B(VLC_CODEC_TEXT, "Plain text subtitles"),
        A("TEXT"),

    B(VLC_CODEC_TELETEXT, "Teletext"),
        A("telx"),

    B(VLC_CODEC_KATE, "Kate subtitles"),
        A("kate"),

    B(VLC_CODEC_CMML, "CMML annotations/metadata"),
        A("cmml"),

    B(VLC_CODEC_ITU_T140, "ITU T.140 subtitles"),
        A("t140"),

    B(VLC_CODEC_USF, "USF subtitles"),
        A("usf "),

    B(VLC_CODEC_OGT, "OGT subtitles"),
        A("ogt "),

    B(VLC_CODEC_CVD, "CVD subtitles"),
        A("cvd "),

    B(VLC_CODEC_ARIB_A, "ARIB subtitles (A-profile)"),
        A("arba"),

    B(VLC_CODEC_ARIB_C, "ARIB subtitles (C-profile)"),
        A("arbc"),

    B(VLC_CODEC_BD_PG, "BD PGS subtitles"),
        A("bdpg"),

    B(VLC_CODEC_BD_TEXT, "BD Text subtitles"),
        A("bdtx"),

    B(VLC_CODEC_EBU_STL, "EBU STL subtitles"),
        A("STL "),

    B(VLC_CODEC_SCTE_27, "SCTE-27 subtitles"),
        A("SC27"),

    B(VLC_CODEC_CEA608,  "EIA-608 subtitles"),

    B(VLC_CODEC_TTML, "TTML subtitles"),
        A("ttml"),

    B(VLC_CODEC_WEBVTT, "WEBVTT subtitles"),
};

/*****************************************************************************
 * ac3_parse.c: ac3 parsing procedures
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 VideoLAN
 *
 * Authors: Michel Kaempf <maxx@via.ecp.fr>
 *          Aaron Holtzman <aholtzma@engr.uvic.ca>
 *          Renaud Dartus <reno@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#include "defs.h"

#include "common.h"
#include "intf_msg.h"

#include "ac3_decoder.h"
#include "ac3_internal.h"
#include "ac3_bit_stream.h"

/* Misc LUT */
static const u16 nfchans[] = { 2, 1, 2, 3, 3, 4, 4, 5 };

struct frmsize_s
{
    u16 bit_rate;
    u16 frm_size[3];
};

static const struct frmsize_s frmsizecod_tbl[] =
{
      { 32  ,{64   ,69   ,96   } },
      { 32  ,{64   ,70   ,96   } },
      { 40  ,{80   ,87   ,120  } },
      { 40  ,{80   ,88   ,120  } },
      { 48  ,{96   ,104  ,144  } },
      { 48  ,{96   ,105  ,144  } },
      { 56  ,{112  ,121  ,168  } },
      { 56  ,{112  ,122  ,168  } },
      { 64  ,{128  ,139  ,192  } },
      { 64  ,{128  ,140  ,192  } },
      { 80  ,{160  ,174  ,240  } },
      { 80  ,{160  ,175  ,240  } },
      { 96  ,{192  ,208  ,288  } },
      { 96  ,{192  ,209  ,288  } },
      { 112 ,{224  ,243  ,336  } },
      { 112 ,{224  ,244  ,336  } },
      { 128 ,{256  ,278  ,384  } },
      { 128 ,{256  ,279  ,384  } },
      { 160 ,{320  ,348  ,480  } },
      { 160 ,{320  ,349  ,480  } },
      { 192 ,{384  ,417  ,576  } },
      { 192 ,{384  ,418  ,576  } },
      { 224 ,{448  ,487  ,672  } },
      { 224 ,{448  ,488  ,672  } },
      { 256 ,{512  ,557  ,768  } },
      { 256 ,{512  ,558  ,768  } },
      { 320 ,{640  ,696  ,960  } },
      { 320 ,{640  ,697  ,960  } },
      { 384 ,{768  ,835  ,1152 } },
      { 384 ,{768  ,836  ,1152 } },
      { 448 ,{896  ,975  ,1344 } },
      { 448 ,{896  ,976  ,1344 } },
      { 512 ,{1024 ,1114 ,1536 } },
      { 512 ,{1024 ,1115 ,1536 } },
      { 576 ,{1152 ,1253 ,1728 } },
      { 576 ,{1152 ,1254 ,1728 } },
      { 640 ,{1280 ,1393 ,1920 } },
      { 640 ,{1280 ,1394 ,1920 } }};

static const int fscod_tbl[] = {48000, 44100, 32000};

/* Some internal functions */
void parse_bsi_stats (ac3dec_t * p_ac3dec);
void parse_audblk_stats (ac3dec_t * p_ac3dec);

/* Parse a syncinfo structure */
int ac3_sync_frame (ac3dec_t * p_ac3dec, ac3_sync_info_t * p_sync_info) 
{
    int buf;
    
    p_ac3dec->bit_stream.total_bits_read = 0;
    p_ac3dec->bit_stream.i_available = 0;

    /* sync word - should be 0x0b77 */
    buf =  bitstream_get(&(p_ac3dec->bit_stream),16);
    if (buf != 0x0b77)
    {
        return 1;
    }

    /* Get crc1 - we don't actually use this data though */
    bitstream_get(&(p_ac3dec->bit_stream),16);

    /* Get the sampling rate */
    p_ac3dec->syncinfo.fscod = bitstream_get(&(p_ac3dec->bit_stream),2);

    if (p_ac3dec->syncinfo.fscod >= 3)
    {
        return 1;
    }

    /* Get the frame size code */
    p_ac3dec->syncinfo.frmsizecod = bitstream_get(&(p_ac3dec->bit_stream),6);

    if (p_ac3dec->syncinfo.frmsizecod >= 38)
    {
        return 1;
    }

    p_sync_info->bit_rate = frmsizecod_tbl[p_ac3dec->syncinfo.frmsizecod].bit_rate;

    p_ac3dec->syncinfo.frame_size = frmsizecod_tbl[p_ac3dec->syncinfo.frmsizecod].frm_size[p_ac3dec->syncinfo.fscod];
    p_sync_info->frame_size = 2 * p_ac3dec->syncinfo.frame_size;

    p_sync_info->sample_rate = fscod_tbl[p_ac3dec->syncinfo.fscod];

    return 0;
}

/*
 * This routine fills a bsi struct from the AC3 stream
 */
int parse_bsi (ac3dec_t * p_ac3dec)
{
    /* Check the AC-3 version number */
    p_ac3dec->bsi.bsid = bitstream_get(&(p_ac3dec->bit_stream),5);

    if (p_ac3dec->bsi.bsid > 8)
    {
        return 1;
    }

    /* Get the audio service provided by the stream */
    p_ac3dec->bsi.bsmod = bitstream_get(&(p_ac3dec->bit_stream),3);

    /* Get the audio coding mode (ie how many channels)*/
    p_ac3dec->bsi.acmod = bitstream_get(&(p_ac3dec->bit_stream),3);
    /* Predecode the number of full bandwidth channels as we use this
     * number a lot */
    p_ac3dec->bsi.nfchans = nfchans[p_ac3dec->bsi.acmod];

    /* If it is in use, get the centre channel mix level */
    if ((p_ac3dec->bsi.acmod & 0x1) && (p_ac3dec->bsi.acmod != 0x1))
    {
        p_ac3dec->bsi.cmixlev = bitstream_get(&(p_ac3dec->bit_stream),2);
    }

    /* If it is in use, get the surround channel mix level */
    if (p_ac3dec->bsi.acmod & 0x4)
    {
        p_ac3dec->bsi.surmixlev = bitstream_get(&(p_ac3dec->bit_stream),2);
    }

    /* Get the dolby surround mode if in 2/0 mode */
    if (p_ac3dec->bsi.acmod == 0x2)
    {
        p_ac3dec->bsi.dsurmod = bitstream_get(&(p_ac3dec->bit_stream),2);
    }

    /* Is the low frequency effects channel on? */
    p_ac3dec->bsi.lfeon = bitstream_get(&(p_ac3dec->bit_stream),1);

    /* Get the dialogue normalization level */
    p_ac3dec->bsi.dialnorm = bitstream_get(&(p_ac3dec->bit_stream),5);

    /* Does compression gain exist? */
    if ((p_ac3dec->bsi.compre = bitstream_get(&(p_ac3dec->bit_stream),1)))
    {
        /* Get compression gain */
        p_ac3dec->bsi.compr = bitstream_get(&(p_ac3dec->bit_stream),8);
    }

    /* Does language code exist? */
    if ((p_ac3dec->bsi.langcode = bitstream_get(&(p_ac3dec->bit_stream),1)))
    {
        /* Get langauge code */
        p_ac3dec->bsi.langcod = bitstream_get(&(p_ac3dec->bit_stream),8);
    }

    /* Does audio production info exist? */
    if ((p_ac3dec->bsi.audprodie = bitstream_get(&(p_ac3dec->bit_stream),1)))
    {
        /* Get mix level */
        p_ac3dec->bsi.mixlevel = bitstream_get(&(p_ac3dec->bit_stream),5);

        /* Get room type */
        p_ac3dec->bsi.roomtyp = bitstream_get(&(p_ac3dec->bit_stream),2);
    }

    /* If we're in dual mono mode then get some extra info */
    if (p_ac3dec->bsi.acmod == 0)
    {
        /* Get the dialogue normalization level two */
        p_ac3dec->bsi.dialnorm2 = bitstream_get(&(p_ac3dec->bit_stream),5);

        /* Does compression gain two exist? */
        if ((p_ac3dec->bsi.compr2e = bitstream_get(&(p_ac3dec->bit_stream),1)))
        {
            /* Get compression gain two */
            p_ac3dec->bsi.compr2 = bitstream_get(&(p_ac3dec->bit_stream),8);
        }

        /* Does language code two exist? */
        if ((p_ac3dec->bsi.langcod2e = bitstream_get(&(p_ac3dec->bit_stream),1)))
        {
            /* Get langauge code two */
            p_ac3dec->bsi.langcod2 = bitstream_get(&(p_ac3dec->bit_stream),8);
        }

        /* Does audio production info two exist? */
        if ((p_ac3dec->bsi.audprodi2e = bitstream_get(&(p_ac3dec->bit_stream),1)))
        {
            /* Get mix level two */
            p_ac3dec->bsi.mixlevel2 = bitstream_get(&(p_ac3dec->bit_stream),5);

            /* Get room type two */
            p_ac3dec->bsi.roomtyp2 = bitstream_get(&(p_ac3dec->bit_stream),2);
        }
    }

    /* Get the copyright bit */
    p_ac3dec->bsi.copyrightb = bitstream_get(&(p_ac3dec->bit_stream),1);

    /* Get the original bit */
    p_ac3dec->bsi.origbs = bitstream_get(&(p_ac3dec->bit_stream),1);

    /* Does timecode one exist? */
    if ((p_ac3dec->bsi.timecod1e = bitstream_get(&(p_ac3dec->bit_stream),1)))
    {
        p_ac3dec->bsi.timecod1 = bitstream_get(&(p_ac3dec->bit_stream),14);
    }

    /* Does timecode two exist? */
    if ((p_ac3dec->bsi.timecod2e = bitstream_get(&(p_ac3dec->bit_stream),1)))
    {
        p_ac3dec->bsi.timecod2 = bitstream_get(&(p_ac3dec->bit_stream),14);
    }

    /* Does addition info exist? */
    if ((p_ac3dec->bsi.addbsie = bitstream_get(&(p_ac3dec->bit_stream),1)))
    {
        u32 i;

        /* Get how much info is there */
        p_ac3dec->bsi.addbsil = bitstream_get(&(p_ac3dec->bit_stream),6);

        /* Get the additional info */
        for (i=0;i<(p_ac3dec->bsi.addbsil + 1);i++)
        {
            p_ac3dec->bsi.addbsi[i] = bitstream_get(&(p_ac3dec->bit_stream),8);
        }
    }

#ifdef STATS
    parse_bsi_stats (p_ac3dec);
#endif

    return 0;
}

/* More pain inducing parsing */
int parse_audblk (ac3dec_t * p_ac3dec, int blknum)
{
    int i, j;

    for (i=0; i < p_ac3dec->bsi.nfchans; i++)
    {
        /* Is this channel an interleaved 256 + 256 block ? */
        p_ac3dec->audblk.blksw[i] = bitstream_get(&(p_ac3dec->bit_stream),1);
    }

    for (i=0; i < p_ac3dec->bsi.nfchans; i++)
    {
        /* Should we dither this channel? */
        p_ac3dec->audblk.dithflag[i] = bitstream_get(&(p_ac3dec->bit_stream),1);
    }

    /* Does dynamic range control exist? */
    if ((p_ac3dec->audblk.dynrnge = bitstream_get(&(p_ac3dec->bit_stream),1)))
    {
        /* Get dynamic range info */
        p_ac3dec->audblk.dynrng = bitstream_get(&(p_ac3dec->bit_stream),8);
    }

    /* If we're in dual mono mode then get the second channel DR info */
    if (p_ac3dec->bsi.acmod == 0)
    {
        /* Does dynamic range control two exist? */
        if ((p_ac3dec->audblk.dynrng2e = bitstream_get(&(p_ac3dec->bit_stream),1)))
        {
            /* Get dynamic range info */
            p_ac3dec->audblk.dynrng2 = bitstream_get(&(p_ac3dec->bit_stream),8);
        }
    }

    /* Does coupling strategy exist? */
    p_ac3dec->audblk.cplstre = bitstream_get(&(p_ac3dec->bit_stream),1);

    if ((!blknum) && (!p_ac3dec->audblk.cplstre))
    {
        return 1;
    }

    if (p_ac3dec->audblk.cplstre)
    {
        /* Is coupling turned on? */
        if ((p_ac3dec->audblk.cplinu = bitstream_get(&(p_ac3dec->bit_stream),1)))
        {
            int nb_coupled_channels;

            nb_coupled_channels = 0;
            for (i=0; i < p_ac3dec->bsi.nfchans; i++)
            {
                p_ac3dec->audblk.chincpl[i] = bitstream_get(&(p_ac3dec->bit_stream),1);
                if (p_ac3dec->audblk.chincpl[i])
                {
                    nb_coupled_channels++;
                }
            }
            if (nb_coupled_channels < 2)
            {
                return 1;
            }

            if (p_ac3dec->bsi.acmod == 0x2)
            {
                p_ac3dec->audblk.phsflginu = bitstream_get(&(p_ac3dec->bit_stream),1);
            }
            p_ac3dec->audblk.cplbegf = bitstream_get(&(p_ac3dec->bit_stream),4);
            p_ac3dec->audblk.cplendf = bitstream_get(&(p_ac3dec->bit_stream),4);

            if (p_ac3dec->audblk.cplbegf > p_ac3dec->audblk.cplendf + 2)
            {
                return 1;
            }

            p_ac3dec->audblk.ncplsubnd = (p_ac3dec->audblk.cplendf + 2) - p_ac3dec->audblk.cplbegf + 1;

            /* Calculate the start and end bins of the coupling channel */
            p_ac3dec->audblk.cplstrtmant = (p_ac3dec->audblk.cplbegf * 12) + 37 ;
            p_ac3dec->audblk.cplendmant = ((p_ac3dec->audblk.cplendf + 3) * 12) + 37;

            /* The number of combined subbands is ncplsubnd minus each combined
             * band */
            p_ac3dec->audblk.ncplbnd = p_ac3dec->audblk.ncplsubnd;

            for (i=1; i< p_ac3dec->audblk.ncplsubnd; i++)
            {
                p_ac3dec->audblk.cplbndstrc[i] = bitstream_get(&(p_ac3dec->bit_stream),1);
                p_ac3dec->audblk.ncplbnd -= p_ac3dec->audblk.cplbndstrc[i];
            }
        }
    }

    if (p_ac3dec->audblk.cplinu)
    {
        /* Loop through all the channels and get their coupling co-ords */
        for (i=0; i < p_ac3dec->bsi.nfchans;i++)
        {
            if (!p_ac3dec->audblk.chincpl[i])
            {
                continue;
            }

            /* Is there new coupling co-ordinate info? */
            p_ac3dec->audblk.cplcoe[i] = bitstream_get(&(p_ac3dec->bit_stream),1);

            if ((!blknum) && (!p_ac3dec->audblk.cplcoe[i]))
            {
                return 1;
            }

            if (p_ac3dec->audblk.cplcoe[i])
            {
                p_ac3dec->audblk.mstrcplco[i] = bitstream_get(&(p_ac3dec->bit_stream),2);
                for (j=0;j < p_ac3dec->audblk.ncplbnd; j++)
                {
                    p_ac3dec->audblk.cplcoexp[i][j] = bitstream_get(&(p_ac3dec->bit_stream),4);
                    p_ac3dec->audblk.cplcomant[i][j] = bitstream_get(&(p_ac3dec->bit_stream),4);
                }
            }
        }

        /* If we're in dual mono mode, there's going to be some phase info */
        if ((p_ac3dec->bsi.acmod == 0x2) && p_ac3dec->audblk.phsflginu &&
           (p_ac3dec->audblk.cplcoe[0] || p_ac3dec->audblk.cplcoe[1]))
        {
            for (j=0; j < p_ac3dec->audblk.ncplbnd; j++)
            {
                p_ac3dec->audblk.phsflg[j] = bitstream_get(&(p_ac3dec->bit_stream),1);
            }

        }
    }

    /* If we're in dual mono mode, there may be a rematrix strategy */
    if (p_ac3dec->bsi.acmod == 0x2)
    {
        p_ac3dec->audblk.rematstr = bitstream_get(&(p_ac3dec->bit_stream),1);

        if ((!blknum) && (!p_ac3dec->audblk.rematstr))
        {
            return 1;
        }

        if (p_ac3dec->audblk.rematstr)
        {
            if (p_ac3dec->audblk.cplinu == 0)
            {
                for (i = 0; i < 4; i++)
                {
                    p_ac3dec->audblk.rematflg[i] = bitstream_get(&(p_ac3dec->bit_stream),1);
                }
            }
            if ((p_ac3dec->audblk.cplbegf > 2) && p_ac3dec->audblk.cplinu)
            {
                for (i = 0; i < 4; i++)
                {
                    p_ac3dec->audblk.rematflg[i] = bitstream_get(&(p_ac3dec->bit_stream),1);
                }
            }
            if ((p_ac3dec->audblk.cplbegf <= 2) && p_ac3dec->audblk.cplinu)
            {
                for (i = 0; i < 3; i++)
                {
                    p_ac3dec->audblk.rematflg[i] = bitstream_get(&(p_ac3dec->bit_stream),1);
                }
            }
            if ((p_ac3dec->audblk.cplbegf == 0) && p_ac3dec->audblk.cplinu)
            {
                for (i = 0; i < 2; i++)
                {
                    p_ac3dec->audblk.rematflg[i] = bitstream_get(&(p_ac3dec->bit_stream),1);
                }
            }
        }
    }

    if (p_ac3dec->audblk.cplinu)
    {
        /* Get the coupling channel exponent strategy */
        p_ac3dec->audblk.cplexpstr = bitstream_get(&(p_ac3dec->bit_stream),2);

        if ((!blknum) && (p_ac3dec->audblk.cplexpstr == EXP_REUSE))
        {
            return 1;
        }

        if (p_ac3dec->audblk.cplexpstr==0)
        {
            p_ac3dec->audblk.ncplgrps = 0;
        }
        else
        {
            p_ac3dec->audblk.ncplgrps = (p_ac3dec->audblk.cplendmant - p_ac3dec->audblk.cplstrtmant) /
                (3 << (p_ac3dec->audblk.cplexpstr-1));
        }

    }

    for (i = 0; i < p_ac3dec->bsi.nfchans; i++)
    {
        p_ac3dec->audblk.chexpstr[i] = bitstream_get(&(p_ac3dec->bit_stream),2);

        if ((!blknum) && (p_ac3dec->audblk.chexpstr[i] == EXP_REUSE))
        {
            return 1;
        }
    }

    /* Get the exponent strategy for lfe channel */
    if (p_ac3dec->bsi.lfeon)
    {
        p_ac3dec->audblk.lfeexpstr = bitstream_get(&(p_ac3dec->bit_stream),1);

        if ((!blknum) && (p_ac3dec->audblk.lfeexpstr == EXP_REUSE))
        {
            return 1;
        }
    }

    /* Determine the bandwidths of all the fbw channels */
    for (i = 0; i < p_ac3dec->bsi.nfchans; i++)
    {
        u16 grp_size;

        if (p_ac3dec->audblk.chexpstr[i] != EXP_REUSE)
        {
            if (p_ac3dec->audblk.cplinu && p_ac3dec->audblk.chincpl[i])
            {
                p_ac3dec->audblk.endmant[i] = p_ac3dec->audblk.cplstrtmant;
            }
            else
            {
                p_ac3dec->audblk.chbwcod[i] = bitstream_get(&(p_ac3dec->bit_stream),6);

                if (p_ac3dec->audblk.chbwcod[i] > 60)
                {
                    return 1;
                }

                p_ac3dec->audblk.endmant[i] = ((p_ac3dec->audblk.chbwcod[i] + 12) * 3) + 37;
            }

            /* Calculate the number of exponent groups to fetch */
            grp_size =  3 * (1 << (p_ac3dec->audblk.chexpstr[i] - 1));
            p_ac3dec->audblk.nchgrps[i] = (p_ac3dec->audblk.endmant[i] - 1 + (grp_size - 3)) / grp_size;
        }
    }

    /* Get the coupling exponents if they exist */
    if (p_ac3dec->audblk.cplinu && (p_ac3dec->audblk.cplexpstr != EXP_REUSE))
    {
        p_ac3dec->audblk.cplabsexp = bitstream_get(&(p_ac3dec->bit_stream),4);
        for (i=0; i< p_ac3dec->audblk.ncplgrps;i++)
        {
            p_ac3dec->audblk.cplexps[i] = bitstream_get(&(p_ac3dec->bit_stream),7);

            if (p_ac3dec->audblk.cplexps[i] >= 125)
            {
                return 1;
            }
        }
    }

    /* Get the fwb channel exponents */
    for (i=0; i < p_ac3dec->bsi.nfchans; i++)
    {
        if (p_ac3dec->audblk.chexpstr[i] != EXP_REUSE)
        {
            p_ac3dec->audblk.exps[i][0] = bitstream_get(&(p_ac3dec->bit_stream),4);
            for (j=1; j<=p_ac3dec->audblk.nchgrps[i];j++)
            {
                p_ac3dec->audblk.exps[i][j] = bitstream_get(&(p_ac3dec->bit_stream),7);
                if (p_ac3dec->audblk.exps[i][j] >= 125)
                {
                    return 1;
                }
            }
            p_ac3dec->audblk.gainrng[i] = bitstream_get(&(p_ac3dec->bit_stream),2);
        }
    }

    /* Get the lfe channel exponents */
    if (p_ac3dec->bsi.lfeon && (p_ac3dec->audblk.lfeexpstr != EXP_REUSE))
    {
        p_ac3dec->audblk.lfeexps[0] = bitstream_get(&(p_ac3dec->bit_stream),4);
        p_ac3dec->audblk.lfeexps[1] = bitstream_get(&(p_ac3dec->bit_stream),7);
        if (p_ac3dec->audblk.lfeexps[1] >= 125)
        {
            return 1;
        }
        p_ac3dec->audblk.lfeexps[2] = bitstream_get(&(p_ac3dec->bit_stream),7);
        if (p_ac3dec->audblk.lfeexps[2] >= 125)
        {
            return 1;
        }
    }

    /* Get the parametric bit allocation parameters */
    p_ac3dec->audblk.baie = bitstream_get(&(p_ac3dec->bit_stream),1);

    if ((!blknum) && (!p_ac3dec->audblk.baie))
    {
        return 1;
    }

    if (p_ac3dec->audblk.baie)
    {
        p_ac3dec->audblk.sdcycod = bitstream_get(&(p_ac3dec->bit_stream),2);
        p_ac3dec->audblk.fdcycod = bitstream_get(&(p_ac3dec->bit_stream),2);
        p_ac3dec->audblk.sgaincod = bitstream_get(&(p_ac3dec->bit_stream),2);
        p_ac3dec->audblk.dbpbcod = bitstream_get(&(p_ac3dec->bit_stream),2);
        p_ac3dec->audblk.floorcod = bitstream_get(&(p_ac3dec->bit_stream),3);
    }

    /* Get the SNR off set info if it exists */
    p_ac3dec->audblk.snroffste = bitstream_get(&(p_ac3dec->bit_stream),1);
    if ((!blknum) && (!p_ac3dec->audblk.snroffste))
    {
        return 1;
    }

    if (p_ac3dec->audblk.snroffste)
    {
        p_ac3dec->audblk.csnroffst = bitstream_get(&(p_ac3dec->bit_stream),6);

        if (p_ac3dec->audblk.cplinu)
        {
            p_ac3dec->audblk.cplfsnroffst = bitstream_get(&(p_ac3dec->bit_stream),4);
            p_ac3dec->audblk.cplfgaincod = bitstream_get(&(p_ac3dec->bit_stream),3);
        }

        for (i = 0;i < p_ac3dec->bsi.nfchans; i++)
        {
            p_ac3dec->audblk.fsnroffst[i] = bitstream_get(&(p_ac3dec->bit_stream),4);
            p_ac3dec->audblk.fgaincod[i] = bitstream_get(&(p_ac3dec->bit_stream),3);
        }
        if (p_ac3dec->bsi.lfeon)
        {
            p_ac3dec->audblk.lfefsnroffst = bitstream_get(&(p_ac3dec->bit_stream),4);
            p_ac3dec->audblk.lfefgaincod = bitstream_get(&(p_ac3dec->bit_stream),3);
        }
    }

    /* Get coupling leakage info if it exists */
    if (p_ac3dec->audblk.cplinu)
    {
        p_ac3dec->audblk.cplleake = bitstream_get(&(p_ac3dec->bit_stream),1);
        if ((!blknum) && (!p_ac3dec->audblk.cplleake))
        {
            return 1;
        }

        if (p_ac3dec->audblk.cplleake)
        {
            p_ac3dec->audblk.cplfleak = bitstream_get(&(p_ac3dec->bit_stream),3);
            p_ac3dec->audblk.cplsleak = bitstream_get(&(p_ac3dec->bit_stream),3);
        }
    }

    /* Get the delta bit alloaction info */
    p_ac3dec->audblk.deltbaie = bitstream_get(&(p_ac3dec->bit_stream),1);

    if (p_ac3dec->audblk.deltbaie)
    {
        if (p_ac3dec->audblk.cplinu)
        {
            p_ac3dec->audblk.cpldeltbae = bitstream_get(&(p_ac3dec->bit_stream),2);
            if (p_ac3dec->audblk.cpldeltbae == 3)
            {
                return 1;
            }
        }

        for (i = 0;i < p_ac3dec->bsi.nfchans; i++)
        {
            p_ac3dec->audblk.deltbae[i] = bitstream_get(&(p_ac3dec->bit_stream),2);
            if (p_ac3dec->audblk.deltbae[i] == 3)
            {
                return 1;
            }
        }

        if (p_ac3dec->audblk.cplinu && (p_ac3dec->audblk.cpldeltbae == DELTA_BIT_NEW))
        {
            p_ac3dec->audblk.cpldeltnseg = bitstream_get(&(p_ac3dec->bit_stream),3);
            for (i = 0;i < p_ac3dec->audblk.cpldeltnseg + 1; i++)
            {
                p_ac3dec->audblk.cpldeltoffst[i] = bitstream_get(&(p_ac3dec->bit_stream),5);
                p_ac3dec->audblk.cpldeltlen[i] = bitstream_get(&(p_ac3dec->bit_stream),4);
                p_ac3dec->audblk.cpldeltba[i] = bitstream_get(&(p_ac3dec->bit_stream),3);
            }
        }

        for (i = 0; i < p_ac3dec->bsi.nfchans; i++)
        {
            if (p_ac3dec->audblk.deltbae[i] == DELTA_BIT_NEW)
            {
                p_ac3dec->audblk.deltnseg[i] = bitstream_get(&(p_ac3dec->bit_stream),3);
//                if (p_ac3dec->audblk.deltnseg[i] >= 8)
//                    fprintf (stderr, "parse debug: p_ac3dec->audblk.deltnseg[%i] == %i\n", i, p_ac3dec->audblk.deltnseg[i]);
                for (j = 0; j < p_ac3dec->audblk.deltnseg[i] + 1; j++)
                {
                    p_ac3dec->audblk.deltoffst[i][j] = bitstream_get(&(p_ac3dec->bit_stream),5);
                    p_ac3dec->audblk.deltlen[i][j] = bitstream_get(&(p_ac3dec->bit_stream),4);
                    p_ac3dec->audblk.deltba[i][j] = bitstream_get(&(p_ac3dec->bit_stream),3);
                }
            }
        }
    }

    /* Check to see if there's any dummy info to get */
    p_ac3dec->audblk.skiple = bitstream_get(&(p_ac3dec->bit_stream),1);

    if (p_ac3dec->audblk.skiple)
    {
        p_ac3dec->audblk.skipl = bitstream_get(&(p_ac3dec->bit_stream),9);

        for (i = 0; i < p_ac3dec->audblk.skipl ; i++)
        {
            bitstream_get(&(p_ac3dec->bit_stream),8);
        }
    }

#ifdef STATS
//    parse_audblk_stats(p_ac3dec);
#endif
    
    return 0;
}

void parse_auxdata (ac3dec_t * p_ac3dec)
{
    int i;
    int skip_length;

    skip_length = (p_ac3dec->syncinfo.frame_size * 16) - p_ac3dec->bit_stream.total_bits_read - 17 - 1;

    for (i = 0; i < skip_length; i++)
    {
        bitstream_get(&(p_ac3dec->bit_stream),1);
    }

    /* get the auxdata exists bit */
    bitstream_get(&(p_ac3dec->bit_stream),1);
    
    /* Skip the CRC reserved bit */
    bitstream_get(&(p_ac3dec->bit_stream),1);

    /* Get the crc */
    bitstream_get(&(p_ac3dec->bit_stream),16);
}

void parse_bsi_stats (ac3dec_t * p_ac3dec) /*Some stats */
{  
    struct mixlev_s
    {
   	    float clev;
        char *desc;
    };
    static const char *service_ids[8] = 
    {
        "CM","ME","VI","HI",
        "D", "C","E", "VO"
    };
/*
    static const struct mixlev_s cmixlev_tbl[4] =  
    {
        {0.707, "(-3.0 dB)"}, {0.595, "(-4.5 dB)"},
        {0.500, "(-6.0 dB)"}, {1.0,  "Invalid"}
    };
    static const struct mixlev_s smixlev_tbl[4] =  
    {
        {0.707, "(-3.0 dB)"}, {0.500, "(-6.0 dB)"},
        {  0.0,   "off    "}, {  1.0, "Invalid"}
    };
 */
    
    static int  i;
    
    if ( !i )
    {
/*     	if ((p_ac3dec->bsi.acmod & 0x1) && (p_ac3dec->bsi.acmod != 0x1))
       		printf("CentreMixLevel %s ",cmixlev_tbl[p_ac3dec->bsi.cmixlev].desc);
       	if (p_ac3dec->bsi.acmod & 0x4)
       		printf("SurMixLevel %s",smixlev_tbl[p_ac3dec->bsi.cmixlev].desc);
 */
        intf_Msg ( "(ac3dec_parsebsi) %s %d.%d Mode",
                service_ids[p_ac3dec->bsi.bsmod],
                p_ac3dec->bsi.nfchans,p_ac3dec->bsi.lfeon);
    }
    i++;
    
    if ( i > 100 )
        i = 0;
}

void parse_audblk_stats (ac3dec_t * p_ac3dec)
{
    char *exp_strat_tbl[4] = {"R   ","D15 ","D25 ","D45 "};
    u32 i;

	intf_ErrMsg ("(ac3dec_parseaudblk) ");
	intf_ErrMsg ("%s ",p_ac3dec->audblk.cplinu ? "cpl on" : "cpl off");
	intf_ErrMsg ("%s ",p_ac3dec->audblk.baie? "bai" : " ");
	intf_ErrMsg ("%s ",p_ac3dec->audblk.snroffste? "snroffst" : " ");
	intf_ErrMsg ("%s ",p_ac3dec->audblk.deltbaie? "deltba" : " ");
	intf_ErrMsg ("%s ",p_ac3dec->audblk.phsflginu? "phsflg" : " ");
	intf_ErrMsg ("(%s %s %s %s %s) ",exp_strat_tbl[p_ac3dec->audblk.chexpstr[0]],
		exp_strat_tbl[p_ac3dec->audblk.chexpstr[1]],exp_strat_tbl[p_ac3dec->audblk.chexpstr[2]],
		exp_strat_tbl[p_ac3dec->audblk.chexpstr[3]],exp_strat_tbl[p_ac3dec->audblk.chexpstr[4]]);
	intf_ErrMsg ("[");
	for(i=0;i<p_ac3dec->bsi.nfchans;i++)
		intf_ErrMsg ("%1d",p_ac3dec->audblk.blksw[i]);
	intf_ErrMsg ("]");

	intf_ErrMsg ("\n");
}

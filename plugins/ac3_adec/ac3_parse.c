/*****************************************************************************
 * ac3_parse.c: ac3 parsing procedures
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: ac3_parse.c,v 1.4 2001/12/09 17:01:36 sam Exp $
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include <string.h>                                              /* memset() */

#include "common.h"
#include "intf_msg.h"
#include "threads.h"
#include "mtime.h"

#include "audio_output.h"

#include "modules.h"
#include "modules_export.h"

#include "stream_control.h"
#include "input_ext-dec.h"


#include "ac3_imdct.h"
#include "ac3_downmix.h"
#include "ac3_decoder.h"
#include "ac3_adec.h"                                     /* ac3dec_thread_t */

#include "ac3_internal.h"                                       /* EXP_REUSE */

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
#ifdef TRACE
static void parse_bsi_stats (ac3dec_t * p_ac3dec);
static void parse_audblk_stats (ac3dec_t * p_ac3dec);
#endif

/* Parse a syncinfo structure */
int ac3_sync_frame (ac3dec_t * p_ac3dec, ac3_sync_info_t * p_sync_info) 
{
    ac3dec_thread_t * p_ac3dec_t = (ac3dec_thread_t *) p_ac3dec->bit_stream.p_callback_arg;
    
    p_ac3dec->total_bits_read = 0;
    p_ac3dec->i_available = 0;
    
    /* sync word - should be 0x0b77 */
    RealignBits(&p_ac3dec->bit_stream);
    while( (ShowBits (&p_ac3dec->bit_stream,16)) != 0x0b77 && 
            (!p_ac3dec_t->p_fifo->b_die) && (!p_ac3dec_t->p_fifo->b_error))
    {
        RemoveBits (&p_ac3dec->bit_stream,8);
        p_ac3dec->total_bits_read += 8;
    }
    RemoveBits (&p_ac3dec->bit_stream,16);
    p_ac3dec->total_bits_read += 16;

    
    /* Get crc1 - we don't actually use this data though */
    GetBits (&p_ac3dec->bit_stream,16);

    /* Get the sampling rate */
    p_ac3dec->syncinfo.fscod = GetBits (&p_ac3dec->bit_stream,2);

    if (p_ac3dec->syncinfo.fscod >= 3)
    {
        p_ac3dec->total_bits_read += 34;
        return 1;
    }

    /* Get the frame size code */
    p_ac3dec->syncinfo.frmsizecod = GetBits (&p_ac3dec->bit_stream,6);
    p_ac3dec->total_bits_read += 40;

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
    p_ac3dec->bsi.bsid = GetBits (&p_ac3dec->bit_stream,5);

    if (p_ac3dec->bsi.bsid > 8)
    {
        return 1;
    }

    /* Get the audio service provided by the stream */
    p_ac3dec->bsi.bsmod = GetBits (&p_ac3dec->bit_stream,3);

    /* Get the audio coding mode (ie how many channels)*/
    p_ac3dec->bsi.acmod = GetBits (&p_ac3dec->bit_stream,3);
    
    /* Predecode the number of full bandwidth channels as we use this
     * number a lot */
    p_ac3dec->bsi.nfchans = nfchans[p_ac3dec->bsi.acmod];

    /* If it is in use, get the centre channel mix level */
    if ((p_ac3dec->bsi.acmod & 0x1) && (p_ac3dec->bsi.acmod != 0x1))
    {
        p_ac3dec->bsi.cmixlev = GetBits (&p_ac3dec->bit_stream,2);
        p_ac3dec->total_bits_read += 2;
    }

    /* If it is in use, get the surround channel mix level */
    if (p_ac3dec->bsi.acmod & 0x4)
    {
        p_ac3dec->bsi.surmixlev = GetBits (&p_ac3dec->bit_stream,2);
        p_ac3dec->total_bits_read += 2;
    }

    /* Get the dolby surround mode if in 2/0 mode */
    if (p_ac3dec->bsi.acmod == 0x2)
    {
        p_ac3dec->bsi.dsurmod = GetBits (&p_ac3dec->bit_stream,2);
        p_ac3dec->total_bits_read += 2;
    }

    /* Is the low frequency effects channel on? */
    p_ac3dec->bsi.lfeon = GetBits (&p_ac3dec->bit_stream,1);

    /* Get the dialogue normalization level */
    p_ac3dec->bsi.dialnorm = GetBits (&p_ac3dec->bit_stream,5);

    /* Does compression gain exist? */
    if ((p_ac3dec->bsi.compre = GetBits (&p_ac3dec->bit_stream,1)))
    {
        /* Get compression gain */
        p_ac3dec->bsi.compr = GetBits (&p_ac3dec->bit_stream,8);
        p_ac3dec->total_bits_read += 8;
    }

    /* Does language code exist? */
    if ((p_ac3dec->bsi.langcode = GetBits (&p_ac3dec->bit_stream,1)))
    {
        /* Get langauge code */
        p_ac3dec->bsi.langcod = GetBits (&p_ac3dec->bit_stream,8);
        p_ac3dec->total_bits_read += 8;
    }

    /* Does audio production info exist? */
    if ((p_ac3dec->bsi.audprodie = GetBits (&p_ac3dec->bit_stream,1)))
    {
        /* Get mix level */
        p_ac3dec->bsi.mixlevel = GetBits (&p_ac3dec->bit_stream,5);

        /* Get room type */
        p_ac3dec->bsi.roomtyp = GetBits (&p_ac3dec->bit_stream,2);
        p_ac3dec->total_bits_read += 7;
    }

    /* If we're in dual mono mode then get some extra info */
    if (p_ac3dec->bsi.acmod == 0)
    {
        /* Get the dialogue normalization level two */
        p_ac3dec->bsi.dialnorm2 = GetBits (&p_ac3dec->bit_stream,5);

        /* Does compression gain two exist? */
        if ((p_ac3dec->bsi.compr2e = GetBits (&p_ac3dec->bit_stream,1)))
        {
            /* Get compression gain two */
            p_ac3dec->bsi.compr2 = GetBits (&p_ac3dec->bit_stream,8);
            p_ac3dec->total_bits_read += 8;
        }

        /* Does language code two exist? */
        if ((p_ac3dec->bsi.langcod2e = GetBits (&p_ac3dec->bit_stream,1)))
        {
            /* Get langauge code two */
            p_ac3dec->bsi.langcod2 = GetBits (&p_ac3dec->bit_stream,8);
            p_ac3dec->total_bits_read += 8;
        }

        /* Does audio production info two exist? */
        if ((p_ac3dec->bsi.audprodi2e = GetBits (&p_ac3dec->bit_stream,1)))
        {
            /* Get mix level two */
            p_ac3dec->bsi.mixlevel2 = GetBits (&p_ac3dec->bit_stream,5);

            /* Get room type two */
            p_ac3dec->bsi.roomtyp2 = GetBits (&p_ac3dec->bit_stream,2);
            p_ac3dec->total_bits_read += 7;
        }
        p_ac3dec->total_bits_read += 8;
    }

    /* Get the copyright bit */
    p_ac3dec->bsi.copyrightb = GetBits (&p_ac3dec->bit_stream,1);

    /* Get the original bit */
    p_ac3dec->bsi.origbs = GetBits (&p_ac3dec->bit_stream,1);

    /* Does timecode one exist? */
    if ((p_ac3dec->bsi.timecod1e = GetBits (&p_ac3dec->bit_stream,1)))
    {
        p_ac3dec->bsi.timecod1 = GetBits (&p_ac3dec->bit_stream,14);
        p_ac3dec->total_bits_read += 14;
    }

    /* Does timecode two exist? */
    if ((p_ac3dec->bsi.timecod2e = GetBits (&p_ac3dec->bit_stream,1)))
    {
        p_ac3dec->bsi.timecod2 = GetBits (&p_ac3dec->bit_stream,14);
        p_ac3dec->total_bits_read += 14;
    }

    /* Does addition info exist? */
    if ((p_ac3dec->bsi.addbsie = GetBits (&p_ac3dec->bit_stream,1)))
    {
        u32 i;

        /* Get how much info is there */
        p_ac3dec->bsi.addbsil = GetBits (&p_ac3dec->bit_stream,6);

        /* Get the additional info */
        for (i=0;i<(p_ac3dec->bsi.addbsil + 1);i++)
        {
            p_ac3dec->bsi.addbsi[i] = GetBits (&p_ac3dec->bit_stream,8);
        }
        p_ac3dec->total_bits_read += 6 + 8 * (p_ac3dec->bsi.addbsil + 1);
    }
    p_ac3dec->total_bits_read += 25;
    
#ifdef TRACE
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
        p_ac3dec->audblk.blksw[i] = GetBits (&p_ac3dec->bit_stream,1);
    }

    for (i=0; i < p_ac3dec->bsi.nfchans; i++)
    {
        /* Should we dither this channel? */
        p_ac3dec->audblk.dithflag[i] = GetBits (&p_ac3dec->bit_stream,1);
    }

    /* Does dynamic range control exist? */
    if ((p_ac3dec->audblk.dynrnge = GetBits (&p_ac3dec->bit_stream,1)))
    {
        /* Get dynamic range info */
        p_ac3dec->audblk.dynrng = GetBits (&p_ac3dec->bit_stream,8);
        p_ac3dec->total_bits_read += 8;
    }

    /* If we're in dual mono mode then get the second channel DR info */
    if (p_ac3dec->bsi.acmod == 0)
    {
        /* Does dynamic range control two exist? */
        if ((p_ac3dec->audblk.dynrng2e = GetBits (&p_ac3dec->bit_stream,1)))
        {
            /* Get dynamic range info */
            p_ac3dec->audblk.dynrng2 = GetBits (&p_ac3dec->bit_stream,8);
            p_ac3dec->total_bits_read += 8;
        }
        p_ac3dec->total_bits_read += 1;
    }

    /* Does coupling strategy exist? */
    p_ac3dec->audblk.cplstre = GetBits (&p_ac3dec->bit_stream,1);
    p_ac3dec->total_bits_read += 2 + 2 * p_ac3dec->bsi.nfchans;

    if ((!blknum) && (!p_ac3dec->audblk.cplstre))
    {
        return 1;
    }

    if (p_ac3dec->audblk.cplstre)
    {
        /* Is coupling turned on? */
        if ((p_ac3dec->audblk.cplinu = GetBits (&p_ac3dec->bit_stream,1)))
        {
            int nb_coupled_channels;

            nb_coupled_channels = 0;
            for (i=0; i < p_ac3dec->bsi.nfchans; i++)
            {
                p_ac3dec->audblk.chincpl[i] = GetBits (&p_ac3dec->bit_stream,1);
                if (p_ac3dec->audblk.chincpl[i])
                {
                    nb_coupled_channels++;
                }
            }
            p_ac3dec->total_bits_read += p_ac3dec->bsi.nfchans;
            
            if (nb_coupled_channels < 2)
            {
                return 1;
            }

            if (p_ac3dec->bsi.acmod == 0x2)
            {
                p_ac3dec->audblk.phsflginu = GetBits (&p_ac3dec->bit_stream,1);
                p_ac3dec->total_bits_read += 1;
            }
            p_ac3dec->audblk.cplbegf = GetBits (&p_ac3dec->bit_stream,4);
            p_ac3dec->audblk.cplendf = GetBits (&p_ac3dec->bit_stream,4);
            p_ac3dec->total_bits_read += 8;

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
                p_ac3dec->audblk.cplbndstrc[i] = GetBits (&p_ac3dec->bit_stream,1);
                p_ac3dec->audblk.ncplbnd -= p_ac3dec->audblk.cplbndstrc[i];
            }
            p_ac3dec->total_bits_read += p_ac3dec->audblk.ncplsubnd - 1;
        }
        p_ac3dec->total_bits_read += 1;
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
            p_ac3dec->audblk.cplcoe[i] = GetBits (&p_ac3dec->bit_stream,1);

            if ((!blknum) && (!p_ac3dec->audblk.cplcoe[i]))
            {
                return 1;
            }

            if (p_ac3dec->audblk.cplcoe[i])
            {
                p_ac3dec->audblk.mstrcplco[i] = GetBits (&p_ac3dec->bit_stream,2);
                p_ac3dec->total_bits_read += 2;
                for (j=0;j < p_ac3dec->audblk.ncplbnd; j++)
                {
                    p_ac3dec->audblk.cplcoexp[i][j] = GetBits (&p_ac3dec->bit_stream,4);
                    p_ac3dec->audblk.cplcomant[i][j] = GetBits (&p_ac3dec->bit_stream,4);
                }
                p_ac3dec->total_bits_read += 8 * p_ac3dec->audblk.ncplbnd;
            }
        }
        p_ac3dec->total_bits_read += p_ac3dec->bsi.nfchans;

        /* If we're in dual mono mode, there's going to be some phase info */
        if ((p_ac3dec->bsi.acmod == 0x2) && p_ac3dec->audblk.phsflginu &&
           (p_ac3dec->audblk.cplcoe[0] || p_ac3dec->audblk.cplcoe[1]))
        {
            for (j=0; j < p_ac3dec->audblk.ncplbnd; j++)
            {
                p_ac3dec->audblk.phsflg[j] = GetBits (&p_ac3dec->bit_stream,1);
            }
            p_ac3dec->total_bits_read += p_ac3dec->audblk.ncplbnd;

        }
    }

    /* If we're in dual mono mode, there may be a rematrix strategy */
    if (p_ac3dec->bsi.acmod == 0x2)
    {
        p_ac3dec->audblk.rematstr = GetBits (&p_ac3dec->bit_stream,1);
        p_ac3dec->total_bits_read += 1;

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
                    p_ac3dec->audblk.rematflg[i] = GetBits (&p_ac3dec->bit_stream,1);
                }
                p_ac3dec->total_bits_read += 4;
            }
            if ((p_ac3dec->audblk.cplbegf > 2) && p_ac3dec->audblk.cplinu)
            {
                for (i = 0; i < 4; i++)
                {
                    p_ac3dec->audblk.rematflg[i] = GetBits (&p_ac3dec->bit_stream,1);
                }
                p_ac3dec->total_bits_read += 4;
            }
            if ((p_ac3dec->audblk.cplbegf <= 2) && p_ac3dec->audblk.cplinu)
            {
                for (i = 0; i < 3; i++)
                {
                    p_ac3dec->audblk.rematflg[i] = GetBits (&p_ac3dec->bit_stream,1);
                }
                p_ac3dec->total_bits_read += 3;
            }
            if ((p_ac3dec->audblk.cplbegf == 0) && p_ac3dec->audblk.cplinu)
            {
                for (i = 0; i < 2; i++)
                {
                    p_ac3dec->audblk.rematflg[i] = GetBits (&p_ac3dec->bit_stream,1);
                }
                p_ac3dec->total_bits_read += 2;
            }
        }
    }

    if (p_ac3dec->audblk.cplinu)
    {
        /* Get the coupling channel exponent strategy */
        p_ac3dec->audblk.cplexpstr = GetBits (&p_ac3dec->bit_stream,2);
        p_ac3dec->total_bits_read += 2;

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
        p_ac3dec->audblk.chexpstr[i] = GetBits (&p_ac3dec->bit_stream,2);
        p_ac3dec->total_bits_read += 2;

        if ((!blknum) && (p_ac3dec->audblk.chexpstr[i] == EXP_REUSE))
        {
            return 1;
        }
    }

    /* Get the exponent strategy for lfe channel */
    if (p_ac3dec->bsi.lfeon)
    {
        p_ac3dec->audblk.lfeexpstr = GetBits (&p_ac3dec->bit_stream,1);
        p_ac3dec->total_bits_read += 1;

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
                p_ac3dec->audblk.chbwcod[i] = GetBits (&p_ac3dec->bit_stream,6);
                p_ac3dec->total_bits_read += 6;

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
        p_ac3dec->audblk.cplabsexp = GetBits (&p_ac3dec->bit_stream,4);
        p_ac3dec->total_bits_read += 4;
        for (i=0; i< p_ac3dec->audblk.ncplgrps;i++)
        {
            p_ac3dec->audblk.cplexps[i] = GetBits (&p_ac3dec->bit_stream,7);
            p_ac3dec->total_bits_read += 7;

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
            p_ac3dec->audblk.exps[i][0] = GetBits (&p_ac3dec->bit_stream,4);
            p_ac3dec->total_bits_read += 4;
            for (j=1; j<=p_ac3dec->audblk.nchgrps[i];j++)
            {
                p_ac3dec->audblk.exps[i][j] = GetBits (&p_ac3dec->bit_stream,7);
                p_ac3dec->total_bits_read += 7;
                if (p_ac3dec->audblk.exps[i][j] >= 125)
                {
                    return 1;
                }
            }
            p_ac3dec->audblk.gainrng[i] = GetBits (&p_ac3dec->bit_stream,2);
            p_ac3dec->total_bits_read += 2;
        }
    }

    /* Get the lfe channel exponents */
    if (p_ac3dec->bsi.lfeon && (p_ac3dec->audblk.lfeexpstr != EXP_REUSE))
    {
        p_ac3dec->audblk.lfeexps[0] = GetBits (&p_ac3dec->bit_stream,4);
        p_ac3dec->audblk.lfeexps[1] = GetBits (&p_ac3dec->bit_stream,7);
        p_ac3dec->total_bits_read += 11;
        if (p_ac3dec->audblk.lfeexps[1] >= 125)
        {
            return 1;
        }
        p_ac3dec->audblk.lfeexps[2] = GetBits (&p_ac3dec->bit_stream,7);
        p_ac3dec->total_bits_read += 7;
        if (p_ac3dec->audblk.lfeexps[2] >= 125)
        {
            return 1;
        }
    }

    /* Get the parametric bit allocation parameters */
    p_ac3dec->audblk.baie = GetBits (&p_ac3dec->bit_stream,1);
    p_ac3dec->total_bits_read += 1;

    if ((!blknum) && (!p_ac3dec->audblk.baie))
    {
        return 1;
    }

    if (p_ac3dec->audblk.baie)
    {
        p_ac3dec->audblk.sdcycod = GetBits (&p_ac3dec->bit_stream,2);
        p_ac3dec->audblk.fdcycod = GetBits (&p_ac3dec->bit_stream,2);
        p_ac3dec->audblk.sgaincod = GetBits (&p_ac3dec->bit_stream,2);
        p_ac3dec->audblk.dbpbcod = GetBits (&p_ac3dec->bit_stream,2);
        p_ac3dec->audblk.floorcod = GetBits (&p_ac3dec->bit_stream,3);
        p_ac3dec->total_bits_read += 11;
    }

    /* Get the SNR off set info if it exists */
    p_ac3dec->audblk.snroffste = GetBits (&p_ac3dec->bit_stream,1);
    if ((!blknum) && (!p_ac3dec->audblk.snroffste))
    {
        return 1;
    }

    if (p_ac3dec->audblk.snroffste)
    {
        p_ac3dec->audblk.csnroffst = GetBits (&p_ac3dec->bit_stream,6);
        p_ac3dec->total_bits_read += 6;

        if (p_ac3dec->audblk.cplinu)
        {
            p_ac3dec->audblk.cplfsnroffst = GetBits (&p_ac3dec->bit_stream,4);
            p_ac3dec->audblk.cplfgaincod = GetBits (&p_ac3dec->bit_stream,3);
            p_ac3dec->total_bits_read += 7;
        }

        for (i = 0;i < p_ac3dec->bsi.nfchans; i++)
        {
            p_ac3dec->audblk.fsnroffst[i] = GetBits (&p_ac3dec->bit_stream,4);
            p_ac3dec->audblk.fgaincod[i] = GetBits (&p_ac3dec->bit_stream,3);
        }
        p_ac3dec->total_bits_read += 7 * p_ac3dec->bsi.nfchans;
        if (p_ac3dec->bsi.lfeon)
        {
            p_ac3dec->audblk.lfefsnroffst = GetBits (&p_ac3dec->bit_stream,4);
            p_ac3dec->audblk.lfefgaincod = GetBits (&p_ac3dec->bit_stream,3);
            p_ac3dec->total_bits_read += 7;
        }
    }

    /* Get coupling leakage info if it exists */
    if (p_ac3dec->audblk.cplinu)
    {
        p_ac3dec->audblk.cplleake = GetBits (&p_ac3dec->bit_stream,1);
        p_ac3dec->total_bits_read += 1;
        if ((!blknum) && (!p_ac3dec->audblk.cplleake))
        {
            return 1;
        }

        if (p_ac3dec->audblk.cplleake)
        {
            p_ac3dec->audblk.cplfleak = GetBits (&p_ac3dec->bit_stream,3);
            p_ac3dec->audblk.cplsleak = GetBits (&p_ac3dec->bit_stream,3);
            p_ac3dec->total_bits_read += 6;
        }
    }

    /* Get the delta bit alloaction info */
    p_ac3dec->audblk.deltbaie = GetBits (&p_ac3dec->bit_stream,1);
    p_ac3dec->total_bits_read += 1;

    if (p_ac3dec->audblk.deltbaie)
    {
        if (p_ac3dec->audblk.cplinu)
        {
            p_ac3dec->audblk.cpldeltbae = GetBits (&p_ac3dec->bit_stream,2);
            p_ac3dec->total_bits_read += 2;
            if (p_ac3dec->audblk.cpldeltbae == 3)
            {
                return 1;
            }
        }

        for (i = 0;i < p_ac3dec->bsi.nfchans; i++)
        {
            p_ac3dec->audblk.deltbae[i] = GetBits (&p_ac3dec->bit_stream,2);
            p_ac3dec->total_bits_read += 2;
            if (p_ac3dec->audblk.deltbae[i] == 3)
            {
                return 1;
            }
        }

        if (p_ac3dec->audblk.cplinu && (p_ac3dec->audblk.cpldeltbae == DELTA_BIT_NEW))
        {
            p_ac3dec->audblk.cpldeltnseg = GetBits (&p_ac3dec->bit_stream,3);
            for (i = 0;i < p_ac3dec->audblk.cpldeltnseg + 1; i++)
            {
                p_ac3dec->audblk.cpldeltoffst[i] = GetBits (&p_ac3dec->bit_stream,5);
                p_ac3dec->audblk.cpldeltlen[i] = GetBits (&p_ac3dec->bit_stream,4);
                p_ac3dec->audblk.cpldeltba[i] = GetBits (&p_ac3dec->bit_stream,3);
            }
            p_ac3dec->total_bits_read += 12 * (p_ac3dec->audblk.cpldeltnseg + 1) + 3;
        }

        for (i = 0; i < p_ac3dec->bsi.nfchans; i++)
        {
            if (p_ac3dec->audblk.deltbae[i] == DELTA_BIT_NEW)
            {
                p_ac3dec->audblk.deltnseg[i] = GetBits (&p_ac3dec->bit_stream,3);
//                if (p_ac3dec->audblk.deltnseg[i] >= 8)
//                    fprintf (stderr, "parse debug: p_ac3dec->audblk.deltnseg[%i] == %i\n", i, p_ac3dec->audblk.deltnseg[i]);
                for (j = 0; j < p_ac3dec->audblk.deltnseg[i] + 1; j++)
                {
                    p_ac3dec->audblk.deltoffst[i][j] = GetBits (&p_ac3dec->bit_stream,5);
                    p_ac3dec->audblk.deltlen[i][j] = GetBits (&p_ac3dec->bit_stream,4);
                    p_ac3dec->audblk.deltba[i][j] = GetBits (&p_ac3dec->bit_stream,3);
                }
                p_ac3dec->total_bits_read += 12 * (p_ac3dec->audblk.deltnseg[i] + 1) + 3;
            }
        }
    }

    /* Check to see if there's any dummy info to get */
    p_ac3dec->audblk.skiple = GetBits (&p_ac3dec->bit_stream,1);
    p_ac3dec->total_bits_read += 1;

    if (p_ac3dec->audblk.skiple)
    {
        p_ac3dec->audblk.skipl = GetBits (&p_ac3dec->bit_stream,9);

        for (i = 0; i < p_ac3dec->audblk.skipl ; i++)
        {
            GetBits (&p_ac3dec->bit_stream,8);
        }
        p_ac3dec->total_bits_read += 8 * p_ac3dec->audblk.skipl + 9;
    }
    
#ifdef TRACE
    parse_audblk_stats(p_ac3dec);
#endif
    
    return 0;
}

void parse_auxdata (ac3dec_t * p_ac3dec)
{
    int i;
    int skip_length;

    skip_length = (p_ac3dec->syncinfo.frame_size * 16) - p_ac3dec->total_bits_read - 17 - 1;

    for (i = 0; i < skip_length; i++)
    {
        RemoveBits (&p_ac3dec->bit_stream,1);
    }

    /* get the auxdata exists bit */
    RemoveBits (&p_ac3dec->bit_stream,1);
    
    /* Skip the CRC reserved bit */
    RemoveBits (&p_ac3dec->bit_stream,1);

    /* Get the crc */
    RemoveBits (&p_ac3dec->bit_stream,16);
}

#ifdef TRACE
static void parse_bsi_stats (ac3dec_t * p_ac3dec) /* Some stats */
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
    
    static int  i=0;
    
    if ( !i )
    {
/*      if ((p_ac3dec->bsi.acmod & 0x1) && (p_ac3dec->bsi.acmod != 0x1))
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

static void parse_audblk_stats (ac3dec_t * p_ac3dec)
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
}
#endif


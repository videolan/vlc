#include "int_types.h"
#include "ac3_decoder.h"
#include "ac3_parse.h"
#include "ac3_bit_stream.h"

/* Misc LUT */
static u16 nfchans[] = { 2, 1, 2, 3, 3, 4, 4, 5 };

struct frmsize_s
{
    u16 bit_rate;
    u16 frm_size[3];
};

static struct frmsize_s frmsizecod_tbl[] = {
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

/* Look for a sync word */
int ac3_test_sync (ac3dec_t * p_ac3dec)
{
    NeedBits( &(p_ac3dec->bit_stream), 16 );
    if ( (p_ac3dec->bit_stream.buffer >> (32 - 16)) == 0x0b77 )
    {
	p_ac3dec->bit_stream.total_bits_read = 0;
	DumpBits( &(p_ac3dec->bit_stream), 16 );
	return 0;
    }
    DumpBits( &(p_ac3dec->bit_stream), 1 );
    return 1;
}

/* Parse a syncinfo structure, minus the sync word */
void parse_syncinfo( ac3dec_t * p_ac3dec )
{
    /* Get crc1 - we don't actually use this data though */
    NeedBits( &(p_ac3dec->bit_stream), 16 );
    DumpBits( &(p_ac3dec->bit_stream), 16 );

    /* Get the sampling rate */
    NeedBits( &(p_ac3dec->bit_stream), 2 );
    p_ac3dec->syncinfo.fscod = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 2));
//    fprintf( stderr, "parse debug: fscod == %i\n", p_ac3dec->syncinfo.fscod );
    DumpBits( &(p_ac3dec->bit_stream), 2 );

    /* Get the frame size code */
    NeedBits( &(p_ac3dec->bit_stream), 6 );
    p_ac3dec->syncinfo.frmsizecod = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 6));
//    fprintf( stderr, "parse debug: frmsizecod == %i\n", p_ac3dec->syncinfo.frmsizecod );
    DumpBits( &(p_ac3dec->bit_stream), 6 );

    p_ac3dec->syncinfo.bit_rate = frmsizecod_tbl[p_ac3dec->syncinfo.frmsizecod].bit_rate;
//    fprintf( stderr, "parse debug: bit_rate == %i\n", p_ac3dec->syncinfo.bit_rate );
    p_ac3dec->syncinfo.frame_size = frmsizecod_tbl[p_ac3dec->syncinfo.frmsizecod].frm_size[p_ac3dec->syncinfo.fscod];
//    fprintf( stderr, "parse debug: frame_size == %i\n", p_ac3dec->syncinfo.frame_size );
}

/*
 * This routine fills a bsi struct from the AC3 stream
 */
void parse_bsi( ac3dec_t * p_ac3dec )
{
    u32 i;

    /* Check the AC-3 version number */
    NeedBits( &(p_ac3dec->bit_stream), 5 );
    p_ac3dec->bsi.bsid = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 5));
    DumpBits( &(p_ac3dec->bit_stream), 5 );

    /* Get the audio service provided by the steram */
    NeedBits( &(p_ac3dec->bit_stream), 3 );
    p_ac3dec->bsi.bsmod = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 3));
    DumpBits( &(p_ac3dec->bit_stream), 3 );

    /* Get the audio coding mode (ie how many channels)*/
    NeedBits( &(p_ac3dec->bit_stream), 3 );
    p_ac3dec->bsi.acmod = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 3));
    DumpBits( &(p_ac3dec->bit_stream), 3 );
    /* Predecode the number of full bandwidth channels as we use this
     * number a lot */
    p_ac3dec->bsi.nfchans = nfchans[p_ac3dec->bsi.acmod];

    /* If it is in use, get the centre channel mix level */
    if ((p_ac3dec->bsi.acmod & 0x1) && (p_ac3dec->bsi.acmod != 0x1))
    {
        NeedBits( &(p_ac3dec->bit_stream), 2 );
        p_ac3dec->bsi.cmixlev = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 2));
        DumpBits( &(p_ac3dec->bit_stream), 2 );
    }

    /* If it is in use, get the surround channel mix level */
    if (p_ac3dec->bsi.acmod & 0x4)
    {
        NeedBits( &(p_ac3dec->bit_stream), 2 );
        p_ac3dec->bsi.surmixlev = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 2));
        DumpBits( &(p_ac3dec->bit_stream), 2 );
    }

    /* Get the dolby surround mode if in 2/0 mode */
    if(p_ac3dec->bsi.acmod == 0x2)
    {
        NeedBits( &(p_ac3dec->bit_stream), 2 );
        p_ac3dec->bsi.dsurmod = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 2));
        DumpBits( &(p_ac3dec->bit_stream), 2 );
    }

    /* Is the low frequency effects channel on? */
    NeedBits( &(p_ac3dec->bit_stream), 1 );
    p_ac3dec->bsi.lfeon = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 1));
    DumpBits( &(p_ac3dec->bit_stream), 1 );

    /* Get the dialogue normalization level */
    NeedBits( &(p_ac3dec->bit_stream), 5 );
    p_ac3dec->bsi.dialnorm = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 5));
    DumpBits( &(p_ac3dec->bit_stream), 5 );

    /* Does compression gain exist? */
    NeedBits( &(p_ac3dec->bit_stream), 1 );
    p_ac3dec->bsi.compre = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 1));
    DumpBits( &(p_ac3dec->bit_stream), 1 );
    if (p_ac3dec->bsi.compre)
    {
        /* Get compression gain */
        NeedBits( &(p_ac3dec->bit_stream), 8 );
        p_ac3dec->bsi.compr = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 8));
        DumpBits( &(p_ac3dec->bit_stream), 8 );
    }

    /* Does language code exist? */
    NeedBits( &(p_ac3dec->bit_stream), 1 );
    p_ac3dec->bsi.langcode = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 1));
    DumpBits( &(p_ac3dec->bit_stream), 1 );
    if (p_ac3dec->bsi.langcode)
    {
        /* Get langauge code */
        NeedBits( &(p_ac3dec->bit_stream), 8 );
        p_ac3dec->bsi.langcod = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 8));
        DumpBits( &(p_ac3dec->bit_stream), 8 );
    }

    /* Does audio production info exist? */
    NeedBits( &(p_ac3dec->bit_stream), 1 );
    p_ac3dec->bsi.audprodie = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 1));
    DumpBits( &(p_ac3dec->bit_stream), 1 );
    if (p_ac3dec->bsi.audprodie)
    {
        /* Get mix level */
        NeedBits( &(p_ac3dec->bit_stream), 5 );
        p_ac3dec->bsi.mixlevel = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 5));
        DumpBits( &(p_ac3dec->bit_stream), 5 );

        /* Get room type */
        NeedBits( &(p_ac3dec->bit_stream), 2 );
        p_ac3dec->bsi.roomtyp = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 2));
        DumpBits( &(p_ac3dec->bit_stream), 2 );
    }

    /* If we're in dual mono mode then get some extra info */
    if (p_ac3dec->bsi.acmod ==0)
    {
        /* Get the dialogue normalization level two */
        NeedBits( &(p_ac3dec->bit_stream), 5 );
        p_ac3dec->bsi.dialnorm2 = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 5));
        DumpBits( &(p_ac3dec->bit_stream), 5 );

        /* Does compression gain two exist? */
        NeedBits( &(p_ac3dec->bit_stream), 1 );
        p_ac3dec->bsi.compr2e = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 1));
        DumpBits( &(p_ac3dec->bit_stream), 1 );
        if (p_ac3dec->bsi.compr2e)
        {
            /* Get compression gain two */
            NeedBits( &(p_ac3dec->bit_stream), 8 );
            p_ac3dec->bsi.compr2 = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 8));
            DumpBits( &(p_ac3dec->bit_stream), 8 );
        }

        /* Does language code two exist? */
        NeedBits( &(p_ac3dec->bit_stream), 1 );
        p_ac3dec->bsi.langcod2e = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 1));
        DumpBits( &(p_ac3dec->bit_stream), 1 );
        if (p_ac3dec->bsi.langcod2e)
        {
            /* Get langauge code two */
            NeedBits( &(p_ac3dec->bit_stream), 8 );
            p_ac3dec->bsi.langcod2 = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 8));
            DumpBits( &(p_ac3dec->bit_stream), 8 );
        }

        /* Does audio production info two exist? */
        NeedBits( &(p_ac3dec->bit_stream), 1 );
        p_ac3dec->bsi.audprodi2e = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 1));
        DumpBits( &(p_ac3dec->bit_stream), 1 );
        if (p_ac3dec->bsi.audprodi2e)
        {
            /* Get mix level two */
            NeedBits( &(p_ac3dec->bit_stream), 5 );
            p_ac3dec->bsi.mixlevel2 = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 5));
            DumpBits( &(p_ac3dec->bit_stream), 5 );

            /* Get room type two */
            NeedBits( &(p_ac3dec->bit_stream), 2 );
            p_ac3dec->bsi.roomtyp2 = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 2));
            DumpBits( &(p_ac3dec->bit_stream), 2 );
        }
    }

    /* Get the copyright bit */
    NeedBits( &(p_ac3dec->bit_stream), 1 );
    p_ac3dec->bsi.copyrightb = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 1));
    DumpBits( &(p_ac3dec->bit_stream), 1 );

    /* Get the original bit */
    NeedBits( &(p_ac3dec->bit_stream), 1 );
    p_ac3dec->bsi.origbs = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 1));
    DumpBits( &(p_ac3dec->bit_stream), 1 );

    /* Does timecode one exist? */
    NeedBits( &(p_ac3dec->bit_stream), 1 );
    p_ac3dec->bsi.timecod1e = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 1));
    DumpBits( &(p_ac3dec->bit_stream), 1 );

    if(p_ac3dec->bsi.timecod1e)
    {
        NeedBits( &(p_ac3dec->bit_stream), 14 );
        p_ac3dec->bsi.timecod1 = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 14));
        DumpBits( &(p_ac3dec->bit_stream), 14 );
    }

    /* Does timecode two exist? */
    NeedBits( &(p_ac3dec->bit_stream), 1 );
    p_ac3dec->bsi.timecod2e = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 1));
    DumpBits( &(p_ac3dec->bit_stream), 1 );

    if(p_ac3dec->bsi.timecod2e)
    {
        NeedBits( &(p_ac3dec->bit_stream), 14 );
        p_ac3dec->bsi.timecod2 = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 14));
        DumpBits( &(p_ac3dec->bit_stream), 14 );
    }

    /* Does addition info exist? */
    NeedBits( &(p_ac3dec->bit_stream), 1 );
    p_ac3dec->bsi.addbsie = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 1));
    DumpBits( &(p_ac3dec->bit_stream), 1 );

    if(p_ac3dec->bsi.addbsie)
    {
        /* Get how much info is there */
        NeedBits( &(p_ac3dec->bit_stream), 6 );
        p_ac3dec->bsi.addbsil = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 6));
        DumpBits( &(p_ac3dec->bit_stream), 6 );

        /* Get the additional info */
        for(i=0;i<(p_ac3dec->bsi.addbsil + 1);i++)
        {
            NeedBits( &(p_ac3dec->bit_stream), 8 );
            p_ac3dec->bsi.addbsi[i] = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 8));
            DumpBits( &(p_ac3dec->bit_stream), 8 );
        }
    }
}

/* More pain inducing parsing */
void parse_audblk( ac3dec_t * p_ac3dec )
{
    int i, j;

    for (i=0;i < p_ac3dec->bsi.nfchans; i++)
    {
        /* Is this channel an interleaved 256 + 256 block ? */
        NeedBits( &(p_ac3dec->bit_stream), 1 );
        p_ac3dec->audblk.blksw[i] = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 1));
        DumpBits( &(p_ac3dec->bit_stream), 1 );
    }

    for (i=0;i < p_ac3dec->bsi.nfchans; i++)
    {
        /* Should we dither this channel? */
        NeedBits( &(p_ac3dec->bit_stream), 1 );
        p_ac3dec->audblk.dithflag[i] = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 1));
        DumpBits( &(p_ac3dec->bit_stream), 1 );
    }

    /* Does dynamic range control exist? */
    NeedBits( &(p_ac3dec->bit_stream), 1 );
    p_ac3dec->audblk.dynrnge = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 1));
    DumpBits( &(p_ac3dec->bit_stream), 1 );
    if (p_ac3dec->audblk.dynrnge)
    {
        /* Get dynamic range info */
        NeedBits( &(p_ac3dec->bit_stream), 8 );
        p_ac3dec->audblk.dynrng = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 8));
        DumpBits( &(p_ac3dec->bit_stream), 8 );
    }

    /* If we're in dual mono mode then get the second channel DR info */
    if (p_ac3dec->bsi.acmod == 0)
    {
        /* Does dynamic range control two exist? */
        NeedBits( &(p_ac3dec->bit_stream), 1 );
        p_ac3dec->audblk.dynrng2e = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 1));
        DumpBits( &(p_ac3dec->bit_stream), 1 );
        if (p_ac3dec->audblk.dynrng2e)
        {
            /* Get dynamic range info */
            NeedBits( &(p_ac3dec->bit_stream), 8 );
            p_ac3dec->audblk.dynrng2 = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 8));
            DumpBits( &(p_ac3dec->bit_stream), 8 );
        }
    }

    /* Does coupling strategy exist? */
    NeedBits( &(p_ac3dec->bit_stream), 1 );
    p_ac3dec->audblk.cplstre = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 1));
    DumpBits( &(p_ac3dec->bit_stream), 1 );
    if (p_ac3dec->audblk.cplstre)
    {
        /* Is coupling turned on? */
        NeedBits( &(p_ac3dec->bit_stream), 1 );
        p_ac3dec->audblk.cplinu = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 1));
        DumpBits( &(p_ac3dec->bit_stream), 1 );
        if(p_ac3dec->audblk.cplinu)
        {
            for(i=0;i < p_ac3dec->bsi.nfchans; i++)
            {
                NeedBits( &(p_ac3dec->bit_stream), 1 );
                p_ac3dec->audblk.chincpl[i] = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 1));
                DumpBits( &(p_ac3dec->bit_stream), 1 );
            }
            if(p_ac3dec->bsi.acmod == 0x2)
            {
                NeedBits( &(p_ac3dec->bit_stream), 1 );
                p_ac3dec->audblk.phsflginu = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 1));
                DumpBits( &(p_ac3dec->bit_stream), 1 );
            }
            NeedBits( &(p_ac3dec->bit_stream), 4 );
            p_ac3dec->audblk.cplbegf = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 4));
            DumpBits( &(p_ac3dec->bit_stream), 4 );
            NeedBits( &(p_ac3dec->bit_stream), 4 );
            p_ac3dec->audblk.cplendf = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 4));
            DumpBits( &(p_ac3dec->bit_stream), 4 );
            p_ac3dec->audblk.ncplsubnd = (p_ac3dec->audblk.cplendf + 2) - p_ac3dec->audblk.cplbegf + 1;

            /* Calculate the start and end bins of the coupling channel */
            p_ac3dec->audblk.cplstrtmant = (p_ac3dec->audblk.cplbegf * 12) + 37 ;
            p_ac3dec->audblk.cplendmant =  ((p_ac3dec->audblk.cplendf + 3) * 12) + 37;

            /* The number of combined subbands is ncplsubnd minus each combined
             * band */
            p_ac3dec->audblk.ncplbnd = p_ac3dec->audblk.ncplsubnd;

            for(i=1; i< p_ac3dec->audblk.ncplsubnd; i++)
            {
                NeedBits( &(p_ac3dec->bit_stream), 1 );
                p_ac3dec->audblk.cplbndstrc[i] = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 1));
                DumpBits( &(p_ac3dec->bit_stream), 1 );
                p_ac3dec->audblk.ncplbnd -= p_ac3dec->audblk.cplbndstrc[i];
            }
        }
    }

    if(p_ac3dec->audblk.cplinu)
    {
        /* Loop through all the channels and get their coupling co-ords */
        for(i=0;i < p_ac3dec->bsi.nfchans;i++)
        {
            if(!p_ac3dec->audblk.chincpl[i])
                continue;

            /* Is there new coupling co-ordinate info? */
            NeedBits( &(p_ac3dec->bit_stream), 1 );
            p_ac3dec->audblk.cplcoe[i] = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 1));
            DumpBits( &(p_ac3dec->bit_stream), 1 );

            if(p_ac3dec->audblk.cplcoe[i])
            {
                NeedBits( &(p_ac3dec->bit_stream), 2 );
                p_ac3dec->audblk.mstrcplco[i] = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 2));
                DumpBits( &(p_ac3dec->bit_stream), 2 );
                for(j=0;j < p_ac3dec->audblk.ncplbnd; j++)
                {
                    NeedBits( &(p_ac3dec->bit_stream), 4 );
                    p_ac3dec->audblk.cplcoexp[i][j] = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 4));
                    DumpBits( &(p_ac3dec->bit_stream), 4 );
                    NeedBits( &(p_ac3dec->bit_stream), 4 );
                    p_ac3dec->audblk.cplcomant[i][j] = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 4));
                    DumpBits( &(p_ac3dec->bit_stream), 4 );
                }
            }
        }

        /* If we're in dual mono mode, there's going to be some phase info */
        if( (p_ac3dec->bsi.acmod == 0x2) && p_ac3dec->audblk.phsflginu &&
                (p_ac3dec->audblk.cplcoe[0] || p_ac3dec->audblk.cplcoe[1]))
        {
            for(j=0;j < p_ac3dec->audblk.ncplbnd; j++)
            {
                NeedBits( &(p_ac3dec->bit_stream), 1 );
                p_ac3dec->audblk.phsflg[j] = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 1));
                DumpBits( &(p_ac3dec->bit_stream), 1 );
            }

        }
    }

    /* If we're in dual mono mode, there may be a rematrix strategy */
    if(p_ac3dec->bsi.acmod == 0x2)
    {
        NeedBits( &(p_ac3dec->bit_stream), 1 );
        p_ac3dec->audblk.rematstr = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 1));
        DumpBits( &(p_ac3dec->bit_stream), 1 );
        if(p_ac3dec->audblk.rematstr)
        {
            if (p_ac3dec->audblk.cplinu == 0)
            {
                for(i = 0; i < 4; i++)
                {
                    NeedBits( &(p_ac3dec->bit_stream), 1 );
                    p_ac3dec->audblk.rematflg[i] = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 1));
                    DumpBits( &(p_ac3dec->bit_stream), 1 );
                }
            }
            if((p_ac3dec->audblk.cplbegf > 2) && p_ac3dec->audblk.cplinu)
            {
                for(i = 0; i < 4; i++)
                {
                    NeedBits( &(p_ac3dec->bit_stream), 1 );
                    p_ac3dec->audblk.rematflg[i] = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 1));
                    DumpBits( &(p_ac3dec->bit_stream), 1 );
                }
            }
            if((p_ac3dec->audblk.cplbegf <= 2) && p_ac3dec->audblk.cplinu)
            {
                for(i = 0; i < 3; i++)
                {
                    NeedBits( &(p_ac3dec->bit_stream), 1 );
                    p_ac3dec->audblk.rematflg[i] = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 1));
                    DumpBits( &(p_ac3dec->bit_stream), 1 );
                }
            }
            if((p_ac3dec->audblk.cplbegf == 0) && p_ac3dec->audblk.cplinu)
                for(i = 0; i < 2; i++)
                {
                    NeedBits( &(p_ac3dec->bit_stream), 1 );
                    p_ac3dec->audblk.rematflg[i] = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 1));
                    DumpBits( &(p_ac3dec->bit_stream), 1 );
                }

        }
    }

    if (p_ac3dec->audblk.cplinu)
    {
        /* Get the coupling channel exponent strategy */
        NeedBits( &(p_ac3dec->bit_stream), 2 );
        p_ac3dec->audblk.cplexpstr = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 2));
        DumpBits( &(p_ac3dec->bit_stream), 2 );

        if(p_ac3dec->audblk.cplexpstr==0)
            p_ac3dec->audblk.ncplgrps = 0;
        else
            p_ac3dec->audblk.ncplgrps = (p_ac3dec->audblk.cplendmant - p_ac3dec->audblk.cplstrtmant) /
                                        (3 << (p_ac3dec->audblk.cplexpstr-1));

    }

    for(i = 0; i < p_ac3dec->bsi.nfchans; i++)
    {
        NeedBits( &(p_ac3dec->bit_stream), 2 );
        p_ac3dec->audblk.chexpstr[i] = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 2));
        DumpBits( &(p_ac3dec->bit_stream), 2 );
    }

    /* Get the exponent strategy for lfe channel */
    if(p_ac3dec->bsi.lfeon)
    {
        NeedBits( &(p_ac3dec->bit_stream), 1 );
        p_ac3dec->audblk.lfeexpstr = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 1));
        DumpBits( &(p_ac3dec->bit_stream), 1 );
    }

    /* Determine the bandwidths of all the fbw channels */
    for(i = 0; i < p_ac3dec->bsi.nfchans; i++)
    {
        u16 grp_size;

        if(p_ac3dec->audblk.chexpstr[i] != EXP_REUSE)
        {
            if (p_ac3dec->audblk.cplinu && p_ac3dec->audblk.chincpl[i])
            {
                p_ac3dec->audblk.endmant[i] = p_ac3dec->audblk.cplstrtmant;
            }
            else
            {
                NeedBits( &(p_ac3dec->bit_stream), 6 );
                p_ac3dec->audblk.chbwcod[i] = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 6));
                DumpBits( &(p_ac3dec->bit_stream), 6 );
                p_ac3dec->audblk.endmant[i] = ((p_ac3dec->audblk.chbwcod[i] + 12) * 3) + 37;
            }

            /* Calculate the number of exponent groups to fetch */
            grp_size =  3 * (1 << (p_ac3dec->audblk.chexpstr[i] - 1));
            p_ac3dec->audblk.nchgrps[i] = (p_ac3dec->audblk.endmant[i] - 1 + (grp_size - 3)) / grp_size;
        }
    }

    /* Get the coupling exponents if they exist */
    if(p_ac3dec->audblk.cplinu && (p_ac3dec->audblk.cplexpstr != EXP_REUSE))
    {
        NeedBits( &(p_ac3dec->bit_stream), 4 );
        p_ac3dec->audblk.cplabsexp = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 4));
        DumpBits( &(p_ac3dec->bit_stream), 4 );
        for(i=0;i< p_ac3dec->audblk.ncplgrps;i++)
        {
            NeedBits( &(p_ac3dec->bit_stream), 7 );
            p_ac3dec->audblk.cplexps[i] = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 7));
            DumpBits( &(p_ac3dec->bit_stream), 7 );
        }
    }

    /* Get the fwb channel exponents */
    for(i=0;i < p_ac3dec->bsi.nfchans; i++)
    {
        if(p_ac3dec->audblk.chexpstr[i] != EXP_REUSE)
        {
            NeedBits( &(p_ac3dec->bit_stream), 4 );
            p_ac3dec->audblk.exps[i][0] = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 4));
            DumpBits( &(p_ac3dec->bit_stream), 4 );
            for(j=1;j<=p_ac3dec->audblk.nchgrps[i];j++)
            {
                NeedBits( &(p_ac3dec->bit_stream), 7 );
                p_ac3dec->audblk.exps[i][j] = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 7));
                DumpBits( &(p_ac3dec->bit_stream), 7 );
            }
            NeedBits( &(p_ac3dec->bit_stream), 2 );
            p_ac3dec->audblk.gainrng[i] = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 2));
            DumpBits( &(p_ac3dec->bit_stream), 2 );
        }
    }

    /* Get the lfe channel exponents */
    if(p_ac3dec->bsi.lfeon && (p_ac3dec->audblk.lfeexpstr != EXP_REUSE))
    {
        NeedBits( &(p_ac3dec->bit_stream), 4 );
        p_ac3dec->audblk.lfeexps[0] = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 4));
        DumpBits( &(p_ac3dec->bit_stream), 4 );
        NeedBits( &(p_ac3dec->bit_stream), 7 );
        p_ac3dec->audblk.lfeexps[1] = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 7));
        DumpBits( &(p_ac3dec->bit_stream), 7 );
        NeedBits( &(p_ac3dec->bit_stream), 7 );
        p_ac3dec->audblk.lfeexps[2] = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 7));
        DumpBits( &(p_ac3dec->bit_stream), 7 );
    }

    /* Get the parametric bit allocation parameters */
    NeedBits( &(p_ac3dec->bit_stream), 1 );
    p_ac3dec->audblk.baie = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 1));
    DumpBits( &(p_ac3dec->bit_stream), 1 );

    if(p_ac3dec->audblk.baie)
    {
        NeedBits( &(p_ac3dec->bit_stream), 2 );
        p_ac3dec->audblk.sdcycod = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 2));
        DumpBits( &(p_ac3dec->bit_stream), 2 );
        NeedBits( &(p_ac3dec->bit_stream), 2 );
        p_ac3dec->audblk.fdcycod = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 2));
        DumpBits( &(p_ac3dec->bit_stream), 2 );
        NeedBits( &(p_ac3dec->bit_stream), 2 );
        p_ac3dec->audblk.sgaincod = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 2));
        DumpBits( &(p_ac3dec->bit_stream), 2 );
        NeedBits( &(p_ac3dec->bit_stream), 2 );
        p_ac3dec->audblk.dbpbcod = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 2));
        DumpBits( &(p_ac3dec->bit_stream), 2 );
        NeedBits( &(p_ac3dec->bit_stream), 3 );
        p_ac3dec->audblk.floorcod = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 3));
        DumpBits( &(p_ac3dec->bit_stream), 3 );
    }

    /* Get the SNR off set info if it exists */
    NeedBits( &(p_ac3dec->bit_stream), 1 );
    p_ac3dec->audblk.snroffste = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 1));
    DumpBits( &(p_ac3dec->bit_stream), 1 );

    if(p_ac3dec->audblk.snroffste)
    {
        NeedBits( &(p_ac3dec->bit_stream), 6 );
        p_ac3dec->audblk.csnroffst = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 6));
        DumpBits( &(p_ac3dec->bit_stream), 6 );

        if(p_ac3dec->audblk.cplinu)
        {
            NeedBits( &(p_ac3dec->bit_stream), 4 );
            p_ac3dec->audblk.cplfsnroffst = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 4));
            DumpBits( &(p_ac3dec->bit_stream), 4 );
            NeedBits( &(p_ac3dec->bit_stream), 3 );
            p_ac3dec->audblk.cplfgaincod = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 3));
            DumpBits( &(p_ac3dec->bit_stream), 3 );
        }

        for(i = 0;i < p_ac3dec->bsi.nfchans; i++)
        {
            NeedBits( &(p_ac3dec->bit_stream), 4 );
            p_ac3dec->audblk.fsnroffst[i] = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 4));
            DumpBits( &(p_ac3dec->bit_stream), 4 );
            NeedBits( &(p_ac3dec->bit_stream), 3 );
            p_ac3dec->audblk.fgaincod[i] = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 3));
            DumpBits( &(p_ac3dec->bit_stream), 3 );
        }
        if(p_ac3dec->bsi.lfeon)
        {

            NeedBits( &(p_ac3dec->bit_stream), 4 );
            p_ac3dec->audblk.lfefsnroffst = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 4));
            DumpBits( &(p_ac3dec->bit_stream), 4 );
            NeedBits( &(p_ac3dec->bit_stream), 3 );
            p_ac3dec->audblk.lfefgaincod = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 3));
            DumpBits( &(p_ac3dec->bit_stream), 3 );
        }
    }

    /* Get coupling leakage info if it exists */
    if(p_ac3dec->audblk.cplinu)
    {
        NeedBits( &(p_ac3dec->bit_stream), 1 );
        p_ac3dec->audblk.cplleake = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 1));
        DumpBits( &(p_ac3dec->bit_stream), 1 );

        if(p_ac3dec->audblk.cplleake)
        {
            NeedBits( &(p_ac3dec->bit_stream), 3 );
            p_ac3dec->audblk.cplfleak = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 3));
            DumpBits( &(p_ac3dec->bit_stream), 3 );
            NeedBits( &(p_ac3dec->bit_stream), 3 );
            p_ac3dec->audblk.cplsleak = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 3));
            DumpBits( &(p_ac3dec->bit_stream), 3 );
        }
    }

    /* Get the delta bit alloaction info */
    NeedBits( &(p_ac3dec->bit_stream), 1 );
    p_ac3dec->audblk.deltbaie = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 1));
    DumpBits( &(p_ac3dec->bit_stream), 1 );

    if(p_ac3dec->audblk.deltbaie)
    {
        if(p_ac3dec->audblk.cplinu)
        {
            NeedBits( &(p_ac3dec->bit_stream), 2 );
            p_ac3dec->audblk.cpldeltbae = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 2));
            DumpBits( &(p_ac3dec->bit_stream), 2 );
        }

        for(i = 0;i < p_ac3dec->bsi.nfchans; i++)
        {
            NeedBits( &(p_ac3dec->bit_stream), 2 );
            p_ac3dec->audblk.deltbae[i] = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 2));
            DumpBits( &(p_ac3dec->bit_stream), 2 );
        }

        if (p_ac3dec->audblk.cplinu && (p_ac3dec->audblk.cpldeltbae == DELTA_BIT_NEW))
        {
            NeedBits( &(p_ac3dec->bit_stream), 3 );
            p_ac3dec->audblk.cpldeltnseg = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 3));
            DumpBits( &(p_ac3dec->bit_stream), 3 );
            for(i = 0;i < p_ac3dec->audblk.cpldeltnseg + 1; i++)
            {
                NeedBits( &(p_ac3dec->bit_stream), 5 );
                p_ac3dec->audblk.cpldeltoffst[i] = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 5));
                DumpBits( &(p_ac3dec->bit_stream), 5 );
                NeedBits( &(p_ac3dec->bit_stream), 4 );
                p_ac3dec->audblk.cpldeltlen[i] = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 4));
                DumpBits( &(p_ac3dec->bit_stream), 4 );
                NeedBits( &(p_ac3dec->bit_stream), 3 );
                p_ac3dec->audblk.cpldeltba[i] = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 3));
                DumpBits( &(p_ac3dec->bit_stream), 3 );
            }
        }

        for(i = 0;i < p_ac3dec->bsi.nfchans; i++)
        {
            if (p_ac3dec->audblk.deltbae[i] == DELTA_BIT_NEW)
            {
                NeedBits( &(p_ac3dec->bit_stream), 3 );
                p_ac3dec->audblk.deltnseg[i] = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 3));
                DumpBits( &(p_ac3dec->bit_stream), 3 );
//                if ( p_ac3dec->audblk.deltnseg[i] >= 8 )
//                    fprintf( stderr, "parse debug: p_ac3dec->audblk.deltnseg[%i] == %i\n", i, p_ac3dec->audblk.deltnseg[i] );
                for(j = 0; j < p_ac3dec->audblk.deltnseg[i] + 1; j++)
                {
                    NeedBits( &(p_ac3dec->bit_stream), 5 );
                    p_ac3dec->audblk.deltoffst[i][j] = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 5));
                    DumpBits( &(p_ac3dec->bit_stream), 5 );
                    NeedBits( &(p_ac3dec->bit_stream), 4 );
                    p_ac3dec->audblk.deltlen[i][j] = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 4));
                    DumpBits( &(p_ac3dec->bit_stream), 4 );
                    NeedBits( &(p_ac3dec->bit_stream), 3 );
                    p_ac3dec->audblk.deltba[i][j] = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 3));
                    DumpBits( &(p_ac3dec->bit_stream), 3 );
                }
            }
        }
    }

    /* Check to see if there's any dummy info to get */
    NeedBits( &(p_ac3dec->bit_stream), 1 );
    p_ac3dec->audblk.skiple = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 1));
    DumpBits( &(p_ac3dec->bit_stream), 1 );

    if ( p_ac3dec->audblk.skiple )
    {
        NeedBits( &(p_ac3dec->bit_stream), 9 );
        p_ac3dec->audblk.skipl = (u16)(p_ac3dec->bit_stream.buffer >> (32 - 9));
        DumpBits( &(p_ac3dec->bit_stream), 9 );

        for(i = 0; i < p_ac3dec->audblk.skipl ; i++)
        {
            NeedBits( &(p_ac3dec->bit_stream), 8 );
            DumpBits( &(p_ac3dec->bit_stream), 8 );
        }
    }
}

void parse_auxdata( ac3dec_t * p_ac3dec )
{
    int i;
    int skip_length;

    skip_length = (p_ac3dec->syncinfo.frame_size * 16) - p_ac3dec->bit_stream.total_bits_read - 17 - 1;
//    fprintf( stderr, "parse debug: skip_length == %i\n", skip_length );

    for ( i = 0; i < skip_length; i++ )
    {
        NeedBits( &(p_ac3dec->bit_stream), 1 );
        DumpBits( &(p_ac3dec->bit_stream), 1 );
    }

    /* get the auxdata exists bit */
    NeedBits( &(p_ac3dec->bit_stream), 1 );
    DumpBits( &(p_ac3dec->bit_stream), 1 );

    /* Skip the CRC reserved bit */
    NeedBits( &(p_ac3dec->bit_stream), 1 );
    DumpBits( &(p_ac3dec->bit_stream), 1 );

    /* Get the crc */
    NeedBits( &(p_ac3dec->bit_stream), 16 );
    DumpBits( &(p_ac3dec->bit_stream), 16 );
}

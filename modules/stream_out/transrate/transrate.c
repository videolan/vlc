/*****************************************************************************
 * transrate.c
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * Copyright (C) 2003 Freebox S.A.
 * Copyright (C) 2003 Antoine Missout
 * Copyright (C) 2000-2003 Michel Lespinasse <walken@zoy.org>
 * Copyright (C) 1999-2000 Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
 * $Id: transrate.c,v 1.1 2003/11/07 21:30:52 massiot Exp $
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Antoine Missout
 *          Michel Lespinasse <walken@zoy.org>
 *          Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define NDEBUG 1
#include <assert.h>
#include <unistd.h>
#include <math.h>
#include <fcntl.h>

#include <vlc/vlc.h>
#include <vlc/input.h>
#include <vlc/sout.h>
#include <vlc/vout.h>
#include <vlc/decoder.h>

/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static int      Open    ( vlc_object_t * );
static void     Close   ( vlc_object_t * );

static sout_stream_id_t *Add ( sout_stream_t *, sout_format_t * );
static int               Del ( sout_stream_t *, sout_stream_id_t * );
static int               Send( sout_stream_t *, sout_stream_id_t *, sout_buffer_t * );

static int  transrate_video_new    ( sout_stream_t *, sout_stream_id_t * );
static void transrate_video_close  ( sout_stream_t *, sout_stream_id_t * );
static int  transrate_video_process( sout_stream_t *, sout_stream_id_t *, sout_buffer_t *, sout_buffer_t ** );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("Transrate stream") );
    set_capability( "sout stream", 50 );
    add_shortcut( "transrate" );
    set_callbacks( Open, Close );
vlc_module_end();

struct sout_stream_sys_t
{
    sout_stream_t   *p_out;

    int             i_vbitrate;

    mtime_t         i_first_frame;
    mtime_t         i_dts, i_pts;
};

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_stream_t     *p_stream = (sout_stream_t*)p_this;
    sout_stream_sys_t *p_sys;
    char *val;

    p_sys = malloc( sizeof( sout_stream_sys_t ) );
    p_sys->p_out = sout_stream_new( p_stream->p_sout, p_stream->psz_next );

    p_sys->i_vbitrate   = 0;

    if( ( val = sout_cfg_find_value( p_stream->p_cfg, "vb" ) ) )
    {
        p_sys->i_vbitrate = atoi( val );
        if( p_sys->i_vbitrate < 16000 )
        {
            p_sys->i_vbitrate *= 1000;
        }
    }
    else
    {
        p_sys->i_vbitrate = 3000000;
    }
    p_sys->i_first_frame = 0;

    msg_Dbg( p_stream, "codec video %dkb/s",
             p_sys->i_vbitrate / 1024 );

    if( !p_sys->p_out )
    {
        msg_Err( p_stream, "cannot create chain" );
        free( p_sys );
        return VLC_EGENERIC;
    }
    p_stream->pf_add    = Add;
    p_stream->pf_del    = Del;
    p_stream->pf_send   = Send;

    p_stream->p_sys     = p_sys;

    p_stream->p_sout->i_padding += 200;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    sout_stream_t       *p_stream = (sout_stream_t *)p_this;
    sout_stream_sys_t   *p_sys = p_stream->p_sys;

    sout_stream_delete( p_sys->p_out );
    free( p_sys );
}

struct sout_stream_id_t
{
    void            *id;
    vlc_bool_t      b_transrate;

    sout_buffer_t   *p_current_buffer;
    sout_buffer_t   *p_next_gop;
    mtime_t         i_next_gop_duration;
    size_t          i_next_gop_size;
};


static sout_stream_id_t * Add( sout_stream_t *p_stream, sout_format_t *p_fmt )
{
    sout_stream_sys_t   *p_sys = p_stream->p_sys;
    sout_stream_id_t    *id;

    id = malloc( sizeof( sout_stream_id_t ) );
    id->id = NULL;

    if( p_fmt->i_cat == VIDEO_ES
            && p_fmt->i_fourcc == VLC_FOURCC('m', 'p', 'g', 'v') )
    {
        msg_Dbg( p_stream,
                 "creating video transrating for fcc=`%4.4s'",
                 (char*)&p_fmt->i_fourcc );

        /* build decoder -> filter -> encoder */
        if( transrate_video_new( p_stream, id ) )
        {
            msg_Err( p_stream, "cannot create video chain" );
            free( id );
            return NULL;
        }

        /* open output stream */
        id->id = p_sys->p_out->pf_add( p_sys->p_out, p_fmt );
        id->b_transrate = VLC_TRUE;
    }
    else
    {
        msg_Dbg( p_stream, "not transrating a stream (fcc=`%4.4s')", (char*)&p_fmt->i_fourcc );
        id->id = p_sys->p_out->pf_add( p_sys->p_out, p_fmt );
        id->b_transrate = VLC_FALSE;

        if( id->id == NULL )
        {
            free( id );
            return NULL;
        }
    }

    return id;
}

static int     Del      ( sout_stream_t *p_stream, sout_stream_id_t *id )
{
    sout_stream_sys_t   *p_sys = p_stream->p_sys;

    if( id->b_transrate )
    {
        transrate_video_close( p_stream, id );
    }

    if( id->id ) p_sys->p_out->pf_del( p_sys->p_out, id->id );
    free( id );

    return VLC_SUCCESS;
}

static int Send( sout_stream_t *p_stream, sout_stream_id_t *id,
                 sout_buffer_t *p_buffer )
{
    sout_stream_sys_t   *p_sys = p_stream->p_sys;

    if( id->b_transrate )
    {
        sout_buffer_t *p_buffer_out;
        transrate_video_process( p_stream, id, p_buffer, &p_buffer_out );

        if( p_buffer_out )
        {
            return p_sys->p_out->pf_send( p_sys->p_out, id->id, p_buffer_out );
        }
        return VLC_SUCCESS;
    }
    else if( id->id != NULL )
    {
        return p_sys->p_out->pf_send( p_sys->p_out, id->id, p_buffer );
    }
    else
    {
        sout_BufferDelete( p_stream->p_sout, p_buffer );
        return VLC_EGENERIC;
    }
}

/****************************************************************************
 * transrater, code from M2VRequantizer http://www.metakine.com/
 ****************************************************************************/

// toggles:
// #define NDEBUG // turns off asserts
#define REMOVE_BYTE_STUFFING	// removes 0x 00 00 00 00 00 00 used in cbr streams (look for 6 0x00 and remove 1 0x00)
								/*	4 0x00 might be legit, for exemple:
									00 00 01 b5 14 82 00 01 00 00 00 00 01 b8 .. ..
												 these two: -- -- are part of the seq. header ext.
									AFAIK 5 0x00 should never happen except for byte stuffing but to be safe look for 6 */

/* This is awful magic --Meuuh */
//#define REACT_DELAY (1024.0*128.0)
#define REACT_DELAY (1024.0*4.0)

// notes:
//
// - intra block:
// 		- the quantiser is increment by one step
//
// - non intra block:
//		- in P_FRAME we keep the original quantiser but drop the last coefficient
//		  if there is more than one
//		- in B_FRAME we multiply the quantiser by a factor
//
// - I_FRAME is recoded when we're 5.0 * REACT_DELAY late
// - P_FRAME is recoded when we're 2.5 * REACT_DELAY late
// - B_FRAME are always recoded

// if we're getting *very* late (60 * REACT_DELAY)
//
// - intra blocks quantiser is incremented two step
// - drop a few coefficients but always keep the first one

// useful constants
#define I_TYPE 1
#define P_TYPE 2
#define B_TYPE 3

// gcc
#ifdef HAVE_BUILTIN_EXPECT
#define likely(x) __builtin_expect ((x) != 0, 1)
#define unlikely(x) __builtin_expect ((x) != 0, 0)
#else
#define likely(x) (x)
#define unlikely(x) (x)
#endif

// user defined types
//typedef unsigned int		uint;
typedef unsigned char		uint8;
typedef unsigned short		uint16;
typedef unsigned int		uint32;
typedef unsigned long long	uint64;

typedef char				int8;
typedef short				int16;
typedef int					int32;
typedef long long			int64;

typedef signed int			sint;
typedef signed char			sint8;
typedef signed short		sint16;
typedef signed int			sint32;
typedef signed long long	sint64;

#define BITS_IN_BUF (8)

// global variables
static uint8	*cbuf, *rbuf, *wbuf, *orbuf, *owbuf, *rwbuf;
static int		inbitcnt, outbitcnt;
static uint32	inbitbuf, outbitbuf;
static uint32	inbytecnt, outbytecnt;

    static struct sout_stream_sys_t * p_sys;
// mpeg2 state
	// seq header
	static uint horizontal_size_value;
	static uint vertical_size_value;
	
	// pic header
	static uint picture_coding_type;
	
	// pic code ext
	static uint f_code[2][2];
	static uint intra_dc_precision;
	static uint picture_structure;
	static uint frame_pred_frame_dct;
	static uint concealment_motion_vectors;
	static uint q_scale_type;
	static uint intra_vlc_format;
	static uint alternate_scan;
	
	// slice or mb
	// quantizer_scale_code
	static uint quantizer_scale;
	static uint new_quantizer_scale;
	static uint last_coded_scale;
	static int	 h_offset, v_offset;
	
	// mb
	static double quant_corr, fact_x;
	
	// block data
	typedef struct
	{
		uint8 run;
		short level;
	} RunLevel;

	static RunLevel block[6][65]; // terminated by level = 0, so we need 64+1
// end mpeg2 state

#define LOG(msg) fprintf (stderr, msg)
#define LOGF(format, args...) fprintf (stderr, format, args)

#define WRITE \
        do { } while(0);

#define LOCK(x) \
        do { } while(0);

#define COPY(x)\
		assert(x > 0); \
		memcpy(wbuf, cbuf, x);\
		cbuf += x; \
		wbuf += x;

#define SEEKR(x)\
		cbuf += x;
	
#define SEEKW(x)\
		wbuf += x;

static inline void putbits(uint val, int n)
{
	assert(n < 32);
	assert(!(val & (0xffffffffU << n)));

	while (unlikely(n >= outbitcnt))
	{
		wbuf[0] = (outbitbuf << outbitcnt ) | (val >> (n - outbitcnt));
		SEEKW(1);
		n -= outbitcnt;
		outbitbuf = 0;
		val &= ~(0xffffffffU << n);
		outbitcnt = BITS_IN_BUF;
	}
	
	if (likely(n))
	{
		outbitbuf = (outbitbuf << n) | val;
		outbitcnt -= n;
	}
	
	assert(outbitcnt > 0);
	assert(outbitcnt <= BITS_IN_BUF);
}

static inline void Refill_bits(void)
{
	assert((rbuf - cbuf) >= 1);
	inbitbuf |= cbuf[0] << (24 - inbitcnt);
	inbitcnt += 8;
	SEEKR(1)
}

static inline void Flush_Bits(uint n)
{
	assert(inbitcnt >= n);

	inbitbuf <<= n;
	inbitcnt -= n;

	assert( (!n) || ((n>0) && !(inbitbuf & 0x1)) );

	while (unlikely(inbitcnt < 24)) Refill_bits();
}

static inline uint Show_Bits(uint n)
{
	return ((unsigned int)inbitbuf) >> (32 - n);
}

static inline uint Get_Bits(uint n)
{
	uint Val = Show_Bits(n);
	Flush_Bits(n);
	return Val;
}

static inline uint Copy_Bits(uint n)
{
	uint Val = Get_Bits(n);
	putbits(Val, n);
	return Val;
}

static inline void flush_read_buffer()
{
	int i = inbitcnt & 0x7;
	if (i)
	{
		//uint val = Show_Bits(i);
		//putbits(val, i);
		assert(((unsigned int)inbitbuf) >> (32 - i) == 0);
		inbitbuf <<= i;
		inbitcnt -= i;
	}
	SEEKR(-1 * (inbitcnt >> 3));
	inbitcnt = 0;
}


static inline void flush_write_buffer()
{
	if (outbitcnt != 8) putbits(0, outbitcnt);
}

/////---- begin ext mpeg code

const uint8 non_linear_mquant_table[32] =
{
	0, 1, 2, 3, 4, 5, 6, 7,
	8,10,12,14,16,18,20,22,
	24,28,32,36,40,44,48,52,
	56,64,72,80,88,96,104,112
};
const uint8 map_non_linear_mquant[113] =
{
	0,1,2,3,4,5,6,7,8,8,9,9,10,10,11,11,12,12,13,13,14,14,15,15,16,16,
	16,17,17,17,18,18,18,18,19,19,19,19,20,20,20,20,21,21,21,21,22,22,
	22,22,23,23,23,23,24,24,24,24,24,24,24,25,25,25,25,25,25,25,26,26,
	26,26,26,26,26,26,27,27,27,27,27,27,27,27,28,28,28,28,28,28,28,29,
	29,29,29,29,29,29,29,29,29,30,30,30,30,30,30,30,31,31,31,31,31
};

static int scale_quant(double quant )
{
	int iquant;
	if (q_scale_type)
	{
		iquant = (int) floor(quant+0.5);

		/* clip mquant to legal (linear) range */
		if (iquant<1) iquant = 1;
		if (iquant>112) iquant = 112;

		iquant = non_linear_mquant_table[map_non_linear_mquant[iquant]];
	}
	else
	{
		/* clip mquant to legal (linear) range */
		iquant = (int)floor(quant+0.5);
		if (iquant<2) iquant = 2;
		if (iquant>62) iquant = 62;
		iquant = (iquant/2)*2; // Must be *even*
	}
	return iquant;
}

static int increment_quant(int quant)
{
	if (q_scale_type)
	{
		assert(quant >= 1 && quant <= 112);
		quant = map_non_linear_mquant[quant] + 1;
		if (quant_corr < -60.0f) quant++;
		if (quant > 31) quant = 31;
		quant = non_linear_mquant_table[quant];
	}
	else
	{
		assert(!(quant & 1));
		quant += 2;
		if (quant_corr < -60.0f) quant += 2;
		if (quant > 62) quant = 62;
	}
	return quant;
}

static inline int intmax( register int x, register int y )
{ return x < y ? y : x; }

static inline int intmin( register int x, register int y )
{ return x < y ? x : y; }

static int getNewQuant(int curQuant)
{
	double calc_quant, quant_to_use;
	int mquant = 0;
	
	quant_corr = (((inbytecnt - (rbuf - 4 - cbuf)) / fact_x) - (outbytecnt + (wbuf - owbuf))) / REACT_DELAY;
	calc_quant = curQuant * fact_x;
	quant_to_use = calc_quant - quant_corr;

	switch (picture_coding_type)
	{
		case I_TYPE:
		case P_TYPE:
			mquant = increment_quant(curQuant);
			break;
			
		case B_TYPE:
			mquant = intmax(scale_quant(quant_to_use), increment_quant(curQuant));
			break;
			
		default:
			assert(0);
			break;
	}
	
	/*
		LOGF("type: %s orig_quant: %3i calc_quant: %7.1f quant_corr: %7.1f using_quant: %3i\n",
		(picture_coding_type == I_TYPE ? "I_TYPE" : (picture_coding_type == P_TYPE ? "P_TYPE" : "B_TYPE")),
		(int)curQuant, (float)calc_quant, (float)quant_corr, (int)mquant);
	*/
		
	assert(mquant >= curQuant);
	
	return mquant;
}

static inline int isNotEmpty(RunLevel *blk)
{
	return (blk->level);
}

#include "putvlc.h"

void putAC(int run, int signed_level, int vlcformat)
{
	int level, len;
	const VLCtable *ptab = NULL;
	
	level = (signed_level<0) ? -signed_level : signed_level; /* abs(signed_level) */
	
	assert(!(run<0 || run>63 || level==0 || level>2047));
	
	len = 0;
	
	if (run<2 && level<41)
	{
		if (vlcformat)  ptab = &dct_code_tab1a[run][level-1];
		else ptab = &dct_code_tab1[run][level-1];
		len = ptab->len;
	}
	else if (run<32 && level<6)
	{
		if (vlcformat) ptab = &dct_code_tab2a[run-2][level-1];
		else ptab = &dct_code_tab2[run-2][level-1];
		len = ptab->len;
	}
	
	if (len) /* a VLC code exists */
	{
		putbits(ptab->code, len);
		putbits(signed_level<0, 1); /* sign */
	}
	else
	{
		putbits(1l, 6); /* Escape */
		putbits(run, 6); /* 6 bit code for run */
		putbits(((uint)signed_level) & 0xFFF, 12);
	}
}


static inline void putACfirst(int run, int val)
{
	if (run==0 && (val==1 || val==-1)) putbits(2|(val<0),2);
	else putAC(run,val,0);
}

void putnonintrablk(RunLevel *blk)
{
	assert(blk->level);
	
	putACfirst(blk->run, blk->level);
	blk++;
	
	while(blk->level)
	{
		putAC(blk->run, blk->level, 0);
		blk++;
	}
	
	putbits(2,2);
}

static inline void putcbp(int cbp)
{
	putbits(cbptable[cbp].code,cbptable[cbp].len);
}

void putmbtype(int mb_type)
{
	putbits(mbtypetab[picture_coding_type-1][mb_type].code,
			mbtypetab[picture_coding_type-1][mb_type].len);
}

#include <inttypes.h>
#include "getvlc.h"

static int non_linear_quantizer_scale [] =
{
     0,  1,  2,  3,  4,  5,   6,   7,
     8, 10, 12, 14, 16, 18,  20,  22,
    24, 28, 32, 36, 40, 44,  48,  52,
    56, 64, 72, 80, 88, 96, 104, 112
};

static inline int get_macroblock_modes ()
{
    int macroblock_modes;
    const MBtab * tab;

    switch (picture_coding_type)
	{
		case I_TYPE:
	
			tab = MB_I + UBITS (bit_buf, 1);
			DUMPBITS (bit_buf, bits, tab->len);
			macroblock_modes = tab->modes;
		
			if ((! (frame_pred_frame_dct)) && (picture_structure == FRAME_PICTURE))
			{
				macroblock_modes |= UBITS (bit_buf, 1) * DCT_TYPE_INTERLACED;
				DUMPBITS (bit_buf, bits, 1);
			}
		
			return macroblock_modes;
	
		case P_TYPE:
	
			tab = MB_P + UBITS (bit_buf, 5);
			DUMPBITS (bit_buf, bits, tab->len);
			macroblock_modes = tab->modes;
		
			if (picture_structure != FRAME_PICTURE)
			{
				if (macroblock_modes & MACROBLOCK_MOTION_FORWARD)
				{
					macroblock_modes |= UBITS (bit_buf, 2) * MOTION_TYPE_BASE;
					DUMPBITS (bit_buf, bits, 2);
				}
				return macroblock_modes;
			}
			else if (frame_pred_frame_dct)
			{
				if (macroblock_modes & MACROBLOCK_MOTION_FORWARD)
					macroblock_modes |= MC_FRAME;
				return macroblock_modes;
			}
			else
			{
				if (macroblock_modes & MACROBLOCK_MOTION_FORWARD)
				{
					macroblock_modes |= UBITS (bit_buf, 2) * MOTION_TYPE_BASE;
					DUMPBITS (bit_buf, bits, 2);
				}
				if (macroblock_modes & (MACROBLOCK_INTRA | MACROBLOCK_PATTERN))
				{
					macroblock_modes |= UBITS (bit_buf, 1) * DCT_TYPE_INTERLACED;
					DUMPBITS (bit_buf, bits, 1);
				}
				return macroblock_modes;
			}
	
		case B_TYPE:
	
			tab = MB_B + UBITS (bit_buf, 6);
			DUMPBITS (bit_buf, bits, tab->len);
			macroblock_modes = tab->modes;
		
			if (picture_structure != FRAME_PICTURE)
			{
				if (! (macroblock_modes & MACROBLOCK_INTRA))
				{
					macroblock_modes |= UBITS (bit_buf, 2) * MOTION_TYPE_BASE;
					DUMPBITS (bit_buf, bits, 2);
				}
				return macroblock_modes;
			}
			else if (frame_pred_frame_dct)
			{
				/* if (! (macroblock_modes & MACROBLOCK_INTRA)) */
				macroblock_modes |= MC_FRAME;
				return macroblock_modes;
			}
			else
			{
				if (macroblock_modes & MACROBLOCK_INTRA) goto intra;
				macroblock_modes |= UBITS (bit_buf, 2) * MOTION_TYPE_BASE;
				DUMPBITS (bit_buf, bits, 2);
				if (macroblock_modes & (MACROBLOCK_INTRA | MACROBLOCK_PATTERN))
				{
					intra:
					macroblock_modes |= UBITS (bit_buf, 1) * DCT_TYPE_INTERLACED;
					DUMPBITS (bit_buf, bits, 1);
				}
				return macroblock_modes;
			}
	
		default:
			return 0;
    }

}

static inline int get_quantizer_scale ()
{
    int quantizer_scale_code;

    quantizer_scale_code = UBITS (bit_buf, 5);
	DUMPBITS (bit_buf, bits, 5); 
	
	if (q_scale_type) return non_linear_quantizer_scale[quantizer_scale_code];
    else return quantizer_scale_code << 1;
}

static inline int get_motion_delta (const int f_code)
{
#define bit_buf (inbitbuf)

    int delta;
    int sign;
    const MVtab * tab;

    if (bit_buf & 0x80000000)
	{
		COPYBITS (bit_buf, bits, 1);
		return 0;
    }
	else if (bit_buf >= 0x0c000000)
	{

		tab = MV_4 + UBITS (bit_buf, 4);
		delta = (tab->delta << f_code) + 1;
		COPYBITS (bit_buf, bits, tab->len);
	
		sign = SBITS (bit_buf, 1);
		COPYBITS (bit_buf, bits, 1);
	
		if (f_code) delta += UBITS (bit_buf, f_code);
		COPYBITS (bit_buf, bits, f_code);
	
		return (delta ^ sign) - sign;
    }
	else
	{

		tab = MV_10 + UBITS (bit_buf, 10);
		delta = (tab->delta << f_code) + 1;
		COPYBITS (bit_buf, bits, tab->len);
	
		sign = SBITS (bit_buf, 1);
		COPYBITS (bit_buf, bits, 1);
	
		if (f_code)
		{
			delta += UBITS (bit_buf, f_code);
			COPYBITS (bit_buf, bits, f_code);
		}
	
		return (delta ^ sign) - sign;
    }
}


static inline int get_dmv ()
{
    const DMVtab * tab;

    tab = DMV_2 + UBITS (bit_buf, 2);
    COPYBITS (bit_buf, bits, tab->len);
    return tab->dmv;
}

static inline int get_coded_block_pattern ()
{
#define bit_buf (inbitbuf)
    const CBPtab * tab;

    if (bit_buf >= 0x20000000)
	{
		tab = CBP_7 + (UBITS (bit_buf, 7) - 16);
		DUMPBITS (bit_buf, bits, tab->len);
		return tab->cbp;
    }
	else
	{
		tab = CBP_9 + UBITS (bit_buf, 9);
		DUMPBITS (bit_buf, bits, tab->len);
		return tab->cbp;
    }
}

static inline int get_luma_dc_dct_diff ()
{
#define bit_buf (inbitbuf)
    const DCtab * tab;
    int size;
    int dc_diff;

    if (bit_buf < 0xf8000000)
	{
		tab = DC_lum_5 + UBITS (bit_buf, 5);
		size = tab->size;
		if (size)
		{
			COPYBITS (bit_buf, bits, tab->len);
			//dc_diff = UBITS (bit_buf, size) - UBITS (SBITS (~bit_buf, 1), size);
			dc_diff = UBITS (bit_buf, size); if (!(dc_diff >> (size - 1))) dc_diff = (dc_diff + 1) - (1 << size);
			COPYBITS (bit_buf, bits, size);
			return dc_diff;
		}
		else
		{
			COPYBITS (bit_buf, bits, 3);
			return 0;
		}
    }
	else
	{
		tab = DC_long + (UBITS (bit_buf, 9) - 0x1e0);
		size = tab->size;
		COPYBITS (bit_buf, bits, tab->len);
		//dc_diff = UBITS (bit_buf, size) - UBITS (SBITS (~bit_buf, 1), size);
		dc_diff = UBITS (bit_buf, size); if (!(dc_diff >> (size - 1))) dc_diff = (dc_diff + 1) - (1 << size);
		COPYBITS (bit_buf, bits, size);
		return dc_diff;
    }
}

static inline int get_chroma_dc_dct_diff ()
{
#define bit_buf (inbitbuf)

    const DCtab * tab;
    int size;
    int dc_diff;

    if (bit_buf < 0xf8000000)
	{
		tab = DC_chrom_5 + UBITS (bit_buf, 5);
		size = tab->size;
		if (size)
		{
			COPYBITS (bit_buf, bits, tab->len);
			//dc_diff = UBITS (bit_buf, size) - UBITS (SBITS (~bit_buf, 1), size);
			dc_diff = UBITS (bit_buf, size); if (!(dc_diff >> (size - 1))) dc_diff = (dc_diff + 1) - (1 << size);
			COPYBITS (bit_buf, bits, size);
			return dc_diff;
		} else
		{
			COPYBITS (bit_buf, bits, 2);
			return 0;
		}
    }
	else
	{
		tab = DC_long + (UBITS (bit_buf, 10) - 0x3e0);
		size = tab->size;
		COPYBITS (bit_buf, bits, tab->len + 1);
		//dc_diff = UBITS (bit_buf, size) - UBITS (SBITS (~bit_buf, 1), size);
		dc_diff = UBITS (bit_buf, size); if (!(dc_diff >> (size - 1))) dc_diff = (dc_diff + 1) - (1 << size);
		COPYBITS (bit_buf, bits, size);
		return dc_diff;
    }
}

static void get_intra_block_B14 ()
{
#define bit_buf (inbitbuf)
	int q = quantizer_scale, nq = new_quantizer_scale, tst;
    int i, li;
    int val;
    const DCTtab * tab;
	
    /* Basic sanity check --Meuuh */
    if ( q == 0 )
        return;

    tst = (nq / q) + ((nq % q) ? 1 : 0);

    li = i = 0;

    while (1)
	{
		if (bit_buf >= 0x28000000)
		{
			tab = DCT_B14AC_5 + (UBITS (bit_buf, 5) - 5);
	
			i += tab->run;
			if (i >= 64) break;	/* end of block */
	
	normal_code:
			DUMPBITS (bit_buf, bits, tab->len);
			val = tab->level;
			if (val >= tst)
			{
				val = (val ^ SBITS (bit_buf, 1)) - SBITS (bit_buf, 1);
				putAC(i - li - 1, (val * q) / nq, 0);
				li = i;
			}
	
			DUMPBITS (bit_buf, bits, 1);
	
			continue;
		}
		else if (bit_buf >= 0x04000000)
		{
			tab = DCT_B14_8 + (UBITS (bit_buf, 8) - 4);
	
			i += tab->run;
			if (i < 64) goto normal_code;
	
			/* escape code */
			i += (UBITS (bit_buf, 12) & 0x3F) - 64;
			if (i >= 64) break;	/* illegal, check needed to avoid buffer overflow */
	
			DUMPBITS (bit_buf, bits, 12);
			val = SBITS (bit_buf, 12);
			if (abs(val) >= tst)
			{
				putAC(i - li - 1, (val * q) / nq, 0);
				li = i;
			}
	
			DUMPBITS (bit_buf, bits, 12);
	
			continue;
		}
		else if (bit_buf >= 0x02000000)
		{
			tab = DCT_B14_10 + (UBITS (bit_buf, 10) - 8);
			i += tab->run;
			if (i < 64) goto normal_code;
		}
		else if (bit_buf >= 0x00800000)
		{
			tab = DCT_13 + (UBITS (bit_buf, 13) - 16);
			i += tab->run;
			if (i < 64) goto normal_code;
		}
		else if (bit_buf >= 0x00200000)
		{
			tab = DCT_15 + (UBITS (bit_buf, 15) - 16);
			i += tab->run;
			if (i < 64) goto normal_code;
		}
		else
		{
			tab = DCT_16 + UBITS (bit_buf, 16);
			DUMPBITS (bit_buf, bits, 16);
			i += tab->run;
			if (i < 64) goto normal_code;
		}
		break;	/* illegal, check needed to avoid buffer overflow */
	}
	
	COPYBITS (bit_buf, bits, 2);	/* end of block code */
}

static void get_intra_block_B15 ()
{
#define bit_buf (inbitbuf)
	int q = quantizer_scale, nq = new_quantizer_scale, tst;
    int i, li;
    int val;
    const DCTtab * tab;
	
    /* Basic sanity check --Meuuh */
    if ( q == 0 )
        return;

    tst = (nq / q) + ((nq % q) ? 1 : 0);

    li = i = 0;

    while (1)
	{
		if (bit_buf >= 0x04000000)
		{
			tab = DCT_B15_8 + (UBITS (bit_buf, 8) - 4);
	
			i += tab->run;
			if (i < 64)
			{
	normal_code:
				DUMPBITS (bit_buf, bits, tab->len);
				
				val = tab->level;
				if (val >= tst)
				{
					val = (val ^ SBITS (bit_buf, 1)) - SBITS (bit_buf, 1);
					putAC(i - li - 1, (val * q) / nq, 1);
					li = i;
				}
		
				DUMPBITS (bit_buf, bits, 1);
		
				continue;
			}
			else
			{
				i += (UBITS (bit_buf, 12) & 0x3F) - 64;
				
				if (i >= 64) break;	/* illegal, check against buffer overflow */
		
				DUMPBITS (bit_buf, bits, 12);
				val = SBITS (bit_buf, 12);
				if (abs(val) >= tst)
				{
					putAC(i - li - 1, (val * q) / nq, 1);
					li = i;
				}
		
				DUMPBITS (bit_buf, bits, 12);
		
				continue;
			}
		}
		else if (bit_buf >= 0x02000000)
		{
			tab = DCT_B15_10 + (UBITS (bit_buf, 10) - 8);
			i += tab->run;
			if (i < 64) goto normal_code;
		}
		else if (bit_buf >= 0x00800000)
		{
			tab = DCT_13 + (UBITS (bit_buf, 13) - 16);
			i += tab->run;
			if (i < 64) goto normal_code;
		}
		else if (bit_buf >= 0x00200000)
		{
			tab = DCT_15 + (UBITS (bit_buf, 15) - 16);
			i += tab->run;
			if (i < 64) goto normal_code;
		}
		else
		{
			tab = DCT_16 + UBITS (bit_buf, 16);
			DUMPBITS (bit_buf, bits, 16);
			i += tab->run;
			if (i < 64) goto normal_code;
		}
		break;	/* illegal, check needed to avoid buffer overflow */
	}

	COPYBITS (bit_buf, bits, 4);	/* end of block code */
}


static int get_non_intra_block_drop (RunLevel *blk)
{
#define bit_buf (inbitbuf)

    int i, li;
    int val;
    const DCTtab * tab;
	RunLevel *sblk = blk + 1;

    li = i = -1;

    if (bit_buf >= 0x28000000)
	{
		tab = DCT_B14DC_5 + (UBITS (bit_buf, 5) - 5);
		goto entry_1;
    }
	else goto entry_2;

    while (1)
	{
		if (bit_buf >= 0x28000000)
		{
			tab = DCT_B14AC_5 + (UBITS (bit_buf, 5) - 5);
	
	entry_1:
			i += tab->run;
			if (i >= 64) break;	/* end of block */
	
	normal_code:
	
			DUMPBITS (bit_buf, bits, tab->len);
			val = tab->level;
			val = (val ^ SBITS (bit_buf, 1)) - SBITS (bit_buf, 1); /* if (bitstream_get (1)) val = -val; */
	
			blk->level = val;
			blk->run = i - li - 1;
			li = i;
			blk++;
	
			DUMPBITS (bit_buf, bits, 1);
	
			continue;
		}
	
	entry_2:
	
		if (bit_buf >= 0x04000000)
		{
			tab = DCT_B14_8 + (UBITS (bit_buf, 8) - 4);
	
			i += tab->run;
			if (i < 64) goto normal_code;
	
			/* escape code */
	
			i += (UBITS (bit_buf, 12) & 0x3F) - 64;
			
			if (i >= 64) break;	/* illegal, check needed to avoid buffer overflow */
	
			DUMPBITS (bit_buf, bits, 12);
			val = SBITS (bit_buf, 12);

			blk->level = val;
			blk->run = i - li - 1;
			li = i;
			blk++;
			
			DUMPBITS (bit_buf, bits, 12);
	
			continue;
		}
		else if (bit_buf >= 0x02000000)
		{
			tab = DCT_B14_10 + (UBITS (bit_buf, 10) - 8);
			i += tab->run;
			if (i < 64) goto normal_code;
		}
		else if (bit_buf >= 0x00800000)
		{
			tab = DCT_13 + (UBITS (bit_buf, 13) - 16);
			i += tab->run;
			if (i < 64) goto normal_code;
		}
		else if (bit_buf >= 0x00200000)
		{
			tab = DCT_15 + (UBITS (bit_buf, 15) - 16);
			i += tab->run;
			if (i < 64) goto normal_code;
		}
		else
		{
			tab = DCT_16 + UBITS (bit_buf, 16);
			DUMPBITS (bit_buf, bits, 16);
			i += tab->run;
			if (i < 64) goto normal_code;
		}
		break;	/* illegal, check needed to avoid buffer overflow */
	}
    DUMPBITS (bit_buf, bits, 2);	/* dump end of block code */
	
	// remove last coeff
	if (blk != sblk) 
	{
		blk--;
		// remove more coeffs if very late
		if ((quant_corr < -60.0f) && (blk != sblk))
		{
			blk--;
			if ((quant_corr < -80.0f) && (blk != sblk))
			{
				blk--;
				if ((quant_corr < -100.0f) && (blk != sblk))
				{
					blk--;
					if ((quant_corr < -120.0f) && (blk != sblk))
						blk--;
				}
			}
		}
	}

	blk->level = 0;
	
    return i;
}

static int get_non_intra_block_rq (RunLevel *blk)
{
#define bit_buf (inbitbuf)
	int q = quantizer_scale, nq = new_quantizer_scale, tst;
    int i, li;
    int val;
    const DCTtab * tab;

    /* Basic sanity check --Meuuh */
    if ( q == 0 )
        return 0;

    tst = (nq / q) + ((nq % q) ? 1 : 0);

    li = i = -1;

    if (bit_buf >= 0x28000000)
	{
		tab = DCT_B14DC_5 + (UBITS (bit_buf, 5) - 5);
		goto entry_1;
    }
	else goto entry_2;

    while (1)
	{
		if (bit_buf >= 0x28000000)
		{
			tab = DCT_B14AC_5 + (UBITS (bit_buf, 5) - 5);
	
	entry_1:
			i += tab->run;
			if (i >= 64)
			break;	/* end of block */
	
	normal_code:
	
			DUMPBITS (bit_buf, bits, tab->len);
			val = tab->level;
			if (val >= tst)
			{
				val = (val ^ SBITS (bit_buf, 1)) - SBITS (bit_buf, 1);
				blk->level = (val * q) / nq;
				blk->run = i - li - 1;
				li = i;
				blk++;
			}
			
			//if ( ((val) && (tab->level < tst)) || ((!val) && (tab->level >= tst)) )
			//	LOGF("level: %i val: %i tst : %i q: %i nq : %i\n", tab->level, val, tst, q, nq);
	
			DUMPBITS (bit_buf, bits, 1);
	
			continue;
		}
	
	entry_2:
		if (bit_buf >= 0x04000000)
		{
			tab = DCT_B14_8 + (UBITS (bit_buf, 8) - 4);
	
			i += tab->run;
			if (i < 64) goto normal_code;
	
			/* escape code */
	
			i += (UBITS (bit_buf, 12) & 0x3F) - 64;
			
			if (i >= 64) break;	/* illegal, check needed to avoid buffer overflow */
	
			DUMPBITS (bit_buf, bits, 12);
			val = SBITS (bit_buf, 12);
			if (abs(val) >= tst)
			{
				blk->level = (val * q) / nq;
				blk->run = i - li - 1;
				li = i;
				blk++;
			}
			
			DUMPBITS (bit_buf, bits, 12);
	
			continue;
		}
		else if (bit_buf >= 0x02000000)
		{
			tab = DCT_B14_10 + (UBITS (bit_buf, 10) - 8);
			i += tab->run;
			if (i < 64) goto normal_code;
		}
		else if (bit_buf >= 0x00800000)
		{
			tab = DCT_13 + (UBITS (bit_buf, 13) - 16);
			i += tab->run;
			if (i < 64) goto normal_code;
		}
		else if (bit_buf >= 0x00200000)
		{
			tab = DCT_15 + (UBITS (bit_buf, 15) - 16);
			i += tab->run;
			if (i < 64) goto normal_code;
		}
		else
		{
			tab = DCT_16 + UBITS (bit_buf, 16);
			DUMPBITS (bit_buf, bits, 16);
			
			i += tab->run;
			if (i < 64) goto normal_code;
		}
		break;	/* illegal, check needed to avoid buffer overflow */
	}
    DUMPBITS (bit_buf, bits, 2);	/* dump end of block code */
	
	blk->level = 0;

    return i;
}

static inline void slice_intra_DCT (const int cc)
{
    if (cc == 0)	get_luma_dc_dct_diff ();
    else			get_chroma_dc_dct_diff ();

    if (intra_vlc_format) get_intra_block_B15 ();
    else get_intra_block_B14 ();
}

static inline void slice_non_intra_DCT (int cur_block)
{
	if (picture_coding_type == P_TYPE) get_non_intra_block_drop(block[cur_block]);
	else get_non_intra_block_rq(block[cur_block]);
}

static void motion_fr_frame ( uint f_code[2] )
{
	get_motion_delta (f_code[0]);
	get_motion_delta (f_code[1]);
}

static void motion_fr_field ( uint f_code[2] )
{
    COPYBITS (bit_buf, bits, 1);

	get_motion_delta (f_code[0]);
	get_motion_delta (f_code[1]);

    COPYBITS (bit_buf, bits, 1);

	get_motion_delta (f_code[0]);
	get_motion_delta (f_code[1]);
}

static void motion_fr_dmv ( uint f_code[2] )
{
    get_motion_delta (f_code[0]);
	get_dmv ();

	get_motion_delta (f_code[1]);
	get_dmv ();
}

/* like motion_frame, but parsing without actual motion compensation */
static void motion_fr_conceal ( )
{
	get_motion_delta (f_code[0][0]);
	get_motion_delta (f_code[0][1]);

    COPYBITS (bit_buf, bits, 1); /* remove marker_bit */
}

static void motion_fi_field ( uint f_code[2] )
{
    COPYBITS (bit_buf, bits, 1);

	get_motion_delta (f_code[0]);
	get_motion_delta (f_code[1]);
}

static void motion_fi_16x8 ( uint f_code[2] )
{
    COPYBITS (bit_buf, bits, 1);

	get_motion_delta (f_code[0]);
	get_motion_delta (f_code[1]);

    COPYBITS (bit_buf, bits, 1);

	get_motion_delta (f_code[0]);
	get_motion_delta (f_code[1]);
}

static void motion_fi_dmv ( uint f_code[2] )
{
	get_motion_delta (f_code[0]);
    get_dmv ();

    get_motion_delta (f_code[1]);
	get_dmv ();
}

static void motion_fi_conceal ()
{
    COPYBITS (bit_buf, bits, 1); /* remove field_select */

	get_motion_delta (f_code[0][0]);
	get_motion_delta (f_code[0][1]);

    COPYBITS (bit_buf, bits, 1); /* remove marker_bit */
}

#define MOTION_CALL(routine,direction) 						\
do {														\
    if ((direction) & MACROBLOCK_MOTION_FORWARD)			\
		routine (f_code[0]);								\
    if ((direction) & MACROBLOCK_MOTION_BACKWARD)			\
		routine (f_code[1]);								\
} while (0)

#define NEXT_MACROBLOCK											\
do {															\
    h_offset += 16;												\
    if (h_offset == horizontal_size_value) 						\
	{															\
		v_offset += 16;											\
		if (v_offset > (vertical_size_value - 16)) return;		\
		h_offset = 0;											\
    }															\
} while (0)

void putmbdata(int macroblock_modes)
{
		putmbtype(macroblock_modes & 0x1F);
		
		switch (picture_coding_type)
		{
			case I_TYPE:
				if ((! (frame_pred_frame_dct)) && (picture_structure == FRAME_PICTURE))
					putbits(macroblock_modes & DCT_TYPE_INTERLACED ? 1 : 0, 1);
				break;
		
			case P_TYPE:
				if (picture_structure != FRAME_PICTURE)
				{
					if (macroblock_modes & MACROBLOCK_MOTION_FORWARD)
						putbits((macroblock_modes & MOTION_TYPE_MASK) / MOTION_TYPE_BASE, 2);
					break;
				}
				else if (frame_pred_frame_dct) break;
				else
				{
					if (macroblock_modes & MACROBLOCK_MOTION_FORWARD)
						putbits((macroblock_modes & MOTION_TYPE_MASK) / MOTION_TYPE_BASE, 2);
					if (macroblock_modes & (MACROBLOCK_INTRA | MACROBLOCK_PATTERN))
						putbits(macroblock_modes & DCT_TYPE_INTERLACED ? 1 : 0, 1);
					break;
				}
		
			case B_TYPE:
				if (picture_structure != FRAME_PICTURE)
				{
					if (! (macroblock_modes & MACROBLOCK_INTRA))
						putbits((macroblock_modes & MOTION_TYPE_MASK) / MOTION_TYPE_BASE, 2);
					break;
				}
				else if (frame_pred_frame_dct) break;
				else
				{
					if (macroblock_modes & MACROBLOCK_INTRA) goto intra;
					putbits((macroblock_modes & MOTION_TYPE_MASK) / MOTION_TYPE_BASE, 2);
					if (macroblock_modes & (MACROBLOCK_INTRA | MACROBLOCK_PATTERN))
					{
						intra:
						putbits(macroblock_modes & DCT_TYPE_INTERLACED ? 1 : 0, 1);
					}
					break;
				}
		}

}

static inline void put_quantiser(int quantiser)
{
	putbits(q_scale_type ? map_non_linear_mquant[quantiser] : quantiser >> 1, 5);
	last_coded_scale = quantiser;
}

static int slice_init (int code)
{
#define bit_buf (inbitbuf)

    int offset;
    const MBAtab * mba;

    v_offset = (code - 1) * 16;

    quantizer_scale = get_quantizer_scale ();
	if (picture_coding_type == P_TYPE) new_quantizer_scale = quantizer_scale;
	else new_quantizer_scale = getNewQuant(quantizer_scale);
	put_quantiser(new_quantizer_scale);
	
	/*LOGF("************************\nstart of slice %i in %s picture. ori quant: %i new quant: %i\n", code,
		(picture_coding_type == I_TYPE ? "I_TYPE" : (picture_coding_type == P_TYPE ? "P_TYPE" : "B_TYPE")),
		quantizer_scale, new_quantizer_scale);*/

    /* ignore intra_slice and all the extra data */
    while (bit_buf & 0x80000000)
	{
		DUMPBITS (bit_buf, bits, 9);
    }

    /* decode initial macroblock address increment */
    offset = 0;
    while (1)
	{
		if (bit_buf >= 0x08000000)
		{
			mba = MBA_5 + (UBITS (bit_buf, 6) - 2);
			break;
		}
		else if (bit_buf >= 0x01800000)
		{
			mba = MBA_11 + (UBITS (bit_buf, 12) - 24);
			break;
		}
		else switch (UBITS (bit_buf, 12))
		{
			case 8:		/* macroblock_escape */
				offset += 33;
				COPYBITS (bit_buf, bits, 11);
				continue;
			default:	/* error */
				return 1;
		}
    }

    COPYBITS (bit_buf, bits, mba->len + 1);
    h_offset = (offset + mba->mba) << 4;

    while (h_offset - (int)horizontal_size_value >= 0)
	{
		h_offset -= horizontal_size_value;
		v_offset += 16;
    }

    if (v_offset > (vertical_size_value - 16)) return 1;

    return 0;

}

void mpeg2_slice ( const int code )
{
#define bit_buf (inbitbuf)

    if (slice_init (code)) return;

    while (1)
	{
		int macroblock_modes;
		int mba_inc;
		const MBAtab * mba;
	
		macroblock_modes = get_macroblock_modes ();
		if (macroblock_modes & MACROBLOCK_QUANT) quantizer_scale = get_quantizer_scale ();
		
		//LOGF("blk %i : ", h_offset >> 4);

		if (macroblock_modes & MACROBLOCK_INTRA)
		{
			//LOG("intra "); if (macroblock_modes & MACROBLOCK_QUANT) LOGF("got new quant: %i ", quantizer_scale);

			new_quantizer_scale = increment_quant(quantizer_scale);
			if (last_coded_scale == new_quantizer_scale) macroblock_modes &= 0xFFFFFFEF; // remove MACROBLOCK_QUANT
			else macroblock_modes |= MACROBLOCK_QUANT; //add MACROBLOCK_QUANT
			putmbdata(macroblock_modes);
			if (macroblock_modes & MACROBLOCK_QUANT) put_quantiser(new_quantizer_scale);
			
			//if (macroblock_modes & MACROBLOCK_QUANT) LOGF("put new quant: %i ", new_quantizer_scale);
		
			if (concealment_motion_vectors)
			{
				if (picture_structure == FRAME_PICTURE) motion_fr_conceal ();
				else motion_fi_conceal ();
			}
			
			slice_intra_DCT ( 0);
			slice_intra_DCT ( 0);
			slice_intra_DCT ( 0);
			slice_intra_DCT ( 0);
			slice_intra_DCT ( 1);
			slice_intra_DCT ( 2);
		}
		else
		{
			int new_coded_block_pattern = 0;

			// begin saving data
			int batb;
			uint8	n_owbuf[32], *n_wbuf,
					*o_owbuf = owbuf, *o_wbuf = wbuf;
			uint32	n_outbitcnt, n_outbitbuf,
					o_outbitcnt = outbitcnt, o_outbitbuf = outbitbuf;

			outbitbuf = 0; outbitcnt = BITS_IN_BUF;
			owbuf = wbuf = n_owbuf;

			if (picture_structure == FRAME_PICTURE)
				switch (macroblock_modes & MOTION_TYPE_MASK)
				{
					case MC_FRAME: MOTION_CALL (motion_fr_frame, macroblock_modes); break;
					case MC_FIELD: MOTION_CALL (motion_fr_field, macroblock_modes); break;
					case MC_DMV: MOTION_CALL (motion_fr_dmv, MACROBLOCK_MOTION_FORWARD); break;
				}
			else
				switch (macroblock_modes & MOTION_TYPE_MASK)
				{
					case MC_FIELD: MOTION_CALL (motion_fi_field, macroblock_modes); break;
					case MC_16X8: MOTION_CALL (motion_fi_16x8, macroblock_modes); break;
					case MC_DMV: MOTION_CALL (motion_fi_dmv, MACROBLOCK_MOTION_FORWARD); break;
				}
				
			assert(wbuf - owbuf < 32);
			
			n_wbuf = wbuf;
			n_outbitcnt = outbitcnt;
			n_outbitbuf = outbitbuf;
			assert(owbuf == n_owbuf);
			
			outbitcnt = o_outbitcnt;
			outbitbuf = o_outbitbuf;
			owbuf = o_owbuf;
			wbuf = o_wbuf;
			// end saving data
			
			if (picture_coding_type == P_TYPE) new_quantizer_scale = quantizer_scale;
			else new_quantizer_scale = getNewQuant(quantizer_scale);
			
			//LOG("non intra "); if (macroblock_modes & MACROBLOCK_QUANT) LOGF("got new quant: %i ", quantizer_scale);
	
			if (macroblock_modes & MACROBLOCK_PATTERN)
			{
				int coded_block_pattern = get_coded_block_pattern ();
				
				if (coded_block_pattern & 0x20) slice_non_intra_DCT(0);
				if (coded_block_pattern & 0x10) slice_non_intra_DCT(1);
				if (coded_block_pattern & 0x08) slice_non_intra_DCT(2);
				if (coded_block_pattern & 0x04) slice_non_intra_DCT(3);
				if (coded_block_pattern & 0x02) slice_non_intra_DCT(4);
				if (coded_block_pattern & 0x01) slice_non_intra_DCT(5);
				
				if (picture_coding_type == B_TYPE)
				{
					if (coded_block_pattern & 0x20) if (isNotEmpty(block[0])) new_coded_block_pattern |= 0x20;
					if (coded_block_pattern & 0x10) if (isNotEmpty(block[1])) new_coded_block_pattern |= 0x10;
					if (coded_block_pattern & 0x08) if (isNotEmpty(block[2])) new_coded_block_pattern |= 0x08;
					if (coded_block_pattern & 0x04) if (isNotEmpty(block[3])) new_coded_block_pattern |= 0x04;
					if (coded_block_pattern & 0x02) if (isNotEmpty(block[4])) new_coded_block_pattern |= 0x02;
					if (coded_block_pattern & 0x01) if (isNotEmpty(block[5])) new_coded_block_pattern |= 0x01;
					if (!new_coded_block_pattern) macroblock_modes &= 0xFFFFFFED; // remove MACROBLOCK_PATTERN and MACROBLOCK_QUANT flag
				}
				else new_coded_block_pattern = coded_block_pattern;
			}

			if (last_coded_scale == new_quantizer_scale) macroblock_modes &= 0xFFFFFFEF; // remove MACROBLOCK_QUANT
			else if (macroblock_modes & MACROBLOCK_PATTERN) macroblock_modes |= MACROBLOCK_QUANT; //add MACROBLOCK_QUANT
			assert( (macroblock_modes & MACROBLOCK_PATTERN) || !(macroblock_modes & MACROBLOCK_QUANT) );
			
			putmbdata(macroblock_modes);
			if (macroblock_modes & MACROBLOCK_QUANT) put_quantiser(new_quantizer_scale);

			//if (macroblock_modes & MACROBLOCK_PATTERN) LOG("coded ");
			//if (macroblock_modes & MACROBLOCK_QUANT) LOGF("put new quant: %i ", new_quantizer_scale);

			// put saved motion data...
			for (batb = 0; batb < (n_wbuf - n_owbuf); batb++) putbits(n_owbuf[batb], 8);
			putbits(n_outbitbuf, BITS_IN_BUF - n_outbitcnt);
			// end saved motion data...
			
			if (macroblock_modes & MACROBLOCK_PATTERN)
			{
				putcbp(new_coded_block_pattern);
				
				if (new_coded_block_pattern & 0x20) putnonintrablk(block[0]);
				if (new_coded_block_pattern & 0x10) putnonintrablk(block[1]);
				if (new_coded_block_pattern & 0x08) putnonintrablk(block[2]);
				if (new_coded_block_pattern & 0x04) putnonintrablk(block[3]);
				if (new_coded_block_pattern & 0x02) putnonintrablk(block[4]);
				if (new_coded_block_pattern & 0x01) putnonintrablk(block[5]);
			}
		}
	
		//LOGF("\n\to: %i c: %i n: %i\n", quantizer_scale, last_coded_scale, new_quantizer_scale);
	
		NEXT_MACROBLOCK;
	
		mba_inc = 0;
		while (1)
		{
			if (bit_buf >= 0x10000000)
			{
				mba = MBA_5 + (UBITS (bit_buf, 5) - 2);
				break;
			}
			else if (bit_buf >= 0x03000000)
			{
				mba = MBA_11 + (UBITS (bit_buf, 11) - 24);
				break;
			}
			else
				switch (UBITS (bit_buf, 11))
				{
					case 8:		/* macroblock_escape */
						mba_inc += 33;
						COPYBITS (bit_buf, bits, 11);
						continue;
					default:	/* end of slice, or error */
						return;
				}
		}
		COPYBITS (bit_buf, bits, mba->len);
		mba_inc += mba->mba;
	
		if (mba_inc) do { NEXT_MACROBLOCK; } while (--mba_inc);
    }

}

/////---- end ext mpeg code

static int do_next_start_code(void)
{
	uint8 ID;

    // get start code
    LOCK(1)
    ID = cbuf[0];
    COPY(1)

    if (ID == 0x00) // pic header
    {
        LOCK(4)
        picture_coding_type = (cbuf[1] >> 3) & 0x7;
        cbuf[1] |= 0x7; cbuf[2] = 0xFF; cbuf[3] |= 0xF8; // vbv_delay is now 0xFFFF
        COPY(4)
    }
    else if (ID == 0xB3) // seq header
    {
        LOCK(8)
        horizontal_size_value = (cbuf[0] << 4) | (cbuf[1] >> 4);
        vertical_size_value = ((cbuf[1] & 0xF) << 8) | cbuf[2];
        if (!horizontal_size_value || !vertical_size_value)
            return -1;
        COPY(8)
    }
    else if (ID == 0xB5) // extension
    {
        LOCK(1)
        if ((cbuf[0] >> 4) == 0x8) // pic coding ext
        {
            LOCK(5)

            f_code[0][0] = (cbuf[0] & 0xF) - 1;
            f_code[0][1] = (cbuf[1] >> 4) - 1;
            f_code[1][0] = (cbuf[1] & 0xF) - 1;
            f_code[1][1] = (cbuf[2] >> 4) - 1;
            
            intra_dc_precision = (cbuf[2] >> 2) & 0x3;
            picture_structure = cbuf[2] & 0x3;
            frame_pred_frame_dct = (cbuf[3] >> 6) & 0x1;
            concealment_motion_vectors = (cbuf[3] >> 5) & 0x1;
            q_scale_type = (cbuf[3] >> 4) & 0x1;
            intra_vlc_format = (cbuf[3] >> 3) & 0x1;
            alternate_scan = (cbuf[3] >> 2) & 0x1;
            COPY(5)
        }
        else
        {
            COPY(1)
        }
    }
    else if (ID == 0xB8) // gop header
    {
        LOCK(4)
        COPY(4)
    }
    else if ((ID >= 0x01) && (ID <= 0xAF)) // slice
    {
        uint8 *outTemp = wbuf, *inTemp = cbuf;
    
        if 	(		((picture_coding_type == B_TYPE) && (quant_corr < 2.5f)) // don't recompress if we're in advance!
                ||	((picture_coding_type == P_TYPE) && (quant_corr < -2.5f))
                ||	((picture_coding_type == I_TYPE) && (quant_corr < -5.0f))
            )
        {
            uint8 *nsc = cbuf;
            int fsc = 0, toLock;
            
            if (!horizontal_size_value || !vertical_size_value)
                return -1;

            // lock all the slice
            while (!fsc)
            {
                toLock = nsc - cbuf + 3;
                LOCK(toLock)

                if ( (nsc[0] == 0) && (nsc[1] == 0) && (nsc[2] == 1) ) fsc = 1; // start code !
                else nsc++; // continue search
            }
        
            // init bit buffer
            inbitbuf = 0; inbitcnt = 0;
            outbitbuf = 0; outbitcnt = BITS_IN_BUF;
            
            // get 32 bits
            Refill_bits();
            Refill_bits();
            Refill_bits();
            Refill_bits();
        
            // begin bit level recoding
            mpeg2_slice(ID);
            flush_read_buffer();
            flush_write_buffer();
            // end bit level recoding

            /* Basic sanity checks --Meuuh */
            if (cbuf > rbuf || wbuf > rwbuf)
                return -1;
            
            /*LOGF("type: %s code: %02i in : %6i out : %6i diff : %6i fact: %2.2f\n",
            (picture_coding_type == I_TYPE ? "I_TYPE" : (picture_coding_type == P_TYPE ? "P_TYPE" : "B_TYPE")),
            ID,  cbuf - inTemp, wbuf - outTemp, (wbuf - outTemp) - (cbuf - inTemp), (float)(cbuf - inTemp) / (float)(wbuf - outTemp));*/
            
            if (wbuf - outTemp > cbuf - inTemp) // yes that might happen, rarely
            {
                /*LOGF("*** slice bigger than before !! (type: %s code: %i in : %i out : %i diff : %i)\n",
                (picture_coding_type == I_TYPE ? "I_TYPE" : (picture_coding_type == P_TYPE ? "P_TYPE" : "B_TYPE")),
                ID, cbuf - inTemp, wbuf - outTemp, (wbuf - outTemp) - (cbuf - inTemp));*/
            
                // in this case, we'll just use the original slice !
                memcpy(outTemp, inTemp, cbuf - inTemp);
                wbuf = outTemp + (cbuf - inTemp);
                
                // adjust outbytecnt
                outbytecnt -= (wbuf - outTemp) - (cbuf - inTemp);
            }
        }
    }
    return 0;
}

static void process_frame( sout_stream_t *p_stream,
                sout_stream_id_t *id, sout_buffer_t *in, sout_buffer_t **out )
{
    uint8_t             found;
    sout_buffer_t       *p_out;

    /* The output buffer can't be bigger than the input buffer. */
    p_out = sout_BufferNew( p_stream->p_sout, in->i_size );

    p_out->i_length = in->i_length;
    p_out->i_dts    = in->i_dts;
    p_out->i_pts    = in->i_pts;

    sout_BufferChain( out, p_out );

    rwbuf = owbuf = wbuf = p_out->p_buffer;
    cbuf = rbuf = in->p_buffer;
    rbuf += in->i_size + 4;
    rwbuf += in->i_size;
    *(in->p_buffer + in->i_size) = 0;
    *(in->p_buffer + in->i_size + 1) = 0;
    *(in->p_buffer + in->i_size + 2) = 1;
    *(in->p_buffer + in->i_size + 3) = 0;
    inbytecnt += in->i_size;

    for ( ; ; )
    {
        // get next start code prefix
        found = 0;
        while (!found)
        {
#ifndef REMOVE_BYTE_STUFFING
            LOCK(3)
#else
            LOCK(6)
            if		( (cbuf[0] == 0) && (cbuf[1] == 0) && (cbuf[2] == 0) && (cbuf[3] == 0) && (cbuf[4] == 0) && (cbuf[5] == 0) ) { SEEKR(1) }
            else 
#endif
            if ( (cbuf[0] == 0) && (cbuf[1] == 0) && (cbuf[2] == 1) ) found = 1; // start code !
            else { COPY(1) } // continue search

            if (cbuf >= in->p_buffer + in->i_size)
                break;
        }
        if (cbuf >= in->p_buffer + in->i_size)
            break;
        COPY(3)
        
        if (do_next_start_code() == -1)
            break;

        quant_corr = (((inbytecnt - (rbuf - 4 - cbuf)) / fact_x) - (outbytecnt + (wbuf - owbuf))) / REACT_DELAY;
    }

    outbytecnt += wbuf - owbuf;
    p_out->i_size = wbuf - owbuf;
}

static int transrate_video_process( sout_stream_t *p_stream,
               sout_stream_id_t *id, sout_buffer_t *in, sout_buffer_t **out )
{
    *out = NULL;

    if ( in->i_flags & SOUT_BUFFER_FLAGS_GOP )
    {
        while ( id->p_current_buffer != NULL )
        {
            sout_buffer_t * p_next = id->p_current_buffer->p_next;
            if ( fact_x == 1.0 )
            {
                outbytecnt += id->p_current_buffer->i_size;
                id->p_current_buffer->p_next = NULL;
                sout_BufferChain( out, id->p_current_buffer );
            }
            else
            {
                process_frame( p_stream, id, id->p_current_buffer, out );
                sout_BufferDelete(p_stream->p_sout, id->p_current_buffer);
            }
            id->p_current_buffer = p_next;
        }
            
        if ( id->i_next_gop_duration )
        {
            mtime_t i_bitrate = (mtime_t)id->i_next_gop_size * 8000
                                    / (id->i_next_gop_duration / 1000);
            static mtime_t i_old_bitrate = 0;
            static mtime_t i_old_duration = 0;
            if (i_old_bitrate)
            {
                msg_Dbg(p_stream, "bitrate = %lld -> %lld", i_old_bitrate,
                        (mtime_t)outbytecnt * 8000 / (i_old_duration / 1000));
            }
            i_old_bitrate = i_bitrate;
            i_old_duration = id->i_next_gop_duration;
            if ( i_bitrate > p_sys->i_vbitrate )
            {
                fact_x = (double)i_bitrate / p_sys->i_vbitrate;
            }
            else
            {
                fact_x = 1.0;
            }
            msg_Dbg(p_stream, "new fact_x = %f", fact_x);

            id->p_current_buffer = id->p_next_gop;
            id->p_next_gop = NULL;
            id->i_next_gop_duration = 0;
            id->i_next_gop_size = 0;
            inbytecnt = 0;
            outbytecnt  = 0;
        }
    }

    /* Store the buffer for the next GOP. */
    sout_BufferChain( &id->p_next_gop, in );
    id->i_next_gop_duration += in->i_length;
    id->i_next_gop_size += in->i_size;

    if ( id->p_current_buffer != NULL )
    {
        sout_buffer_t * p_next = id->p_current_buffer->p_next;
        if ( fact_x == 1.0 )
        {
            outbytecnt += id->p_current_buffer->i_size;
            id->p_current_buffer->p_next = NULL;
            sout_BufferChain( out, id->p_current_buffer );
        }
        else
        {
            process_frame( p_stream, id, id->p_current_buffer, out );
            sout_BufferDelete(p_stream->p_sout, id->p_current_buffer);
        }
        id->p_current_buffer = p_next;
    }

    return VLC_SUCCESS;
}

static int transrate_video_new( sout_stream_t *p_stream,
                                sout_stream_id_t *id )
{
    id->p_current_buffer = NULL;
    id->p_next_gop = NULL;
    id->i_next_gop_duration = 0;
    id->i_next_gop_size = 0;

    p_sys = p_stream->p_sys;

	inbytecnt = outbytecnt = 0;

    quant_corr = 0.0;
    fact_x = 1.0;

    return VLC_SUCCESS;
}

static void transrate_video_close ( sout_stream_t *p_stream, sout_stream_id_t *id )
{
}

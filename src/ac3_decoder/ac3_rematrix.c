#include <unistd.h>                                              /* getpid() */

#include <stdio.h>                                           /* "intf_msg.h" */
#include <stdlib.h>                                      /* malloc(), free() */
#include <sys/soundcard.h>                               /* "audio_output.h" */
#include <sys/types.h>
#include <sys/uio.h>                                            /* "input.h" */

#include "common.h"
#include "config.h"
#include "mtime.h"
#include "vlc_thread.h"
#include "debug.h"                                      /* "input_netlist.h" */

#include "intf_msg.h"                        /* intf_DbgMsg(), intf_ErrMsg() */

#include "input.h"                                           /* pes_packet_t */
#include "input_netlist.h"                         /* input_NetlistFreePES() */
#include "decoder_fifo.h"         /* DECODER_FIFO_(ISEMPTY|START|INCSTART)() */

#include "audio_output.h"

#include "ac3_decoder.h"
#include "ac3_parse.h"
#include "ac3_exponent.h"
#include "ac3_bit_allocate.h"
#include "ac3_mantissa.h"
#include "ac3_rematrix.h"

struct rematrix_band_s
{
	u32 start;
	u32 end;
};

static struct rematrix_band_s rematrix_band[] = { {13,24}, {25,36}, {37 ,60}, {61,252}};

static __inline__ u32 min( u32 a, u32 b )
{
	return( a < b ? a : b );
}

/* This routine simply does stereo rematixing for the 2 channel 
 * stereo mode */
void rematrix( ac3dec_thread_t * p_ac3dec )
{
	u32 num_bands;
	u32 start;
	u32 end;
	u32 i,j;
	float left,right;

	if(p_ac3dec->audblk.cplinu || p_ac3dec->audblk.cplbegf > 2)
		num_bands = 4;
	else if (p_ac3dec->audblk.cplbegf > 0)
		num_bands = 3;
	else
		num_bands = 2;

	for(i=0;i < num_bands; i++)
	{
		if(!p_ac3dec->audblk.rematflg[i])
			continue;

		start = rematrix_band[i].start;
		end = min(rematrix_band[i].end ,12 * p_ac3dec->audblk.cplbegf + 36);
	
		for(j=start;j < end; j++)
		{
			left  = 0.5f * (p_ac3dec->coeffs.fbw[0][j] + p_ac3dec->coeffs.fbw[1][j]);
			right = 0.5f * (p_ac3dec->coeffs.fbw[0][j] - p_ac3dec->coeffs.fbw[1][j]);
			p_ac3dec->coeffs.fbw[0][j] = left;
			p_ac3dec->coeffs.fbw[1][j] = right;
		}
	}
}

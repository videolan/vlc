#include <unistd.h>                                              /* getpid() */

#include <stdio.h>                                           /* "intf_msg.h" */
#include <stdlib.h>                                      /* malloc(), free() */
#include <sys/soundcard.h>                               /* "audio_output.h" */
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
#include "ac3_exponent.h"

static const s16 exps_1[128] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5 };
static const s16 exps_2[128] = { 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 0, 0, 0 };
static const s16 exps_3[128] = { 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 1, 2 };

static __inline__ void exp_unpack_ch( u16 type, u16 expstr, u16 ngrps, u16 initial_exp, u16 exps[], u16 * dest )
{
	u16 i,j;
	s16 exp_acc;
	/*
	s16 exp_1,exp_2,exp_3;
	*/

	if(expstr == EXP_REUSE)
		return;

	/* Handle the initial absolute exponent */
	exp_acc = initial_exp;
	j = 0;

	/* In the case of a fbw channel then the initial absolute values is 
	 * also an exponent */
	if(type != UNPACK_CPL)
		dest[j++] = exp_acc;

	/* Loop through the groups and fill the dest array appropriately */
	for ( i = 0; i < ngrps; i++ )
	{
		/*
		if ( exps[i] > 124 )
		{
			//FIXME set an error flag and mute the frame
			fprintf( stderr, "!! Invalid exponent !!\n" );
//			exit( 1 );
		}
		*/

		/*
		exp_1 = exps[i] / 25;
		exp_2 = (exps[i] - (exp_1 * 25)) / 5;
		exp_3 = exps[i] - (exp_1 * 25) - (exp_2 * 5) ;
		*/

		switch ( expstr )
		{
			case EXP_D45:
				exp_acc += (exps_1[exps[i]] - 2);
				dest[j++] = exp_acc;
				dest[j++] = exp_acc;
				dest[j++] = exp_acc;
				dest[j++] = exp_acc;
				exp_acc += (exps_2[exps[i]] - 2);
				dest[j++] = exp_acc;
				dest[j++] = exp_acc;
				dest[j++] = exp_acc;
				dest[j++] = exp_acc;
				exp_acc += (exps_3[exps[i]] - 2);
				dest[j++] = exp_acc;
				dest[j++] = exp_acc;
				dest[j++] = exp_acc;
				dest[j++] = exp_acc;
			break;

			case EXP_D25:
				exp_acc += (exps_1[exps[i]] - 2);
				dest[j++] = exp_acc;
				dest[j++] = exp_acc;
				exp_acc += (exps_2[exps[i]] - 2);
				dest[j++] = exp_acc;
				dest[j++] = exp_acc;
				exp_acc += (exps_3[exps[i]] - 2);
				dest[j++] = exp_acc;
				dest[j++] = exp_acc;
			break;

			case EXP_D15:
				exp_acc += (exps_1[exps[i]] - 2);
				dest[j++] = exp_acc;
				exp_acc += (exps_2[exps[i]] - 2);
				dest[j++] = exp_acc;
				exp_acc += (exps_3[exps[i]] - 2);
				dest[j++] = exp_acc;
			break;
		}
	}
}

void exponent_unpack( ac3dec_thread_t * p_ac3dec )
{
	u16 i;

	for(i=0; i< p_ac3dec->bsi.nfchans; i++)
		exp_unpack_ch(UNPACK_FBW, p_ac3dec->audblk.chexpstr[i], p_ac3dec->audblk.nchgrps[i], p_ac3dec->audblk.exps[i][0],
				&p_ac3dec->audblk.exps[i][1], p_ac3dec->audblk.fbw_exp[i]);

	if(p_ac3dec->audblk.cplinu)
		exp_unpack_ch(UNPACK_CPL, p_ac3dec->audblk.cplexpstr, p_ac3dec->audblk.ncplgrps, p_ac3dec->audblk.cplabsexp << 1,
				p_ac3dec->audblk.cplexps, &p_ac3dec->audblk.cpl_exp[p_ac3dec->audblk.cplstrtmant]);

	if(p_ac3dec->bsi.lfeon)
		exp_unpack_ch(UNPACK_LFE, p_ac3dec->audblk.lfeexpstr, 2, p_ac3dec->audblk.lfeexps[0],
				&p_ac3dec->audblk.lfeexps[1], p_ac3dec->audblk.lfe_exp);
}

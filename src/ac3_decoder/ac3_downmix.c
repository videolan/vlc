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
#include "ac3_downmix.h"

#define NORM 16384

typedef struct prefs_s
{
	u16 use_dolby_surround;
	u16 dual_mono_channel_select;

} prefs_t;

prefs_t global_prefs = {0,0};

//Pre-scaled downmix coefficients
static float cmixlev_lut[4] = { 0.2928, 0.2468, 0.2071, 0.2468 };
static float smixlev_lut[4] = { 0.2928, 0.2071, 0.0   , 0.2071 };

/* Downmix into _two_ channels...other downmix modes aren't implemented
 * to reduce complexity. Realistically, there aren't many machines around
 * with > 2 channel output anyways */

void downmix( ac3dec_t * p_ac3dec, s16 * out_buf )
{
	int j;
	float right_tmp;
	float left_tmp;
	float clev,slev;
	float *centre = 0, *left = 0, *right = 0, *left_sur = 0, *right_sur = 0;

	/*
	if(p_ac3dec->bsi.acmod > 7)
		dprintf("(downmix) invalid acmod number\n");
	*/

	//There are two main cases, with or without Dolby Surround
	if(global_prefs.use_dolby_surround)
	{
		switch(p_ac3dec->bsi.acmod)
		{
			// 3/2
			case 7:
				left      = p_ac3dec->samples.channel[0];
				centre    = p_ac3dec->samples.channel[1];
				right     = p_ac3dec->samples.channel[2];
				left_sur  = p_ac3dec->samples.channel[3];
				right_sur = p_ac3dec->samples.channel[4];

				for ( j = 0; j < 256; j++ )
				{
					right_tmp = 0.2265f * *left_sur++ + 0.2265f * *right_sur++;
					left_tmp  = -1 * right_tmp;
					right_tmp += 0.3204f * *right++ + 0.2265f * *centre;
					left_tmp  += 0.3204f * *left++  + 0.2265f * *centre++;

					*(out_buf++) = left_tmp * NORM;
					*(out_buf++) = right_tmp * NORM;
					/*
					p_ac3dec->samples.channel[1][j] = right_tmp;
					p_ac3dec->samples.channel[0][j] = left_tmp;
					*/
				}
			break;

			// 2/2
			case 6:
				left      = p_ac3dec->samples.channel[0];
				right     = p_ac3dec->samples.channel[1];
				left_sur  = p_ac3dec->samples.channel[2];
				right_sur = p_ac3dec->samples.channel[3];

				for (j = 0; j < 256; j++)
				{
					right_tmp = 0.2265f * *left_sur++ + 0.2265f * *right_sur++;
					left_tmp  = -1 * right_tmp;
					right_tmp += 0.3204f * *right++;
					left_tmp  += 0.3204f * *left++ ;

					*(out_buf++) = left_tmp * NORM;
					*(out_buf++) = right_tmp * NORM;
					/*
					p_ac3dec->samples.channel[1][j] = right_tmp;
					p_ac3dec->samples.channel[0][j] = left_tmp;
					*/
				}
			break;

			// 3/1
			case 5:
				left      = p_ac3dec->samples.channel[0];
				centre    = p_ac3dec->samples.channel[1];
				right     = p_ac3dec->samples.channel[2];
				//Mono surround
				right_sur = p_ac3dec->samples.channel[3];

				for (j = 0; j < 256; j++)
				{
					right_tmp =  0.2265f * *right_sur++;
					left_tmp  = - right_tmp;
					right_tmp += 0.3204f * *right++ + 0.2265f * *centre;
					left_tmp  += 0.3204f * *left++  + 0.2265f * *centre++;

					*(out_buf++) = left_tmp * NORM;
					*(out_buf++) = right_tmp * NORM;
					/*
					p_ac3dec->samples.channel[1][j] = right_tmp;
					p_ac3dec->samples.channel[0][j] = left_tmp;
					*/
				}
			break;

			// 2/1
			case 4:
				left      = p_ac3dec->samples.channel[0];
				right     = p_ac3dec->samples.channel[1];
				//Mono surround
				right_sur = p_ac3dec->samples.channel[2];

				for (j = 0; j < 256; j++)
				{
					right_tmp =  0.2265f * *right_sur++;
					left_tmp  = - right_tmp;
					right_tmp += 0.3204f * *right++;
					left_tmp  += 0.3204f * *left++;

					*(out_buf++) = left_tmp * NORM;
					*(out_buf++) = right_tmp * NORM;
					/*
					p_ac3dec->samples.channel[1][j] = right_tmp;
					p_ac3dec->samples.channel[0][j] = left_tmp;
					*/
				}
			break;

			// 3/0
			case 3:
				left      = p_ac3dec->samples.channel[0];
				centre    = p_ac3dec->samples.channel[1];
				right     = p_ac3dec->samples.channel[2];

				for (j = 0; j < 256; j++)
				{
					right_tmp = 0.3204f * *right++ + 0.2265f * *centre;
					left_tmp  = 0.3204f * *left++  + 0.2265f * *centre++;

					*(out_buf++) = left_tmp * NORM;
					*(out_buf++) = right_tmp * NORM;
					/*
					p_ac3dec->samples.channel[1][j] = right_tmp;
					p_ac3dec->samples.channel[0][j] = left_tmp;
					*/
				}
			break;

			// 2/0
			case 2:
				left = p_ac3dec->samples.channel[0];
				right = p_ac3dec->samples.channel[1];

				for ( j = 0; j < 256; j++ )
				{
					*(out_buf++) = *(left++) * NORM;
					*(out_buf++) = *(right++) * NORM;
				}
			break;

			// 1/0
			case 1:
				//Mono program!
				right = p_ac3dec->samples.channel[0];

				for (j = 0; j < 256; j++)
				{
					right_tmp = 0.7071f * *right++;

					*(out_buf++) = right_tmp * NORM;
					*(out_buf++) = right_tmp * NORM;
					/*
					p_ac3dec->samples.channel[1][j] = right_tmp;
					p_ac3dec->samples.channel[0][j] = right_tmp;
					*/
				}
			break;

			// 1+1
			case 0:
				//Dual mono, output selected by user
				right = p_ac3dec->samples.channel[global_prefs.dual_mono_channel_select];

				for (j = 0; j < 256; j++)
				{
					right_tmp = 0.7071f * *right++;

					*(out_buf++) = right_tmp * NORM;
					*(out_buf++) = right_tmp * NORM;
					/*
					p_ac3dec->samples.channel[1][j] = right_tmp;
					p_ac3dec->samples.channel[0][j] = right_tmp;
					*/
				}
			break;
		}
	}
	else
	{
		//Non-Dolby surround downmixes
		switch(p_ac3dec->bsi.acmod)
		{
			// 3/2
			case 7:
				left      = p_ac3dec->samples.channel[0];
				centre    = p_ac3dec->samples.channel[1];
				right     = p_ac3dec->samples.channel[2];
				left_sur  = p_ac3dec->samples.channel[3];
				right_sur = p_ac3dec->samples.channel[4];

				clev = cmixlev_lut[p_ac3dec->bsi.cmixlev];
				slev = smixlev_lut[p_ac3dec->bsi.surmixlev];

				for (j = 0; j < 256; j++)
				{
					right_tmp= 0.4142f * *right++ + clev * *centre   + slev * *right_sur++;
					left_tmp = 0.4142f * *left++  + clev * *centre++ + slev * *left_sur++;

					*(out_buf++) = left_tmp * NORM;
					*(out_buf++) = right_tmp * NORM;
					/*
					p_ac3dec->samples.channel[1][j] = right_tmp;
					p_ac3dec->samples.channel[0][j] = left_tmp;
					*/
				}
			break;

			// 2/2
			case 6:
				left      = p_ac3dec->samples.channel[0];
				right     = p_ac3dec->samples.channel[1];
				left_sur  = p_ac3dec->samples.channel[2];
				right_sur = p_ac3dec->samples.channel[3];

				slev = smixlev_lut[p_ac3dec->bsi.surmixlev];

				for (j = 0; j < 256; j++)
				{
					right_tmp= 0.4142f * *right++ + slev * *right_sur++;
					left_tmp = 0.4142f * *left++  + slev * *left_sur++;

					*(out_buf++) = left_tmp * NORM;
					*(out_buf++) = right_tmp * NORM;
					/*
					p_ac3dec->samples.channel[1][j] = right_tmp;
					p_ac3dec->samples.channel[0][j] = left_tmp;
					*/
				}
			break;

			// 3/1
			case 5:
				left      = p_ac3dec->samples.channel[0];
				centre    = p_ac3dec->samples.channel[1];
				right     = p_ac3dec->samples.channel[2];
				//Mono surround
				right_sur = p_ac3dec->samples.channel[3];

				clev = cmixlev_lut[p_ac3dec->bsi.cmixlev];
				slev = smixlev_lut[p_ac3dec->bsi.surmixlev];

				for (j = 0; j < 256; j++)
				{
					right_tmp= 0.4142f * *right++ + clev * *centre   + slev * *right_sur;
					left_tmp = 0.4142f * *left++  + clev * *centre++ + slev * *right_sur++;

					*(out_buf++) = left_tmp * NORM;
					*(out_buf++) = right_tmp * NORM;
					/*
					p_ac3dec->samples.channel[1][j] = right_tmp;
					p_ac3dec->samples.channel[0][j] = left_tmp;
					*/
				}
			break;

			// 2/1
			case 4:
				left      = p_ac3dec->samples.channel[0];
				right     = p_ac3dec->samples.channel[1];
				//Mono surround
				right_sur = p_ac3dec->samples.channel[2];

				slev = smixlev_lut[p_ac3dec->bsi.surmixlev];

				for (j = 0; j < 256; j++)
				{
					right_tmp= 0.4142f * *right++ + slev * *right_sur;
					left_tmp = 0.4142f * *left++  + slev * *right_sur++;

					*(out_buf++) = left_tmp * NORM;
					*(out_buf++) = right_tmp * NORM;
					/*
					p_ac3dec->samples.channel[1][j] = right_tmp;
					p_ac3dec->samples.channel[0][j] = left_tmp;
					*/
				}
			break;

			// 3/0
			case 3:
				left      = p_ac3dec->samples.channel[0];
				centre    = p_ac3dec->samples.channel[1];
				right     = p_ac3dec->samples.channel[2];

				clev = cmixlev_lut[p_ac3dec->bsi.cmixlev];

				for (j = 0; j < 256; j++)
				{
					right_tmp= 0.4142f * *right++ + clev * *centre;
					left_tmp = 0.4142f * *left++  + clev * *centre++;

					*(out_buf++) = left_tmp * NORM;
					*(out_buf++) = right_tmp * NORM;
					/*
					p_ac3dec->samples.channel[1][j] = right_tmp;
					p_ac3dec->samples.channel[0][j] = left_tmp;
					*/
				}
			break;

			case 2:
				left = p_ac3dec->samples.channel[0];
				right = p_ac3dec->samples.channel[1];

				for ( j = 0; j < 256; j++ )
				{
					*(out_buf++) = *(left++) * NORM;
					*(out_buf++) = *(right++) * NORM;
				}
			break;

			// 1/0
			case 1:
				//Mono program!
				right = p_ac3dec->samples.channel[0];

				for (j = 0; j < 256; j++)
				{
					right_tmp = 0.7071f * *right++;

					*(out_buf++) = right_tmp * NORM;
					*(out_buf++) = right_tmp * NORM;
					/*
					p_ac3dec->samples.channel[1][j] = right_tmp;
					p_ac3dec->samples.channel[0][j] = right_tmp;
					*/
				}
			break;

			// 1+1
			case 0:
				//Dual mono, output selected by user
				right = p_ac3dec->samples.channel[global_prefs.dual_mono_channel_select];

				for (j = 0; j < 256; j++)
				{
					right_tmp = 0.7071f * *right++;

					*(out_buf++) = right_tmp * NORM;
					*(out_buf++) = right_tmp * NORM;
					/*
					p_ac3dec->samples.channel[1][j] = right_tmp;
					p_ac3dec->samples.channel[0][j] = right_tmp;
					*/
				}
			break;
		}
	}
}

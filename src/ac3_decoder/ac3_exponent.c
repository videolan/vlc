#include <stdio.h>                                           /* "intf_msg.h" */

#include "int_types.h"
#include "ac3_decoder.h"
#include "ac3_bit_stream.h"
#include "ac3_internal.h"

static const s16 exps_1[128] =
  { -2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
     0, 0, 0 };

static const s16 exps_2[128] =
  { -2,-2,-2,-2,-2,-1,-1,-1,-1,-1, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2,
    -2,-2,-2,-2,-2,-1,-1,-1,-1,-1, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2,
    -2,-2,-2,-2,-2,-1,-1,-1,-1,-1, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2,
    -2,-2,-2,-2,-2,-1,-1,-1,-1,-1, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2,
    -2,-2,-2,-2,-2,-1,-1,-1,-1,-1, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2,
     0, 0, 0 };

static const s16 exps_3[128] =
  { -2,-1, 0, 1, 2,-2,-1, 0, 1, 2,-2,-1, 0, 1, 2,-2,-1, 0, 1, 2,-2,-1, 0, 1, 2,
    -2,-1, 0, 1, 2,-2,-1, 0, 1, 2,-2,-1, 0, 1, 2,-2,-1, 0, 1, 2,-2,-1, 0, 1, 2,
    -2,-1, 0, 1, 2,-2,-1, 0, 1, 2,-2,-1, 0, 1, 2,-2,-1, 0, 1, 2,-2,-1, 0, 1, 2,
    -2,-1, 0, 1, 2,-2,-1, 0, 1, 2,-2,-1, 0, 1, 2,-2,-1, 0, 1, 2,-2,-1, 0, 1, 2,
    -2,-1, 0, 1, 2,-2,-1, 0, 1, 2,-2,-1, 0, 1, 2,-2,-1, 0, 1, 2,-2,-1, 0, 1, 2,
     0, 0, 0 };

#define UNPACK_FBW 1 
#define UNPACK_CPL 2 
#define UNPACK_LFE 4

static __inline__ int exp_unpack_ch (ac3dec_t * p_ac3dec, u16 type,
				     u16 expstr, u16 ngrps, u16 initial_exp,
				     u16 exps[], u16 * dest)
{
    u16 i,j;
    s16 exp_acc;

    if  (expstr == EXP_REUSE) {
        return 0;
    }

    /* Handle the initial absolute exponent */
    exp_acc = initial_exp;
    j = 0;

    /* In the case of a fbw channel then the initial absolute values is
     * also an exponent */
    if (type != UNPACK_CPL) {
        dest[j++] = exp_acc;
    }

    /* Loop through the groups and fill the dest array appropriately */
    switch (expstr) {
    case EXP_D15:	/* 1 */
	for (i = 0; i < ngrps; i++) {
	    if (exps[i] > 124) {
		fprintf (stderr, "ac3dec debug: invalid exponent\n");
		return 1;
	    }
	    exp_acc += (exps_1[exps[i]] /*- 2*/);
	    dest[j++] = exp_acc;
	    exp_acc += (exps_2[exps[i]] /*- 2*/);
	    dest[j++] = exp_acc;
	    exp_acc += (exps_3[exps[i]] /*- 2*/);
	    dest[j++] = exp_acc;
	}
	break;

    case EXP_D25:	/* 2 */
	for (i = 0; i < ngrps; i++) {
	    if (exps[i] > 124) {
		fprintf (stderr, "ac3dec debug: invalid exponent\n");
		return 1;
	    }
	    exp_acc += (exps_1[exps[i]] /*- 2*/);
	    dest[j++] = exp_acc;
	    dest[j++] = exp_acc;
	    exp_acc += (exps_2[exps[i]] /*- 2*/);
	    dest[j++] = exp_acc;
	    dest[j++] = exp_acc;
	    exp_acc += (exps_3[exps[i]] /*- 2*/);
	    dest[j++] = exp_acc;
	    dest[j++] = exp_acc;
	}
	break;

    case EXP_D45:	/* 3 */
	for (i = 0; i < ngrps; i++) {
	    if (exps[i] > 124) {
		fprintf (stderr, "ac3dec debug: invalid exponent\n");
		return 1;
	    }
	    exp_acc += (exps_1[exps[i]] /*- 2*/);
	    dest[j++] = exp_acc;
	    dest[j++] = exp_acc;
	    dest[j++] = exp_acc;
	    dest[j++] = exp_acc;
	    exp_acc += (exps_2[exps[i]] /*- 2*/);
	    dest[j++] = exp_acc;
	    dest[j++] = exp_acc;
	    dest[j++] = exp_acc;
	    dest[j++] = exp_acc;
	    exp_acc += (exps_3[exps[i]] /*- 2*/);
	    dest[j++] = exp_acc;
	    dest[j++] = exp_acc;
	    dest[j++] = exp_acc;
	    dest[j++] = exp_acc;
	}
	break;
    }

    return 0;
}

int exponent_unpack (ac3dec_t * p_ac3dec)
{
    u16 i;

    for (i = 0; i < p_ac3dec->bsi.nfchans; i++) {
	if (exp_unpack_ch (p_ac3dec, UNPACK_FBW, p_ac3dec->audblk.chexpstr[i],
			   p_ac3dec->audblk.nchgrps[i],
			   p_ac3dec->audblk.exps[i][0],
			   &p_ac3dec->audblk.exps[i][1],
			   p_ac3dec->audblk.fbw_exp[i]))
	    return 1;
    }

    if (p_ac3dec->audblk.cplinu) {
        if (exp_unpack_ch (p_ac3dec, UNPACK_CPL, p_ac3dec->audblk.cplexpstr,
			   p_ac3dec->audblk.ncplgrps,
			   p_ac3dec->audblk.cplabsexp << 1,
			   p_ac3dec->audblk.cplexps,
			   &p_ac3dec->audblk.cpl_exp[p_ac3dec->audblk.cplstrtmant]))
	    return 1;
    }

    if (p_ac3dec->bsi.lfeon) {
        if (exp_unpack_ch (p_ac3dec, UNPACK_LFE, p_ac3dec->audblk.lfeexpstr,
			   2, p_ac3dec->audblk.lfeexps[0],
			   &p_ac3dec->audblk.lfeexps[1],
			   p_ac3dec->audblk.lfe_exp))
	    return 1;
    }

    return 0;
}

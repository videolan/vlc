#include <math.h>
#include <sys/uio.h>                                            /* "input.h" */

#include "common.h"
#include "config.h"
#include "vlc_thread.h"
#include "mtime.h"
#include "input.h"
#include "decoder_fifo.h"
#include "audio_output.h"

#include "ac3_decoder.h"
#include "ac3_parse.h"
#include "ac3_exponent.h"
#include "ac3_bit_allocate.h"
#include "ac3_mantissa.h"
#include "ac3_rematrix.h"
#include "ac3_imdct.h"
#include "ac3_downmix.h"

int ac3_audio_block (ac3dec_t * p_ac3dec, s16 * buffer)
    {
    parse_audblk( p_ac3dec );
    exponent_unpack( p_ac3dec );
    if ( p_ac3dec->b_invalid )
        return 1;
    bit_allocate( p_ac3dec );
    mantissa_unpack( p_ac3dec );
    if ( p_ac3dec->b_invalid )
        return 1;
    if ( p_ac3dec->bsi.acmod == 0x2 )
        rematrix( p_ac3dec );
    imdct( p_ac3dec );
    downmix( p_ac3dec, buffer );
    return 0;
    }

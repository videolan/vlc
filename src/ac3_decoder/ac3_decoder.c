#include "int_types.h"
#include "ac3_decoder.h"
#include "ac3_internal.h"

int ac3_init (ac3dec_t * p_ac3dec)
{
    //p_ac3dec->bit_stream.buffer = 0;
    //p_ac3dec->bit_stream.i_available = 0;

    return 0;
}

int ac3_decode_frame (ac3dec_t * p_ac3dec, s16 * buffer)
{
    int i;

    if (parse_bsi (p_ac3dec))
	return 1;

    for (i = 0; i < 6; i++) {
	if (parse_audblk (p_ac3dec, i))
	    return 1;
	if (exponent_unpack (p_ac3dec))
	    return 1;
	bit_allocate (p_ac3dec);
	mantissa_unpack (p_ac3dec);
	if  (p_ac3dec->bsi.acmod == 0x2)
	    rematrix (p_ac3dec);
	imdct (p_ac3dec);
	downmix (p_ac3dec, buffer);

	buffer += 2*256;
    }

    parse_auxdata (p_ac3dec);

    return 0;
}

/*****************************************************************************
 * vpar_blocks.h : video parser blocks management
 * (c)1999 VideoLAN
 *****************************************************************************
 *****************************************************************************
 * Requires:
 *  "config.h"
 *  "common.h"
 *  "mtime.h"
 *  "vlc_thread.h"
 *  "input.h"
 *  "video.h"
 *  "video_output.h"
 *  "decoder_fifo.h"
 *  "video_fifo.h"
 *****************************************************************************/

/*****************************************************************************
 * quant_matrix_t : Quantization Matrix
 *****************************************************************************
 * ??
 *****************************************************************************/
typedef struct quant_matrix_s
{
    int         pi_matrix[64];
    boolean_t   b_allocated;
                          /* Has the matrix been allocated by vpar_headers ? */
} quant_matrix_t;

extern int *    pi_default_intra_quant;
extern int *    pi_default_nonintra_quant;

/*****************************************************************************
 * video_fifo.h : FIFO for the pool of video_decoders
 * (c)1999 VideoLAN
 *****************************************************************************
 *****************************************************************************
 * Requires:
 *  "config.h"
 *  "common.h"
 *  "vlc_thread.h"
 *  "video_parser.h"
 *  "undec_picture.h"
 *****************************************************************************/

/*****************************************************************************
 * Macros
 *****************************************************************************/

/* ?? move to inline functions */
#define VIDEO_FIFO_ISEMPTY( fifo )    ( (fifo).i_start == (fifo).i_end )
#define VIDEO_FIFO_ISFULL( fifo )     ( ( ( (fifo).i_end + 1 - (fifo).i_start ) \
                                          & VFIFO_SIZE ) == 0 )
#define VIDEO_FIFO_START( fifo )      ( (fifo).buffer[ (fifo).i_start ] )
#define VIDEO_FIFO_INCSTART( fifo )   ( (fifo).i_start = ((fifo).i_start + 1) \
                                                           & VFIFO_SIZE ) 
#define VIDEO_FIFO_END( fifo )        ( (fifo).buffer[ (fifo).i_end ] )
#define VIDEO_FIFO_INCEND( fifo )     ( (fifo).i_end = ((fifo).i_end + 1) \
                                                         & VFIFO_SIZE )

/*****************************************************************************
 * video_fifo_t
 *****************************************************************************
 * This rotative FIFO contains undecoded pictures that are to be decoded
 *****************************************************************************/
typedef struct video_fifo_s
{
    vlc_mutex_t         lock;                              /* fifo data lock */
    vlc_cond_t          wait;              /* fifo data conditional variable */

    /* buffer is an array of undec_picture_t pointers */
    undec_picture_t *           buffer[VFIFO_SIZE + 1];
    int                         i_start;
    int                         i_end;

    struct video_parser_s *     p_vpar;
} video_fifo_t;

/*****************************************************************************
 * video_buffer_t
 *****************************************************************************
 * This structure enables the parser to maintain a list of free
 * undec_picture_t structures
 *****************************************************************************/
typedef struct video_buffer_s
{
    vlc_mutex_t         lock;                            /* buffer data lock */

    undec_picture_t     p_undec_p[VFIFO_SIZE + 1];
    undec_picture_t *   pp_undec_free[VFIFO_SIZE+1];       /* this is a LIFO */
    int                 i_index;
} video_buffer_t;

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
undec_picture_t * vpar_GetPicture( video_fifo_t * p_fifo );
undec_picture_t * vpar_NewPicture( video_fifo_t * p_fifo );
void vpar_DecodePicture( video_fifo_t * p_fifo, undec_picture_t * p_undec_p );
void vpar_ReleasePicture( video_fifo_t * p_fifo, undec_picture_t * p_undec_p );
void vpar_DestroyPicture( video_fifo_t * p_fifo, undec_picture_t * p_undec_p );

/******************************************************************************
 * audio_decoder.c: MPEG1 Layer I-II audio decoder thread
 * (c)1999 VideoLAN
 ******************************************************************************/

/*
 * TODO :
 * - Optimiser les NeedBits() et les GetBits() du code là où c'est possible
 */

/******************************************************************************
 * Preamble
 ******************************************************************************/
#include <unistd.h>

#include <pthread.h>
#include <stdio.h>                                            /* "intf_msg.h" */
#include <stdlib.h>                                       /* malloc(), free() */
#include <netinet/in.h>                                            /* ntohl() */
#include <sys/soundcard.h>                                /* "audio_output.h" */
#include <sys/uio.h>                                             /* "input.h" */

#include "common.h"
#include "config.h"
#include "mtime.h"
#include "debug.h"                                       /* "input_netlist.h" */

#include "intf_msg.h"                         /* intf_DbgMsg(), intf_ErrMsg() */

#include "input.h"                                            /* pes_packet_t */
#include "input_netlist.h"                          /* input_NetlistFreePES() */
#include "decoder_fifo.h"          /* DECODER_FIFO_(ISEMPTY|START|INCSTART)() */

#include "audio_output.h"

#include "audio_constants.h"
#include "audio_decoder.h"
#include "audio_math.h"

/******************************************************************************
 * Local prototypes
 ******************************************************************************/
static int      InitThread              ( adec_thread_t * p_adec );
static void     RunThread               ( adec_thread_t * p_adec );
static void     ErrorThread             ( adec_thread_t * p_adec );
static void     EndThread               ( adec_thread_t * p_adec );

static int      adec_Layer1_Mono        ( adec_thread_t * p_adec );
static int      adec_Layer1_Stereo      ( adec_thread_t * p_adec );
static int      adec_Layer2_Mono        ( adec_thread_t * p_adec );
static int      adec_Layer2_Stereo      ( adec_thread_t * p_adec );

static byte_t   GetByte                 ( bit_stream_t * p_bit_stream );
static void     NeedBits                ( bit_stream_t * p_bit_stream, int i_bits );
static void     DumpBits                ( bit_stream_t * p_bit_stream, int i_bits );
static int      FindHeader              ( adec_thread_t * p_adec );

/******************************************************************************
 * adec_CreateThread: creates an audio decoder thread
 ******************************************************************************
 * This function creates a new audio decoder thread, and returns a pointer to
 * its description. On error, it returns NULL.
 ******************************************************************************/
adec_thread_t * adec_CreateThread( input_thread_t * p_input )
{
    adec_thread_t *     p_adec;

    intf_DbgMsg("adec debug: creating audio decoder thread\n");

    /* Allocate the memory needed to store the thread's structure */
    if ( (p_adec = (adec_thread_t *)malloc( sizeof(adec_thread_t) )) == NULL )
    {
        intf_ErrMsg("adec error: not enough memory for adec_CreateThread() to create the new thread\n");
        return( NULL );
    }

    /*
     * Initialize the thread properties
     */
    p_adec->b_die = 0;
    p_adec->b_error = 0;

    /*
     * Initialize the input properties
     */
    /* Initialize the decoder fifo's data lock and conditional variable and set
     * its buffer as empty */
    pthread_mutex_init( &p_adec->fifo.data_lock, NULL );
    pthread_cond_init( &p_adec->fifo.data_wait, NULL );
    p_adec->fifo.i_start = 0;
    p_adec->fifo.i_end = 0;
    /* Initialize the bit stream structure */
    p_adec->bit_stream.p_input = p_input;
    p_adec->bit_stream.p_decoder_fifo = &p_adec->fifo;
    p_adec->bit_stream.fifo.buffer = 0;
    p_adec->bit_stream.fifo.i_available = 0;

    /*
     * Initialize the decoder properties
     */
    p_adec->bank_0.actual = p_adec->bank_0.v1;
    p_adec->bank_0.pos = 0;
    p_adec->bank_1.actual = p_adec->bank_1.v1;
    p_adec->bank_1.pos = 0;

    /*
     * Initialize the output properties
     */
    p_adec->p_aout = p_input->p_aout;
    p_adec->p_aout_fifo = NULL;

    /* Spawn the audio decoder thread */
    if ( pthread_create(&p_adec->thread_id, NULL, (void *)RunThread, (void *)p_adec) )
    {
        intf_ErrMsg("adec error: can't spawn audio decoder thread\n");
        free( p_adec );
        return( NULL );
    }

    intf_DbgMsg("adec debug: audio decoder thread (%p) created\n", p_adec);
    return( p_adec );
}

/******************************************************************************
 * adec_DestroyThread: destroys an audio decoder thread
 ******************************************************************************
 * This function asks an audio decoder thread to terminate. This function has
 * not to wait until the decoder thread has really died, because the killer (ie
 * this function's caller) is the input thread, that's why we are sure that no
 * other thread will try to access to this thread's descriptor after its
 * destruction.
 ******************************************************************************/
void adec_DestroyThread( adec_thread_t * p_adec )
{
    intf_DbgMsg("adec debug: requesting termination of audio decoder thread %p\n", p_adec);

    /* Ask thread to kill itself */
    p_adec->b_die = 1;

    /* Remove this as soon as the "status" flag is implemented */
    pthread_join( p_adec->thread_id, NULL );         /* wait until it's done */
}

/* Following functions are local */

/******************************************************************************
 * FindHeader : parses an input stream until an audio frame header could be
 *              found
 ******************************************************************************
 * When this function returns successfully, the header can be found in the
 * buffer of the bit stream fifo.
 ******************************************************************************/
static int FindHeader( adec_thread_t * p_adec )
{
    while ( (!p_adec->b_die) && (!p_adec->b_error) )
    {
        NeedBits( &p_adec->bit_stream, 32 );
        if ( (p_adec->bit_stream.fifo.buffer & ADEC_HEADER_SYNCWORD_MASK) == ADEC_HEADER_SYNCWORD_MASK )
        {
#ifdef DEBUG
//            fprintf(stderr, "H");
#endif
            return( 0 );
        }
#ifdef DEBUG
//        fprintf(stderr, "!");
#endif
        DumpBits( &p_adec->bit_stream, 8 );
    }

    return( -1 );
}

/******************************************************************************
 * adec_Layer`L'_`M': decodes an mpeg 1, layer `L', mode `M', audio frame
 ******************************************************************************
 * These functions decode the audio frame which has already its header loaded
 * in the i_header member of the audio decoder thread structure and its first
 * byte of data described by the bit stream structure of the audio decoder
 * thread (there is no bit available in the bit buffer yet)
 ******************************************************************************/

/******************************************************************************
 * adec_Layer1_Mono
 ******************************************************************************/
static __inline__ int adec_Layer1_Mono( adec_thread_t * p_adec )
{
    p_adec->bit_stream.fifo.buffer = 0;
    p_adec->bit_stream.fifo.i_available = 0;
    return( 0 );
}

/******************************************************************************
 * adec_Layer1_Stereo
 ******************************************************************************/
static __inline__ int adec_Layer1_Stereo( adec_thread_t * p_adec )
{
    p_adec->bit_stream.fifo.buffer = 0;
    p_adec->bit_stream.fifo.i_available = 0;
    return( 0 );
}

/******************************************************************************
 * adec_Layer2_Mono
 ******************************************************************************/
static __inline__ int adec_Layer2_Mono( adec_thread_t * p_adec )
{
    p_adec->bit_stream.fifo.buffer = 0;
    p_adec->bit_stream.fifo.i_available = 0;
    return( 0 );
}

/******************************************************************************
 * adec_Layer2_Stereo
 ******************************************************************************/
static __inline__ int adec_Layer2_Stereo( adec_thread_t * p_adec )
{
    typedef struct requantization_s
    {
        byte_t                          i_bits_per_codeword;
        const float *                   pf_ungroup;
        float                           f_slope;
        float                           f_offset;
    } requantization_t;

    static const float                  pf_scalefactor[64] = ADEC_SCALE_FACTOR;

    static u32                          i_header;
    static int                          i_sampling_frequency, i_mode, i_bound;
    static int                          pi_allocation_0[32], pi_allocation_1[32]; /* see ISO/IEC 11172-3 2.4.1.6 */
    int                                 i_sb, i_nbal;
    float                               f_scalefactor_0, f_scalefactor_1;

    static const byte_t                 ppi_bitrate_per_channel_index[4][15] = ADEC_LAYER2_BITRATE_PER_CHANNEL_INDEX;
    static const byte_t                 ppi_sblimit[3][11] = ADEC_LAYER2_SBLIMIT;
    static const byte_t                 ppi_nbal[2][32] = ADEC_LAYER2_NBAL;

    static const float                  pf_ungroup3[3*3*3 * 3] = ADEC_LAYER2_UNGROUP3;
    static const float                  pf_ungroup5[5*5*5 * 3] = ADEC_LAYER2_UNGROUP5;
    static const float                  pf_ungroup9[9*9*9 * 3] = ADEC_LAYER2_UNGROUP9;

    static const requantization_t       p_requantization_cd[16] = ADEC_LAYER2_REQUANTIZATION_CD;
    static const requantization_t       p_requantization_ab1[16] = ADEC_LAYER2_REQUANTIZATION_AB1;
    static const requantization_t       p_requantization_ab2[16] = ADEC_LAYER2_REQUANTIZATION_AB2;
    static const requantization_t       p_requantization_ab3[16] = ADEC_LAYER2_REQUANTIZATION_AB3;
    static const requantization_t       p_requantization_ab4[16] = ADEC_LAYER2_REQUANTIZATION_AB4;
    static const requantization_t *     pp_requantization_ab[30] = ADEC_LAYER2_REQUANTIZATION_AB;

    static int                          i_sblimit, i_bitrate_per_channel_index;
    static int                          pi_scfsi_0[30], pi_scfsi_1[30];
    static const byte_t *               pi_nbal;
    static float                        ppf_sample_0[3][32], ppf_sample_1[3][32];
    static const requantization_t *     pp_requantization_0[30];
    static const requantization_t *     pp_requantization_1[30];
    static requantization_t             requantization;
    static const float *                pf_ungroup;

    static float                        pf_scalefactor_0_0[30], pf_scalefactor_0_1[30], pf_scalefactor_0_2[30];
    static float                        pf_scalefactor_1_0[30], pf_scalefactor_1_1[30], pf_scalefactor_1_2[30];

    int                                 i_2nbal, i_gr;
    float                               f_dummy;

    long                                l_end_frame;
    s16 *                               p_s16;

    int                                 i_need = 0, i_dump = 0;
//    static const int                    pi_framesize[512] = ADEC_FRAME_SIZE;

    /* Read the audio frame header and flush the bit buffer */
    i_header = p_adec->bit_stream.fifo.buffer;
    p_adec->bit_stream.fifo.buffer = 0;
    p_adec->bit_stream.fifo.i_available = 0;
    /* Read the sampling frequency (see ISO/IEC 11172-3 2.4.2.3) */
    i_sampling_frequency = (int)((i_header & ADEC_HEADER_SAMPLING_FREQUENCY_MASK)
        >> ADEC_HEADER_SAMPLING_FREQUENCY_SHIFT);
    /* Read the mode (see ISO/IEC 11172-3 2.4.2.3) */
    i_mode = (int)((i_header & ADEC_HEADER_MODE_MASK) >> ADEC_HEADER_MODE_SHIFT);
    /* If a CRC can be found in the frame, get rid of it */
    if ( (i_header & ADEC_HEADER_PROTECTION_BIT_MASK) == 0 )
    {
        GetByte( &p_adec->bit_stream );
        GetByte( &p_adec->bit_stream );
    }

    /* Find out the bitrate per channel index */
    i_bitrate_per_channel_index = (int)ppi_bitrate_per_channel_index[i_mode]
        [(i_header & ADEC_HEADER_BITRATE_INDEX_MASK) >> ADEC_HEADER_BITRATE_INDEX_SHIFT];
    /* Find out the number of subbands */
    i_sblimit = (int)ppi_sblimit[i_sampling_frequency][i_bitrate_per_channel_index];
    /* Check if the frame is valid or not */
    if ( i_sblimit == 0 )
    {
        return( 0 );                                  /* the frame is invalid */
    }
    /* Find out the number of bits allocated */
    pi_nbal = ppi_nbal[ (i_bitrate_per_channel_index <= 2) ? 0 : 1 ];

    /* Find out the `bound' subband (see ISO/IEC 11172-3 2.4.2.3) */
    if ( i_mode == 1 )
    {
        i_bound = (int)(((i_header & ADEC_HEADER_MODE_EXTENSION_MASK) >> (ADEC_HEADER_MODE_EXTENSION_SHIFT - 2)) + 4);
        if ( i_bound > i_sblimit )
        {
            i_bound = i_sblimit;
        }
    }
    else
    {
        i_bound = i_sblimit;
    }

    /* Read the allocation information (see ISO/IEC 11172-3 2.4.1.6) */
    for ( i_sb = 0; i_sb < i_bound; i_sb++ )
    {
        i_2nbal = 2 * (i_nbal = (int)pi_nbal[ i_sb ]);
        NeedBits( &p_adec->bit_stream, i_2nbal );
        i_need += i_2nbal;
        pi_allocation_0[ i_sb ] = (int)(p_adec->bit_stream.fifo.buffer >> (32 - i_nbal));
        p_adec->bit_stream.fifo.buffer <<= i_nbal;
        pi_allocation_1[ i_sb ] = (int)(p_adec->bit_stream.fifo.buffer >> (32 - i_nbal));
        p_adec->bit_stream.fifo.buffer <<= i_nbal;
        p_adec->bit_stream.fifo.i_available -= i_2nbal;
        i_dump += i_2nbal;
    }
    for ( ; i_sb < i_sblimit; i_sb++ )
    {
        i_nbal = (int)pi_nbal[ i_sb ];
        NeedBits( &p_adec->bit_stream, i_nbal );
        i_need += i_nbal;
        pi_allocation_0[ i_sb ] = (int)(p_adec->bit_stream.fifo.buffer >> (32 - i_nbal));
        DumpBits( &p_adec->bit_stream, i_nbal );
        i_dump += i_nbal;
    }

#define MACRO( p_requantization ) \
    for ( i_sb = 0; i_sb < i_bound; i_sb++ ) \
    { \
        if ( pi_allocation_0[i_sb] ) \
        { \
            pp_requantization_0[i_sb] = &((p_requantization)[pi_allocation_0[i_sb]]); \
            NeedBits( &p_adec->bit_stream, 2 ); \
            i_need += 2; \
            pi_scfsi_0[i_sb] = (int)(p_adec->bit_stream.fifo.buffer >> (32 - 2)); \
            DumpBits( &p_adec->bit_stream, 2 ); \
            i_dump += 2; \
        } \
        else \
        { \
            ppf_sample_0[0][i_sb] = .0; \
            ppf_sample_0[1][i_sb] = .0; \
            ppf_sample_0[2][i_sb] = .0; \
        } \
\
        if ( pi_allocation_1[i_sb] ) \
        { \
            pp_requantization_1[i_sb] = &((p_requantization)[pi_allocation_1[i_sb]]); \
            NeedBits( &p_adec->bit_stream, 2 ); \
            i_need += 2; \
            pi_scfsi_1[i_sb] = (int)(p_adec->bit_stream.fifo.buffer >> (32 - 2)); \
            DumpBits( &p_adec->bit_stream, 2 ); \
            i_dump += 2; \
        } \
        else \
        { \
            ppf_sample_1[0][i_sb] = .0; \
            ppf_sample_1[1][i_sb] = .0; \
            ppf_sample_1[2][i_sb] = .0; \
        } \
    } \
\
    for ( ; i_sb < i_sblimit; i_sb++ ) \
    { \
        if ( pi_allocation_0[i_sb] ) \
        { \
            pp_requantization_0[i_sb] = &((p_requantization)[pi_allocation_0[i_sb]]); \
            NeedBits( &p_adec->bit_stream, 4 ); \
            i_need += 4; \
            pi_scfsi_0[i_sb] = (int)(p_adec->bit_stream.fifo.buffer >> (32 - 2)); \
            p_adec->bit_stream.fifo.buffer <<= 2; \
            pi_scfsi_1[i_sb] = (int)(p_adec->bit_stream.fifo.buffer >> (32 - 2)); \
            p_adec->bit_stream.fifo.buffer <<= 2; \
            p_adec->bit_stream.fifo.i_available -= 4; \
            i_dump += 4; \
        } \
        else \
        { \
            ppf_sample_0[0][i_sb] = .0; \
            ppf_sample_0[1][i_sb] = .0; \
            ppf_sample_0[2][i_sb] = .0; \
            ppf_sample_1[0][i_sb] = .0; \
            ppf_sample_1[1][i_sb] = .0; \
            ppf_sample_1[2][i_sb] = .0; \
        } \
    }
/* #define MACRO */

    if ( i_bitrate_per_channel_index <= 2 )
    {
        MACRO( p_requantization_cd )
    }
    else
    {
        MACRO( pp_requantization_ab[i_sb] )
    }

#define SWITCH( pi_scfsi, pf_scalefactor_0, pf_scalefactor_1, pf_scalefactor_2 ) \
    switch ( (pi_scfsi)[i_sb] ) \
    { \
        case 0: \
            NeedBits( &p_adec->bit_stream, (3*6) ); \
            i_need += 18; \
            (pf_scalefactor_0)[i_sb] = pf_scalefactor[p_adec->bit_stream.fifo.buffer >> (32 - 6)]; \
            p_adec->bit_stream.fifo.buffer <<= 6; \
            (pf_scalefactor_1)[i_sb] = pf_scalefactor[p_adec->bit_stream.fifo.buffer >> (32 - 6)]; \
            p_adec->bit_stream.fifo.buffer <<= 6; \
            (pf_scalefactor_2)[i_sb] = pf_scalefactor[p_adec->bit_stream.fifo.buffer >> (32 - 6)]; \
            p_adec->bit_stream.fifo.buffer <<= 6; \
            p_adec->bit_stream.fifo.i_available -= (3*6); \
            i_dump += 18; \
            break; \
\
        case 1: \
            NeedBits( &p_adec->bit_stream, (2*6) ); \
            i_need += 12; \
            (pf_scalefactor_0)[i_sb] = \
                (pf_scalefactor_1)[i_sb] = pf_scalefactor[p_adec->bit_stream.fifo.buffer >> (32 - 6)]; \
            p_adec->bit_stream.fifo.buffer <<= 6; \
            (pf_scalefactor_2)[i_sb] = pf_scalefactor[p_adec->bit_stream.fifo.buffer >> (32 - 6)]; \
            p_adec->bit_stream.fifo.buffer <<= 6; \
            p_adec->bit_stream.fifo.i_available -= (2*6); \
            i_dump += 12; \
            break; \
\
        case 2: \
            NeedBits( &p_adec->bit_stream, (1*6) ); \
            i_need += 6; \
            (pf_scalefactor_0)[i_sb] = \
                (pf_scalefactor_1)[i_sb] = \
                (pf_scalefactor_2)[i_sb] = pf_scalefactor[p_adec->bit_stream.fifo.buffer >> (32 - 6)]; \
            DumpBits( &p_adec->bit_stream, (1*6) ); \
            i_dump += 6; \
            break; \
\
        case 3: \
            NeedBits( &p_adec->bit_stream, (2*6) ); \
            i_need += 12; \
            (pf_scalefactor_0)[i_sb] = pf_scalefactor[p_adec->bit_stream.fifo.buffer >> (32 - 6)]; \
            p_adec->bit_stream.fifo.buffer <<= 6; \
            (pf_scalefactor_1)[i_sb] = \
                (pf_scalefactor_2)[i_sb] = pf_scalefactor[p_adec->bit_stream.fifo.buffer >> (32 - 6)]; \
            p_adec->bit_stream.fifo.buffer <<= 6; \
            p_adec->bit_stream.fifo.i_available -= (2*6); \
            i_dump += 12; \
            break; \
    }
/* #define SWITCH */

    for ( i_sb = 0; i_sb < i_bound; i_sb++ )
    {
        if ( pi_allocation_0[i_sb] )
        {
            SWITCH( pi_scfsi_0, pf_scalefactor_0_0, pf_scalefactor_0_1, pf_scalefactor_0_2 )
        }
        if ( pi_allocation_1[i_sb] )
        {
            SWITCH( pi_scfsi_1, pf_scalefactor_1_0, pf_scalefactor_1_1, pf_scalefactor_1_2 )
        }
    }
    for ( ; i_sb < i_sblimit; i_sb++ )
    {
        if ( pi_allocation_0[i_sb] )
        {
            SWITCH( pi_scfsi_0, pf_scalefactor_0_0, pf_scalefactor_0_1, pf_scalefactor_0_2 )
            SWITCH( pi_scfsi_1, pf_scalefactor_1_0, pf_scalefactor_1_1, pf_scalefactor_1_2 )
        }
    }
    for ( ; i_sb < 32; i_sb++ )
    {
        ppf_sample_0[0][i_sb] = .0;
        ppf_sample_0[1][i_sb] = .0;
        ppf_sample_0[2][i_sb] = .0;
        ppf_sample_1[0][i_sb] = .0;
        ppf_sample_1[1][i_sb] = .0;
        ppf_sample_1[2][i_sb] = .0;
    }

#define NEXT_BUF \
/* fprintf(stderr, "%p\n", p_adec->p_aout_fifo->buffer); */ \
/* fprintf(stderr, "l_end_frame == %li, %p\n", l_end_frame, (aout_frame_t *)p_adec->p_aout_fifo->buffer + l_end_frame); */ \
    p_s16 = ((aout_frame_t *)p_adec->p_aout_fifo->buffer)[ l_end_frame ]; \
/* fprintf(stderr, "p_s16 == %p\n", p_s16); */ \
    l_end_frame += 1; \
    l_end_frame &= AOUT_FIFO_SIZE;
/* #define NEXT_BUF */

#define GROUPTEST( pp_requantization, ppf_sample, pf_sf ) \
    requantization = *((pp_requantization)[i_sb]); \
    if ( requantization.pf_ungroup == NULL ) \
    { \
        NeedBits( &p_adec->bit_stream, requantization.i_bits_per_codeword ); \
        i_need += requantization.i_bits_per_codeword; \
        (ppf_sample)[0][i_sb] = (f_scalefactor_0 = (pf_sf)[i_sb]) * (requantization.f_slope * \
            (p_adec->bit_stream.fifo.buffer >> (32 - requantization.i_bits_per_codeword)) + requantization.f_offset); \
        DumpBits( &p_adec->bit_stream, requantization.i_bits_per_codeword ); \
        i_dump += requantization.i_bits_per_codeword; \
\
        NeedBits( &p_adec->bit_stream, requantization.i_bits_per_codeword ); \
        i_need += requantization.i_bits_per_codeword; \
        (ppf_sample)[1][i_sb] = f_scalefactor_0 * (requantization.f_slope * \
            (p_adec->bit_stream.fifo.buffer >> (32 - requantization.i_bits_per_codeword)) + requantization.f_offset); \
        DumpBits( &p_adec->bit_stream, requantization.i_bits_per_codeword ); \
        i_dump += requantization.i_bits_per_codeword; \
\
        NeedBits( &p_adec->bit_stream, requantization.i_bits_per_codeword ); \
        i_need += requantization.i_bits_per_codeword; \
        (ppf_sample)[2][i_sb] = f_scalefactor_0 * (requantization.f_slope * \
            (p_adec->bit_stream.fifo.buffer >> (32 - requantization.i_bits_per_codeword)) + requantization.f_offset); \
        DumpBits( &p_adec->bit_stream, requantization.i_bits_per_codeword ); \
        i_dump += requantization.i_bits_per_codeword; \
    } \
    else \
    { \
        NeedBits( &p_adec->bit_stream, requantization.i_bits_per_codeword ); \
        i_need += requantization.i_bits_per_codeword; \
        pf_ungroup = requantization.pf_ungroup + 3 * \
            (p_adec->bit_stream.fifo.buffer >> (32 - requantization.i_bits_per_codeword)); \
        DumpBits( &p_adec->bit_stream, requantization.i_bits_per_codeword ); \
        i_dump += requantization.i_bits_per_codeword; \
        (ppf_sample)[0][i_sb] = (f_scalefactor_0 = (pf_sf)[i_sb]) * pf_ungroup[0]; \
        (ppf_sample)[1][i_sb] = f_scalefactor_0 * pf_ungroup[1]; \
        (ppf_sample)[2][i_sb] = f_scalefactor_0 * pf_ungroup[2]; \
    }
/* #define GROUPTEST */

#define READ_SAMPLE_L2S( pf_scalefactor_0, pf_scalefactor_1, i_grlimit ) \
    for ( ; i_gr < (i_grlimit); i_gr++ ) \
    { \
        for ( i_sb = 0; i_sb < i_bound; i_sb++ ) \
        { \
            if ( pi_allocation_0[i_sb] ) \
            { \
                GROUPTEST( pp_requantization_0, ppf_sample_0, (pf_scalefactor_0) ) \
            } \
            if ( pi_allocation_1[i_sb] ) \
            { \
                GROUPTEST( pp_requantization_1, ppf_sample_1, (pf_scalefactor_1) ) \
            } \
        } \
        for ( ; i_sb < i_sblimit; i_sb++ ) \
        { \
            if ( pi_allocation_0[i_sb] ) \
            { \
                requantization = *(pp_requantization_0[i_sb]); \
                if ( requantization.pf_ungroup == NULL ) \
                { \
                    NeedBits( &p_adec->bit_stream, requantization.i_bits_per_codeword ); \
                    i_need += requantization.i_bits_per_codeword; \
                    ppf_sample_0[0][i_sb] = (f_scalefactor_0 = (pf_scalefactor_0)[i_sb]) * \
                        (requantization.f_slope * (f_dummy = \
                        (float)(p_adec->bit_stream.fifo.buffer >> (32 - requantization.i_bits_per_codeword))) + \
                        requantization.f_offset); \
                    DumpBits( &p_adec->bit_stream, requantization.i_bits_per_codeword ); \
                    i_dump += requantization.i_bits_per_codeword; \
                    ppf_sample_1[0][i_sb] = (f_scalefactor_1 = (pf_scalefactor_1)[i_sb]) * \
                        (requantization.f_slope * f_dummy + requantization.f_offset); \
\
                    NeedBits( &p_adec->bit_stream, requantization.i_bits_per_codeword ); \
                    i_need += requantization.i_bits_per_codeword; \
                    ppf_sample_0[1][i_sb] = f_scalefactor_0 * \
                        (requantization.f_slope * (f_dummy = \
                        (float)(p_adec->bit_stream.fifo.buffer >> (32 - requantization.i_bits_per_codeword))) + \
                        requantization.f_offset); \
                    DumpBits( &p_adec->bit_stream, requantization.i_bits_per_codeword ); \
                    i_dump += requantization.i_bits_per_codeword; \
                    ppf_sample_1[1][i_sb] = f_scalefactor_1 * \
                        (requantization.f_slope * f_dummy + requantization.f_offset); \
\
                    NeedBits( &p_adec->bit_stream, requantization.i_bits_per_codeword ); \
                    i_need += requantization.i_bits_per_codeword; \
                    ppf_sample_0[2][i_sb] = f_scalefactor_0 * \
                        (requantization.f_slope * (f_dummy = \
                        (float)(p_adec->bit_stream.fifo.buffer >> (32 - requantization.i_bits_per_codeword))) + \
                        requantization.f_offset); \
                    DumpBits( &p_adec->bit_stream, requantization.i_bits_per_codeword ); \
                    i_dump += requantization.i_bits_per_codeword; \
                    ppf_sample_1[2][i_sb] = f_scalefactor_1 * \
                        (requantization.f_slope * f_dummy + requantization.f_offset); \
                } \
                else \
                { \
                    NeedBits( &p_adec->bit_stream, requantization.i_bits_per_codeword ); \
                    i_need += requantization.i_bits_per_codeword; \
                    pf_ungroup = requantization.pf_ungroup + 3 * \
                        (p_adec->bit_stream.fifo.buffer >> (32 - requantization.i_bits_per_codeword)); \
                    DumpBits( &p_adec->bit_stream, requantization.i_bits_per_codeword ); \
                    i_dump += requantization.i_bits_per_codeword; \
\
                    ppf_sample_0[0][i_sb] = (f_scalefactor_0 = (pf_scalefactor_0)[i_sb]) * pf_ungroup[0]; \
                    ppf_sample_0[1][i_sb] = f_scalefactor_0 * pf_ungroup[1]; \
                    ppf_sample_0[2][i_sb] = f_scalefactor_0 * pf_ungroup[2]; \
\
                    ppf_sample_1[0][i_sb] = (f_scalefactor_1 = (pf_scalefactor_1)[i_sb]) * pf_ungroup[0]; \
                    ppf_sample_1[1][i_sb] = f_scalefactor_1 * pf_ungroup[1]; \
                    ppf_sample_1[2][i_sb] = f_scalefactor_1 * pf_ungroup[2]; \
                } \
            } \
        } \
\
/* fprintf(stderr, "%p", p_s16); */ \
        DCT32( ppf_sample_0[0], &p_adec->bank_0 ); \
        PCM( &p_adec->bank_0, &p_s16, 2 ); \
/* fprintf(stderr, " %p", p_s16); */ \
        p_s16 -= 63; \
/* fprintf(stderr, " %p\n", p_s16); */ \
\
/* fprintf(stderr, "%p", p_s16); */ \
        DCT32( ppf_sample_1[0], &p_adec->bank_1 ); \
        PCM( &p_adec->bank_1, &p_s16, 2 ); \
/* fprintf(stderr, " %p", p_s16); */ \
        p_s16 -= 1; \
/* fprintf(stderr, " %p\n", p_s16); */ \
\
/* fprintf(stderr, "%p", p_s16); */ \
        DCT32( ppf_sample_0[1], &p_adec->bank_0 ); \
        PCM( &p_adec->bank_0, &p_s16, 2 ); \
/* fprintf(stderr, " %p", p_s16); */ \
        p_s16 -= 63; \
/* fprintf(stderr, " %p\n", p_s16); */ \
\
/* fprintf(stderr, "%p", p_s16); */ \
        DCT32( ppf_sample_1[1], &p_adec->bank_1 ); \
        PCM( &p_adec->bank_1, &p_s16, 2 ); \
/* fprintf(stderr, " %p", p_s16); */ \
        p_s16 -= 1; \
/* fprintf(stderr, " %p\n", p_s16); */ \
\
/* fprintf(stderr, "%p", p_s16); */ \
        DCT32( ppf_sample_0[2], &p_adec->bank_0 ); \
        PCM( &p_adec->bank_0, &p_s16, 2 ); \
/* fprintf(stderr, " %p", p_s16); */ \
        p_s16 -= 63; \
/* fprintf(stderr, " %p\n", p_s16); */ \
\
/* fprintf(stderr, "%p", p_s16); */ \
        DCT32( ppf_sample_1[2], &p_adec->bank_1 ); \
        PCM( &p_adec->bank_1, &p_s16, 2 ); \
/* fprintf(stderr, " %p", p_s16); */ \
        p_s16 -= 1; \
/* fprintf(stderr, " %p\n", p_s16); */ \
    }
/* #define READ_SAMPLE_L2S */

    l_end_frame = p_adec->p_aout_fifo->l_end_frame;
    i_gr = 0;

    NEXT_BUF
    READ_SAMPLE_L2S( pf_scalefactor_0_0, pf_scalefactor_1_0, 2 )

    NEXT_BUF
    READ_SAMPLE_L2S( pf_scalefactor_0_0, pf_scalefactor_1_0, 4 )

    NEXT_BUF
    READ_SAMPLE_L2S( pf_scalefactor_0_1, pf_scalefactor_1_1, 6 )

    NEXT_BUF
    READ_SAMPLE_L2S( pf_scalefactor_0_1, pf_scalefactor_1_1, 8 )

    NEXT_BUF
    READ_SAMPLE_L2S( pf_scalefactor_0_2, pf_scalefactor_1_2, 10 )

    NEXT_BUF
    READ_SAMPLE_L2S( pf_scalefactor_0_2, pf_scalefactor_1_2, 12 )

//    fprintf(stderr, "adec debug: layer == %i, padding_bit == %i, sampling_frequency == %i, bitrate_index == %i\n",
//        (i_header & ADEC_HEADER_LAYER_MASK) >> ADEC_HEADER_LAYER_SHIFT,
//        (i_header & ADEC_HEADER_PADDING_BIT_MASK) >> ADEC_HEADER_PADDING_BIT_SHIFT,
//        (i_header & ADEC_HEADER_SAMPLING_FREQUENCY_MASK) >> ADEC_HEADER_SAMPLING_FREQUENCY_SHIFT,
//        (i_header & ADEC_HEADER_BITRATE_INDEX_MASK) >> ADEC_HEADER_BITRATE_INDEX_SHIFT);
//    fprintf(stderr, "adec debug: framesize == %i, i_need == %i, i_dump == %i\n",
//        pi_framesize[ 128 * ((i_header & ADEC_HEADER_LAYER_MASK) >> ADEC_HEADER_LAYER_SHIFT) +
//            64 * ((i_header & ADEC_HEADER_PADDING_BIT_MASK) >> ADEC_HEADER_PADDING_BIT_SHIFT) +
//            16 * ((i_header & ADEC_HEADER_SAMPLING_FREQUENCY_MASK) >> ADEC_HEADER_SAMPLING_FREQUENCY_SHIFT) +
//            1 * ((i_header & ADEC_HEADER_BITRATE_INDEX_MASK) >> ADEC_HEADER_BITRATE_INDEX_SHIFT) ],
//        i_need,
//        i_dump);
    p_adec->bit_stream.fifo.buffer = 0;
    p_adec->bit_stream.fifo.i_available = 0;
    return( 6 );
}

/******************************************************************************
 * InitThread : initialize an audio decoder thread
 ******************************************************************************
 * This function is called from RunThread and performs the second step of the
 * initialization. It returns 0 on success.
 ******************************************************************************/
static int InitThread( adec_thread_t * p_adec )
{
    aout_fifo_t         aout_fifo;

    intf_DbgMsg("adec debug: initializing audio decoder thread %p\n", p_adec);

    /* Our first job is to initialize the bit stream structure with the
     * beginning of the input stream */
    pthread_mutex_lock( &p_adec->fifo.data_lock );
    while ( DECODER_FIFO_ISEMPTY(p_adec->fifo) )
    {
        pthread_cond_wait( &p_adec->fifo.data_wait, &p_adec->fifo.data_lock );
    }
    p_adec->bit_stream.p_ts = DECODER_FIFO_START( p_adec->fifo )->p_first_ts;
    p_adec->bit_stream.i_byte = p_adec->bit_stream.p_ts->i_payload_start;
    pthread_mutex_unlock( &p_adec->fifo.data_lock );

    /* Now we look for an audio frame header in the input stream */
    if ( FindHeader(p_adec) )
    {
        return( -1 );                              /* b_die or b_error is set */
    }

    /*
     * We have the header and all its informations : we must be able to create
     * the audio output fifo.
     */

    /* Is the sound in mono mode or stereo mode ? */
    if ( (p_adec->bit_stream.fifo.buffer & ADEC_HEADER_MODE_MASK) == ADEC_HEADER_MODE_MASK )
    {
        intf_DbgMsg("adec debug: mode == mono\n");
        aout_fifo.i_type = AOUT_ADEC_MONO_FIFO;
        aout_fifo.b_stereo = 0;
    }
    else
    {
        intf_DbgMsg("adec debug: mode == stereo\n");
        aout_fifo.i_type = AOUT_ADEC_STEREO_FIFO;
        aout_fifo.b_stereo = 1;
    }

    /* Checking the sampling frequency */
    switch ( (p_adec->bit_stream.fifo.buffer & ADEC_HEADER_SAMPLING_FREQUENCY_MASK) \
             >> ADEC_HEADER_SAMPLING_FREQUENCY_SHIFT )
    {
        case 0:
            intf_DbgMsg("adec debug: sampling_frequency == 44100 Hz\n");
            aout_fifo.l_rate = 44100;
            break;

        case 1:
            intf_DbgMsg("adec debug: sampling_frequency == 48000 Hz\n");
            aout_fifo.l_rate = 48000;
            break;

        case 2:
            intf_DbgMsg("adec debug: sampling_frequency == 32000 Hz\n");
            aout_fifo.l_rate = 32000;
            break;

        case 3:
            intf_ErrMsg("adec error: can't create audio output fifo (sampling_frequency == `reserved')\n");
            return( -1 );
    }

    /* Creating the audio output fifo */
    if ( (p_adec->p_aout_fifo = aout_CreateFifo(p_adec->p_aout, &aout_fifo)) == NULL )
    {
        return( -1 );
    }

    intf_DbgMsg("adec debug: audio decoder thread %p initialized\n", p_adec);
    return( 0 );
}

#define UPDATE_INCREMENT( increment, integer ) \
    if ( ((increment).l_remainder += (increment).l_euclidean_remainder) >= 0 ) \
    { \
        (integer) += (increment).l_euclidean_integer + 1; \
        (increment).l_remainder -= (increment).l_euclidean_denominator; \
    } \
    else \
    { \
        (integer) += (increment).l_euclidean_integer; \
    }

/******************************************************************************
 * RunThread : audio decoder thread
 ******************************************************************************
 * Audio decoder thread. This function does only returns when the thread is
 * terminated.
 ******************************************************************************/
static void RunThread( adec_thread_t * p_adec )
{
//    static const int    pi_framesize[512] = ADEC_FRAME_SIZE;
//    int                 i_header;
//    int                 i_framesize;
//    int                 i_dummy;
    s64                 s64_numerator;
    s64                 s64_denominator;
    /* The synchronization needs date and date_increment for the moment */
    mtime_t             date = 0;
    aout_increment_t    date_increment;

    intf_DbgMsg("adec debug: running audio decoder thread (%p) (pid == %i)\n", p_adec, getpid());

    /* Initializing the audio decoder thread */
    if ( InitThread(p_adec) )
    {
        p_adec->b_error = 1;
    }

    /* Initializing date_increment */
    switch ( (p_adec->bit_stream.fifo.buffer & ADEC_HEADER_LAYER_MASK) >> ADEC_HEADER_LAYER_SHIFT )
    {
        /* Layer 2 */
        case 2:
            s64_numerator = 1152 * 1000000;
            s64_denominator = (s64)p_adec->p_aout->dsp.l_rate;
            break;

        /* Layer 1 */
        case 3:
            s64_numerator = 384 * 1000000;
            s64_denominator = (s64)p_adec->p_aout->dsp.l_rate;
            break;
    }
    date_increment.l_remainder = -(long)s64_denominator;
    date_increment.l_euclidean_integer = 0;
    while ( s64_numerator >= s64_denominator )
    {
        date_increment.l_euclidean_integer++;
        s64_numerator -= s64_denominator;
    }
    date_increment.l_euclidean_remainder = (long)s64_numerator;
    date_increment.l_euclidean_denominator = (long)s64_denominator;

    /* Audio decoder thread's main loop */
    while ( (!p_adec->b_die) && (!p_adec->b_error) )
    {
        switch ( (p_adec->bit_stream.fifo.buffer & ADEC_HEADER_LAYER_MASK) >> ADEC_HEADER_LAYER_SHIFT )
        {
            case 0:
                intf_DbgMsg("adec debug: layer == 0 (reserved)\n");
                p_adec->bit_stream.fifo.buffer = 0;
                p_adec->bit_stream.fifo.i_available = 0;
                break;

            case 1:
                p_adec->bit_stream.fifo.buffer = 0;
                p_adec->bit_stream.fifo.i_available = 0;
                break;

            case 2:
                if ( (p_adec->bit_stream.fifo.buffer & ADEC_HEADER_MODE_MASK) == ADEC_HEADER_MODE_MASK )
                {
                    adec_Layer2_Mono( p_adec );
                }
                else
                {
//                    i_header = p_adec->bit_stream.fifo.buffer;
//                    i_framesize = pi_framesize[ 128*((i_header & ADEC_HEADER_LAYER_MASK) >> ADEC_HEADER_LAYER_SHIFT) +
//                        64*((i_header & ADEC_HEADER_PADDING_BIT_MASK) >> ADEC_HEADER_PADDING_BIT_SHIFT) +
//                        16*((i_header & ADEC_HEADER_SAMPLING_FREQUENCY_MASK) >> ADEC_HEADER_SAMPLING_FREQUENCY_SHIFT) +
//                        1*((i_header & ADEC_HEADER_BITRATE_INDEX_MASK) >> ADEC_HEADER_BITRATE_INDEX_SHIFT) ];
//                    for ( i_dummy = 0; i_dummy < i_framesize; i_dummy++ )
//                    {
//                        GetByte( &p_adec->bit_stream );
//                    }
//                    for ( i_dummy = 0; i_dummy < 512; i_dummy++ )
//                    {
//                        p_adec->bank_0.v1[ i_dummy ] = .0;
//                        p_adec->bank_1.v1[ i_dummy ] = .0;
//                        p_adec->bank_0.v2[ i_dummy ] = .0;
//                        p_adec->bank_1.v2[ i_dummy ] = .0;
//                    }

                    pthread_mutex_lock( &p_adec->p_aout_fifo->data_lock );
                    /* adec_Layer2_Stereo() produces 6 output frames (2*1152/384)...
                     * If these 6 frames would be recorded in the audio output fifo,
                     * the l_end_frame index would be incremented 6 times. But, if after
                     * this operation the audio output fifo would contain less than 6 frames,
                     * it would mean that we had not enough room to store the 6 frames :-P */
                    while ( (((p_adec->p_aout_fifo->l_end_frame + 6) - p_adec->p_aout_fifo->l_start_frame) & AOUT_FIFO_SIZE) < 6 ) /* !! */
                    {
                        pthread_cond_wait( &p_adec->p_aout_fifo->data_wait, &p_adec->p_aout_fifo->data_lock );
                    }
                    pthread_mutex_unlock( &p_adec->p_aout_fifo->data_lock );

                    if ( adec_Layer2_Stereo(p_adec) )
                    {
                        pthread_mutex_lock( &p_adec->p_aout_fifo->data_lock );
                        /* Frame 1 */
                        p_adec->p_aout_fifo->date[p_adec->p_aout_fifo->l_end_frame] = date;
                        p_adec->p_aout_fifo->l_end_frame = (p_adec->p_aout_fifo->l_end_frame + 1) & AOUT_FIFO_SIZE;
                        /* Frame 2 */
                        p_adec->p_aout_fifo->date[p_adec->p_aout_fifo->l_end_frame] = LAST_MDATE;
                        p_adec->p_aout_fifo->l_end_frame = (p_adec->p_aout_fifo->l_end_frame + 1) & AOUT_FIFO_SIZE;
                        /* Frame 3 */
                        p_adec->p_aout_fifo->date[p_adec->p_aout_fifo->l_end_frame] = LAST_MDATE;
                        p_adec->p_aout_fifo->l_end_frame = (p_adec->p_aout_fifo->l_end_frame + 1) & AOUT_FIFO_SIZE;
                        /* Frame 4 */
                        p_adec->p_aout_fifo->date[p_adec->p_aout_fifo->l_end_frame] = LAST_MDATE;
                        p_adec->p_aout_fifo->l_end_frame = (p_adec->p_aout_fifo->l_end_frame + 1) & AOUT_FIFO_SIZE;
                        /* Frame 5 */
                        p_adec->p_aout_fifo->date[p_adec->p_aout_fifo->l_end_frame] = LAST_MDATE;
                        p_adec->p_aout_fifo->l_end_frame = (p_adec->p_aout_fifo->l_end_frame + 1) & AOUT_FIFO_SIZE;
                        /* Frame 6 */
                        p_adec->p_aout_fifo->date[p_adec->p_aout_fifo->l_end_frame] = LAST_MDATE;
                        p_adec->p_aout_fifo->l_end_frame = (p_adec->p_aout_fifo->l_end_frame + 1) & AOUT_FIFO_SIZE;
                        pthread_mutex_unlock( &p_adec->p_aout_fifo->data_lock );
                        date += 24000;
/*
                        UPDATE_INCREMENT( date_increment, date )
*/
                    }
                }
                break;

            case 3:
                if ( (p_adec->bit_stream.fifo.buffer & ADEC_HEADER_MODE_MASK) == ADEC_HEADER_MODE_MASK )
                {
                    adec_Layer1_Mono( p_adec );
                }
                else
                {
                    adec_Layer1_Stereo( p_adec );
                }
                break;

            default:
                intf_DbgMsg("adec debug: layer == %i (unknown)\n",
                    (p_adec->bit_stream.fifo.buffer & ADEC_HEADER_LAYER_MASK) >> ADEC_HEADER_LAYER_SHIFT);
                p_adec->bit_stream.fifo.buffer = 0;
                p_adec->bit_stream.fifo.i_available = 0;
                break;
        }
        FindHeader( p_adec );
    }

    /* If b_error is set, the audio decoder thread enters the error loop */
    if ( p_adec->b_error )
    {
        ErrorThread( p_adec );
    }

    /* End of the audio decoder thread */
    EndThread( p_adec );
}

/******************************************************************************
 * ErrorThread : audio decoder's RunThread() error loop
 ******************************************************************************
 * This function is called when an error occured during thread main's loop. The
 * thread can still receive feed, but must be ready to terminate as soon as
 * possible.
 ******************************************************************************/
static void ErrorThread( adec_thread_t *p_adec )
{
    /* We take the lock, because we are going to read/write the start/end
     * indexes of the decoder fifo */
    pthread_mutex_lock( &p_adec->fifo.data_lock );

    /* Wait until a `die' order is sent */
    while( !p_adec->b_die )
    {
        /* Trash all received PES packets */
        while( !DECODER_FIFO_ISEMPTY(p_adec->fifo) )
        {
            input_NetlistFreePES( p_adec->bit_stream.p_input, DECODER_FIFO_START(p_adec->fifo) );
	    DECODER_FIFO_INCSTART( p_adec->fifo );
#ifdef DEBUG
//            fprintf(stderr, "*");
#endif
        }

        /* Waiting for the input thread to put new PES packets in the fifo */
        pthread_cond_wait( &p_adec->fifo.data_wait, &p_adec->fifo.data_lock );
    }

    /* We can release the lock before leaving */
    pthread_mutex_unlock( &p_adec->fifo.data_lock );
}

/******************************************************************************
 * EndThread : audio decoder thread destruction
 ******************************************************************************
 * This function is called when the thread ends after a sucessfull 
 * initialization.
 ******************************************************************************/
static void EndThread( adec_thread_t *p_adec )
{
    intf_DbgMsg("adec debug: destroying audio decoder thread %p\n", p_adec);

    /* If the audio output fifo was created, we destroy it */
    if ( p_adec->p_aout_fifo != NULL )
    {
        aout_DestroyFifo( p_adec->p_aout_fifo );
    }
    /* Destroy descriptor */
    free( p_adec );

    intf_DbgMsg("adec debug: audio decoder thread %p destroyed\n", p_adec);
}

/*******************************************************************************
 * pcr.c: PCR management 
 * (c)1999 VideoLAN
 *******************************************************************************
 * Manages structures containing PCR information.
 *******************************************************************************/

/*******************************************************************************
 * Preamble
 *******************************************************************************/
#include <stdio.h>
#include <pthread.h>
#include <sys/uio.h>                                                 /* iovec */
#include <stdlib.h>                               /* atoi(), malloc(), free() */
#include <netinet/in.h>

#include "config.h"
#include "common.h"
#include "mtime.h"
#include "debug.h"
#include "input.h"
#include "intf_msg.h"
#include "input_pcr.h"

/******************************************************************************
 * input_PcrReInit : Reinitialize the pcr_descriptor
 ******************************************************************************/
void input_PcrReInit( input_thread_t *p_input )
{
    pcr_descriptor_t* p_pcr;
    ASSERT(p_input);
    
    p_pcr = p_input->p_pcr;

    p_pcr->c_average = 0;
    p_pcr->c_average_jitter = 0;

#ifdef STATS
    p_pcr->c_pcr = 0;
    p_pcr->max_jitter = 0;

/* For the printf in input_PcrDecode(), this is used for debug purpose only */
    printf("\n");
#endif
}

/******************************************************************************
 * input_PcrInit : Initialize PCR decoder
 ******************************************************************************/
int input_PcrInit( input_thread_t *p_input )
{
    ASSERT(p_input);

    if( (p_input->p_pcr = malloc(sizeof(pcr_descriptor_t))) == NULL )
    {
        return( -1 );
    }
    pthread_mutex_init( &p_input->p_pcr->lock, NULL );
    input_PcrReInit(p_input);
    
    return( 0 );
}

/******************************************************************************
 * input_PcrDecode : Decode a PCR frame
 ******************************************************************************/
void input_PcrDecode( input_thread_t *p_input, es_descriptor_t *p_es,
                      u8* p_pcr_data )
{
    s64 pcr_time, sys_time, delta_clock;
    pcr_descriptor_t *p_pcr;
        
    ASSERT(p_pcr_data);
    ASSERT(p_input);
    ASSERT(p_es);

    p_pcr = p_input->p_pcr;
    if( p_es->b_discontinuity )
    {
        input_PcrReInit(p_input);
        p_es->b_discontinuity = 0;
    }
    
    /* Express the PCR in microseconde
     * WARNING: do not remove the casts in the following calculation ! */
    pcr_time = ( (( (s64)U32_AT((u32*)p_pcr_data) << 1 ) | ( p_pcr_data[4] >> 7 )) * 300 ) / 27;
    sys_time = mdate();
    delta_clock = sys_time - pcr_time;
    
    pthread_mutex_lock( &p_pcr->lock );
    
    if( p_pcr->c_average == PCR_MAX_AVERAGE_COUNTER )
    {
        p_pcr->delta_clock = (delta_clock + (p_pcr->delta_clock * (PCR_MAX_AVERAGE_COUNTER-1)))
                             / PCR_MAX_AVERAGE_COUNTER;
    }
    else
    {
        p_pcr->delta_clock = (delta_clock + (p_pcr->delta_clock *  p_pcr->c_average))
                             / (p_pcr->c_average + 1);
        p_pcr->c_average++;
    }

    pthread_mutex_unlock( &p_pcr->lock );
    
#ifdef STATS
    {
        s64 jitter;
        
        jitter = delta_clock - p_pcr->delta_clock;
        /* Compute the maximum jitter */
        if( jitter < 0 )
        {
            if( (p_pcr->max_jitter <= 0 && p_pcr->max_jitter >= jitter) ||
                (p_pcr->max_jitter >= 0 && p_pcr->max_jitter <= -jitter))
                {
                    p_pcr->max_jitter = jitter;
                }
        }
        else
        {
            if( (p_pcr->max_jitter <= 0 && -p_pcr->max_jitter <= jitter) ||
                (p_pcr->max_jitter >= 0 && p_pcr->max_jitter <= jitter))
                {
                    p_pcr->max_jitter = jitter;
                }
        }        
                    
        /* Compute the average jitter */
        if( p_pcr->c_average_jitter == PCR_MAX_AVERAGE_COUNTER )
        {
            p_pcr->average_jitter = (jitter + (p_pcr->average_jitter * (PCR_MAX_AVERAGE_COUNTER-1)))
                                    / PCR_MAX_AVERAGE_COUNTER;
        }
        else
        {
            p_pcr->average_jitter = (jitter + (p_pcr->average_jitter *  p_pcr->c_average_jitter))
                                    / (p_pcr->c_average + 1);
            p_pcr->c_average_jitter++;
        }
        
        printf("delta: % 13Ld, max_jitter: % 9Ld, av. jitter: % 6Ld, PCR %6ld \r",
               p_pcr->delta_clock , p_pcr->max_jitter, p_pcr->average_jitter, p_pcr->c_pcr);    
        fflush(stdout);
        
        p_pcr->c_pcr++;
    }
#endif
}

/******************************************************************************
 * input_PcrClean : Clean PCR structures before dying
 ******************************************************************************/
void input_PcrClean( input_thread_t *p_input )
{
    ASSERT( p_input );

    free( p_input->p_pcr );
}

/**
 * ===========================================================================
 * davs2.c
 * - the AVS2 Video Decoder library for vlc
 * ---------------------------------------------------------------------------
 *
 * CAVS2DEC, a decoding library of Chinese AVS2 video coding standard.
 *
 * ===========================================================================
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_sout.h>
#include <vlc_codec.h>
#include <vlc_block.h>
#include <vlc_block_helper.h>
#include <vlc_bits.h>
#include <vlc_threads.h>
#include <vlc_cpu.h>

#include "davs2.h"

/**
 * ===========================================================================
 * macro defines
 * ===========================================================================
 */
#define CTRL_LOOP_DEC_FILE 0

#ifndef UNREFERENCED_PARAMETER
#if defined(_MSC_VER) || defined(__INTEL_COMPILER)
#define UNREFERENCED_PARAMETER(v) (v)
#else
#define UNREFERENCED_PARAMETER(v) (void)(v)
#endif
#endif

/*****************************************************************************
 * decoder_sys_t: libdeavs2 decoder descriptor
 *****************************************************************************/
struct decoder_sys_t
{
    block_t *frame;
    void * decoder;

    bool decoder_open;

    davs2_param_t    param;      // decoding parameters
    davs2_packet_t   packet;     // input bitstream

    davs2_picture_t  out_frame;  // output data, frame data
    davs2_seq_info_t headerset;  // output data, sequence header
};

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  OpenDecoder ( vlc_object_t * );
static void CloseDecoder( vlc_object_t * );
static int *Decode( decoder_t *, block_t * );
static void Flush( vlc_object_t * );
static void DumpFrames(picture_t *, davs2_picture_t * );

static const unsigned int FRAME_RATE_DEV[8][2] = {
    {24000,1001}, {24,1}, {25,1},
    {30000,1001}, {30,1}, {50,1},
    {60000,1001}, {60,1}
};

vlc_module_begin()
    set_shortname("avs2")
    set_description( N_("avs2 Decoder library") )
    set_capability("video decoder", 200)
    set_callbacks(OpenDecoder, CloseDecoder)
    set_category(CAT_INPUT)
    set_subcategory( SUBCAT_INPUT_VCODEC )
vlc_module_end ()

/*****************************************************************************
 * Open: open the decoder
 *****************************************************************************/
static int OpenDecoder( vlc_object_t *p_this )
{
    decoder_t     *p_dec = (decoder_t*)p_this;
    struct decoder_sys_t *p_sys;

    // check only AVS2 
    if ( p_dec->fmt_in.i_codec != VLC_CODEC_AVS2 ) 
       return VLC_EGENERIC;

    // get new sys instance
    if( ( p_dec->p_sys = p_sys = malloc( sizeof(struct decoder_sys_t) ) ) == NULL )
        return VLC_ENOMEM;

    // set compile params
    p_sys->param.threads = 0; 		// 0:auto, 1:single, ... 
    p_sys->param.info_level = 0; 	// 0:all , 1:warning and errors, 2: only errors
    //p_sys->param.disable_avx = 1;
    p_sys->param.disable_avx  = !vlc_CPU_AVX2();	//AVX2

    // open decoder
    p_sys->decoder = davs2_decoder_open(&p_sys->param);

	// open failed
	if( !p_sys->decoder )
	{
		msg_Err( p_dec,"Error: davs2_decoder_open.\n");		
        	return VLC_EGENERIC;
	}

    p_sys->decoder_open = true;

    // set call back
    p_dec->pf_decode = Decode;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Decode: the whole thing
 *****************************************************************************/
static int *Decode(decoder_t *p_dec, block_t *p_block)
{
    struct decoder_sys_t *p_sys = p_dec->p_sys;    // avs2 context
    picture_t *p_pic;                       // frame
    int ret = 0;                            // decode length

	bool b_need_update = false;

    if( p_block == NULL )
        return VLCDEC_SUCCESS;

    if( p_block->i_flags & (BLOCK_FLAG_CORRUPTED|BLOCK_FLAG_DISCONTINUITY) )
    {
        date_Set( &p_sys->packet.pts, p_block->i_dts );
        if( p_block->i_flags & BLOCK_FLAG_CORRUPTED )
        {
            block_Release( p_block );
            return VLCDEC_SUCCESS;
        }
    }

    p_sys->packet.data = p_block->p_buffer;  // Payload start
    p_sys->packet.len  = p_block->i_buffer;  // Payload length

    // set original pts && dts
    //p_sys->packet.pts  = p_block->i_pts ? p_block->i_pts : p_block->i_dts;
    p_sys->packet.dts  = p_block->i_dts;

    if( p_block->i_pts == VLC_TICK_INVALID && p_block->i_dts == VLC_TICK_INVALID &&
        date_Get( &p_sys->packet.pts ) == VLC_TICK_INVALID )
    {
        block_Release( p_block );
        return VLCDEC_SUCCESS;
    }

    if( p_block->i_pts != VLC_TICK_INVALID )
    {
        date_Set( &p_sys->packet.pts, p_block->i_pts );
    }
    else if( p_block->i_dts != VLC_TICK_INVALID )
    {
        date_Set( &p_sys->packet.pts, p_block->i_dts );
    }

    // do decode
    ret = davs2_decoder_send_packet(p_sys->decoder, &p_sys->packet);
    if (ret == DAVS2_ERROR) 
    {
        msg_Err( p_dec,"Error: davs2_decoder_send_packet.\n");
        block_Release( p_block );
        return VLCDEC_SUCCESS;
    }

    // fetch sources anyways
    ret = davs2_decoder_recv_frame(p_sys->decoder, &p_sys->headerset, &p_sys->out_frame);
    block_Release(p_block);
    if (ret == DAVS2_DEFAULT) 	// DAVS2_DEFAULT: frames won't display now
    {
        return VLCDEC_SUCCESS;
    }

	// make a short name
	video_format_t *vi = &p_dec->fmt_in.video;
	video_format_t *vo = &p_dec->fmt_out.video;
	davs2_picture_t *p_frame = &p_sys->out_frame;

	p_dec->fmt_out.i_codec =p_sys->headerset.output_bit_depth == 10 ? VLC_CODEC_I420_10L : VLC_CODEC_I420;

    if (ret == DAVS2_GOT_HEADER) 
	{
        	vo->i_visible_width  = vo->i_width  = p_sys->headerset.width;
		vo->i_visible_height = vo->i_height = p_sys->headerset.height;

        	int n = p_sys->headerset.frame_rate_id;
	        vo->i_frame_rate        = FRAME_RATE_DEV[n-1][0];
        	vo->i_frame_rate_base   = FRAME_RATE_DEV[n-1][1];

        	vo->i_sar_num = 1;
        	vo->i_sar_den = 1;

		msg_Err( p_dec,"Error: DAVS2_GOT_HEADER to decoder_UpdateVideoFormat.\n");
	        decoder_UpdateVideoFormat( p_dec );
	        return VLCDEC_SUCCESS;
	}

	// set video resolution ratio
	if( vo->i_visible_width  != p_frame->widths[0] ||
	vo->i_visible_height != p_frame->lines[0] )
	{
        	vo->i_visible_width  = vo->i_width  = p_frame->widths[0];
		vo->i_visible_height = vo->i_height = p_frame->lines[0];
		b_need_update = true;
	}

    	// set video rate
	    if( !vo->i_frame_rate )
    	{
        	int n = p_sys->headerset.frame_rate_id;
	        vo->i_frame_rate        = FRAME_RATE_DEV[n-1][0];
        	vo->i_frame_rate_base   = FRAME_RATE_DEV[n-1][1];
	        b_need_update = true;
    	}

	// set sample/pixel aspect ratio
	if( !vo->i_sar_num || !vo->i_sar_den )
	{
        	vo->i_sar_num = 1;
        	vo->i_sar_den = 1;
	        b_need_update = true;
    	}

	// update format, return 0 if success
	if(b_need_update)
	{
		msg_Err( p_dec,"Error: para changed, to decoder_UpdateVideoFormat.\n");
	        if(decoder_UpdateVideoFormat( p_dec ))
	            return VLCDEC_SUCCESS;
    	}

   
    // get a new picture
    p_pic = decoder_NewPicture( p_dec );

    // not get
    if( !p_pic )
        return VLCDEC_SUCCESS;

    // dump frame from out_frame to p_pic
    DumpFrames(p_pic, p_frame);

    // free feame
    davs2_decoder_frame_unref(p_sys->decoder, &p_sys->out_frame);

    // transfer to vlc fifo
    decoder_QueueVideo( p_dec, p_pic );

    return VLCDEC_SUCCESS;
}



/*****************************************************************************
 * DumpFrames
 *****************************************************************************/
static void DumpFrames(picture_t *p_pic, davs2_picture_t *p_frame )
{
    // dump frame from out_frame to p_pic
    for( int plane = 0; plane < p_pic->i_planes; plane++ ) 
    {
        uint8_t *src = p_frame->planes[plane];
        uint8_t *dst = p_pic->p[plane].p_pixels;

        int dst_stride = p_pic->p[plane].i_pitch;
        int src_stride = p_frame->strides[plane];
        
        int size = __MIN( src_stride, dst_stride );
        for( int line = 0; line < p_pic->p[plane].i_visible_lines; line++ ) 
        {
            memcpy( dst, src, size );
            src += src_stride;
            dst += dst_stride;
        }
    }
    // fetch pts
    p_pic->date = p_frame->pts;
    // not support interlacing
    p_pic->b_progressive = true;

}
/*****************************************************************************
 * Close: clean up the decoder
 *****************************************************************************/
static void CloseDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t *)p_this;
    struct decoder_sys_t *p_sys = p_dec->p_sys;
    picture_t *p_pic;
    int ret = 0 ;

    decoder_AbortPictures( p_dec, true );

    /* do not flush buffers if codec hasn't been opened */
    if( p_sys && p_sys->decoder_open ){
        while(1) {
            ret = davs2_decoder_flush(p_sys->decoder, &p_sys->headerset, &p_sys->out_frame);
            if (ret == DAVS2_ERROR || ret == DAVS2_END)
                break;
            if (ret != DAVS2_DEFAULT) 
            {
                if(p_sys->decoder_open){
                    p_pic = decoder_NewPicture(p_dec);
                    if(p_pic){
                        DumpFrames(p_pic, &p_sys->out_frame);
                        decoder_QueueVideo( p_dec, p_pic );
                    }
                }
                davs2_decoder_frame_unref(p_sys->decoder, &p_sys->out_frame);
            }
        }
    }

    /* Reset cancel state to false */
    decoder_AbortPictures( p_dec, false );

    p_sys->decoder_open = false;

    // close decoder, free itself
    if( p_sys->decoder )
        davs2_decoder_close( p_sys->decoder );

    free( p_sys );
}

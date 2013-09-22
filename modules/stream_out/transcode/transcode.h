#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <vlc_common.h>

#include <vlc_sout.h>
#include <vlc_filter.h>
#include <vlc_es.h>
#include <vlc_codec.h>

#include <vlc_picture_fifo.h>

#define MASTER_SYNC_MAX_DRIFT 100000

struct sout_stream_sys_t
{
    sout_stream_id_t *id_video;
    block_t         *p_buffers;
    vlc_mutex_t     lock_out;
    vlc_cond_t      cond;
    bool            b_abort;
    picture_fifo_t *pp_pics;
    vlc_thread_t    thread;

    /* Audio */
    vlc_fourcc_t    i_acodec;   /* codec audio (0 if not transcode) */
    audio_sample_format_t   fmt_audio;
    char            *psz_aenc;
    char            *psz_alang;
    config_chain_t  *p_audio_cfg;
    uint32_t        i_sample_rate;
    uint32_t        i_channels;
    int             i_abitrate;

    char            *psz_af;

    /* Video */
    vlc_fourcc_t    i_vcodec;   /* codec video (0 if not transcode) */
    video_format_t  fmt_input_video;
    char            *psz_venc;
    config_chain_t  *p_video_cfg;
    int             i_vbitrate;
    double          f_scale;
    double          f_fps;
    unsigned int    i_width, i_maxwidth;
    unsigned int    i_height, i_maxheight;
    bool            b_deinterlace;
    char            *psz_deinterlace;
    config_chain_t  *p_deinterlace_cfg;
    int             i_threads;
    bool            b_high_priority;
    bool            b_hurry_up;

    char            *psz_vf2;

    /* SPU */
    vlc_fourcc_t    i_scodec;   /* codec spu (0 if not transcode) */
    char            *psz_senc;
    bool            b_soverlay;
    config_chain_t  *p_spu_cfg;
    spu_t           *p_spu;
    filter_t        *p_spu_blend;

    /* OSD Menu */
    vlc_fourcc_t    i_osdcodec; /* codec osd menu (0 if not transcode) */
    char            *psz_osdenc;
    config_chain_t  *p_osd_cfg;
    bool            b_osd;   /* true when osd es is registered */

    /* Sync */
    bool            b_master_sync;
    mtime_t         i_master_drift;
};

struct aout_filters;

struct sout_stream_id_t
{
    bool            b_transcode;

    /* id of the out stream */
    void *id;

    /* Decoder */
    decoder_t       *p_decoder;

    union
    {
         struct
         {
             filter_chain_t  *p_f_chain; /**< Video filters */
             filter_chain_t  *p_uf_chain; /**< User-specified video filters */
         };
         struct aout_filters *p_af_chain; /**< Audio filters */
    };

    /* Encoder */
    encoder_t       *p_encoder;

    /* Sync */
    date_t          interpolated_pts;
};

/* OSD */

int transcode_osd_new( sout_stream_t *p_stream, sout_stream_id_t *id );
void transcode_osd_close( sout_stream_t *p_stream, sout_stream_id_t *id);
int transcode_osd_process( sout_stream_t *p_stream, sout_stream_id_t *id,
                                  block_t *in, block_t **out );
bool transcode_osd_add    ( sout_stream_t *, es_format_t *, sout_stream_id_t *);

/* SPU */

int  transcode_spu_new    ( sout_stream_t *, sout_stream_id_t * );
void transcode_spu_close  ( sout_stream_t *, sout_stream_id_t * );
int  transcode_spu_process( sout_stream_t *, sout_stream_id_t *,
                                   block_t *, block_t ** );
bool transcode_spu_add    ( sout_stream_t *, es_format_t *, sout_stream_id_t *);

/* AUDIO */

int  transcode_audio_new    ( sout_stream_t *, sout_stream_id_t * );
void transcode_audio_close  ( sout_stream_id_t * );
int  transcode_audio_process( sout_stream_t *, sout_stream_id_t *,
                                     block_t *, block_t ** );
bool transcode_audio_add    ( sout_stream_t *, es_format_t *,
                                sout_stream_id_t *);

/* VIDEO */

int  transcode_video_new    ( sout_stream_t *, sout_stream_id_t * );
void transcode_video_close  ( sout_stream_t *, sout_stream_id_t * );
int  transcode_video_process( sout_stream_t *, sout_stream_id_t *,
                                     block_t *, block_t ** );
bool transcode_video_add    ( sout_stream_t *, es_format_t *,
                                sout_stream_id_t *);

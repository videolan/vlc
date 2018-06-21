#include <vlc_picture_fifo.h>
#include <vlc_filter.h>
#include <vlc_codec.h>

/*100ms is around the limit where people are noticing lipsync issues*/
#define MASTER_SYNC_MAX_DRIFT VLC_TICK_FROM_MS(100)

typedef struct sout_stream_id_sys_t sout_stream_id_sys_t;

typedef struct
{
    sout_stream_id_sys_t *id_video;
    uint32_t        pool_size;

    /* Audio */
    vlc_fourcc_t    i_acodec;   /* codec audio (0 if not transcode) */
    char            *psz_aenc;
    char            *psz_alang;
    config_chain_t  *p_audio_cfg;
    uint32_t        i_sample_rate;
    uint32_t        i_channels;
    int             i_abitrate;

    char            *psz_af;

    /* Video */
    vlc_fourcc_t    i_vcodec;   /* codec video (0 if not transcode) */
    char            *psz_venc;
    config_chain_t  *p_video_cfg;
    int             i_vbitrate;
    float           f_scale;
    unsigned int    i_width, i_maxwidth;
    unsigned int    i_height, i_maxheight;
    char            *psz_deinterlace;
    config_chain_t  *p_deinterlace_cfg;
    int             i_threads;
    int             i_thread_priority;
    bool            b_hurry_up;
    unsigned int    fps_num,fps_den;

    char            *psz_vf2;

    /* SPU */
    vlc_fourcc_t    i_scodec;   /* codec spu (0 if not transcode) */
    char            *psz_senc;
    bool            b_soverlay;
    config_chain_t  *p_spu_cfg;
    spu_t           *p_spu;
    filter_t        *p_spu_blend;
    unsigned int     i_spu_width; /* render width */
    unsigned int     i_spu_height;

    /* Sync */
    bool            b_master_sync;
    /* i_master drift is how much audio buffer is ahead of calculated pts */
    vlc_tick_t      i_master_drift;
} sout_stream_sys_t;

struct aout_filters;

struct sout_stream_id_sys_t
{
    bool            b_transcode;
    bool            b_error;

    /* id of the out stream */
    void *downstream_id;

    /* Decoder */
    decoder_t       *p_decoder;

    struct
    {
        vlc_mutex_t lock;
        union
        {
            struct {
                picture_t *first;
                picture_t **last;
            } pic;
            struct {
                subpicture_t *first;
                subpicture_t **last;
            } spu;
            struct {
                block_t *first;
                block_t **last;
            } audio;
        };
    } fifo;

    union
    {
         struct
         {
             filter_chain_t  *p_f_chain; /**< Video filters */
             filter_chain_t  *p_uf_chain; /**< User-specified video filters */
             video_format_t  fmt_input_video;
             video_format_t  video_dec_out; /* only rw from pf_vout_format_update() */
         };
         struct
         {
             struct aout_filters    *p_af_chain; /**< Audio filters */
             audio_format_t  fmt_audio;
             audio_format_t  audio_dec_out; /* only rw from pf_aout_format_update() */
         };

    };

    /* Encoder */
    encoder_t       *p_encoder;
    block_t         *p_buffers;
    vlc_thread_t    thread;
    vlc_mutex_t     lock_out;
    bool            b_abort;
    picture_fifo_t *pp_pics;
    vlc_sem_t       picture_pool_has_room;
    vlc_cond_t      cond;


    /* Sync */
    date_t          next_input_pts; /**< Incoming calculated PTS */
    date_t          next_output_pts; /**< output calculated PTS */

};

struct decoder_owner
{
    decoder_t dec;
    vlc_object_t *p_obj;
    sout_stream_id_sys_t *id;
};

static inline struct decoder_owner *dec_get_owner( decoder_t *p_dec )
{
    return container_of( p_dec, struct decoder_owner, dec );
}

/* SPU */

void transcode_spu_close  ( sout_stream_t *, sout_stream_id_sys_t * );
int  transcode_spu_process( sout_stream_t *, sout_stream_id_sys_t *,
                                   block_t *, block_t ** );
bool transcode_spu_add    ( sout_stream_t *, const es_format_t *, sout_stream_id_sys_t *);

/* AUDIO */

void transcode_audio_close  ( sout_stream_id_sys_t * );
int  transcode_audio_process( sout_stream_t *, sout_stream_id_sys_t *,
                                     block_t *, block_t ** );
bool transcode_audio_add    ( sout_stream_t *, const es_format_t *,
                                sout_stream_id_sys_t *);

/* VIDEO */

void transcode_video_close  ( sout_stream_t *, sout_stream_id_sys_t * );
int  transcode_video_process( sout_stream_t *, sout_stream_id_sys_t *,
                                     block_t *, block_t ** );
bool transcode_video_add    ( sout_stream_t *, const es_format_t *,
                                sout_stream_id_sys_t *);

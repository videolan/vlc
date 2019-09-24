#include <vlc_picture_fifo.h>
#include <vlc_filter.h>
#include <vlc_codec.h>
#include "encoder/encoder.h"

/*100ms is around the limit where people are noticing lipsync issues*/
#define MASTER_SYNC_MAX_DRIFT VLC_TICK_FROM_MS(100)

typedef struct
{
    char *psz_filters;
    union
    {
        struct
        {
            char            *psz_deinterlace;
            config_chain_t  *p_deinterlace_cfg;
            char            *psz_spu_sources;
            bool             b_reorient;
        } video;
    };
} sout_filters_config_t;

static inline
void sout_filters_config_init( sout_filters_config_t *p_cfg )
{
    memset( p_cfg, 0, sizeof(*p_cfg) );
}

static inline
void sout_filters_config_clean( sout_filters_config_t *p_cfg )
{
    free( p_cfg->psz_filters );
    if( p_cfg->video.psz_deinterlace )
    {
        free( p_cfg->video.psz_deinterlace );
        config_ChainDestroy( p_cfg->video.p_deinterlace_cfg );
    }
    free( p_cfg->video.psz_spu_sources );
}

typedef struct sout_stream_id_sys_t sout_stream_id_sys_t;

typedef struct
{
    bool                  b_soverlay;

    /* Audio */
    transcode_encoder_config_t aenc_cfg;
    sout_filters_config_t afilters_cfg;

    /* Video */
    transcode_encoder_config_t venc_cfg;
    sout_filters_config_t vfilters_cfg;

    /* SPU */
    transcode_encoder_config_t senc_cfg;

    /* Shared betweeen streams */
    vlc_mutex_t     lock;
    /* Sync */
    bool            b_master_sync;
    sout_stream_id_sys_t *id_master_sync;
    /* Spu's video */
    sout_stream_id_sys_t *id_video;

} sout_stream_sys_t;

struct aout_filters;

struct sout_stream_id_sys_t
{
    bool            b_transcode;
    bool            b_error;

    /* id of the out stream */
    void *downstream_id;
    void *(*pf_transcode_downstream_add)( sout_stream_t *,
                                          const es_format_t *orig,
                                          const es_format_t *current );

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
            int (*pf_drift_validate)(void *cbdata, vlc_tick_t);
        };
        struct
        {
            void (*pf_send_subpicture)(void *cbdata, subpicture_t *);
            int (*pf_get_output_dimensions)(void *cbdata, unsigned *, unsigned *);
            vlc_tick_t (*pf_get_master_drift)(void *cbdata);
        };
    };
    void *callback_data;

    union
    {
         struct
         {
             filter_chain_t  *p_f_chain; /**< Video filters */
             filter_chain_t  *p_conv_nonstatic;
             filter_chain_t  *p_conv_static;
             filter_chain_t  *p_uf_chain; /**< User-specified video filters */
             filter_chain_t  *p_final_conv_static;
             vlc_blender_t   *p_spu_blender;
             spu_t           *p_spu;
             vlc_decoder_device *dec_dev;
             vlc_video_context *enc_vctx_in;
         };
         struct
         {
             struct aout_filters    *p_af_chain; /**< Audio filters */
             audio_format_t  fmt_input_audio;
         };
    };

    /* only rw from pf_*_format_update() */
    es_format_t decoder_out;
    vlc_video_context *decoder_vctx_out;

    const sout_filters_config_t *p_filterscfg;

    /* Encoder */
    const transcode_encoder_config_t *p_enccfg;
    transcode_encoder_t *encoder;

    /* Sync */
    date_t          next_input_pts; /**< Incoming calculated PTS */
    vlc_tick_t      i_drift; /** how much buffer is ahead of calculated PTS */
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

static inline void es_format_SetMeta( es_format_t *p_dst, const es_format_t *p_src )
{
    p_dst->i_group = p_src->i_group;
    p_dst->i_id = p_src->i_id;
    if( p_src->psz_language )
    {
        free( p_dst->psz_language );
        p_dst->psz_language = strdup( p_src->psz_language );
    }
    if( p_src->psz_description )
    {
        free( p_dst->psz_description );
        p_dst->psz_description = strdup( p_src->psz_description );
    }
}

static inline void transcode_remove_filters( filter_chain_t **pp )
{
    if( *pp )
    {
        filter_chain_Delete( *pp );
        *pp = NULL;
    }
}

/* SPU */

void transcode_spu_clean  ( sout_stream_t *, sout_stream_id_sys_t * );
int  transcode_spu_process( sout_stream_t *, sout_stream_id_sys_t *,
                                   block_t *, block_t ** );
int  transcode_spu_init   ( sout_stream_t *, const es_format_t *, sout_stream_id_sys_t *);

/* AUDIO */

void transcode_audio_clean  ( sout_stream_t *, sout_stream_id_sys_t * );
int  transcode_audio_process( sout_stream_t *, sout_stream_id_sys_t *,
                                     block_t *, block_t ** );
int  transcode_audio_init   ( sout_stream_t *, const es_format_t *,
                              sout_stream_id_sys_t *);

/* VIDEO */

void transcode_video_clean  ( sout_stream_id_sys_t * );
int  transcode_video_process( sout_stream_t *, sout_stream_id_sys_t *,
                                     block_t *, block_t ** );
int transcode_video_get_output_dimensions( sout_stream_id_sys_t *,
                                           unsigned *w, unsigned *h );
void transcode_video_push_spu( sout_stream_t *, sout_stream_id_sys_t *, subpicture_t * );
int  transcode_video_init    ( sout_stream_t *, const es_format_t *,
                               sout_stream_id_sys_t *);

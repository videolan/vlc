/*****************************************************************************
 * kate.c : a decoder for the kate bitstream format
 *****************************************************************************
 * Copyright (C) 2000-2008 VLC authors and VideoLAN
 *
 * Authors: Vincent Penquerc'h <ogg.k.ogg.k@googlemail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_input_item.h>
#include <vlc_codec.h>
#include "../demux/xiph.h"

#include <kate/kate.h>
#ifdef HAVE_TIGER
# include <tiger/tiger.h>
#endif

/* #define ENABLE_PACKETIZER */
/* #define ENABLE_PROFILE */

#ifdef ENABLE_PROFILE
# define PROFILE_START(name) int64_t profile_start_##name = vlc_tick_now()
# define PROFILE_STOP(name) fprintf( stderr, #name ": %f ms\n", (vlc_tick_now() - profile_start_##name)/1000.0f )
#else
# define PROFILE_START(name) ((void)0)
# define PROFILE_STOP(name) ((void)0)
#endif

#define CHECK_TIGER_RET( statement )                                   \
    do                                                                 \
    {                                                                  \
        int i_ret_check = (statement);                                 \
        if( i_ret_check < 0 )                                          \
        {                                                              \
            msg_Dbg( p_dec, "Error in " #statement ": %d",             \
                     i_ret_check );                                    \
        }                                                              \
    } while( 0 )

/*****************************************************************************
 * decoder_sys_t : decoder descriptor
 *****************************************************************************/
typedef struct
{
#ifdef ENABLE_PACKETIZER
    /* Module mode */
    bool b_packetizer;
#endif

    /*
     * Input properties
     */
    bool b_has_headers;

    /*
     * Kate properties
     */
    bool           b_ready;
    kate_info      ki;
    kate_comment   kc;
    kate_state     k;

    /*
     * Common properties
     */
    vlc_tick_t i_pts;
    vlc_tick_t i_max_stop;

    /* decoder_sys_t is shared between decoder and spu units */
    vlc_mutex_t lock;
    int         i_refcount;

#ifdef HAVE_TIGER
    /*
     * Tiger properties
     */
    tiger_renderer    *p_tr;
    bool               b_dirty;

    uint32_t           i_tiger_default_font_color;
    uint32_t           i_tiger_default_background_color;
    tiger_font_effect  e_tiger_default_font_effect;
    double             f_tiger_default_font_effect_strength;
    char              *psz_tiger_default_font_desc;
    double             f_tiger_quality;
#endif

    /*
     * Options
     */
    bool   b_formatted;
    bool   b_use_tiger;
} decoder_sys_t;

typedef struct
{
    decoder_sys_t *p_dec_sys;
    vlc_tick_t     i_start;
} kate_spu_updater_sys_t;


/*
 * This is a global list of existing decoders.
 * The reason for this list is that:
 *  - I want to be able to reconfigure Tiger when user prefs change
 *  - User prefs are variables which are not specific to a decoder (eg, if
 *    there are several decoders, there will still be one set of variables)
 *  - A callback set on those will not be passed a pointer to the decoder
 *    (as the decoder isn't known, and there could be multiple ones)
 *  - Creating variables in the decoder will create different ones, with
 *    values copied from the relevant user pref, so a callback on those
 *    won't be called when the user pref is changed
 * Therefore, each decoder will register/unregister itself with this list,
 * callbacks will be set for the user prefs, and these will in turn walk
 * through this list and tell each decoder to update the relevant variable.
 * HOWEVER.
 * VLC's variable system is still in my way as I can't get the value of
 * the user pref variables at decoder start *unless* I create my own vars
 * which inherit from the user ones, but those are utterly useless after
 * that first use, since they'll be detached from the ones the user can
 * change. So, I create them, read their values, and then destroy them.
 */
static vlc_mutex_t kate_decoder_list_mutex = VLC_STATIC_MUTEX;
static size_t kate_decoder_list_size = 0;
static decoder_t **kate_decoder_list = NULL;

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  OpenDecoder   ( vlc_object_t * );
static void CloseDecoder  ( vlc_object_t * );
#ifdef ENABLE_PACKETIZER
static int OpenPacketizer( vlc_object_t *p_this );
#endif

static int  DecodeSub( decoder_t *p_dec, block_t *p_block );
static block_t * Packetize( decoder_t *p_dec, block_t **pp_block );
static void Flush( decoder_t * );
static int ProcessHeaders( decoder_t *p_dec );
static void         *ProcessPacket( decoder_t *p_dec, kate_packet *p_kp,
                            block_t *p_block );
static subpicture_t *DecodePacket( decoder_t *p_dec, kate_packet *p_kp,
                            block_t *p_block );
static void ParseKateComments( decoder_t * );
static subpicture_t *SetupSimpleKateSPU( decoder_t *p_dec, subpicture_t *p_spu,
                            const kate_event *ev );
static void DecSysRelease( decoder_sys_t *p_sys );
static void DecSysHold( decoder_sys_t *p_sys );
#ifdef HAVE_TIGER
static uint32_t GetTigerColor( decoder_t *p_dec, const char *psz_prefix );
static char *GetTigerString( decoder_t *p_dec, const char *psz_name );
static int GetTigerInteger( decoder_t *p_dec, const char *psz_name );
static double GetTigerFloat( decoder_t *p_dec, const char *psz_name );
static void UpdateTigerFontColor( decoder_t *p_dec );
static void UpdateTigerBackgroundColor( decoder_t *p_dec );
static void UpdateTigerFontEffect( decoder_t *p_dec );
static void UpdateTigerQuality( decoder_t *p_dec );
static void UpdateTigerFontDesc( decoder_t *p_dec );
#endif

#define DEFAULT_NAME "Default"
#define MAX_LINE 8192

/*****************************************************************************
 * Module descriptor.
 *****************************************************************************/

#define FORMAT_TEXT N_("Formatted Subtitles")
#define FORMAT_LONGTEXT N_("Kate streams allow for text formatting. " \
 "VLC partly implements this, but you can choose to disable all formatting. " \
 "Note that this has no effect is rendering via Tiger is enabled.")

#ifdef HAVE_TIGER

static const tiger_font_effect pi_font_effects[] = { tiger_font_plain, tiger_font_shadow, tiger_font_outline };
static const char * const ppsz_font_effect_names[] = { N_("None"), N_("Shadow"), N_("Outline") };

/* nicked off freetype.c */
static const int pi_color_values[] = {
  0x00000000, 0x00808080, 0x00C0C0C0, 0x00FFFFFF, 0x00800000,
  0x00FF0000, 0x00FF00FF, 0x00FFFF00, 0x00808000, 0x00008000, 0x00008080,
  0x0000FF00, 0x00800080, 0x00000080, 0x000000FF, 0x0000FFFF };
static const char *const ppsz_color_descriptions[] = {
  N_("Black"), N_("Gray"), N_("Silver"), N_("White"), N_("Maroon"),
  N_("Red"), N_("Fuchsia"), N_("Yellow"), N_("Olive"), N_("Green"), N_("Teal"),
  N_("Lime"), N_("Purple"), N_("Navy"), N_("Blue"), N_("Aqua") };

#define TIGER_TEXT N_("Use Tiger for rendering")
#define TIGER_LONGTEXT N_("Kate streams can be rendered using the Tiger library. " \
 "Disabling this will only render static text and bitmap based streams.")

#define TIGER_QUALITY_DEFAULT 1.0
#define TIGER_QUALITY_TEXT N_("Rendering quality")
#define TIGER_QUALITY_LONGTEXT N_("Select rendering quality, at the expense of speed. " \
 "0 is fastest, 1 is highest quality.")

#define TIGER_DEFAULT_FONT_EFFECT_DEFAULT 0
#define TIGER_DEFAULT_FONT_EFFECT_TEXT N_("Default font effect")
#define TIGER_DEFAULT_FONT_EFFECT_LONGTEXT N_("Add a font effect to text to improve readability " \
 "against different backgrounds.")

#define TIGER_DEFAULT_FONT_EFFECT_STRENGTH_DEFAULT 0.5
#define TIGER_DEFAULT_FONT_EFFECT_STRENGTH_TEXT N_("Default font effect strength")
#define TIGER_DEFAULT_FONT_EFFECT_STRENGTH_LONGTEXT N_("How pronounced to make the chosen font effect " \
 "(effect dependent).")

#define TIGER_DEFAULT_FONT_DESC_DEFAULT ""
#define TIGER_DEFAULT_FONT_DESC_TEXT N_("Default font description")
#define TIGER_DEFAULT_FONT_DESC_LONGTEXT N_("Which font description to use if the Kate stream " \
 "does not specify particular font parameters (name, size, etc) to use. " \
 "A blank name will let Tiger choose font parameters where appropriate.")

#define TIGER_DEFAULT_FONT_COLOR_DEFAULT 0x00ffffff
#define TIGER_DEFAULT_FONT_COLOR_TEXT N_("Default font color")
#define TIGER_DEFAULT_FONT_COLOR_LONGTEXT N_("Default font color to use if the Kate stream " \
 "does not specify a particular font color to use.")

#define TIGER_DEFAULT_FONT_ALPHA_DEFAULT 0xff
#define TIGER_DEFAULT_FONT_ALPHA_TEXT N_("Default font alpha")
#define TIGER_DEFAULT_FONT_ALPHA_LONGTEXT N_("Transparency of the default font color if the Kate stream " \
 "does not specify a particular font color to use.")

#define TIGER_DEFAULT_BACKGROUND_COLOR_DEFAULT 0x00ffffff
#define TIGER_DEFAULT_BACKGROUND_COLOR_TEXT N_("Default background color")
#define TIGER_DEFAULT_BACKGROUND_COLOR_LONGTEXT N_("Default background color if the Kate stream " \
 "does not specify a background color to use.")

#define TIGER_DEFAULT_BACKGROUND_ALPHA_DEFAULT 0
#define TIGER_DEFAULT_BACKGROUND_ALPHA_TEXT N_("Default background alpha")
#define TIGER_DEFAULT_BACKGROUND_ALPHA_LONGTEXT N_("Transparency of the default background color if the Kate stream " \
 "does not specify a particular background color to use.")

#endif

#define HELP_TEXT N_( \
    "Kate is a codec for text and image based overlays.\n" \
    "The Tiger rendering library is needed to render complex Kate streams, " \
    "but VLC can still render static text and image based subtitles if " \
    "it is not available.\n" \
    "Note that changing settings below will not take effect until a new stream is played. " \
    "This will hopefully be fixed soon." \
    )

vlc_module_begin ()
    set_shortname( N_("Kate"))
    set_description( N_("Kate overlay decoder") )
    set_help( HELP_TEXT )
    set_capability( "spu decoder", 50 )
    set_callbacks( OpenDecoder, CloseDecoder )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_SCODEC )
    add_shortcut( "kate" )

    add_bool( "kate-formatted", true, FORMAT_TEXT, FORMAT_LONGTEXT,
              true )

#ifdef HAVE_TIGER
    add_bool( "kate-use-tiger", true, TIGER_TEXT, TIGER_LONGTEXT,
              true )
    add_float_with_range( "kate-tiger-quality",
                          TIGER_QUALITY_DEFAULT, 0.0f, 1.0f,
                          TIGER_QUALITY_TEXT, TIGER_QUALITY_LONGTEXT,
                          true )

    set_section( N_("Tiger rendering defaults"), NULL );
    add_string( "kate-tiger-default-font-desc", TIGER_DEFAULT_FONT_DESC_DEFAULT,
                TIGER_DEFAULT_FONT_DESC_TEXT, TIGER_DEFAULT_FONT_DESC_LONGTEXT, true);
    add_integer_with_range( "kate-tiger-default-font-effect",
                            TIGER_DEFAULT_FONT_EFFECT_DEFAULT,
                            0, sizeof(pi_font_effects)/sizeof(pi_font_effects[0])-1,
                            TIGER_DEFAULT_FONT_EFFECT_TEXT, TIGER_DEFAULT_FONT_EFFECT_LONGTEXT,
                            true )
    change_integer_list( pi_font_effects, ppsz_font_effect_names );
    add_float_with_range( "kate-tiger-default-font-effect-strength",
              TIGER_DEFAULT_FONT_EFFECT_STRENGTH_DEFAULT, 0.0f, 1.0f,
              TIGER_DEFAULT_FONT_EFFECT_STRENGTH_TEXT, TIGER_DEFAULT_FONT_EFFECT_STRENGTH_LONGTEXT,
              true )
    add_integer_with_range( "kate-tiger-default-font-color",
                            TIGER_DEFAULT_FONT_COLOR_DEFAULT, 0, 0x00ffffff,
                            TIGER_DEFAULT_FONT_COLOR_TEXT, TIGER_DEFAULT_FONT_COLOR_LONGTEXT,
                            true);
    change_integer_list( pi_color_values, ppsz_color_descriptions );
    add_integer_with_range( "kate-tiger-default-font-alpha",
                            TIGER_DEFAULT_FONT_ALPHA_DEFAULT, 0, 255,
                            TIGER_DEFAULT_FONT_ALPHA_TEXT, TIGER_DEFAULT_FONT_ALPHA_LONGTEXT,
                            true);
    add_integer_with_range( "kate-tiger-default-background-color",
                            TIGER_DEFAULT_BACKGROUND_COLOR_DEFAULT, 0, 0x00ffffff,
                            TIGER_DEFAULT_BACKGROUND_COLOR_TEXT, TIGER_DEFAULT_BACKGROUND_COLOR_LONGTEXT,
                            true);
    change_integer_list( pi_color_values, ppsz_color_descriptions );
    add_integer_with_range( "kate-tiger-default-background-alpha",
                            TIGER_DEFAULT_BACKGROUND_ALPHA_DEFAULT, 0, 255,
                            TIGER_DEFAULT_BACKGROUND_ALPHA_TEXT, TIGER_DEFAULT_BACKGROUND_ALPHA_LONGTEXT,
                            true);
#endif

#ifdef ENABLE_PACKETIZER
    add_submodule ()
    set_description( N_("Kate text subtitles packetizer") )
    set_capability( "packetizer", 100 )
    set_callbacks( OpenPacketizer, CloseDecoder )
    add_shortcut( "kate" )
#endif

vlc_module_end ()

static int OpenCommon( vlc_object_t *p_this, bool b_packetizer )
{
    decoder_t     *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys;

    if( p_dec->fmt_in.i_codec != VLC_CODEC_KATE )
    {
        return VLC_EGENERIC;
    }

    msg_Dbg( p_dec, "kate: OpenDecoder");

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_dec->p_sys = p_sys = malloc(sizeof(*p_sys)) ) == NULL )
        return VLC_ENOMEM;

    if( b_packetizer )
        p_dec->pf_packetize = Packetize;
    else
        p_dec->pf_decode    = DecodeSub;
    p_dec->pf_flush = Flush;

    vlc_mutex_init( &p_sys->lock );
    p_sys->i_refcount = 0;
    DecSysHold( p_sys );

    /* init of p_sys */
#ifdef ENABLE_PACKETIZER
    p_sys->b_packetizer = b_packetizer;
#endif
    p_sys->b_ready = false;
    p_sys->i_pts =
    p_sys->i_max_stop = VLC_TICK_INVALID;

    kate_comment_init( &p_sys->kc );
    kate_info_init( &p_sys->ki );

    p_sys->b_has_headers = false;

    /* retrieve options */
    p_sys->b_formatted = var_CreateGetBool( p_dec, "kate-formatted" );

    vlc_mutex_lock( &kate_decoder_list_mutex );

#ifdef HAVE_TIGER

    p_sys->b_use_tiger = var_CreateGetBool( p_dec, "kate-use-tiger" );

    p_sys->p_tr = NULL;

    /* get initial value of configuration */
    p_sys->i_tiger_default_font_color = GetTigerColor( p_dec, "kate-tiger-default-font" );
    p_sys->i_tiger_default_background_color = GetTigerColor( p_dec, "kate-tiger-default-background" );
    p_sys->e_tiger_default_font_effect = GetTigerInteger( p_dec, "kate-tiger-default-font-effect" );
    p_sys->f_tiger_default_font_effect_strength = GetTigerFloat( p_dec, "kate-tiger-default-font-effect-strength" );
    p_sys->psz_tiger_default_font_desc = GetTigerString( p_dec, "kate-tiger-default-font-desc" );
    p_sys->f_tiger_quality = GetTigerFloat( p_dec, "kate-tiger-quality" );

    if( p_sys->b_use_tiger )
    {
        int i_ret = tiger_renderer_create( &p_sys->p_tr );
        if( i_ret < 0 )
        {
            msg_Warn ( p_dec, "Failed to create Tiger renderer, falling back to basic rendering" );
            p_sys->p_tr = NULL;
            p_sys->b_use_tiger = false;
        }
        else {
            CHECK_TIGER_RET( tiger_renderer_set_surface_clear_color( p_sys->p_tr, 1, 0, 0, 0, 0 ) );

            UpdateTigerFontEffect( p_dec );
            UpdateTigerFontColor( p_dec );
            UpdateTigerBackgroundColor( p_dec );
            UpdateTigerQuality( p_dec );
            UpdateTigerFontDesc( p_dec );
        }
    }

#else

    p_sys->b_use_tiger = false;

#endif

    if( b_packetizer )
        p_dec->fmt_out.i_codec = VLC_CODEC_KATE;
    else
        p_dec->fmt_out.i_codec = 0; // may vary during the stream

    /* add the decoder to the global list */
    decoder_t **list = realloc( kate_decoder_list, (kate_decoder_list_size+1) * sizeof( *list ));
    if( list )
    {
        list[ kate_decoder_list_size++ ] = p_dec;
        kate_decoder_list = list;
    }

    vlc_mutex_unlock( &kate_decoder_list_mutex );

    return VLC_SUCCESS;
}

static int OpenDecoder( vlc_object_t *p_this )
{
    return OpenCommon( p_this, false );
}

#ifdef ENABLE_PACKETIZER
static int OpenPacketizer( vlc_object_t *p_this )
{
    return OpenCommon( p_this, true );
}
#endif

/*****************************************************************************
 * Flush:
 *****************************************************************************/
static void Flush( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

#ifdef HAVE_TIGER
    /* Hmm, should we wait before flushing the renderer ? I think not, but not certain... */
    vlc_mutex_lock( &p_sys->lock );
    tiger_renderer_seek( p_sys->p_tr, 0 );
    vlc_mutex_unlock( &p_sys->lock );
#endif
    p_sys->i_max_stop = VLC_TICK_INVALID;
}

/****************************************************************************
 * DecodeBlock: the whole thing
 ****************************************************************************
 * This function must be fed with kate packets.
 ****************************************************************************/
static void *DecodeBlock( decoder_t *p_dec, block_t *p_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    kate_packet kp;

    if( p_block->i_flags & (BLOCK_FLAG_DISCONTINUITY|BLOCK_FLAG_CORRUPTED) )
    {
#ifdef HAVE_TIGER
        if( p_block->i_flags & BLOCK_FLAG_DISCONTINUITY)
        {
            /* Hmm, should we wait before flushing the renderer ? I think not, but not certain... */
            vlc_mutex_lock( &p_sys->lock );
            tiger_renderer_seek( p_sys->p_tr, 0 );
            vlc_mutex_unlock( &p_sys->lock );
        }
#endif
        if( p_block->i_flags & BLOCK_FLAG_CORRUPTED )
        {
            p_sys->i_max_stop = VLC_TICK_INVALID;
            block_Release( p_block );
            return NULL;
        }
    }

    /* Block to Kate packet */
    kate_packet_wrap(&kp, p_block->i_buffer, p_block->p_buffer);

    if( !p_sys->b_has_headers )
    {
        if( ProcessHeaders( p_dec ) )
        {
            block_Release( p_block );
            return NULL;
        }
        p_sys->b_has_headers = true;
    }

    return ProcessPacket( p_dec, &kp, p_block );
}

static int DecodeSub( decoder_t *p_dec, block_t *p_block )
{
    if( p_block == NULL ) /* No Drain */
        return VLCDEC_SUCCESS;

    subpicture_t *p_spu = DecodeBlock( p_dec, p_block );
    if( p_spu != NULL )
        decoder_QueueSub( p_dec, p_spu );
    return VLCDEC_SUCCESS;
}

static block_t *Packetize( decoder_t *p_dec, block_t **pp_block )
{
    if( pp_block == NULL ) /* No Drain */
        return NULL;
    block_t *p_block = *pp_block; *pp_block = NULL;
    if( p_block == NULL )
        return NULL;
    return DecodeBlock( p_dec, p_block );
}

/*****************************************************************************
 * ProcessHeaders: process Kate headers.
 *****************************************************************************/
static int ProcessHeaders( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    kate_packet kp;

    unsigned pi_size[XIPH_MAX_HEADER_COUNT];
    const void *pp_data[XIPH_MAX_HEADER_COUNT];
    unsigned i_count;
    if( xiph_SplitHeaders( pi_size, pp_data, &i_count,
                           p_dec->fmt_in.i_extra, p_dec->fmt_in.p_extra) )
        return VLC_EGENERIC;

    if( i_count < 1 )
        return VLC_EGENERIC;

    /* Take care of the initial Kate header */
    kp.nbytes = pi_size[0];
    kp.data   = (void *)pp_data[0];
    int i_ret = kate_decode_headerin( &p_sys->ki, &p_sys->kc, &kp );
    if( i_ret < 0 )
    {
        msg_Err( p_dec, "this bitstream does not contain Kate data (%d)", i_ret );
        return VLC_EGENERIC;
    }

    msg_Dbg( p_dec, "%s %s text, granule rate %f, granule shift %d",
             p_sys->ki.language, p_sys->ki.category,
             (double)p_sys->ki.gps_numerator/p_sys->ki.gps_denominator,
             p_sys->ki.granule_shift);

    /* parse all remaining header packets */
    for( unsigned i_headeridx = 1; i_headeridx < i_count; i_headeridx++ )
    {
        kp.nbytes = pi_size[i_headeridx];
        kp.data   = (void *)pp_data[i_headeridx];
        i_ret = kate_decode_headerin( &p_sys->ki, &p_sys->kc, &kp );
        if( i_ret < 0 )
        {
            msg_Err( p_dec, "Kate header %d is corrupted: %d", i_headeridx, i_ret );
            return VLC_EGENERIC;
        }

        /* header 1 is comments */
        if( i_headeridx == 1 )
        {
            ParseKateComments( p_dec );
        }
    }

#ifdef ENABLE_PACKETIZER
    if( !p_sys->b_packetizer )
#endif
    {
        /* We have all the headers, initialize decoder */
        msg_Dbg( p_dec, "we have all headers, initialize libkate for decoding" );
        i_ret = kate_decode_init( &p_sys->k, &p_sys->ki );
        if (i_ret < 0)
        {
            msg_Err( p_dec, "Kate failed to initialize for decoding: %d", i_ret );
            return VLC_EGENERIC;
        }
        p_sys->b_ready = true;
    }
#ifdef ENABLE_PACKETIZER
    else
    {
        void* p_extra = realloc( p_dec->fmt_out.p_extra,
                                 p_dec->fmt_in.i_extra );
        if( unlikely( p_extra == NULL ) )
        {
            return VLC_ENOMEM;
        }
        p_dec->fmt_out.p_extra = p_extra;
        p_dec->fmt_out.i_extra = p_dec->fmt_in.i_extra;
        memcpy( p_dec->fmt_out.p_extra,
                p_dec->fmt_in.p_extra, p_dec->fmt_out.i_extra );
    }
#endif

    return VLC_SUCCESS;
}

/*****************************************************************************
 * ProcessPacket: processes a kate packet.
 *****************************************************************************/
static void *ProcessPacket( decoder_t *p_dec, kate_packet *p_kp,
                            block_t *p_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    /* Date management */
    if( p_block->i_pts != VLC_TICK_INVALID && p_block->i_pts != p_sys->i_pts )
    {
        p_sys->i_pts = p_block->i_pts;
    }

#ifdef ENABLE_PACKETIZER
    if( p_sys->b_packetizer )
    {
        /* Date management */
        p_block->i_dts = p_block->i_pts = p_sys->i_pts;

        if( p_sys->i_headers >= p_sys->i_num_headers )
            p_block->i_length = p_sys->i_pts - p_block->i_pts;
        else
            p_block->i_length = 0;
        return p_block;
    }
    else
#endif
    {
        subpicture_t *p_buf = DecodePacket( p_dec, p_kp, p_block );

        block_Release( p_block );
        return p_buf;
    }
}

/* nicked off blend.c */
static inline void rgb_to_yuv( uint8_t *y, uint8_t *u, uint8_t *v,
                               int r, int g, int b )
{
    *y = ( ( (  66 * r + 129 * g +  25 * b + 128 ) >> 8 ) + 16 );
    *u =   ( ( -38 * r -  74 * g + 112 * b + 128 ) >> 8 ) + 128 ;
    *v =   ( ( 112 * r -  94 * g -  18 * b + 128 ) >> 8 ) + 128 ;
}

/*
  This retrieves the size of the video.
  The best case is when the original video size is known, as we can then
  scale images to match. In this case, since VLC autoscales, we want to
  return the original size and let VLC scale everything.
  if the original size is not known, then VLC can't resize, so we return
  the size of the incoming video. If sizes in the Kate stream are in
  relative units, it works fine. If they are absolute, you get what you
  ask for. Images aren't rescaled.
*/
static void GetVideoSize( decoder_t *p_dec, int *w, int *h )
{
    /* searching for vout to get its size is frowned upon, so we don't and
       use a default size if the original canvas size is not specified. */
    decoder_sys_t *p_sys = p_dec->p_sys;
    if( p_sys->ki.original_canvas_width > 0 && p_sys->ki.original_canvas_height > 0 )
    {
        *w = p_sys->ki.original_canvas_width;
        *h = p_sys->ki.original_canvas_height;
        msg_Dbg( p_dec, "original canvas %zu %zu",
                 p_sys->ki.original_canvas_width, p_sys->ki.original_canvas_height );
    }
    else
    {
        /* nothing, leave defaults */
        msg_Dbg( p_dec, "original canvas size unknown");
    }
}

static void CreateKateBitmap( picture_t *pic, const kate_bitmap *bitmap )
{
    size_t y;

    for( y=0; y<bitmap->height; ++y )
    {
        uint8_t *dest = pic->Y_PIXELS+pic->Y_PITCH*y;
        const uint8_t *src = bitmap->pixels+y*bitmap->width;
        memcpy( dest, src, bitmap->width );
    }
}

static void CreateKatePalette( video_palette_t *fmt_palette, const kate_palette *palette )
{
    size_t n;

    fmt_palette->i_entries = palette->ncolors;
    for( n=0; n<palette->ncolors; ++n )
    {
        rgb_to_yuv(
            &fmt_palette->palette[n][0], &fmt_palette->palette[n][1], &fmt_palette->palette[n][2],
            palette->colors[n].r, palette->colors[n].g, palette->colors[n].b
        );
        fmt_palette->palette[n][3] = palette->colors[n].a;
    }
}

static void SetupText( decoder_t *p_dec, subpicture_t *p_spu, const kate_event *ev )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( ev->text_encoding != kate_utf8 )
    {
        msg_Warn( p_dec, "Text isn't UTF-8, unsupported, ignored" );
        return;
    }

    switch( ev->text_markup_type )
    {
        case kate_markup_none:
        case kate_markup_simple:
            if( p_sys->b_formatted )
//                p_spu->p_region->p_text = ParseSubtitles(&p_spu->p_region->i_align, ev->text );
                ;//FIXME
            else
                p_spu->p_region->p_text = text_segment_New( ev->text ); /* no leak, this actually gets killed by the core */
            break;
        default:
            /* we don't know about this one, so remove markup and display as text */
            {
                char *copy = strdup( ev->text );
                size_t len0 = strlen( copy ) + 1;
                kate_text_remove_markup( ev->text_encoding, copy, &len0 );
                p_spu->p_region->p_text = text_segment_New( copy );
                free( copy );
            }
            break;
    }
}

#ifdef HAVE_TIGER

static void TigerDestroySubpicture( subpicture_t *p_subpic )
{
    kate_spu_updater_sys_t *p_spusys = p_subpic->updater.p_sys;
    DecSysRelease( p_spusys->p_dec_sys );
    free( p_spusys );
}
/*
 * We get premultiplied alpha, but VLC doesn't expect this, so we demultiply
 * alpha to avoid double multiply (and thus thinner text than we should)).
 * Best would be to have VLC be able to handle premultiplied alpha, as it
 * would be faster to blend as well.
 *
 * Also, we flip color components around for big endian machines, as Tiger
 * outputs ARGB or ABGR (the one we selected here) in host endianness.
 */
static void PostprocessTigerImage( plane_t *p_plane, unsigned int i_width )
{
    PROFILE_START( tiger_renderer_postprocess );
    int y;
    for( y=0; y<p_plane->i_lines; ++y )
    {
        uint8_t *p_line = (uint8_t*)(p_plane->p_pixels + y*p_plane->i_pitch);
        unsigned int x;
        for( x=0; x<i_width; ++x )
        {
            uint8_t *p_pixel = p_line+x*4;
#ifdef WORDS_BIGENDIAN
            uint8_t a = p_pixel[0];
#else
            uint8_t a = p_pixel[3];
#endif
            if( a )
            {
#ifdef WORDS_BIGENDIAN
                uint8_t tmp = p_pixel[2];
                p_pixel[0] = clip_uint8_vlc((p_pixel[3] * 255 + a / 2) / a);
                p_pixel[3] = a;
                p_pixel[2] = clip_uint8_vlc((p_pixel[1] * 255 + a / 2) / a);
                p_pixel[1] = clip_uint8_vlc((tmp * 255 + a / 2) / a);
#else
                p_pixel[0] = clip_uint8_vlc((p_pixel[0] * 255 + a / 2) / a);
                p_pixel[1] = clip_uint8_vlc((p_pixel[1] * 255 + a / 2) / a);
                p_pixel[2] = clip_uint8_vlc((p_pixel[2] * 255 + a / 2) / a);
#endif
            }
            else
            {
                p_pixel[0] = 0;
                p_pixel[1] = 0;
                p_pixel[2] = 0;
                p_pixel[3] = 0;
            }
        }
    }
    PROFILE_STOP( tiger_renderer_postprocess );
}

static int TigerValidateSubpicture( subpicture_t *p_subpic,
                                    bool b_fmt_src, const video_format_t *p_fmt_src,
                                    bool b_fmt_dst, const video_format_t *p_fmt_dst,
                                    vlc_tick_t ts )
{
    VLC_UNUSED(p_fmt_src); VLC_UNUSED(p_fmt_dst);

    kate_spu_updater_sys_t *p_spusys = p_subpic->updater.p_sys;
    decoder_sys_t *p_sys = p_spusys->p_dec_sys;

    if( b_fmt_src || b_fmt_dst )
        return VLC_EGENERIC;

    PROFILE_START( TigerValidateSubpicture );

    /* time in seconds from the start of the stream */
    kate_float t = (p_spusys->i_start + ts - p_subpic->i_start ) / 1000000.0f;

    /* it is likely that the current region (if any) can be kept as is; test for this */
    vlc_mutex_lock( &p_sys->lock );
    int i_ret;
    if( p_sys->b_dirty || tiger_renderer_is_dirty( p_sys->p_tr ) )
    {
        i_ret = VLC_EGENERIC;
        goto exit;
    }
    if( tiger_renderer_update( p_sys->p_tr, t, 1 ) >= 0 &&
        tiger_renderer_is_dirty( p_sys->p_tr ) )
    {
        i_ret = VLC_EGENERIC;
        goto exit;
    }

    i_ret = VLC_SUCCESS;
exit:
    vlc_mutex_unlock( &p_sys->lock );
    PROFILE_STOP( TigerValidateSubpicture );
    return i_ret;
}

/* Tiger renders can end up looking a bit crap since they get overlaid on top of
   a subsampled YUV image, so there can be a fair amount of chroma bleeding.
   Looks good with white though since it's all luma. Hopefully that will be the
   common case. */
static void TigerUpdateSubpicture( subpicture_t *p_subpic,
                                   const video_format_t *p_fmt_src,
                                   const video_format_t *p_fmt_dst,
                                   vlc_tick_t ts )
{
    kate_spu_updater_sys_t *p_spusys = p_subpic->updater.p_sys;
    decoder_sys_t *p_sys = p_spusys->p_dec_sys;
    plane_t *p_plane;
    kate_float t;
    int i_ret;


    /* time in seconds from the start of the stream */
    t = (p_spusys->i_start + ts - p_subpic->i_start ) / 1000000.0f;

    PROFILE_START( TigerUpdateSubpicture );

    /* create a full frame region - this will also tell Tiger the size of the frame */
    video_format_t fmt = *p_fmt_dst;
    fmt.i_chroma         = VLC_CODEC_RGBA;
    fmt.i_bits_per_pixel = 0;
    fmt.i_width          =
    fmt.i_visible_width  = p_fmt_src->i_width;
    fmt.i_height         =
    fmt.i_visible_height = p_fmt_src->i_height;
    fmt.i_x_offset       = fmt.i_y_offset = 0;

    subpicture_region_t *p_r = subpicture_region_New( &fmt );
    if( !p_r )
        return;

    p_r->i_x = 0;
    p_r->i_y = 0;
    p_r->i_align = SUBPICTURE_ALIGN_TOP | SUBPICTURE_ALIGN_LEFT;

    vlc_mutex_lock( &p_sys->lock );

    p_plane = &p_r->p_picture->p[0];
    i_ret = tiger_renderer_set_buffer( p_sys->p_tr, p_plane->p_pixels, fmt.i_width, p_plane->i_lines, p_plane->i_pitch, 1 );
    if( i_ret < 0 )
    {
        goto failure;
    }

    PROFILE_START( tiger_renderer_update );
    i_ret = tiger_renderer_update( p_sys->p_tr, t, 1 );
    if( i_ret < 0 )
    {
        goto failure;
    }
    PROFILE_STOP( tiger_renderer_update );

    PROFILE_START( tiger_renderer_render );
    i_ret = tiger_renderer_render( p_sys->p_tr );
    if( i_ret < 0 )
    {
        goto failure;
    }
    PROFILE_STOP( tiger_renderer_render );

    PostprocessTigerImage( p_plane, fmt.i_width );
    p_subpic->p_region = p_r;
    p_sys->b_dirty = false;

    PROFILE_STOP( TigerUpdateSubpicture );

    vlc_mutex_unlock( &p_sys->lock );

    return;

failure:
    vlc_mutex_unlock( &p_sys->lock );
    subpicture_region_ChainDelete( p_r );
}

static uint32_t GetTigerColor( decoder_t *p_dec, const char *psz_prefix )
{
    char *psz_tmp;
    uint32_t i_color = 0;

    if( asprintf( &psz_tmp, "%s-color", psz_prefix ) >= 0 )
    {
        uint32_t i_rgb = var_CreateGetInteger( p_dec, psz_tmp );
        var_Destroy( p_dec, psz_tmp );
        free( psz_tmp );
        i_color |= i_rgb;
    }

    if( asprintf( &psz_tmp, "%s-alpha", psz_prefix ) >= 0 )
    {
        uint32_t i_alpha = var_CreateGetInteger( p_dec, psz_tmp );
        var_Destroy( p_dec, psz_tmp );
        free( psz_tmp );
        i_color |= (i_alpha << 24);
    }

    return i_color;
}

static char *GetTigerString( decoder_t *p_dec, const char *psz_name )
{
    char *psz_value = var_CreateGetString( p_dec, psz_name );
    if( psz_value)
    {
        psz_value = strdup( psz_value );
    }
    var_Destroy( p_dec, psz_name );
    return psz_value;
}

static int GetTigerInteger( decoder_t *p_dec, const char *psz_name )
{
    int i_value = var_CreateGetInteger( p_dec, psz_name );
    var_Destroy( p_dec, psz_name );
    return i_value;
}

static double GetTigerFloat( decoder_t *p_dec, const char *psz_name )
{
    double f_value = var_CreateGetFloat( p_dec, psz_name );
    var_Destroy( p_dec, psz_name );
    return f_value;
}

static void UpdateTigerQuality( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = (decoder_sys_t*)p_dec->p_sys;
    CHECK_TIGER_RET( tiger_renderer_set_quality( p_sys->p_tr, p_sys->f_tiger_quality ) );
    p_sys->b_dirty = true;
}

static void UpdateTigerFontDesc( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = (decoder_sys_t*)p_dec->p_sys;
    CHECK_TIGER_RET( tiger_renderer_set_default_font_description( p_sys->p_tr, p_sys->psz_tiger_default_font_desc ) );
    p_sys->b_dirty = true;
}

static void UpdateTigerFontColor( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = (decoder_sys_t*)p_dec->p_sys;
    double f_a = ((p_sys->i_tiger_default_font_color >> 24) & 0xff) / 255.0;
    double f_r = ((p_sys->i_tiger_default_font_color >> 16) & 0xff) / 255.0;
    double f_g = ((p_sys->i_tiger_default_font_color >> 8) & 0xff) / 255.0;
    double f_b = (p_sys->i_tiger_default_font_color & 0xff) / 255.0;
    CHECK_TIGER_RET( tiger_renderer_set_default_font_color( p_sys->p_tr, f_r, f_g, f_b, f_a ) );
    p_sys->b_dirty = true;
}

static void UpdateTigerBackgroundColor( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = (decoder_sys_t*)p_dec->p_sys;
    double f_a = ((p_sys->i_tiger_default_background_color >> 24) & 0xff) / 255.0;
    double f_r = ((p_sys->i_tiger_default_background_color >> 16) & 0xff) / 255.0;
    double f_g = ((p_sys->i_tiger_default_background_color >> 8) & 0xff) / 255.0;
    double f_b = (p_sys->i_tiger_default_background_color & 0xff) / 255.0;
    CHECK_TIGER_RET( tiger_renderer_set_default_background_fill_color( p_sys->p_tr, f_r, f_g, f_b, f_a ) );
    p_sys->b_dirty = true;
}

static void UpdateTigerFontEffect( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = (decoder_sys_t*)p_dec->p_sys;
    CHECK_TIGER_RET( tiger_renderer_set_default_font_effect( p_sys->p_tr,
                                                             p_sys->e_tiger_default_font_effect,
                                                             p_sys->f_tiger_default_font_effect_strength ) );
    p_sys->b_dirty = true;
}

#endif

/*****************************************************************************
 * DecodePacket: decodes a Kate packet.
 *****************************************************************************/
static subpicture_t *DecodePacket( decoder_t *p_dec, kate_packet *p_kp, block_t *p_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    const kate_event *ev = NULL;
    subpicture_t *p_spu = NULL;
    int i_ret;

    if( !p_sys->b_ready )
    {
        msg_Err( p_dec, "Cannot decode Kate packet, decoder not initialized" );
        return NULL;
    }

    i_ret = kate_decode_packetin( &p_sys->k, p_kp );
    if( i_ret < 0 )
    {
        msg_Err( p_dec, "Kate failed to decode packet: %d", i_ret );
        return NULL;
    }

    i_ret = kate_decode_eventout( &p_sys->k, &ev );
    if( i_ret < 0 )
    {
        msg_Err( p_dec, "Kate failed to retrieve event: %d", i_ret );
        return NULL;
    }
    if( i_ret > 0 )
    {
        /* no event to go with this packet, this is normal */
        return NULL;
    }

    /* we have an event */

    /* Get a new spu */
    kate_spu_updater_sys_t *p_spu_sys = NULL;
    if( p_sys->b_use_tiger)
    {
        p_spu_sys = malloc( sizeof(*p_spu_sys) );
        if( !p_spu_sys )
            return NULL;
    }
    subpicture_updater_t updater = {
#ifdef HAVE_TIGER
        .pf_validate = TigerValidateSubpicture,
        .pf_update   = TigerUpdateSubpicture,
        .pf_destroy  = TigerDestroySubpicture,
#endif
        .p_sys       = p_spu_sys,
    };
    p_spu = decoder_NewSubpicture( p_dec, p_sys->b_use_tiger ? &updater : NULL );
    if( !p_spu )
    {
        free( p_spu_sys );
        /* this will happen for lyrics as there is no vout - so no error */
        /* msg_Err( p_dec, "Failed to allocate spu buffer" ); */
        return NULL;
    }

    p_spu->i_start = p_block->i_pts;
    p_spu->i_stop = p_block->i_pts + vlc_tick_from_samples(ev->duration * p_sys->ki.gps_denominator, p_sys->ki.gps_numerator);
    p_spu->b_ephemer = false;
    p_spu->b_absolute = false;

#ifdef HAVE_TIGER
    if( p_sys->b_use_tiger)
    {
        p_spu_sys->p_dec_sys = p_sys;
        p_spu_sys->i_start   = p_block->i_pts;
        DecSysHold( p_sys );

        p_spu->i_stop = __MAX( p_sys->i_max_stop, p_spu->i_stop );
        p_spu->b_ephemer = true;
        p_spu->b_absolute = true;

        /* add the event to tiger */
        vlc_mutex_lock( &p_sys->lock );
        CHECK_TIGER_RET( tiger_renderer_add_event( p_sys->p_tr, ev->ki, ev ) );
        vlc_mutex_unlock( &p_sys->lock );
    }
    else
#endif
    {
        p_spu = SetupSimpleKateSPU( p_dec, p_spu, ev );
    }

    return p_spu;
}

/*****************************************************************************
 * SetupSimpleKateSPU: creates text/bitmap regions where appropriate
 *****************************************************************************/
static subpicture_t *SetupSimpleKateSPU( decoder_t *p_dec, subpicture_t *p_spu,
                                         const kate_event *ev )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    video_format_t fmt;
    subpicture_region_t *p_bitmap_region = NULL;
    video_palette_t palette;
    kate_tracker kin;
    bool b_tracker_valid = false;
    int i_ret;

    /* these may be 0 for "not specified" */
    p_spu->i_original_picture_width = p_sys->ki.original_canvas_width;
    p_spu->i_original_picture_height = p_sys->ki.original_canvas_height;

    /* Create a new subpicture region */
    if (p_sys->b_formatted)
    {
        i_ret = kate_tracker_init( &kin, &p_sys->ki, ev );
        if( i_ret < 0)
        {
            msg_Err( p_dec, "failed to initialize kate tracker, event will be unformatted: %d", i_ret );
        }
        else
        {
            int w = 720, h = 576; /* give sensible defaults just in case we fail to get the actual size */
            GetVideoSize(p_dec, &w, &h);
            i_ret = kate_tracker_update(&kin, 0, w, h, 0, 0, w, h);
            if( i_ret < 0)
            {
                kate_tracker_clear(&kin);
                msg_Err( p_dec, "failed to update kate tracker, event will be unformatted: %d", i_ret );
            }
            else
            {
                // TODO: parse tracker and set style, init fmt
                b_tracker_valid = true;
            }
        }
    }

    if (ev->bitmap && ev->bitmap->type==kate_bitmap_type_paletted && ev->palette) {

        /* create a separate region for the bitmap */
        video_format_Init( &fmt, VLC_CODEC_YUVP );
        fmt.i_width = fmt.i_visible_width = ev->bitmap->width;
        fmt.i_height = fmt.i_visible_height = ev->bitmap->height;
        fmt.i_x_offset = fmt.i_y_offset = 0;
        fmt.p_palette = &palette;
        CreateKatePalette( fmt.p_palette, ev->palette );

        p_bitmap_region = subpicture_region_New( &fmt );
        video_format_Clean( &fmt );
        if( !p_bitmap_region )
        {
            msg_Err( p_dec, "cannot allocate SPU region" );
            subpicture_Delete( p_spu );
            return NULL;
        }

        /* create the bitmap */
        CreateKateBitmap( p_bitmap_region->p_picture, ev->bitmap );

        msg_Dbg(p_dec, "Created bitmap, %zux%zu, %zu colors", ev->bitmap->width, ev->bitmap->height, ev->palette->ncolors);
    }

    /* text region */
    video_format_Init( &fmt, VLC_CODEC_TEXT );
    fmt.i_sar_num = 0;
    fmt.i_sar_den = 1;
    fmt.i_width = fmt.i_height = 0;
    fmt.i_x_offset = fmt.i_y_offset = 0;
    p_spu->p_region = subpicture_region_New( &fmt );
    video_format_Clean( &fmt );
    if( !p_spu->p_region )
    {
        msg_Err( p_dec, "cannot allocate SPU region" );
        if( p_bitmap_region )
            subpicture_region_Delete( p_bitmap_region );
        subpicture_Delete( p_spu );
        return NULL;
    }

    SetupText( p_dec, p_spu, ev );

    /* default positioning */
    p_spu->p_region->i_align = SUBPICTURE_ALIGN_BOTTOM;
    if (p_bitmap_region)
    {
        p_bitmap_region->i_align = SUBPICTURE_ALIGN_BOTTOM;
    }
    p_spu->p_region->i_x = 0;
    p_spu->p_region->i_y = 10;

    /* override if tracker info present */
    if (b_tracker_valid)
    {
        if (kin.has.region)
        {
            p_spu->p_region->i_x = kin.region_x;
            p_spu->p_region->i_y = kin.region_y;
            if (p_bitmap_region)
            {
                p_bitmap_region->i_x = kin.region_x;
                p_bitmap_region->i_y = kin.region_y;
            }
            p_spu->b_absolute = true;
        }

        kate_tracker_clear(&kin);
    }

    /* if we have a bitmap, chain it before the text */
    if (p_bitmap_region)
    {
        p_bitmap_region->p_next = p_spu->p_region;
        p_spu->p_region = p_bitmap_region;
    }

    return p_spu;
}

/*****************************************************************************
 * ParseKateComments:
 *****************************************************************************/
static void ParseKateComments( decoder_t *p_dec )
{
    char *psz_name, *psz_value, *psz_comment;
    int i = 0;

    decoder_sys_t *p_sys = p_dec->p_sys;

    while ( i < p_sys->kc.comments )
    {
        psz_comment = strdup( p_sys->kc.user_comments[i] );
        if( !psz_comment )
            break;
        psz_name = psz_comment;
        psz_value = strchr( psz_comment, '=' );
        if( psz_value )
        {
            *psz_value = '\0';
            psz_value++;

            if( !p_dec->p_description )
                p_dec->p_description = vlc_meta_New();
            if( p_dec->p_description )
                vlc_meta_AddExtra( p_dec->p_description, psz_name, psz_value );
        }
        free( psz_comment );
        i++;
    }
}

/*****************************************************************************
 * CloseDecoder: clean up the decoder
 *****************************************************************************/
static void CloseDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t *)p_this;
    size_t     i_index;

    /* remove the decoder from the global list */
    vlc_mutex_lock( &kate_decoder_list_mutex );
    for( i_index = 0; i_index < kate_decoder_list_size; i_index++ )
    {
        if( kate_decoder_list[ i_index ] == p_dec )
        {
            kate_decoder_list[ i_index ] = kate_decoder_list[ --kate_decoder_list_size ];
            break;
        }
    }
    vlc_mutex_unlock( &kate_decoder_list_mutex );

    msg_Dbg( p_dec, "Closing Kate decoder" );
    DecSysRelease( p_dec->p_sys );
}

static void DecSysHold( decoder_sys_t *p_sys )
{
    vlc_mutex_lock( &p_sys->lock );
    p_sys->i_refcount++;
    vlc_mutex_unlock( &p_sys->lock );
}

static void DecSysRelease( decoder_sys_t *p_sys )
{
    vlc_mutex_lock( &p_sys->lock );
    p_sys->i_refcount--;
    if( p_sys->i_refcount > 0)
    {
        vlc_mutex_unlock( &p_sys->lock );
        return;
    }

    vlc_mutex_unlock( &p_sys->lock );

#ifdef HAVE_TIGER
    if( p_sys->p_tr )
        tiger_renderer_destroy( p_sys->p_tr );
    free( p_sys->psz_tiger_default_font_desc );
#endif

    if (p_sys->b_ready)
        kate_clear( &p_sys->k );
    kate_info_clear( &p_sys->ki );
    kate_comment_clear( &p_sys->kc );

    free( p_sys );
}

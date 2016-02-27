/*****************************************************************************
 * delay.c: delay a stream
 *****************************************************************************
 * Copyright © 2014 VLC authors and VideoLAN
 *
 * Authors: Ilkka Ollakka <ileoo at videolan dot org>
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
#include <vlc_sout.h>
#include <vlc_block.h>
#include <vlc_md5.h>
#include <vlc_fs.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define OUTPUT_TEXT N_("Output file")
#define OUTPUT_LONGTEXT N_( \
    "Writes stats to file instead of stdout" )
#define PREFIX_TEXT N_("Prefix to show on output line")

static int  Open    ( vlc_object_t * );
static void Close   ( vlc_object_t * );

#define SOUT_CFG_PREFIX "sout-stats-"

vlc_module_begin()
    set_shortname( N_("Stats"))
    set_description( N_("Writes statistic info about stream"))
    set_capability( "sout stream", 0 )
    add_shortcut( "stats" )
    set_category( CAT_SOUT )
    set_subcategory( SUBCAT_SOUT_STREAM )
    set_callbacks( Open, Close )
    add_string( SOUT_CFG_PREFIX "output", "", OUTPUT_TEXT,OUTPUT_LONGTEXT, false );
    add_string( SOUT_CFG_PREFIX "prefix", "stats", PREFIX_TEXT,PREFIX_TEXT, false );
vlc_module_end()


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static const char *ppsz_sout_options[] = {
    "output", "prefix", NULL
};

static sout_stream_id_sys_t *Add( sout_stream_t *, const es_format_t * );
static void               Del   ( sout_stream_t *, sout_stream_id_sys_t * );
static int               Send  ( sout_stream_t *, sout_stream_id_sys_t *, block_t * );

struct sout_stream_sys_t
{
    FILE *output;
    char *prefix;
};

struct sout_stream_id_sys_t
{
    int id;
    uint64_t segment_number;
    void *next_id;
    const char *type;
    mtime_t previous_dts,track_duration;
    struct md5_s hash;
};

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_stream_t     *p_stream = (sout_stream_t*)p_this;
    sout_stream_sys_t *p_sys;
    char              *outputFile;

    p_sys = calloc( 1, sizeof( sout_stream_sys_t ) );
    if( !p_sys )
        return VLC_ENOMEM;


    config_ChainParse( p_stream, SOUT_CFG_PREFIX, ppsz_sout_options,
                   p_stream->p_cfg );


    outputFile = var_InheritString( p_stream, SOUT_CFG_PREFIX "output" );

    if( outputFile )
    {
        p_sys->output = vlc_fopen( outputFile, "wt" );
        if( !p_sys->output )
        {
            msg_Err( p_stream, "Unable to open file '%s' for writing", outputFile );
            free( p_sys );
            free( outputFile );
            return VLC_EGENERIC;
        } else {
            fprintf( p_sys->output,"#prefix\ttrack\ttype\tsegment_number\tdts_difference\tlength\tmd5\n");
        }
        free( outputFile );
    }
    p_sys->prefix = var_InheritString( p_stream, SOUT_CFG_PREFIX "prefix" );

    p_stream->p_sys     = p_sys;

    p_stream->pf_add    = Add;
    p_stream->pf_del    = Del;
    p_stream->pf_send   = Send;


    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    sout_stream_t     *p_stream = (sout_stream_t*)p_this;
    sout_stream_sys_t *p_sys = (sout_stream_sys_t *)p_stream->p_sys;

    if( p_sys->output )
        fclose( p_sys->output );

    free( p_sys->prefix );
    free( p_sys );
}

static sout_stream_id_sys_t * Add( sout_stream_t *p_stream, const es_format_t *p_fmt )
{
    sout_stream_sys_t *p_sys = (sout_stream_sys_t *)p_stream->p_sys;
    sout_stream_id_sys_t *id;

    id = malloc( sizeof( sout_stream_id_sys_t ) );
    if( unlikely( !id ) )
        return NULL;

    id->id = p_fmt->i_id;
    switch( p_fmt->i_cat)
    {
    case VIDEO_ES:
        id->type = "Video";
        break;
    case AUDIO_ES:
        id->type = "Audio";
        break;
    case SPU_ES:
        id->type = "SPU";
        break;
    default:
        id->type = "Data";
        break;
    }
    id->next_id = NULL;
    id->segment_number = 0;
    id->previous_dts = VLC_TS_INVALID;
    id->track_duration = 0;
    InitMD5( &id->hash );

    msg_Dbg( p_stream, "%s: Adding track type:%s id:%d", p_sys->prefix, id->type, id->id);

    if( p_stream->p_next )
        id->next_id = sout_StreamIdAdd( p_stream->p_next, p_fmt );

    return id;
}

static void Del( sout_stream_t *p_stream, sout_stream_id_sys_t *id )
{
    sout_stream_sys_t *p_sys = (sout_stream_sys_t *)p_stream->p_sys;

    EndMD5( &id->hash );
    char *outputhash = psz_md5_hash( &id->hash );
    unsigned int num,den;
    vlc_ureduce( &num, &den, id->track_duration, id->segment_number, 0 );
    msg_Dbg( p_stream, "%s: Removing track type:%s id:%d", p_sys->prefix, id->type, id->id );
    if( p_sys->output )
    {
        fprintf( p_sys->output,"#%s: final type:%s id:%d segments:%"PRIu64" total_duration:%"PRId64" avg_track:%d/%d md5:%16s\n",
               p_sys->prefix, id->type, id->id, id->segment_number, id->track_duration, num, den, outputhash );
    } else {
        msg_Info( p_stream, "%s: final type:%s id:%d segments:%"PRIu64" total_duration:%"PRId64" avg_track:%d/%d md5:%16s",
               p_sys->prefix, id->type, id->id, id->segment_number, id->track_duration, num, den, outputhash );
    }
    free( outputhash );
    if( id->next_id ) sout_StreamIdDel( p_stream->p_next, id->next_id );
    free( id );
}

static int Send( sout_stream_t *p_stream, sout_stream_id_sys_t *id,
                 block_t *p_buffer )
{
    sout_stream_sys_t *p_sys = (sout_stream_sys_t *)p_stream->p_sys;
    struct md5_s hash;

    block_t *p_block = p_buffer;
    while ( p_block != NULL )
    {
        InitMD5( &hash );
        AddMD5( &hash, p_block->p_buffer, p_block->i_buffer );
        AddMD5( &id->hash, p_block->p_buffer, p_block->i_buffer );
        EndMD5( &hash );
        char *outputhash = psz_md5_hash( &hash );

        /* We could just set p_sys->output to stdout and remove user of msg_Dbg
         * if we don't need ability to output info to gui modules (like qt messages window
         */
        mtime_t dts_difference = VLC_TS_INVALID;
        if( likely( id->previous_dts != VLC_TS_INVALID ) )
            dts_difference = p_block->i_dts - id->previous_dts;
        if( p_sys->output )
        {
            /* Write data in a form that it's easy to plot for example with gnuplot*/
            fprintf( p_sys->output, "%s\t%d\t%s\t%"PRIu64"\t%"PRId64"\t%"PRId64"\t%16s\n",
                  p_sys->prefix, id->id, id->type, ++id->segment_number, dts_difference,
                  p_block->i_length, outputhash );

        } else {
            msg_Dbg( p_stream, "%s: track:%d type:%s segment_number:%"PRIu64" dts_difference:%"PRId64" length:%"PRId64" md5:%16s",
                  p_sys->prefix, id->id, id->type, ++id->segment_number, dts_difference,
                  p_block->i_length, outputhash );
        }
        id->track_duration += p_block->i_length ? p_block->i_length : dts_difference;
        free( outputhash );
        id->previous_dts = p_block->i_dts;
        p_block = p_block->p_next;
    }

    if( p_stream->p_next )
        return sout_StreamIdSend( p_stream->p_next, id->next_id, p_buffer );
    else
        block_Release( p_buffer );
    return VLC_SUCCESS;
}

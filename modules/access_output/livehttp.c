/*****************************************************************************
 * livehttp.c: Live HTTP Streaming
 *****************************************************************************
 * Copyright © 2001, 2002, 2013 VLC authors and VideoLAN
 * Copyright © 2009-2010 by Keary Griffin
 *
 * Authors: Keary Griffin <kearygriffin at gmail.com>
 *          Ilkka Ollakka <ileoo at videolan dot org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
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

#include <sys/types.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_sout.h>
#include <vlc_block.h>
#include <vlc_fs.h>
#include <vlc_strings.h>
#include <vlc_charset.h>

#include <gcrypt.h>
#include <vlc_gcrypt.h>

#ifndef O_LARGEFILE
#   define O_LARGEFILE 0
#endif

#define STR_ENDLIST "#EXT-X-ENDLIST\n"

#define MAX_RENAME_RETRIES        10

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

#define SOUT_CFG_PREFIX "sout-livehttp-"
#define SEGLEN_TEXT N_("Segment length")
#define SEGLEN_LONGTEXT N_("Length of TS stream segments")

#define SPLITANYWHERE_TEXT N_("Split segments anywhere")
#define SPLITANYWHERE_LONGTEXT N_("Don't require a keyframe before splitting "\
                                "a segment. Needed for audio only.")

#define NUMSEGS_TEXT N_("Number of segments")
#define NUMSEGS_LONGTEXT N_("Number of segments to include in index")

#define NOCACHE_TEXT N_("Allow cache")
#define NOCACHE_LONGTEXT N_("Add EXT-X-ALLOW-CACHE:NO directive in playlist-file if this is disabled")

#define INDEX_TEXT N_("Index file")
#define INDEX_LONGTEXT N_("Path to the index file to create")

#define INDEXURL_TEXT N_("Full URL to put in index file")
#define INDEXURL_LONGTEXT N_("Full URL to put in index file. "\
                          "Use #'s to represent segment number")

#define DELSEGS_TEXT N_("Delete segments")
#define DELSEGS_LONGTEXT N_("Delete segments when they are no longer needed")

#define RATECONTROL_TEXT N_("Use muxers rate control mechanism")

#define KEYURI_TEXT N_("AES key URI to place in playlist")
#define KEYURI_LONGTEXT N_("Location from where client will retrieve the stream decryption key")

#define KEYFILE_TEXT N_("AES key file")
#define KEYFILE_LONGTEXT N_("File containing the 16 bytes encryption key")

vlc_module_begin ()
    set_description( N_("HTTP Live streaming output") )
    set_shortname( N_("LiveHTTP" ))
    add_shortcut( "livehttp" )
    set_capability( "sout access", 0 )
    set_category( CAT_SOUT )
    set_subcategory( SUBCAT_SOUT_ACO )
    add_integer( SOUT_CFG_PREFIX "seglen", 10, SEGLEN_TEXT, SEGLEN_LONGTEXT, false )
    add_integer( SOUT_CFG_PREFIX "numsegs", 0, NUMSEGS_TEXT, NUMSEGS_LONGTEXT, false )
    add_bool( SOUT_CFG_PREFIX "splitanywhere", false,
              SPLITANYWHERE_TEXT, SPLITANYWHERE_LONGTEXT, true )
    add_bool( SOUT_CFG_PREFIX "delsegs", true,
              DELSEGS_TEXT, DELSEGS_LONGTEXT, true )
    add_bool( SOUT_CFG_PREFIX "ratecontrol", false,
              RATECONTROL_TEXT, RATECONTROL_TEXT, true )
    add_bool( SOUT_CFG_PREFIX "caching", false,
              NOCACHE_TEXT, NOCACHE_LONGTEXT, true )
    add_string( SOUT_CFG_PREFIX "index", NULL,
                INDEX_TEXT, INDEX_LONGTEXT, false )
    add_string( SOUT_CFG_PREFIX "index-url", NULL,
                INDEXURL_TEXT, INDEXURL_LONGTEXT, false )
    add_string( SOUT_CFG_PREFIX "key-uri", NULL,
                KEYURI_TEXT, KEYURI_TEXT, true )
    add_loadfile( SOUT_CFG_PREFIX "key-file", NULL,
                KEYFILE_TEXT, KEYFILE_LONGTEXT, true )
    set_callbacks( Open, Close )
vlc_module_end ()


/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static const char *const ppsz_sout_options[] = {
    "seglen",
    "splitanywhere",
    "numsegs",
    "delsegs",
    "index",
    "index-url",
    "ratecontrol",
    "caching",
    "key-uri",
    "key-file",
    NULL
};

static ssize_t Write( sout_access_out_t *, block_t * );
static int Seek ( sout_access_out_t *, off_t  );
static int Control( sout_access_out_t *, int, va_list );

struct sout_access_out_sys_t
{
    char *psz_cursegPath;
    char *psz_indexPath;
    char *psz_indexUrl;
    mtime_t i_opendts;
    mtime_t  i_seglenm;
    uint32_t i_segment;
    size_t  i_seglen;
    float   *p_seglens;
    block_t *block_buffer;
    int i_handle;
    unsigned i_numsegs;
    unsigned i_seglens;
    bool b_delsegs;
    bool b_ratecontrol;
    bool b_splitanywhere;
    bool b_caching;
    uint8_t aes_ivs[16];
    gcry_cipher_hd_t aes_ctx;
    char *key_uri;
};

static int CryptSetup( sout_access_out_t *p_access );
/*****************************************************************************
 * Open: open the file
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_access_out_t   *p_access = (sout_access_out_t*)p_this;
    sout_access_out_sys_t *p_sys;
    char *psz_idx;

    config_ChainParse( p_access, SOUT_CFG_PREFIX, ppsz_sout_options, p_access->p_cfg );

    if( !p_access->psz_path )
    {
        msg_Err( p_access, "no file name specified" );
        return VLC_EGENERIC;
    }

    if( unlikely( !( p_sys = malloc ( sizeof( *p_sys ) ) ) ) )
        return VLC_ENOMEM;

    p_sys->i_seglen = var_GetInteger( p_access, SOUT_CFG_PREFIX "seglen" );
    /* Try to get within asked segment length */
    p_sys->i_seglenm = CLOCK_FREQ * p_sys->i_seglen;
    p_sys->block_buffer = NULL;

    p_sys->i_numsegs = var_GetInteger( p_access, SOUT_CFG_PREFIX "numsegs" );
    p_sys->b_splitanywhere = var_GetBool( p_access, SOUT_CFG_PREFIX "splitanywhere" );
    p_sys->b_delsegs = var_GetBool( p_access, SOUT_CFG_PREFIX "delsegs" );
    p_sys->b_ratecontrol = var_GetBool( p_access, SOUT_CFG_PREFIX "ratecontrol") ;
    p_sys->b_caching = var_GetBool( p_access, SOUT_CFG_PREFIX "caching") ;


    /* 5 elements is from harrison-stetson algorithm to start from some number
     * if we don't have numsegs defined
     */
    p_sys->i_seglens = 5;
    if( p_sys->i_numsegs )
        p_sys->i_seglens = p_sys->i_numsegs+1;
    p_sys->p_seglens = malloc( sizeof(float) * p_sys->i_seglens  );
    if( unlikely( !p_sys->p_seglens ) )
    {
        free( p_sys );
        return VLC_ENOMEM;
    }

    p_sys->psz_indexPath = NULL;
    psz_idx = var_GetNonEmptyString( p_access, SOUT_CFG_PREFIX "index" );
    if ( psz_idx )
    {
        char *psz_tmp;
        psz_tmp = str_format_time( psz_idx );
        free( psz_idx );
        if ( !psz_tmp )
        {
            free( p_sys->p_seglens );
            free( p_sys );
            return VLC_ENOMEM;
        }
        path_sanitize( psz_tmp );
        p_sys->psz_indexPath = psz_tmp;
        vlc_unlink( p_sys->psz_indexPath );
    }

    p_sys->psz_indexUrl = var_GetNonEmptyString( p_access, SOUT_CFG_PREFIX "index-url" );

    p_access->p_sys = p_sys;

    if( CryptSetup( p_access ) < 0 )
    {
        free( p_sys->psz_indexUrl );
        free( p_sys->psz_indexPath );
        free( p_sys->p_seglens );
        free( p_sys );
        msg_Err( p_access, "Encryption init failed" );
        return VLC_EGENERIC;
    }

    p_sys->i_handle = -1;
    p_sys->i_segment = 0;
    p_sys->psz_cursegPath = NULL;

    p_access->pf_write = Write;
    p_access->pf_seek  = Seek;
    p_access->pf_control = Control;

    return VLC_SUCCESS;
}

/************************************************************************
 * CryptSetup: Initialize encryption
 ************************************************************************/
static int CryptSetup( sout_access_out_t *p_access )
{
    sout_access_out_sys_t *p_sys = p_access->p_sys;
    uint8_t key[16];

    p_sys->key_uri = var_GetNonEmptyString( p_access, SOUT_CFG_PREFIX "key-uri" );
    if( !p_sys->key_uri ) /*No key uri, assume no encryption wanted*/
    {
        msg_Dbg( p_access, "No key uri, no encryption");
        return VLC_SUCCESS;
    }
    vlc_gcrypt_init();

    /*Setup encryption cipher*/
    gcry_error_t err = gcry_cipher_open( &p_sys->aes_ctx, GCRY_CIPHER_AES,
                                         GCRY_CIPHER_MODE_CBC, 0 );
    if( err )
    {
        msg_Err( p_access, "Openin AES Cipher failed: %s", gpg_strerror(err));
        return VLC_EGENERIC;
    }

    char *keyfile = var_InheritString( p_access, SOUT_CFG_PREFIX "key-file" );
    if( unlikely(keyfile == NULL) )
    {
        msg_Err( p_access, "No key-file, no encryption" );
        return VLC_EGENERIC;
    }

    int keyfd = vlc_open( keyfile, O_RDONLY | O_NONBLOCK );
    if( unlikely( keyfd == -1 ) )
    {
        msg_Err( p_access, "Unable to open keyfile %s: %m", keyfile );
        free( keyfile );
        return VLC_EGENERIC;
    }
    free( keyfile );

    ssize_t keylen = read( keyfd, key, 16 );

    close( keyfd );
    if( keylen < 16 )
    {
        msg_Err( p_access, "No key at least 16 octects (you provided %zd), no encryption", keylen );
        return VLC_EGENERIC;
    }

    err = gcry_cipher_setkey( p_sys->aes_ctx, key, 16 );
    if(err)
    {
        msg_Err(p_access, "Setting AES key failed: %s", gpg_strerror(err));
        gcry_cipher_close( p_sys->aes_ctx);
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

/************************************************************************
 * CryptKey: Set encryption IV to current segment number
 ************************************************************************/
static int CryptKey( sout_access_out_t *p_access, uint32_t i_segment )
{
    sout_access_out_sys_t *p_sys = p_access->p_sys;
    memset( p_sys->aes_ivs, 0, 16 * sizeof(uint8_t));
    p_sys->aes_ivs[15] = i_segment & 0xff;
    p_sys->aes_ivs[14] = (i_segment >> 8 ) & 0xff;
    p_sys->aes_ivs[13] = (i_segment >> 16 ) & 0xff;
    p_sys->aes_ivs[12] = (i_segment >> 24 ) & 0xff;

    gcry_error_t err = gcry_cipher_setiv( p_sys->aes_ctx,
                                          p_sys->aes_ivs, 16);
    if( err )
    {
        msg_Err(p_access, "Setting AES IVs failed: %s", gpg_strerror(err) );
        gcry_cipher_close( p_sys->aes_ctx);
        return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}


#define SEG_NUMBER_PLACEHOLDER "#"
/*****************************************************************************
 * formatSegmentPath: create segment path name based on seg #
 *****************************************************************************/
static char *formatSegmentPath( char *psz_path, uint32_t i_seg, bool b_sanitize )
{
    char *psz_result;
    char *psz_firstNumSign;

    if ( ! ( psz_result  = str_format_time( psz_path ) ) )
        return NULL;

    psz_firstNumSign = psz_result + strcspn( psz_result, SEG_NUMBER_PLACEHOLDER );
    if ( *psz_firstNumSign )
    {
        char *psz_newResult;
        int i_cnt = strspn( psz_firstNumSign, SEG_NUMBER_PLACEHOLDER );
        int ret;

        *psz_firstNumSign = '\0';
        ret = asprintf( &psz_newResult, "%s%0*d%s", psz_result, i_cnt, i_seg, psz_firstNumSign + i_cnt );
        free ( psz_result );
        if ( ret < 0 )
            return NULL;
        psz_result = psz_newResult;
    }

    if ( b_sanitize )
        path_sanitize( psz_result );

    return psz_result;
}

/************************************************************************
 * updateIndexAndDel: If necessary, update index file & delete old segments
 ************************************************************************/
static int updateIndexAndDel( sout_access_out_t *p_access, sout_access_out_sys_t *p_sys, bool b_isend )
{

    uint32_t i_firstseg;

    if ( p_sys->i_numsegs == 0 || p_sys->i_segment < p_sys->i_numsegs )
    {
        i_firstseg = 1;

        if( p_sys->i_segment >= (p_sys->i_seglens-1) )
        {
            p_sys->i_seglens <<= 1;
            msg_Dbg( p_access, "Segment amount %u", p_sys->i_seglens );
            p_sys->p_seglens = realloc( p_sys->p_seglens, sizeof(float) * p_sys->i_seglens );
            if( unlikely( !p_sys->p_seglens ) )
             return -1;
        }
    }
    else
        i_firstseg = ( p_sys->i_segment - p_sys->i_numsegs ) + 1;

    // First update index
    if ( p_sys->psz_indexPath )
    {
        int val;
        FILE *fp;
        char *psz_idxTmp;
        if ( asprintf( &psz_idxTmp, "%s.tmp", p_sys->psz_indexPath ) < 0)
            return -1;

        fp = vlc_fopen( psz_idxTmp, "wt");
        if ( !fp )
        {
            msg_Err( p_access, "cannot open index file `%s'", psz_idxTmp );
            free( psz_idxTmp );
            return -1;
        }

        if ( fprintf( fp, "#EXTM3U\n#EXT-X-TARGETDURATION:%zu\n#EXT-X-VERSION:3\n#EXT-X-ALLOW-CACHE:%s\n#EXT-X-MEDIA-SEQUENCE:%"PRIu32"\n", p_sys->i_seglen, p_sys->b_caching ? "YES" : "NO",i_firstseg ) < 0 )
        {
            free( psz_idxTmp );
            fclose( fp );
            return -1;
        }

        if( p_sys->key_uri )
        {
            if( fprintf( fp, "#EXT-X-KEY:METHOD=AES-128,URI=\"%s\"\n", p_sys->key_uri ) < 0 )
            {
                free( psz_idxTmp );
                fclose( fp );
                return -1;
            }
        }

        char *psz_idxFormat = p_sys->psz_indexUrl ? p_sys->psz_indexUrl : p_access->psz_path;
        for ( uint32_t i = i_firstseg; i <= p_sys->i_segment; i++ )
        {
            char *psz_name;
            char *psz_duration = NULL;
            if ( ! ( psz_name = formatSegmentPath( psz_idxFormat, i, false ) ) )
            {
                free( psz_idxTmp );
                fclose( fp );
                return -1;
            }
            if( ! ( us_asprintf( &psz_duration, "%.2f", p_sys->p_seglens[i % p_sys->i_seglens ], psz_name ) ) )
            {
                free( psz_idxTmp );
                fclose( fp );
                return -1;
            }
            val = fprintf( fp, "#EXTINF:%s,\n%s\n", psz_duration, psz_name );
            free( psz_duration );
            free( psz_name );
            if ( val < 0 )
            {
                free( psz_idxTmp );
                fclose( fp );
                return -1;
            }
        }

        if ( b_isend )
        {
            if ( fputs ( STR_ENDLIST, fp ) < 0)
            {
                free( psz_idxTmp );
                fclose( fp ) ;
                return -1;
            }

        }
        fclose( fp );

        val = vlc_rename ( psz_idxTmp, p_sys->psz_indexPath);

        if ( val < 0 )
        {
            vlc_unlink( psz_idxTmp );
            msg_Err( p_access, "Error moving LiveHttp index file" );
        }
        else
            msg_Info( p_access, "LiveHttpIndexComplete: %s" , p_sys->psz_indexPath );

        free( psz_idxTmp );
    }

    // Then take care of deletion
    if ( p_sys->b_delsegs && i_firstseg > 1 )
    {
        char *psz_name = formatSegmentPath( p_access->psz_path, i_firstseg-1, true );
         if ( psz_name )
         {
             vlc_unlink( psz_name );
             free( psz_name );
         }
    }
    return 0;
}

/*****************************************************************************
 * closeCurrentSegment: Close the segment file
 *****************************************************************************/
static void closeCurrentSegment( sout_access_out_t *p_access, sout_access_out_sys_t *p_sys, bool b_isend )
{
    if ( p_sys->i_handle >= 0 )
    {
        close( p_sys->i_handle );
        p_sys->i_handle = -1;
        if ( p_sys->psz_cursegPath )
        {
            msg_Info( p_access, "LiveHttpSegmentComplete: %s (%"PRIu32")" , p_sys->psz_cursegPath, p_sys->i_segment );
            free( p_sys->psz_cursegPath );
            p_sys->psz_cursegPath = 0;
            updateIndexAndDel( p_access, p_sys, b_isend );
        }
    }
}

/*****************************************************************************
 * Close: close the target
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    sout_access_out_t *p_access = (sout_access_out_t*)p_this;
    sout_access_out_sys_t *p_sys = p_access->p_sys;

    msg_Dbg( p_access, "Flushing buffer to last file");
    while( p_sys->block_buffer )
    {
        if( p_sys->key_uri )
        {
            size_t original = p_sys->block_buffer->i_buffer;
            size_t padded = (p_sys->block_buffer->i_buffer + 15 ) & ~15;
            if( padded == p_sys->block_buffer->i_buffer )
                padded += 16;
            p_sys->block_buffer = block_Realloc( p_sys->block_buffer, 0, padded );
            if( !p_sys->block_buffer )
                break;
            int pad = padded - original;
            memset( &p_sys->block_buffer->p_buffer[original], pad, pad );

            gcry_error_t err = gcry_cipher_encrypt( p_sys->aes_ctx,
                                p_sys->block_buffer->p_buffer, padded, NULL, 0 );
            if( err )
            {
                msg_Err( p_access, "Encryption failure: %s ", gpg_strerror(err) );
                break;
            }

        }
        ssize_t val = write( p_sys->i_handle, p_sys->block_buffer->p_buffer, p_sys->block_buffer->i_buffer );
        if ( val == -1 )
        {
           if ( errno == EINTR )
              continue;
           block_ChainRelease ( p_sys->block_buffer);
           break;
        }
        if( !p_sys->block_buffer->p_next )
        {
            p_sys->p_seglens[p_sys->i_segment % p_sys->i_seglens ] =
                (float)( p_sys->block_buffer->i_length / (1000000)) +
                (float)(p_sys->block_buffer->i_dts - p_sys->i_opendts) / CLOCK_FREQ;
        }

        if ( (size_t)val >= p_sys->block_buffer->i_buffer )
        {
           block_t *p_next = p_sys->block_buffer->p_next;
           block_Release (p_sys->block_buffer);
           p_sys->block_buffer = p_next;
        }
        else
        {
           p_sys->block_buffer->p_buffer += val;
           p_sys->block_buffer->i_buffer -= val;
        }
    }

    closeCurrentSegment( p_access, p_sys, true );
    free( p_sys->psz_indexUrl );
    free( p_sys->psz_indexPath );
    free( p_sys->p_seglens );
    free( p_sys );

    if( p_sys->key_uri )
    {
        gcry_cipher_close( p_sys->aes_ctx );
        free( p_sys->key_uri );
    }
    msg_Dbg( p_access, "livehttp access output closed" );
}

static int Control( sout_access_out_t *p_access, int i_query, va_list args )
{
    sout_access_out_sys_t *p_sys = p_access->p_sys;

    switch( i_query )
    {
        case ACCESS_OUT_CONTROLS_PACE:
        {
            bool *pb = va_arg( args, bool * );
            *pb = !p_sys->b_ratecontrol;
            //*pb = true;
            break;
        }

        default:
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

/*****************************************************************************
 * openNextFile: Open the segment file
 *****************************************************************************/
static ssize_t openNextFile( sout_access_out_t *p_access, sout_access_out_sys_t *p_sys )
{
    int fd;

    uint32_t i_newseg = p_sys->i_segment + 1;

    char *psz_seg = formatSegmentPath( p_access->psz_path, i_newseg, true );
    if ( !psz_seg )
        return -1;

    fd = vlc_open( psz_seg, O_WRONLY | O_CREAT | O_LARGEFILE |
                     O_TRUNC, 0666 );
    if ( fd == -1 )
    {
        msg_Err( p_access, "cannot open `%s' (%m)", psz_seg );
        free( psz_seg );
        return -1;
    }

    CryptKey( p_access, i_newseg );
    msg_Dbg( p_access, "Successfully opened livehttp file: %s (%"PRIu32")" , psz_seg, i_newseg );

    //free( psz_seg );
    p_sys->psz_cursegPath = psz_seg;
    p_sys->i_handle = fd;
    p_sys->i_segment = i_newseg;
    return fd;
}

/*****************************************************************************
 * Write: standard write on a file descriptor.
 *****************************************************************************/
static ssize_t Write( sout_access_out_t *p_access, block_t *p_buffer )
{
    size_t i_write = 0;
    sout_access_out_sys_t *p_sys = p_access->p_sys;
    block_t *p_temp;

    while( p_buffer )
    {
        if ( ( p_sys->b_splitanywhere || ( p_buffer->i_flags & BLOCK_FLAG_HEADER ) ) )
        {
            block_t *output = p_sys->block_buffer;
            p_sys->block_buffer = NULL;


            if( p_sys->i_handle > 0 &&
                ( p_buffer->i_dts - p_sys->i_opendts +
                  p_buffer->i_length * CLOCK_FREQ / INT64_C(1000000)
                ) >= p_sys->i_seglenm )
                closeCurrentSegment( p_access, p_sys, false );

            if ( p_sys->i_handle < 0 )
            {
                p_sys->i_opendts = output ? output->i_dts : p_buffer->i_dts;
                if ( openNextFile( p_access, p_sys ) < 0 )
                   return -1;
            }

            while( output )
            {
                if( p_sys->key_uri )
                {
                    size_t original = output->i_buffer;
                    size_t padded = (output->i_buffer + 15 ) & ~15;
                    if( padded == output->i_buffer )
                        padded += 16;
                    output = block_Realloc( output, 0, padded );
                    if( !output )
                        return VLC_ENOMEM;
                    int pad = padded - original;
                    memset( &output->p_buffer[original], pad, pad );

                    gcry_error_t err = gcry_cipher_encrypt( p_sys->aes_ctx,
                                        output->p_buffer, padded, NULL, 0 );
                    if( err )
                    {
                        msg_Err( p_access, "Encryption failure: %s ", gpg_strerror(err) );
                        return -1;
                    }

                }
                ssize_t val = write( p_sys->i_handle, output->p_buffer, output->i_buffer );
                if ( val == -1 )
                {
                   if ( errno == EINTR )
                      continue;
                   block_ChainRelease ( p_buffer );
                   return -1;
                }
                p_sys->p_seglens[p_sys->i_segment % p_sys->i_seglens ] =
                    (float)output->i_length / INT64_C(1000000) +
                    (float)(output->i_dts - p_sys->i_opendts) / CLOCK_FREQ;

                if ( (size_t)val >= output->i_buffer )
                {
                   block_t *p_next = output->p_next;
                   block_Release (output);
                   output = p_next;
                }
                else
                {
                   output->p_buffer += val;
                   output->i_buffer -= val;
                }
                i_write += val;
            }
        }

        p_temp = p_buffer->p_next;
        p_buffer->p_next = NULL;
        block_ChainAppend( &p_sys->block_buffer, p_buffer );
        p_buffer = p_temp;
    }

    return i_write;
}

/*****************************************************************************
 * Seek: seek to a specific location in a file
 *****************************************************************************/
static int Seek( sout_access_out_t *p_access, off_t i_pos )
{
    (void) i_pos;
    msg_Err( p_access, "livehttp sout access cannot seek" );
    return -1;
}

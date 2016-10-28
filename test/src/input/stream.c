/*****************************************************************************
 * stream.c test streams and stream filters
 *****************************************************************************
 * Copyright (C) 2015 VLC authors and VideoLAN
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

#include "../../libvlc/test.h"
#include "../lib/libvlc_internal.h"

#include <vlc_md5.h>
#include <vlc_stream.h>
#include <vlc_rand.h>
#include <vlc_fs.h>

#include <inttypes.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef TEST_NET
#define RAND_FILE_SIZE (25 * 1024 * 1024)
#else
#define HTTP_URL "http://streams.videolan.org/streams/ogm/MJPEG.ogm"
#define HTTP_MD5 "4eaf9e8837759b670694398a33f02bc0"
#endif

struct reader
{
    const char *psz_name;
    union {
        FILE *f;
        stream_t *s;
    } u;
    void *p_data;

    void        (*pf_close)( struct reader * );
    uint64_t    (*pf_getsize)( struct reader * );
    ssize_t     (*pf_read)( struct reader *, void *, size_t );
    ssize_t     (*pf_peek)( struct reader *, const uint8_t **, size_t );
    uint64_t    (*pf_tell)( struct reader * );
    int         (*pf_seek)( struct reader *, uint64_t );
};

#ifndef TEST_NET
static uint64_t
libc_getsize( struct reader *p_reader )
{
    struct stat st;
    int i_fd = fileno( p_reader->u.f );

    assert( i_fd >= 0 );
    assert( fstat( i_fd, &st ) != -1 );

    return st.st_size;
}

static ssize_t
libc_read( struct reader *p_reader, void *p_buf, size_t i_len )
{
    return fread( p_buf, 1, i_len , p_reader->u.f );
}

static ssize_t
libc_peek( struct reader *p_reader, const uint8_t **pp_buf, size_t i_len )
{
    ssize_t i_ret;
    long i_last_pos;

    free( p_reader->p_data );
    p_reader->p_data = malloc( i_len );
    assert( p_reader->p_data );

    i_last_pos = ftell( p_reader->u.f );

    i_ret = fread( p_reader->p_data, 1, i_len, p_reader->u.f );
    *pp_buf = p_reader->p_data;

    assert( fseek( p_reader->u.f, i_last_pos, SEEK_SET ) == 0 );
    return i_ret;
}

static uint64_t
libc_tell( struct reader *p_reader )
{
    long i_ret = ftell( p_reader->u.f );
    assert( i_ret >= 0 );
    return i_ret;
}

static int
libc_seek( struct reader *p_reader, uint64_t i_offset )
{
    return fseek( p_reader->u.f, (long) i_offset, SEEK_SET );
}

static void
libc_close( struct reader *p_reader )
{
    fclose( p_reader->u.f );
    free( p_reader->p_data );
    free( p_reader );
}

static struct reader *
libc_open( const char *psz_file )
{
    struct reader *p_reader = calloc( 1, sizeof(struct reader) );
    assert( p_reader );

    p_reader->u.f = fopen( psz_file, "r" );
    if( !p_reader->u.f )
    {
        free( p_reader );
        return NULL;
    }
    p_reader->pf_close = libc_close;
    p_reader->pf_getsize = libc_getsize;
    p_reader->pf_read = libc_read;
    p_reader->pf_peek = libc_peek;
    p_reader->pf_tell = libc_tell;
    p_reader->pf_seek = libc_seek;
    p_reader->psz_name = "libc";
    assert( p_reader->psz_name );
    return p_reader;
}
#endif

static uint64_t
stream_getsize( struct reader *p_reader )
{
    uint64_t i_size;

    assert( vlc_stream_GetSize( p_reader->u.s, &i_size ) == 0 );
    return i_size;
}

static ssize_t
stream_read( struct reader *p_reader, void *p_buf, size_t i_len )
{
    return vlc_stream_Read( p_reader->u.s, p_buf, i_len );
}

static ssize_t
stream_peek( struct reader *p_reader, const uint8_t **pp_buf, size_t i_len )
{
    return vlc_stream_Peek( p_reader->u.s, pp_buf, i_len );
}

static uint64_t
stream_tell( struct reader *p_reader )
{
    return vlc_stream_Tell( p_reader->u.s );
}

static int
stream_seek( struct reader *p_reader, uint64_t i_offset )
{
    return vlc_stream_Seek( p_reader->u.s, i_offset );
}

static void
stream_close( struct reader *p_reader )
{
    vlc_stream_Delete( p_reader->u.s );
    libvlc_release( p_reader->p_data );
    free( p_reader );
}

static struct reader *
stream_open( const char *psz_url )
{
    libvlc_instance_t *p_vlc;
    struct reader *p_reader;
    const char * argv[] = {
        "-v",
        "--ignore-config",
        "-I",
        "dummy",
        "--no-media-library",
        "--vout=dummy",
        "--aout=dummy",
    };

    p_reader = calloc( 1, sizeof(struct reader) );
    assert( p_reader );

    p_vlc = libvlc_new( sizeof(argv) / sizeof(argv[0]), argv );
    assert( p_vlc != NULL );

    p_reader->u.s = vlc_stream_NewURL( p_vlc->p_libvlc_int, psz_url );
    if( !p_reader->u.s )
    {
        libvlc_release( p_vlc );
        free( p_reader );
        return NULL;
    }
    p_reader->pf_close = stream_close;
    p_reader->pf_getsize = stream_getsize;
    p_reader->pf_read = stream_read;
    p_reader->pf_peek = stream_peek;
    p_reader->pf_tell = stream_tell;
    p_reader->pf_seek = stream_seek;
    p_reader->p_data = p_vlc;
    p_reader->psz_name = "stream";
    return p_reader;
}

static ssize_t
read_at( struct reader **pp_readers, unsigned int i_readers,
         void *p_buf, uint64_t i_offset,
         size_t i_read, uint64_t i_size )
{
    void *p_cmp_buf = NULL;
    ssize_t i_cmp_ret = 0, i_ret = 0;

    for( unsigned i = 0; i < i_readers; ++i )
    {
        uint64_t i_last_pos;
        const uint8_t *p_peek = NULL;
        struct reader *p_reader = pp_readers[i];

        log( "%s: %s %zu @ %"PRIu64" (size: %" PRIu64 ")\n", p_reader->psz_name,
              p_buf ? "read" : "peek", i_read, i_offset, i_size );
        assert( p_reader->pf_seek( p_reader, i_offset ) != -1 );

        i_last_pos = p_reader->pf_tell( p_reader );
        assert( i_last_pos == i_offset );

        if( p_buf )
        {
            i_ret = p_reader->pf_read( p_reader, p_buf, i_read );
            assert( i_ret >= 0 );
            assert( p_reader->pf_tell( p_reader ) == i_ret + i_last_pos );
        }
        else
        {
            i_ret = p_reader->pf_peek( p_reader, &p_peek, i_read );
            assert( i_ret >= 0 );
            assert( p_reader->pf_tell( p_reader ) == i_last_pos );
            if( i_ret > 0 )
                assert( p_peek );
        }

        if( i_offset < i_size )
            assert( (size_t) i_ret == __MIN( i_read, i_size - i_last_pos ) );

        if( i_readers > 1 )
        {
            if( i == 0 )
            {
                if( i_ret > 0 )
                {
                    p_cmp_buf = malloc( i_ret );
                    assert( p_cmp_buf );
                    memcpy( p_cmp_buf, p_buf ? p_buf : p_peek, i_ret );
                }
                i_cmp_ret = i_ret;
            }
            else
            {
                assert( i_cmp_ret == i_ret );
                if( i_ret > 0 )
                    assert( memcmp( p_cmp_buf, p_buf ? p_buf : p_peek, i_ret ) == 0 );
            }
        }
    }
    free( p_cmp_buf );
    return i_ret;
}

static void
test( struct reader **pp_readers, unsigned int i_readers, const char *psz_md5 )
{
#define READ_AT( i_offset, i_read ) \
    read_at( pp_readers, i_readers, p_buf, i_offset, i_read, i_size )
#define PEEK_AT( i_offset, i_read ) \
    read_at( pp_readers, i_readers, NULL, i_offset, i_read, i_size )
    uint8_t p_buf[4096];
    ssize_t i_ret = 0;
    uint64_t i_offset = 0;
    uint64_t i_size;
    char *psz_read_md5;
    struct md5_s md5;

    /* Compare size between each readers */
    i_size = pp_readers[0]->pf_getsize( pp_readers[0] );
    assert( i_size > 0 );

    log( "stream size: %"PRIu64"\n", i_size );
    for( unsigned int i = 1; i < i_readers; ++i )
        assert( pp_readers[i]->pf_getsize( pp_readers[i] ) == i_size );

    /* Read the whole file and compare between each readers */
    if( psz_md5 != NULL )
        InitMD5( &md5 );
    while( ( i_ret = READ_AT( i_offset, 4096 ) ) > 0 )
    {
        i_offset += i_ret;
        if( psz_md5 != NULL )
            AddMD5( &md5, p_buf, i_ret );
    }
    if( psz_md5 != NULL )
    {
        EndMD5( &md5 );
        psz_read_md5 = psz_md5_hash( &md5 );
        assert( psz_read_md5 );
        assert( strcmp( psz_read_md5, psz_md5 ) == 0 );
        free( psz_read_md5 );
    }

    /* Test cache skip */
    i_offset = 9 * i_size / 10;
    while( i_offset < i_size && ( i_ret = READ_AT( i_offset, 4096 ) ) > 0 )
        i_offset += i_ret + 1;

    /* Test seek and peek */
    READ_AT( 0, 42 );
    READ_AT( i_size - 5, 43 );
    READ_AT( 1, 45 );
    READ_AT( 2, 45 );
    READ_AT( i_size / 2, 45 );
    READ_AT( 2, 45 );
    READ_AT( 1, 45 );

    PEEK_AT( 0, 46 );
    PEEK_AT( i_size - 23, 46 );
    PEEK_AT( i_size / 2, 46 );
    PEEK_AT( 0, 46 );
}

#ifndef TEST_NET
static void
fill_rand( int i_fd, size_t i_size )
{
    uint8_t p_buf[4096];
    size_t i_written = 0;
    while( i_written < i_size )
    {
        size_t i_tocopy = __MIN( i_size - i_written, 4096 );

        vlc_rand_bytes(p_buf, i_tocopy);
        ssize_t i_ret = write( i_fd, p_buf, i_tocopy );
        assert( i_ret > 0 );
        i_written += i_ret;
    }
    assert( i_written == i_size );
}
#endif

int
main( void )
{
    struct reader *pp_readers[3];

    test_init();

#ifndef TEST_NET
    char psz_tmp_path[] = "/tmp/libvlc_XXXXXX";
    char *psz_url;
    int i_tmp_fd;

    log( "Test random file with libc, and stream\n" );
    i_tmp_fd = vlc_mkstemp( psz_tmp_path );
    fill_rand( i_tmp_fd, RAND_FILE_SIZE );
    assert( i_tmp_fd != -1 );
    assert( asprintf( &psz_url, "file://%s", psz_tmp_path ) != -1 );

    assert( ( pp_readers[0] = libc_open( psz_tmp_path ) ) );
    assert( ( pp_readers[1] = stream_open( psz_url ) ) );

    test( pp_readers, 2, NULL );
    for( unsigned int i = 0; i < 2; ++i )
        pp_readers[i]->pf_close( pp_readers[i] );
    free( psz_url );

    close( i_tmp_fd );
#else

    log( "Test http url with stream\n" );
    alarm( 0 );
    if( !( pp_readers[0] = stream_open( HTTP_URL ) ) )
    {
        log( "WARNING: can't test http url" );
        return 0;
    }

    test( pp_readers, 1, HTTP_MD5 );
    for( unsigned int i = 0; i < 1; ++i )
        pp_readers[i]->pf_close( pp_readers[i] );

#endif

    return 0;
}

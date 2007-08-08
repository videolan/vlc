/*****************************************************************************
 * overlay-test.c : test program for the dynamic overlay plugin
 *****************************************************************************
 * Copyright (C) 2007 the VideoLAN team
 * $Id$
 *
 * Author: Søren Bøg <avacore@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>

#include <sys/fcntl.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>

/*****************************************************************************
 * Images
 *****************************************************************************/

#define WIDTH 128
#define HEIGHT 128

#define TEXT "Hello world!"
#define TEXTSIZE sizeof( TEXT )

char *p_imageRGBA;
char *p_text;

void DataCreate( void ) {
    char *p_data = p_imageRGBA;
    for( size_t i = 0; i < HEIGHT; ++i ) {
        for( size_t j = 0; j < HEIGHT; ++j ) {
            *(p_data++) = i * 4 & 0xFF;
            *(p_data++) = 0xFF;
            *(p_data++) = 0x00;
            *(p_data++) = j * 4 & 0xFF;
        }
    }

    memcpy( p_text, TEXT, TEXTSIZE );
}

/*****************************************************************************
 * I/O Helpers
 *****************************************************************************/

int IsFailure( char *psz_text ) {
    return strncmp( psz_text, "SUCCESS:", 8 );
}

void CheckResult( FILE *p_res ) {
    char psz_resp[9];

    fscanf( p_res, "%8s", &psz_resp );
    if( IsFailure( psz_resp ) ) {
        printf( " failed\n" );
        exit( -1 );
    }
}

void CheckedCommand( FILE *p_cmd, FILE *p_res, char *p_format, ... ) {
    va_list ap;
    va_start( ap, p_format );

    vfprintf( p_cmd, p_format, ap );
    fflush( p_cmd );
    if( p_res != NULL ) {
        CheckResult( p_res );
    }
    va_end( ap );
}

int GenImage( FILE *p_cmd, FILE *p_res ) {
    int i_overlay;

    printf( "Getting an overlay..." );
    CheckedCommand( p_cmd, p_res, "GenImage\n" );
    fscanf( p_res, "%d", &i_overlay );
    printf( " done. Overlay is %d\n", i_overlay );

    return i_overlay;
}

void DeleteImage( FILE *p_cmd, FILE *p_res, int i_overlay ) {
    printf( "Removing image..." );
    CheckedCommand( p_cmd, p_res, "DeleteImage %d\n", i_overlay );
    printf( " done\n" );
}

void StartAtomic( FILE *p_cmd, FILE *p_res ) {
    CheckedCommand( p_cmd, p_res, "StartAtomic\n" );
}

void EndAtomic( FILE *p_cmd, FILE *p_res ) {
    CheckedCommand( p_cmd, p_res, "EndAtomic\n" );
}

void DataSharedMem( FILE *p_cmd, FILE *p_res, int i_overlay, int i_width,
                    int i_height, char *psz_format, int i_shmid ) {

    printf( "Sending data via shared memory..." );
    CheckedCommand( p_cmd, p_res, "DataSharedMem %d %d %d %s %d\n", i_overlay,
                    i_width, i_height, psz_format, i_shmid );
    printf( " done\n" );
}

void SetAlpha( FILE *p_cmd, FILE *p_res, int i_overlay, int i_alpha ) {
    CheckedCommand( p_cmd, p_res, "SetAlpha %d %d\n", i_overlay, i_alpha );
}

void SetPosition( FILE *p_cmd, FILE *p_res, int i_overlay, int i_x, int i_y ) {
    CheckedCommand( p_cmd, p_res, "SetPosition %d %d %d\n", i_overlay, i_x,
                    i_y );
}

void SetVisibility( FILE *p_cmd, FILE *p_res, int i_overlay, int i_visible ) {
    CheckedCommand( p_cmd, p_res, "SetVisibility %d %d\n", i_overlay,
                    i_visible );
}

/*****************************************************************************
 * Test Routines
 *****************************************************************************/

void BasicTest( FILE *p_cmd, FILE *p_res, int i_overlay ) {
    printf( "Activating overlay..." );
    SetVisibility( p_cmd, p_res, i_overlay, 1 );
    printf( " done\n" );

    printf( "Sweeping alpha..." );
    for( int i_alpha = 0xFF; i_alpha >= -0xFF ; i_alpha -= 8 ) {
        SetAlpha( p_cmd, p_res, i_overlay, abs( i_alpha ) );
        usleep( 20000 );
    }
    SetAlpha( p_cmd, p_res, i_overlay, 255 );
    printf( " done\n" );

    printf( "Circle motion..." );
    for( float f_theta = 0; f_theta <= 2 * M_PI ; f_theta += M_PI / 64.0 ) {
        SetPosition( p_cmd, p_res, i_overlay,
                     (int)( - cos( f_theta ) * 100.0 + 100.0 ),
                     (int)( - sin( f_theta ) * 100.0 + 100.0 ) );
        usleep( 20000 );
    }
    SetPosition( p_cmd, p_res, i_overlay, 0, 100 );
    printf( " done\n" );

    printf( "Atomic motion..." );
    StartAtomic( p_cmd, p_res );
    SetPosition( p_cmd, NULL, i_overlay, 200, 50 );
    sleep( 1 );
    SetPosition( p_cmd, NULL, i_overlay, 0, 0 );
    EndAtomic( p_cmd, p_res );
    CheckResult( p_res );
    CheckResult( p_res );
    printf( " done\n" );

    sleep( 5 );
}

/*****************************************************************************
 * main
 *****************************************************************************/

int main( int i_argc, char *ppsz_argv[] ) {
    if( i_argc != 3 ) {
        printf( "Incorrect number of parameters.\n"
                "Usage is: %s command-fifo response-fifo\n", ppsz_argv[0] );
        exit( -2 );
    }

    printf( "Creating shared memory for RGBA..." );
    int i_shmRGBA = shmget( IPC_PRIVATE, WIDTH * HEIGHT * 4, S_IRWXU );
    if( i_shmRGBA == -1 ) {
        printf( " failed\n" );
        exit( -1 );
    }
    printf( " done, ID is %d. Text...", i_shmRGBA );
    int i_shmText = shmget( IPC_PRIVATE, TEXTSIZE, S_IRWXU );
    if( i_shmText == -1 ) {
        printf( " failed\n" );
        exit( -1 );
    }
    printf( " done, ID is %d\n", i_shmText );

    printf( "Attaching shared memory for RGBA..." );
    p_imageRGBA = shmat( i_shmRGBA, NULL, 0 );
    if( p_imageRGBA == -1 ) {
        printf( " failed\n" );
        exit( -1 );
    }
    printf( " done. Text..." );
    p_text = shmat( i_shmText, NULL, 0 );
    if( p_text == -1 ) {
        printf( " failed\n" );
        exit( -1 );
    }
    printf( " done\n" );

    printf( "Queueing shared memory for destruction, RGBA..." );
    if( shmctl( i_shmRGBA, IPC_RMID, 0 ) == -1 ) {
        printf( " failed\n" );
        exit( -1 );
    }
    printf( " done. Text..." );
    if( shmctl( i_shmText, IPC_RMID, 0 ) == -1 ) {
        printf( " failed\n" );
        exit( -1 );
    }
    printf( " done\n" );

    printf( "Generating data..." );
    DataCreate();
    printf( " done\n" );

    printf( "Please make sure vlc is running.\n"
            "You should append parameters similar to the following:\n"
            "--sub-filter overlay --overlay-input %s --overlay-output %s\n",
            ppsz_argv[1], ppsz_argv[2] );

    printf( "Opening FIFOs..." );
    FILE *p_cmd = fopen( ppsz_argv[1], "w" );
    if( p_cmd == NULL ) {
        printf( " failed\n" );
        exit( -1 );
    }
    FILE *p_res = fopen( ppsz_argv[2], "r" );
    if( p_res == NULL ) {
        printf( " failed\n" );
        exit( -1 );
    }
    printf( " done\n" );

    int i_overlay_image = GenImage( p_cmd, p_res );
    int i_overlay_text = GenImage( p_cmd, p_res );
    DataSharedMem( p_cmd, p_res, i_overlay_image, WIDTH, HEIGHT, "RGBA",
                   i_shmRGBA );
    DataSharedMem( p_cmd, p_res, i_overlay_text, TEXTSIZE, 1, "TEXT",
                   i_shmText );

    BasicTest( p_cmd, p_res, i_overlay_image );
    BasicTest( p_cmd, p_res, i_overlay_text );

    DeleteImage( p_cmd, p_res, i_overlay_image );
    DeleteImage( p_cmd, p_res, i_overlay_text );
}

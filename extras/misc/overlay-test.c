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

char *p_imageRGBA;

void ImagesCreate( void ) {
    char *p_data = p_imageRGBA;
    for( size_t i = 0; i < HEIGHT; ++i ) {
        for( size_t j = 0; j < HEIGHT; ++j ) {
            *(p_data++) = i * 4 & 0xFF;
            *(p_data++) = 0xFF;
            *(p_data++) = 0x00;
            *(p_data++) = j * 4 & 0xFF;
        }
    }
}

/*****************************************************************************
 * I/O Helpers
 *****************************************************************************/

int IsFailure( char *psz_text ) {
    return strncmp( psz_text, "SUCCESS:", 8 );
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

    printf( "Creating shared memory..." );
    int i_shmRGBA = shmget( IPC_PRIVATE, WIDTH * HEIGHT * 4, S_IRWXU );
    if( i_shmRGBA == -1 ) {
        printf( " failed\n" );
        exit( -1 );
    }
    printf( " done. ID is %d\n", i_shmRGBA );

    printf( "Attaching shared memory..." );
    p_imageRGBA = shmat( i_shmRGBA, NULL, 0 );
    if( p_imageRGBA == -1 ) {
        printf( " failed\n" );
        exit( -1 );
    }
    printf( " done\n" );

    printf( "Queueing shared memory for destruction..." );
    if( shmctl( i_shmRGBA, IPC_RMID, 0 ) == -1 ) {
        printf( " failed\n" );
        exit( -1 );
    }
    printf( " done\n" );

    printf( "Generating images..." );
    ImagesCreate();
    printf( " done\n" );

    printf( "Please make sure vlc is running.\n"
            "You should append parameters similar to the following:\n"
            "--sub-filter overlay --overlay-input %s --overlay-output %s\n",
            ppsz_argv[1], ppsz_argv[2] );

    printf( "Opening FIFOs..." );
    FILE *f_cmd = fopen( ppsz_argv[1], "w" );
    if( f_cmd == NULL ) {
        printf( " failed\n" );
        exit( -1 );
    }
    FILE *f_res = fopen( ppsz_argv[2], "r" );
    if( f_res == NULL ) {
        printf( " failed\n" );
        exit( -1 );
    }
    printf( " done\n" );

    printf( "Getting an overlay..." );
    int i_overlay;
    char psz_resp[9];
    fprintf( f_cmd, "GenImage\n" );
    fflush( f_cmd );
    fscanf( f_res, "%8s", &psz_resp );
    if( IsFailure( psz_resp ) ) {
        printf( " failed\n" );
        exit( -1 );
    }
    fscanf( f_res, "%d", &i_overlay );
    printf( " done. Overlay is %d\n", i_overlay );

    printf( "Sending data..." );
    fprintf( f_cmd, "DataSharedMem %d %d %d RGBA %d\n", i_overlay, WIDTH,
             HEIGHT, i_shmRGBA );
    fflush( f_cmd );
    fscanf( f_res, "%8s", &psz_resp );
    if( IsFailure( psz_resp ) ) {
        printf( " failed\n" );
        exit( -1 );
    }
    printf( " done\n" );

    printf( "Activating overlay..." );
    fprintf( f_cmd, "SetVisibility %d 1\n", i_overlay );
    fflush( f_cmd );
    fscanf( f_res, "%8s", &psz_resp );
    if( IsFailure( psz_resp ) ) {
        printf( " failed\n" );
        exit( -1 );
    }
    printf( " done\n" );

    printf( "Sweeping alpha..." );
    for( int i_alpha = 0xFF; i_alpha >= -0xFF ; i_alpha -= 8 ) {
        fprintf( f_cmd, "SetAlpha %d %d\n", i_overlay, abs( i_alpha ) );
        fflush( f_cmd );
        fscanf( f_res, "%8s", &psz_resp );
        if( IsFailure( psz_resp ) ) {
            printf( " failed\n" );
            exit( -1 );
        }
        usleep( 20000 );
    }
    fprintf( f_cmd, "SetAlpha %d 255\n", i_overlay );
    fflush( f_cmd );
    fscanf( f_res, "%8s", &psz_resp );
    if( IsFailure( psz_resp ) ) {
        printf( " failed\n" );
        exit( -1 );
    }
    printf( " done\n" );

    printf( "Circle motion..." );
    for( float f_theta = 0; f_theta <= 2 * M_PI ; f_theta += M_PI / 64.0 ) {
        fprintf( f_cmd, "SetPosition %d %d %d\n", i_overlay,
                 (int)( - cos( f_theta ) * 100.0 + 100.0 ),
                 (int)( - sin( f_theta ) * 100.0 + 100.0 ) );
        fflush( f_cmd );
        fscanf( f_res, "%8s", &psz_resp );
        if( IsFailure( psz_resp ) ) {
            printf( " failed\n" );
            exit( -1 );
        }
        usleep( 20000 );
    }
    fprintf( f_cmd, "SetPosition %d 0 0\n", i_overlay );
    fflush( f_cmd );
    fscanf( f_res, "%8s", &psz_resp );
    if( IsFailure( psz_resp ) ) {
        printf( " failed\n" );
        exit( -1 );
    }
    printf( " done\n" );

    printf( "Removing image..." );
    fprintf( f_cmd, "DeleteImage %d\n", i_overlay );
    fflush( f_cmd );
    fscanf( f_res, "%8s", &psz_resp );
    if( IsFailure( psz_resp ) ) {
        printf( " failed\n" );
        exit( -1 );
    }
    printf( " done\n" );
}

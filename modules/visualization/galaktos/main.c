/*****************************************************************************
 * main.c:
 *****************************************************************************
 * Copyright (C) 2004 VideoLAN
 * $Id$
 *
 * Authors: Cyril Deguet <asmax@videolan.org>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#include "glx.h"
#include <GL/gl.h>
#include <unistd.h>
#include <math.h>

int galaktos_update( galaktos_thread_t *p_thread, int16_t p_data[2][512] )
{
    int j;

    /* Process X11 events */
    if( galaktos_glx_handle_events( p_thread ) == 1 )
    {
        return 1;
    }

    glClearColor( 0.0f, 0.0f, 0.0f, 1.0f );
    glClear(GL_COLOR_BUFFER_BIT);

    glBegin(GL_TRIANGLE_STRIP);
        glColor3f( 0.5f, 0.0f, 0.0f);
        glVertex2f(  -1.0f,  1.0f);
        glColor3f( 0.2f, 0.2f, 0.0f);
        glVertex2f(  1.0f,  1.0f);
        glColor3f( 0.0f, 0.2f, 0.2f);
        glVertex2f(  -1.0f,  -1.0f);
        glColor3f( 0.0f, 0.0f, 0.5f);
        glVertex2f(  1.0f,  -1.0f);
    glEnd();
    glBegin(GL_LINE_STRIP);
        glColor3f( 1.0f, 1.0f, 1.0f);
        glVertex2f( -1.0f, 0.0f);
        for(j=0; j<512; j++)
        {
            glVertex2f( (float)j/256-1.0f, (float)p_data[0][j]/32000);
        }
    glEnd();

    galaktos_glx_swap( p_thread );

    return 0;
}



/*****************************************************************************
 * video_init.c:
 *****************************************************************************
 * Copyright (C) 2004 the VideoLAN team
 * $Id$
 *
 * Authors: Cyril Deguet <asmax@videolan.org>
 *          code from projectM http://xmms-projectm.sourceforge.net
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



//video_init.c - SDL/Opengl Windowing Creation/Resizing Functions
//
//by Peter Sperl
//
//Opens an SDL Window and creates an OpenGL session
//also able to handle resizing and fullscreening of windows
//just call init_display again with differant variables

#include <GL/gl.h>
#include <GL/glu.h>
#include "video_init.h"

extern int texsize;

extern char *buffer;


void setup_opengl( int w, int h )
{
   
    /* Our shading model--Gouraud (smooth). */
     glShadeModel( GL_SMOOTH);
    /* Culling. */
    //    glCullFace( GL_BACK );
    //    glFrontFace( GL_CCW );
    //    glEnable( GL_CULL_FACE );
    /* Set the clear color. */
    glClearColor( 0, 0, 0, 0 );
    /* Setup our viewport. */
     glViewport( 0, 0, w, h );
    /*
     * Change to the projection matrix and set
     * our viewing volume.
     */
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    
    //    gluOrtho2D(0.0, (GLfloat) width, 0.0, (GLfloat) height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();  
   
    //    glFrustum(0.0, height, 0.0,width,10,40);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

glDrawBuffer(GL_BACK); 
  glReadBuffer(GL_BACK); 
  glEnable(GL_BLEND); 

     glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); 
     // glBlendFunc(GL_SRC_ALPHA, GL_ONE); 
  glEnable(GL_LINE_SMOOTH);
  glEnable(GL_POINT_SMOOTH);
  glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT);
 
  // glCopyTexImage2D(GL_TEXTURE_2D,0,GL_RGB,0,0,texsize,texsize,0);
  //glCopyTexSubImage2D(GL_TEXTURE_2D,0,0,0,0,0,texsize,texsize);
   glLineStipple(2, 0xAAAA);
  
    
}

void CreateRenderTarget(int texsize,int *RenderTargetTextureID, int *RenderTarget )
{
    /* Create the texture that will be bound to the render target */
    glGenTextures(1, RenderTargetTextureID);
    glBindTexture(GL_TEXTURE_2D, *RenderTargetTextureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
#if 0
    /* Create the render target */
    *RenderTarget = SDL_GL_CreateRenderTarget(texsize,texsize, NULL);
        if ( *RenderTarget ) {
    
	int value;

	//printf("Created render target:\n");
	SDL_GL_GetRenderTargetAttribute( *RenderTarget, SDL_GL_RED_SIZE, &value );
	//	printf( "SDL_GL_RED_SIZE: %d\n", value);
	SDL_GL_GetRenderTargetAttribute( *RenderTarget, SDL_GL_GREEN_SIZE, &value );
	//	printf( "SDL_GL_GREEN_SIZE: %d\n", value);
	SDL_GL_GetRenderTargetAttribute( *RenderTarget, SDL_GL_BLUE_SIZE, &value );
	//	printf( "SDL_GL_BLUE_SIZE: %d\n", value);
	SDL_GL_GetRenderTargetAttribute( *RenderTarget, SDL_GL_ALPHA_SIZE, &value );
	//	printf( "SDL_GL_ALPHA_SIZE: %d\n", value);
	SDL_GL_GetRenderTargetAttribute( *RenderTarget, SDL_GL_DEPTH_SIZE, &value );
	//	printf( "SDL_GL_DEPTH_SIZE: %d\n", value );

	SDL_GL_BindRenderTarget(*RenderTarget, *RenderTargetTextureID);
       
    } else {
#endif
        /* We can fake a render target in this demo by rendering to the
         * screen and copying to a texture before we do normal rendering.
         */
    buffer = malloc(3*texsize*texsize);

        glBindTexture(GL_TEXTURE_2D, *RenderTargetTextureID);
        glTexImage2D(GL_TEXTURE_2D,
			0,
			GL_RGB,
			texsize, texsize,
			0,
			GL_RGB,
			GL_UNSIGNED_BYTE,
			buffer);
 //   }

}





/*****************************************************************************
 * main.c:
 *****************************************************************************
 * Copyright (C) 2004 the VideoLAN team
 * $Id$
 *
 * Authors: Cyril Deguet <asmax@videolan.org>
 *          Adapted from projectM (http://xmms-projectm.sourceforge.net/)
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

#include "plugin.h"
#include <GL/gl.h>
#include <GL/glu.h>
#include <unistd.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "common.h"
#include "preset_types.h"
#include "preset.h"
#include "engine_vars.h"
#include "per_pixel_eqn_types.h"
#include "per_pixel_eqn.h"
#include "interface_types.h"
#include "video_init.h"             //Video Init Routines, resizing/fullscreen, creating pbuffers
#include "PCM.h"                    //Sound data handler (buffering, FFT, etc.)
#include "beat_detect.h"            //beat detection routines
#include "custom_wave_types.h"
#include "custom_wave.h"
#include "custom_shape_types.h"
#include "custom_shape.h"
//#include <dmalloc.h>

// Forward declarations

void read_cfg();

void modulate_opacity_by_volume();
void maximize_colors();
void do_per_pixel_math();
void do_per_frame();

void render_interpolation();
void render_texture_to_screen();
void render_texture_to_studio();
void draw_motion_vectors();
void draw_borders();
void draw_shapes();
void draw_waveform();
void draw_custom_waves();

void reset_per_pixel_matrices();
void init_per_pixel_matrices();
void free_per_pixel_matrices();

int noSwitch=0;
int pcmframes=1;
int freqframes=0;
int totalframes=1;

int studio=0;

extern preset_t * active_preset;

GLuint RenderTargetTextureID;

double wave_o;

//double gx=32;  //size of interpolation
//double gy=24;

int texsize=512;   //size of texture to do actual graphics
int vw=512;           //runtime dimensions
int vh=512;
int fullscreen=0;

int maxsamples=2048; //size of PCM buffer
int numsamples; //size of new PCM info
double *pcmdataL;     //holder for most recent pcm data
double *pcmdataR;     //holder for most recent pcm data

int avgtime=500;  //# frames per preset

char *title = NULL;
int drawtitle;
int title_font;
int other_font;

int correction=1;

double vol;

//per pixel equation variables


double **gridx;  //grid containing interpolated mesh
double **gridy;
double **origtheta;  //grid containing interpolated mesh reference values
double **origrad;
double **origx;  //original mesh
double **origy;

char *buffer; //XXX

int galaktos_init( galaktos_thread_t *p_thread )
{
    init_per_pixel_matrices();
    pcmdataL=(double *)malloc(maxsamples*sizeof(double));
    pcmdataR=(double *)malloc(maxsamples*sizeof(double));

    /* Preset loading function */
    initPresetLoader();

    /* Load default preset directory */
//    loadPresetDir("/home/cyril/.vlc/galaktos");
    loadPresetDir("/etc/projectM/presets");

    initPCM(maxsamples);
    initBeatDetect();

    // mutex = SDL_CreateMutex();
    return 0;
}


void galaktos_done( galaktos_thread_t *p_thread )
{
    free(pcmdataL);
    free(pcmdataR);

    freeBeatDetect();
    freePCM();
    free_per_pixel_matrices();
    closePresetDir();
//    destroyPresetLoader(); XXX segfaults :(
}


int galaktos_update( galaktos_thread_t *p_thread )
{
    static int nohard=0;
    double vdataL[512];  //holders for FFT data (spectrum)
    double vdataR[512];

    avgtime=fps*18;
    totalframes++; //total amount of frames since startup

    Time=(double)(mdate()/1000000);

    frame++;  //number of frames for current preset
    progress= frame/(double)avgtime;
    if (progress>1.0) progress=1.0;
    // printf("start:%d at:%d min:%d stop:%d on:%d %d\n",startframe, frame frame-startframe,avgtime,  noSwitch,progress);

    if (frame>avgtime)
    {
        if (noSwitch==0) switchPreset(RANDOM_NEXT,0);
    }

    evalInitConditions();
    evalPerFrameEquations();

    evalCustomWaveInitConditions();
    evalCustomShapeInitConditions();

    //     printf("%f %d\n",Time,frame);

    reset_per_pixel_matrices();


    numsamples = getPCMnew(pcmdataR,1,0,fWaveSmoothing,0,0);
    getPCMnew(pcmdataL,0,0,fWaveSmoothing,0,1);
    getPCM(vdataL,512,0,1,0,0);
    getPCM(vdataR,512,1,1,0,0);

    bass=0;mid=0;treb=0;

    getBeatVals(vdataL,vdataR,&vol);

    nohard--;
    if(vol>8.0 && nohard<0 && noSwitch==0)
    {

        switchPreset(RANDOM_NEXT, HARD_CUT);
        nohard=100;
    }

    //BEGIN PASS 1
    //
    //This pass is used to render our texture
    //the texture is drawn to a subsection of the framebuffer
    //and then we perform our manipulations on it
    //in pass 2 we will copy the texture into texture memory

  //  galaktos_glx_activate_pbuffer( p_thread );

    glPushAttrib( GL_ALL_ATTRIB_BITS ); /* Overkill, but safe */

    //   if (RenderTarget) glViewport( 0, 0, RenderTarget->w, RenderTarget->h );
    if (0) {}
    else glViewport( 0, 0, texsize, texsize );


    glMatrixMode( GL_MODELVIEW );
    glPushMatrix();
    glLoadIdentity();

    glMatrixMode( GL_PROJECTION );
    glPushMatrix();
    glLoadIdentity();

    glOrtho(0.0, texsize, 0.0,texsize,10,40);

    do_per_pixel_math();

    do_per_frame();               //apply per-frame effects
    render_interpolation();       //apply per-pixel effects
    draw_motion_vectors();        //draw motion vectors
    draw_borders();               //draw borders

    draw_waveform();
    draw_shapes();
    draw_custom_waves();

    glMatrixMode( GL_MODELVIEW );
    glPopMatrix();

    glMatrixMode( GL_PROJECTION );
    glPopMatrix();

    glPopAttrib();

    //if ( RenderTarget )        SDL_GL_UnlockRenderTarget(RenderTarget);
        /* Copy our rendering to the fake render target texture */
    glBindTexture( GL_TEXTURE_2D, RenderTargetTextureID );
    glCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 0, 0, texsize, texsize);
//    galaktos_glx_activate_window( p_thread );

    //BEGIN PASS 2
    //
    //end of texture rendering
    //now we copy the texture from the framebuffer to
    //video texture memory and render fullscreen on a quad surface.
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    glFrustum(-vw*.5, vw*.5, -vh*.5,vh*.5,10,40);

    glLineWidth(texsize/512.0);
    if(studio%2)render_texture_to_studio();
    else render_texture_to_screen();

    glFinish();
    glFlush();
    //  printf("Flush %d\n",(SDL_GetTicks()-timestart));

    p_thread->p_opengl->pf_swap( p_thread->p_opengl );

    /* Process events */
    if( p_thread->p_opengl->pf_manage &&
        p_thread->p_opengl->pf_manage( p_thread->p_opengl ) )
    {
        return 1;
    }

    return 0;
}


void free_per_pixel_matrices()
{
    int x;

    for(x = 0; x < gx; x++)
    {
        free(gridx[x]);
        free(gridy[x]);
        free(origtheta[x]);
        free(origrad[x]);
        free(origx[x]);
        free(origy[x]);
        free(x_mesh[x]);
        free(y_mesh[x]);
        free(rad_mesh[x]);
        free(theta_mesh[x]);
    }

    free(origx);
    free(origy);
    free(gridx);
    free(gridy);
    free(x_mesh);
    free(y_mesh);
    free(rad_mesh);
    free(theta_mesh);
}


void init_per_pixel_matrices()
{
    int x,y;

    gridx=(double **)malloc(gx * sizeof(double *));
    gridy=(double **)malloc(gx * sizeof(double *));

    origx=(double **)malloc(gx * sizeof(double *));
    origy=(double **)malloc(gx * sizeof(double *));
    origrad=(double **)malloc(gx * sizeof(double *));
    origtheta=(double **)malloc(gx * sizeof(double *));

    x_mesh=(double **)malloc(gx * sizeof(double *));
    y_mesh=(double **)malloc(gx * sizeof(double *));
    rad_mesh=(double **)malloc(gx * sizeof(double *));
    theta_mesh=(double **)malloc(gx * sizeof(double *));

    sx_mesh=(double **)malloc(gx * sizeof(double *));
    sy_mesh=(double **)malloc(gx * sizeof(double *));
    dx_mesh=(double **)malloc(gx * sizeof(double *));
    dy_mesh=(double **)malloc(gx * sizeof(double *));
    cx_mesh=(double **)malloc(gx * sizeof(double *));
    cy_mesh=(double **)malloc(gx * sizeof(double *));
    zoom_mesh=(double **)malloc(gx * sizeof(double *));
    zoomexp_mesh=(double **)malloc(gx * sizeof(double *));
    rot_mesh=(double **)malloc(gx * sizeof(double *));

    for(x = 0; x < gx; x++)
    {
        gridx[x] = (double *)malloc(gy * sizeof(double));
        gridy[x] = (double *)malloc(gy * sizeof(double));

        origtheta[x] = (double *)malloc(gy * sizeof(double));
        origrad[x] = (double *)malloc(gy * sizeof(double));
        origx[x] = (double *)malloc(gy * sizeof(double));
        origy[x] = (double *)malloc(gy * sizeof(double));

        x_mesh[x] = (double *)malloc(gy * sizeof(double));
        y_mesh[x] = (double *)malloc(gy * sizeof(double));

        rad_mesh[x] = (double *)malloc(gy * sizeof(double));
        theta_mesh[x] = (double *)malloc(gy * sizeof(double));

        sx_mesh[x] = (double *)malloc(gy * sizeof(double));
        sy_mesh[x] = (double *)malloc(gy * sizeof(double));
        dx_mesh[x] = (double *)malloc(gy * sizeof(double));
        dy_mesh[x] = (double *)malloc(gy * sizeof(double));
        cx_mesh[x] = (double *)malloc(gy * sizeof(double));
        cy_mesh[x] = (double *)malloc(gy * sizeof(double));

        zoom_mesh[x] = (double *)malloc(gy * sizeof(double));
        zoomexp_mesh[x] = (double *)malloc(gy * sizeof(double));

        rot_mesh[x] = (double *)malloc(gy * sizeof(double));
    }

    //initialize reference grid values
    for (x=0;x<gx;x++)
    {
        for(y=0;y<gy;y++)
        {
            origx[x][y]=x/(double)(gx-1);
            origy[x][y]=-((y/(double)(gy-1))-1);
            origrad[x][y]=hypot((origx[x][y]-.5)*2,(origy[x][y]-.5)*2) * .7071067;
            origtheta[x][y]=atan2(((origy[x][y]-.5)*2),((origx[x][y]-.5)*2));
            gridx[x][y]=origx[x][y]*texsize;
            gridy[x][y]=origy[x][y]*texsize;
        }
    }
}



//calculate matrices for per_pixel
void do_per_pixel_math()
{
    int x,y;

    double rotx=0,roty=0;
    evalPerPixelEqns();

    if(!isPerPixelEqn(CX_OP))
    {
        for (x=0;x<gx;x++)
        {
            for(y=0;y<gy;y++){
                cx_mesh[x][y]=cx;
            }
        }
    }

    if(!isPerPixelEqn(CY_OP))
    {
        for (x=0;x<gx;x++)
        {
            for(y=0;y<gy;y++)
            {
                cy_mesh[x][y]=cy;
            }
        }
    }

    if(isPerPixelEqn(ROT_OP))
    {
        for (x=0;x<gx;x++)
        {
            for(y=0;y<gy;y++)
            {
                x_mesh[x][y]=x_mesh[x][y]-cx_mesh[x][y];
                y_mesh[x][y]=y_mesh[x][y]-cy_mesh[x][y];
                rotx=(x_mesh[x][y])*cos(rot_mesh[x][y])-(y_mesh[x][y])*sin(rot_mesh[x][y]);
                roty=(x_mesh[x][y])*sin(rot_mesh[x][y])+(y_mesh[x][y])*cos(rot_mesh[x][y]);
                x_mesh[x][y]=rotx+cx_mesh[x][y];
                y_mesh[x][y]=roty+cy_mesh[x][y];
            }
        }
    }



    if(!isPerPixelEqn(ZOOM_OP))
    {
        for (x=0;x<gx;x++)
        {
            for(y=0;y<gy;y++)
            {
                zoom_mesh[x][y]=zoom;
            }
        }
    }

    if(!isPerPixelEqn(ZOOMEXP_OP))
    {
        for (x=0;x<gx;x++)
        {
            for(y=0;y<gy;y++)
            {
                zoomexp_mesh[x][y]=zoomexp;
            }
        }
    }


    //DO ZOOM PER PIXEL
    for (x=0;x<gx;x++)
    {
        for(y=0;y<gy;y++)
        {
            x_mesh[x][y]=(x_mesh[x][y]-.5)*2;
            y_mesh[x][y]=(y_mesh[x][y]-.5)*2;
            x_mesh[x][y]=x_mesh[x][y]/(((zoom_mesh[x][y]-1)*(pow(rad_mesh[x][y],zoomexp_mesh[x][y])/rad_mesh[x][y]))+1);
            y_mesh[x][y]=y_mesh[x][y]/(((zoom_mesh[x][y]-1)*(pow(rad_mesh[x][y],zoomexp_mesh[x][y])/rad_mesh[x][y]))+1);
            x_mesh[x][y]=(x_mesh[x][y]*.5)+.5;
            y_mesh[x][y]=(y_mesh[x][y]*.5)+.5;
        }
    }

    if(isPerPixelEqn(SX_OP))
    {
        for (x=0;x<gx;x++)
        {
            for(y=0;y<gy;y++)
            {
                x_mesh[x][y]=((x_mesh[x][y]-cx_mesh[x][y])/sx_mesh[x][y])+cx_mesh[x][y];
            }
        }
    }

    if(isPerPixelEqn(SY_OP))
    {
        for (x=0;x<gx;x++)
        {
            for(y=0;y<gy;y++)
            {
                y_mesh[x][y]=((y_mesh[x][y]-cy_mesh[x][y])/sy_mesh[x][y])+cy_mesh[x][y];
            }
        }
    }

    if(isPerPixelEqn(DX_OP))
    {
        for (x=0;x<gx;x++)
        {
            for(y=0;y<gy;y++)
            {

                x_mesh[x][y]=x_mesh[x][y]-dx_mesh[x][y];

            }
        }
    }

    if(isPerPixelEqn(DY_OP))
    {
        for (x=0;x<gx;x++)
        {
            for(y=0;y<gy;y++)
            {
                y_mesh[x][y]=y_mesh[x][y]-dy_mesh[x][y];

            }
        }
    }


}

void reset_per_pixel_matrices()
{
    int x,y;

    for (x=0;x<gx;x++)
    {
        for(y=0;y<gy;y++)
        {
            x_mesh[x][y]=origx[x][y];
            y_mesh[x][y]=origy[x][y];
            rad_mesh[x][y]=origrad[x][y];
            theta_mesh[x][y]=origtheta[x][y];
        }
    }
}



void draw_custom_waves()
{
    int x;

    custom_wave_t *wavecode;
    glPointSize(texsize/512);
    //printf("%d\n",wavecode);
    //  more=isMoreCustomWave();
    // printf("not inner loop\n");
    while ((wavecode = nextCustomWave()) != NULL)
    {
        //printf("begin inner loop\n");
        if(wavecode->enabled==1)
        {
            // nextCustomWave();

            //glPushMatrix();

            //if(wavecode->bUseDots==1) glEnable(GL_LINE_STIPPLE);
            if (wavecode->bAdditive==0)  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            else    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            if (wavecode->bDrawThick==1)  glLineWidth(2*texsize/512);

            //  xx= ((pcmdataL[x]-pcmdataL[x-1])*80*fWaveScale)*2;
            //yy=pcmdataL[x]*80*fWaveScale,-1;
            //glVertex3f( (wave_x*texsize)+(xx+yy)*cos(45), (wave_y*texsize)+(-yy+xx)*cos(45),-1);
            // printf("samples: %d\n", wavecode->samples);

            getPCM(wavecode->value1,wavecode->samples,0,wavecode->bSpectrum,wavecode->smoothing,0);
            getPCM(wavecode->value2,wavecode->samples,1,wavecode->bSpectrum,wavecode->smoothing,0);
            // printf("%f\n",pcmL[0]);
            for(x=0;x<wavecode->samples;x++)
            {wavecode->value1[x]=wavecode->value1[x]*wavecode->scaling;}

            for(x=0;x<wavecode->samples;x++)
            {wavecode->value2[x]=wavecode->value2[x]*wavecode->scaling;}

            for(x=0;x<wavecode->samples;x++)
            {wavecode->sample_mesh[x]=((double)x)/((double)(wavecode->samples-1));}

            // printf("mid inner loop\n");
            evalPerPointEqns();
            /*
               if(!isPerPointEquation("x"))
               {for(x=0;x<wavecode->samples;x++)
               {cw_x[x]=0;} }

               if(!isPerPointEquation(Y_POINT_OP))
               {for(x=0;x<wavecode->samples;x++)
               {cw_y[x]=0;}}

               if(!isPerPointEquation(R_POINT_OP))
               {for(x=0;x<wavecode->samples;x++)
               {cw_r[x]=wavecode->r;}}
               if(!isPerPointEquation(G_POINT_OP))
               {for(x=0;x<wavecode->samples;x++)
               {cw_g[x]=wavecode->g;}}
               if(!isPerPointEquation(B_POINT_OP))
               {for(x=0;x<wavecode->samples;x++)
               {cw_b[x]=wavecode->b;}}
               if(!isPerPointEquation(A_POINT_OP))
               {for(x=0;x<wavecode->samples;x++)
               {cw_a[x]=wavecode->a;}}
             */
            //put drawing code here
            if (wavecode->bUseDots==1)   glBegin(GL_POINTS);
            else   glBegin(GL_LINE_STRIP);

            for(x=0;x<wavecode->samples;x++)
            {
                //          printf("x:%f y:%f a:%f g:%f %f\n", wavecode->x_mesh[x], wavecode->y_mesh[x], wavecode->a_mesh[x], wavecode->g_mesh[x], wavecode->sample_mesh[x]);
                glColor4f(wavecode->r_mesh[x],wavecode->g_mesh[x],wavecode->b_mesh[x],wavecode->a_mesh[x]);
                glVertex3f(wavecode->x_mesh[x]*texsize,-(wavecode->y_mesh[x]-1)*texsize,-1);
            }
            glEnd();
            glPointSize(texsize/512);
            glLineWidth(texsize/512);
            glDisable(GL_LINE_STIPPLE);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            //  glPopMatrix();

        }

    }
}



void draw_shapes()
{
    int i;

    double theta;
    double rad2;

    double pi = 3.14159265;
    double start,inc,xval,yval;
    custom_shape_t *shapecode;

    while ((shapecode = nextCustomShape()) != NULL)
    {
        if(shapecode->enabled==1)
        {
            // printf("drawing shape %f\n",shapecode->ang);
            shapecode->y=-((shapecode->y)-1);
            rad2=.5;
            shapecode->rad=shapecode->rad*(texsize*.707*.707*.707*1.04);
            //Additive Drawing or Overwrite
            if (shapecode->additive==0)  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            else    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

            glMatrixMode(GL_MODELVIEW);
            glPushMatrix();
            if(correction)
            {
                glTranslatef(texsize*.5,texsize*.5, 0);
                glScalef(1.0,vw/(double)vh,1.0);
                glTranslatef((-texsize*.5) ,(-texsize*.5),0);
            }

            start=.78539+shapecode->ang;
            inc=(pi*2)/(double)shapecode->sides;
            xval=shapecode->x*texsize;
            yval=shapecode->y*texsize;

            if (shapecode->textured)
            {
                glMatrixMode(GL_TEXTURE);
                glPushMatrix();
                glLoadIdentity();

                glTranslatef(.5,.5, 0);
                if (correction) glScalef(1,vw/(double)vh,1);

                glRotatef((shapecode->tex_ang*360/6.280), 0, 0, 1);

                glScalef(1/(shapecode->tex_zoom),1/(shapecode->tex_zoom),1);

                // glScalef(1,vh/(double)vw,1);
                glTranslatef((-.5) ,(-.5),0);
                // glScalef(1,vw/(double)vh,1);
                glEnable(GL_TEXTURE_2D);


                glBegin(GL_TRIANGLE_FAN);

                glColor4f(shapecode->r,shapecode->g,shapecode->b,shapecode->a);
                theta=start;
                glTexCoord2f(.5,.5);
                glVertex3f(xval,yval,-1);
                glColor4f(shapecode->r2,shapecode->g2,shapecode->b2,shapecode->a2);

                for ( i=0;i<shapecode->sides+1;i++)
                {

                    theta+=inc;
                    //  glColor4f(shapecode->r2,shapecode->g2,shapecode->b2,shapecode->a2);
                    glTexCoord2f(rad2*cos(theta)+.5 ,rad2*sin(theta)+.5 );
                    glVertex3f(shapecode->rad*cos(theta)+xval,shapecode->rad*sin(theta)+yval,-1);
                }
                glEnd();




                glDisable(GL_TEXTURE_2D);
                glPopMatrix();
                glMatrixMode(GL_MODELVIEW);
            }
            else
            {//Untextured (use color values)
                //printf("untextured %f %f %f @:%f,%f %f %f\n",shapecode->a2,shapecode->a,shapecode->border_a, shapecode->x,shapecode->y,shapecode->rad,shapecode->ang);
                //draw first n-1 triangular pieces
                glBegin(GL_TRIANGLE_FAN);

                glColor4f(shapecode->r,shapecode->g,shapecode->b,shapecode->a);
                theta=start;
                // glTexCoord2f(.5,.5);
                glVertex3f(xval,yval,-1);
                glColor4f(shapecode->r2,shapecode->g2,shapecode->b2,shapecode->a2);

                for ( i=0;i<shapecode->sides+1;i++)
                {

                    theta+=inc;
                    //  glColor4f(shapecode->r2,shapecode->g2,shapecode->b2,shapecode->a2);
                    //  glTexCoord2f(rad2*cos(theta)+.5 ,rad2*sin(theta)+.5 );
                    glVertex3f(shapecode->rad*cos(theta)+xval,shapecode->rad*sin(theta)+yval,-1);
                }
                glEnd();


            }
            if (bWaveThick==1)  glLineWidth(2*texsize/512);
            glBegin(GL_LINE_LOOP);
            glColor4f(shapecode->border_r,shapecode->border_g,shapecode->border_b,shapecode->border_a);
            for ( i=0;i<shapecode->sides;i++)
            {
                theta+=inc;
                glVertex3f(shapecode->rad*cos(theta)+xval,shapecode->rad*sin(theta)+yval,-1);
            }
            glEnd();
            if (bWaveThick==1)  glLineWidth(texsize/512);

            glPopMatrix();
        }
    }

}


void draw_waveform()
{

    int x;

    double r,theta;

    double offset,scale,dy2_adj;

    double co;

    double wave_x_temp=0;
    double wave_y_temp=0;

    modulate_opacity_by_volume();
    maximize_colors();

    if(bWaveDots==1) glEnable(GL_LINE_STIPPLE);

    offset=(wave_x-.5)*texsize;
    scale=texsize/505.0;

    //Thick wave drawing
    if (bWaveThick==1)  glLineWidth(2*texsize/512);

    //Additive wave drawing (vice overwrite)
    if (bAdditiveWaves==0)  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    else    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    switch(nWaveMode)
    {
        case 8://monitor

            glPushMatrix();

            glTranslatef(texsize*.5,texsize*.5, 0);
            glRotated(-wave_mystery*90,0,0,1);

            glTranslatef(-texsize*.5,-texsize*.825, 0);

            /*
               for (x=0;x<16;x++)
               {
               glBegin(GL_LINE_STRIP);
               glColor4f(1.0-(x/15.0),.5,x/15.0,1.0);
               glVertex3f((totalframes%256)*2*scale, -beat_val[x]*fWaveScale+texsize*wave_y,-1);
               glColor4f(.5,.5,.5,1.0);
               glVertex3f((totalframes%256)*2*scale, texsize*wave_y,-1);
               glColor4f(1.0,1.0,0,1.0);
            //glVertex3f((totalframes%256)*scale*2, beat_val_att[x]*fWaveScale+texsize*wave_y,-1);
            glEnd();

            glTranslatef(0,texsize*(1/36.0), 0);
            }
             */

            glTranslatef(0,texsize*(1/18.0), 0);


            glBegin(GL_LINE_STRIP);
            glColor4f(1.0,1.0,0.5,1.0);
            glVertex3f((totalframes%256)*2*scale, treb_att*5*fWaveScale+texsize*wave_y,-1);
            glColor4f(.2,.2,.2,1.0);
            glVertex3f((totalframes%256)*2*scale, texsize*wave_y,-1);
            glColor4f(1.0,1.0,0,1.0);
            glVertex3f((totalframes%256)*scale*2, treb*-5*fWaveScale+texsize*wave_y,-1);
            glEnd();

            glTranslatef(0,texsize*.075, 0);
            glBegin(GL_LINE_STRIP);
            glColor4f(0,1.0,0.0,1.0);
            glVertex3f((totalframes%256)*2*scale, mid_att*5*fWaveScale+texsize*wave_y,-1);
            glColor4f(.2,.2,.2,1.0);
            glVertex3f((totalframes%256)*2*scale, texsize*wave_y,-1);
            glColor4f(.5,1.0,.5,1.0);
            glVertex3f((totalframes%256)*scale*2, mid*-5*fWaveScale+texsize*wave_y,-1);
            glEnd();


            glTranslatef(0,texsize*.075, 0);
            glBegin(GL_LINE_STRIP);
            glColor4f(1.0,0,0,1.0);
            glVertex3f((totalframes%256)*2*scale, bass_att*5*fWaveScale+texsize*wave_y,-1);
            glColor4f(.2,.2,.2,1.0);
            glVertex3f((totalframes%256)*2*scale, texsize*wave_y,-1);
            glColor4f(1.0,.5,.5,1.0);
            glVertex3f((totalframes%256)*scale*2, bass*-5*fWaveScale+texsize*wave_y,-1);
            glEnd();


            glPopMatrix();
            break;

        case 0://circular waveforms
            //  double co;
            glPushMatrix();

            glTranslatef(texsize*.5,texsize*.5, 0);
            glScalef(1.0,vw/(double)vh,1.0);
            glTranslatef((-texsize*.5) ,(-texsize*.5),0);

            wave_y=-1*(wave_y-1.0);

            glBegin(GL_LINE_STRIP);

            for ( x=0;x<numsamples;x++)
            {
                co= -(abs(x-((numsamples*.5)-1))/numsamples)+1;
                // printf("%d %f\n",x,co);
                theta=x*(6.28/numsamples);
                r= ((1+2*wave_mystery)*(texsize/5.0)+
                    ( co*pcmdataL[x]+ (1-co)*pcmdataL[-(x-(numsamples-1))])
                    *25*fWaveScale);

                glVertex3f(r*cos(theta)+(wave_x*texsize),r*sin(theta)+(wave_y*texsize),-1);
            }

            r= ( (1+2*wave_mystery)*(texsize/5.0)+
                 (0.5*pcmdataL[0]+ 0.5*pcmdataL[numsamples-1])
                 *20*fWaveScale);

            glVertex3f(r*cos(0)+(wave_x*texsize),r*sin(0)+(wave_y*texsize),-1);

            glEnd();
            /*
               glBegin(GL_LINE_LOOP);

               for ( x=0;x<(512/pcmbreak);x++)
               {
               theta=(blockstart+x)*((6.28*pcmbreak)/512.0);
               r= ((1+2*wave_mystery)*(texsize/5.0)+fdata_buffer[fbuffer][0][blockstart+x]*.0025*fWaveScale);

               glVertex3f(r*cos(theta)+(wave_x*texsize),r*sin(theta)+(wave_y*texsize),-1);
               }
               glEnd();
             */
            glPopMatrix();

            break;

        case 1://circularly moving waveform
            //  double co;
            glPushMatrix();

            glTranslatef(texsize*.5,texsize*.5, 0);
            glScalef(1.0,vw/(double)vh,1.0);
            glTranslatef((-texsize*.5) ,(-texsize*.5),0);

            wave_y=-1*(wave_y-1.0);

            glBegin(GL_LINE_STRIP);
            //theta=(frame%512)*(6.28/512.0);

            for ( x=1;x<512;x++)
            {
                co= -(abs(x-255)/512.0)+1;
                // printf("%d %f\n",x,co);
                theta=((frame%256)*(2*6.28/512.0))+pcmdataL[x]*.2*fWaveScale;
                r= ((1+2*wave_mystery)*(texsize/5.0)+
                    (pcmdataL[x]-pcmdataL[x-1])*80*fWaveScale);

                glVertex3f(r*cos(theta)+(wave_x*texsize),r*sin(theta)+(wave_y*texsize),-1);
            }

            glEnd();

            glPopMatrix();

            break;

        case 2://EXPERIMENTAL
            wave_y=-1*(wave_y-1.0);
            glPushMatrix();
            glBegin(GL_LINE_STRIP);
            double xx,yy;
            // double xr= (wave_x*texsize), yr=(wave_y*texsize);
            xx=0;
            for ( x=1;x<512;x++)
            {
                //xx = ((pcmdataL[x]-pcmdataL[x-1])*80*fWaveScale)*2;
                xx += (pcmdataL[x]*fWaveScale);
                yy= pcmdataL[x]*80*fWaveScale;
                //  glVertex3f( (wave_x*texsize)+(xx+yy)*2, (wave_y*texsize)+(xx-yy)*2,-1);
                glVertex3f( (wave_x*texsize)+(xx)*2, (wave_y*texsize)+(yy)*2,-1);


                //   xr+=fdata_buffer[fbuffer][0][x] *.0005* fWaveScale;
                //yr=(fdata_buffer[fbuffer][0][x]-fdata_buffer[fbuffer][0][x-1])*.05*fWaveScale+(wave_y*texsize);
                //glVertex3f(xr,yr,-1);

            }
            glEnd();
            glPopMatrix();
            break;

        case 3://EXPERIMENTAL
            glPushMatrix();
            wave_y=-1*(wave_y-1.0);
            glBegin(GL_LINE_STRIP);


            for ( x=1;x<512;x++)
            {
                xx= ((pcmdataL[x]-pcmdataL[x-1])*80*fWaveScale)*2;
                yy=pcmdataL[x]*80*fWaveScale,-1;
                glVertex3f( (wave_x*texsize)+(xx+yy)*cos(45), (wave_y*texsize)+(-yy+xx)*cos(45),-1);
            }
            glEnd();
            glPopMatrix();
            break;

        case 4://single x-axis derivative waveform
            glPushMatrix();
            wave_y=-1*(wave_y-1.0);
            glTranslatef(texsize*.5,texsize*.5, 0);
            glRotated(-wave_mystery*90,0,0,1);
            glTranslatef(-texsize*.5,-texsize*.5, 0);
            wave_x=(wave_x*.75)+.125;      wave_x=-(wave_x-1);
            glBegin(GL_LINE_STRIP);

            double dy_adj;
            for ( x=1;x<512;x++)
            {
                dy_adj=  pcmdataL[x]*20*fWaveScale-pcmdataL[x-1]*20*fWaveScale;
                glVertex3f((x*scale)+dy_adj, pcmdataL[x]*20*fWaveScale+texsize*wave_x,-1);
            }
            glEnd();
            glPopMatrix();
            break;

        case 5://EXPERIMENTAL
            glPushMatrix();


            wave_y=-1*(wave_y-1.0);
            wave_x_temp=(wave_x*.75)+.125;
            wave_x_temp=-(wave_x_temp-1);
            glBegin(GL_LINE_STRIP);

            for ( x=1;x<(512);x++)
            {
                dy2_adj=  (pcmdataL[x]-pcmdataL[x-1])*20*fWaveScale;
                glVertex3f((wave_x_temp*texsize)+dy2_adj*2, pcmdataL[x]*20*fWaveScale+texsize*wave_y,-1);
            }
            glEnd();
            glPopMatrix();
            break;

        case 6://single waveform




            glTranslatef(0,0, -1);

            //glMatrixMode(GL_MODELVIEW);
            glPushMatrix();
            //            glLoadIdentity();

            glTranslatef(texsize*.5,texsize*.5, 0);
            glRotated(-wave_mystery*90,0,0,1);

            wave_x_temp=-2*0.4142*(abs(abs(wave_mystery)-.5)-.5);
            glScalef(1.0+wave_x_temp,1.0,1.0);
            glTranslatef(-texsize*.5,-texsize*.5, 0);
            wave_x_temp=-1*(wave_x-1.0);

            glBegin(GL_LINE_STRIP);
            //      wave_x_temp=(wave_x*.75)+.125;
            //      wave_x_temp=-(wave_x_temp-1);
            for ( x=0;x<numsamples;x++)
            {

                //glVertex3f(x*scale, fdata_buffer[fbuffer][0][blockstart+x]*.0012*fWaveScale+texsize*wave_x_temp,-1);
                glVertex3f(x*texsize/(double)numsamples, pcmdataR[x]*20*fWaveScale+texsize*wave_x_temp,-1);

                //glVertex3f(x*scale, texsize*wave_y_temp,-1);
            }
            //      printf("%f %f\n",texsize*wave_y_temp,wave_y_temp);
            glEnd();
            glPopMatrix();
            break;

        case 7://dual waveforms

            glPushMatrix();

            glTranslatef(texsize*.5,texsize*.5, 0);
            glRotated(-wave_mystery*90,0,0,1);

            wave_x_temp=-2*0.4142*(abs(abs(wave_mystery)-.5)-.5);
            glScalef(1.0+wave_x_temp,1.0,1.0);
            glTranslatef(-texsize*.5,-texsize*.5, 0);

            wave_y_temp=-1*(wave_x-1);

            glBegin(GL_LINE_STRIP);

            for ( x=0;x<numsamples;x++)
            {

                glVertex3f((x*texsize)/(double)numsamples, pcmdataL[x]*20*fWaveScale+texsize*(wave_y_temp+(wave_y*wave_y*.5)),-1);
            }
            glEnd();

            glBegin(GL_LINE_STRIP);


            for ( x=0;x<numsamples;x++)
            {

                glVertex3f((x*texsize)/(double)numsamples, pcmdataR[x]*20*fWaveScale+texsize*(wave_y_temp-(wave_y*wave_y*.5)),-1);
            }
            glEnd();
            glPopMatrix();
            break;

        default:
            glBegin(GL_LINE_LOOP);

            for ( x=0;x<512;x++)
            {
                theta=(x)*(6.28/512.0);
                r= (texsize/5.0+pcmdataL[x]*.002);

                glVertex3f(r*cos(theta)+(wave_x*texsize),r*sin(theta)+(wave_y*texsize),-1);
            }
            glEnd();

            glBegin(GL_LINE_STRIP);

            for ( x=0;x<512;x++)
            {
                glVertex3f(x*scale, pcmdataL[x]*20*fWaveScale+(texsize*(wave_x+.1)),-1);
            }
            glEnd();

            glBegin(GL_LINE_STRIP);

            for ( x=0;x<512;x++)
            {
                glVertex3f(x*scale, pcmdataR[x]*20*fWaveScale+(texsize*(wave_x-.1)),-1);

            }
            glEnd();
            break;
            if (bWaveThick==1)  glLineWidth(2*texsize/512);
    }
    glLineWidth(texsize/512);
    glDisable(GL_LINE_STIPPLE);
}


void maximize_colors()
{
    float wave_r_switch=0,wave_g_switch=0,wave_b_switch=0;
    //wave color brightening
    //
    //forces max color value to 1.0 and scales
    // the rest accordingly

    if (bMaximizeWaveColor==1)
    {
        if(wave_r>=wave_g && wave_r>=wave_b)   //red brightest
        {
            wave_b_switch=wave_b*(1/wave_r);
            wave_g_switch=wave_g*(1/wave_r);
            wave_r_switch=1.0;
        }
        else if   (wave_b>=wave_g && wave_b>=wave_r)         //blue brightest
        {
            wave_r_switch=wave_r*(1/wave_b);
            wave_g_switch=wave_g*(1/wave_b);
            wave_b_switch=1.0;

        }

        else  if (wave_g>=wave_b && wave_g>=wave_r)         //green brightest
        {
            wave_b_switch=wave_b*(1/wave_g);
            wave_r_switch=wave_r*(1/wave_g);
            wave_g_switch=1.0;
        }
        glColor4f(wave_r_switch, wave_g_switch, wave_b_switch, wave_o);
    }
    else
    {
        glColor4f(wave_r, wave_g, wave_b, wave_o);
    }

}


void modulate_opacity_by_volume()

{
    //modulate volume by opacity
    //
    //set an upper and lower bound and linearly
    //calculate the opacity from 0=lower to 1=upper
    //based on current volume

    if (bModWaveAlphaByVolume==1)
    {if (vol<=fModWaveAlphaStart)  wave_o=0.0;
        else if (vol>=fModWaveAlphaEnd) wave_o=fWaveAlpha;
        else wave_o=fWaveAlpha*((vol-fModWaveAlphaStart)/(fModWaveAlphaEnd-fModWaveAlphaStart));}
    else wave_o=fWaveAlpha;
}


void draw_motion_vectors()
{
    int x,y;

    double offsetx=mv_dx*texsize, intervalx=texsize/(double)mv_x;
    double offsety=mv_dy*texsize, intervaly=texsize/(double)mv_y;

    glPointSize(mv_l);
    glColor4f(mv_r, mv_g, mv_b, mv_a);
    glBegin(GL_POINTS);
    for (x=0;x<mv_x;x++){
        for(y=0;y<mv_y;y++){
            glVertex3f(offsetx+x*intervalx,offsety+y*intervaly,-1);
        }}

    glEnd();
}


void draw_borders()
{
    //no additive drawing for borders
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glTranslatef(0,0,-1);
    //Draw Borders
    double of=texsize*ob_size*.5;
    double iff=(texsize*ib_size*.5);
    double texof=texsize-of;

    glColor4d(ob_r,ob_g,ob_b,ob_a);

    glRectd(0,0,of,texsize);
    glRectd(of,0,texof,of);
    glRectd(texof,0,texsize,texsize);
    glRectd(of,texsize,texof,texof);
    glColor4d(ib_r,ib_g,ib_b,ib_a);
    glRectd(of,of,of+iff,texof);
    glRectd(of+iff,of,texof-iff,of+iff);
    glRectd(texof-iff,of,texof,texof);
    glRectd(of+iff,texof,texof-iff,texof-iff);
}


//Here we render the interpolated mesh, and then apply the texture to it.
//Well, we actually do the inverse, but its all the same.
void render_interpolation()
{

    int x,y;

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslated(0, 0, -9);

    glColor4f(0.0, 0.0, 0.0,decay);

    glEnable(GL_TEXTURE_2D);

    for (x=0;x<gx-1;x++)
    {
        glBegin(GL_TRIANGLE_STRIP);
        for(y=0;y<gy;y++)
        {
            glTexCoord4f(x_mesh[x][y], y_mesh[x][y],-1,1); glVertex4f(gridx[x][y], gridy[x][y],-1,1);
            glTexCoord4f(x_mesh[x+1][y], y_mesh[x+1][y],-1,1); glVertex4f(gridx[x+1][y], gridy[x+1][y],-1,1);
        }
        glEnd();
    }
    glDisable(GL_TEXTURE_2D);
}


void do_per_frame()
{
    //Texture wrapping( clamp vs. wrap)
    if (bTexWrap==0)
    {
        glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    }
    else
    {
        glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    }


    //      glRasterPos2i(0,0);
    //      glClear(GL_COLOR_BUFFER_BIT);
    //      glColor4d(0.0, 0.0, 0.0,1.0);

    //      glMatrixMode(GL_TEXTURE);
    //  glLoadIdentity();

    glRasterPos2i(0,0);
    glClear(GL_COLOR_BUFFER_BIT);
    glColor4d(0.0, 0.0, 0.0,1.0);

    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();

    glTranslatef(cx,cy, 0);
    if(correction)  glScalef(1,vw/(double)vh,1);

    if(!isPerPixelEqn(ROT_OP))
    {
        //    printf("ROTATING: rot = %f\n", rot);
        glRotatef(rot*90, 0, 0, 1);
    }
    if(!isPerPixelEqn(SX_OP)) glScalef(1/sx,1,1);
    if(!isPerPixelEqn(SY_OP)) glScalef(1,1/sy,1);

    if(correction) glScalef(1,vh/(double)vw,1);
    glTranslatef((-cx) ,(-cy),0);

    if(!isPerPixelEqn(DX_OP)) glTranslatef(-dx,0,0);
    if(!isPerPixelEqn(DY_OP)) glTranslatef(0 ,-dy,0);

}


//Actually draws the texture to the screen
//
//The Video Echo effect is also applied here
void render_texture_to_screen()
{
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0, 0, -9);

    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE,  GL_DECAL);
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);

    //       glClear(GL_ACCUM_BUFFER_BIT);
    glColor4d(0.0, 0.0, 0.0,1.0f);

    glBegin(GL_QUADS);
    glVertex4d(-vw*.5,-vh*.5,-1,1);
    glVertex4d(-vw*.5,  vh*.5,-1,1);
    glVertex4d(vw*.5,  vh*.5,-1,1);
    glVertex4d(vw*.5, -vh*.5,-1,1);
    glEnd();

    //      glBindTexture( GL_TEXTURE_2D, tex2 );
    glEnable(GL_TEXTURE_2D);

    // glAccum(GL_LOAD,0);
    // if (bDarken==1)  glBlendFunc(GL_SRC_COLOR,GL_ZERO);

    //Draw giant rectangle and texture it with our texture!
    glBegin(GL_QUADS);
    glTexCoord4d(0, 1,0,1); glVertex4d(-vw*.5,-vh*.5,-1,1);
    glTexCoord4d(0, 0,0,1); glVertex4d(-vw*.5,  vh*.5,-1,1);
    glTexCoord4d(1, 0,0,1); glVertex4d(vw*.5,  vh*.5,-1,1);
    glTexCoord4d(1, 1,0,1); glVertex4d(vw*.5, -vh*.5,-1,1);
    glEnd();

    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

    //  if (bDarken==1)  glBlendFunc(GL_SRC_COLOR,GL_ONE_MINUS_SRC_ALPHA);

    // if (bDarken==1) { glAccum(GL_ACCUM,1-fVideoEchoAlpha); glBlendFunc(GL_SRC_COLOR,GL_ZERO); }

    glMatrixMode(GL_TEXTURE);

    //draw video echo
    glColor4f(0.0, 0.0, 0.0,fVideoEchoAlpha);
    glTranslated(.5,.5,0);
    glScaled(1/fVideoEchoZoom,1/fVideoEchoZoom,1);
    glTranslated(-.5,-.5,0);

    int flipx=1,flipy=1;
    switch (((int)nVideoEchoOrientation))
    {
        case 0: flipx=1;flipy=1;break;
        case 1: flipx=-1;flipy=1;break;
        case 2: flipx=1;flipy=-1;break;
        case 3: flipx=-1;flipy=-1;break;
        default: flipx=1;flipy=1; break;
    }
    glBegin(GL_QUADS);
    glTexCoord4d(0, 1,0,1); glVertex4f(-vw*.5*flipx,-vh*.5*flipy,-1,1);
    glTexCoord4d(0, 0,0,1); glVertex4f(-vw*.5*flipx,  vh*.5*flipy,-1,1);
    glTexCoord4d(1, 0,0,1); glVertex4f(vw*.5*flipx,  vh*.5*flipy,-1,1);
    glTexCoord4d(1, 1,0,1); glVertex4f(vw*.5*flipx, -vh*.5*flipy,-1,1);
    glEnd();


    glDisable(GL_TEXTURE_2D);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

    // if (bDarken==1) { glAccum(GL_ACCUM,fVideoEchoAlpha); glAccum(GL_RETURN,1);}

    if (bInvert==1)
    {
        glColor4f(1.0, 1.0, 1.0,1.0);
        glBlendFunc(GL_ONE_MINUS_DST_COLOR,GL_ZERO);
        glBegin(GL_QUADS);
        glVertex4f(-vw*.5*flipx,-vh*.5*flipy,-1,1);
        glVertex4f(-vw*.5*flipx,  vh*.5*flipy,-1,1);
        glVertex4f(vw*.5*flipx,  vh*.5*flipy,-1,1);
        glVertex4f(vw*.5*flipx, -vh*.5*flipy,-1,1);
        glEnd();
        glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    }
}


void render_texture_to_studio()
{
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0, 0, -9);

    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE,  GL_DECAL);
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);

    //       glClear(GL_ACCUM_BUFFER_BIT);
    glColor4f(0.0, 0.0, 0.0,0.04);

    glVertex4d(-vw*.5,-vh*.5,-1,1);
    glVertex4d(-vw*.5,  vh*.5,-1,1);
    glVertex4d(vw*.5,  vh*.5,-1,1);
    glVertex4d(vw*.5, -vh*.5,-1,1);
    glEnd();

    glColor4f(0.0, 0.0, 0.0,1.0);

    glBegin(GL_QUADS);
    glVertex4d(-vw*.5,0,-1,1);
    glVertex4d(-vw*.5,  vh*.5,-1,1);
    glVertex4d(vw*.5,  vh*.5,-1,1);
    glVertex4d(vw*.5, 0,-1,1);
    glEnd();

    glBegin(GL_QUADS);
    glVertex4d(0,-vh*.5,-1,1);
    glVertex4d(0,  vh*.5,-1,1);
    glVertex4d(vw*.5,  vh*.5,-1,1);
    glVertex4d(vw*.5, -vh*.5,-1,1);
    glEnd();

    glPushMatrix();
    glTranslatef(.25*vw, .25*vh, 0);
    glScalef(.5,.5,1);

    //      glBindTexture( GL_TEXTURE_2D, tex2 );
    glEnable(GL_TEXTURE_2D);

    // glAccum(GL_LOAD,0);
    // if (bDarken==1)  glBlendFunc(GL_SRC_COLOR,GL_ZERO);

    //Draw giant rectangle and texture it with our texture!
    glBegin(GL_QUADS);
    glTexCoord4d(0, 1,0,1); glVertex4d(-vw*.5,-vh*.5,-1,1);
    glTexCoord4d(0, 0,0,1); glVertex4d(-vw*.5,  vh*.5,-1,1);
    glTexCoord4d(1, 0,0,1); glVertex4d(vw*.5,  vh*.5,-1,1);
    glTexCoord4d(1, 1,0,1); glVertex4d(vw*.5, -vh*.5,-1,1);
    glEnd();

    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

    //  if (bDarken==1)  glBlendFunc(GL_SRC_COLOR,GL_ONE_MINUS_SRC_ALPHA);

    // if (bDarken==1) { glAccum(GL_ACCUM,1-fVideoEchoAlpha); glBlendFunc(GL_SRC_COLOR,GL_ZERO); }

    glMatrixMode(GL_TEXTURE);

    //draw video echo
    glColor4f(0.0, 0.0, 0.0,fVideoEchoAlpha);
    glTranslated(.5,.5,0);
    glScaled(1/fVideoEchoZoom,1/fVideoEchoZoom,1);
    glTranslated(-.5,-.5,0);

    int flipx=1,flipy=1;
    switch (((int)nVideoEchoOrientation))
    {
        case 0: flipx=1;flipy=1;break;
        case 1: flipx=-1;flipy=1;break;
        case 2: flipx=1;flipy=-1;break;
        case 3: flipx=-1;flipy=-1;break;
        default: flipx=1;flipy=1; break;
    }
    glBegin(GL_QUADS);
    glTexCoord4d(0, 1,0,1); glVertex4f(-vw*.5*flipx,-vh*.5*flipy,-1,1);
    glTexCoord4d(0, 0,0,1); glVertex4f(-vw*.5*flipx,  vh*.5*flipy,-1,1);
    glTexCoord4d(1, 0,0,1); glVertex4f(vw*.5*flipx,  vh*.5*flipy,-1,1);
    glTexCoord4d(1, 1,0,1); glVertex4f(vw*.5*flipx, -vh*.5*flipy,-1,1);
    glEnd();

    glDisable(GL_TEXTURE_2D);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

    // if (bDarken==1) { glAccum(GL_ACCUM,fVideoEchoAlpha); glAccum(GL_RETURN,1);}


    if (bInvert==1)
    {
        glColor4f(1.0, 1.0, 1.0,1.0);
        glBlendFunc(GL_ONE_MINUS_DST_COLOR,GL_ZERO);
        glBegin(GL_QUADS);
        glVertex4f(-vw*.5*flipx,-vh*.5*flipy,-1,1);
        glVertex4f(-vw*.5*flipx,  vh*.5*flipy,-1,1);
        glVertex4f(vw*.5*flipx,  vh*.5*flipy,-1,1);
        glVertex4f(vw*.5*flipx, -vh*.5*flipy,-1,1);
        glEnd();
        glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    }

    //  glTranslated(.5,.5,0);
    //  glScaled(1/fVideoEchoZoom,1/fVideoEchoZoom,1);
    //   glTranslated(-.5,-.5,0);
    //glTranslatef(0,.5*vh,0);

    //per_pixel monitor
    //glBlendFunc(GL_ONE_MINUS_DST_COLOR,GL_ZERO);
    int x,y;
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glPushMatrix();
    glTranslatef(.25*vw, -.25*vh, 0);
    glScalef(.5,.5,1);
    glColor4f(1,1,1,.6);

    for (x=0;x<gx;x++)
    {
        glBegin(GL_LINE_STRIP);
        for(y=0;y<gy;y++)
        {
            glVertex4f((x_mesh[x][y]-.5)* vw, (y_mesh[x][y]-.5)*vh,-1,1);
            //glVertex4f((origx[x+1][y]-.5) * vw, (origy[x+1][y]-.5) *vh ,-1,1);
        }
        glEnd();
    }

    for (y=0;y<gy;y++)
    {
        glBegin(GL_LINE_STRIP);
        for(x=0;x<gx;x++)
        {
            glVertex4f((x_mesh[x][y]-.5)* vw, (y_mesh[x][y]-.5)*vh,-1,1);
            //glVertex4f((origx[x+1][y]-.5) * vw, (origy[x+1][y]-.5) *vh ,-1,1);
        }
        glEnd();
    }

    /*
       for (x=0;x<gx-1;x++){
       glBegin(GL_POINTS);
       for(y=0;y<gy;y++){
       glVertex4f((origx[x][y]-.5)* vw, (origy[x][y]-.5)*vh,-1,1);
       glVertex4f((origx[x+1][y]-.5) * vw, (origy[x+1][y]-.5) *vh ,-1,1);
       }
       glEnd();
       }
     */
    // glTranslated(-.5,-.5,0);     glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glPushMatrix();
    glTranslatef(-.5*vw,0, 0);


    glTranslatef(0,-vh*.10, 0);
    glBegin(GL_LINE_STRIP);
    glColor4f(0,1.0,1.0,1.0);
    glVertex3f((((totalframes%256)/551.0))*vw, treb_att*-7,-1);
    glColor4f(1.0,1.0,1.0,1.0);
    glVertex3f((((totalframes%256)/551.0))*vw,0 ,-1);
    glColor4f(.5,1.0,1.0,1.0);
    glVertex3f((((totalframes%256)/551.0))*vw, treb*7,-1);
    glEnd();

    glTranslatef(0,-vh*.13, 0);
    glBegin(GL_LINE_STRIP);
    glColor4f(0,1.0,0.0,1.0);
    glVertex3f((((totalframes%256)/551.0))*vw, mid_att*-7,-1);
    glColor4f(1.0,1.0,1.0,1.0);
    glVertex3f((((totalframes%256)/551.0))*vw,0 ,-1);
    glColor4f(.5,1.0,0.0,0.5);
    glVertex3f((((totalframes%256)/551.0))*vw, mid*7,-1);
    glEnd();

    glTranslatef(0,-vh*.13, 0);
    glBegin(GL_LINE_STRIP);
    glColor4f(1.0,0.0,0.0,1.0);
    glVertex3f((((totalframes%256)/551.0))*vw, bass_att*-7,-1);
    glColor4f(1.0,1.0,1.0,1.0);
    glVertex3f((((totalframes%256)/551.0))*vw,0 ,-1);
    glColor4f(.7,0.2,0.2,1.0);
    glVertex3f((((totalframes%256)/551.0))*vw, bass*7,-1);
    glEnd();

    glTranslatef(0,-vh*.13, 0);
    glBegin(GL_LINES);

    glColor4f(1.0,1.0,1.0,1.0);
    glVertex3f((((totalframes%256)/551.0))*vw,0 ,-1);
    glColor4f(1.0,0.6,1.0,1.0);
    glVertex3f((((totalframes%256)/551.0))*vw, vol*7,-1);
    glEnd();

    glPopMatrix();
}



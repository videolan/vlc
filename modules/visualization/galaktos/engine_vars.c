/*****************************************************************************
 * engine_vars.c:
 *****************************************************************************
 * Copyright (C) 2004 VideoLAN (Centrale Réseaux) and its contributors
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/



/* Include engine_vars.h to use these variables */
 char preset_name[256];
/* PER FRAME CONSTANTS BEGIN */
 double zoom=1.0;
 double zoomexp= 1.0;
 double rot= 0.0;
 double warp= 0.0;

 double sx= 1.0;
 double sy= 1.0;
 double dx= 0.0;
 double dy= 0.0;
 double cx= 0.5;
 double cy= 0.5;

  int gx = 32;
  int gy = 24;

 double decay=.98;

 double wave_r= 1.0;
 double wave_g= 0.2;
 double wave_b= 0.0;
 double wave_x= 0.5;
 double wave_y= 0.5;
 double wave_mystery= 0.0;

 double ob_size= 0.0;
 double ob_r= 0.0;
 double ob_g= 0.0;
 double ob_b= 0.0;
 double ob_a= 0.0;

 double ib_size = 0.0;
 double ib_r = 0.0;
 double ib_g = 0.0;
 double ib_b = 0.0;
 double ib_a = 0.0;

 double mv_a = 0.0;
 double mv_r = 0.0;
 double mv_g = 0.0;
 double mv_b = 0.0;
 double mv_l = 1.0;
 double mv_x = 16.0;
 double mv_y = 12.0;
 double mv_dy = 0.02;
 double mv_dx = 0.02;
  
 int meshx = 0;
 int meshy = 0;
 
 double Time = 0;
 double treb = 0;
 double mid = 0;
 double bass = 0;
 double treb_att = 0;
 double mid_att = 0;
 double bass_att = 0;
 double progress = 0;
 int frame = 0;
 int fps = 30;
//double bass_thresh = 0;

/* PER_FRAME CONSTANTS END */
 double fRating = 0;
 double fGammaAdj = 1.0;
 double fVideoEchoZoom = 1.0;
 double fVideoEchoAlpha = 0;
 double nVideoEchoOrientation = 0;
 
 int nWaveMode = 7;
 int bAdditiveWaves = 0;
 int bWaveDots = 0;
 int bWaveThick = 0;
 int bModWaveAlphaByVolume = 0;
 int bMaximizeWaveColor = 0;
 int bTexWrap = 0;
 int bDarkenCenter = 0;
 int bRedBlueStereo = 0;
 int bBrighten = 0;
 int bDarken = 0;
 int bSolarize = 0;
int bInvert = 0;
int bMotionVectorsOn = 1;
 
 double fWaveAlpha =1.0;
 double fWaveScale = 1.0;
 double fWaveSmoothing = 0;
 double fWaveParam = 0;
 double fModWaveAlphaStart = 0;
 double fModWaveAlphaEnd = 0;
 double fWarpAnimSpeed = 0;
 double fWarpScale = 0;
 double fShader = 0;


/* PER_PIXEL CONSTANTS BEGIN */
double x_per_pixel = 0;
double y_per_pixel = 0;
double rad_per_pixel = 0;
double ang_per_pixel = 0;

/* PER_PIXEL CONSTANT END */


/* Q AND T VARIABLES START */

double q1 = 0;
double q2 = 0;
double q3 = 0;
double q4 = 0;
double q5 = 0;
double q6 = 0;
double q7 = 0;
double q8 = 0;


/* Q AND T VARIABLES END */

//per pixel meshes
 double **zoom_mesh;
 double **zoomexp_mesh;
 double **rot_mesh;
 

 double **sx_mesh;
 double **sy_mesh;
 double **dx_mesh;
 double **dy_mesh;
 double **cx_mesh;
 double **cy_mesh;

 double **x_mesh;
 double **y_mesh;
 double **rad_mesh;
 double **theta_mesh;

//custom wave per point meshes


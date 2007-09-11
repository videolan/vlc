/* Temporay file until these variables are all externed */
#ifndef ENGINE_VARS_H
#define ENGINE_VARS_H

extern char preset_name[256];

/* PER FRAME CONSTANTS BEGIN */
extern double zoom;
extern double zoomexp;
extern double rot;
extern double warp;

extern double sx;
extern double sy;
extern double dx;
extern double dy;
extern double cx;
extern double cy;

extern int gy;
extern int gx;

extern double decay;

extern double wave_r;
extern double wave_g;
extern double wave_b;
extern double wave_x;
extern double wave_y;
extern double wave_mystery;

extern double ob_size;
extern double ob_r;
extern double ob_g;
extern double ob_b;
extern double ob_a;

extern double ib_size;
extern double ib_r;
extern double ib_g;
extern double ib_b;
extern double ib_a;

extern int meshx;
extern int meshy;

extern double mv_a ;
extern double mv_r ;
extern double mv_g ;
extern double mv_b ;
extern double mv_l;
extern double mv_x;
extern double mv_y;
extern double mv_dy;
extern double mv_dx;

extern double Time;
extern double treb ;
extern double mid ;
extern double bass ;
extern double treb_att ;
extern double mid_att ;
extern double bass_att ;
extern double progress ;
extern int frame ;

/* PER_FRAME CONSTANTS END */

/* PER_PIXEL CONSTANTS BEGIN */

extern double x_per_pixel;
extern double y_per_pixel;
extern double rad_per_pixel;
extern double ang_per_pixel;

/* PER_PIXEL CONSTANT END */


extern double fRating;
extern double fGammaAdj;
extern double fVideoEchoZoom;
extern double fVideoEchoAlpha;

extern int nVideoEchoOrientation;
extern int nWaveMode;
extern int bAdditiveWaves;
extern int bWaveDots;
extern int bWaveThick;
extern int bModWaveAlphaByVolume;
extern int bMaximizeWaveColor;
extern int bTexWrap;
extern int bDarkenCenter;
extern int bRedBlueStereo;
extern int bBrighten;
extern int bDarken;
extern int bSolarize;
extern int bInvert;
extern int bMotionVectorsOn;
extern int fps; 

extern double fWaveAlpha ;
extern double fWaveScale;
extern double fWaveSmoothing;
extern double fWaveParam;
extern double fModWaveAlphaStart;
extern double fModWaveAlphaEnd;
extern double fWarpAnimSpeed;
extern double fWarpScale;
extern double fShader;


/* Q VARIABLES START */

extern double q1;
extern double q2;
extern double q3;
extern double q4;
extern double q5;
extern double q6;
extern double q7;
extern double q8;


/* Q VARIABLES END */

extern double **zoom_mesh;
extern double **zoomexp_mesh;
extern double **rot_mesh;

extern double **sx_mesh;
extern double **sy_mesh;
extern double **dx_mesh;
extern double **dy_mesh;
extern double **cx_mesh;
extern double **cy_mesh;

extern double **x_mesh;
extern double **y_mesh;
extern double **rad_mesh;
extern double **theta_mesh;

#endif

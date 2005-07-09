/*****************************************************************************
 * PCM.c:
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/


//PCM.c - Sound data handler
//
//by Peter Sperl
//
//Takes sound data from wherever and hands it back out.
//Returns PCM Data or spectrum data, or the derivative of the PCM data

#include <stdlib.h>
#include <stdio.h>

double **PCMd;    //data structure to store PCM data  PCM[channels][maxsamples]
int maxsamples;   //size of PCM buffer
int start;        //where to add data next

int *ip;          //working space for FFT routines
double *w;        //lookup table for FFT routines
int new;          //how many new samples


//initPCM(int samples)
//
//Initializes the PCM buffer to
// number of samples specified.

void initPCM(int samples)
{
  int i; 

  //Allocate memory for PCM data buffer
  PCMd = (double **)malloc(2 * sizeof(double *));
  PCMd[0] = (double *)malloc(samples * sizeof(double));
  PCMd[1] = (double *)malloc(samples * sizeof(double));
  
  maxsamples=samples;
  new=0;

  //Initialize buffers to 0
  for (i=0;i<samples;i++)
    {
      PCMd[0][i]=0;
      PCMd[1][i]=0;
    }

  start=0;

  //Allocate FFT workspace
  w=  (double *)malloc(maxsamples*sizeof(double));
  ip= (int *)malloc(maxsamples*sizeof(int));
  ip[0]=0;
}

//The only current addPCM function, can support more
//
//Takes in a 2x512 array of PCM samples
//and stores them

void addPCM(int16_t PCMdata[2][512])
{
  int i,j;
  int samples=512;

	 for(i=0;i<samples;i++)
	   {
	     j=i+start;
	     PCMd[0][j%maxsamples]=(PCMdata[0][i]/16384.0);
	     PCMd[1][j%maxsamples]=(PCMdata[1][i]/16384.0);  
	   }
       
 
	 // printf("Added %d samples %d %d %f\n",samples,start,(start+samples)%maxsamples,PCM[0][start+10]); 

 start+=samples;
 start=start%maxsamples;

 new+=samples;
 if (new>maxsamples) new=maxsamples;
}


//puts sound data requested at provided pointer
//
//samples is number of PCM samples to return
//freq = 0 gives PCM data
//freq = 1 gives FFT data
//smoothing is the smoothing coefficient

//returned values are normalized from -1 to 1

void getPCM(double *PCMdata, int samples, int channel, int freq, double smoothing, int derive)
{
   int i,index;
   
   index=start-1;

   if (index<0) index=maxsamples+index;

   PCMdata[0]=PCMd[channel][index];
   
   for(i=1;i<samples;i++)
     {
       index=start-1-i;
       if (index<0) index=maxsamples+index;
       
       PCMdata[i]=(1-smoothing)*PCMd[channel][index]+smoothing*PCMdata[i-1];
     }
   
   //return derivative of PCM data
   if(derive)
     {
       for(i=0;i<samples-1;i++)
	 {	   
	   PCMdata[i]=PCMdata[i]-PCMdata[i+1];
	 }
       PCMdata[samples-1]=0;
     }

   //return frequency data instead of PCM (perform FFT)
   if (freq) rdft(samples, 1, PCMdata, ip, w);


     
}

//getPCMnew
//
//Like getPCM except it returns all new samples in the buffer
//the actual return value is the number of samples, up to maxsamples.
//the passed pointer, PCMData, must bee able to hold up to maxsamples

int getPCMnew(double *PCMdata, int channel, int freq, double smoothing, int derive, int reset)
{
   int i,index;
   
   index=start-1;

   if (index<0) index=maxsamples+index;

   PCMdata[0]=PCMd[channel][index];
   
   for(i=1;i<new;i++)
     {
       index=start-1-i;
       if (index<0) index=maxsamples+index;
       
       PCMdata[i]=(1-smoothing)*PCMd[channel][index]+smoothing*PCMdata[i-1];
     }
   
   //return derivative of PCM data
   if(derive)
     {
       for(i=0;i<new-1;i++)
	 {	   
	   PCMdata[i]=PCMdata[i]-PCMdata[i+1];
	 }
       PCMdata[new-1]=0;
     }

   //return frequency data instead of PCM (perform FFT)
   //   if (freq) rdft(samples, 1, PCMdata, ip, w);
   i=new;
   if (reset)  new=0;

   return i;
}

//Free stuff
void freePCM()
{
  free(PCMd[0]);
  free(PCMd[1]);
  free(PCMd);
  free(ip);
  free(w);
}

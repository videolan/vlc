/*****************************************************************************
 * effects.c : Effects for the visualization system
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: effects.c,v 1.1 2003/08/19 21:20:00 zorglub Exp $
 *
 * Authors: Clément Stenac <zorglub@via.ecp.fr>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "visual.h"
#include <math.h>


/*****************************************************************************
 * Argument list parsers                                                     *
 *****************************************************************************/
int args_getint(char *psz_parse, char * name,const int defaut)
{
    char *psz_eof;
    int i_value;
    if( psz_parse != NULL )
    {
        if(!strncmp( psz_parse, name, strlen(name) ) )
        {
            psz_parse += strlen( name );
            psz_eof = strchr( psz_parse , ',' );
            if( !psz_eof)
                psz_eof = psz_parse + strlen(psz_parse);
            if( psz_eof )
            {
                *psz_eof = '\0' ;
            }
            i_value = atoi(++psz_parse);
            psz_parse= psz_eof;
            psz_parse++;
            return i_value;
        }
    }
    return defaut;
}


char * args_getpsz(char *psz_parse, char * name,const char * defaut)                 
{
    char *psz_eof;
    char *psz_value;
    if( psz_parse != NULL )
    {
        if(!strncmp( psz_parse, name, strlen(name) ) )                            
        {
            psz_parse += strlen( name );
            psz_eof = strchr( psz_parse , ',' );
            if( !psz_eof)
                psz_eof = psz_parse + strlen(psz_parse);
            if( psz_eof )
            {
                *psz_eof = '\0' ;
            }
            psz_value = strdup(++psz_parse);
            psz_parse= psz_eof;
            psz_parse++;
            return psz_value;
        }
    }
        return strdup(defaut);
}


/*****************************************************************************
 * dummy_Run
 *****************************************************************************/
int dummy_Run( visual_effect_t * p_effect, aout_instance_t *p_aout,
               aout_buffer_t * p_buffer , picture_t * p_picture)
{
    return 0;
}

/*****************************************************************************
 * spectrum_Run: spectrum analyser
 *****************************************************************************/
int spectrum_Run(visual_effect_t * p_effect, aout_instance_t *p_aout,
                 aout_buffer_t * p_buffer , picture_t * p_picture)
{
        return 0;
}

        
/*****************************************************************************
 * scope_Run: scope effect
 *****************************************************************************/
int scope_Run(visual_effect_t * p_effect, aout_instance_t *p_aout,
              aout_buffer_t * p_buffer , picture_t * p_picture)
{
    int i_index;
    float *p_sample ;
    u8 *ppp_area[2][3];
  
    
        for( i_index = 0 ; i_index < 2 ; i_index++ )
        {
            int j;
            for( j = 0 ; j < 3 ; j++ )
            {
                ppp_area[i_index][j] =
                    p_picture->p[j].p_pixels + i_index * p_picture->p[j].i_lines
                                / 2 * p_picture->p[j].i_pitch;
            }
        }

        for( i_index = 0, p_sample = (float *)p_buffer->p_buffer;
             i_index < p_effect->i_width;
             i_index++ )
        {
            u8 i_value;

            /* Left channel */
            i_value =  (*p_sample++ +1) * 127; 
            *(ppp_area[0][0]
               + p_picture->p[0].i_pitch * i_index / p_effect->i_width
               + p_picture->p[0].i_lines * i_value / 512
                   * p_picture->p[0].i_pitch) = 0xbf;
            *(ppp_area[0][1]
                + p_picture->p[1].i_pitch * i_index / p_effect->i_width
                + p_picture->p[1].i_lines * i_value / 512
                   * p_picture->p[1].i_pitch) = 0xff;

                
           /* Right channel */
           i_value = ( *p_sample++ +1 ) * 127;
           *(ppp_area[1][0]
              + p_picture->p[0].i_pitch * i_index / p_effect->i_width
              + p_picture->p[0].i_lines * i_value / 512
                 * p_picture->p[0].i_pitch) = 0x9f;
           *(ppp_area[1][2]
              + p_picture->p[2].i_pitch * i_index / p_effect->i_width
              + p_picture->p[2].i_lines * i_value / 512
                * p_picture->p[2].i_pitch) = 0xdd;
        }
        return 0;
}


/*****************************************************************************
 * random_Run:  random plots display effect
 *****************************************************************************/
int random_Run(visual_effect_t * p_effect, aout_instance_t *p_aout,
              aout_buffer_t * p_buffer , picture_t * p_picture)
{
    int i_nb_plots;
    char *psz_parse= NULL;
    int i , i_y , i_u , i_v;
    int i_position;
    srand((unsigned int)mdate());

    if( p_effect->psz_args )
    {
        psz_parse = strdup( p_effect->psz_args );
        while(1)
        {
            i_nb_plots = args_getint ( psz_parse , "nb" , 200 );
            if(i_nb_plots) break;
            if( *psz_parse )
                 psz_parse ++;
            else
                break;
        }
    }
    else
    {
        i_nb_plots = 200;
    }

    for( i = 0 ; i < i_nb_plots ; i++ )
    {
        i_position = rand() % (p_effect->i_width * p_effect->i_height );
        i_y = rand() % 256;
        i_u = rand() % 256;
        i_v = rand() % 256;
        *(p_picture->p[0].p_pixels + i_position )= i_u;
    }
    return 0;
}

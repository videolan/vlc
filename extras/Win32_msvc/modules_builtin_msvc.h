/*****************************************************************************
 * modules_builtin.h: built-in modules list
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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

#define ALLOCATE_BUILTIN( NAME ) \
    AllocateBuiltinModule( InitModule__MODULE_ ## NAME, \
                           ActivateModule__MODULE_ ## NAME, \
                           DeactivateModule__MODULE_ ## NAME );

/* We also consider the main program as a module (useful for config stuff) */
int InitModule__MODULE_main( module_t* );
int ActivateModule__MODULE_main( module_t* );
int DeactivateModule__MODULE_main( module_t* );

/* Add stuff here */
int InitModule__MODULE_null( module_t* );
int ActivateModule__MODULE_null( module_t* );
int DeactivateModule__MODULE_null( module_t* );
int InitModule__MODULE_mpeg_es( module_t* );
int ActivateModule__MODULE_mpeg_es( module_t* );
int DeactivateModule__MODULE_mpeg_es( module_t* );
int InitModule__MODULE_mpeg_ps( module_t* );
int ActivateModule__MODULE_mpeg_ps( module_t* );
int DeactivateModule__MODULE_mpeg_ps( module_t* );
int InitModule__MODULE_mpeg_ts( module_t* );
int ActivateModule__MODULE_mpeg_ts( module_t* );
int DeactivateModule__MODULE_mpeg_ts( module_t* );
int InitModule__MODULE_file( module_t* );
int ActivateModule__MODULE_file( module_t* );
int DeactivateModule__MODULE_file( module_t* );
int InitModule__MODULE_memcpy( module_t* );
int ActivateModule__MODULE_memcpy( module_t* );
int DeactivateModule__MODULE_memcpy( module_t* );
int InitModule__MODULE_lpcm_adec( module_t* );
int ActivateModule__MODULE_lpcm_adec( module_t* );
int DeactivateModule__MODULE_lpcm_adec( module_t* );
int InitModule__MODULE_ac3_spdif( module_t* );
int ActivateModule__MODULE_ac3_spdif( module_t* );
int DeactivateModule__MODULE_ac3_spdif( module_t* );
int InitModule__MODULE_spudec( module_t* );
int ActivateModule__MODULE_spudec( module_t* );
int DeactivateModule__MODULE_spudec( module_t* );
int InitModule__MODULE_sdl( module_t* );
int ActivateModule__MODULE_sdl( module_t* );
int DeactivateModule__MODULE_sdl( module_t* );
int InitModule__MODULE_directx( module_t* );
int ActivateModule__MODULE_directx( module_t* );
int DeactivateModule__MODULE_directx( module_t* );
int InitModule__MODULE_waveout( module_t* );
int ActivateModule__MODULE_waveout( module_t* );
int DeactivateModule__MODULE_waveout( module_t* );
int InitModule__MODULE_idct( module_t* );
int ActivateModule__MODULE_idct( module_t* );
int DeactivateModule__MODULE_idct( module_t* );
int InitModule__MODULE_idctclassic( module_t* );
int ActivateModule__MODULE_idctclassic( module_t* );
int DeactivateModule__MODULE_idctclassic( module_t* );
int InitModule__MODULE_motion( module_t* );
int ActivateModule__MODULE_motion( module_t* );
int DeactivateModule__MODULE_motion( module_t* );
int InitModule__MODULE_imdct( module_t* );
int ActivateModule__MODULE_imdct( module_t* );
int DeactivateModule__MODULE_imdct( module_t* );
int InitModule__MODULE_downmix( module_t* );
int ActivateModule__MODULE_downmix( module_t* );
int DeactivateModule__MODULE_downmix( module_t* );
int InitModule__MODULE_chroma_i420_rgb( module_t* );
int ActivateModule__MODULE_chroma_i420_rgb( module_t* );
int DeactivateModule__MODULE_chroma_i420_rgb( module_t* );
int InitModule__MODULE_chroma_i420_yuy2( module_t* );
int ActivateModule__MODULE_chroma_i420_yuy2( module_t* );
int DeactivateModule__MODULE_chroma_i420_yuy2( module_t* );
int InitModule__MODULE_chroma_i422_yuy2( module_t* );
int ActivateModule__MODULE_chroma_i422_yuy2( module_t* );
int DeactivateModule__MODULE_chroma_i422_yuy2( module_t* );
int InitModule__MODULE_mpeg_adec( module_t* );
int ActivateModule__MODULE_mpeg_adec( module_t* );
int DeactivateModule__MODULE_mpeg_adec( module_t* );
int InitModule__MODULE_ac3_adec( module_t* );
int ActivateModule__MODULE_ac3_adec( module_t* );
int DeactivateModule__MODULE_ac3_adec( module_t* );
int InitModule__MODULE_dummy( module_t* );
int ActivateModule__MODULE_dummy( module_t* );
int DeactivateModule__MODULE_dummy( module_t* );
int InitModule__MODULE_mpeg_vdec( module_t* );
int ActivateModule__MODULE_mpeg_vdec( module_t* );
int DeactivateModule__MODULE_mpeg_vdec( module_t* );
int InitModule__MODULE_dvd( module_t* );
int ActivateModule__MODULE_dvd( module_t* );
int DeactivateModule__MODULE_dvd( module_t* );
int InitModule__MODULE_wall( module_t* );
int ActivateModule__MODULE_wall( module_t* );
int DeactivateModule__MODULE_wall( module_t* );

#define ALLOCATE_ALL_BUILTINS() \
    do \
    { \
        ALLOCATE_BUILTIN(null); \
        ALLOCATE_BUILTIN(mpeg_es); \
        ALLOCATE_BUILTIN(mpeg_ps); \
        ALLOCATE_BUILTIN(mpeg_ts); \
        ALLOCATE_BUILTIN(file); \
        ALLOCATE_BUILTIN(memcpy); \
        ALLOCATE_BUILTIN(lpcm_adec); \
        ALLOCATE_BUILTIN(spudec); \
        ALLOCATE_BUILTIN(directx); \
        ALLOCATE_BUILTIN(waveout); \
        ALLOCATE_BUILTIN(idct); \
        ALLOCATE_BUILTIN(idctclassic); \
        ALLOCATE_BUILTIN(motion); \
        ALLOCATE_BUILTIN(imdct); \
        ALLOCATE_BUILTIN(downmix); \
        ALLOCATE_BUILTIN(chroma_i420_rgb); \
        ALLOCATE_BUILTIN(chroma_i420_yuy2); \
        ALLOCATE_BUILTIN(chroma_i422_yuy2); \
        ALLOCATE_BUILTIN(mpeg_adec); \
        ALLOCATE_BUILTIN(ac3_adec); \
        ALLOCATE_BUILTIN(dummy); \
        ALLOCATE_BUILTIN(mpeg_vdec); \
        ALLOCATE_BUILTIN(dvd); \
        ALLOCATE_BUILTIN(wall); \
    } while( 0 );

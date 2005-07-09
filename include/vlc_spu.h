/*****************************************************************************
 * vlc_spu.h : subpicture unit
 *****************************************************************************
 * Copyright (C) 1999, 2000 the VideoLAN team
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
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

/**
 * \defgroup spu Subpicture Unit
 * This module describes the programming interface for the subpicture unit.
 * It includes functions allowing to create/destroy an spu, create/destroy
 * subpictures and render them.
 * @{
 */

/**
 * Subpicture unit descriptor
 */
struct spu_t
{
    VLC_COMMON_MEMBERS

    vlc_mutex_t  subpicture_lock;                  /**< subpicture heap lock */
    subpicture_t p_subpicture[VOUT_MAX_SUBPICTURES];        /**< subpictures */
    int i_channel;             /**< number of subpicture channels registered */

    filter_t *p_blend;                            /**< alpha blending module */
    filter_t *p_text;                              /**< text renderer module */
    filter_t *p_scale;                                   /**< scaling module */

    vlc_bool_t b_force_crop;               /**< force cropping of subpicture */
    int i_crop_x, i_crop_y, i_crop_width, i_crop_height;       /**< cropping */

    int i_margin;                        /**< force position of a subpicture */
    vlc_bool_t b_force_palette;             /**< force palette of subpicture */
    uint8_t palette[4][4];                               /**< forced palette */

    int ( *pf_control ) ( spu_t *, int, va_list );

    /* Supciture filters */
    filter_t *pp_filter[10];
    int      i_filter;
};

static inline int spu_vaControl( spu_t *p_spu, int i_query, va_list args )
{
    if( p_spu->pf_control )
        return p_spu->pf_control( p_spu, i_query, args );
    else
        return VLC_EGENERIC;
}

static inline int spu_Control( spu_t *p_spu, int i_query, ... )
{
    va_list args;
    int i_result;

    va_start( args, i_query );
    i_result = spu_vaControl( p_spu, i_query, args );
    va_end( args );
    return i_result;
}

enum spu_query_e
{
    SPU_CHANNEL_REGISTER,         /* arg1= int *   res=    */
    SPU_CHANNEL_CLEAR             /* arg1= int     res=    */
};

/**
 * \addtogroup subpicture
 * @{
 */
#define spu_Create(a) __spu_Create(VLC_OBJECT(a))
VLC_EXPORT( spu_t *, __spu_Create, ( vlc_object_t * ) );
VLC_EXPORT( int, spu_Init, ( spu_t * ) );
VLC_EXPORT( void, spu_Destroy, ( spu_t * ) );
void spu_Attach( spu_t *, vlc_object_t *, vlc_bool_t );

VLC_EXPORT( subpicture_t *, spu_CreateSubpicture, ( spu_t * ) );
VLC_EXPORT( void, spu_DestroySubpicture, ( spu_t *, subpicture_t * ) );
VLC_EXPORT( void, spu_DisplaySubpicture, ( spu_t *, subpicture_t * ) );

#define spu_CreateRegion(a,b) __spu_CreateRegion(VLC_OBJECT(a),b)
VLC_EXPORT( subpicture_region_t *,__spu_CreateRegion, ( vlc_object_t *, video_format_t * ) );
#define spu_MakeRegion(a,b,c) __spu_MakeRegion(VLC_OBJECT(a),b,c)
VLC_EXPORT( subpicture_region_t *,__spu_MakeRegion, ( vlc_object_t *, video_format_t *, picture_t * ) );
#define spu_DestroyRegion(a,b) __spu_DestroyRegion(VLC_OBJECT(a),b)
VLC_EXPORT( void, __spu_DestroyRegion, ( vlc_object_t *, subpicture_region_t * ) );

VLC_EXPORT( subpicture_t *, spu_SortSubpictures, ( spu_t *, mtime_t ) );
VLC_EXPORT( void, spu_RenderSubpictures, ( spu_t *,  video_format_t *, picture_t *, picture_t *, subpicture_t *, int, int ) );

/** @}*/
/**
 * @}
 */

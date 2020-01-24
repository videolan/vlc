/*****************************************************************************
 * mmal_cmal.h:
 *****************************************************************************
 * Copyright © 2018-2020 John Cox
 *
 * Authors: John Cox <jc@kynesim.co.uk>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef VLC_MMAL_MMAL_CMA_H_
#define VLC_MMAL_MMAL_CMA_H_


struct cma_pool_fixed_s;
typedef struct cma_pool_fixed_s cma_pool_fixed_t;

struct cma_buf_s;
typedef struct cma_buf_s cma_buf_t;

size_t cma_buf_size(const cma_buf_t * const cb);
unsigned int cma_buf_vc_handle(const cma_buf_t *const cb);
void * cma_buf_addr(const cma_buf_t *const cb);

void cma_buf_unref(cma_buf_t * const cb);
cma_buf_t * cma_buf_ref(cma_buf_t * const cb);

struct cma_buf_pool_s;
typedef struct cma_buf_pool_s cma_buf_pool_t;

cma_buf_t * cma_buf_pool_alloc_buf(cma_buf_pool_t * const p, const size_t size);
void cma_buf_pool_delete(cma_buf_pool_t * const p);
cma_buf_pool_t * cma_buf_pool_new(const unsigned int pool_size,
                                  const unsigned int flight_size,
                                  bool is_cma, const char * const name);

#endif // VLC_MMAL_MMAL_CMA_H_

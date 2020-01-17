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

void cma_buf_in_flight(cma_buf_t * const cb);
void cma_buf_end_flight(cma_buf_t * const cb);
unsigned int cma_buf_vcsm_handle(const cma_buf_t * const cb);
size_t cma_buf_size(const cma_buf_t * const cb);
int cma_buf_add_context2(cma_buf_t *const cb, picture_context_t * const ctx2);
unsigned int cma_buf_vc_handle(const cma_buf_t *const cb);
int cma_buf_fd(const cma_buf_t *const cb);
void * cma_buf_addr(const cma_buf_t *const cb);
unsigned int cma_buf_vc_addr(const cma_buf_t *const cb);
picture_context_t * cma_buf_context2(const cma_buf_t *const cb);

void cma_buf_unref(cma_buf_t * const cb);
cma_buf_t * cma_buf_ref(cma_buf_t * const cb);

struct cma_buf_pool_s;
typedef struct cma_buf_pool_s cma_buf_pool_t;

cma_buf_t * cma_buf_pool_alloc_buf(cma_buf_pool_t * const p, const size_t size);
void cma_buf_pool_cancel(cma_buf_pool_t * const cbp);
void cma_buf_pool_uncancel(cma_buf_pool_t * const cbp);
void cma_buf_pool_delete(cma_buf_pool_t * const p);
int cma_buf_pool_fill(cma_buf_pool_t * const cbp, const size_t el_size);
int cma_buf_pool_resize(cma_buf_pool_t * const cbp,
                          const unsigned int new_pool_size, const int new_flight_size);
cma_buf_pool_t * cma_buf_pool_new(const unsigned int pool_size, const unsigned int flight_size,
                                  const bool all_in_flight, const char * const name);

static inline void cma_buf_pool_deletez(cma_buf_pool_t ** const pp)
{
    cma_buf_pool_t * const p = *pp;
    if (p != NULL) {
        *pp = NULL;
        cma_buf_pool_delete(p);
    }
}

#endif // VLC_MMAL_MMAL_CMA_H_

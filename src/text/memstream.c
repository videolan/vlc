/*****************************************************************************
 * memstream.c: heap-based output streams
 *****************************************************************************
 * Copyright (C) 2016 Rémi Denis-Courmont
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_memstream.h>

#ifdef HAVE_OPEN_MEMSTREAM
int vlc_memstream_open(struct vlc_memstream *ms)
{
    ms->stream = open_memstream(&ms->ptr, &ms->length);
    return likely(ms->stream != NULL) ? 0 : EOF;
}

int vlc_memstream_flush(struct vlc_memstream *ms)
{
    if (unlikely(ms->stream == NULL))
        return EOF;

    if (ferror(ms->stream))
        return EOF;

    return fflush(ms->stream);
}

int vlc_memstream_close(struct vlc_memstream *ms)
{
    FILE *stream = ms->stream;
    int ret;

    if (unlikely(stream == NULL))
        return EOF;

    ms->stream = NULL;
    ret = ferror(stream);

    if (fclose(stream))
        return EOF;

    if (unlikely(ret))
    {
        free(ms->ptr);
        return EOF;
    }
    return 0;
} 

size_t vlc_memstream_write(struct vlc_memstream *ms, const void *ptr,
                           size_t len)
{
    if (unlikely(ms->stream == NULL))
        return 0;

    return fwrite(ptr, 1, len, ms->stream);
}

int vlc_memstream_putc(struct vlc_memstream *ms, int c)
{
    if (unlikely(ms->stream == NULL))
        return EOF;

    return fputc(c, ms->stream);
}

int (vlc_memstream_puts)(struct vlc_memstream *ms, const char *str)
{
    if (unlikely(ms->stream == NULL))
        return EOF;

    return fputs(str, ms->stream);
}

int vlc_memstream_vprintf(struct vlc_memstream *ms, const char *fmt,
                          va_list args)
{
    if (unlikely(ms->stream == NULL))
        return EOF;

    return vfprintf(ms->stream, fmt, args);
}

#else
#include <stdlib.h>

int vlc_memstream_open(struct vlc_memstream *ms)
{
    ms->error = 0;
    ms->ptr = calloc(1, 1);
    if (unlikely(ms->ptr == NULL))
        ms->error = EOF;
    ms->length = 0;
    return ms->error;
}

int vlc_memstream_flush(struct vlc_memstream *ms)
{
    return ms->error;
}

int vlc_memstream_close(struct vlc_memstream *ms)
{
    if (ms->error)
        free(ms->ptr);
    return ms->error;
} 

size_t vlc_memstream_write(struct vlc_memstream *ms, const void *ptr,
                           size_t len)
{
    size_t newlen;

    if (unlikely(add_overflow(ms->length, len, &newlen))
     || unlikely(add_overflow(newlen, 1, &newlen)))
        goto error;

    char *base = realloc(ms->ptr, newlen);
    if (unlikely(base == NULL))
        goto error;

    memcpy(base + ms->length, ptr, len);
    ms->ptr = base;
    ms->length += len;
    base[ms->length] = '\0';
    return len;

error:
    ms->error = EOF;
    return 0;
}

int vlc_memstream_putc(struct vlc_memstream *ms, int c)
{
    return (vlc_memstream_write(ms, &(unsigned char){ c }, 1u) == 1) ? c : EOF;
}

int (vlc_memstream_puts)(struct vlc_memstream *ms, const char *str)
{
    size_t len = strlen(str);
    return (vlc_memstream_write(ms, str, len) == len) ? 0 : EOF;
}

int vlc_memstream_vprintf(struct vlc_memstream *ms, const char *fmt,
                          va_list args)
{
    va_list ap;
    char *ptr;
    int len;
    size_t newlen;

    va_copy(ap, args);
    len = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);

    if (len < 0
     || unlikely(add_overflow(ms->length, len, &newlen))
     || unlikely(add_overflow(newlen, 1, &newlen)))
        goto error;

    ptr = realloc(ms->ptr, newlen);
    if (ptr == NULL)
        goto error;

    vsnprintf(ptr + ms->length, len + 1, fmt, args);
    ms->ptr = ptr;
    ms->length += len;
    return len;

error:
    ms->error = EOF;
    return EOF;
}
#endif

int vlc_memstream_printf(struct vlc_memstream *ms, const char *fmt, ...)
{
    va_list ap;
    int ret;

    va_start(ap, fmt);
    ret = vlc_memstream_vprintf(ms, fmt, ap);
    va_end(ap);
    return ret;
}

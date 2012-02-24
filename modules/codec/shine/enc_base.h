/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Base declarations for working with software encoders
 *
 * Copyright (C) 2006 Michael Sevakis
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/

#ifndef ENC_BASE_H
#define ENC_BASE_H

/* firmware/export/system.h */
/* return p incremented by specified number of bytes */
#define SKIPBYTES(p, count) ((typeof (p))((char *)(p) + (count)))

#define P2_M1(p2)  ((1 << (p2))-1)

/* align up or down to nearest 2^p2 */
#define ALIGN_DOWN_P2(n, p2) ((n) & ~P2_M1(p2))
#define ALIGN_UP_P2(n, p2)   ALIGN_DOWN_P2((n) + P2_M1(p2),p2)
/* end of firmware/export/system.h */

/** encoder config structures **/

/** aiff_enc.codec **/
struct aiff_enc_config
{
#if 0
    unsigned long sample_depth;
#endif
};

/** mp3_enc.codec **/
#define MP3_BITR_CAP_8      (1 << 0)
#define MP3_BITR_CAP_16     (1 << 1)
#define MP3_BITR_CAP_24     (1 << 2)
#define MP3_BITR_CAP_32     (1 << 3)
#define MP3_BITR_CAP_40     (1 << 4)
#define MP3_BITR_CAP_48     (1 << 5)
#define MP3_BITR_CAP_56     (1 << 6)
#define MP3_BITR_CAP_64     (1 << 7)
#define MP3_BITR_CAP_80     (1 << 8)
#define MP3_BITR_CAP_96     (1 << 9)
#define MP3_BITR_CAP_112    (1 << 10)
#define MP3_BITR_CAP_128    (1 << 11)
#define MP3_BITR_CAP_144    (1 << 12)
#define MP3_BITR_CAP_160    (1 << 13)
#define MP3_BITR_CAP_192    (1 << 14)
#define MP3_BITR_CAP_224    (1 << 15)
#define MP3_BITR_CAP_256    (1 << 16)
#define MP3_BITR_CAP_320    (1 << 17)
#define MP3_ENC_NUM_BITR    18

/* MPEG 1 */
#define MPEG1_SAMPR_CAPS    (SAMPR_CAP_32 | SAMPR_CAP_48 | SAMPR_CAP_44)
#define MPEG1_BITR_CAPS     (MP3_BITR_CAP_32  | MP3_BITR_CAP_40  | MP3_BITR_CAP_48  | \
                             MP3_BITR_CAP_56  | MP3_BITR_CAP_64  | MP3_BITR_CAP_80  | \
                             MP3_BITR_CAP_96  | MP3_BITR_CAP_112 | MP3_BITR_CAP_128 | \
                             MP3_BITR_CAP_160 | MP3_BITR_CAP_192 | MP3_BITR_CAP_224 | \
                             MP3_BITR_CAP_256 | MP3_BITR_CAP_320)

/* MPEG 2 */
#define MPEG2_SAMPR_CAPS    (SAMPR_CAP_22 | SAMPR_CAP_24 | SAMPR_CAP_16)
#define MPEG2_BITR_CAPS     (MP3_BITR_CAP_8   | MP3_BITR_CAP_16  | MP3_BITR_CAP_24  | \
                             MP3_BITR_CAP_32  | MP3_BITR_CAP_40  | MP3_BITR_CAP_48  | \
                             MP3_BITR_CAP_56  | MP3_BITR_CAP_64  | MP3_BITR_CAP_80  | \
                             MP3_BITR_CAP_96  | MP3_BITR_CAP_112 | MP3_BITR_CAP_128 | \
                             MP3_BITR_CAP_144 | MP3_BITR_CAP_160)

#if 0
/* MPEG 2.5 */
#define MPEG2_5_SAMPR_CAPS  (SAMPR_CAP_8  | SAMPR_CAP_12 | SAMPR_CAP_11)
#define MPEG2_5_BITR_CAPS   MPEG2_BITR_CAPS
#endif

#if 0
/* HAVE_MPEG* defines mainly apply to the bitrate menu */
#if (REC_SAMPR_CAPS & MPEG1_SAMPR_CAPS) || defined (HAVE_SPDIF_REC)
#define HAVE_MPEG1_SAMPR
#endif

#if (REC_SAMPR_CAPS & MPEG2_SAMPR_CAPS) || defined (HAVE_SPDIF_REC)
#define HAVE_MPEG2_SAMPR
#endif
#endif

#if 0
#if (REC_SAMPR_CAPS & MPEG2_5_SAMPR_CAPS) || defined (HAVE_SPDIF_REC)
#define HAVE_MPEG2_5_SAMPR
#endif
#endif /* 0 */

#define MP3_ENC_SAMPR_CAPS      (MPEG1_SAMPR_CAPS | MPEG2_SAMPR_CAPS)

/* This number is count of full encoder set */
#define MP3_ENC_NUM_SAMPR       6

extern const unsigned long mp3_enc_sampr[MP3_ENC_NUM_SAMPR];
extern const unsigned long mp3_enc_bitr[MP3_ENC_NUM_BITR];

struct mp3_enc_config
{
    unsigned long bitrate;
};

#define MP3_ENC_BITRATE_CFG_DEFAULT     11 /* 128 */
#define MP3_ENC_BITRATE_CFG_VALUE_LIST  "8,16,24,32,40,48,56,64,80,96," \
                                        "112,128,144,160,192,224,256,320"

/** wav_enc.codec **/
#define WAV_ENC_SAMPR_CAPS      SAMPR_CAP_ALL

struct wav_enc_config
{
#if 0
    unsigned long sample_depth;
#endif
};

/** wavpack_enc.codec **/
#define WAVPACK_ENC_SAMPR_CAPS  SAMPR_CAP_ALL

struct wavpack_enc_config
{
#if 0
    unsigned long sample_depth;
#endif
};

struct encoder_config
{
    union
    {
        /* states which *_enc_config member is valid */
        int rec_format; /* REC_FORMAT_* value */
        int afmt;       /* AFMT_* value       */
    };

    union
    {
        struct mp3_enc_config     mp3_enc;
        struct wavpack_enc_config wavpack_enc;
        struct wav_enc_config     wav_enc;
    };
};

/** Encoder chunk macros and definitions **/
#define CHUNKF_START_FILE 0x0001ul /* This chunk starts a new file         */
#define CHUNKF_END_FILE   0x0002ul /* This chunk ends the current file     */
#define CHUNKF_PRERECORD  0x0010ul /* This chunk is prerecord data,
                                      a new file could start anytime       */
#define CHUNKF_ABORT      0x0020ul /* Encoder should not finish this
                                      chunk                                */
#define CHUNKF_ERROR    (~0ul ^ (~0ul >> 1)) /* An error has occurred
                                      (passed to/from encoder). Use the
                                      sign bit to check (long)flags < 0.   */
#define CHUNKF_ALLFLAGS (0x0033ul | CHUNKF_ERROR)

/* Header at the beginning of every encoder chunk */
#ifdef DEBUG
#define ENC_CHUNK_MAGIC H_TO_BE32(('P' << 24) | ('T' << 16) | ('Y' << 8) | 'R')
#endif
struct enc_chunk_hdr
{
#ifdef DEBUG
    unsigned long id;         /* overflow detection - 'PTYR' - acronym for
                                 "PTYR Tells You Right" ;)                 */
#endif
    unsigned long flags;      /* in/out: flags used by encoder and file
                                         writing                           */
    size_t        enc_size;   /* out:    amount of encoder data written to
                                         chunk                             */
    unsigned long num_pcm;    /* out:    number of PCM samples eaten during
                                         processing
                                         (<= size of allocated buffer)     */
    unsigned char *enc_data;  /* out:    pointer to enc_size_written bytes
                                         of encoded audio data in chunk    */
    /* Encoder defined data follows header. Can be audio data + any other
       stuff the encoder needs to handle on a per chunk basis */
};

/* Paranoia: be sure header size is 4-byte aligned */
#define ENC_CHUNK_HDR_SIZE \
            ALIGN_UP_P2(sizeof (struct enc_chunk_hdr), 2)
/* Skip the chunk header and return data */
#define ENC_CHUNK_SKIP_HDR(t, hdr) \
            ((typeof (t))((char *)hdr + ENC_CHUNK_HDR_SIZE))
/* Cast p to struct enc_chunk_hdr * */
#define ENC_CHUNK_HDR(p) \
            ((struct enc_chunk_hdr *)(p))

enum enc_events
{
    /* File writing events - data points to enc_file_event_data            */
    ENC_START_FILE = 0,  /* a new file has been opened and no data has yet
                            been written                                   */
    ENC_WRITE_CHUNK,     /* write the current chunk to disk                */
    ENC_END_FILE,        /* current file about to be closed and all valid
                            data has been written                          */
    /* Encoder buffer events - data points to enc_buffer_event_data        */
    ENC_REC_NEW_STREAM,  /* Take steps to finish current stream and start
                            new                                            */
};

/**
 * encoder can write extra data to the file such as headers or more encoded
 * samples and must update sizes and samples accordingly.
 */
struct enc_file_event_data
{
    struct enc_chunk_hdr *chunk;   /* Current chunk                        */
    size_t        new_enc_size;    /* New size of chunk                    */
    unsigned long new_num_pcm;     /* New number of pcm in chunk           */
    const char   *filename;        /* filename to open if ENC_START_FILE   */
    int           rec_file;        /* Current file or < 0 if none          */
    unsigned long num_pcm_samples; /* Current pcm sample count written to
                                      file so far.                         */
};

/**
 * encoder may add some data to the end of the last and start of the next
 * but must never yield when called so any encoding done should be absolutely
 * minimal.
 */
struct enc_buffer_event_data
{
    unsigned long         flags;       /* in: One or more of:
                                        *     CHUNKF_PRERECORD
                                        *     CHUNKF_END_FILE
                                        *     CHUNKF_START_FILE
                                        */
    struct enc_chunk_hdr *pre_chunk;   /* in: pointer to first prerecord
                                        *     chunk
                                        */
    struct enc_chunk_hdr *chunk;       /* in,out: chunk were split occurs -
                                        *         first chunk of start
                                        */
};

/** Callbacks called by encoder codec **/

/* parameters passed to encoder by enc_get_inputs */
struct enc_inputs
{
    unsigned long sample_rate;     /* out - pcm frequency                  */
    int           num_channels;    /* out - number of audio channels       */
    struct encoder_config *config; /* out - encoder settings               */
};

void enc_get_inputs(struct enc_inputs *inputs);

/* parameters pass from encoder to enc_set_parameters */
struct enc_parameters
{
    /* IN parameters */
    int           afmt;            /* AFMT_* id - sanity checker           */
    size_t        chunk_size;      /* max chunk size required              */
    unsigned long enc_sample_rate; /* actual sample rate used by encoder
                                      (for recorded time calculation)      */
    size_t        reserve_bytes;   /* number of bytes to reserve immediately
                                      following chunks                     */
    void (*events_callback)(enum enc_events event,
                            void *data); /*  pointer to events callback    */
    /* OUT parameters */
    unsigned char *enc_buffer;     /* pointer to enc_buffer                */
    size_t         buf_chunk_size; /* size of chunks in enc_buffer         */
    int            num_chunks;     /* number of chunks allotted to encoder */
    unsigned char *reserve_buffer; /* pointer to reserve_bytes bytes       */
};

/* set the encoder dimensions - called by encoder codec at initialization
   and termination */
void   enc_set_parameters(struct enc_parameters *params);
/* returns pointer to next write chunk in circular buffer */
struct enc_chunk_hdr * enc_get_chunk(void);
/* releases the current chunk into the available chunks */
void   enc_finish_chunk(void);

#define PCM_MAX_FEED_SIZE       20000 /* max pcm size passed to encoder    */

/* passes a pointer to next chunk of unprocessed wav data */
unsigned char * enc_get_pcm_data(size_t size);
/* puts some pcm data back in the queue */
size_t          enc_unget_pcm_data(size_t size);

#endif /* ENC_BASE_H */

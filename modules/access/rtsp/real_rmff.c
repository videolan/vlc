/*
 * Copyright (C) 2002-2003 the xine project
 *
 * This file is part of xine, a free video player.
 *
 * xine is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * xine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 *
 * $Id$
 *
 * functions for real media file format
 * adopted from joschkas real tools
 */

#include "real.h"

#define BE_16(x)  ((((uint8_t*)(x))[0] << 8) | ((uint8_t*)(x))[1])
#define BE_32(x)  ((((uint8_t*)(x))[0] << 24) | \
                   (((uint8_t*)(x))[1] << 16) | \
                   (((uint8_t*)(x))[2] << 8) | \
                    ((uint8_t*)(x))[3])

/*
 * writes header data to a buffer
 */

static int rmff_dump_fileheader(rmff_fileheader_t *fileheader, uint8_t *buffer, int bufsize) {
    if (!fileheader) return 0;
    if (bufsize < RMFF_FILEHEADER_SIZE)
        return -1;

    fileheader->object_id=BE_32(&fileheader->object_id);
    fileheader->size=BE_32(&fileheader->size);
    fileheader->object_version=BE_16(&fileheader->object_version);
    fileheader->file_version=BE_32(&fileheader->file_version);
    fileheader->num_headers=BE_32(&fileheader->num_headers);

    memcpy(buffer, fileheader, 8);
    memcpy(&buffer[8], &fileheader->object_version, 2);
    memcpy(&buffer[10], &fileheader->file_version, 8);

    fileheader->size=BE_32(&fileheader->size);
    fileheader->object_version=BE_16(&fileheader->object_version);
    fileheader->file_version=BE_32(&fileheader->file_version);
    fileheader->num_headers=BE_32(&fileheader->num_headers);
    fileheader->object_id=BE_32(&fileheader->object_id);

    return RMFF_FILEHEADER_SIZE;
}

static int rmff_dump_prop(rmff_prop_t *prop, uint8_t *buffer, int bufsize) {

    if (!prop) return 0;
    
    if (bufsize < RMFF_PROPHEADER_SIZE)
        return -1;

    prop->object_id=BE_32(&prop->object_id);
    prop->size=BE_32(&prop->size);
    prop->object_version=BE_16(&prop->object_version);
    prop->max_bit_rate=BE_32(&prop->max_bit_rate);
    prop->avg_bit_rate=BE_32(&prop->avg_bit_rate);
    prop->max_packet_size=BE_32(&prop->max_packet_size);
    prop->avg_packet_size=BE_32(&prop->avg_packet_size);
    prop->num_packets=BE_32(&prop->num_packets);
    prop->duration=BE_32(&prop->duration);
    prop->preroll=BE_32(&prop->preroll);
    prop->index_offset=BE_32(&prop->index_offset);
    prop->data_offset=BE_32(&prop->data_offset);
    prop->num_streams=BE_16(&prop->num_streams);
    prop->flags=BE_16(&prop->flags);

    memcpy(buffer, prop, 8);
    memcpy(&buffer[8], &prop->object_version, 2);
    memcpy(&buffer[10], &prop->max_bit_rate, 36);
    memcpy(&buffer[46], &prop->num_streams, 2);
    memcpy(&buffer[48], &prop->flags, 2);

    prop->size=BE_32(&prop->size);
    prop->object_version=BE_16(&prop->object_version);
    prop->max_bit_rate=BE_32(&prop->max_bit_rate);
    prop->avg_bit_rate=BE_32(&prop->avg_bit_rate);
    prop->max_packet_size=BE_32(&prop->max_packet_size);
    prop->avg_packet_size=BE_32(&prop->avg_packet_size);
    prop->num_packets=BE_32(&prop->num_packets);
    prop->duration=BE_32(&prop->duration);
    prop->preroll=BE_32(&prop->preroll);
    prop->index_offset=BE_32(&prop->index_offset);
    prop->data_offset=BE_32(&prop->data_offset);
    prop->num_streams=BE_16(&prop->num_streams);
    prop->flags=BE_16(&prop->flags);
    prop->object_id=BE_32(&prop->object_id);

    return RMFF_PROPHEADER_SIZE;
}

static int rmff_dump_mdpr(rmff_mdpr_t *mdpr, uint8_t *buffer, int bufsize) {

    int s1, s2, s3;

    if (!mdpr) return 0;
    if (bufsize < RMFF_MDPRHEADER_SIZE + mdpr->type_specific_len +
            mdpr->stream_name_size + mdpr->mime_type_size)
    return -1;

    mdpr->object_id=BE_32(&mdpr->object_id);
    mdpr->size=BE_32(&mdpr->size);
    mdpr->object_version=BE_16(&mdpr->object_version);
    mdpr->stream_number=BE_16(&mdpr->stream_number);
    mdpr->max_bit_rate=BE_32(&mdpr->max_bit_rate);
    mdpr->avg_bit_rate=BE_32(&mdpr->avg_bit_rate);
    mdpr->max_packet_size=BE_32(&mdpr->max_packet_size);
    mdpr->avg_packet_size=BE_32(&mdpr->avg_packet_size);
    mdpr->start_time=BE_32(&mdpr->start_time);
    mdpr->preroll=BE_32(&mdpr->preroll);
    mdpr->duration=BE_32(&mdpr->duration);

    memcpy(buffer, mdpr, 8);
    memcpy(&buffer[8], &mdpr->object_version, 2);
    memcpy(&buffer[10], &mdpr->stream_number, 2);
    memcpy(&buffer[12], &mdpr->max_bit_rate, 28);
    memcpy(&buffer[40], &mdpr->stream_name_size, 1);
    s1=mdpr->stream_name_size;
    memcpy(&buffer[41], mdpr->stream_name, s1);

    memcpy(&buffer[41+s1], &mdpr->mime_type_size, 1);
    s2=mdpr->mime_type_size;
    memcpy(&buffer[42+s1], mdpr->mime_type, s2);

    mdpr->type_specific_len=BE_32(&mdpr->type_specific_len);
    memcpy(&buffer[42+s1+s2], &mdpr->type_specific_len, 4);
    mdpr->type_specific_len=BE_32(&mdpr->type_specific_len);
    s3=mdpr->type_specific_len;
    memcpy(&buffer[46+s1+s2], mdpr->type_specific_data, s3);

    mdpr->size=BE_32(&mdpr->size);
    mdpr->stream_number=BE_16(&mdpr->stream_number);
    mdpr->max_bit_rate=BE_32(&mdpr->max_bit_rate);
    mdpr->avg_bit_rate=BE_32(&mdpr->avg_bit_rate);
    mdpr->max_packet_size=BE_32(&mdpr->max_packet_size);
    mdpr->avg_packet_size=BE_32(&mdpr->avg_packet_size);
    mdpr->start_time=BE_32(&mdpr->start_time);
    mdpr->preroll=BE_32(&mdpr->preroll);
    mdpr->duration=BE_32(&mdpr->duration);
    mdpr->object_id=BE_32(&mdpr->object_id);

    return RMFF_MDPRHEADER_SIZE + s1 + s2 + s3;
}

static int rmff_dump_cont(rmff_cont_t *cont, uint8_t *buffer, int bufsize) {

    int p;

    if (!cont) return 0;
    
    if (bufsize < RMFF_CONTHEADER_SIZE + cont->title_len + cont->author_len + \
            cont->copyright_len + cont->comment_len)
        return -1;

    cont->object_id=BE_32(&cont->object_id);
    cont->size=BE_32(&cont->size);
    cont->object_version=BE_16(&cont->object_version);

    memcpy(buffer, cont, 8);
    memcpy(&buffer[8], &cont->object_version, 2);

    cont->title_len=BE_16(&cont->title_len);
    memcpy(&buffer[10], &cont->title_len, 2);
    cont->title_len=BE_16(&cont->title_len);
    memcpy(&buffer[12], cont->title, cont->title_len);
    p=12+cont->title_len;

    cont->author_len=BE_16(&cont->author_len);
    memcpy(&buffer[p], &cont->author_len, 2);
    cont->author_len=BE_16(&cont->author_len);
    memcpy(&buffer[p+2], cont->author, cont->author_len);
    p+=2+cont->author_len;

    cont->copyright_len=BE_16(&cont->copyright_len);
    memcpy(&buffer[p], &cont->copyright_len, 2);
    cont->copyright_len=BE_16(&cont->copyright_len);
    memcpy(&buffer[p+2], cont->copyright, cont->copyright_len);
    p+=2+cont->copyright_len;

    cont->comment_len=BE_16(&cont->comment_len);
    memcpy(&buffer[p], &cont->comment_len, 2);
    cont->comment_len=BE_16(&cont->comment_len);
    memcpy(&buffer[p+2], cont->comment, cont->comment_len);

    cont->size=BE_32(&cont->size);
    cont->object_version=BE_16(&cont->object_version);
    cont->object_id=BE_32(&cont->object_id);

    return RMFF_CONTHEADER_SIZE + cont->title_len + cont->author_len + \
        cont->copyright_len + cont->comment_len;
}

static int rmff_dump_dataheader(rmff_data_t *data, uint8_t *buffer, int bufsize) {

  if (!data) return 0;
  
  if (bufsize < RMFF_DATAHEADER_SIZE)
      return -1;


  data->object_id=BE_32(&data->object_id);
  data->size=BE_32(&data->size);
  data->object_version=BE_16(&data->object_version);
  data->num_packets=BE_32(&data->num_packets);
  data->next_data_header=BE_32(&data->next_data_header);

  memcpy(buffer, data, 8);
  memcpy(&buffer[8], &data->object_version, 2);
  memcpy(&buffer[10], &data->num_packets, 8);

  data->num_packets=BE_32(&data->num_packets);
  data->next_data_header=BE_32(&data->next_data_header);
  data->size=BE_32(&data->size);
  data->object_version=BE_16(&data->object_version);
  data->object_id=BE_32(&data->object_id);

  return RMFF_DATAHEADER_SIZE;
}

int rmff_dump_header(rmff_header_t *h, void *buf_gen, int max) {
    uint8_t *buffer = buf_gen;

    int written=0, size;
    rmff_mdpr_t **stream=h->streams;

    if ((size=rmff_dump_fileheader(h->fileheader, &buffer[written], max)) < 0)
        return -1;
    
    written += size;
    max -= size;

    if ((size=rmff_dump_prop(h->prop, &buffer[written], max)) < 0)
        return -1;
    
    written += size;
    max -= size;

    if ((size=rmff_dump_cont(h->cont, &buffer[written], max)) < 0)
        return -1;

    written += size;
    max -= size;

    if (stream) {
        while(*stream) {
            if ((size=rmff_dump_mdpr(*stream, &buffer[written], max)) < 0)
                return -1;
            written += size;
            max -= size;
            stream++;
        }
    }

    if ((size=rmff_dump_dataheader(h->data, &buffer[written], max)) < 0)
        return -1;
    
    written+=size;

    return written;
}

void rmff_dump_pheader(rmff_pheader_t *h, char *data) {

  data[0]=(h->object_version>>8) & 0xff;
  data[1]=h->object_version & 0xff;
  data[2]=(h->length>>8) & 0xff;
  data[3]=h->length & 0xff;
  data[4]=(h->stream_number>>8) & 0xff;
  data[5]=h->stream_number & 0xff;
  data[6]=(h->timestamp>>24) & 0xff;
  data[7]=(h->timestamp>>16) & 0xff;
  data[8]=(h->timestamp>>8) & 0xff;
  data[9]=h->timestamp & 0xff;
  data[10]=h->reserved;
  data[11]=h->flags;
}

rmff_fileheader_t *rmff_new_fileheader(uint32_t num_headers) {

  rmff_fileheader_t *fileheader = malloc(sizeof(rmff_fileheader_t));
  if( !fileheader ) return NULL;

  memset(fileheader, 0, sizeof(rmff_fileheader_t));
  fileheader->object_id=RMF_TAG;
  fileheader->size=18;
  fileheader->object_version=0;
  fileheader->file_version=0;
  fileheader->num_headers=num_headers;

  return fileheader;
}

rmff_prop_t *rmff_new_prop (
  uint32_t max_bit_rate,
  uint32_t avg_bit_rate,
  uint32_t max_packet_size,
  uint32_t avg_packet_size,
  uint32_t num_packets,
  uint32_t duration,
  uint32_t preroll,
  uint32_t index_offset,
  uint32_t data_offset,
  uint16_t num_streams,
  uint16_t flags ) {

  rmff_prop_t *prop = malloc(sizeof(rmff_prop_t));
  if( !prop ) return NULL;

  memset(prop, 0, sizeof(rmff_prop_t));
  prop->object_id=PROP_TAG;
  prop->size=50;
  prop->object_version=0;
  prop->max_bit_rate=max_bit_rate;
  prop->avg_bit_rate=avg_bit_rate;
  prop->max_packet_size=max_packet_size;
  prop->avg_packet_size=avg_packet_size;
  prop->num_packets=num_packets;
  prop->duration=duration;
  prop->preroll=preroll;
  prop->index_offset=index_offset;
  prop->data_offset=data_offset;
  prop->num_streams=num_streams;
  prop->flags=flags;

  return prop;
}

rmff_mdpr_t *rmff_new_mdpr(
  uint16_t   stream_number,
  uint32_t   max_bit_rate,
  uint32_t   avg_bit_rate,
  uint32_t   max_packet_size,
  uint32_t   avg_packet_size,
  uint32_t   start_time,
  uint32_t   preroll,
  uint32_t   duration,
  const char *stream_name,
  const char *mime_type,
  uint32_t   type_specific_len,
  const char *type_specific_data ) {

  rmff_mdpr_t *mdpr = malloc(sizeof(rmff_mdpr_t));
  if( !mdpr ) return NULL;

  memset(mdpr, 0, sizeof(rmff_mdpr_t));
  mdpr->object_id=MDPR_TAG;
  mdpr->object_version=0;
  mdpr->stream_number=stream_number;
  mdpr->max_bit_rate=max_bit_rate;
  mdpr->avg_bit_rate=avg_bit_rate;
  mdpr->max_packet_size=max_packet_size;
  mdpr->avg_packet_size=avg_packet_size;
  mdpr->start_time=start_time;
  mdpr->preroll=preroll;
  mdpr->duration=duration;
  mdpr->stream_name_size=0;
  if (stream_name) {
    mdpr->stream_name=strdup(stream_name);
    mdpr->stream_name_size=strlen(stream_name);
  }
  mdpr->mime_type_size=0;
  if (mime_type) {
    mdpr->mime_type=strdup(mime_type);
    mdpr->mime_type_size=strlen(mime_type);
  }
  mdpr->type_specific_len=type_specific_len;

  mdpr->type_specific_data = malloc(sizeof(char)*type_specific_len);
  if( !mdpr->type_specific_data ) {
    free( mdpr->stream_name );
    free( mdpr );
    return NULL;
  }
  memcpy(mdpr->type_specific_data,type_specific_data,type_specific_len);
  mdpr->mlti_data=NULL;
  mdpr->size=mdpr->stream_name_size+mdpr->mime_type_size+mdpr->type_specific_len+46;
  return mdpr;
}

rmff_cont_t *rmff_new_cont(const char *title, const char *author, const char *copyright, const char *comment) {

  rmff_cont_t *cont = malloc(sizeof(rmff_cont_t));
  if( !cont ) return NULL;

  memset(cont, 0, sizeof(rmff_cont_t));
  cont->object_id=CONT_TAG;
  cont->object_version=0;
  cont->title=NULL;
  cont->author=NULL;
  cont->copyright=NULL;
  cont->comment=NULL;
  cont->title_len=0;
  cont->author_len=0;
  cont->copyright_len=0;
  cont->comment_len=0;

  if (title) {
    cont->title_len=strlen(title);
    cont->title=strdup(title);
  }
  if (author)
  {
    cont->author_len=strlen(author);
    cont->author=strdup(author);
  }
  if (copyright) {
    cont->copyright_len=strlen(copyright);
    cont->copyright=strdup(copyright);
  }
  if (comment) {
    cont->comment_len=strlen(comment);
    cont->comment=strdup(comment);
  }
  cont->size=cont->title_len+cont->author_len+cont->copyright_len+cont->comment_len+18;
  return cont;
}

rmff_data_t *rmff_new_dataheader(uint32_t num_packets, uint32_t next_data_header) {
  rmff_data_t *data = malloc(sizeof(rmff_data_t));
  if( !data ) return NULL;

  memset(data, 0, sizeof(rmff_data_t));
  data->object_id=DATA_TAG;
  data->size=18;
  data->object_version=0;
  data->num_packets=num_packets;
  data->next_data_header=next_data_header;

  return data;
}

void rmff_print_header(rmff_header_t *h) {
  rmff_mdpr_t **stream;

  if(!h) {
    printf("rmff_print_header: NULL given\n");
    return;
  }
  if(h->fileheader) {
    printf("\nFILE:\n");
    printf("file version      : %d\n", h->fileheader->file_version);
    printf("number of headers : %d\n", h->fileheader->num_headers);
  }
  if(h->cont) {
    printf("\nCONTENT:\n");
    printf("title     : %s\n", h->cont->title);
    printf("author    : %s\n", h->cont->author);
    printf("copyright : %s\n", h->cont->copyright);
    printf("comment   : %s\n", h->cont->comment);
  }
  if(h->prop) {
    printf("\nSTREAM PROPERTIES:\n");
    printf("bit rate (max/avg)    : %i/%i\n", h->prop->max_bit_rate, h->prop->avg_bit_rate);
    printf("packet size (max/avg) : %i/%i bytes\n", h->prop->max_packet_size, h->prop->avg_packet_size);
    printf("packets       : %i\n", h->prop->num_packets);
    printf("duration      : %i ms\n", h->prop->duration);
    printf("pre-buffer    : %i ms\n", h->prop->preroll);
    printf("index offset  : %i bytes\n", h->prop->index_offset);
    printf("data offset   : %i bytes\n", h->prop->data_offset);
    printf("media streams : %i\n", h->prop->num_streams);
    printf("flags         : ");
    if (h->prop->flags & PN_SAVE_ENABLED) printf("save_enabled ");
    if (h->prop->flags & PN_PERFECT_PLAY_ENABLED) printf("perfect_play_enabled ");
    if (h->prop->flags & PN_LIVE_BROADCAST) printf("live_broadcast ");
    printf("\n");
  }
  stream=h->streams;
  if(stream) {
    while (*stream) {
      printf("\nSTREAM %i:\n", (*stream)->stream_number);
      printf("stream name [mime type] : %s [%s]\n", (*stream)->stream_name, (*stream)->mime_type);
      printf("bit rate (max/avg)      : %i/%i\n", (*stream)->max_bit_rate, (*stream)->avg_bit_rate);
      printf("packet size (max/avg)   : %i/%i bytes\n", (*stream)->max_packet_size, (*stream)->avg_packet_size);
      printf("start time : %i\n", (*stream)->start_time);
      printf("pre-buffer : %i ms\n", (*stream)->preroll);
      printf("duration   : %i ms\n", (*stream)->duration);
      printf("type specific data:\n");
      stream++;
    }
  }
  if(h->data) {
    printf("\nDATA:\n");
    printf("size      : %i\n", h->data->size);
    printf("packets   : %i\n", h->data->num_packets);
    printf("next DATA : 0x%08x\n", h->data->next_data_header);
  }
}

void rmff_fix_header(rmff_header_t *h) {

  unsigned int num_headers=0;
  unsigned int header_size=0;
  rmff_mdpr_t **streams;
  int num_streams=0;

  if (!h) {
    lprintf("rmff_fix_header: fatal: no header given.\n");
    return;
  }
  if (!h->streams) {
    lprintf("rmff_fix_header: warning: no MDPR chunks\n");
  } else {
    streams=h->streams;
    while (*streams) {
        num_streams++;
        num_headers++;
        header_size+=(*streams)->size;
        streams++;
    }
  }
  if (h->prop) {
    if (h->prop->size != 50) {
      lprintf("rmff_fix_header: correcting prop.size from %i to %i\n", h->prop->size, 50);
      h->prop->size=50;
    }
    if (h->prop->num_streams != num_streams) {
      lprintf("rmff_fix_header: correcting prop.num_streams from %i to %i\n", h->prop->num_streams, num_streams);
      h->prop->num_streams=num_streams;
    }
    num_headers++;
    header_size+=50;
  } else lprintf("rmff_fix_header: warning: no PROP chunk.\n");

  if (h->cont) {
    num_headers++;
    header_size+=h->cont->size;
  } else lprintf("rmff_fix_header: warning: no CONT chunk.\n");

  if (!h->data) {
    lprintf("rmff_fix_header: no DATA chunk, creating one\n");
    h->data = malloc(sizeof(rmff_data_t));
    if( h->data ) {
      memset(h->data, 0, sizeof(rmff_data_t));
      h->data->object_id=DATA_TAG;
      h->data->object_version=0;
      h->data->size=18;
      h->data->num_packets=0;
      h->data->next_data_header=0;
    }
  }
  num_headers++;

  if (!h->fileheader) {
    lprintf("rmff_fix_header: no fileheader, creating one");
    h->fileheader = malloc(sizeof(rmff_fileheader_t));
    if( h->fileheader ) {
      memset(h->fileheader, 0, sizeof(rmff_fileheader_t));
      h->fileheader->object_id=RMF_TAG;
      h->fileheader->size=18;
      h->fileheader->object_version=0;
      h->fileheader->file_version=0;
      h->fileheader->num_headers=num_headers+1;
    }
  }
  header_size+=h->fileheader->size;
  num_headers++;

  if(h->fileheader->num_headers != num_headers) {
    lprintf("rmff_fix_header: setting num_headers from %i to %i\n", h->fileheader->num_headers, num_headers);
    h->fileheader->num_headers=num_headers;
  }
  if(h->prop) {
    if (h->prop->data_offset != header_size) {
      lprintf("rmff_fix_header: setting prop.data_offset from %i to %i\n", h->prop->data_offset, header_size);
      h->prop->data_offset=header_size;
    }

    /* FIXME: I doubt this is right to do this here.
     * It should belong to the demux. */
    if (h->prop->num_packets == 0) {
      int p=(int)(h->prop->avg_bit_rate/8.0*(h->prop->duration/1000.0)/h->prop->avg_packet_size);
      lprintf("rmff_fix_header: assuming prop.num_packets=%i\n", p);
      h->prop->num_packets=p;
    }
    if (h->data->num_packets == 0) {
      lprintf("rmff_fix_header: assuming data.num_packets=%i\n", h->prop->num_packets);
      h->data->num_packets=h->prop->num_packets;
    }
    if (h->data->size == 18 || !h->data->size ) {
      lprintf("rmff_fix_header: assuming data.size=%i\n", h->prop->num_packets*h->prop->avg_packet_size);
      h->data->size+=h->prop->num_packets*h->prop->avg_packet_size;
    }
  }
}

int rmff_get_header_size(rmff_header_t *h) {
  if (!h) return 0;
  if (!h->prop) return -1;
  return h->prop->data_offset+18;
}

void rmff_free_header(rmff_header_t *h)
{
  if (!h) return;

  free( h->fileheader );
  free( h->prop );
  free( h->data );
  if( h->cont ) {
    free( h->cont->title );
    free( h->cont->author );
    free( h->cont->copyright );
    free( h->cont->comment );
    free( h->cont );
  }
  if (h->streams) {
    rmff_mdpr_t **s=h->streams;

    while(*s) {
      free((*s)->stream_name);
      free((*s)->mime_type);
      free((*s)->type_specific_data);
      free(*s);
      s++;
    }
    free(h->streams);
  }
  free(h);
}

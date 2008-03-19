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
 * sdp/sdpplin parser.
 *
 */
 
#include "real.h"
#define BUFLEN 32000

/*
 * Decodes base64 strings (based upon b64 package)
 */

static char *b64_decode(const char *in, char *out, int *size) {

  char dtable[256];              /* Encode / decode table */
  int i,k;
  unsigned int j;

  for (i = 0; i < 255; i++) {
    dtable[i] = 0x80;
  }
  for (i = 'A'; i <= 'Z'; i++) {
    dtable[i] = 0 + (i - 'A');
  }
  for (i = 'a'; i <= 'z'; i++) {
    dtable[i] = 26 + (i - 'a');
  }
  for (i = '0'; i <= '9'; i++) {
    dtable[i] = 52 + (i - '0');
  }
  dtable['+'] = 62;
  dtable['/'] = 63;
  dtable['='] = 0;

  k=0;
  /*CONSTANTCONDITION*/
  for (j=0; j<strlen(in); j+=4) {
    char a[4], b[4];

    for (i = 0; i < 4; i++) {
      int c = in[i+j];

      if (dtable[c] & 0x80) {
        printf("Illegal character '%c' in input.\n", c);
        exit(1);
      }
      a[i] = (char) c;
      b[i] = (char) dtable[c];
    }
    //xine_buffer_ensure_size(out, k+3);
    out[k++] = (b[0] << 2) | (b[1] >> 4);
    out[k++] = (b[1] << 4) | (b[2] >> 2);
    out[k++] = (b[2] << 6) | b[3];
    i = a[2] == '=' ? 1 : (a[3] == '=' ? 2 : 3);
    if (i < 3) {
      out[k]=0;
      *size=k;
      return out;
    }
  }
  out[k]=0;
  *size=k;
  return out;
}

static char *nl(char *data) {

  char *nlptr = (data) ? strchr(data,'\n') : NULL;
  return (nlptr) ? nlptr + 1 : NULL;
}

static int filter(const char *in, const char *filter, char **out, size_t outlen) {

  int flen=strlen(filter);
  size_t len;

  if (!in) return 0;

  len = (strchr(in,'\n')) ? (size_t)(strchr(in,'\n')-in) : strlen(in);
  if (!strncmp(in,filter,flen)) {
    if(in[flen]=='"') flen++;
    if(in[len-1]==13) len--;
    if(in[len-1]=='"') len--;
    if( len-flen+1 > outlen )
    {
        printf("Discarding end of string to avoid overflow");
        len=outlen+flen-1;
    }
    memcpy(*out, in+flen, len-flen+1);
    (*out)[len-flen]=0;
    return len-flen;
  }
  return 0;
}

static sdpplin_stream_t *sdpplin_parse_stream(char **data) {

  sdpplin_stream_t *desc = malloc(sizeof(sdpplin_stream_t));
  char      *buf = malloc(BUFLEN);
  char      *decoded = malloc(BUFLEN);
  int       handled;

  if( !desc ) return NULL;
  memset(desc, 0, sizeof(sdpplin_stream_t));

  if( !buf ) goto error;
  if( !decoded ) goto error;

  if (filter(*data, "m=", &buf, BUFLEN)) {
    desc->id = strdup(buf);
  } else {
    lprintf("sdpplin: no m= found.\n");
    goto error;
  }
  *data=nl(*data);

  while (*data && **data && *data[0]!='m') {
    handled=0;

    if(filter(*data,"a=control:streamid=",&buf, BUFLEN)) {
        /* This way negative values are mapped to unfeasibly high
         * values, and will be discarded afterward
         */
        unsigned long tmp = strtoul(buf, NULL, 10);
        if ( tmp > UINT16_MAX )
            lprintf("stream id out of bound: %lu\n", tmp);
        else
            desc->stream_id=tmp;
        handled=1;
        *data=nl(*data);
    }
    if(filter(*data,"a=MaxBitRate:integer;",&buf, BUFLEN)) {
      desc->max_bit_rate=atoi(buf);
      if (!desc->avg_bit_rate)
        desc->avg_bit_rate=desc->max_bit_rate;
      handled=1;
      *data=nl(*data);
    }
    if(filter(*data,"a=MaxPacketSize:integer;",&buf, BUFLEN)) {
      desc->max_packet_size=atoi(buf);
      if (!desc->avg_packet_size)
        desc->avg_packet_size=desc->max_packet_size;
      handled=1;
      *data=nl(*data);
    }
    if(filter(*data,"a=StartTime:integer;",&buf, BUFLEN)) {
      desc->start_time=atoi(buf);
      handled=1;
      *data=nl(*data);
    }
    if(filter(*data,"a=Preroll:integer;",&buf, BUFLEN)) {
      desc->preroll=atoi(buf);
      handled=1;
      *data=nl(*data);
    }
    if(filter(*data,"a=length:npt=",&buf, BUFLEN)) {
      desc->duration=(uint32_t)(atof(buf)*1000);
      handled=1;
      *data=nl(*data);
    }
    if(filter(*data,"a=StreamName:string;",&buf, BUFLEN)) {
      desc->stream_name=strdup(buf);
      desc->stream_name_size=strlen(desc->stream_name);
      handled=1;
      *data=nl(*data);
    }
    if(filter(*data,"a=mimetype:string;",&buf, BUFLEN)) {
      desc->mime_type=strdup(buf);
      desc->mime_type_size=strlen(desc->mime_type);
      handled=1;
      *data=nl(*data);
    }
    if(filter(*data,"a=OpaqueData:buffer;",&buf, BUFLEN)) {
      decoded = b64_decode(buf, decoded, &(desc->mlti_data_size));
      if ( decoded != NULL ) {
          desc->mlti_data = malloc(sizeof(char)*desc->mlti_data_size);
          memcpy(desc->mlti_data, decoded, desc->mlti_data_size);
          handled=1;
          *data=nl(*data);
          lprintf("mlti_data_size: %i\n", desc->mlti_data_size);
      }
    }
    if(filter(*data,"a=ASMRuleBook:string;",&buf, BUFLEN)) {
      desc->asm_rule_book=strdup(buf);
      handled=1;
      *data=nl(*data);
    }

    if(!handled) {
#ifdef LOG
      int len=strchr(*data,'\n')-(*data);
      memcpy(buf, *data, len+1);
      buf[len]=0;
      printf("libreal: sdpplin: not handled: '%s'\n", buf);
#endif
      *data=nl(*data);
    }
  }
  free( buf );
  free( decoded) ;
  return desc;

error:
  free( decoded );
  free( desc );
  free( buf );
  return NULL;
}

sdpplin_t *sdpplin_parse(char *data) {

  sdpplin_t        *desc = malloc(sizeof(sdpplin_t));
  sdpplin_stream_t *stream;
  char             *buf=malloc(BUFLEN);
  char             *decoded=malloc(BUFLEN);
  int              handled;
  int              len;

  if( !desc ) return NULL;
  if( !buf ) {
    free( desc );
    return NULL;
  }
  if( !decoded ) {
    free( buf );
    free( desc );
    return NULL;
  }

  desc->stream = NULL;

  memset(desc, 0, sizeof(sdpplin_t));

  while (data && *data) {
    handled=0;

    if (filter(data, "m=", &buf, BUFLEN)) {
        if ( !desc->stream ) {
            fprintf(stderr, "sdpplin.c: stream identifier found before stream count, skipping.");
            continue;
        }
        stream=sdpplin_parse_stream(&data);
        lprintf("got data for stream id %u\n", stream->stream_id);
        if ( stream->stream_id >= desc->stream_count )
            lprintf("stream id %u is greater than stream count %u\n", stream->stream_id, desc->stream_count);
        else
            desc->stream[stream->stream_id]=stream;
        continue;
    }
    if(filter(data,"a=Title:buffer;",&buf, BUFLEN)) {
      decoded=b64_decode(buf, decoded, &len);
	  if ( decoded != NULL ) {
          desc->title=strdup(decoded);
          handled=1;
          data=nl(data);
      }
    }
    if(filter(data,"a=Author:buffer;",&buf, BUFLEN)) {
      decoded=b64_decode(buf, decoded, &len);
	  if ( decoded != NULL ) {
          desc->author=strdup(decoded);
          handled=1;
          data=nl(data);
      }
    }
    if(filter(data,"a=Copyright:buffer;",&buf, BUFLEN)) {
      decoded=b64_decode(buf, decoded, &len);
	  if ( decoded != NULL ) {
          desc->copyright=strdup(decoded);
          handled=1;
          data=nl(data);
      }
    }
    if(filter(data,"a=Abstract:buffer;",&buf, BUFLEN)) {
      decoded=b64_decode(buf, decoded, &len);
      if ( decoded != NULL ) {
           desc->abstract=strdup(decoded);
           handled=1;
           data=nl(data);
      }
    }
    if(filter(data,"a=StreamCount:integer;",&buf, BUFLEN)) {
        /* This way negative values are mapped to unfeasibly high
         * values, and will be discarded afterward
         */
        unsigned long tmp = strtoul(buf, NULL, 10);
        if ( tmp > UINT16_MAX )
            lprintf("stream count out of bound: %lu\n", tmp);
        else
            desc->stream_count = tmp;
        desc->stream = malloc(sizeof(sdpplin_stream_t*)*desc->stream_count);
        handled=1;
        data=nl(data);
    }
    if(filter(data,"a=Flags:integer;",&buf, BUFLEN)) {
      desc->flags=atoi(buf);
      handled=1;
      data=nl(data);
    }

    if(!handled) {
#ifdef LOG
      int len=strchr(data,'\n')-data;
      memcpy(buf, data, len+1);
      buf[len]=0;
      printf("libreal: sdpplin: not handled: '%s'\n", buf);
#endif
      data=nl(data);
    }
  }

  free( decoded );
  free( buf );
  return desc;
}

void sdpplin_free(sdpplin_t *description) {

  int i;

  if( !description ) return;

  for( i=0; i<description->stream_count; i++ ) {
    if( description->stream[i] ) {
      free( description->stream[i]->id );
      free( description->stream[i]->bandwidth );
      free( description->stream[i]->range );
      free( description->stream[i]->length );
      free( description->stream[i]->rtpmap );
      free( description->stream[i]->mimetype );
      free( description->stream[i]->stream_name );
      free( description->stream[i]->mime_type );
      free( description->stream[i]->mlti_data );
      free( description->stream[i]->rmff_flags );
      free( description->stream[i]->asm_rule_book );
      free( description->stream[i] );
    }
  }
  if( description->stream_count ) free( description->stream );

  free( description->owner );
  free( description->session_name );
  free( description->session_info );
  free( description->uri );
  free( description->email );
  free( description->phone );
  free( description->connection );
  free( description->bandwidth );
  free( description->title );
  free( description->author );
  free( description->copyright );
  free( description->keywords );
  free( description->asm_rule_book );
  free( description->abstract );
  free( description->range );
  free(description);
}

/*****************************************************************************
 * psi.c: PSI management
 * Manages structures containing PSI information, and affiliated decoders.
 * TODO: Fonctions d'init des structures
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 *
 * Authors: Benoît Steiner <benny@via.ecp.fr>
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
#include "defs.h"

#include <sys/types.h>                        /* on BSD, uio.h needs types.h */
#include <sys/uio.h>                                            /* "input.h" */
#include <stdlib.h>                                     /* free(), realloc() */
#include <string.h>                                               /* bzero() */
#include <netinet/in.h>                                           /* ntohs() */

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"
#include "intf_msg.h"
#include "plugins.h"
#include "debug.h"

#include "input.h"
#include "input_ctrl.h"
#include "input_psi.h"

#include "main.h"

/*
 * Precalculated 32-bits CRC table, shared by all instances of the PSI decoder
 */
boolean_t b_crc_initialised = 0;
u32 i_crc_32_table[256];

/*
 * Global configuration variable, need by AUTO_SPAWN to determine
 * the option (audio and video) passed to the VideoLAN client.
 */
#ifdef AUTO_SPAWN
//XXX?? extern program_data_t *p_main;
#endif

/*
 * Locale type definitions
 */
#define PSI_VIDEO_STREAM_DESCRIPTOR                 0x02
#define PSI_AUDIO_STREAM_DESCRIPTOR                 0x03
#define PSI_TARGET_BACKGROUND_GRID_DESCRIPTOR       0x07
#define PSI_VIDEO_WINDOW_DESCRIPTOR                 0x08

#ifdef DVB_EXTENSIONS
#define PSI_SERVICE_DESCRIPTOR                      0x48
#endif

/* That info must be stored in the version field since it contains
   unused bits */
#define PSI_UNINITIALISED                           0xFF

/*
 * Local prototypes
 */
static int input_AddPsiPID( input_thread_t *p_input, int i_pid );
static int input_DelPsiPID( input_thread_t *p_input, int i_pid );
static void DecodePgrmAssocSection( byte_t* p_pas, input_thread_t *p_input );
static void DecodePgrmMapSection( byte_t* p_pms, input_thread_t *p_input );
static void DecodeSrvDescrSection( byte_t* p_sdt, input_thread_t *p_input );
static void DecodePgrmDescriptor( byte_t* p_descr, pgrm_descriptor_t* p_pgrm );
static void DecodeESDescriptor( byte_t* p_descriptor, es_descriptor_t* p_es );

static stream_descriptor_t* AddStreamDescr( input_thread_t* p_input,
                                            u16 i_stream_id );
static void DestroyStreamDescr( input_thread_t* p_input, u16 i_stream_id );
static pgrm_descriptor_t* AddPgrmDescr( stream_descriptor_t* p_stream,
                                       u16 i_pgrm_id );
static void DestroyPgrmDescr( input_thread_t* p_input,
                              stream_descriptor_t* p_stream, u16 i_pgrm_id );
static es_descriptor_t* AddESDescr( input_thread_t* p_input,
                                    pgrm_descriptor_t* p_pgrm, u16 i_es_pid );
static void DestroyESDescr( input_thread_t* p_input, pgrm_descriptor_t* p_pgrm,
                            u16 i_es_pid);

static void BuildCrc32Table();
static int CheckCRC32( u8* p_pms, int i_size);
static boolean_t Is_known( byte_t* a_known_section, u8 i_section );
static void Set_known( byte_t* a_known_section, u8 i_section );
static void Unset_known( byte_t* a_known_section, u8 i_section );

/*****************************************************************************
 * input_PsiInit: Initialize PSI decoder
 *****************************************************************************
 * Init the structures in which the PSI decoder will put the informations it
 * got from PSI tables and request for the reception of the PAT.
 *****************************************************************************/
int input_PsiInit( input_thread_t *p_input )
{
  ASSERT(p_input);

  /* Precalculate the 32-bit CRC table if not already done ?
     FIXME: Put a lock or do that at pgrm init ?? */
  if( !b_crc_initialised )
  {
    BuildCrc32Table();
    b_crc_initialised = 1;
  }

  /* Init the structure that describes the stream we are receiving */
  AddStreamDescr( p_input, PSI_UNINITIALISED );

  /* Request for reception of the program association table */
  input_AddPsiPID( p_input, 0 );

#ifdef DVB_EXTENSIONS
  /* Request for reception of the service description table */
  input_AddPsiPID( p_input, 17 );
#endif

  return( 0 );
}

/*****************************************************************************
 * input_PsiClean: Clean PSI structures before dying
 *****************************************************************************/
int input_PsiEnd( input_thread_t *p_input )
{
  ASSERT(p_input);

  /* Stop to receive all the PSI tables associated with that program */
  /* FIXME: Not really useful ??*/

  /* Clean also descriptors for programs associated with that stream */
  /* FIXME: -> Not really useful and maybe buggy ??*/

  /* Destroy the stream description */
  DestroyStreamDescr( p_input, p_input->p_stream->i_stream_id );

  return( 0 );
}

/*****************************************************************************
 * input_PsiRead: Read the table of programs
 *****************************************************************************
 * Ugly debugging function at that time ? XXX??
 *****************************************************************************/
void input_PsiRead( input_thread_t *p_input /* XXX?? */ )
{
  int i_index;
  int i_index2;
  pgrm_descriptor_t* p_pgrm;

  ASSERT( p_input );

  /* Lock the tables, since this method can be called from any thread */
  //vlc_mutex_lock()

  /* Check if the table is complete or not */
  if( !p_input->p_stream->b_is_PMT_complete )
  {
    intf_IntfMsg( "Warning: PMT not yet complete\n" );
  }

  /* Read the table */
  for( i_index = 0; i_index < p_input->p_stream->i_pgrm_number; i_index++ )
  {
    p_pgrm = p_input->p_stream->ap_programs[i_index];
    intf_DbgMsg("Printing info for program %d\n", p_pgrm->i_number );
    intf_IntfMsg("Printing info for program %d\n", p_pgrm->i_number );

    for( i_index2 = 0; i_index2 < p_pgrm->i_es_number; i_index2++ )
    {
      intf_DbgMsg( " ->Pid %d: type %d, PCR: %d, PSI: %d\n",
                   p_pgrm->ap_es[i_index2]->i_id,
                   p_pgrm->ap_es[i_index2]->i_type,
                   p_pgrm->ap_es[i_index2]->b_pcr,
                   p_pgrm->ap_es[i_index2]->b_psi);

      intf_IntfMsg( " ->Pid %d: type %d, PCR: %d, PSI: %d\n",
                    p_pgrm->ap_es[i_index2]->i_id,
                    p_pgrm->ap_es[i_index2]->i_type,
                    p_pgrm->ap_es[i_index2]->b_pcr,
                    p_pgrm->ap_es[i_index2]->b_psi);
    }
  }

  /* Unock the tables */
  //vlc_mutex_unlock()
}

/*****************************************************************************
 * input_PsiDecode: Decode a PSI section
 *****************************************************************************
 * This funtion is essentially a wrapper that will  perform basic checks on
 * the section and then call the right function according to its type.
 *****************************************************************************/
void input_PsiDecode( input_thread_t *p_input, psi_section_t* p_psi_section )
{
  ASSERT(p_input);
  ASSERT(p_psi_section);

  /* Hexa dump of the beginning of the section (for real men) */
  //intf_DbgMsg( "Section: %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x\n", (u8)p_psi_section->buffer[0], (u8)p_psi_section->buffer[1], (u8)p_psi_section->buffer[2], (u8)p_psi_section->buffer[3], (u8)p_psi_section->buffer[4], (u8)p_psi_section->buffer[5], (u8)p_psi_section->buffer[6], (u8)p_psi_section->buffer[7], (u8)p_psi_section->buffer[8], (u8)p_psi_section->buffer[9], (u8)p_psi_section->buffer[10], (u8)p_psi_section->buffer[11], (u8)p_psi_section->buffer[12], (u8)p_psi_section->buffer[13], (u8)p_psi_section->buffer[14], (u8)p_psi_section->buffer[15], (u8)p_psi_section->buffer[16], (u8)p_psi_section->buffer[17], (u8)p_psi_section->buffer[18], (u8)p_psi_section->buffer[19] );

  /* Check the CRC validity if any CRC is carried */
#if 0
  if( p_psi_section->buffer[1] & 0x80 )
  {
    if( CheckCRC32 (p_psi_section->buffer, p_psi_section->i_length) )
    {
      intf_DbgMsg("iSize: %d, CRC: %d\n", p_psi_section->i_length,
                  U32_AT(&p_psi_section->buffer[p_psi_section->i_length-4]));
      intf_DbgMsg( "Invalid CRC for PSI\n" );
      return;
    }
  }
#endif

  /* If the section is not immediatly applicable, trash it (DVB drafts disallow
     transmission of such sections, so we didn't implement it) */
  if( !p_psi_section->buffer[5] & 0x01 )
  {
    intf_DbgMsg( "PSI not yet applicable: trash it\n" );
    return;
  }

  /* Handle the packet according to it's type given it the table_id  */
  switch ( p_psi_section->buffer[0] )
  {
    case 0x00:
      //intf_DbgMsg("Program association section received\n");
      DecodePgrmAssocSection(p_psi_section->buffer, p_input);
      break;
    case 0x01:
      //intf_DbgMsg("Conditional access section received\n");
      break;
    case 0x02:
      //intf_DbgMsg("Program map section received\n");
      DecodePgrmMapSection(p_psi_section->buffer, p_input);
      break;
    case 0x42:
      //intf_DbgMsg("Service description section received\n");
      DecodeSrvDescrSection(p_psi_section->buffer, p_input);
      break;
    default:
      //intf_DbgMsg("Private PSI data received (type %x), ignoring it\n",
      //            p_psi_section->buffer[0]);
  }
}

/*****************************************************************************
 * DecodeAssocSection: Decode a PAS
 *****************************************************************************
 * No check is made to known if the table is currently applicable or not, so
 * that unapplicable sections must be filtered before calling this function
 * The Program Association Table can be segmented to occupy multiple sections
 * so that we have to know which sections we have already received (IsKnown() /
 * SetKnown() calls)
 *****************************************************************************/
static void DecodePgrmAssocSection(u8* p_pas, input_thread_t *p_input )
{
    u8 i_stream_id;            /* Id of the stream described in that section */
    u8 i_version;             /* Version of the table carried in the section */

    u16 i_pgrm_id;                      /* Id of the current described  pgrm */
    u16 i_pgrm_map_pid;           /* PID of the associated program map table */
    int i_pgrm_number;        /* Number of programs described in the section */

    boolean_t b_is_invalid = 0;

    u8 i_current_section;
    u8 i_last_section;

    int i_pgrm_index;
    int i_es_index;

    ASSERT(p_pas);
    ASSERT(p_input);

#define p_descr (p_input->p_stream)

    /* Read stream id and version number immediately, to be sure they will be
       initialised in all cases we will need it */
    i_stream_id = U16_AT(&p_pas[3]);
    i_version = (p_pas[5] >> 1) & 0x1F;
    //intf_DbgMsg("TS Id: %d, version: %d\n", U16_AT(&p_pas[3]),(p_pas[5] >> 1) & 0x1F);

    /* Test if the stream has not changed by looking at the stream_id */
    if( p_descr->i_stream_id != i_stream_id )
    {
        /* This can either mean that the PSI decoder has just started or that
           the stream has changed */
        if( p_descr->i_PAT_version== PSI_UNINITIALISED )
            intf_DbgMsg("Building Program Association table\n");
        else
            intf_ErrMsg( "Stream Id has suddenly changed ! Rebuilding PAT\n" );

        /* Whatever it is, ask the PSI decoder to rebuild the table */
        b_is_invalid = 1;
    }
    else
    {
        /* Stream has not changed, test if the PMT is up to date */
        if( p_descr->i_PAT_version != i_version )
        {
            intf_DbgMsg("PAT has been updated, rebuilding it\n");
            /* Ask the PSI decoder to rebuild the table */
            b_is_invalid = 1;
        }
    }

    /* Clear the table if needed */
    if( b_is_invalid )
    {
        intf_DbgMsg("Updating PAT table\n");

        /* Any program in the stream may have disapeared, or a new one
           can have been added. The good way to handle such a case would be
           to build a temporary table and to make a diff */

        /* Stop the reception of all programs and PSI informations
           associated with this stream, excepted the PAT on PID 0 and the SDT
           on PID 17 */
        for( i_es_index = 0; i_es_index < INPUT_MAX_SELECTED_ES &&
             p_input->pp_selected_es[i_es_index]; i_es_index++ )
        {
          if( p_input->pp_selected_es[i_es_index]->b_psi )
          {
            if( p_input->pp_selected_es[i_es_index]->i_id != 0
#ifdef DVB_EXTENSIONS
                && p_input->pp_selected_es[i_es_index]->i_id != 17
#endif
            )
              input_DelPsiPID( p_input,
                               p_input->pp_selected_es[i_es_index]->i_id );
          }
          else
            input_DelPgrmElem( p_input,
                               p_input->pp_selected_es[i_es_index]->i_id );
        }

        /* Recreate a new stream description. Since it is virgin, the decoder
           will rebuild it entirely on is own */
        DestroyStreamDescr(p_input, p_descr->i_stream_id);
        AddStreamDescr(p_input, i_stream_id);

        /* Record the new version for that table */
        p_descr->i_PAT_version = i_version;
    }

    /* Build the table if not already complete or if it was cleared */
    if( p_descr->b_is_PAT_complete )
    {
        /* Nothing to do */
        if( b_is_invalid )
            intf_DbgMsg("Bug: table invalid but PAT said to be complete\n");
    }
    else
    {
        /* Check if we already heard about that section */
        i_last_section = p_pas[7];
        i_current_section = p_pas[6];

//        intf_DbgMsg( "Section %d (last section %d)\n",
//                     i_current_section, i_last_section );

        if( Is_known(p_descr->a_known_PAT_sections, i_current_section) )
        {
          /* Nothing to do */
//          intf_DbgMsg("Section already received, skipping\n");
        }
        else
        {
          /* Compute the number of program_map PID carried in this section */
          i_pgrm_number = ((U16_AT(&p_pas[1]) & 0xFFF) - 9) / 4;
          intf_DbgMsg("Number of Pgrm in that section: %d\n", i_pgrm_number);

          /* Start the reception of those program map PID */
          for( i_pgrm_index = 0; i_pgrm_index < i_pgrm_number; i_pgrm_index++ )
          {
            i_pgrm_id = U16_AT(&p_pas[8+4*i_pgrm_index]);
            i_pgrm_map_pid = U16_AT(&p_pas[8+4*i_pgrm_index+2]) & 0x1fff;
            intf_DbgMsg("Pgrm %d described on pid %d\n", i_pgrm_id,
                        i_pgrm_map_pid);

            /* Check we are not already receiving that pid because it carries
               info for another program */
            for( i_es_index = 0; i_es_index < INPUT_MAX_ES; i_es_index++ )
            {
              if( p_input->p_es[i_es_index].i_id == i_pgrm_map_pid)
              {
                intf_DbgMsg("Already receiving pid %d", i_pgrm_map_pid);
                i_es_index = INPUT_MAX_ES+1;
                break;
              }
            }
            /* Start to receive that PID if we're not already doing it */
            if( i_es_index <= INPUT_MAX_ES )
              input_AddPsiPID( p_input, i_pgrm_map_pid );

            /* Append an entry to the pgrm_descriptor table to later store
               the description of this program, unless program number is 0
               (Network information table) */
            if( i_pgrm_id != 0 )
            {
              intf_DbgMsg("Adding program %d to the PMT\n", i_pgrm_id);
              AddPgrmDescr(p_descr, i_pgrm_id);
            }
          }

          /* We now know the info carried in this section */
          Set_known(p_descr->a_known_PAT_sections, i_current_section);

          /* Check if the table is now complete */
          p_descr->i_known_PAT_sections++;
          if( p_descr->i_known_PAT_sections >= i_last_section)
            p_descr->b_is_PAT_complete = 1;
        }
    }

#undef p_descr
}

/*****************************************************************************
 * DecodePgrmMapSection: Decode a PMS
 *****************************************************************************
 * No check is made to known if the table is currently applicable or not, so
 * that unapplicable sections must be filtered before calling this function
 * The Program Map Table can be segmented to occupy multiple sections so that
 * we have to know which sections we have already received (IsKnown() /
 * SetKnown() calls)
 * Note that the processing of those sections is different from the one of the
 * others since here a section refers to a single program, and a program cannot
 * be segmented into multiple sections
 *****************************************************************************/
static void DecodePgrmMapSection( u8* p_pms, input_thread_t* p_input )
{
  u16 i_pgrm_number;          /* Id of the program described in that section */
  u8 i_version;               /* Version of the description for that program */

  u16 i_offset;
  u16 i_section_length;
  u16 i_descr_end;

  u8 i_last_section;
  u8 i_current_section;

  u16 i_es_pid;

  int i_index = 0;
#ifdef AUTO_SPAWN
  int i_es_loop;
#endif
  pgrm_descriptor_t* p_pgrm;
  es_descriptor_t* p_es;

#define p_descr (p_input->p_stream)

  /* Read the id of the program described in that section */
  i_pgrm_number = U16_AT(&p_pms[3]);
//  intf_DbgMsg( "PMT section received for program %d\n", i_pgrm_number );

  /* Find where is stored the description of this program */
  for( i_index = 0; i_index < p_descr->i_pgrm_number &&
       i_pgrm_number != p_descr->ap_programs[i_index]->i_number; i_index++ );

  if( i_index >= p_descr->i_pgrm_number )
  {
    /* If none entry exists, this simply means that the PAT is not complete,
       so skip this section since it is the responsability of the PAT decoder
       to add pgrm_descriptor slots to the table of known pgrms */
    intf_DbgMsg( "Unknown pgrm %d: skipping its description\n", i_pgrm_number );
    return;
  }

  /* We now have the slot which is the program described: we can begin with
     the decoding of its description */
  p_pgrm = p_descr->ap_programs[i_index];

  /* Which section of the description of that program did we receive ? */
  i_last_section = p_pms[7];
  i_current_section = p_pms[6];
//  intf_DbgMsg("Section %d (last section %d)\n", i_current_section, i_last_section);

  /* Is this an update of the description for this program ? */
  i_version = (p_pms[5] >> 1) & 0x1F;
    if( p_pgrm->i_version != i_version )
    {
        intf_DbgMsg("Updating PMT for program %d\n", i_pgrm_number);

        for( i_index = 0; i_index < p_pgrm->i_es_number; i_index++ )
        {
          /* Stop the reception of the ES if needed by calling the function
             normally used by the interface to manage this */
          if( input_IsElemRecv(p_input, p_pgrm->ap_es[i_index]->i_id) )
          {
            intf_DbgMsg( "PID %d is no more valid: stopping its reception\n",
                      p_pgrm->ap_es[i_index]->i_id );
            input_DelPgrmElem( p_input, p_pgrm->ap_es[i_index]->i_id );
          }

          /* Remove the descriptor associated to the es of this programs */
          intf_DbgMsg( "Invalidating PID %d\n", p_pgrm->ap_es[i_index]->i_id );
          DestroyESDescr(p_input, p_pgrm, p_pgrm->ap_es[i_index]->i_id);
        }

       /* Update version number */
        p_pgrm->i_version = i_version;

        /* Ask the decoder to update the description of that program */
        p_descr->i_known_PMT_sections--;
        Unset_known( p_descr->a_known_PMT_sections, i_current_section );
        p_pgrm->b_is_ok = 0;
    }

    /* Read the info for that pgrm is the one we have is not up to date or
       if we don't have any */
    if( p_pgrm->b_is_ok )
    {
      /* Nothing to do */
//      intf_DbgMsg("Program description OK, nothing to do\n");
    }
    else
    {
        /* Check if we already heard about that section */
        if( Is_known(p_descr->a_known_PMT_sections, i_current_section) )
        {
          /* Nothing to do */
//          intf_DbgMsg("Section already received, skipping\n");
        }
        else
        {
          /* Read the corresponding PCR */
          p_pgrm->i_pcr_pid = U16_AT(&p_pms[8]) & 0x1fff;
          intf_DbgMsg("PCR at PID: %d\n", p_pgrm->i_pcr_pid);

          /* Compute the length of the section minus the final CRC */
          i_section_length = (U16_AT(&p_pms[1]) & 0xfff) + 3 - 4;
          intf_DbgMsg("Section length (without CRC): %d\n", i_section_length);

          /* Read additional info stored in the descriptors if any */
          intf_DbgMsg("Description length for program %d: %d\n",
                      p_pgrm->i_number, (U16_AT(&p_pms[10]) & 0x0fff));
          i_descr_end = (U16_AT(&p_pms[10]) & 0x0fff) + 12;
          intf_DbgMsg("description ends at offset: %d\n",  i_descr_end);

          i_offset = 12;
          while( i_offset < i_descr_end )
          {
            DecodePgrmDescriptor(&p_pms[i_offset], p_pgrm);
            i_offset += p_pms[i_offset+1] + 2;
          }

          /* Read all the ES descriptions */
          while( i_offset < i_section_length )
          {
            /* Read type of that ES */
            intf_DbgMsg("ES Type: %d\n", p_pms[i_offset]);

            /* Read PID of that ES */
            i_es_pid = U16_AT(&p_pms[i_offset+1]) & 0x1fff;
            intf_DbgMsg("ES PID: %d\n", i_es_pid);

            /* Add the ES to the program description and reserve a slot in the
               table of ES to store its description */
            p_es = AddESDescr(p_input, p_pgrm, i_es_pid);
            if (!p_es)
            {
              intf_ErrMsg("Warning: definition for pgrm %d won't be complete\n",
                          p_pgrm->i_number);
              /* The best way to handle this is to stop decoding the info for
                 that section but do as if everything is ok. Thus we will
                 eventually have an uncomplete ES table marked as being
                 complete and some error msgs */
              break;
            }
            else
            {
              /* Store the description of that PID in the slot */
              p_es->i_type = p_pms[i_offset];
              p_es->b_psi = 0;
              if( i_es_pid == p_pgrm->i_pcr_pid )
                p_es->b_pcr = 1;
              else
                p_es->b_pcr = 0;

              /* Read additional info given by the descriptors */
              i_offset += 5;
              intf_DbgMsg("description length for PID %d: %d\n", p_es->i_id,
                       (U16_AT(&p_pms[i_offset-2]) & 0x0fff));
              i_descr_end = (U16_AT(&p_pms[i_offset-2]) & 0x0fff) + i_offset;
              intf_DbgMsg("description ends at offset: %d\n",  i_descr_end);
              while( i_offset < i_descr_end )
              {
                DecodeESDescriptor(&p_pms[i_offset], p_es);
                i_offset += p_pms[i_offset+1] + 2;
              }
            }

            /* Jump to next ES description */
          }

          /* We now know the info carried in this section */
          intf_DbgMsg("Description of program %d complete\n", p_pgrm->i_number);
          p_pgrm->b_is_ok = 1;
          Set_known(p_descr->a_known_PMT_sections, i_current_section);

          /* Check if the PMT is now complete */
          p_descr->i_known_PMT_sections++;
          if( p_descr->i_known_PMT_sections >= i_last_section)
          {
              p_descr->b_is_PMT_complete = 1;

#ifdef AUTO_SPAWN
              /* Spawn an audio and a video thread, if possible. */
              for( i_es_loop = 0; i_es_loop < INPUT_MAX_ES; i_es_loop++ )
              {
                  switch( p_input->p_es[i_es_loop].i_type )
                  {
                      case MPEG1_VIDEO_ES:
                      case MPEG2_VIDEO_ES:
                          if( p_main->b_video )
                          {
                              /* Spawn a video thread */
                              input_AddPgrmElem( p_input,
                                  p_input->p_es[i_es_loop].i_id );
                          }
                          break;

                      case AC3_AUDIO_ES:
                          if ( p_main->b_audio )
                          {
                              /* Spawn an ac3 thread */
                              input_AddPgrmElem( p_input,
                                  p_input->p_es[i_es_loop].i_id );
                              }
                          break;

                      case LPCM_AUDIO_ES:
                          if ( p_main->b_audio )
                          {
                              /* Spawn an lpcm thread */
                              input_AddPgrmElem( p_input,
                                  p_input->p_es[i_es_loop].i_id );
                              }
                          break;

                     case DVD_SPU_ES:
                          if ( p_main->b_video )
                          {
                              /* Spawn a spu decoder thread */
                              input_AddPgrmElem( p_input,
                                  p_input->p_es[i_es_loop].i_id );
                              }
                          break;

                      case MPEG1_AUDIO_ES:
                      case MPEG2_AUDIO_ES:
                          if( p_main->b_audio )
                          {
                              /* Spawn an audio thread */
                              input_AddPgrmElem( p_input,
                                  p_input->p_es[i_es_loop].i_id );
                          }
                          break;

                      default:
                          break;
                  }
              }
#endif
          }
        }
    }

#undef p_descr
}

/*****************************************************************************
 * DecodeSrvDescrSection
 *****************************************************************************
 * FIXME: A finir et a refaire proprement ??
 *****************************************************************************/
void DecodeSrvDescrSection( byte_t* p_sdt, input_thread_t *p_input )
{
  u16 i_stream_id;
  u8 i_version;
  u16 i_length;

  int i_index;
  int i_offset;
  boolean_t b_must_update = 0;

  int i_descr_end;

  ASSERT(p_sdt);
  ASSERT(p_input);

#define p_stream (p_input->p_stream)

   /* Read stream id and version number immediately, to be sure they will be
      initialised in all the cases in which we will need them */
   i_stream_id = U16_AT(&p_sdt[3]);
   i_version = (p_sdt[5] >> 1) & 0x1F;
   intf_DbgMsg("TS Id: %d, version: %d\n", i_stream_id, i_version);

   /* Take the descriptor into account only if it describes the streams we are
      receiving */
   if( p_stream->i_stream_id != i_stream_id )
   {
     intf_DbgMsg("SDT doen't apply to our TS but to %s: aborting\n",
                  U16_AT(&p_sdt[3]));
   }
   else
   {
     /* Section applyies to our TS, test if the SDT is up to date */
     if( p_stream->i_SDT_version != i_version )
     {
       intf_DbgMsg("SDT has been updated, NOT YET IMPLEMENTED\n");

       /* Ask the PSI decoder to rebuild the table */
        b_must_update = 1;
     }
   }

   /* Rebuild the table if needed */
   if( b_must_update )
   {
     intf_DbgMsg("Updating SDT table\n");

     i_length = p_sdt[1] & 0x0FFF;
     i_offset = 11;

     while(i_offset < i_length)
     {

       /* Find the program to which the description applies */
       for( i_index = 0; i_index < p_stream->i_pgrm_number; i_index++ )
       {
         if( p_stream->ap_programs[i_index]->i_number ==
             U16_AT(&p_sdt[i_offset]) )
         {
           /* Here we are */
           intf_DbgMsg("FOUND: %d\n", p_stream->ap_programs[i_index]->i_number);
           break;
         }
       }

       /* Copy the info to the description of that program */
       i_offset += 5;
       intf_DbgMsg("description length for SDT: %d\n",
                   (U16_AT(&p_sdt[i_offset-2]) & 0x0FFF));
       i_descr_end = (U16_AT(&p_sdt[i_offset-2]) & 0x0FFF) + i_offset;
       intf_DbgMsg("description ends at offset: %d\n",  i_descr_end);
       while( i_offset < i_descr_end )
       {
         DecodePgrmDescriptor(&p_sdt[i_offset], p_stream->ap_programs[i_index]);
         i_offset += p_sdt[i_offset+1] + 2;
       }
     }
   }
#undef p_stream
}

/*****************************************************************************
 * DecodePgrmDescr
 *****************************************************************************
 * Decode any descriptor applying to the definition of a program
 *****************************************************************************/
static void DecodePgrmDescriptor( byte_t* p_descriptor,
                                  pgrm_descriptor_t* p_pgrm )
{
    u8 i_type;                                     /* Type of the descriptor */
    u8 i_length;                                 /* Length of the descriptor */
#ifdef DVB_EXTENSIONS
    int i_offset;                      /* Current position in the descriptor */
#endif

    ASSERT(p_descriptor);
    ASSERT(p_pgrm);

    /* Read type and length of the descriptor */
    i_type = p_descriptor[0];
    i_length = p_descriptor[1];

    /* Handle specific descriptor info */
    switch(i_type)
    {
#ifdef DVB_EXTENSIONS
    case PSI_SERVICE_DESCRIPTOR:
    {
        /* Store service type */
        p_pgrm->i_srv_type = p_descriptor[2];

        /* Jump to the beginning of the service name */
        i_offset = p_descriptor[3] + 5;

        /* Check that the charset used is the normal one (latin) by testing the
           first byte of the name */
        if( p_descriptor[i_offset] >= 0x20 )
        {
            /* The charset is the one of our computer: just dup the string */
            p_pgrm->psz_srv_name = malloc(i_length - i_offset +1);
            memcpy(p_pgrm->psz_srv_name, &p_descriptor[i_offset],
                   i_length - i_offset);
            p_pgrm->psz_srv_name[i_length - i_offset + 1] = '\0';
        }
        else
        {
            /* Indicate that the name couldn't be printed */
            p_pgrm->psz_srv_name = "Ununderstandable :)";
        }
        break;
    }
#endif
    default:
//        intf_DbgMsg("Unhandled program descriptor received (type: %d)\n", i_type);
//        intf_DbgMsg("Unhandled ES descriptor received (type: %d)\n", i_type);
    }
}

/*****************************************************************************
 * DecodeESDescriptor
 *****************************************************************************
 * Decode any descriptor applying to the definition of an ES
 *****************************************************************************/
static void DecodeESDescriptor( byte_t* p_descriptor, es_descriptor_t* p_es )
{
    u8 i_type;                                     /* Type of the descriptor */
    u8 i_length;                                 /* Length of the descriptor */
//    int i_offset;                    /* Current position in the descriptor */

    ASSERT(p_descriptor);
    ASSERT(p_es);

    /* Read type and length of the descriptor */
    i_type = p_descriptor[0];
    i_length = p_descriptor[1];

    switch( i_type )
    {
    case PSI_VIDEO_STREAM_DESCRIPTOR:
    {
        intf_DbgMsg("Video stream descriptor received\n");
        break;
    }
    case PSI_AUDIO_STREAM_DESCRIPTOR:
    {
        intf_DbgMsg("Audio stream descriptor received\n");
        break;
    }
    case PSI_TARGET_BACKGROUND_GRID_DESCRIPTOR:
    {
        intf_DbgMsg("Target background descriptor received\n");
        break;
    }
    case PSI_VIDEO_WINDOW_DESCRIPTOR:
    {
        intf_DbgMsg("Video window descriptor received\n");
        break;
    }
    default:
//        intf_DbgMsg("Unhandled ES descriptor received (type: %d)\n", i_type);
//        intf_DbgMsg("Unhandled ES descriptor received (type: %d)\n", i_type);
    }
}

/*****************************************************************************
 * input_AddPsiPID: Start to receive the PSI info contained in a PID
 *****************************************************************************
 * Add a descriptor to the table of es descriptor for that es and mark the es
 * as being to be received by the input (since all PSI must be received to
 * build the description of the program)
 *****************************************************************************/
static int input_AddPsiPID( input_thread_t *p_input, int i_pid )
{
  int i_index;
  es_descriptor_t* p_psi_es;
  int i_rc = 0;

  /* Store the description of this stream in the input thread */
  p_psi_es = AddESDescr(p_input, NULL, i_pid);

  if(p_psi_es)
  {
    /* Precise this ES carries PSI */
    p_psi_es->b_psi = 1;

    /* Create the buffer needed by the PSI decoder */
    p_psi_es->p_psi_section = malloc( sizeof( psi_section_t) );
    if( !p_psi_es->p_psi_section )
    {
      intf_ErrMsg( "Malloc error\n" );
      i_rc = -1;
    }
    else
    {
      /* Init the reception for that PID */
      p_psi_es->p_psi_section->b_running_section = 0;
//      p_psi_es->p_psi_section->b_discard_payload = 0;

      /* Ask the input thread to demultiplex it: since the interface
         can also access the table of selected es, lock the elementary
         stream structure */
      vlc_mutex_lock( &p_input->es_lock );
      for( i_index = 0; i_index < INPUT_MAX_SELECTED_ES; i_index++ )
      {
        if( !p_input->pp_selected_es[i_index] )
        {
          intf_DbgMsg( "Free Selected ES slot found at offset %d for PID %d\n",
                       i_index, i_pid );
          p_input->pp_selected_es[i_index] = p_psi_es;
          break;
        }
      }
      vlc_mutex_unlock( &p_input->es_lock );

      if( i_index >= INPUT_MAX_SELECTED_ES )
      {
        intf_ErrMsg( "Stream carries to many PID for our decoder\n" );
        i_rc = -1;
      }
    }
  }

  return( i_rc );
}

/*****************************************************************************
 * input_DelPsiPID: Stop to receive the PSI info contained in a PID
 *****************************************************************************
 * Remove the PID from the list of ES descriptors and from the list of ES that
 * the input must receive.
 * Known PID for PSI should always be received, so that their description
 * should be pointed out by a member of pp_selected_es. But as INPUT_MAX_ES
 * can be different of INPUT_MAX_SELECTED_ES, this may happen, so that we must
 * do 2 loops.
 *****************************************************************************/
static int input_DelPsiPID( input_thread_t *p_input, int i_pid )
{
  int i_es_index, i_last_sel;

  intf_DbgMsg( "Deleting PSI PID %d\n", i_pid );

  /* Stop to receive the ES. Since the interface can also access the table
     of selected es, lock the elementary stream structure */
  vlc_mutex_lock( &p_input->es_lock );

  for( i_es_index = 0; i_es_index < INPUT_MAX_SELECTED_ES; i_es_index++ )
  {
    if( p_input->pp_selected_es[i_es_index] &&
        p_input->pp_selected_es[i_es_index]->i_id == i_pid )
    {
      /* Unmark the stream */
      p_input->pp_selected_es[i_es_index] = NULL;

      /* There must not be any gap in the pp_selected_es, so move the last
         selected stream to this slot */
      for( i_last_sel = i_es_index; p_input->pp_selected_es[i_last_sel] &&
            i_last_sel < INPUT_MAX_SELECTED_ES; i_last_sel++ );
      p_input->pp_selected_es[i_es_index] = p_input->pp_selected_es[i_last_sel];
      p_input->pp_selected_es[i_last_sel] = NULL;
      break;
    }
  }

  vlc_mutex_unlock( &p_input->es_lock );

#ifdef DEBUG
  /* Check if the pp_selected_es table may be corrupted */
  if( i_es_index >= INPUT_MAX_PROGRAM_ES )
  {
    intf_ErrMsg( "DelPsiPID error: PID %d is not currently received\n", i_pid );
  }
#endif

  /* Remove the desription of that ES from the table of ES */
  DestroyESDescr(p_input, NULL, i_pid);

  return( 0 );
}

/*****************************************************************************
 * Precalculate the 32-bit CRC table
 *****************************************************************************
 * This table is a global variable shared by all decoders, so it has to be
 * initialised only once
 *****************************************************************************/
void BuildCrc32Table( )
{
    u32 i, j, k;
    for( i = 0 ; i < 256 ; i++ )
    {
        k = 0;
        for (j = (i << 24) | 0x800000 ; j != 0x80000000 ; j <<= 1)
            k = (k << 1) ^ (((k ^ j) & 0x80000000) ? 0x04c11db7 : 0);
        i_crc_32_table[i] = k;
    }
}

/*****************************************************************************
 * Test the validity of a checksum
 *****************************************************************************
 * The checksum must be stored at the end of the data, and the given size must
 * include the 32 bits of the CRC.
 * Return 0 if the checksum is OK, any other value if the data are corrupted
 *****************************************************************************/
int CheckCRC32(byte_t* p_data, int i_data_size)
{
  int i;
  u32 i_crc = 0xffffffff;

  ASSERT(p_data);

  for (i = 0; i < i_data_size; i++)
    i_crc = (i_crc << 8) ^ i_crc_32_table[(i_crc >> 24) ^ p_data[i]];

  return i_crc;
}

/*****************************************************************************
 * Is_known: check if a given section has already been received
 *****************************************************************************
 * As a table cannot be segmented into more than 256 sections, we store a 256
 * bits long table, each bit set to one indicating that the corresponding
 * saction has been received
 *****************************************************************************/
boolean_t Is_known( byte_t* a_known_section, u8 i_section )
{
  byte_t mask;
  boolean_t b_is_known;

  /* Where to get the information ? */
  int i_bit_in_byte = i_section % 8;
  int i_byte_in_table = (i_section - i_bit_in_byte) / 8;

  /* Build mask to read the Is_known flag */
  mask = 0x01 << i_bit_in_byte;

  /* Read the flag */
  b_is_known = a_known_section[i_byte_in_table] & mask;

  return b_is_known;
}

/*****************************************************************************
 * Set_known: mark a given section has having been received
 *****************************************************************************
 *
 *****************************************************************************/
static void Set_known( byte_t* a_known_section, u8 i_section )
{
  byte_t mask;

  /* Where to get the information ? */
  int i_bit_in_byte = i_section % 8;
  int i_byte_in_table = (i_section - i_bit_in_byte) / 8;

  /* Build mask to read the Is_known flag */
  mask = 0x01 << i_bit_in_byte;

  /* Set the flag */
  a_known_section[i_byte_in_table] |= mask;
}

/*****************************************************************************
 * Unset_known: remove the 'received' mark for a given section
 *****************************************************************************
 *
 *****************************************************************************/
static void Unset_known( byte_t* a_known_section, u8 i_section )
{
  byte_t mask;

  /* Where to get the information ? */
  int i_bit_in_byte = i_section % 8;
  int i_byte_in_table = (i_section - i_bit_in_byte) / 8;

  /* Build mask to read the Is_known flag */
  mask = 0x01 << i_bit_in_byte;
  mask = ~mask;

  /* Unset the flag */
  a_known_section[i_byte_in_table] &= mask;
}

/*****************************************************************************
 * AddStreamDescr: add and init the stream descriptor of the given input
 *****************************************************************************
 *
 *****************************************************************************/
static stream_descriptor_t* AddStreamDescr(input_thread_t* p_input,
                                           u16 i_stream_id)
{
  ASSERT(p_input);

  intf_DbgMsg("Adding description for stream %d\n", i_stream_id);

  p_input->p_stream = malloc( sizeof(stream_descriptor_t) );

  p_input->p_stream->i_stream_id = i_stream_id;

  p_input->p_stream->i_PAT_version = PSI_UNINITIALISED;
  p_input->p_stream->i_known_PAT_sections = 0;
  memset( p_input->p_stream->a_known_PAT_sections, 0,
          sizeof(*p_input->p_stream->a_known_PAT_sections) );
  p_input->p_stream->b_is_PAT_complete = 0;

  p_input->p_stream->i_known_PMT_sections = 0;
  memset( p_input->p_stream->a_known_PMT_sections, 0,
          sizeof(*p_input->p_stream->a_known_PMT_sections) );
  p_input->p_stream->b_is_PMT_complete = 0;

#ifdef DVB_EXTENSIONS
  p_input->p_stream->i_SDT_version = PSI_UNINITIALISED;
  p_input->p_stream->i_known_SDT_sections = 0;
  memset( p_input->p_stream->a_known_SDT_sections, 0,
         sizeof(*p_input->p_stream->a_known_SDT_sections) );
  p_input->p_stream->b_is_SDT_complete = 0;
#endif

  p_input->p_stream->i_pgrm_number = 0;
  p_input->p_stream->ap_programs = NULL;

  return p_input->p_stream;
}

/*****************************************************************************
 * DestroyStreamDescr: destroy the stream desciptor of the given input
 *****************************************************************************
 *
 *****************************************************************************/
static void DestroyStreamDescr(input_thread_t* p_input, u16 i_stream_id)
{
  int i_index;

  ASSERT(p_input);

  /* Free the structures that describes the programs of that stream */
  for( i_index = 0; i_index < p_input->p_stream->i_pgrm_number; i_index++ )
  {
    DestroyPgrmDescr( p_input, p_input->p_stream,
                      p_input->p_stream->ap_programs[i_index]->i_number );
  }

  /* Free the table of pgrm descriptors */
  free( p_input->p_stream->ap_programs );

  /* Free the structure that describes the stream itself */
  free( p_input->p_stream );

  /* Input thread has no more stream descriptor */
  p_input->p_stream = NULL;
}

/*****************************************************************************
 * AddPgrmDescr: add and init a program descriptor
 *****************************************************************************
 * This program descriptor will be referenced in the given stream descriptor
 *****************************************************************************/
static pgrm_descriptor_t* AddPgrmDescr( stream_descriptor_t* p_stream,
                                        u16 i_pgrm_id)
{
  int i_pgrm_index = p_stream->i_pgrm_number;       /* Where to add the pgrm */

  ASSERT(p_stream);

  intf_DbgMsg("Adding description for pgrm %d\n", i_pgrm_id);

  /* Add an entry to the list of program associated with the stream */
  p_stream->i_pgrm_number++;
  p_stream->ap_programs = realloc( p_stream->ap_programs,
                          p_stream->i_pgrm_number*sizeof(pgrm_descriptor_t*) );

  /* Allocate the structure to store this description */
  p_stream->ap_programs[i_pgrm_index] = malloc(sizeof(pgrm_descriptor_t));

  /* Init this entry */
  p_stream->ap_programs[i_pgrm_index]->i_number = i_pgrm_id;
  p_stream->ap_programs[i_pgrm_index]->i_version = PSI_UNINITIALISED;
  p_stream->ap_programs[i_pgrm_index]->b_is_ok = 0;

  p_stream->ap_programs[i_pgrm_index]->i_es_number = 0;
  p_stream->ap_programs[i_pgrm_index]->ap_es = NULL;

  /* descriptors ? XXX?? */

  return p_stream->ap_programs[i_pgrm_index];
}

/*****************************************************************************
 * AddPgrmDescr: destroy a program descriptor
 *****************************************************************************
 * All ES descriptions referenced in the descriptor will be deleted.
 *****************************************************************************/
static void DestroyPgrmDescr( input_thread_t * p_input,
                              stream_descriptor_t * p_stream, u16 i_pgrm_id )
{
  int i_index, i_pgrm_index = -1;
  pgrm_descriptor_t* p_pgrm = NULL;

  ASSERT( p_stream );

  intf_DbgMsg("Deleting description for pgrm %d\n", i_pgrm_id);

  /* Find where this program is described */
  for( i_index = 0; i_index < p_stream->i_pgrm_number; i_index++ )
  {
    if( p_stream->ap_programs[i_index]->i_number == i_pgrm_id )
    {
      i_pgrm_index = i_index;
      p_pgrm = p_stream->ap_programs[ i_pgrm_index ];
      break;
    }
  }

  /* Make sure that the pgrm exists */
  ASSERT(i_pgrm_index >= 0);
  ASSERT(p_pgrm);

  /* Free the structures that describe the es that belongs to that program */
  for( i_index = 0; i_index < p_pgrm->i_es_number; i_index++ )
  {
    DestroyESDescr( p_input, p_pgrm, p_pgrm->ap_es[i_index]->i_id );
  }

  /* Free the table of es descriptors */
  free( p_pgrm->ap_es );

  /* Free the description of this stream */
  free( p_pgrm );

  /* Remove this program from the stream's list of programs */
  p_stream->i_pgrm_number--;
  p_stream->ap_programs[i_pgrm_index] =
                                 p_stream->ap_programs[p_stream->i_pgrm_number];
  p_stream->ap_programs = realloc( p_stream->ap_programs,
                          p_stream->i_pgrm_number*sizeof(pgrm_descriptor_t *) );
}

/*****************************************************************************
 * AddESDescr:
 *****************************************************************************
 * Reserve a slot in the table of ES descritors for the ES and add it to the
 * list of ES of p_pgrm. If p_pgrm if NULL, then the ES is considered as stand
 * alone (PSI ?)
 *****************************************************************************/
static es_descriptor_t* AddESDescr(input_thread_t* p_input,
                                   pgrm_descriptor_t* p_pgrm, u16 i_es_pid)
{
  int i_index;
  es_descriptor_t* p_es = NULL;

  ASSERT(p_input);

  intf_DbgMsg("Adding description for ES %d\n", i_es_pid);

  /* Find an empty slot to store the description of that es */
  for( i_index = 0; i_index < INPUT_MAX_ES &&
        p_input->p_es[i_index].i_id != EMPTY_PID; i_index++ );

  if( i_index >= INPUT_MAX_ES )
  {
    /* No slot is empty */
    intf_ErrMsg("Stream carries to many PID for our decoder\n");
  }
  else
  {
    /* Reserve the slot for that ES */
    p_es = &p_input->p_es[i_index];
    p_es->i_id = i_es_pid;
    intf_DbgMsg("Slot %d in p_es table assigned to ES %d\n", i_index, i_es_pid);

    /* Init its values */
    p_es->i_type = 0;  /* XXX?? */
    p_es->b_psi = 0;
    p_es->b_pcr = 0;
    p_es->i_continuity_counter = 0xFF;

    p_es->p_pes_packet = NULL;
//    p_es->p_next_pes_packet = NULL;
    p_es->p_dec = NULL;

    /* Add this ES to the program definition if one is given */
    if( p_pgrm )
    {
      p_pgrm->i_es_number++;
      p_pgrm->ap_es = realloc( p_pgrm->ap_es,
                               p_pgrm->i_es_number*sizeof(es_descriptor_t *) );
      p_pgrm->ap_es[p_pgrm->i_es_number-1] = p_es;
      intf_DbgMsg( "Added ES %d to definition of pgrm %d\n",
                   i_es_pid, p_pgrm->i_number );
    }
    else
      intf_DbgMsg( "Added ES %d not added to the definition of any pgrm\n",
                   i_es_pid );
  }

  return p_es;
}

/*****************************************************************************
 * DestroyESDescr:
 *****************************************************************************
 *
 *****************************************************************************/
static void DestroyESDescr(input_thread_t* p_input,
                           pgrm_descriptor_t* p_pgrm, u16 i_pid)
{
  int i_index;

  /* Look for the description of the ES */
  for(i_index = 0; i_index < INPUT_MAX_ES; i_index++)
  {
    if( p_input->p_es[i_index].i_id == i_pid )
    {
      /* The table of stream descriptors is static, so don't free memory
         but just mark the slot as unused */
      p_input->p_es[i_index].i_id = EMPTY_PID;
      break;
    }
  }

  /* Remove this ES from the description of the program if it is associated to
     one */
  if( p_pgrm )
  {
    for( i_index = 0; i_index < INPUT_MAX_ES; i_index++ )
    {
      if( p_input->p_es[i_index].i_id == i_pid )
      {
        p_pgrm->i_es_number--;
        p_pgrm->ap_es[i_index] = p_pgrm->ap_es[p_pgrm->i_es_number];
        p_pgrm->ap_es = realloc(p_pgrm->ap_es, p_pgrm->i_es_number);
        break;
      }
    }
  }
}

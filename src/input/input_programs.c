/*****************************************************************************
 * input_programs.c: es_descriptor_t, pgrm_descriptor_t management
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 *
 * Authors:
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

/*****************************************************************************
 * input_InitStream: init the stream descriptor of the given input
 *****************************************************************************/
void input_InitStream( input_thread_t * p_input )
{
    p_input->stream->i_pgrm_number = 0;
    p_input->stream->pp_programs = NULL;
}

/*****************************************************************************
 * input_AddProgram: add and init a program descriptor
 *****************************************************************************
 * This program descriptor will be referenced in the given stream descriptor
 *****************************************************************************/
pgrm_descriptor_t * input_AddProgram( input_thread_t * p_input,
                                      u16 i_pgrm_id)
{
    int i_pgrm_index = p_stream->i_pgrm_number;     /* Where to add the pgrm */

    intf_DbgMsg("Adding description for pgrm %d", i_pgrm_id);

    /* Add an entry to the list of program associated with the stream */
    p_input->stream.i_pgrm_number++;
    p_input->stream.pp_programs = realloc( p_input->stream.pp_programs,
                                           p_input->stream.i_pgrm_number
                                            * sizeof(pgrm_descriptor_t *) );

    /* Allocate the structure to store this description */
    p_input->stream.pp_programs[i_pgrm_index] =
                                        malloc( sizeof(pgrm_descriptor_t) );

    /* Init this entry */
    p_input->stream.pp_programs[i_pgrm_index]->i_number = i_pgrm_id;
    p_input->stream.pp_programs[i_pgrm_index]->b_is_ok = 0;

    p_input->stream.pp_programs[i_pgrm_index]->i_es_number = 0;
    p_input->stream.pp_programs[i_pgrm_index]->pp_es = NULL;

    p_input->stream.pp_programs[i_pgrm_index]->delta_cr = 0;
    p_input->stream.pp_programs[i_pgrm_index]->delta_absolute = 0;
    p_input->stream.pp_programs[i_pgrm_index]->last_cr = 0;
    p_input->stream.pp_programs[i_pgrm_index]->c_average_count = 0;
    p_input->stream.pp_programs[i_pgrm_index]->i_synchro_state
                                                = SYNCHRO_NOT_STARTED;

    p_input->stream.pp_programs[i_pgrm_index]->p_vout
                                            = p_input->p_default_vout;
    p_input->stream.pp_programs[i_pgrm_index]->p_aout
                                            = p_input->p_default_aout;

    return p_input->stream.pp_programs[i_pgrm_index];
}

/*****************************************************************************
 * input_DelProgram: destroy a program descriptor
 *****************************************************************************
 * All ES descriptions referenced in the descriptor will be deleted.
 *****************************************************************************/
void input_DelProgram( input_thread_t * p_input, u16 i_pgrm_id )
{
    int i_index, i_pgrm_index = -1;
    pgrm_descriptor_t * p_pgrm = NULL;

    intf_DbgMsg("Deleting description for pgrm %d", i_pgrm_id);

    /* Find where this program is described */
    for( i_index = 0; i_index < p_input->stream.i_pgrm_number; i_index++ )
    {
        if( p_input->stream.pp_programs[i_index]->i_number == i_pgrm_id )
        {
            i_pgrm_index = i_index;
            p_pgrm = p_input->stream.pp_programs[ i_pgrm_index ];
            break;
        }
    }

    /* Make sure that the pgrm exists */
    ASSERT(i_pgrm_index >= 0);
    ASSERT(p_pgrm);

    /* Free the structures that describe the es that belongs to that program */
    for( i_index = 0; i_index < p_pgrm->i_es_number; i_index++ )
    {
        DestroyESDescr( p_input, p_pgrm, p_pgrm->pp_es[i_index]->i_id );
    }

    /* Free the table of es descriptors */
    free( p_pgrm->pp_es );

    /* Free the description of this stream */
    free( p_pgrm );

    /* Remove this program from the stream's list of programs */
    p_input->stream.i_pgrm_number--;
    p_input->stream.pp_programs[i_pgrm_index] =
        p_input->stream.pp_programs[p_input->stream.i_pgrm_number];
    p_input->stream.pp_programs = realloc( p_input->stream.pp_programs,
                                           p_input->stream.i_pgrm_number
                                            * sizeof(pgrm_descriptor_t *) );
}

/*****************************************************************************
 * input_AddES:
 *****************************************************************************
 * Reserve a slot in the table of ES descriptors for the ES and add it to the
 * list of ES of p_pgrm. If p_pgrm if NULL, then the ES is considered as stand
 * alone (PSI ?)
 *****************************************************************************/
es_descriptor_t * input_AddES( input_thread_t * p_input,
                               pgrm_descriptor_t * p_pgrm, u16 i_es_id)
{
    int i_index;
    es_descriptor_t * p_es = NULL;

    intf_DbgMsg("Adding description for ES %d", i_es_id);

    /* Find an empty slot to store the description of that es */
    for( i_index = 0; i_index < INPUT_MAX_ES &&
         p_input->p_es[i_index].i_id != EMPTY_ID; i_index++ );

    if( i_index >= INPUT_MAX_ES )
    {
        /* No slot is empty */
        intf_ErrMsg("Stream carries too many ES for our decoder");
    }
    else
    {
        /* Reserve the slot for that ES */
        p_es = &p_input->p_es[i_index];
        p_es->i_id = i_es_id;
        intf_DbgMsg("Slot %d in p_es table assigned to ES %d",
                    i_index, i_es_pid);

        /* Init its values */
        p_es->b_discontinuity = 0;
        p_es->p_pes_packet = NULL;
        p_es->p_decoder_fifo = NULL;

        /* Add this ES to the program definition if one is given */
        if( p_pgrm )
        {
            p_pgrm->i_es_number++;
            p_pgrm->pp_es = realloc( p_pgrm->pp_es,
                                     p_pgrm->i_es_number
                                      * sizeof(es_descriptor_t *) );
            p_pgrm->pp_es[p_pgrm->i_es_number - 1] = p_es;
            p_es->p_pgrm = p_pgrm;
        }
        else
        {
            p_es->p_pgrm = NULL;
        }
    }

    return p_es;
}

/*****************************************************************************
 * input_DelES:
 *****************************************************************************/
void input_DelES( input_thread_t * p_input, u16 i_id)
{
    int                     i_index;
    program_descriptor_t *  p_pgrm;
    es_descriptor_t *       p_es;

    /* Look for the description of the ES */
    for( i_index = 0; i_index < INPUT_MAX_ES; i_index++ )
    {
        if( p_input->p_es[i_index].i_id == i_id )
        {
            p_es = &p_input->p_es[i_index];
            p_pgrm = p_input->p_es[i_index].p_pgrm;
            break;
        }
    }

    ASSERT( p_es );

    /* Remove this ES from the description of the program if it is associated to
     * one */
    if( p_pgrm )
    {
        for( i_index = 0; ; i_index++ )
        {
            if( p_pgrm->p_es[i_index].i_id == i_id )
            {
                p_pgrm->i_es_number--;
                p_pgrm->pp_es[i_index] = p_pgrm->p_es[p_pgrm->i_es_number];
                p_pgrm->pp_es = realloc( p_pgrm->pp_es,
                                         p_pgrm->i_es_number
                                          * sizeof(es_decriptor_t *));
                break;
            }
        }
    }

    /* The table of stream descriptors is static, so don't free memory
     * but just mark the slot as unused */
    p_es->i_id = EMPTY_ID;
}

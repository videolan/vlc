/*******************************************************************************
 * input_ctrl.h: Decodeur control
 * (c)1999 VideoLAN
 *******************************************************************************
 * Requires:
 *  "config.h"
 *  "common.h"
 *  "mtime.h"
 ******************************************************************************/






/******************************************************************************
 * Prototypes
 ******************************************************************************/
int input_AddPgrmElem( input_thread_t *p_input, int i_current_pid );
int input_DelPgrmElem( input_thread_t *p_input, int i_current_pid );
boolean_t input_IsElemRecv( input_thread_t *p_input, int i_pid );

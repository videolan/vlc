/*******************************************************************************
 * psi.h: PSI management interface
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
int input_PsiInit( input_thread_t *p_input );
void input_PsiDecode( input_thread_t *p_input, psi_section_t* p_psi_section );
void input_PsiRead( input_thread_t *p_input /* ??? */ );
int input_PsiClean( input_thread_t *p_input );

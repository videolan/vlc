/*******************************************************************************
 * network.h: network functions interface
 * (c)1999 VideoLAN
 *******************************************************************************/

/*
  needs :
   - <sys/uio.h>
   - <sys/socket.h>
   - <unistd.h>
   - <netinet/in.h>
   - <netdb.h>
   - <arpa/inet.h>
   - <stdio.h>
   - <sys/ioctl.h>
   - <net/if.h>
*/

/******************************************************************************
 * Prototypes
 ******************************************************************************/
int input_NetworkCreateMethod( input_thread_t *p_input,
                               input_cfg_t *p_cfg );
int input_NetworkRead( input_thread_t *p_input, const struct iovec *p_vector,
                       size_t i_count );
void input_NetworkDestroyMethod( input_thread_t *p_input );

int  imdct_init_sse (imdct_t * p_imdct);
void imdct_do_512_sse(imdct_t * p_imdct, float data[], float delay[]);
void imdct_do_512_nol_sse(imdct_t * p_imdct, float data[], float delay[]);

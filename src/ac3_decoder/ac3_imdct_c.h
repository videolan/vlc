int  imdct_init_c (imdct_t * p_imdct);
void imdct_do_256(imdct_t * p_imdct, float data[], float delay[]);
void imdct_do_256_nol(imdct_t * p_imdct, float data[], float delay[]);
void imdct_do_512_c(imdct_t * p_imdct, float data[], float delay[]);
void imdct_do_512_nol_c(imdct_t * p_imdct, float data[], float delay[]);


/* Wrappers for all the builtin functions 
   The arg_list pointer is a list of doubles. Its
   size is equal to the number of arguments the parameter
   takes */
 
inline double below_wrapper(double * arg_list);
inline double above_wrapper(double * arg_list);
inline double equal_wrapper(double * arg_list);
inline double if_wrapper(double * arg_list);
inline double bnot_wrapper(double * arg_list);
inline double rand_wrapper(double * arg_list);
inline double bor_wrapper(double * arg_list);
inline double band_wrapper(double * arg_list);
inline double sigmoid_wrapper(double * arg_list);
inline double max_wrapper(double * arg_list);
inline double min_wrapper(double * arg_list);
inline double sign_wrapper(double * arg_list);
inline double sqr_wrapper(double * arg_list);
inline double int_wrapper(double * arg_list);
inline double nchoosek_wrapper(double * arg_list);
inline double sin_wrapper(double * arg_list);
inline double cos_wrapper(double * arg_list);
inline double tan_wrapper(double * arg_list);
inline double fact_wrapper(double * arg_list);
inline double asin_wrapper(double * arg_list);
inline double acos_wrapper(double * arg_list);
inline double atan_wrapper(double * arg_list);
inline double atan2_wrapper(double * arg_list);

inline double pow_wrapper(double * arg_list);
inline double exp_wrapper(double * arg_list);
inline double abs_wrapper(double * arg_list);
inline double log_wrapper(double *arg_list);
inline double log10_wrapper(double * arg_list);
inline double sqrt_wrapper(double * arg_list);

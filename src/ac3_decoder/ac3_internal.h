/* Exponent strategy constants */
#define EXP_REUSE	(0)
#define EXP_D15		(1)
#define EXP_D25		(2)
#define EXP_D45		(3)

/* Delta bit allocation constants */
#define DELTA_BIT_REUSE		(0)
#define DELTA_BIT_NEW		(1)
#define DELTA_BIT_NONE		(2)
#define DELTA_BIT_RESERVED	(3)

/* ac3_bit_allocate.c */
void bit_allocate (ac3dec_t *);

/* ac3_downmix.c */
void downmix (ac3dec_t *, s16 *);

/* ac3_exponent.c */
int exponent_unpack (ac3dec_t *);

/* ac3_imdct.c */
void imdct (ac3dec_t * p_ac3dec);

/* ac3_mantissa.c */
void mantissa_unpack (ac3dec_t *);

/* ac3_parse.c */
int ac3_test_sync (ac3dec_t *);
void parse_syncinfo (ac3dec_t *);
void parse_bsi (ac3dec_t *);
void parse_audblk (ac3dec_t *);
void parse_auxdata (ac3dec_t *);

/* ac3_rematrix.c */
void rematrix (ac3dec_t *);

/***************************************************************
 *
 * Copyright:   (c) Copyright Motorola Inc. 1998
 *
 * Date:        April 17, 1998
 *
 * Function:    Matrix_Transpose
 *
 * Description: The following Matrix Transpose is adapted
 *              from an algorithm developed by Brett Olsson
 *              from IBM. It performs a 8x8 16-bit element
 *              full matrix transpose.
 *
 * Inputs:      array elements stored in input
 *               input[0] = [ 00 01 02 03 04 05 06 07 ]
 *               input[1] = [ 10 11 12 13 14 15 16 17 ]
 *               input[2] = [ 20 21 22 23 24 25 26 27 ]
 *               input[3] = [ 30 31 32 33 34 35 36 37 ]
 *               input[4] = [ 40 41 42 43 44 45 46 47 ]
 *               input[5] = [ 50 51 52 53 54 55 56 57 ]
 *               input[6] = [ 60 61 62 63 64 65 66 67 ]
 *               input[7] = [ 70 71 72 73 74 75 76 77 ]
 *
 * Outputs:     transposed elements in output
 *
 **************************************************************/

static __inline__ void Matrix_Transpose ( vector signed short *input,
                               vector signed short *output )
{
  vector signed short a0, a1, a2, a3, a4, a5, a6, a7;
  vector signed short b0, b1, b2, b3, b4, b5, b6, b7;

  b0 = vec_mergeh( input[0], input[4] );     /* [ 00 40 01 41 02 42 03 43 ]*/
  b1 = vec_mergel( input[0], input[4] );     /* [ 04 44 05 45 06 46 07 47 ]*/
  b2 = vec_mergeh( input[1], input[5] );     /* [ 10 50 11 51 12 52 13 53 ]*/
  b3 = vec_mergel( input[1], input[5] );     /* [ 14 54 15 55 16 56 17 57 ]*/
  b4 = vec_mergeh( input[2], input[6] );     /* [ 20 60 21 61 22 62 23 63 ]*/
  b5 = vec_mergel( input[2], input[6] );     /* [ 24 64 25 65 26 66 27 67 ]*/
  b6 = vec_mergeh( input[3], input[7] );     /* [ 30 70 31 71 32 72 33 73 ]*/
  b7 = vec_mergel( input[3], input[7] );     /* [ 34 74 35 75 36 76 37 77 ]*/

  a0 = vec_mergeh( b0, b4 );                 /* [ 00 20 40 60 01 21 41 61 ]*/
  a1 = vec_mergel( b0, b4 );                 /* [ 02 22 42 62 03 23 43 63 ]*/
  a2 = vec_mergeh( b1, b5 );                 /* [ 04 24 44 64 05 25 45 65 ]*/
  a3 = vec_mergel( b1, b5 );                 /* [ 06 26 46 66 07 27 47 67 ]*/
  a4 = vec_mergeh( b2, b6 );                 /* [ 10 30 50 70 11 31 51 71 ]*/
  a5 = vec_mergel( b2, b6 );                 /* [ 12 32 52 72 13 33 53 73 ]*/
  a6 = vec_mergeh( b3, b7 );                 /* [ 14 34 54 74 15 35 55 75 ]*/
  a7 = vec_mergel( b3, b7 );                 /* [ 16 36 56 76 17 37 57 77 ]*/

  output[0] = vec_mergeh( a0, a4 );          /* [ 00 10 20 30 40 50 60 70 ]*/
  output[1] = vec_mergel( a0, a4 );          /* [ 01 11 21 31 41 51 61 71 ]*/
  output[2] = vec_mergeh( a1, a5 );          /* [ 02 12 22 32 42 52 62 72 ]*/
  output[3] = vec_mergel( a1, a5 );          /* [ 03 13 23 33 43 53 63 73 ]*/
  output[4] = vec_mergeh( a2, a6 );          /* [ 04 14 24 34 44 54 64 74 ]*/
  output[5] = vec_mergel( a2, a6 );          /* [ 05 15 25 35 45 55 65 75 ]*/
  output[6] = vec_mergeh( a3, a7 );          /* [ 06 16 26 36 46 56 66 76 ]*/
  output[7] = vec_mergel( a3, a7 );          /* [ 07 17 27 37 47 57 67 77 ]*/

}


/***************************************************************
 *
 * Copyright:   (c) Copyright Motorola Inc. 1998
 *
 * Date:        April 20, 1998
 *
 * Macro:       IDCT_Transform
 *
 * Description: Discrete Cosign Transform implemented by the
 *              Scaled Chen (III) Algorithm developed by Haifa
 *              Research Lab.  The major difference between this
 *              algorithm and the Scaled Chen (I) is that
 *              certain multiply-subtracts are replaced by
 *              multiply adds.  A full description of the
 *              Scaled Chen (I) algorithm can be found in:
 *              W.C.Chen, C.H.Smith and S.C.Fralick, "A Fast
 *              Computational Algorithm for the Discrete Cosine
 *              Transform", IEEE Transactions on Commnuications,
 *              Vol. COM-25, No. 9, pp 1004-1009, Sept. 1997.
 *
 * Inputs:      vx     : array of vector short
 *              t1-t10 : temporary vector variables set up by caller
 *              c4     : cos(4*pi/16)
 *              mc4    : -c4
 *              a0     : c6/c2
 *              a1     : c7/c1
 *              a2     : c5/c3
 *              ma2    : -a2
 *              zero   : an array of zero elements
 *
 * Outputs:     vy     : array of vector short
 *
 **************************************************************/

#define IDCT_Transform(vx,vy) \
                                                                  \
  /* 1st stage. */                                                \
  t9 = vec_mradds( a1, vx[1], zero );  /* t8 = (a1) * x1 - x7  */ \
  t8 = vec_subs( t9, vx[7]);                                      \
  t1 = vec_mradds( a1, vx[7], vx[1] ); /* t1 = (a1) * x7 + x1  */ \
  t7 = vec_mradds( a2, vx[5], vx[3] ); /* t7 = (a2) * x5 + x3  */ \
  t3 = vec_mradds( ma2, vx[3], vx[5] );/* t3 = (-a2) * x5 + x3 */ \
                                                                  \
  /* 2nd stage */                                                 \
  t5 = vec_adds( vx[0], vx[4] );        /* t5 = x0 + x4 */        \
  t0 = vec_subs( vx[0], vx[4] );        /* t0 = x0 - x4 */        \
  t9 = vec_mradds( a0, vx[2], zero );   /* t4 = (a0) * x2 - x6 */ \
  t4 = vec_subs( t9, vx[6] );                                     \
  t2 = vec_mradds( a0, vx[6], vx[2] );  /* t2 = (a0) * x6 + x2 */ \
                                                                  \
  t6 = vec_adds( t8, t3 );              /* t6 = t8 + t3 */        \
  t3 = vec_subs( t8, t3 );              /* t3 = t8 - t3 */        \
  t8 = vec_subs( t1, t7 );              /* t8 = t1 - t7 */        \
  t1 = vec_adds( t1, t7 );              /* t1 = t1 + t7 */        \
                                                                  \
  /* 3rd stage. */                                                \
  t7 = vec_adds( t5, t2 );              /* t7 = t5 + t2 */        \
  t2 = vec_subs( t5, t2 );              /* t2 = t5 - t2 */        \
  t5 = vec_adds( t0, t4 );              /* t5 = t0 + t4 */        \
  t0 = vec_subs( t0, t4 );              /* t0 = t0 - t4 */        \
                                                                  \
  t4 = vec_subs( t8, t3 );              /* t4 = t8 - t3 */        \
  t3 = vec_adds( t8, t3 );              /* t3 = t8 + t3 */        \
                                                                  \
  /* 4th stage. */                                                \
  vy[0] = vec_adds( t7, t1 );        /* y0 = t7 + t1 */           \
  vy[7] = vec_subs( t7, t1 );        /* y7 = t7 - t1 */           \
  vy[1] = vec_mradds( c4, t3, t5 );  /* y1 = (c4) * t3 + t5  */   \
  vy[6] = vec_mradds( mc4, t3, t5 ); /* y6 = (-c4) * t3 + t5 */   \
  vy[2] = vec_mradds( c4, t4, t0 );  /* y2 = (c4) * t4 + t0  */   \
  vy[5] = vec_mradds( mc4, t4, t0 ); /* y5 = (-c4) * t4 + t0 */   \
  vy[3] = vec_adds( t2, t6 );        /* y3 = t2 + t6 */           \
  vy[4] = vec_subs( t2, t6 );        /* y4 = t2 - t6 */


/* Pre-Scaling matrix -- scaled by 1 */
static vector signed short PreScale[8] = {
    (vector signed short)( 4095, 5681, 5351, 4816, 4095, 4816, 5351, 5681 ),
    (vector signed short)( 5681, 7880, 7422, 6680, 5681, 6680, 7422, 7880 ),
    (vector signed short)( 5351, 7422, 6992, 6292, 5351, 6292, 6992, 7422 ),
    (vector signed short)( 4816, 6680, 6292, 5663, 4816, 5663, 6292, 6680 ),
    (vector signed short)( 4095, 5681, 5351, 4816, 4095, 4816, 5351, 5681 ),
    (vector signed short)( 4816, 6680, 6292, 5663, 4816, 5663, 6292, 6680 ),
    (vector signed short)( 5351, 7422, 6992, 6292, 5351, 6292, 6992, 7422 ),
    (vector signed short)( 5681, 7880, 7422, 6680, 5681, 6680, 7422, 7880 )
};

/***************************************************************
 *
 * Copyright:   (c) Copyright Motorola Inc. 1998
 *
 * Date:        April 17, 1998
 *
 * Function:    IDCT
 *
 * Description: Scaled Chen (III) algorithm for IDCT
 *              Arithmetic is 16-bit fixed point.
 *
 * Inputs:      input - Pointer to input data (short), which
 *                      must be between -2048 to +2047.
 *                      It is assumed that the allocated array
 *                      has been 128-bit aligned and contains
 *                      8x8 short elements.
 *
 * Outputs:     output - Pointer to output area for the transfored
 *                       data. The output values are between -255
 *                       and 255 . It is assumed that a 128-bit
 *                       aligned 8x8 array of short has been
 *                       pre-allocated.
 *
 * Return:      None
 *
 ***************************************************************/

static __inline__ void IDCT(short *input, short *output) {

  vector signed short t0, t1, t2, t3, t4, t5, t6, t7, t8, t9;
  vector signed short a0, a1, a2, ma2, c4, mc4, zero;
  vector signed short vx[8], vy[8];
  vector signed short *vec_ptr;  /* used for conversion between
                                    arrays of short and vector
                                    signed short array.  */


  /* Load the multiplication constants.  Note: these constants
   * could all be loaded directly ( like zero case ), but using the
   * SpecialConstants approach causes vsplth instructions to be
   * generated instead of lvx which is more efficient given the remainder
   * of the instruction mix.
   */
  vector signed short SpecialConstants =
     (vector signed short)( 23170, 13573, 6518, 21895, -23170, -21895, 0 , 0
);

  c4   = vec_splat( SpecialConstants, 0 );  /* c4 = cos(4*pi/16)  */
  a0   = vec_splat( SpecialConstants, 1 );  /* a0 = c6/c2         */
  a1   = vec_splat( SpecialConstants, 2 );  /* a1 = c7/c1         */
  a2   = vec_splat( SpecialConstants, 3 );  /* a2 = c5/c3         */
  mc4  = vec_splat( SpecialConstants, 4 );  /* -c4                */
  ma2  = vec_splat( SpecialConstants, 5 );  /* -a2                */
  zero = (vector signed short)(0);

  /* Load the rows of input data and Pre-Scale them. */
  vec_ptr = ( vector signed short * ) input;
  vx[0] = vec_mradds( vec_ptr[0], PreScale[0], zero );
  vx[1] = vec_mradds( vec_ptr[1], PreScale[1], zero );
  vx[2] = vec_mradds( vec_ptr[2], PreScale[2], zero );
  vx[3] = vec_mradds( vec_ptr[3], PreScale[3], zero );
  vx[4] = vec_mradds( vec_ptr[4], PreScale[4], zero );
  vx[5] = vec_mradds( vec_ptr[5], PreScale[5], zero );
  vx[6] = vec_mradds( vec_ptr[6], PreScale[6], zero );
  vx[7] = vec_mradds( vec_ptr[7], PreScale[7], zero );

  /* Perform IDCT first on the 8 columns */
  IDCT_Transform( vx, vy );

  /* Transpose matrix to work on rows */
  Matrix_Transpose( vy, vx );

  /* Perform IDCT next on the 8 rows */
  IDCT_Transform( vx, vy );

  /* Post-scale and store result. */
  vec_ptr = (vector signed short *) output;
  vec_ptr[0] = vy[0];
  vec_ptr[1] = vy[1];
  vec_ptr[2] = vy[2];
  vec_ptr[3] = vy[3];
  vec_ptr[4] = vy[4];
  vec_ptr[5] = vy[5];
  vec_ptr[6] = vy[6];
  vec_ptr[7] = vy[7];

}


# Lossy compression algorithms very often make use of DCT or DFT calculations,
# or variations of these calculations. This file is intended to be a short
# reference about classical DCT and DFT algorithms.


import math
import cmath

pi = math.pi
sin = math.sin
cos = math.cos
sqrt = math.sqrt

def exp_j (alpha):
    return cmath.exp (alpha*1j)

def conjugate (c):
    c = c * (1+0j)
    return c.real-1j*c.imag

def vector (N):
    return [0.0] * N


# Let us start with the canonical definition of the unscaled DCT algorithm :
# (I can not draw sigmas in text mode so I'll use python code instead)  :)

def unscaled_DCT (N, input, output):
    for o in range(N):		# o is output index
	output[o] = 0
	for i in range(N):	# i is input index
	    output[o] = output[o] + input[i] * cos (((2*i+1)*o*pi)/(2*N))

# This trivial algorithm uses N*N multiplications and N*(N-1) additions.


# And the unscaled DFT algorithm :

def W (k, N):
    return exp_j ((-2*pi*k)/N)

def unscaled_DFT (N, input, output):
    for o in range(N):		# o is output index
	output[o] = 0
	for i in range(N):
	    output[o] = output[o] + input[i] * W(i*o,N)

# This algorithm takes complex input and output. There are N*N complex
# multiplications and N*(N-1) complex additions. One complex addition can be
# implemented with 2 real additions, and one complex multiplication by a
# constant can be implemented with either 4 real multiplications and 2 real
# additions, or 3 real multiplications and 3 real additions.


# Of course these algorithms are extremely naive implementations and there are
# some ways to use the trigonometric properties of the coefficients to find
# some decompositions that can accelerate the calculations by several orders
# of magnitude...


# The Lee algorithm splits a DCT calculation of size N into two DCT
# calculations of size N/2

def unscaled_DCT_Lee (N, input, output):
    even_input = vector(N/2)
    odd_input = vector(N/2)
    even_output = vector(N/2)
    odd_output = vector(N/2)

    for i in range(N/2):
	even_input[i] = input[i] + input[N-1-i]
	odd_input[i] = input[i] - input[N-1-i]

    for i in range(N/2):
	odd_input[i] = odd_input[i] * (0.5 / cos (((2*i+1)*pi)/(2*N)))

    unscaled_DCT (N/2, even_input, even_output)
    unscaled_DCT (N/2, odd_input, odd_output)

    for i in range(N/2-1):
	odd_output[i] = odd_output[i] + odd_output[i+1]

    for i in range(N/2):
	output[2*i] = even_output[i]
	output[2*i+1] = odd_output[i];

# Notes about this algorithm :

# The algorithm can be easily inverted to calculate the IDCT instead :
# each of the basic stages are separately inversible...

# This function does N adds, then N/2 muls, then 2 recursive calls with
# size N/2, then N/2-1 adds again. The total number of operations will be
# N*log2(N)/2 multiplies and less than 3*N*log2(N)/2 additions.
# (exactly N*(3*log2(N)/2-1) + 1 additions). So this is much
# faster than the canonical algorithm.

# Some of the multiplication coefficient, 0.5/cos(...) can get quite large.
# This means that a small error in the input will give a large error on the
# output... For a DCT of size N the biggest coefficient will be at i=N/2-1
# and it will be slighly more than N/pi which can be large for large N's.

# In the IDCT however, the multiplication coefficients for the reverse
# transformation are of the form 2*cos(...) so they can not get big and there
# is no accuracy problem.

# You can find another description of this algorithm at
# http://www.intel.com/drg/mmx/appnotes/ap533.htm


# The AAN algorithm uses another approach, transforming a DCT calculation into
# a DFT calculation of size 2N:

def unscaled_DCT_AAN (N, input, output):
    DFT_input = vector (2*N)
    DFT_output = vector (2*N)

    for i in range(N):
	DFT_input[i] = input[i]
	DFT_input[2*N-1-i] = input[i]

    unscaled_DFT (2*N, DFT_input, DFT_output)

    for i in range(N):
	output[i] = DFT_output[i].real * (0.5 / cos ((i*pi)/(2*N)))

# Notes about the AAN algorithm :

# The cost of this function is N real multiplies and a DFT of size 2*N. The
# DFT to calculate has special properties : the inputs are real and symmetric.
# Also, we only need to calculate the real parts of the N first DFT outputs.
# We will see how we can take advantage of that later.

# We can invert this algorithm to calculate the IDCT. The final multiply
# stage is trivially invertible. The DFT stage is invertible too, but we have
# to take into account the special properties of this particular DFT for that.

# Once again we have to take care of numerical precision for the DFT : the
# output coefficients can get large, so that a small error in the input will
# give a large error on the output... For a DCT of size N the biggest
# coefficient will be at i=N/2-1 and it will be slightly more than N/pi

# You can find another description of this algorithm at this url :
# www.cmlab.csie.ntu.edu.tw/cml/dsp/training/coding/transform/fastdct.html


# The DFT calculation can be decomposed into smaller DFT calculations just like
# the Lee algorithm does for DCT calculations. This is a well known and studied
# problem. One of the available explanations of this process is at this url :
# www.cmlab.csie.ntu.edu.tw/cml/dsp/training/coding/transform/fft.html
# (This is on the same server as the AAN algorithm description !)


# Let's start with the radix-2 decimation-in-time algorithm :

def unscaled_DFT_radix2_time (N, input, output):
    even_input = vector(N/2)
    odd_input = vector(N/2)
    even_output = vector(N/2)
    odd_output = vector(N/2)

    for i in range(N/2):
	even_input[i] = input[2*i]
	odd_input[i] = input[2*i+1]

    unscaled_DFT (N/2, even_input, even_output)
    unscaled_DFT (N/2, odd_input, odd_output)

    for i in range(N/2):
	odd_output[i] = odd_output[i] * W(i,N)

    for i in range(N/2):
	output[i] = even_output[i] + odd_output[i]
	output[i+N/2] = even_output[i] - odd_output[i]

# This algorithm takes complex input and output.

# We divide the DFT calculation into 2 DFT calculations of size N/2
# We then do N/2 complex multiplies followed by N complex additions.
# (actually W(0,N) = 1 and W(N/4,N) = -j so we can skip a few of these complex
# multiplies... we will skip 1 for i=0 and 1 for i=N/4. Also for i=N/8 and for
# i=3*N/8 the W(i,N) values can be special-cased to implement the complex
# multiplication using only 2 real additions and 2 real multiplies)

# Also note that all the basic stages of this DFT algorithm are easily
# reversible, so we can calculate the IDFT with the same complexity.


# A varient of this is the radix-2 decimation-in-frequency algorithm :

def unscaled_DFT_radix2_freq (N, input, output):
    even_input = vector(N/2)
    odd_input = vector(N/2)
    even_output = vector(N/2)
    odd_output = vector(N/2)

    for i in range(N/2):
	even_input[i] = input[i] + input[i+N/2]
	odd_input[i] = input[i] - input[i+N/2]

    for i in range(N/2):
	odd_input[i] = odd_input[i] * W(i,N)

    unscaled_DFT (N/2, even_input, even_output)
    unscaled_DFT (N/2, odd_input, odd_output)

    for i in range(N/2):
	output[2*i] = even_output[i]
	output[2*i+1] = odd_output[i]

# Note that the decimation-in-time and the decimation-in-frequency varients
# have exactly the same complexity, they only do the operations in a different
# order.

# Actually, if you look at the decimation-in-time varient of the DFT, and
# reverse it to calculate an IDFT, you get something that is extremely close
# to the decimation-in-frequency DFT algorithm...


# The radix-4 algorithms are slightly more efficient : they take into account
# the fact that with complex numbers, multiplications by j and -j are also
# free...

# Let's start with the radix-4 decimation-in-time algorithm :

def unscaled_DFT_radix4_time (N, input, output):
    input_0 = vector(N/4)
    input_1 = vector(N/4)
    input_2 = vector(N/4)
    input_3 = vector(N/4)
    output_0 = vector(N/4)
    output_1 = vector(N/4)
    output_2 = vector(N/4)
    output_3 = vector(N/4)
    tmp_0 = vector(N/4)
    tmp_1 = vector(N/4)
    tmp_2 = vector(N/4)
    tmp_3 = vector(N/4)

    for i in range(N/4):
	input_0[i] = input[4*i]
	input_1[i] = input[4*i+1]
	input_2[i] = input[4*i+2]
	input_3[i] = input[4*i+3]

    unscaled_DFT (N/4, input_0, output_0)
    unscaled_DFT (N/4, input_1, output_1)
    unscaled_DFT (N/4, input_2, output_2)
    unscaled_DFT (N/4, input_3, output_3)

    for i in range(N/4):
	output_1[i] = output_1[i] * W(i,N)
	output_2[i] = output_2[i] * W(2*i,N)
	output_3[i] = output_3[i] * W(3*i,N)

    for i in range(N/4):
	tmp_0[i] = output_0[i] + output_2[i]
	tmp_1[i] = output_0[i] - output_2[i]
	tmp_2[i] = output_1[i] + output_3[i]
	tmp_3[i] = output_1[i] - output_3[i]

    for i in range(N/4):
	output[i] = tmp_0[i] + tmp_2[i]
	output[i+N/4] = tmp_1[i] - 1j * tmp_3[i]
	output[i+N/2] = tmp_0[i] - tmp_2[i]
	output[i+3*N/4] = tmp_1[i] + 1j * tmp_3[i]

# This algorithm takes complex input and output.

# We divide the DFT calculation into 4 DFT calculations of size N/4
# We then do 3*N/4 complex multiplies followed by 2*N complex additions.
# (actually W(0,N) = 1 and W(N/4,N) = -j so we can skip a few of these complex
# multiplies... we will skip 3 for i=0 and 1 for i=N/8. Also for i=N/8
# the remaining W(i,N) and W(3*i,N) multiplies can be implemented using only
# 2 real additions and 2 real multiplies. For i=N/16 and i=3*N/16 we can also
# optimise the W(2*i/N) multiply this way.

# If we wanted to do the same decomposition with one radix-2 decomposition
# of size N and 2 radix-2 decompositions of size N/2, the total cost would be
# N complex multiplies and 2*N complex additions. Thus we see that the
# decomposition of one DFT calculation of size N into 4 calculations of size
# N/4 using the radix-4 algorithm instead of the radix-2 algorithm saved N/4
# complex multiplies...


# The radix-4 decimation-in-frequency algorithm is similar :

def unscaled_DFT_radix4_freq (N, input, output):
    input_0 = vector(N/4)
    input_1 = vector(N/4)
    input_2 = vector(N/4)
    input_3 = vector(N/4)
    output_0 = vector(N/4)
    output_1 = vector(N/4)
    output_2 = vector(N/4)
    output_3 = vector(N/4)
    tmp_0 = vector(N/4)
    tmp_1 = vector(N/4)
    tmp_2 = vector(N/4)
    tmp_3 = vector(N/4)

    for i in range(N/4):
	tmp_0[i] = input[i] + input[i+N/2]
	tmp_1[i] = input[i+N/4] + input[i+3*N/4]
	tmp_2[i] = input[i] - input[i+N/2]
	tmp_3[i] = input[i+N/4] - input[i+3*N/4]

    for i in range(N/4):
	input_0[i] = tmp_0[i] + tmp_1[i]
	input_1[i] = tmp_2[i] - 1j * tmp_3[i]
	input_2[i] = tmp_0[i] - tmp_1[i]
	input_3[i] = tmp_2[i] + 1j * tmp_3[i]

    for i in range(N/4):
	input_1[i] = input_1[i] * W(i,N)
	input_2[i] = input_2[i] * W(2*i,N)
	input_3[i] = input_3[i] * W(3*i,N)

    unscaled_DFT (N/4, input_0, output_0)
    unscaled_DFT (N/4, input_1, output_1)
    unscaled_DFT (N/4, input_2, output_2)
    unscaled_DFT (N/4, input_3, output_3)

    for i in range(N/4):
	output[4*i] = output_0[i]
	output[4*i+1] = output_1[i]
	output[4*i+2] = output_2[i]
	output[4*i+3] = output_3[i]

# Once again the complexity is exactly the same as for the radix-4
# decimation-in-time DFT algorithm, only the order of the operations is
# different.


# Now let us reorder the radix-4 algorithms in a different way :

#def unscaled_DFT_radix4_time (N, input, output):
#   input_0 = vector(N/4)
#   input_1 = vector(N/4)
#   input_2 = vector(N/4)
#   input_3 = vector(N/4)
#   output_0 = vector(N/4)
#   output_1 = vector(N/4)
#   output_2 = vector(N/4)
#   output_3 = vector(N/4)
#   tmp_0 = vector(N/4)
#   tmp_1 = vector(N/4)
#   tmp_2 = vector(N/4)
#   tmp_3 = vector(N/4)
#
#   for i in range(N/4):
#	input_0[i] = input[4*i]
#	input_2[i] = input[4*i+2]
#
#   unscaled_DFT (N/4, input_0, output_0)
#   unscaled_DFT (N/4, input_2, output_2)
#
#   for i in range(N/4):
#	output_2[i] = output_2[i] * W(2*i,N)
#
#   for i in range(N/4):
#	tmp_0[i] = output_0[i] + output_2[i]
#	tmp_1[i] = output_0[i] - output_2[i]
#
#   for i in range(N/4):
#	input_1[i] = input[4*i+1]
#	input_3[i] = input[4*i+3]
#
#   unscaled_DFT (N/4, input_1, output_1)
#   unscaled_DFT (N/4, input_3, output_3)
#
#   for i in range(N/4):
#	output_1[i] = output_1[i] * W(i,N)
#	output_3[i] = output_3[i] * W(3*i,N)
#
#   for i in range(N/4):
#	tmp_2[i] = output_1[i] + output_3[i]
#	tmp_3[i] = output_1[i] - output_3[i]
#
#   for i in range(N/4):
#	output[i] = tmp_0[i] + tmp_2[i]
#	output[i+N/4] = tmp_1[i] - 1j * tmp_3[i]
#	output[i+N/2] = tmp_0[i] - tmp_2[i]
#	output[i+3*N/4] = tmp_1[i] + 1j * tmp_3[i]

# We didnt do anything here, only reorder the operations. But now, look at the
# first part of this function, up to the calculations of tmp0 and tmp1 : this
# is extremely similar to the radix-2 decimation-in-time algorithm ! or more
# precisely, it IS the radix-2 decimation-in-time algorithm, with size N/2,
# applied on a vector representing the even input coefficients, and giving
# an output vector that is the concatenation of tmp0 and tmp1.
# This is important to notice, because this means we can now choose to
# calculate tmp0 and tmp1 using any DFT algorithm that we want, and we know
# that some of them are more efficient than radix-2...

# This leads us directly to the split-radix decimation-in-time algorithm :

def unscaled_DFT_split_radix_time (N, input, output):
    even_input = vector(N/2)
    input_1 = vector(N/4)
    input_3 = vector(N/4)
    even_output = vector(N/2)
    output_1 = vector(N/4)
    output_3 = vector(N/4)
    tmp_0 = vector(N/4)
    tmp_1 = vector(N/4)

    for i in range(N/2):
	even_input[i] = input[2*i]

    for i in range(N/4):
	input_1[i] = input[4*i+1]
	input_3[i] = input[4*i+3]

    unscaled_DFT (N/2, even_input, even_output)
    unscaled_DFT (N/4, input_1, output_1)
    unscaled_DFT (N/4, input_3, output_3)

    for i in range(N/4):
	output_1[i] = output_1[i] * W(i,N)
	output_3[i] = output_3[i] * W(3*i,N)

    for i in range(N/4):
	tmp_0[i] = output_1[i] + output_3[i]
	tmp_1[i] = output_1[i] - output_3[i]

    for i in range(N/4):
	output[i] = even_output[i] + tmp_0[i]
	output[i+N/4] = even_output[i+N/4] - 1j * tmp_1[i]
	output[i+N/2] = even_output[i] - tmp_0[i]
	output[i+3*N/4] = even_output[i+N/4] + 1j * tmp_1[i]

# This function performs one DFT of size N/2 and two of size N/4, followed by
# N/2 complex multiplies and 3*N/2 complex additions.
# (actually W(0,N) = 1 and W(N/4,N) = -j so we can skip a few of these complex
# multiplies... we will skip 2 for i=0. Also for i=N/8 the W(i,N) and W(3*i,N)
# multiplies can be implemented using only 2 real additions and 2 real
# multiplies)


# We can similarly define the split-radix decimation-in-frequency DFT :

def unscaled_DFT_split_radix_freq (N, input, output):
    even_input = vector(N/2)
    input_1 = vector(N/4)
    input_3 = vector(N/4)
    even_output = vector(N/2)
    output_1 = vector(N/4)
    output_3 = vector(N/4)
    tmp_0 = vector(N/4)
    tmp_1 = vector(N/4)

    for i in range(N/2):
	even_input[i] = input[i] + input[i+N/2]

    for i in range(N/4):
	tmp_0[i] = input[i] - input[i+N/2]
	tmp_1[i] = input[i+N/4] - input[i+3*N/4]

    for i in range(N/4):
	input_1[i] = tmp_0[i] - 1j * tmp_1[i]
	input_3[i] = tmp_0[i] + 1j * tmp_1[i]

    for i in range(N/4):
	input_1[i] = input_1[i] * W(i,N)
	input_3[i] = input_3[i] * W(3*i,N)

    unscaled_DFT (N/2, even_input, even_output)
    unscaled_DFT (N/4, input_1, output_1)
    unscaled_DFT (N/4, input_3, output_3)

    for i in range(N/2):
	output[2*i] = even_output[i]

    for i in range(N/4):
	output[4*i+1] = output_1[i]
	output[4*i+3] = output_3[i]

# The complexity is again the same as for the decimation-in-time varient.


# Now let us now summarize our various algorithms for DFT decomposition :

# radix-2 : DFT(N) -> 2*DFT(N/2) using N/2 multiplies and N additions
# radix-4 : DFT(N) -> 4*DFT(N/2) using 3*N/4 multiplies and 2*N additions
# split-radix : DFT(N) -> DFT(N/2) + 2*DFT(N/4) using N/2 muls and 3*N/2 adds

# (we are always speaking of complex multiplies and complex additions...
# remember than a complex addition is implemented with 2 real additions, and
# a complex multiply is implemented with)

# If we want to take into account the special values of W(i,N), we can remove
# a few complex multiplies. Supposing N>=16 we can remove :
# radix-2 : remove 2 complex multiplies, simplify 2 others
# radix-4 : remove 4 complex multiplies, simplify 4 others
# split-radix : remove 2 complex multiplies, simplify 2 others

# The best performance using these methods is thus :
#	N	complex muls	simple muls	complex adds	method
#       1               0               0               0       trivial!
#       2               0               0               2       trivial!
#       4               0               0               8       radix-4
#       8               0               2              24       radix-4
#      16               4               4              64       split radix
#      32              16              10             160       split radix
#      64              52              20             384       split radix
#     128             144              42             896       split radix
#     256             372              84            2048       split radix
#     512             912             170            4608       split radix
#    1024            2164             340           10240       split radix
#    2048            5008             682           22528       split radix
#    4096           11380            1364           49152       split radix
#    8192           25488            2730          106496       split radix
#   16384           56436            5460          229376       split radix
#   32768          123792           10922          491520       split radix
#   65536          269428           21844         1048576       split radix

# Now a complex addition is implemented with 2 real additions, a "simple"
# complex multiply is implemented with 2 real multiplies and 2 real additions,
# and complex multiplies can be implemented with either 2 real additions and
# 4 real multiplies, or 3 real additions and 3 real multiplies, so we will
# keep them in a separate column. Which gives...

#	N	real additions	real multiplies	complex multiplies
#       1               0               0               0
#       2               4               0               0
#       4              16               0               0
#       8              52               4               0
#      16             136               8               4
#      32             340              20              16
#      64             808              40              52
#     128            1876              84             144
#     256            4264             168             372
#     512            9556             340             912
#    1024           21160             680            2164
#    2048           46420            1364            5008
#    4096          101032            2728           11380
#    8192          218452            5460           25488
#   16384          469672           10920           56436
#   32768         1004884           21844          123792
#   65536         2140840           43688          269428

# If a complex multiply is implemented with 3 real muls + 3 real adds,
# a complex "simple" multiply is implemented with 2 real muls + 2 real adds,
# and a complex addition is implemented with 2 real adds, then these results
# are consistent with the table at the end of the www.cmlab.csie.ntu.edu.tw
# DFT tutorial that I mentionned earlier.


# Now another important case for the DFT is the one where the inputs are
# real numbers instead of complex ones. We have to find ways to optimize for
# this important case.

# If the DFT inputs are real-valued, then the DFT outputs have nice properties
# too : output[0] and output[N/2] will be real numbers, and output[N-i] will
# be the conjugate of output[i] for i in 0...N/2-1

# Likewise, if the DFT inputs are purely imaginary numbers, then the DFT
# outputs will have special properties too : output[0] and output[N/2] will be
# purely imaginary, and output[N-i] will be the opposite of the conjugate of
# output[i] for i in 0...N/2-1

# We can use these properties to calculate two real-valued DFT at once :

def two_real_unscaled_DFT (N, input1, input2, output1, output2):
    input = vector(N)
    output = vector(N)

    for i in range(N):
	input[i] = input1[i] + 1j * input2[i]

    unscaled_DFT (N, input, output)

    output1[0] = output[0].real + 0j
    output2[0] = output[0].imag + 0j

    for i in range(N/2)[1:]:
	output1[i] = 0.5 * (output[i] + conjugate(output[N-i]))
	output2[i] = -0.5j * (output[i] - conjugate(output[N-i]))

	output1[N-i] = conjugate(output1[i])
	output2[N-i] = conjugate(output2[i])

    output1[N/2] = output[N/2].real + 0j
    output2[N/2] = output[N/2].imag + 0j

# This routine does a total of N-2 complex additions and N-2 complex
# multiplies by 0.5

# This routine can also be inverted to calculate the IDFT of two vectors at
# once if we know that the outputs will be real-valued.


# If we have only one real-valued DFT calculation to do, we can still cut this
# calculation in several parts using one of the decimate-in-time methods
# (so that the different parts are still real-valued)

# As with complex DFT calculations, the best method is to use a split radix.
# There are a lot of symetries in the DFT outputs that we can exploit to
# reduce the number of operations...

def real_unscaled_DFT_split_radix_1 (N, input, output):
    even_input = vector(N/2)
    even_output = vector(N/2)
    input_1 = vector(N/4)
    output_1 = vector(N/4)
    input_3 = vector(N/4)
    output_3 = vector(N/4)
    tmp_0 = vector(N/4)
    tmp_1 = vector(N/4)

    for i in range(N/2):
	even_input[i] = input[2*i]

    for i in range(N/4):
	input_1[i] = input[4*i+1]
	input_3[i] = input[4*i+3]

    unscaled_DFT (N/2, even_input, even_output)
    # this is again a real DFT !
    # we will only use even_output[i] for i in 0 ... N/4 included. we know that
    # even_output[N/2-i] is the conjugate of even_output[i]... also we know
    # that even_output[0] and even_output[N/4] are purely real.

    unscaled_DFT (N/4, input_1, output_1)
    unscaled_DFT (N/4, input_3, output_3)
    # these are real DFTs too... with symetries in the outputs... once again

    tmp_0[0] = output_1[0] + output_3[0]	# real numbers
    tmp_1[0] = output_1[0] - output_3[0]	# real numbers

    tmp__0 = (output_1[N/8] + output_3[N/8]) * sqrt(0.5)	# real numbers
    tmp__1 = (output_1[N/8] - output_3[N/8]) * sqrt(0.5)	# real numbers
    tmp_0[N/8] = tmp__1 - 1j * tmp__0		# real + 1j * real
    tmp_1[N/8] = tmp__0 - 1j * tmp__1		# real + 1j * real

    for i in range(N/8)[1:]:
	output_1[i] = output_1[i] * W(i,N)
	output_3[i] = output_3[i] * W(3*i,N)

	tmp_0[i] = output_1[i] + output_3[i]
	tmp_1[i] = output_1[i] - output_3[i]

	tmp_0[N/4-i] = -1j * conjugate(tmp_1[i])
	tmp_1[N/4-i] = -1j * conjugate(tmp_0[i])

    output[0] = even_output[0] + tmp_0[0]		# real numbers
    output[N/4] = even_output[N/4] - 1j * tmp_1[0]	# real + 1j * real
    output[N/2] = even_output[0] - tmp_0[0]		# real numbers
    output[3*N/4] = even_output[N/4] + 1j * tmp_1[0]	# real + 1j * real

    for i in range(N/4)[1:]:
	output[i] = even_output[i] + tmp_0[i]
	output[i+N/4] = conjugate(even_output[N/4-i]) - 1j * tmp_1[i]

	output[N-i] = conjugate(output[i])
	output[3*N/4-i] = conjugate(output[i+N/4])

# This function performs one real DFT of size N/2 and two real DFT of size
# N/4, followed by 6 real additions, 2 real multiplies, 3*N/4-4 complex
# additions and N/4-2 complex multiplies.


# We can also try to combine the two real DFT of size N/4 into a single complex
# DFT :

def real_unscaled_DFT_split_radix_2 (N, input, output):
    even_input = vector(N/2)
    even_output = vector(N/2)
    odd_input = vector(N/4)
    odd_output = vector(N/4)
    tmp_0 = vector(N/4)
    tmp_1 = vector(N/4)

    for i in range(N/2):
	even_input[i] = input[2*i]

    for i in range(N/4):
	odd_input[i] = input[4*i+1] + 1j * input[4*i+3]

    unscaled_DFT (N/2, even_input, even_output)
    # this is again a real DFT !
    # we will only use even_output[i] for i in 0 ... N/4 included. we know that
    # even_output[N/2-i] is the conjugate of even_output[i]... also we know
    # that even_output[0] and even_output[N/4] are purely real.

    unscaled_DFT (N/4, odd_input, odd_output)
    # but this one is a complex DFT so no special properties here

    output_1 = odd_output[0].real
    output_3 = odd_output[0].imag
    tmp_0[0] = output_1 + output_3	# real numbers
    tmp_1[0] = output_1 - output_3	# real numbers

    output_1 = odd_output[N/8].real
    output_3 = odd_output[N/8].imag
    tmp__0 = (output_1 + output_3) * sqrt(0.5)	# real numbers
    tmp__1 = (output_1 - output_3) * sqrt(0.5)	# real numbers
    tmp_0[N/8] = tmp__1 - 1j * tmp__0		# real + 1j * real
    tmp_1[N/8] = tmp__0 - 1j * tmp__1		# real + 1j * real

    for i in range(N/8)[1:]:
	output_1 = odd_output[i] + conjugate(odd_output[N/4-i])
	output_3 = odd_output[i] - conjugate(odd_output[N/4-i])

	output_1 = output_1 * 0.5 * W(i,N)
	output_3 = output_3 * -0.5j * W(3*i,N)

	tmp_0[i] = output_1 + output_3
	tmp_1[i] = output_1 - output_3

	tmp_0[N/4-i] = -1j * conjugate(tmp_1[i])
	tmp_1[N/4-i] = -1j * conjugate(tmp_0[i])

    output[0] = even_output[0] + tmp_0[0]		# real numbers
    output[N/4] = even_output[N/4] - 1j * tmp_1[0]	# real + 1j * real
    output[N/2] = even_output[0] - tmp_0[0]		# real numbers
    output[3*N/4] = even_output[N/4] + 1j * tmp_1[0]	# real + 1j * real

    for i in range(N/4)[1:]:
	output[i] = even_output[i] + tmp_0[i]
	output[i+N/4] = conjugate(even_output[N/4-i]) - 1j * tmp_1[i]

	output[N-i] = conjugate(output[i])
	output[3*N/4-i] = conjugate(output[i+N/4])

# This function performs one real DFT of size N/2 and one complex DFT of size
# N/4, followed by 6 real additions, 2 real multiplies, N-6 complex additions
# and N/4-2 complex multiplies.


# After comparing the performance, it turns out that for real-valued DFT, the
# version of the algorithm that subdivides the calculation into one real
# DFT of size N/2 and two real DFT of size N/4 is the most efficient one.
# The other version gives exactly the same number of multiplies and a few more
# real additions.

# The performance we get for real-valued DFT is as follows :

#	N	real additions	real multiplies	complex multiplies
#       2               2               0               0
#       4               6               0               0
#       8              20               2               0
#      16              54               4               2
#      32             140              10               8
#      64             342              20              26
#     128             812              42              72
#     256            1878              84             186
#     512            4268             170             456
#    1024            9558             340            1082
#    2048           21164             682            2504
#    4096           46422            1364            5690
#    8192          101036            2730           12744
#   16384          218454            5460           28218
#   32768          469676           10922           61896
#   65536         1004886           21844          134714


# As an example, this is an implementation of a real-valued DFT8, using the
# above-mentionned algorithm :

def DFT8 (input, output):
    tmp_0 = input[0] + input[4]
    tmp_1 = input[0] - input[4]
    tmp_2 = input[2] + input[6]
    tmp_3 = input[2] - input[6]

    even_0 = tmp_0 + tmp_2		# real + real
    even_1 = tmp_1 - 1j * tmp_3		# real + 1j * real
    even_2 = tmp_0 - tmp_2		# real + real
    even_3 = tmp_1 + 1j * tmp_3		# real + 1j * real

    tmp__0 = input[1] + input[5]
    tmp__1 = input[1] - input[5]
    tmp__2 = input[3] + input[7]
    tmp__3 = input[3] - input[7]

    tmp_0 = tmp__0 + tmp__2	# real numbers
    tmp_2 = tmp__0 - tmp__2	# real numbers

    tmp__0 = (tmp__1 + tmp__3) * sqrt(0.5)	# real numbers
    tmp__1 = (tmp__1 - tmp__3) * sqrt(0.5)	# real numbers
    tmp_1 = tmp__1 - 1j * tmp__0		# real + 1j * real
    tmp_3 = tmp__0 - 1j * tmp__1		# real + 1j * real

    output[0] = even_0 + tmp_0		# real numbers
    output[2] = even_2 - 1j * tmp_2	# real + 1j * real
    output[4] = even_0 - tmp_0		# real numbers
    output[6] = even_2 + 1j * tmp_2	# real + 1j * real

    output[1] = even_1 + tmp_1			# complex numbers
    output[3] = conjugate(even_1) - 1j * tmp_3	# complex numbers
    output[5] = conjugate(output[3])
    output[7] = conjugate(output[1])


# Also a basic implementation of the real-valued DFT4 :

def DFT4 (input, output):
    tmp_0 = input[0] + input[2]
    tmp_1 = input[0] - input[2]
    tmp_2 = input[1] + input[3]
    tmp_3 = input[1] - input[3]

    output[0] = tmp_0 + tmp_2		# real + real
    output[1] = tmp_1 - 1j * tmp_3	# real + 1j * real
    output[2] = tmp_0 - tmp_2		# real + real
    output[3] = tmp_1 + 1j * tmp_3	# real + 1j * real


# Now the last piece of the puzzle is the implementation of real-valued DFT
# with a symetrical input. If you remember about the AAN DCT algorithm, this
# is useful there...

# The best method I have found is to use a modification of the radix2
# decimate-in-time algorithm here. The trick is that odd_input will be the
# symetric of even_input... so we can deduce the value of odd_output from
# the value of even_output :
# odd_output[i] = conjugate(even_output[i]) * W(-i,N/2)
# if we then merge this multiply with the one that is just after it in the
# radix-2 decimate-in-time algorithm, and then we take all the symetries into
# account to remove the corresponding code, we get the following function :

def real_symetric_unscaled_DFT (N, input, output):
    even_input = vector(N/2)
    even_output = vector(N/2)
    odd_output = vector(N/2)

    for i in range(N/2):
	even_input[i] = input[2*i]

    unscaled_DFT (N/2, even_input, even_output)
    # This is once again a real-valued DFT

    output[0] = 2 * even_output[0]	# real number
    output[N/2] = 0

    output[N/4] = (1 + 1j) * even_output[N/4]	# complex * real
    output[3*N/4] = conjugate(output[N/4])

    for i in range(N/4)[1:]:
	#odd_output = conjugate(even_output[i]) * W(-i,N)
 	#output[i] = even_output[i] + odd_output
	#odd_output = even_output[i] * W(N/2+i,N)
 	#output[N/2-i] = conjugate(even_output[i]) + odd_output

	cr = W(-i,N).real
	ci = W(-i,N).imag

	real = even_output[i].real * (1+cr) + even_output[i].imag * ci
	imag = even_output[i].real * ci + even_output[i].imag * (1-cr)
	output[i] = real + 1j * imag

	real = even_output[i].real * (1-cr) - even_output[i].imag * ci
	imag = even_output[i].real * ci - even_output[i].imag * (1+cr)
	output[N/2-i] = real + 1j * imag

	output[N-i] = conjugate(output[i])
	output[N/2+i] = conjugate(output[N/2-i])

# This function does one real unscaled DFT of size N/2, one multiply by 2, and
# N/4-1 times something that can be written with either 6 real muls and 4 real
# adds (as I did), or 1 complex mul and 2 complex adds (giving 4 real muls and
# 6 adds, or 3 real muls and 7 adds).


# Now we can use this new knowledge to write a new optimized version of the
# AAN algorithm for the DCT calculation :

def unscaled_DCT_AAN_optim (N, input, output):
    DFT_input = vector (N)
    DFT_output = vector (N)

    for i in range(N/2):
	DFT_input[i] = input[2*i]
	DFT_input[N-1-i] = input[2*i+1]

    unscaled_DFT (N, DFT_input, DFT_output)
    # This is another real-valued DFT

    output[0] = DFT_output[0]
    output[N/2] = DFT_output[N/2] * sqrt(0.5)

    for i in range(N/2)[1:]:
	tmp = (conjugate(DFT_output[i]) *
	       (1+W(-i,2*N)) * 0.5 / cos ((i*pi)/(2*N)))
	output[i] = tmp.real
	output[N-i] = tmp.imag

# Now the DCT calculation can be reduced to one real-valued DFT calculation of
# size N, followed by 1 real multiply and N/2-1 complex multiplies

# One funny result is that if we calculate the number of real operations needed
# to implement this AAN DCT algorithm, and supposing that we choose to
# implement complex multiplies with 3 real adds and 3 real muls, then the
# number of operations is *exactly* the same as for the original Lee DCT
# algorithm...


# THATS ALL FOLKS !


def dump (vector):
    str = ""
    for i in range(len(vector)):
	if i:
	    str = str + ", "
	vector[i] = vector[i] + 0j
	realstr = "%+.4f" % vector[i].real
	imagstr = "%+.4fj" % vector[i].imag
	if (realstr == "-0.0000"):
	    realstr = "+0.0000"
	if (imagstr == "-0.0000j"):
	    imagstr = "+0.0000j"
	str = str + realstr + imagstr
    return "[%s]" % str

import whrandom

def test(N):
    input = vector(N)
    output = vector(N)
    verify = vector(N)

    for i in range(N):
	input[i] = whrandom.random()

    unscaled_DCT_AAN_optim (N, input, output)
    unscaled_DCT (N, input, verify)

    if (dump(verify) != dump(output)):
	print dump(verify)
	#print dump(output)

test (32)

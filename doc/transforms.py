# Lossy compression algorithms very often make use of DCT or DFT calculations,
# or variations of these calculations. This file is intended to be a short
# reference about classical DCT and DFT algorithms.


from random import random
from math import pi, sin, cos, sqrt
from cmath import exp

def exp_j (alpha):
    return exp (alpha * 1j)

def conjugate (c):
    c = c + 0j
    return c.real - 1j * c.imag

def vector (N):
    return [0j] * N


# Let us start withthe canonical definition of the unscaled DFT algorithm :
# (I can not draw sigmas in a text file so I'll use python code instead)  :)

def W (k, N):
    return exp_j ((-2*pi*k)/N)

def unscaled_DFT (N, input, output):
    for o in range(N):		# o is output index
	output[o] = 0
	for i in range(N):
	    output[o] = output[o] + input[i] * W (i*o, N)

# This algorithm takes complex input and output. There are N*N complex
# multiplications and N*(N-1) complex additions.


# Of course this algorithm is an extremely naive implementation and there are
# some ways to use the trigonometric properties of the coefficients to find
# some decompositions that can accelerate the calculation by several orders
# of magnitude... This is a well known and studied problem. One of the
# available explanations of this process is at this url :
# www.cmlab.csie.ntu.edu.tw/cml/dsp/training/coding/transform/fft.html


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
	odd_output[i] = odd_output[i] * W (i, N)

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


# A variant of this is the radix-2 decimation-in-frequency algorithm :

def unscaled_DFT_radix2_freq (N, input, output):
    even_input = vector(N/2)
    odd_input = vector(N/2)
    even_output = vector(N/2)
    odd_output = vector(N/2)

    for i in range(N/2):
	even_input[i] = input[i] + input[i+N/2]
	odd_input[i] = input[i] - input[i+N/2]

    for i in range(N/2):
	odd_input[i] = odd_input[i] * W (i, N)

    unscaled_DFT (N/2, even_input, even_output)
    unscaled_DFT (N/2, odd_input, odd_output)

    for i in range(N/2):
	output[2*i] = even_output[i]
	output[2*i+1] = odd_output[i]

# Note that the decimation-in-time and the decimation-in-frequency varients
# have exactly the same complexity, they only do the operations in a different
# order.

# Actually, if you look at the decimation-in-time variant of the DFT, and
# reverse it to calculate an IDFT, you get something that is extremely close
# to the decimation-in-frequency DFT algorithm...


# The radix-4 algorithms are slightly more efficient : they take into account
# the fact that with complex numbers, multiplications by j and -j are also
# "free"... i.e. when you code them using real numbers, they translate into
# a few data moves but no real operation.

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
	output_1[i] = output_1[i] * W (i, N)
	output_2[i] = output_2[i] * W (2*i, N)
	output_3[i] = output_3[i] * W (3*i, N)

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
	input_1[i] = input_1[i] * W (i, N)
	input_2[i] = input_2[i] * W (2*i, N)
	input_3[i] = input_3[i] * W (3*i, N)

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
#	output_2[i] = output_2[i] * W (2*i, N)
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
#	output_1[i] = output_1[i] * W (i, N)
#	output_3[i] = output_3[i] * W (3*i, N)
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

# We didn't do anything here, only reorder the operations. But now, look at the
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
	output_1[i] = output_1[i] * W (i, N)
	output_3[i] = output_3[i] * W (3*i, N)

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
	input_1[i] = input_1[i] * W (i, N)
	input_3[i] = input_3[i] * W (3*i, N)

    unscaled_DFT (N/2, even_input, even_output)
    unscaled_DFT (N/4, input_1, output_1)
    unscaled_DFT (N/4, input_3, output_3)

    for i in range(N/2):
	output[2*i] = even_output[i]

    for i in range(N/4):
	output[4*i+1] = output_1[i]
	output[4*i+3] = output_3[i]

# The complexity is again the same as for the decimation-in-time variant.


# Now let us now summarize our various algorithms for DFT decomposition :

# radix-2 : DFT(N) -> 2*DFT(N/2) using N/2 multiplies and N additions
# radix-4 : DFT(N) -> 4*DFT(N/2) using 3*N/4 multiplies and 2*N additions
# split-radix : DFT(N) -> DFT(N/2) + 2*DFT(N/4) using N/2 muls and 3*N/2 adds

# (we are always speaking of complex multiplies and complex additions... a
# complex addition is implemented with 2 real additions, and a complex
# multiply is implemented with either 2 adds and 4 muls or 3 adds and 3 muls,
# so we will keep a separate count of these)

# If we want to take into account the special values of W(i,N), we can remove
# a few complex multiplies. Supposing N>=16 we can remove :
# radix-2 : remove 2 complex multiplies, simplify 2 others
# radix-4 : remove 4 complex multiplies, simplify 4 others
# split-radix : remove 2 complex multiplies, simplify 2 others

# This gives the following table for the complexity of a complex DFT :
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

# If we chose to implement complex multiplies with 3 real muls + 3 real adds,
# then these results are consistent with the table at the end of the
# www.cmlab.csie.ntu.edu.tw DFT tutorial that I mentionned earlier.


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

def real_unscaled_DFT_split_radix_time_1 (N, input, output):
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
	output_1[i] = output_1[i] * W (i, N)
	output_3[i] = output_3[i] * W (3*i, N)

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

def real_unscaled_DFT_split_radix_time_2 (N, input, output):
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

	output_1 = output_1 * 0.5 * W (i, N)
	output_3 = output_3 * -0.5j * W (3*i, N)

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


# Now we can also try the decimate-in-frequency method for a real-valued DFT.
# Using the split-radix algorithm, and by taking into account the symetries of
# the outputs :

def real_unscaled_DFT_split_radix_freq (N, input, output):
    even_input = vector(N/2)
    input_1 = vector(N/4)
    even_output = vector(N/2)
    output_1 = vector(N/4)
    tmp_0 = vector(N/4)
    tmp_1 = vector(N/4)

    for i in range(N/2):
	even_input[i] = input[i] + input[i+N/2]

    for i in range(N/4):
	tmp_0[i] = input[i] - input[i+N/2]
	tmp_1[i] = input[i+N/4] - input[i+3*N/4]

    for i in range(N/4):
	input_1[i] = tmp_0[i] - 1j * tmp_1[i]

    for i in range(N/4):
	input_1[i] = input_1[i] * W (i, N)

    unscaled_DFT (N/2, even_input, even_output)
    # This is still a real-valued DFT

    unscaled_DFT (N/4, input_1, output_1)
    # But that one is a complex-valued DFT

    for i in range(N/2):
	output[2*i] = even_output[i]

    for i in range(N/4):
	output[4*i+1] = output_1[i]
	output[N-1-4*i] = conjugate(output_1[i])

# I think this implementation is much more elegant than the decimate-in-time
# version ! It looks very much like the complex-valued version, all we had to
# do was remove one of the complex-valued internal DFT calls because we could
# deduce the outputs by using the symetries of the problem.

# As for performance, we did N real additions, N/4 complex multiplies (a bit
# less actually, because W(0,N) = 1 and W(N/8,N) is a "simple" multiply), then
# one real DFT of size N/2 and one complex DFT of size N/4.

# It turns out that even if the methods are so different, the number of
# operations is exactly the same as for the best of the two decimation-in-time
# methods that we tried.


# This gives us the following performance for real-valued DFT :
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


# As an example, this is an implementation of the real-valued DFT8 :

def DFT8 (input, output):
    even_0 = input[0] + input[4]
    even_1 = input[1] + input[5]
    even_2 = input[2] + input[6]
    even_3 = input[3] + input[7]

    tmp_0 = even_0 + even_2
    tmp_1 = even_0 - even_2
    tmp_2 = even_1 + even_3
    tmp_3 = even_1 - even_3

    output[0] = tmp_0 + tmp_2
    output[2] = tmp_1 - 1j * tmp_3
    output[4] = tmp_0 - tmp_2

    odd_0_r = input[0] - input[4]
    odd_0_i = input[2] - input[6]

    tmp_0 = input[1] - input[5]
    tmp_1 = input[3] - input[7]
    odd_1_r = (tmp_0 - tmp_1) * sqrt(0.5)
    odd_1_i = (tmp_0 + tmp_1) * sqrt(0.5)

    output[1] = (odd_0_r + odd_1_r) - 1j * (odd_0_i + odd_1_i)
    output[5] = (odd_0_r - odd_1_r) - 1j * (odd_0_i - odd_1_i)

    output[3] = conjugate(output[5])
    output[6] = conjugate(output[2])
    output[7] = conjugate(output[1])


# Also a basic implementation of the real-valued DFT4 :

def DFT4 (input, output):
    tmp_0 = input[0] + input[2]
    tmp_1 = input[0] - input[2]
    tmp_2 = input[1] + input[3]
    tmp_3 = input[1] - input[3]

    output[0] = tmp_0 + tmp_2
    output[1] = tmp_1 - 1j * tmp_3
    output[2] = tmp_0 - tmp_2
    output[3] = tmp_1 + 1j * tmp_3


# A similar idea might be used to calculate only the real part of the output
# of a complex DFT : we take an DFT algorithm for real inputs and complex
# outputs and we simply reverse it. The resulting algorithm will only work
# with inputs that satisfy the conjugaison rule (input[i] is the conjugate of
# input[N-i]) so we can do a first pass to modify the input so that it follows
# this rule. An example implementation is as follows (adapted from the
# unscaled_DFT_split_radix_time algorithm) :

def complex2real_unscaled_DFT_split_radix_time (N, input, output):
    even_input = vector(N/2)
    input_1 = vector(N/4)
    even_output = vector(N/2)
    output_1 = vector(N/4)

    for i in range(N/2):
	even_input[i] = input[2*i]

    for i in range(N/4):
	input_1[i] = input[4*i+1] + conjugate(input[N-1-4*i])

    unscaled_DFT (N/2, even_input, even_output)
    unscaled_DFT (N/4, input_1, output_1)

    for i in range(N/4):
	output_1[i] = output_1[i] * W (i, N)

    for i in range(N/4):
	output[i] = even_output[i] + output_1[i].real
	output[i+N/4] = even_output[i+N/4] + output_1[i].imag
	output[i+N/2] = even_output[i] - output_1[i].real
	output[i+3*N/4] = even_output[i+N/4] - output_1[i].imag

# This algorithm does N/4 complex additions, N/4-1 complex multiplies
# (including one "simple" multiply for i=N/8), N real additions, one
# "complex-to-real" DFT of size N/2, and one complex DFT of size N/4.
# Also, in the complex DFT of size N/4, we do not care about the imaginary
# part of output_1[0], which in practice allows us to save one real addition.

# This gives us the following performance for complex DFT with real outputs :
#	N	real additions	real multiplies	complex multiplies
#       1               0               0               0
#       2               2               0               0
#       4               8               0               0
#       8              25               2               0
#      16              66               4               2
#      32             167              10               8
#      64             400              20              26
#     128             933              42              72
#     256            2126              84             186
#     512            4771             170             456
#    1024           10572             340            1082
#    2048           23201             682            2504
#    4096           50506            1364            5690
#    8192          109215            2730           12744
#   16384          234824            5460           28218
#   32768          502429           10922           61896
#   65536         1070406           21844          134714


# Now let's talk about the DCT algorithm. The canonical definition for it is
# as follows :

def C (k, N):
    return cos ((k*pi)/(2*N))

def unscaled_DCT (N, input, output):
    for o in range(N):		# o is output index
	output[o] = 0
	for i in range(N):	# i is input index
	    output[o] = output[o] + input[i] * C ((2*i+1)*o, N)

# This trivial algorithm uses N*N multiplications and N*(N-1) additions.


# One possible decomposition on this calculus is to use the fact that C (i, N)
# and C (2*N-i, N) are opposed. This can lead to this decomposition :

#def unscaled_DCT (N, input, output):
#   even_input = vector (N)
#   odd_input = vector (N)
#   even_output = vector (N)
#   odd_output = vector (N)
#
#   for i in range(N/2):
#	even_input[i] = input[i] + input[N-1-i]
#	odd_input[i] = input[i] - input[N-1-i]
#
#   unscaled_DCT (N, even_input, even_output)
#   unscaled_DCT (N, odd_input, odd_output)
#
#   for i in range(N/2):
#	output[2*i] = even_output[2*i]
#	output[2*i+1] = odd_output[2*i+1]

# Now the even part can easily be calculated : by looking at the C(k,N)
# formula, we see that the even part is actually an unscaled DCT of size N/2.
# The odd part looks like a DCT of size N/2, but the coefficients are
# actually C ((2*i+1)*(2*o+1), 2*N) instead of C ((2*i+1)*o, N).

# We use a trigonometric relation here :
# 2 * C ((a+b)/2, N) * C ((a-b)/2, N) = C (a, N) + C (b, N)
# Thus with a = (2*i+1)*o and b = (2*i+1)*(o+1) :
# 2 * C((2*i+1)*(2*o+1),2N) * C(2*i+1,2N) = C((2*i+1)*o,N) + C((2*i+1)*(o+1),N)

# This leads us to the Lee DCT algorithm :

def unscaled_DCT_Lee (N, input, output):
    even_input = vector(N/2)
    odd_input = vector(N/2)
    even_output = vector(N/2)
    odd_output = vector(N/2)

    for i in range(N/2):
	even_input[i] = input[i] + input[N-1-i]
	odd_input[i] = input[i] - input[N-1-i]

    for i in range(N/2):
	odd_input[i] = odd_input[i] * (0.5 / C (2*i+1, N))

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
# size N/2, then N/2-1 adds again. If we apply it recursively, the total
# number of operations will be N*log2(N)/2 multiplies and N*(3*log2(N)/2-1) + 1
# additions. So this is much faster than the canonical algorithm.

# Some of the multiplication coefficients 0.5/cos(...) can get quite large.
# This means that a small error in the input will give a large error on the
# output... For a DCT of size N the biggest coefficient will be at i=N/2-1
# and it will be slightly more than N/pi which can be large for large N's.

# In the IDCT however, the multiplication coefficients for the reverse
# transformation are of the form 2*cos(...) so they can not get big and there
# is no accuracy problem.

# You can find another description of this algorithm at
# http://www.intel.com/drg/mmx/appnotes/ap533.htm



# Another idea is to observe that the DCT calculation can be made to look like
# the DFT calculation : C (k, N) is the real part of W (k, 4*N) or W (-k, 4*N).
# We can use this idea translate the DCT algorithm into a call to the DFT
# algorithm :

def unscaled_DCT_DFT (N, input, output):
    DFT_input = vector (4*N)
    DFT_output = vector (4*N)

    for i in range(N):
	DFT_input[2*i+1] = input[i]
	#DFT_input[4*N-2*i-1] = input[i]	# We could use this instead

    unscaled_DFT (4*N, DFT_input, DFT_output)

    for i in range(N):
	output[i] = DFT_output[i].real


# We can then use our knowledge of the DFT calculation to optimize for this
# particular case. For example using the radix-2 decimation-in-time method :

#def unscaled_DCT_DFT (N, input, output):
#   DFT_input = vector (2*N)
#   DFT_output = vector (2*N)
#
#   for i in range(N):
#	DFT_input[i] = input[i]
#	#DFT_input[2*N-1-i] = input[i]	# We could use this instead
#
#   unscaled_DFT (2*N, DFT_input, DFT_output)
#
#   for i in range(N):
#       DFT_output[i] = DFT_output[i] * W (i, 4*N)
#
#   for i in range(N):
#	output[i] = DFT_output[i].real

# This leads us to the AAN implementation of the DCT algorithm : if we set
# both DFT_input[i] and DFT_input[2*N-1-i] to input[i], then the imaginary
# parts of W(2*i+1) and W(-2*i-1) will compensate, and output_DFT[i] will
# already be a real after the multiplication by W(i,4*N). Which means that
# before the multiplication, it is the product of a real number and W(-i,4*N).
# This leads to the following code, called the AAN algorithm :

def unscaled_DCT_AAN (N, input, output):
    DFT_input = vector (2*N)
    DFT_output = vector (2*N)

    for i in range(N):
	DFT_input[i] = input[i]
	DFT_input[2*N-1-i] = input[i]

    symetrical_unscaled_DFT (2*N, DFT_input, DFT_output)

    for i in range(N):
	output[i] = DFT_output[i].real * (0.5 / C (i, N))

# Notes about the AAN algorithm :

# The cost of this function is N real multiplies and a DFT of size 2*N. The
# DFT to calculate has special properties : the inputs are real and symmetric.
# Also, we only need to calculate the real parts of the N first DFT outputs.
# We can try to take advantage of all that.

# We can invert this algorithm to calculate the IDCT. The final multiply
# stage is trivially invertible. The DFT stage is invertible too, but we have
# to take into account the special properties of this particular DFT for that.

# Once again we have to take care of numerical precision for the DFT : the
# output coefficients can get large, so that a small error in the input will
# give a large error on the output... For a DCT of size N the biggest
# coefficient will be at i=N/2-1 and it will be slightly more than N/pi

# You can find another description of this algorithm at this url :
# www.cmlab.csie.ntu.edu.tw/cml/dsp/training/coding/transform/fastdct.html
# (It is the same server where we already found a description of the fast DFT)


# To optimize the DFT calculation, we can take a lot of specific things into
# account : the input is real and symetric, and we only care about the real
# part of the output. Also, we only care about the N first output coefficients,
# but that one does not save operations actually, because the other
# coefficients are the conjugates of the ones we look anyway.

# One useful way to use the symmetry of the input is to use the radix-2
# decimation-in-frequency algorithm. We can write a version of
# unscaled_DFT_radix2_freq for the case where the input is symmetrical :
# we have removed a few additions in the first stages because even_input
# is symmetrical and odd_input is antisymetrical. Also, we have modified the
# odd_input vector so that the second half of it is set to zero and the real
# part of the DFT output is not modified. After that modification, the second
# part of the odd_input was null so we used the radix-2 decimation-in-frequency
# again on the odd DFT. Also odd_output is symmetrical because input is real...

def symetrical_unscaled_DFT (N, input, output):
    even_input = vector(N/2)
    odd_tmp = vector(N/2)
    odd_input = vector(N/2)
    even_output = vector(N/2)
    odd_output = vector(N/2)

    for i in range(N/4):
	even_input[N/2-i-1] = even_input[i] = input[i] + input[N/2-1-i]

    for i in range(N/4):
	odd_tmp[i] = input[i] - input[N/2-1-i]

    odd_input[0] = odd_tmp[0]
    for i in range(N/4)[1:]:
	odd_input[i] = (odd_tmp[i] + odd_tmp[i-1]) * W (i, N)

    unscaled_DFT (N/2, even_input, even_output)
    # symmetrical real inputs, real outputs

    unscaled_DFT (N/4, odd_input, odd_output)
    # complex inputs, real outputs

    for i in range(N/2):
	output[2*i] = even_output[i]

    for i in range(N/4):
	output[N-1-4*i] = output[4*i+1] = odd_output[i]

# This procedure takes 3*N/4-1 real additions and N/2-3 real multiplies,
# followed by another symmetrical real DFT of size N/2 and a "complex to real"
# DFT of size N/4.

# We thus get the following performance results :
#	N	real additions	real multiplies	complex multiplies
#       1               0               0               0
#       2               0               0               0
#       4               2               0               0
#       8               9               1               0
#      16              28               6               0
#      32              76              21               0
#      64             189              54               2
#     128             451             125              10
#     256            1042             270              36
#     512            2358             565             108
#    1024            5251            1158             294
#    2048           11557            2349             750
#    4096           25200            4734            1832
#    8192           54544            9509            4336
#   16384          117337           19062           10026
#   32768          251127           38173           22770
#   65536          535102           76398           50988


# We thus get a better performance with the AAN DCT algorithm than with the
# Lee DCT algorithm : we can do a DCT of size 32 with 189 additions, 54+32 real
# multiplies, and 2 complex multiplies. The Lee algorithm would have used 209
# additions and 80 multiplies. With the AAN algorithm, we also have the
# advantage that a big number of the multiplies are actually grouped at the
# output stage of the algorithm, so if we want to do a DCT followed by a
# quantization stage, we will be able to group the multiply of the output with
# the multiply of the quantization stage, thus saving 32 more operations. In
# the mpeg audio layer 1 or 2 processing, we can also group the multiply of the
# output with the multiply of the convolution stage...

# Another source code for the AAN algorithm (implemented on 8 points, and
# without all of the explanations) can be found at this URL :
# http://developer.intel.com/drg/pentiumII/appnotes/aan_org.c . This
# implementation uses 28 adds and 6+8 muls instead of 29 adds and 5+8 muls -
# the difference is that in the symetrical_unscaled_DFT procedure, they noticed
# how odd_input[i] and odd_input[N/4-i] will be combined at the start of the
# complex-to-real DFT and they took advantage of this to convert 2 real adds
# and 4 real muls into one complex multiply.


# TODO : write about multi-dimentional DCT


# TEST CODE

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
	str = str + realstr #+ imagstr
    return "[%s]" % str

def test(N):
    input = vector(N)
    output = vector(N)
    verify = vector(N)

    for i in range(N):
	input[i] = random() + 1j * random()

    unscaled_DFT (N, input, output)
    unscaled_DFT (N, input, verify)

    if (dump(output) != dump(verify)):
	print dump(output)
	print dump(verify)

#test (64)


# PERFORMANCE ANALYSIS CODE

def display (table):
    N = 1
    print "#\tN\treal additions\treal multiplies\tcomplex multiplies"
    while table.has_key(N):
	print "#%8d%16d%16d%16d" % (N, table[N][0], table[N][1], table[N][2])
	N = 2*N
    print

best_complex_DFT = {}

def complex_DFT (max_N):
    best_complex_DFT[1] = (0,0,0)
    best_complex_DFT[2] = (4,0,0)
    best_complex_DFT[4] = (16,0,0)
    N = 8
    while (N<=max_N):
	# best method = split radix
	best2 = best_complex_DFT[N/2]
	best4 = best_complex_DFT[N/4]
	best_complex_DFT[N] = (best2[0] + 2*best4[0] + 3*N + 4,
			       best2[1] + 2*best4[1] + 4,
			       best2[2] + 2*best4[2] + N/2 - 4)
	N = 2*N

best_real_DFT = {}

def real_DFT (max_N):
    best_real_DFT[1] = (0,0,0)
    best_real_DFT[2] = (2,0,0)
    best_real_DFT[4] = (6,0,0)
    N = 8
    while (N<=max_N):
	# best method = split radix decimate-in-frequency
	best2 = best_real_DFT[N/2]
	best4 = best_complex_DFT[N/4]
	best_real_DFT[N] = (best2[0] + best4[0] + N + 2,
			    best2[1] + best4[1] + 2,
			    best2[2] + best4[2] + N/4 - 2)
	N = 2*N

best_complex2real_DFT = {}

def complex2real_DFT (max_N):
    best_complex2real_DFT[1] = (0,0,0)
    best_complex2real_DFT[2] = (2,0,0)
    best_complex2real_DFT[4] = (8,0,0)
    N = 8
    while (N<=max_N):
	best2 = best_complex2real_DFT[N/2]
	best4 = best_complex_DFT[N/4]
	best_complex2real_DFT[N] = (best2[0] + best4[0] + 3*N/2 + 1,
				    best2[1] + best4[1] + 2,
				    best2[2] + best4[2] + N/4 - 2)
	N = 2*N

best_real_symetric_DFT = {}

def real_symetric_DFT (max_N):
    best_real_symetric_DFT[1] = (0,0,0)
    best_real_symetric_DFT[2] = (0,0,0)
    best_real_symetric_DFT[4] = (2,0,0)
    N = 8
    while (N<=max_N):
	best2 = best_real_symetric_DFT[N/2]
	best4 = best_complex2real_DFT[N/4]
	best_real_symetric_DFT[N] = (best2[0] + best4[0] + 3*N/4 - 1,
				     best2[1] + best4[1] + N/2 - 3,
				     best2[2] + best4[2])
	N = 2*N

complex_DFT (65536)
real_DFT (65536)
complex2real_DFT (65536)
real_symetric_DFT (65536)


print "complex DFT"
display (best_complex_DFT)

print "real DFT"
display (best_real_DFT)

print "complex2real DFT"
display (best_complex2real_DFT)

print "real symetric DFT"
display (best_real_symetric_DFT)

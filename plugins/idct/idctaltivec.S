#*****************************************************************************
# idctaltivec.S : Altivec IDCT code
#*****************************************************************************
# Copyright (C) 1999, 2001 VideoLAN
# $Id: idctaltivec.S,v 1.1 2001/09/05 16:07:49 massiot Exp $
# 
# Author: Paul Mackerras <paulus@linuxcare.com>
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
#*****************************************************************************/

	.data
	.align	4
wvec:	.long	0x3f0a8bd4
	.long	0x3e8d42af
	.long	0x3f3504f3
	.long	0x3f968317
	.long	0x3f8e39da
	.long	0x3fd4db31
	.long	0x3ec7c5c2
	.long	0x3ffb14be
	.long	0x3f43ef15
	.long	0x3fec835e
	.long	0
	.long	0

d:	.long	0,0,0,0

	.text
  	.globl	idct_block_copy_altivec
idct_block_copy_altivec:
  	li	6,0
  	b	idct_asm_altivec

  	.global	idct_block_add_altivec
idct_block_add_altivec:
  	li	6,1

	.globl	idct_asm_altivec
idct_asm_altivec:
	lvx	22,0,3		#	 ih = *(vector signed short *)(p);
	addi	3,3,16		#	 p += 8;
	vupkhsh	20,22		#	 i0 = vec_unpackh(ih);
	vupklsh	21,22		#	 i1 = vec_unpackl(ih);
	vcfsx	0,20,3		#	 x00 = vec_ctf(i0, 3);
	vcfsx	10,21,3		#	 x01 = vec_ctf(i1, 3);
	lvx	22,0,3		#	 ih = *(vector signed short *)(p);
	addi	3,3,16		#	 p += 8;
	vupkhsh	20,22		#	 i0 = vec_unpackh(ih);
	vupklsh	21,22		#	 i1 = vec_unpackl(ih);
	vcfsx	1,20,3		#	 x10 = vec_ctf(i0, 3);
	vcfsx	11,21,3		#	 x11 = vec_ctf(i1, 3);
	lvx	22,0,3		#	 ih = *(vector signed short *)(p);
	addi	3,3,16		#	 p += 8;
	vupkhsh	20,22		#	 i0 = vec_unpackh(ih);
	vupklsh	21,22		#	 i1 = vec_unpackl(ih);
	vcfsx	2,20,3		#	 x20 = vec_ctf(i0, 3);
	vcfsx	12,21,3		#	 x21 = vec_ctf(i1, 3);
	lvx	22,0,3		#	 ih = *(vector signed short *)(p);
	addi	3,3,16		#	 p += 8;
	vupkhsh	20,22		#	 i0 = vec_unpackh(ih);
	vupklsh	21,22		#	 i1 = vec_unpackl(ih);
	vcfsx	3,20,3		#	 x30 = vec_ctf(i0, 3);
	vcfsx	13,21,3		#	 x31 = vec_ctf(i1, 3);
	lvx	22,0,3		#	 ih = *(vector signed short *)(p);
	addi	3,3,16		#	 p += 8;
	vupkhsh	20,22		#	 i0 = vec_unpackh(ih);
	vupklsh	21,22		#	 i1 = vec_unpackl(ih);
	vcfsx	4,20,3		#	 x40 = vec_ctf(i0, 3);
	vcfsx	14,21,3		#	 x41 = vec_ctf(i1, 3);
	lvx	22,0,3		#	 ih = *(vector signed short *)(p);
	addi	3,3,16		#	 p += 8;
	vupkhsh	20,22		#	 i0 = vec_unpackh(ih);
	vupklsh	21,22		#	 i1 = vec_unpackl(ih);
	vcfsx	5,20,3		#	 x50 = vec_ctf(i0, 3);
	vcfsx	15,21,3		#	 x51 = vec_ctf(i1, 3);
	lvx	22,0,3		#	 ih = *(vector signed short *)(p);
	addi	3,3,16		#	 p += 8;
	vupkhsh	20,22		#	 i0 = vec_unpackh(ih);
	vupklsh	21,22		#	 i1 = vec_unpackl(ih);
	vcfsx	6,20,3		#	 x60 = vec_ctf(i0, 3);
	vcfsx	16,21,3		#	 x61 = vec_ctf(i1, 3);
	lvx	22,0,3		#	 ih = *(vector signed short *)(p);
	vupkhsh	20,22		#	 i0 = vec_unpackh(ih);
	vupklsh	21,22		#	 i1 = vec_unpackl(ih);
	vcfsx	7,20,3		#	 x70 = vec_ctf(i0, 3);
	vcfsx	17,21,3		#	 x71 = vec_ctf(i1, 3);

	vmrghw	8,0,2		#  	x80 = vec_mergeh(x00, x20);
	vmrghw	9,1,3		#  	x90 = vec_mergeh(x10, x30);
	vmrglw	18,0,2		#  	x81 = vec_mergel(x00, x20);
	vmrglw	19,1,3		#  	x91 = vec_mergel(x10, x30);
	vmrghw	0,8,9		#  	x00 = vec_mergeh(x80, x90);
	vmrglw	1,8,9		#  	x10 = vec_mergel(x80, x90);
	vmrghw	2,18,19		#  	x20 = vec_mergeh(x81, x91);
	vmrglw	3,18,19		#  	x30 = vec_mergel(x81, x91);

	vmrghw	8,10,12		#  	x80 = vec_mergeh(x01, x21);
	vmrghw	9,11,13		#  	x90 = vec_mergeh(x11, x31);
	vmrglw	18,10,12	#  	x81 = vec_mergel(x01, x21);
	vmrglw	19,11,13	#  	x91 = vec_mergel(x11, x31);
	vmrghw	20,4,6		#  	y80 = vec_mergeh(x40, x60);
	vmrghw	22,5,7		#  	y90 = vec_mergeh(x50, x70);
	vmrglw	21,4,6		#  	y81 = vec_mergel(x40, x60);
	vmrglw	23,5,7		#  	y91 = vec_mergel(x50, x70);
	vmrghw	4,8,9		#  	x40 = vec_mergeh(x80, x90);
	vmrglw	5,8,9		#  	x50 = vec_mergel(x80, x90);
	vmrghw	6,18,19		#  	x60 = vec_mergeh(x81, x91);
	vmrglw	7,18,19		#  	x70 = vec_mergel(x81, x91);
	vmrghw	10,20,22	#  	x01 = vec_mergeh(y80, y90);
	vmrglw	11,20,22	#  	x11 = vec_mergel(y80, y90);
	vmrghw	12,21,23	#  	x21 = vec_mergeh(y81, y91);
	vmrglw	13,21,23	#  	x31 = vec_mergel(y81, y91);

	vmrghw	20,14,16	#  	y80 = vec_mergeh(x41, x61);
	vmrghw	22,15,17	#  	y90 = vec_mergeh(x51, x71);
	vmrglw	21,14,16	#  	y81 = vec_mergel(x41, x61);
	vmrglw	23,15,17	#  	y91 = vec_mergel(x51, x71);
	vmrghw	14,20,22	#  	x41 = vec_mergeh(y80, y90);
	vmrglw	15,20,22	#  	x51 = vec_mergel(y80, y90);
	vmrghw	16,21,23	#  	x61 = vec_mergeh(y81, y91);
	vmrglw	17,21,23	#  	x71 = vec_mergel(y81, y91);

	lis	7,wvec@ha
	addi	7,7,wvec@l
	addi	8,7,16
	addi	9,7,32
	lvx	28,0,7		#  	*(vector float *)wvec2;
	lvx	29,0,8		#  	*(vector float *)wvec3;
	lvx	30,0,9		#  	*(vector float *)wvec4;

	vspltw	20,28,3		#  	W3 = vec_splat(wvec2, 3);
	vspltw	21,28,1		#  	W7 = vec_splat(wvec2, 1);
	vspltw	22,29,0		#  	W1_W7 = vec_splat(wvec3, 0);
	vspltw	23,29,1		#  	W1pW7 = vec_splat(wvec3, 1);
	vspltw	24,29,2		#  	W3_W5 = vec_splat(wvec3, 2);
	vspltw	25,29,3		#  	W3pW5 = vec_splat(wvec3, 3);
	vspltisw 31,0		#	z = (vector float)(0);

				#  	/* first stage */
	vaddfp	26,1,7
	vmaddfp	8,21,26,31	#  	x80 = vec_madd(W7, vec_add(x10, x70), z);
	vaddfp	27,11,17
	vmaddfp	18,21,27,31	#  	x81 = vec_madd(W7, vec_add(x11, x71), z);
	vmaddfp	1,22,1,8	#  	x10 = vec_madd(W1_W7, x10, x80);
	vmaddfp	11,22,11,18	#  	x11 = vec_madd(W1_W7, x11, x81);
	vnmsubfp 7,23,7,8	#  	x70 = vec_nmsub(W1pW7, x70, x80);
	vnmsubfp 17,23,17,18	#  	x71 = vec_nmsub(W1pW7, x71, x81);
	vaddfp	26,5,3
	vmaddfp	8,20,26,31	#  	x80 = vec_madd(W3, vec_add(x50, x30), z);
	vaddfp	27,15,13
	vmaddfp	18,20,27,31	#  	x81 = vec_madd(W3, vec_add(x51, x31), z);
	vnmsubfp 5,24,5,8	#  	x50 = vec_nmsub(W3_W5, x50, x80);
	vnmsubfp 15,24,15,18	#  	x51 = vec_nmsub(W3_W5, x51, x81);
	vnmsubfp 3,25,3,8	#  	x30 = vec_nmsub(W3pW5, x30, x80);
	vnmsubfp 13,25,13,18	#  	x31 = vec_nmsub(W3pW5, x31, x81);
 
	vspltw	20,28,0		#  	W6 = vec_splat(wvec2, 0);
	vspltw	21,30,0		#  	W2_W6 = vec_splat(wvec4, 0);
	vspltw	22,30,1		#  	W2pW6 = vec_splat(wvec4, 1);
	vspltw	23,28,2		#  	SQRT0_5 = vec_splat(wvec2, 2);

				#  	/* second stage */
	vaddfp	8,0,4		#  	x80 = vec_add(x00, x40);
	vaddfp	18,10,14	#  	x81 = vec_add(x01, x41);
	vsubfp	0,0,4		#  	x00 = vec_sub(x00, x40);
	vsubfp	10,10,14	#  	x01 = vec_sub(x01, x41);
	vaddfp	26,2,6
	vmaddfp	4,20,26,31	#  	x40 = vec_madd(W6, vec_add(x20, x60), z);
	vaddfp	27,12,16
	vmaddfp	14,20,27,31	#  	x41 = vec_madd(W6, vec_add(x21, x61), z);
	vnmsubfp 6,22,6,4	#  	x60 = vec_nmsub(W2pW6, x60, x40);
	vnmsubfp 16,22,16,14	#  	x61 = vec_nmsub(W2pW6, x61, x41);
	vmaddfp	2,21,2,4	#  	x20 = vec_madd(W2_W6, x20, x40);
	vmaddfp	12,21,12,14	#  	x21 = vec_madd(W2_W6, x21, x41);
	vaddfp	4,1,5		#  	x40 = vec_add(x10, x50);
	vaddfp	14,11,15	#  	x41 = vec_add(x11, x51);
	vsubfp	1,1,5		#  	x10 = vec_sub(x10, x50);
	vsubfp	11,11,15	#  	x11 = vec_sub(x11, x51);
	vaddfp	5,7,3		#  	x50 = vec_add(x70, x30);
	vaddfp	15,17,13	#  	x51 = vec_add(x71, x31);
	vsubfp	7,7,3		#  	x70 = vec_sub(x70, x30);
	vsubfp	17,17,13	#  	x71 = vec_sub(x71, x31);
 
				#  	/* third stage */
	vaddfp	3,8,2		#  	x30 = vec_add(x80, x20);
	vaddfp	13,18,12	#  	x31 = vec_add(x81, x21);
	vsubfp	8,8,2		#  	x80 = vec_sub(x80, x20);
	vsubfp	18,18,12	#  	x81 = vec_sub(x81, x21);
	vaddfp	2,0,6		#  	x20 = vec_add(x00, x60);
	vaddfp	12,10,16	#  	x21 = vec_add(x01, x61);
	vsubfp	0,0,6		#  	x00 = vec_sub(x00, x60);
	vsubfp	10,10,16	#  	x01 = vec_sub(x01, x61);
	vaddfp	24,1,7
	vmaddfp	6,23,24,31	#  	x60 = vec_madd(SQRT0_5, vec_add(x10, x70), z);
	vaddfp	25,11,17
	vmaddfp	16,23,25,31	#  	x61 = vec_madd(SQRT0_5, vec_add(x11, x71), z);
	vsubfp	26,1,7
	vmaddfp	1,23,26,31	#  	x10 = vec_madd(SQRT0_5, vec_sub(x10, x70), z);
	vsubfp	27,11,17
	vmaddfp	11,23,27,31	#  	x11 = vec_madd(SQRT0_5, vec_sub(x11, x71), z);

				#  	/* fourth stage */
	vsubfp	7,3,4		#  	x70 = vec_sub(x30, x40);
	vsubfp	17,13,14	#  	x71 = vec_sub(x31, x41);
	vaddfp	9,3,4		#  	x90 = vec_add(x30, x40);
	vaddfp	19,13,14	#  	x91 = vec_add(x31, x41);
	vaddfp	3,8,5		#  	x30 = vec_add(x80, x50);
	vaddfp	13,18,15	#  	x31 = vec_add(x81, x51);
	vsubfp	4,8,5		#  	x40 = vec_sub(x80, x50);
	vsubfp	14,18,15	#  	x41 = vec_sub(x81, x51);
	vsubfp	5,0,1		#  	x50 = vec_sub(x00, x10);
	vsubfp	15,10,11	#  	x51 = vec_sub(x01, x11);
	vaddfp	8,0,1		#  	x80 = vec_add(x00, x10);
	vaddfp	18,10,11	#  	x81 = vec_add(x01, x11);
	vaddfp	1,2,6		#  	x10 = vec_add(x20, x60);
	vaddfp	11,12,16	#  	x11 = vec_add(x21, x61);
	vsubfp	6,2,6		#  	x60 = vec_sub(x20, x60);
	vsubfp	16,12,16	#  	x61 = vec_sub(x21, x61);
				#  	/* x0* is now in x9*, x2* is in x8* */

	vmrghw	20,9,8		#        y80 = vec_mergeh(x90, x80);
	vmrghw	22,1,3		#        y90 = vec_mergeh(x10, x30);
	vmrglw	21,9,8		#        y81 = vec_mergel(x90, x80);
	vmrglw	23,1,3		#        y91 = vec_mergel(x10, x30);
	vmrghw	0,20,22		#        x00 = vec_mergeh(y80, y90);
	vmrglw	1,20,22		#        x10 = vec_mergel(y80, y90);
	vmrghw	2,21,23		#        x20 = vec_mergeh(y81, y91);
	vmrglw	3,21,23		#        x30 = vec_mergel(y81, y91);

	vmrghw	8,19,18		#        x80 = vec_mergeh(x91, x81);
	vmrghw	9,11,13		#        x90 = vec_mergeh(x11, x31);
	vmrglw	18,19,18	#        x81 = vec_mergel(x91, x81);
	vmrglw	19,11,13	#        x91 = vec_mergel(x11, x31);
	vmrghw	20,4,6		#        y80 = vec_mergeh(x40, x60);
	vmrghw	22,5,7		#        y90 = vec_mergeh(x50, x70);
	vmrglw	21,4,6		#        y81 = vec_mergel(x40, x60);
	vmrglw	23,5,7		#        y91 = vec_mergel(x50, x70);
	vmrghw	4,8,9		#        x40 = vec_mergeh(x80, x90);
	vmrglw	5,8,9		#        x50 = vec_mergel(x80, x90);
	vmrghw	6,18,19		#        x60 = vec_mergeh(x81, x91);
	vmrglw	7,18,19		#        x70 = vec_mergel(x81, x91);
	vmrghw	10,20,22	#        x01 = vec_mergeh(y80, y90);
	vmrglw	11,20,22	#        x11 = vec_mergel(y80, y90);
	vmrghw	12,21,23	#        x21 = vec_mergeh(y81, y91);
	vmrglw	13,21,23	#        x31 = vec_mergel(y81, y91);

	vmrghw	20,14,16	#        y80 = vec_mergeh(x41, x61);
	vmrghw	22,15,17	#        y90 = vec_mergeh(x51, x71);
	vmrglw	21,14,16	#        y81 = vec_mergel(x41, x61);
	vmrglw	23,15,17	#        y91 = vec_mergel(x51, x71);
	vmrghw	14,20,22	#        x41 = vec_mergeh(y80, y90);
	vmrglw	15,20,22	#        x51 = vec_mergel(y80, y90);
	vmrghw	16,21,23	#        x61 = vec_mergeh(y81, y91);
	vmrglw	17,21,23	#        x71 = vec_mergel(y81, y91);

	vspltw	20,28,3		#  	W3 = vec_splat(wvec2, 3);
	vspltw	21,28,1		#  	W7 = vec_splat(wvec2, 1);
	vspltw	22,29,0		#  	W1_W7 = vec_splat(wvec3, 0);
	vspltw	23,29,1		#  	W1pW7 = vec_splat(wvec3, 1);
	vspltw	24,29,2		#  	W3_W5 = vec_splat(wvec3, 2);
	vspltw	25,29,3		#  	W3pW5 = vec_splat(wvec3, 3);

				#  	/* first stage */
	vaddfp	26,1,7
	vmaddfp	8,21,26,31	#  	x80 = vec_madd(W7, vec_add(x10, x70), z);
	vaddfp	27,11,17
	vmaddfp	18,21,27,31	#  	x81 = vec_madd(W7, vec_add(x11, x71), z);
	vmaddfp	1,22,1,8	#  	x10 = vec_madd(W1_W7, x10, x80);
	vmaddfp	11,22,11,18	#  	x11 = vec_madd(W1_W7, x11, x81);
	vnmsubfp 7,23,7,8	#  	x70 = vec_nmsub(W1pW7, x70, x80);
	vnmsubfp 17,23,17,18	#  	x71 = vec_nmsub(W1pW7, x71, x81);
	vaddfp	26,5,3
	vmaddfp	8,20,26,31	#  	x80 = vec_madd(W3, vec_add(x50, x30), z);
	vaddfp	27,15,13
	vmaddfp	18,20,27,31	#  	x81 = vec_madd(W3, vec_add(x51, x31), z);
	vnmsubfp 5,24,5,8	#  	x50 = vec_nmsub(W3_W5, x50, x80);
	vnmsubfp 15,24,15,18	#  	x51 = vec_nmsub(W3_W5, x51, x81);
	vnmsubfp 3,25,3,8	#  	x30 = vec_nmsub(W3pW5, x30, x80);
	vnmsubfp 13,25,13,18	#  	x31 = vec_nmsub(W3pW5, x31, x81);
 
	vspltw	20,28,0		#  	W6 = vec_splat(wvec2, 0);
	vspltw	21,30,0		#  	W2_W6 = vec_splat(wvec4, 0);
	vspltw	22,30,1		#  	W2pW6 = vec_splat(wvec4, 1);
	vspltw	23,28,2		#  	SQRT0_5 = vec_splat(wvec2, 2);

				#  	/* second stage */
	vaddfp	8,0,4		#  	x80 = vec_add(x00, x40);
	vaddfp	18,10,14	#  	x81 = vec_add(x01, x41);
	vsubfp	0,0,4		#  	x00 = vec_sub(x00, x40);
	vsubfp	10,10,14	#  	x01 = vec_sub(x01, x41);
	vaddfp	26,2,6
	vmaddfp	4,20,26,31	#  	x40 = vec_madd(W6, vec_add(x20, x60), z);
	vaddfp	27,12,16
	vmaddfp	14,20,27,31	#  	x41 = vec_madd(W6, vec_add(x21, x61), z);
	vnmsubfp 6,22,6,4	#  	x60 = vec_nmsub(W2pW6, x60, x40);
	vnmsubfp 16,22,16,14	#  	x61 = vec_nmsub(W2pW6, x61, x41);
	vmaddfp	2,21,2,4	#  	x20 = vec_madd(W2_W6, x20, x40);
	vmaddfp	12,21,12,14	#  	x21 = vec_madd(W2_W6, x21, x41);
	vaddfp	4,1,5		#  	x40 = vec_add(x10, x50);
	vaddfp	14,11,15	#  	x41 = vec_add(x11, x51);
	vsubfp	1,1,5		#  	x10 = vec_sub(x10, x50);
	vsubfp	11,11,15	#  	x11 = vec_sub(x11, x51);
	vaddfp	5,7,3		#  	x50 = vec_add(x70, x30);
	vaddfp	15,17,13	#  	x51 = vec_add(x71, x31);
	vsubfp	7,7,3		#  	x70 = vec_sub(x70, x30);
	vsubfp	17,17,13	#  	x71 = vec_sub(x71, x31);
 
				#  	/* third stage */
	vaddfp	3,8,2		#  	x30 = vec_add(x80, x20);
	vaddfp	13,18,12	#  	x31 = vec_add(x81, x21);
	vsubfp	8,8,2		#  	x80 = vec_sub(x80, x20);
	vsubfp	18,18,12	#  	x81 = vec_sub(x81, x21);
	vaddfp	2,0,6		#  	x20 = vec_add(x00, x60);
	vaddfp	12,10,16	#  	x21 = vec_add(x01, x61);
	vsubfp	0,0,6		#  	x00 = vec_sub(x00, x60);
	vsubfp	10,10,16	#  	x01 = vec_sub(x01, x61);
	vaddfp	24,1,7
	vmaddfp	6,23,24,31	#  	x60 = vec_madd(SQRT0_5, vec_add(x10, x70), z);
	vaddfp	25,11,17
	vmaddfp	16,23,25,31	#  	x61 = vec_madd(SQRT0_5, vec_add(x11, x71), z);
	vsubfp	26,1,7
	vmaddfp	1,23,26,31	#  	x10 = vec_madd(SQRT0_5, vec_sub(x10, x70), z);
	vsubfp	27,11,17
	vmaddfp	11,23,27,31	#  	x11 = vec_madd(SQRT0_5, vec_sub(x11, x71), z);

				#  	/* fourth stage */
	vsubfp	7,3,4		#  	x70 = vec_sub(x30, x40);
	vsubfp	17,13,14	#  	x71 = vec_sub(x31, x41);
	vaddfp	9,3,4		#  	x90 = vec_add(x30, x40);
	vaddfp	19,13,14	#  	x91 = vec_add(x31, x41);
	vaddfp	3,8,5		#  	x30 = vec_add(x80, x50);
	vaddfp	13,18,15	#  	x31 = vec_add(x81, x51);
	vsubfp	4,8,5		#  	x40 = vec_sub(x80, x50);
	vsubfp	14,18,15	#  	x41 = vec_sub(x81, x51);
	vsubfp	5,0,1		#  	x50 = vec_sub(x00, x10);
	vsubfp	15,10,11	#  	x51 = vec_sub(x01, x11);
	vaddfp	8,0,1		#  	x80 = vec_add(x00, x10);
	vaddfp	18,10,11	#  	x81 = vec_add(x01, x11);
	vaddfp	1,2,6		#  	x10 = vec_add(x20, x60);
	vaddfp	11,12,16	#  	x11 = vec_add(x21, x61);
	vsubfp	6,2,6		#  	x60 = vec_sub(x20, x60);
	vsubfp	16,12,16	#  	x61 = vec_sub(x21, x61);
				#  	/* x0* is now in x9*, x2* is in x8* */

	cmpwi	6,0
	lis	6,d@ha
	addi	6,6,d@l
	vctsxs	20,9,0		#  	i0 = vec_cts(x90, 0);
	vctsxs	21,19,0		#  	i1 = vec_cts(x91, 0);
	vpkswss	22,20,21	#  	ih = vec_packs(i0, i1);
	beq	1f		#  	if (accum) {
	lfd	0,0(4)
	stfd	0,0(6)		#  		*(long long *)&d = *(long long *)dest;
	lvx	24,0,6
	vmrghb	23,31,24	#  		dh = (vector signed short) vec_mergeh(zb, d);
	vaddshs	22,23,22	#  		ih = vec_adds(dh, ih);
1:				#  	}
	vpkshus	24,22,31
	stvx	24,0,6		#  	d = vec_packsu(ih, zh);
	lfd	0,0(6)	
	stfd	0,0(4)		#  	*(long long *)dest = *(long long *)&d;
	add	4,4,5		#  	dest += stride;
	vctsxs	20,1,0		#  	i0 = vec_cts(x10, 0);
	vctsxs	21,11,0		#  	i1 = vec_cts(x11, 0);
	vpkswss	22,20,21	#  	ih = vec_packs(i0, i1);
	beq	1f		#  	if (accum) {
	lfd	0,0(4)
	stfd	0,0(6)		#  		*(long long *)&d = *(long long *)dest;
	lvx	24,0,6
	vmrghb	23,31,24	#  		dh = (vector signed short) vec_mergeh(zb, d);
	vaddshs	22,23,22	#  		ih = vec_adds(dh, ih);
1:				#  	}
	vpkshus	24,22,31
	stvx	24,0,6		#  	d = vec_packsu(ih, zh);
	lfd	0,0(6)	
	stfd	0,0(4)		#  	*(long long *)dest = *(long long *)&d;
	add	4,4,5		#  	dest += stride;
	vctsxs	20,8,0		#  	i0 = vec_cts(x80, 0);
	vctsxs	21,18,0		#  	i1 = vec_cts(x81, 0);
	vpkswss	22,20,21	#  	ih = vec_packs(i0, i1);
	beq	1f		#  	if (accum) {
	lfd	0,0(4)
	stfd	0,0(6)		#  		*(long long *)&d = *(long long *)dest;
	lvx	24,0,6
	vmrghb	23,31,24	#  		dh = (vector signed short) vec_mergeh(zb, d);
	vaddshs	22,23,22	#  		ih = vec_adds(dh, ih);
1:				#  	}
	vpkshus	24,22,31
	stvx	24,0,6		#  	d = vec_packsu(ih, zh);
	lfd	0,0(6)	
	stfd	0,0(4)		#  	*(long long *)dest = *(long long *)&d;
	add	4,4,5		#  	dest += stride;
	vctsxs	20,3,0		#  	i0 = vec_cts(x30, 0);
	vctsxs	21,13,0		#  	i1 = vec_cts(x31, 0);
	vpkswss	22,20,21	#  	ih = vec_packs(i0, i1);
	beq	1f		#  	if (accum) {
	lfd	0,0(4)
	stfd	0,0(6)		#  		*(long long *)&d = *(long long *)dest;
	lvx	24,0,6
	vmrghb	23,31,24	#  		dh = (vector signed short) vec_mergeh(zb, d);
	vaddshs	22,23,22	#  		ih = vec_adds(dh, ih);
1:				#  	}
	vpkshus	24,22,31
	stvx	24,0,6		#  	d = vec_packsu(ih, zh);
	lfd	0,0(6)	
	stfd	0,0(4)		#  	*(long long *)dest = *(long long *)&d;
	add	4,4,5		#  	dest += stride;
	vctsxs	20,4,0		#  	i0 = vec_cts(x40, 0);
	vctsxs	21,14,0		#  	i1 = vec_cts(x41, 0);
	vpkswss	22,20,21	#  	ih = vec_packs(i0, i1);
	beq	1f		#  	if (accum) {
	lfd	0,0(4)
	stfd	0,0(6)		#  		*(long long *)&d = *(long long *)dest;
	lvx	24,0,6
	vmrghb	23,31,24	#  		dh = (vector signed short) vec_mergeh(zb, d);
	vaddshs	22,23,22	#  		ih = vec_adds(dh, ih);
1:				#  	}
	vpkshus	24,22,31
	stvx	24,0,6		#  	d = vec_packsu(ih, zh);
	lfd	0,0(6)	
	stfd	0,0(4)		#  	*(long long *)dest = *(long long *)&d;
	add	4,4,5		#  	dest += stride;
	vctsxs	20,5,0		#  	i0 = vec_cts(x50, 0);
	vctsxs	21,15,0		#  	i1 = vec_cts(x51, 0);
	vpkswss	22,20,21	#  	ih = vec_packs(i0, i1);
	beq	1f		#  	if (accum) {
	lfd	0,0(4)
	stfd	0,0(6)		#  		*(long long *)&d = *(long long *)dest;
	lvx	24,0,6
	vmrghb	23,31,24	#  		dh = (vector signed short) vec_mergeh(zb, d);
	vaddshs	22,23,22	#  		ih = vec_adds(dh, ih);
1:				#  	}
	vpkshus	24,22,31
	stvx	24,0,6		#  	d = vec_packsu(ih, zh);
	lfd	0,0(6)	
	stfd	0,0(4)		#  	*(long long *)dest = *(long long *)&d;
	add	4,4,5		#  	dest += stride;
	vctsxs	20,6,0		#  	i0 = vec_cts(x60, 0);
	vctsxs	21,16,0		#  	i1 = vec_cts(x61, 0);
	vpkswss	22,20,21	#  	ih = vec_packs(i0, i1);
	beq	1f		#  	if (accum) {
	lfd	0,0(4)
	stfd	0,0(6)		#  		*(long long *)&d = *(long long *)dest;
	lvx	24,0,6
	vmrghb	23,31,24	#  		dh = (vector signed short) vec_mergeh(zb, d);
	vaddshs	22,23,22	#  		ih = vec_adds(dh, ih);
1:				#  	}
	vpkshus	24,22,31
	stvx	24,0,6		#  	d = vec_packsu(ih, zh);
	lfd	0,0(6)	
	stfd	0,0(4)		#  	*(long long *)dest = *(long long *)&d;
	add	4,4,5		#  	dest += stride;
	vctsxs	20,7,0		#  	i0 = vec_cts(x70, 0);
	vctsxs	21,17,0		#  	i1 = vec_cts(x71, 0);
	vpkswss	22,20,21	#  	ih = vec_packs(i0, i1);
	beq	1f		#  	if (accum) {
	lfd	0,0(4)
	stfd	0,0(6)		#  		*(long long *)&d = *(long long *)dest;
	lvx	24,0,6
	vmrghb	23,31,24	#  		dh = (vector signed short) vec_mergeh(zb, d);
	vaddshs	22,23,22	#  		ih = vec_adds(dh, ih);
1:				#  	}
	vpkshus	24,22,31
	stvx	24,0,6		#  	d = vec_packsu(ih, zh);
	lfd	0,0(6)	
	stfd	0,0(4)		#  	*(long long *)dest = *(long long *)&d;

	blr

/* { dg-do assemble { target aarch64_asm_sve2p1_ok } } */
/* { dg-do compile { target { ! aarch64_asm_sve2p1_ok } } } */
/* { dg-final { check-function-bodies "**" "" "-DCHECK_ASM" { target { ! ilp32 } } } } */

#include "test_sve_acle.h"

#pragma GCC target "+sve2p1"
#ifdef STREAMING_COMPATIBLE
#pragma GCC target "+sme2"
#endif

/*
** ld1_s16_base:
**	ld1h	{z0\.h(?: - |, )z1\.h}, pn8/z, \[x0\]
**	ret
*/
TEST_LOAD_COUNT (ld1_s16_base, svint16x2_t, int16_t,
		 z0 = svld1_s16_x2 (pn8, x0),
		 z0 = svld1_x2 (pn8, x0))

/*
** ld1_s16_index:
**	ld1h	{z0\.h(?: - |, )z1\.h}, pn8/z, \[x0, x1, lsl #?1\]
**	ret
*/
TEST_LOAD_COUNT (ld1_s16_index, svint16x2_t, int16_t,
		 z0 = svld1_s16_x2 (pn8, x0 + x1),
		 z0 = svld1_x2 (pn8, x0 + x1))

/* Moving the constant into a register would also be OK.  */
/*
** ld1_s16_1:
**	incb	x0
**	ld1h	{z0\.h(?: - |, )z1\.h}, pn8/z, \[x0\]
**	ret
*/
TEST_LOAD_COUNT (ld1_s16_1, svint16x2_t, int16_t,
		 z0 = svld1_s16_x2 (pn8, x0 + svcnth ()),
		 z0 = svld1_x2 (pn8, x0 + svcnth ()))

/*
** ld1_s16_2:
**	ld1h	{z0\.h(?: - |, )z1\.h}, pn8/z, \[x0, #2, mul vl\]
**	ret
*/
TEST_LOAD_COUNT (ld1_s16_2, svint16x2_t, int16_t,
		 z0 = svld1_s16_x2 (pn8, x0 + svcnth () * 2),
		 z0 = svld1_x2 (pn8, x0 + svcnth () * 2))

/*
** ld1_s16_14:
**	ld1h	{z0\.h(?: - |, )z1\.h}, pn8/z, \[x0, #14, mul vl\]
**	ret
*/
TEST_LOAD_COUNT (ld1_s16_14, svint16x2_t, int16_t,
		 z0 = svld1_s16_x2 (pn8, x0 + svcnth () * 14),
		 z0 = svld1_x2 (pn8, x0 + svcnth () * 14))

/* Moving the constant into a register would also be OK.  */
/*
** ld1_s16_16:
**	incb	x0, all, mul #16
**	ld1h	{z0\.h(?: - |, )z1\.h}, pn8/z, \[x0\]
**	ret
*/
TEST_LOAD_COUNT (ld1_s16_16, svint16x2_t, int16_t,
		 z0 = svld1_s16_x2 (pn8, x0 + svcnth () * 16),
		 z0 = svld1_x2 (pn8, x0 + svcnth () * 16))

/* Moving the constant into a register would also be OK.  */
/*
** ld1_s16_m1:
**	decb	x0
**	ld1h	{z0\.h(?: - |, )z1\.h}, pn8/z, \[x0\]
**	ret
*/
TEST_LOAD_COUNT (ld1_s16_m1, svint16x2_t, int16_t,
		 z0 = svld1_s16_x2 (pn8, x0 - svcnth ()),
		 z0 = svld1_x2 (pn8, x0 - svcnth ()))

/*
** ld1_s16_m2:
**	ld1h	{z0\.h(?: - |, )z1\.h}, pn8/z, \[x0, #-2, mul vl\]
**	ret
*/
TEST_LOAD_COUNT (ld1_s16_m2, svint16x2_t, int16_t,
		 z0 = svld1_s16_x2 (pn8, x0 - svcnth () * 2),
		 z0 = svld1_x2 (pn8, x0 - svcnth () * 2))

/*
** ld1_s16_m16:
**	ld1h	{z0\.h(?: - |, )z1\.h}, pn8/z, \[x0, #-16, mul vl\]
**	ret
*/
TEST_LOAD_COUNT (ld1_s16_m16, svint16x2_t, int16_t,
		 z0 = svld1_s16_x2 (pn8, x0 - svcnth () * 16),
		 z0 = svld1_x2 (pn8, x0 - svcnth () * 16))

/*
** ld1_s16_m18:
**	addvl	(x[0-9]+), x0, #-18
**	ld1h	{z0\.h(?: - |, )z1\.h}, pn8/z, \[\1\]
**	ret
*/
TEST_LOAD_COUNT (ld1_s16_m18, svint16x2_t, int16_t,
		 z0 = svld1_s16_x2 (pn8, x0 - svcnth () * 18),
		 z0 = svld1_x2 (pn8, x0 - svcnth () * 18))

/*
** ld1_s16_z17:
**	ld1h	{z[^\n]+}, pn8/z, \[x0\]
**	mov	[^\n]+
**	mov	[^\n]+
**	ret
*/
TEST_LOAD_COUNT (ld1_s16_z17, svint16x2_t, int16_t,
		 z17 = svld1_s16_x2 (pn8, x0),
		 z17 = svld1_x2 (pn8, x0))

/*
** ld1_s16_z22:
**	ld1h	{z22\.h(?: - |, )z23\.h}, pn8/z, \[x0\]
**	ret
*/
TEST_LOAD_COUNT (ld1_s16_z22, svint16x2_t, int16_t,
		 z22 = svld1_s16_x2 (pn8, x0),
		 z22 = svld1_x2 (pn8, x0))

/*
** ld1_s16_z28:
**	ld1h	{z28\.h(?: - |, )z29\.h}, pn8/z, \[x0\]
**	ret
*/
TEST_LOAD_COUNT (ld1_s16_z28, svint16x2_t, int16_t,
		 z28 = svld1_s16_x2 (pn8, x0),
		 z28 = svld1_x2 (pn8, x0))

/*
** ld1_s16_pn0:
**	mov	p([89]|1[0-5])\.b, p0\.b
**	ld1h	{z0\.h(?: - |, )z1\.h}, pn\1/z, \[x0\]
**	ret
*/
TEST_LOAD_COUNT (ld1_s16_pn0, svint16x2_t, int16_t,
		 z0 = svld1_s16_x2 (pn0, x0),
		 z0 = svld1_x2 (pn0, x0))

/*
** ld1_s16_pn7:
**	mov	p([89]|1[0-5])\.b, p7\.b
**	ld1h	{z0\.h(?: - |, )z1\.h}, pn\1/z, \[x0\]
**	ret
*/
TEST_LOAD_COUNT (ld1_s16_pn7, svint16x2_t, int16_t,
		 z0 = svld1_s16_x2 (pn7, x0),
		 z0 = svld1_x2 (pn7, x0))

/*
** ld1_s16_pn15:
**	ld1h	{z0\.h(?: - |, )z1\.h}, pn15/z, \[x0\]
**	ret
*/
TEST_LOAD_COUNT (ld1_s16_pn15, svint16x2_t, int16_t,
		 z0 = svld1_s16_x2 (pn15, x0),
		 z0 = svld1_x2 (pn15, x0))

/*
** ld1_vnum_s16_0:
**	ld1h	{z0\.h(?: - |, )z1\.h}, pn8/z, \[x0\]
**	ret
*/
TEST_LOAD_COUNT (ld1_vnum_s16_0, svint16x2_t, int16_t,
		 z0 = svld1_vnum_s16_x2 (pn8, x0, 0),
		 z0 = svld1_vnum_x2 (pn8, x0, 0))

/* Moving the constant into a register would also be OK.  */
/*
** ld1_vnum_s16_1:
**	incb	x0
**	ld1h	{z0\.h(?: - |, )z1\.h}, pn8/z, \[x0\]
**	ret
*/
TEST_LOAD_COUNT (ld1_vnum_s16_1, svint16x2_t, int16_t,
		 z0 = svld1_vnum_s16_x2 (pn8, x0, 1),
		 z0 = svld1_vnum_x2 (pn8, x0, 1))

/*
** ld1_vnum_s16_2:
**	ld1h	{z0\.h(?: - |, )z1\.h}, pn8/z, \[x0, #2, mul vl\]
**	ret
*/
TEST_LOAD_COUNT (ld1_vnum_s16_2, svint16x2_t, int16_t,
		 z0 = svld1_vnum_s16_x2 (pn8, x0, 2),
		 z0 = svld1_vnum_x2 (pn8, x0, 2))

/*
** ld1_vnum_s16_14:
**	ld1h	{z0\.h(?: - |, )z1\.h}, pn8/z, \[x0, #14, mul vl\]
**	ret
*/
TEST_LOAD_COUNT (ld1_vnum_s16_14, svint16x2_t, int16_t,
		 z0 = svld1_vnum_s16_x2 (pn8, x0, 14),
		 z0 = svld1_vnum_x2 (pn8, x0, 14))

/* Moving the constant into a register would also be OK.  */
/*
** ld1_vnum_s16_16:
**	incb	x0, all, mul #16
**	ld1h	{z0\.h(?: - |, )z1\.h}, pn8/z, \[x0\]
**	ret
*/
TEST_LOAD_COUNT (ld1_vnum_s16_16, svint16x2_t, int16_t,
		 z0 = svld1_vnum_s16_x2 (pn8, x0, 16),
		 z0 = svld1_vnum_x2 (pn8, x0, 16))

/* Moving the constant into a register would also be OK.  */
/*
** ld1_vnum_s16_m1:
**	decb	x0
**	ld1h	{z0\.h(?: - |, )z1\.h}, pn8/z, \[x0\]
**	ret
*/
TEST_LOAD_COUNT (ld1_vnum_s16_m1, svint16x2_t, int16_t,
		 z0 = svld1_vnum_s16_x2 (pn8, x0, -1),
		 z0 = svld1_vnum_x2 (pn8, x0, -1))

/*
** ld1_vnum_s16_m2:
**	ld1h	{z0\.h(?: - |, )z1\.h}, pn8/z, \[x0, #-2, mul vl\]
**	ret
*/
TEST_LOAD_COUNT (ld1_vnum_s16_m2, svint16x2_t, int16_t,
		 z0 = svld1_vnum_s16_x2 (pn8, x0, -2),
		 z0 = svld1_vnum_x2 (pn8, x0, -2))

/*
** ld1_vnum_s16_m16:
**	ld1h	{z0\.h(?: - |, )z1\.h}, pn8/z, \[x0, #-16, mul vl\]
**	ret
*/
TEST_LOAD_COUNT (ld1_vnum_s16_m16, svint16x2_t, int16_t,
		 z0 = svld1_vnum_s16_x2 (pn8, x0, -16),
		 z0 = svld1_vnum_x2 (pn8, x0, -16))

/*
** ld1_vnum_s16_m18:
**	addvl	(x[0-9]+), x0, #-18
**	ld1h	{z0\.h(?: - |, )z1\.h}, pn8/z, \[\1\]
**	ret
*/
TEST_LOAD_COUNT (ld1_vnum_s16_m18, svint16x2_t, int16_t,
		 z0 = svld1_vnum_s16_x2 (pn8, x0, -18),
		 z0 = svld1_vnum_x2 (pn8, x0, -18))

/*
** ld1_vnum_s16_x1:
**	cntb	(x[0-9]+)
** (
**	madd	(x[0-9]+), (?:x1, \1|\1, x1), x0
**	ld1h	{z0\.h(?: - |, )z1\.h}, pn8/z, \[\2\]
** |
**	mul	(x[0-9]+), (?:x1, \1|\1, x1)
**	ld1h	{z0\.h(?: - |, )z1\.h}, pn8/z, \[x0, \3\]
** )
**	ret
*/
TEST_LOAD_COUNT (ld1_vnum_s16_x1, svint16x2_t, int16_t,
		 z0 = svld1_vnum_s16_x2 (pn8, x0, x1),
		 z0 = svld1_vnum_x2 (pn8, x0, x1))

/* { dg-final { check-function-bodies "**" "" "-DCHECK_ASM" } } */

#include "test_sve_acle.h"

/*
** trn2_mf8_tied1:
**	trn2	z0\.b, z0\.b, z1\.b
**	ret
*/
TEST_UNIFORM_Z (trn2_mf8_tied1, svmfloat8_t,
		z0 = svtrn2_mf8 (z0, z1),
		z0 = svtrn2 (z0, z1))

/*
** trn2_mf8_tied2:
**	trn2	z0\.b, z1\.b, z0\.b
**	ret
*/
TEST_UNIFORM_Z (trn2_mf8_tied2, svmfloat8_t,
		z0 = svtrn2_mf8 (z1, z0),
		z0 = svtrn2 (z1, z0))

/*
** trn2_mf8_untied:
**	trn2	z0\.b, z1\.b, z2\.b
**	ret
*/
TEST_UNIFORM_Z (trn2_mf8_untied, svmfloat8_t,
		z0 = svtrn2_mf8 (z1, z2),
		z0 = svtrn2 (z1, z2))

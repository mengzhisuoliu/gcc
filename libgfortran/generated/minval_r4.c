/* Implementation of the MINVAL intrinsic
   Copyright (C) 2002-2025 Free Software Foundation, Inc.
   Contributed by Paul Brook <paul@nowt.org>

This file is part of the GNU Fortran runtime library (libgfortran).

Libgfortran is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public
License as published by the Free Software Foundation; either
version 3 of the License, or (at your option) any later version.

Libgfortran is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

Under Section 7 of GPL version 3, you are granted additional
permissions described in the GCC Runtime Library Exception, version
3.1, as published by the Free Software Foundation.

You should have received a copy of the GNU General Public License and
a copy of the GCC Runtime Library Exception along with this program;
see the files COPYING3 and COPYING.RUNTIME respectively.  If not, see
<http://www.gnu.org/licenses/>.  */

#include "libgfortran.h"


#if defined (HAVE_GFC_REAL_4) && defined (HAVE_GFC_REAL_4)


extern void minval_r4 (gfc_array_r4 * const restrict,
	gfc_array_r4 * const restrict, const index_type * const restrict);
export_proto(minval_r4);

void
minval_r4 (gfc_array_r4 * const restrict retarray,
	gfc_array_r4 * const restrict array,
	const index_type * const restrict pdim)
{
  index_type count[GFC_MAX_DIMENSIONS];
  index_type extent[GFC_MAX_DIMENSIONS];
  index_type sstride[GFC_MAX_DIMENSIONS];
  index_type dstride[GFC_MAX_DIMENSIONS];
  const GFC_REAL_4 * restrict base;
  GFC_REAL_4 * restrict dest;
  index_type rank;
  index_type n;
  index_type len;
  index_type delta;
  index_type dim;
  int continue_loop;

  /* Make dim zero based to avoid confusion.  */
  rank = GFC_DESCRIPTOR_RANK (array) - 1;
  dim = (*pdim) - 1;

  if (unlikely (dim < 0 || dim > rank))
    {
      runtime_error ("Dim argument incorrect in MINVAL intrinsic: "
 		     "is %ld, should be between 1 and %ld",
		     (long int) dim + 1, (long int) rank + 1);
    }

  len = GFC_DESCRIPTOR_EXTENT(array,dim);
  if (len < 0)
    len = 0;
  delta = GFC_DESCRIPTOR_STRIDE(array,dim);

  for (n = 0; n < dim; n++)
    {
      sstride[n] = GFC_DESCRIPTOR_STRIDE(array,n);
      extent[n] = GFC_DESCRIPTOR_EXTENT(array,n);

      if (extent[n] < 0)
	extent[n] = 0;
    }
  for (n = dim; n < rank; n++)
    {
      sstride[n] = GFC_DESCRIPTOR_STRIDE(array, n + 1);
      extent[n] = GFC_DESCRIPTOR_EXTENT(array, n + 1);

      if (extent[n] < 0)
	extent[n] = 0;
    }

  if (retarray->base_addr == NULL)
    {
      size_t alloc_size, str;

      for (n = 0; n < rank; n++)
	{
	  if (n == 0)
	    str = 1;
	  else
	    str = GFC_DESCRIPTOR_STRIDE(retarray,n-1) * extent[n-1];

	  GFC_DIMENSION_SET(retarray->dim[n], 0, extent[n] - 1, str);

	}

      retarray->offset = 0;
      retarray->dtype.rank = rank;

      alloc_size = GFC_DESCRIPTOR_STRIDE(retarray,rank-1) * extent[rank-1];

      retarray->base_addr = xmallocarray (alloc_size, sizeof (GFC_REAL_4));
      if (alloc_size == 0)
	return;
    }
  else
    {
      if (rank != GFC_DESCRIPTOR_RANK (retarray))
	runtime_error ("rank of return array incorrect in"
		       " MINVAL intrinsic: is %ld, should be %ld",
		       (long int) (GFC_DESCRIPTOR_RANK (retarray)),
		       (long int) rank);

      if (unlikely (compile_options.bounds_check))
	bounds_ifunction_return ((array_t *) retarray, extent,
				 "return value", "MINVAL");
    }

  for (n = 0; n < rank; n++)
    {
      count[n] = 0;
      dstride[n] = GFC_DESCRIPTOR_STRIDE(retarray,n);
      if (extent[n] <= 0)
	return;
    }

  base = array->base_addr;
  dest = retarray->base_addr;

  continue_loop = 1;
  while (continue_loop)
    {
      const GFC_REAL_4 * restrict src;
      GFC_REAL_4 result;
      src = base;
      {

#if defined (GFC_REAL_4_INFINITY)
	result = GFC_REAL_4_INFINITY;
#else
	result = GFC_REAL_4_HUGE;
#endif
	if (len <= 0)
	  *dest = GFC_REAL_4_HUGE;
	else
	  {
#if ! defined HAVE_BACK_ARG
	    for (n = 0; n < len; n++, src += delta)
	      {
#endif

#if defined (GFC_REAL_4_QUIET_NAN)
		if (*src <= result)
		  break;
	      }
	    if (unlikely (n >= len))
	      result = GFC_REAL_4_QUIET_NAN;
	    else for (; n < len; n++, src += delta)
	      {
#endif
		if (*src < result)
		  result = *src;
	      }
	    
	    *dest = result;
	  }
      }
      /* Advance to the next element.  */
      count[0]++;
      base += sstride[0];
      dest += dstride[0];
      n = 0;
      while (count[n] == extent[n])
	{
	  /* When we get to the end of a dimension, reset it and increment
	     the next dimension.  */
	  count[n] = 0;
	  /* We could precalculate these products, but this is a less
	     frequently used path so probably not worth it.  */
	  base -= sstride[n] * extent[n];
	  dest -= dstride[n] * extent[n];
	  n++;
	  if (n >= rank)
	    {
	      /* Break out of the loop.  */
	      continue_loop = 0;
	      break;
	    }
	  else
	    {
	      count[n]++;
	      base += sstride[n];
	      dest += dstride[n];
	    }
	}
    }
}


extern void mminval_r4 (gfc_array_r4 * const restrict,
	gfc_array_r4 * const restrict, const index_type * const restrict,
	gfc_array_l1 * const restrict);
export_proto(mminval_r4);

void
mminval_r4 (gfc_array_r4 * const restrict retarray,
	gfc_array_r4 * const restrict array,
	const index_type * const restrict pdim,
	gfc_array_l1 * const restrict mask)
{
  index_type count[GFC_MAX_DIMENSIONS];
  index_type extent[GFC_MAX_DIMENSIONS];
  index_type sstride[GFC_MAX_DIMENSIONS];
  index_type dstride[GFC_MAX_DIMENSIONS];
  index_type mstride[GFC_MAX_DIMENSIONS];
  GFC_REAL_4 * restrict dest;
  const GFC_REAL_4 * restrict base;
  const GFC_LOGICAL_1 * restrict mbase;
  index_type rank;
  index_type dim;
  index_type n;
  index_type len;
  index_type delta;
  index_type mdelta;
  int mask_kind;

  if (mask == NULL)
    {
#ifdef HAVE_BACK_ARG
      minval_r4 (retarray, array, pdim, back);
#else
      minval_r4 (retarray, array, pdim);
#endif
      return;
    }

  dim = (*pdim) - 1;
  rank = GFC_DESCRIPTOR_RANK (array) - 1;


  if (unlikely (dim < 0 || dim > rank))
    {
      runtime_error ("Dim argument incorrect in MINVAL intrinsic: "
 		     "is %ld, should be between 1 and %ld",
		     (long int) dim + 1, (long int) rank + 1);
    }

  len = GFC_DESCRIPTOR_EXTENT(array,dim);
  if (len < 0)
    len = 0;

  mbase = mask->base_addr;

  mask_kind = GFC_DESCRIPTOR_SIZE (mask);

  if (mask_kind == 1 || mask_kind == 2 || mask_kind == 4 || mask_kind == 8
#ifdef HAVE_GFC_LOGICAL_16
      || mask_kind == 16
#endif
      )
    mbase = GFOR_POINTER_TO_L1 (mbase, mask_kind);
  else
    runtime_error ("Funny sized logical array");

  delta = GFC_DESCRIPTOR_STRIDE(array,dim);
  mdelta = GFC_DESCRIPTOR_STRIDE_BYTES(mask,dim);

  for (n = 0; n < dim; n++)
    {
      sstride[n] = GFC_DESCRIPTOR_STRIDE(array,n);
      mstride[n] = GFC_DESCRIPTOR_STRIDE_BYTES(mask,n);
      extent[n] = GFC_DESCRIPTOR_EXTENT(array,n);

      if (extent[n] < 0)
	extent[n] = 0;

    }
  for (n = dim; n < rank; n++)
    {
      sstride[n] = GFC_DESCRIPTOR_STRIDE(array,n + 1);
      mstride[n] = GFC_DESCRIPTOR_STRIDE_BYTES(mask, n + 1);
      extent[n] = GFC_DESCRIPTOR_EXTENT(array, n + 1);

      if (extent[n] < 0)
	extent[n] = 0;
    }

  if (retarray->base_addr == NULL)
    {
      size_t alloc_size, str;

      for (n = 0; n < rank; n++)
	{
	  if (n == 0)
	    str = 1;
	  else
	    str= GFC_DESCRIPTOR_STRIDE(retarray,n-1) * extent[n-1];

	  GFC_DIMENSION_SET(retarray->dim[n], 0, extent[n] - 1, str);

	}

      alloc_size = GFC_DESCRIPTOR_STRIDE(retarray,rank-1) * extent[rank-1];

      retarray->offset = 0;
      retarray->dtype.rank = rank;

      retarray->base_addr = xmallocarray (alloc_size, sizeof (GFC_REAL_4));
      if (alloc_size == 0)
	return;
    }
  else
    {
      if (rank != GFC_DESCRIPTOR_RANK (retarray))
	runtime_error ("rank of return array incorrect in MINVAL intrinsic");

      if (unlikely (compile_options.bounds_check))
	{
	  bounds_ifunction_return ((array_t *) retarray, extent,
				   "return value", "MINVAL");
	  bounds_equal_extents ((array_t *) mask, (array_t *) array,
	  			"MASK argument", "MINVAL");
	}
    }

  for (n = 0; n < rank; n++)
    {
      count[n] = 0;
      dstride[n] = GFC_DESCRIPTOR_STRIDE(retarray,n);
      if (extent[n] <= 0)
	return;
    }

  dest = retarray->base_addr;
  base = array->base_addr;

  while (base)
    {
      const GFC_REAL_4 * restrict src;
      const GFC_LOGICAL_1 * restrict msrc;
      GFC_REAL_4 result;
      src = base;
      msrc = mbase;
      {

#if defined (GFC_REAL_4_INFINITY)
	result = GFC_REAL_4_INFINITY;
#else
	result = GFC_REAL_4_HUGE;
#endif
#if defined (GFC_REAL_4_QUIET_NAN)
	int non_empty_p = 0;
#endif
	for (n = 0; n < len; n++, src += delta, msrc += mdelta)
	  {

#if defined (GFC_REAL_4_INFINITY) || defined (GFC_REAL_4_QUIET_NAN)
		if (*msrc)
		  {
#if defined (GFC_REAL_4_QUIET_NAN)
		    non_empty_p = 1;
		    if (*src <= result)
#endif
		      break;
		  }
	      }
	    if (unlikely (n >= len))
	      {
#if defined (GFC_REAL_4_QUIET_NAN)
		result = non_empty_p ? GFC_REAL_4_QUIET_NAN : GFC_REAL_4_HUGE;
#else
		result = GFC_REAL_4_HUGE;
#endif
	      }
	    else for (; n < len; n++, src += delta, msrc += mdelta)
	      {
#endif
		if (*msrc && *src < result)
		  result = *src;
	  }
	*dest = result;
      }
      /* Advance to the next element.  */
      count[0]++;
      base += sstride[0];
      mbase += mstride[0];
      dest += dstride[0];
      n = 0;
      while (count[n] == extent[n])
	{
	  /* When we get to the end of a dimension, reset it and increment
	     the next dimension.  */
	  count[n] = 0;
	  /* We could precalculate these products, but this is a less
	     frequently used path so probably not worth it.  */
	  base -= sstride[n] * extent[n];
	  mbase -= mstride[n] * extent[n];
	  dest -= dstride[n] * extent[n];
	  n++;
	  if (n >= rank)
	    {
	      /* Break out of the loop.  */
	      base = NULL;
	      break;
	    }
	  else
	    {
	      count[n]++;
	      base += sstride[n];
	      mbase += mstride[n];
	      dest += dstride[n];
	    }
	}
    }
}


extern void sminval_r4 (gfc_array_r4 * const restrict,
	gfc_array_r4 * const restrict, const index_type * const restrict,
	GFC_LOGICAL_4 *);
export_proto(sminval_r4);

void
sminval_r4 (gfc_array_r4 * const restrict retarray,
	gfc_array_r4 * const restrict array,
	const index_type * const restrict pdim,
	GFC_LOGICAL_4 * mask)
{
  index_type count[GFC_MAX_DIMENSIONS];
  index_type extent[GFC_MAX_DIMENSIONS];
  index_type dstride[GFC_MAX_DIMENSIONS];
  GFC_REAL_4 * restrict dest;
  index_type rank;
  index_type n;
  index_type dim;


  if (mask == NULL || *mask)
    {
#ifdef HAVE_BACK_ARG
      minval_r4 (retarray, array, pdim, back);
#else
      minval_r4 (retarray, array, pdim);
#endif
      return;
    }
  /* Make dim zero based to avoid confusion.  */
  dim = (*pdim) - 1;
  rank = GFC_DESCRIPTOR_RANK (array) - 1;

  if (unlikely (dim < 0 || dim > rank))
    {
      runtime_error ("Dim argument incorrect in MINVAL intrinsic: "
 		     "is %ld, should be between 1 and %ld",
		     (long int) dim + 1, (long int) rank + 1);
    }

  for (n = 0; n < dim; n++)
    {
      extent[n] = GFC_DESCRIPTOR_EXTENT(array,n);

      if (extent[n] <= 0)
	extent[n] = 0;
    }

  for (n = dim; n < rank; n++)
    {
      extent[n] =
	GFC_DESCRIPTOR_EXTENT(array,n + 1);

      if (extent[n] <= 0)
	extent[n] = 0;
    }

  if (retarray->base_addr == NULL)
    {
      size_t alloc_size, str;

      for (n = 0; n < rank; n++)
	{
	  if (n == 0)
	    str = 1;
	  else
	    str = GFC_DESCRIPTOR_STRIDE(retarray,n-1) * extent[n-1];

	  GFC_DIMENSION_SET(retarray->dim[n], 0, extent[n] - 1, str);

	}

      retarray->offset = 0;
      retarray->dtype.rank = rank;

      alloc_size = GFC_DESCRIPTOR_STRIDE(retarray,rank-1) * extent[rank-1];

      retarray->base_addr = xmallocarray (alloc_size, sizeof (GFC_REAL_4));
      if (alloc_size == 0)
	return;
    }
  else
    {
      if (rank != GFC_DESCRIPTOR_RANK (retarray))
	runtime_error ("rank of return array incorrect in"
		       " MINVAL intrinsic: is %ld, should be %ld",
		       (long int) (GFC_DESCRIPTOR_RANK (retarray)),
		       (long int) rank);

      if (unlikely (compile_options.bounds_check))
	{
	  for (n=0; n < rank; n++)
	    {
	      index_type ret_extent;

	      ret_extent = GFC_DESCRIPTOR_EXTENT(retarray,n);
	      if (extent[n] != ret_extent)
		runtime_error ("Incorrect extent in return value of"
			       " MINVAL intrinsic in dimension %ld:"
			       " is %ld, should be %ld", (long int) n + 1,
			       (long int) ret_extent, (long int) extent[n]);
	    }
	}
    }

  for (n = 0; n < rank; n++)
    {
      count[n] = 0;
      dstride[n] = GFC_DESCRIPTOR_STRIDE(retarray,n);
    }

  dest = retarray->base_addr;

  while(1)
    {
      *dest = GFC_REAL_4_HUGE;
      count[0]++;
      dest += dstride[0];
      n = 0;
      while (count[n] == extent[n])
	{
	  /* When we get to the end of a dimension, reset it and increment
	     the next dimension.  */
	  count[n] = 0;
	  /* We could precalculate these products, but this is a less
	     frequently used path so probably not worth it.  */
	  dest -= dstride[n] * extent[n];
	  n++;
	  if (n >= rank)
	    return;
	  else
	    {
	      count[n]++;
	      dest += dstride[n];
	    }
      	}
    }
}

#endif

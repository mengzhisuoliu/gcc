/* Copyright (C) 2018-2025 Free Software Foundation, Inc.

   This file is part of GCC.

   GCC is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   GCC is distributed in the hope that it will be useful,
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

#ifndef _X86GPRINTRIN_H_INCLUDED
# error "Never use <serializeintrin.h> directly; include <x86gprintrin.h> instead."
#endif

#ifndef _SERIALIZE_H_INCLUDED
#define _SERIALIZE_H_INCLUDED

#ifndef __SERIALIZE__
#pragma GCC push_options
#pragma GCC target("serialize")
#define __DISABLE_SERIALIZE__
#endif /* __SERIALIZE__ */

extern __inline void
__attribute__((__gnu_inline__, __always_inline__, __artificial__))
_serialize (void)
{
  __builtin_ia32_serialize ();
}

#ifdef __DISABLE_SERIALIZE__
#undef __DISABLE_SERIALIZE__
#pragma GCC pop_options
#endif /* __DISABLE_SERIALIZE__ */

#endif /* _SERIALIZE_H_INCLUDED.  */

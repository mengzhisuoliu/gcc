/* Definitions of target machine for GCC,
   for SPARC running in an embedded environment using the ELF file format.
   Copyright (C) 2005-2025 Free Software Foundation, Inc.

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3, or (at your option)
any later version.

GCC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING3.  If not see
<http://www.gnu.org/licenses/>.  */

/* It's safe to pass -s always, even if -g is not used.  */
#undef ASM_SPEC
#define ASM_SPEC \
  "-s \
   %{" FPIE_OR_FPIC_SPEC ":-K PIC} %(asm_cpu)"

/* Use the default.  */
#undef LINK_SPEC

#undef STARTFILE_SPEC
#define STARTFILE_SPEC "crt0.o%s crti.o%s crtbegin.o%s"

#undef ENDFILE_SPEC
#define ENDFILE_SPEC \
  "%{Ofast|ffast-math|funsafe-math-optimizations:%{!shared:crtfastmath.o%s}} \
   crtend.o%s crtn.o%s"

/* Don't set the target flags, this is done by the linker script */
#undef LIB_SPEC
#define LIB_SPEC ""

#undef  LOCAL_LABEL_PREFIX
#define LOCAL_LABEL_PREFIX  "."

/* This is how to store into the string LABEL
   the symbol_ref name of an internal numbered label where
   PREFIX is the class of label and NUM is the number within the class.
   This is suitable for output with `assemble_name'.  */

#undef  ASM_GENERATE_INTERNAL_LABEL
#define ASM_GENERATE_INTERNAL_LABEL(LABEL,PREFIX,NUM)	\
  sprintf ((LABEL), "*.L%s%ld", (PREFIX), (long)(NUM))

/* We use GNU ld so undefine this so that attribute((init_priority)) works.  */
#undef CTORS_SECTION_ASM_OP
#undef DTORS_SECTION_ASM_OP

/* ??? Inherited from sol2.h.  Probably wrong.  */
#undef WCHAR_TYPE
#define WCHAR_TYPE "long int"

#undef WCHAR_TYPE_SIZE
#define WCHAR_TYPE_SIZE BITS_PER_WORD

/* ??? until fixed.  */
#undef SPARC_LONG_DOUBLE_TYPE_SIZE
#define SPARC_LONG_DOUBLE_TYPE_SIZE 64

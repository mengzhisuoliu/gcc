/* GNU compiler hardware transactional execution intrinsics
   Copyright (C) 2013-2025 Free Software Foundation, Inc.
   Contributed by Andreas Krebbel (Andreas.Krebbel@de.ibm.com)

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 3, or (at your option) any later
version.

GCC is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING3.  If not see
<http://www.gnu.org/licenses/>.  */

#ifndef _HTMINTRIN_H
#define _HTMINTRIN_H


/* Condition codes generated by tbegin  */
#define _HTM_TBEGIN_STARTED       0
#define _HTM_TBEGIN_INDETERMINATE 1
#define _HTM_TBEGIN_TRANSIENT     2
#define _HTM_TBEGIN_PERSISTENT    3

/* The abort codes below this threshold are reserved for machine
   use.  */
#define _HTM_FIRST_USER_ABORT_CODE 256

/* The transaction diagnostic block is it is defined in the Principles
   of Operation chapter 5-91.  */

struct __htm_tdb {
  unsigned char format;                /*   0 */
  unsigned char flags;
  unsigned char reserved1[4];
  unsigned short nesting_depth;
  unsigned long long abort_code;       /*   8 */
  unsigned long long conflict_token;   /*  16 */
  unsigned long long atia;             /*  24 */
  unsigned char eaid;                  /*  32 */
  unsigned char dxc;
  unsigned char reserved2[2];
  unsigned int program_int_id;
  unsigned long long exception_id;     /*  40 */
  unsigned long long bea;              /*  48 */
  unsigned char reserved3[72];         /*  56 */
  unsigned long long gprs[16];         /* 128 */
} __attribute__((__packed__, __aligned__ (8)));


#endif /* _HTMINTRIN_H */

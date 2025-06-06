/* dtoa.c provide floating point string conversion routines.

Copyright (C) 2009-2025 Free Software Foundation, Inc.
Contributed by Gaius Mulley <gaius@glam.ac.uk>.

This file is part of GNU Modula-2.

GNU Modula-2 is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3, or (at your option)
any later version.

GNU Modula-2 is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

Under Section 7 of GPL version 3, you are granted additional
permissions described in the GCC Runtime Library Exception, version
3.1, as published by the Free Software Foundation.

You should have received a copy of the GNU General Public License and
a copy of the GCC Runtime Library Exception along with this program;
see the files COPYING3 and COPYING.RUNTIME respectively.  If not, see
<http://www.gnu.org/licenses/>.  */

#include "config.h"
#include "system.h"
#include "ansidecl.h"

#include "gm2-libs-host.h"
#include "m2rts.h"

#define LIBNAME "m2pim"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_FP_DIGITS 500

typedef enum Mode { maxsignicant, decimaldigits } Mode;

/* maxsignicant: return a string containing max(1,ndigits)
   significant digits.  The return string contains the string
   produced by ecvt.  decimaldigits: return a string produced by
   fcvt.  The string will contain ndigits past the decimal point
   (ndigits may be negative).  */

double
dtoa_strtod (const char *s, bool *error)
{
  char *endp;
  double d;

  errno = 0;
  d = strtod (s, &endp);
  if (endp != NULL && (*endp == '\0'))
    *error = (errno != 0);
  else
    *error = true;
  return d;
}

/* dtoa_calcmaxsig - calculates the position of the decimal point it
   also removes the decimal point and exponent from string, p.  */

int
dtoa_calcmaxsig (char *p, int ndigits)
{
  char *e;
  char *o;
  int x;

  e = index (p, 'E');
  if (e == NULL)
    x = 0;
  else
    {
      *e = (char)0;
      x = atoi (e + 1);
    }

  o = index (p, '.');
  if (o == NULL)
    return strlen (p) + x;
  else
    {
      memmove (o, o + 1, ndigits - (o - p));
      return o - p + x;
    }
}

/* dtoa_calcdecimal - calculates the position of the decimal point it
   also removes the decimal point and exponent from string, p.  It
   truncates the digits in p accordingly to ndigits.  Ie ndigits is
   the number of digits after the '.' */

int
dtoa_calcdecimal (char *p, int str_size, int ndigits)
{
  char *e;
  char *o;
  int x;
  int l;

  e = index (p, 'E');
  if (e == NULL)
    x = 0;
  else
    {
      *e = (char)0;
      x = atoi (e + 1);
    }

  l = strlen (p);
  o = index (p, '.');
  if (o == NULL)
    x += strlen (p);
  else
    {
      int m = strlen (o);
      memmove (o, o + 1, l - (o - p));
      if (m > 0)
        o[m - 1] = '0';
      x += o - p;
    }
  if ((x + ndigits >= 0) && (x + ndigits < str_size))
    p[x + ndigits] = (char)0;
  return x;
}

bool
dtoa_calcsign (char *p, int str_size)
{
  if (p[0] == '-')
    {
      memmove (p, p + 1, str_size - 1);
      return true;
    }
  else
    return false;
}

char *
dtoa_dtoa (double d, int mode, int ndigits, int *decpt, int *sign)
{
  char format[50];
  char *p;
  int r;
  switch (mode)
    {

    case maxsignicant:
      ndigits += 20; /* enough for exponent.  */
      p = (char *) malloc (ndigits);
      snprintf (format, 50, "%s%d%s", "%.", ndigits - 20, "E");
      snprintf (p, ndigits, format, d);
      *sign = dtoa_calcsign (p, ndigits);
      *decpt = dtoa_calcmaxsig (p, ndigits);
      return p;
    case decimaldigits:
      p = (char *) malloc (MAX_FP_DIGITS + 20);
      snprintf (format, 50, "%s%d%s", "%.", MAX_FP_DIGITS, "E");
      snprintf (p, MAX_FP_DIGITS + 20, format, d);
      *sign = dtoa_calcsign (p, MAX_FP_DIGITS + 20);
      *decpt = dtoa_calcdecimal (p, MAX_FP_DIGITS + 20, ndigits);
      return p;
    default:
      abort ();
    }
}

/* GNU Modula-2 hooks */

void
_M2_dtoa_init (int, char **, char **)
{
}

void
_M2_dtoa_finish (int, char **, char **)
{
}

void
_M2_dtoa_dep (void)
{
}

#ifdef __cplusplus
}

extern "C" void __attribute__((__constructor__))
_M2_dtoa_ctor (void)
{
  M2RTS_RegisterModule_Cstr ("dtoa", LIBNAME, _M2_dtoa_init,
			     _M2_dtoa_finish, _M2_dtoa_dep);
}

#else
void
_M2_dtoa_ctor (void)
{
}

#endif

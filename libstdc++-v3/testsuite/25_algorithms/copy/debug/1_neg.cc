// Copyright (C) 2019-2025 Free Software Foundation, Inc.
//
// This file is part of the GNU ISO C++ Library.  This library is free
// software; you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the
// Free Software Foundation; either version 3, or (at your option)
// any later version.

// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License along
// with this library; see the file COPYING3.  If not see
// <http://www.gnu.org/licenses/>.

// 25.2.1 [lib.alg.copy] Copy.

// { dg-do run { xfail *-*-* } }
// { dg-require-debug-mode "" }

#include <algorithm>
#include <list>
#include <vector>

void
test01()
{
  std::list<int> l(10, 1);
  std::vector<int> v(5, 0);

  std::copy(++l.begin(), --l.end(), v.begin());
}

int
main()
{
  test01();
  return 0;
}

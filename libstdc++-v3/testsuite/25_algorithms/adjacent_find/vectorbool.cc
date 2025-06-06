// Copyright (C) 2013-2025 Free Software Foundation, Inc.
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

// 25.1.5 [lib.alg.adjacent_find]

#include <vector>
#include <algorithm>
#include <testsuite_hooks.h>

void 
test01()
{
  std::vector<bool> v;
  v.push_back(true);
  v.push_back(false);
  v.push_back(true);
  v.push_back(false);
  v.push_back(false);

  VERIFY( std::adjacent_find(v.begin(), v.end()) == v.begin() + 3 );
  VERIFY( std::adjacent_find(v.begin(), v.end() - 1) == v.end() - 1 );
}  

int 
main()
{
  test01();
}

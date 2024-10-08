// { dg-do compile }
// { dg-require-effective-target c++17 }
// { dg-options "-O2 -Wuninitialized -Wsystem-headers" }

#include <memory>
#include <variant>

using Event = std::variant<std::variant<std::tuple<std::unique_ptr<int>>>, int, char>;

void do_something(void* storage)
{
  Event event {};
  auto& swappedValue = *reinterpret_cast<Event*>(storage);
  std::swap(event, swappedValue);
}

// { dg-bogus "may be used uninitialized" "" { xfail *-*-* } 0 }

// PR c++/108266
// { dg-do compile { target c++11 } }
// { dg-skip-if "requires hosted libstdc++ for vector" { ! hostedlib } }

#include <initializer_list>
#include <vector>

struct S { S (const char *); };
void bar (std::vector<S>);

template <int N>
void
foo ()
{
  bar ({"", ""});
}

void
baz ()
{
  foo<0> ();
}

// ensure an audit level assert with a failing predicate does not generate an
// error during runtime when the contract build level is default
// { dg-do run }
// { dg-options "-std=c++2a -fcontracts" }
// { dg-skip-if "requires hosted libstdc++ for stdc++exp" { ! hostedlib } }

int main()
{
  int x = 1;
  [[assert audit: x < 0]];
  return 0;
}

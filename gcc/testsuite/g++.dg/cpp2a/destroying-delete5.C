// PR c++/91859
// { dg-do run { target c++20 } }
// { dg-additional-options -O2 }
// { dg-skip-if "requires hosted libstdc++ for cstdlib malloc" { ! hostedlib } }

#include <cstdlib>
#include <new>

struct Expression {
  int i = 0;
  void *operator new(std::size_t);
  void operator delete(Expression *, std::destroying_delete_t);
};

void * Expression::operator new(std::size_t sz)
{
  return std::malloc(sz);
}

int i;

void Expression::operator delete(Expression *p, std::destroying_delete_t) // { dg-message "destroying delete" }
{
  Expression * e = p;
  ::i = e->i;
  p->~Expression();
  std::free(p);
}

int main()
{
  auto p = new Expression();	// { dg-warning "no corresponding dealloc" }
  p->i = 1;
  delete p;
  if (i != 1)
    __builtin_abort();
}

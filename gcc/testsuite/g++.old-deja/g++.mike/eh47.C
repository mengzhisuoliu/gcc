// { dg-do run { xfail sparc64-*-elf arm-*-pe } }
// { dg-options "-fexceptions" }
// { dg-skip-if "requires hosted libstdc++ for cstdlib exit" { ! hostedlib } }

#include <cstdlib>
#include <exception>

void myterm() {
  exit (0);
}

int
main() {
  try {
    throw "";
  } catch (...) {
  }
  try {
    std::set_terminate (myterm);
    throw;
  } catch (...) {
    return 1;
  }
  return 1;
}

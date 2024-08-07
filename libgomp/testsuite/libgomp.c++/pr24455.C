// { dg-do run }
// { dg-require-effective-target tls_runtime }
// { dg-additional-sources pr24455-1.C }
// { dg-options "-fno-extern-tls-init" }

extern "C" void abort (void);

extern int i;
#pragma omp threadprivate(i)

int main()
{
  i = 0;

#pragma omp parallel default(none) num_threads(10) copyin(i)
    {
      i++;
#pragma omp barrier
      if (i != 1)
	abort ();
    }

    return 0;
}

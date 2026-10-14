/* The build container has a newer glibc than FuriOS: pin symbols that got new versions
   (glibc 2.43 added remainder@GLIBC_2.43 and atan2f@GLIBC_2.43) to the old ones available on
   the phone. */
#include <math.h>

__asm__(".symver remainder_glibc217, remainder@GLIBC_2.17");
double remainder_glibc217(double x, double y);

double remainder(double x, double y)
{
  return remainder_glibc217(x, y);
}

__asm__(".symver atan2f_glibc217, atan2f@GLIBC_2.17");
float atan2f_glibc217(float y, float x);

float atan2f(float y, float x)
{
  return atan2f_glibc217(y, x);
}

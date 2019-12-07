/** @file
  Implements a wrapper around rand() which takes care of seeding.
*/

#include "random.h"

#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "error-handling.h"

/** Wrapper around rand() which seeds it on the first call. */
int sRand(void)
{
  static bool already_seeded = false;
  if(already_seeded == false)
  {
    srand((time(NULL) << 9) + getpid());
    already_seeded = true;
  }

  return rand();
}

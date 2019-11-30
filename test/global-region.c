/** @file
  Tests the global region.
*/

#include "global-region.h"

#include "test.h"
#include "random.h"
#include "safe-math.h"

static void printTestGroupEnd(void *data)
{
  (void)data;
  testGroupEnd();
}

int main(void)
{
  testGroupStart("global region");
  for(size_t counter = 0; counter < 30; counter++)
  {
    CR_Region *r = CR_GetGlobalRegion();
    assert_true(r != NULL);

    const size_t iterations = sRand() % 30;
    for(size_t iteration = 0; iteration < iterations; iteration++)
    {
      assert_true(CR_RegionAlloc(r, sRand() % 5000 + 1) != NULL);
    }
  }

  CR_RegionAttach(CR_GetGlobalRegion(), printTestGroupEnd, NULL);
}

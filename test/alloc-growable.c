/** @file
  Tests allocations of dynamic memory from regions.
*/

#include "alloc-growable.h"

#include <stdint.h>
#include <string.h>

#include "test.h"
#include "random.h"
#include "region.h"
#include "safe-math.h"
#include "error-handling.h"

/** Asserts certain properties of the given memory. */
static void checkPtr(void *ptr)
{
  assert_true(ptr != NULL);

  if((size_t)ptr % 8 != 0)
  {
    CR_ExitFailure("allocated pointer is not aligned properly: %p", (void *)ptr);
  }
}

/** A function type which checks the given memory. */
typedef void PtrTestFunction(char *ptr);

/** Allocates memory from a fresh region and passes it to the given
  function. It takes the size of the first allocation as parameter. */
static void testFromRegion(size_t initial_size, PtrTestFunction *function)
{
  CR_Region *r = CR_RegionNew();
  assert_true(r != NULL);

  char *ptr = CR_RegionAllocGrowable(r, initial_size);
  checkPtr(ptr);
  memset(ptr, sRand() % INT8_MAX, initial_size);

  function(ptr);

  CR_RegionRelease(r);
}

/** Asserts that the given memory with the specified size is filled with
  the given value. */
static void assertPtrContainsValue(char *ptr, size_t size, char value)
{
  for(size_t index = 0; index < size; index++)
  {
    assert_true(ptr[index] == value);
  }
}

static void testGrowth(char *ptr)
{
  size_t previous_size = 0;
  char previous_value = '\0';

  for(size_t size = sRand() % 20 + 1; size < 12000; size += sRand() % 750)
  {
    ptr = CR_EnsureCapacity(ptr, size);
    checkPtr(ptr);

    assertPtrContainsValue(ptr, previous_size, previous_value);

    memset(ptr, sRand() % INT8_MAX, size);
    previous_size = size;
    previous_value = ptr[0];
  }
}

static void testRandomly(char *ptr)
{
  size_t previous_size = 0;
  char previous_value = '\0';

  for(size_t counter = 0; counter < 1000; counter++)
  {
    const size_t size = sRand() % 3000 + 1;
    const char *previous_ptr = ptr;

    ptr = CR_EnsureCapacity(ptr, size);
    checkPtr(ptr);

    if(size <= previous_size)
    {
      /* Assert that the ptr doesn't get reallocated if not required. */
      assert_true(ptr == previous_ptr);
    }
    else
    {
      /* Assert that the reallocated ptr still contains the old data. */
      assertPtrContainsValue(ptr, previous_size, previous_value);
    }

    memset(ptr, sRand() % INT8_MAX, size);
    previous_size = size;
    previous_value = ptr[0];
  }
}

static void testFailure(char *ptr)
{
  assert_error((ptr = CR_EnsureCapacity(ptr, 0)),
               "unable to allocate 0 bytes");
}

static void testOverflow(char *ptr)
{
  assert_error((ptr = CR_EnsureCapacity(ptr, SIZE_MAX)),
               "overflow calculating object size");
}

static void invokeTestFunction(PtrTestFunction *function)
{
  testFromRegion(1, function);
  testFromRegion(7, function);
  testFromRegion(8, function);
  testFromRegion(13, function);
  testFromRegion(401, function);
  testFromRegion(1750, function);
  testFromRegion(4096, function);
  testFromRegion(500000, function);
  testFromRegion(10 * 1024 * 1024, function);
  function(NULL);
  function(NULL);
  function(NULL);
}

int main(void)
{
  testGroupStart("allocating and growing memory");
  {
    invokeTestFunction(testGrowth);
  }
  testGroupEnd();

  testGroupStart("allocating and randomly growing memory");
  {
    invokeTestFunction(testRandomly);
  }
  testGroupEnd();

  testGroupStart("allocation failures");
  {
    assert_error(CR_RegionAllocGrowable(NULL, 0), "unable to allocate 0 bytes");
    invokeTestFunction(testFailure);

    assert_error(CR_RegionAllocGrowable(NULL, SIZE_MAX), "overflow calculating object size");
    invokeTestFunction(testOverflow);
  }
  testGroupEnd();
}

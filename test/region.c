/** @file
  Tests region allocations.
*/

#include "region.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "test.h"
#include "random.h"
#include "safe-math.h"
#include "error-handling.h"
#include "memory-overlap.h"

#define chunks_capacity 10000
static AllocatedChunk chunks[chunks_capacity];
static size_t chunks_used = 0;

/** Wrapper around CR_RegionNew(), which checks for returned NULL
  pointers. */
static CR_Region *checkedRegion(void)
{
  CR_Region *r = CR_RegionNew();
  assert_true(r != NULL);
  return r;
}

/** Wrapper around CR_RegionAlloc(), which checks the returned memory. */
static void *checkedAlloc(CR_Region *r, size_t size)
{
  void *data = CR_RegionAlloc(r, size);
  assert_true(data != NULL);

  if((size_t)data % 8 != 0)
  {
    CR_ExitFailure("region failed to align memory: %p", data);
  }

  return data;
}

/** Wrapper around CR_RegionAlloc(), which checks the returned memory. */
static void *checkedAllocUnaligned(CR_Region *r, size_t size)
{
  void *data = CR_RegionAllocUnaligned(r, size);
  assert_true(data != NULL);
  return data;
}

/** Function that chooses randomly between checkedAlloc() and
  checkedAllocUnaligned(). */
static void *checkedAllocRandom(CR_Region *r, size_t size)
{
  if(sRand() % 2 == 0)
  {
    return checkedAlloc(r, size);
  }
  else
  {
    return checkedAllocUnaligned(r, size);
  }
}

/** Aborts the test program if the given expression evaluates to false.
  This is needed to prevent test callbacks from terminating the program
  on failure. */
#define assert_abort(expression) \
  if(!(expression)) { \
    printf("[FAILURE]\n    %s: line %i: assert failed: %s\n", \
            __FILE__, __LINE__, #expression); \
    abort(); }

/** Functions for testing the calling order of callbacks. */
static void checkValueIs5(void *data)
{
  int *number = data;
  assert_abort(*number == 5);
  *number = -1234;
}
static void checkValueIs27(void *data)
{
  int *number = data;
  assert_abort(*number == 27);
  *number = 5;
}
static void checkValueIsMinus3(void *data)
{
  int *number = data;
  assert_abort(*number == -3);
  *number = 27;
}
static void setToTrue(void *data)
{
  bool *value = data;
  *value = true;
}
static int atexit_test_number = 0;
static void lastCallback(void *data)
{
  int *number = data;
  assert_abort(*number == 79128);
  testGroupEnd();
}
static void checkValueIs9(void *data)
{
  int *number = data;
  assert_abort(*number == 9);
  *number = 79128;
}
static void checkValueIs278(void *data)
{
  int *number = data;
  assert_abort(*number == 278);
  *number = 9;
}
static void checkValueIsMinus9128(void *data)
{
  int *number = data;
  assert_abort(*number == -9128);
  *number = 278;
}
static void checkValueIs117(void *data)
{
  int *number = data;
  assert_abort(*number == 117);
  *number = -9128;
}
static void checkValueIs43(void *data)
{
  int *number = data;
  assert_abort(*number == 43);
  *number = 117;
}

/** The type of an alloc function. */
typedef void *AllocFunction(CR_Region *, size_t);

static void testCreateAndRelease(const char *group_name,
                                 AllocFunction *alloc_function)
{
  testGroupStart(group_name);
  {
    CR_Region *r = checkedRegion();

    chunks[0].size = 112;
    chunks[0].data = alloc_function(r, chunks[0].size);
    memset(chunks[0].data, 12, chunks[0].size);

    assert_error(CR_RegionAlloc(r, 0), "unable to allocate 0 bytes");
    assert_error(CR_RegionAllocUnaligned(r, 0), "unable to allocate 0 bytes");
#ifndef CREGION_ALWAYS_FRESH_MALLOC
    assert_error(CR_RegionAlloc(r, SIZE_MAX), "overflow calculating object size");
    assert_error(CR_RegionAllocUnaligned(r, SIZE_MAX), "overflow calculating object size");
#endif

    chunks[1].size = 1;
    chunks[1].data = alloc_function(r, chunks[1].size);
    *chunks[1].data = 'x';

    chunks_used = 2;
    assertNoOverlaps(chunks, chunks_used);

    CR_RegionRelease(r);
  }
  testGroupEnd();
}

static void testRandomAlloc(const char *group_name_one,
                            const char *group_name_random,
                            AllocFunction *alloc_function)
{
  testGroupStart(group_name_one);
  {
    for(size_t counter = 0; counter < 30; counter++)
    {
      CR_Region *r = checkedRegion();
      chunks_used = sRand() % 2500 + 20;
      assert_true(chunks_used < chunks_capacity);
      const char value = sRand() % INT8_MAX;

      for(size_t index = 0; index < chunks_used; index++)
      {
        chunks[index].size = sRand() % 1500 + 1;
        chunks[index].data = alloc_function(r, chunks[index].size);
        memset(chunks[index].data, value, chunks[index].size);
      }

      assertNoOverlaps(chunks, chunks_used);
      CR_RegionRelease(r);
    }
  }
  testGroupEnd();

  testGroupStart(group_name_random);
  {
    CR_Region *regions[15];
    static const size_t region_count = sizeof(regions)/(sizeof *regions);

    for(size_t index = 0; index < region_count; index++)
    {
      regions[index] = checkedRegion();
    }

    for(size_t counter = 0; counter < 15; counter++)
    {
      chunks_used = sRand() % 2500 + 20;
      assert_true(chunks_used < chunks_capacity);
      const char value = sRand() % INT8_MAX;

      for(size_t index = 0; index < chunks_used; index++)
      {
        CR_Region *r = regions[sRand() % region_count];

        if(sRand() % 50 == 0)
        {
          chunks[index].size = sRand() % 536000 + 1;
        }
        else
        {
          chunks[index].size = sRand() % 2300 + 1;
        }

        chunks[index].data = alloc_function(r, chunks[index].size);
        memset(chunks[index].data, value, chunks[index].size);
      }

      assertNoOverlaps(chunks, chunks_used);
    }

    for(size_t index = 0; index < region_count; index++)
    {
      CR_RegionRelease(regions[index]);
    }
  }
  testGroupEnd();
}

int main(void)
{
  testCreateAndRelease("creating and releasing a region (aligned)",
                       checkedAlloc);
  testCreateAndRelease("creating and releasing a region (unaligned)",
                       checkedAllocUnaligned);
  testCreateAndRelease("creating and releasing a region (randomly aligned)",
                       checkedAllocRandom);

  testGroupStart("callback calling");
  {
    CR_Region *r1 = checkedRegion();
    CR_Region *r2 = checkedRegion();
    CR_Region *r3 = checkedRegion();

    /* Allocate some data and attach it to a region. Valgrind will detect
       if it leaks. */
    char *data = malloc(182);
    assert_true(data != NULL);
    CR_RegionAttach(r1, free, data);

    bool value = false;
    CR_RegionAttach(r2, setToTrue, &value);

    int number = 75;
    CR_RegionAttach(r3, checkValueIs5, &number);
    CR_RegionAttach(r3, checkValueIs27, &number);
    CR_RegionAttach(r3, checkValueIsMinus3, &number);

    /* Test releasing of regions. */
    assert_true(value == false);
    CR_RegionRelease(r2);
    assert_true(value == true);

    number = -3;
    CR_RegionRelease(r3);
    assert_true(number == -1234);

    CR_RegionRelease(r1);
  }
  testGroupEnd();

#ifndef CREGION_ALWAYS_FRESH_MALLOC
  testGroupStart("padding of memory 1");
  {
    CR_Region *r = CR_RegionNew();

    void *data[] =
    {
      checkedAlloc(r, 1),
      checkedAlloc(r, 9),
      checkedAlloc(r, 12),
      checkedAlloc(r, 16),
      checkedAlloc(r, 17),
      checkedAlloc(r, 22),
      checkedAlloc(r, 34),
      checkedAlloc(r, 56),
      checkedAlloc(r, 1),
      checkedAlloc(r, 39),
      checkedAlloc(r, 41),
      checkedAlloc(r, 1),
      checkedAlloc(r, 40),
      checkedAlloc(r, 32),
      checkedAlloc(r, 1),
    };

    assert_true((size_t)data[1]  - (size_t)data[0]  == 8);
    assert_true((size_t)data[2]  - (size_t)data[1]  == 16);
    assert_true((size_t)data[3]  - (size_t)data[2]  == 16);
    assert_true((size_t)data[4]  - (size_t)data[3]  == 16);
    assert_true((size_t)data[5]  - (size_t)data[4]  == 24);
    assert_true((size_t)data[6]  - (size_t)data[5]  == 24);
    assert_true((size_t)data[7]  - (size_t)data[6]  == 40);
    assert_true((size_t)data[8]  - (size_t)data[7]  == 56);
    assert_true((size_t)data[9]  - (size_t)data[8]  == 8);
    assert_true((size_t)data[10] - (size_t)data[9]  == 40);
    assert_true((size_t)data[11] - (size_t)data[10] == 48);
    assert_true((size_t)data[12] - (size_t)data[11] == 8);
    assert_true((size_t)data[13] - (size_t)data[12] == 40);
    assert_true((size_t)data[14] - (size_t)data[13] == 32);

    CR_RegionRelease(r);
  }
  testGroupEnd();

  testGroupStart("padding of memory 2");
  {
    chunks_used = 40;
    CR_Region *r = CR_RegionNew();

    for(size_t index = 0; index < chunks_used; index++)
    {
      chunks[index].size = (index % 8) + 1;
      chunks[index].data = checkedAlloc(r, chunks[index].size);
    }
    assertNoOverlaps(chunks, chunks_used);

    for(size_t index = 1; index < chunks_used; index++)
    {
      if((size_t)chunks[index].data - (size_t)chunks[index - 1].data != 8)
      {
        CR_ExitFailure("memory was padded incorrectly: %p, %p",
            (void *)chunks[index - 1].data,
            (void *)chunks[index].data);
      }
    }

    CR_RegionRelease(r);
  }
  testGroupEnd();
#endif

  testRandomAlloc("random aligned allocations from one region",
                  "random aligned allocations from random regions",
                  checkedAlloc);
  testRandomAlloc("random unaligned allocations from one region",
                  "random unaligned allocations from random regions",
                  checkedAllocUnaligned);
  testRandomAlloc("randomly aligned allocations from one region",
                  "randomly aligned allocations from random regions",
                  checkedAllocRandom);

  testGroupStart("callback calling at exit");
  {
    CR_Region *r1 = checkedRegion();
    CR_Region *r2 = checkedRegion();
    CR_Region *r3 = checkedRegion();

    atexit_test_number = 12;
    CR_RegionAttach(r1, lastCallback,          &atexit_test_number);
    CR_RegionAttach(r3, checkValueIs117,       &atexit_test_number);
    CR_RegionAttach(r2, checkValueIs278,       &atexit_test_number);
    CR_RegionAttach(r3, checkValueIs43,        &atexit_test_number);
    CR_RegionAttach(r1, checkValueIs9,         &atexit_test_number);
    CR_RegionAttach(r2, checkValueIsMinus9128, &atexit_test_number);
    atexit_test_number = 43;
  }
}

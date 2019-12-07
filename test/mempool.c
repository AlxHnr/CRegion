/** @file
  Tests the object pool implementation.
*/

#include "mempool.h"

#include <stdint.h>

#include "error-handling.h"
#include "memory-overlap.h"
#include "random.h"
#include "safe-math.h"
#include "test.h"

#define chunks_capacity 5000
static AllocatedChunk chunks[chunks_capacity];
static size_t chunks_used = 0;

/** Wrapper around CR_MempoolAlloc(), which checks the returned memory. */
static void *checkedMPAlloc(CR_Mempool *mp)
{
  void *data = CR_MempoolAlloc(mp);
  assert_true(data != NULL);

  if((size_t)data % 8 != 0)
  {
    CR_ExitFailure("mempool returned unaligned memory: %p", data);
  }

  return data;
}

/** Explicit destructor which sets the given int ptr to 173. This is for
  testing that only the explicit destructor gets called. */
static int setTo173Explicit(void *data)
{
  int **ptr_to_int = data;
  **ptr_to_int = 173;

  return 0;
}

/** Implicit destructor which sets the given int ptr to 91. This is for
  testing that only the implicit destructor gets called. */
static void setToMinus91Implicit(void *data)
{
  int **ptr_to_int = data;
  **ptr_to_int = -91;
}

/** May allocate some integer ptr from the given pool. This is useful for
  populating the mempools internal linked lists.

  @param int_pool The pool to use for allocations.
  @param dummy_int The dummy int to initialize the allocated pointer to.
*/
static void allcateIntegersFromPool(CR_Mempool *int_pool, int *dummy_int)
{
  const size_t allocations = sRand() % 5;
  for(size_t counter = 0; counter < allocations; counter++)
  {
    int **int_ptr = checkedMPAlloc(int_pool);
    *int_ptr = dummy_int;

    if(sRand() % 2 == 0)
    {
      CR_EnableObjectDestructor(int_ptr);
    }
  }
}

/** The type of CR_DestroyObject(). Useful for injecting additional checks. */
typedef void DestroyFunction(void *data);

/** Configurable function for testing destructors.

  @param explicit_destructor The explicit destructor to create the mempool
  with. Can be NULL.
  @param implicit_destructor The implicit destructor to create the mempool
  with. Can be NULL.
  @param enable_destructor True if the destructor should be enabled via
  CR_EnableObjectDestructor().
  @param destroy_function The function which will be used to destroy the
  object. If it points to NULL, it will be ignored.
  @param after_destroy The value which the test integer should have after
  being passed to destroy_function().
  @param after_release The value wich the test integer should have after
  releasing the region.
*/
static void testDestructor(CR_FailableDestructor *explicit_destructor,
                           CR_ReleaseCallback *implicit_destructor,
                           bool enable_destructor,
                           DestroyFunction *destroy_function,
                           int after_destroy, int after_release)
{
  CR_Region *r = CR_RegionNew();

  CR_Mempool *int_pool =
    CR_MempoolNew(r, sizeof(int *), explicit_destructor, implicit_destructor);
  assert_true(int_pool != NULL);

  /* Allocate some integers from the pool, so the test integer is not
     always the first or only one. */
  int dummy_int = 0;
  allcateIntegersFromPool(int_pool, &dummy_int);

  int **int_ptr = checkedMPAlloc(int_pool);

  /* Allocate some integers from the pool, so the test integer is not
     always the last or only one. */
  allcateIntegersFromPool(int_pool, &dummy_int);

  /* Test destructor calling. */
  int value = 12;
  *int_ptr = &value;

  if(enable_destructor)
  {
    CR_EnableObjectDestructor(int_ptr);
  }

  assert_true(value == 12);

  if(destroy_function != NULL)
  {
    destroy_function(int_ptr);
    assert_true(value == after_destroy);
  }

  CR_RegionRelease(r);
  assert_true(value == after_release);
}

/** Functions for testing destructor failure. */
static int failingDestructor(void *data)
{
  (void)data;
  CR_ExitFailure("this is a test error");
}
static void destroyAndCatchError(void *data)
{
  assert_error(CR_DestroyObject(data), "this is a test error");
}

/** Failable destructor which asserts that an object can't allocate itself
  from its own mempool during destruction.

  @param data Pointer to a pointer to the destroyed objects owning mempool.

  @return Always zero to have a type signature different from fail-safe
  destructors.
*/
static int allocateSelfFromMempool(void *data)
{
  void** ptr_to_own_mp = data;
  CR_Mempool *mp = *ptr_to_own_mp;

  void **allocated_ptr = checkedMPAlloc(mp);
  assert_true(allocated_ptr != data);

  /* Object must be releasable if destructor is not enabled. */
  CR_DestroyObject(allocated_ptr);

  return 0;
}

int main(void)
{
  testGroupStart("creating memory pools");
  {
    CR_Region *r = CR_RegionNew();

    for(size_t size = 1; size < 107; size++)
    {
      assert_true(CR_MempoolNew(r, size, NULL, NULL) != NULL);
      assert_error(CR_MempoolNew(r, 0, NULL, NULL),
                   "unable to create memory pool for allocating zero size objects");
      assert_error(CR_MempoolNew(r, SIZE_MAX - sRand() % 5, NULL, NULL),
                   "overflow calculating object size");
    }

    CR_RegionRelease(r);
  }
  testGroupEnd();

  testGroupStart("allocating from memory pools");
  for(size_t iterations = 0; iterations < 30; iterations++)
  {
    CR_Region *r = CR_RegionNew();

    const size_t object_size = sRand() % 320 + 1;
    CR_Mempool *mp = CR_MempoolNew(r, object_size, NULL, NULL);
    assert_true(mp != NULL);

    chunks_used = sRand() % 2000 + 2;
    assert_true(chunks_used < chunks_capacity);
    for(size_t index = 0; index < chunks_used; index++)
    {
      chunks[index].data = checkedMPAlloc(mp);
      chunks[index].size = object_size;
      memset(chunks[index].data, sRand() % INT8_MAX, chunks[index].size);
    }

    assertNoOverlaps(chunks, chunks_used);
    CR_RegionRelease(r);
  }
  testGroupEnd();

  testGroupStart("destructor calling");
  for(size_t iterations = 0; iterations < 5000; iterations++)
  {
    /* Assert that:
       - The explicit destructor is only called by CR_DestroyObject() if
       enabled and not NULL.
       - The implicit destructor is only called if enabled, not NULL and
       the object was never passed to CR_DestroyObject() before. This
       destructor should only be invoked if the region gets released. */
    DestroyFunction *destroy = CR_DestroyObject;
    testDestructor(setTo173Explicit, setToMinus91Implicit, true,  destroy, 173, 173);
    testDestructor(NULL,             setToMinus91Implicit, true,  destroy, 12,  12);
    testDestructor(setTo173Explicit, NULL,                 true,  destroy, 173, 173);
    testDestructor(NULL,             NULL,                 true,  destroy, 12,  12);
    testDestructor(setTo173Explicit, setToMinus91Implicit, false, destroy, 12,  12);
    testDestructor(NULL,             setToMinus91Implicit, false, destroy, 12,  12);
    testDestructor(setTo173Explicit, NULL,                 false, destroy, 12,  12);
    testDestructor(NULL,             NULL,                 false, destroy, 12,  12);
    testDestructor(setTo173Explicit, setToMinus91Implicit, true,  NULL,    12, -91);
    testDestructor(NULL,             setToMinus91Implicit, true,  NULL,    12, -91);
    testDestructor(setTo173Explicit, NULL,                 true,  NULL,    12,  12);
    testDestructor(NULL,             NULL,                 true,  NULL,    12,  12);
    testDestructor(setTo173Explicit, setToMinus91Implicit, false, NULL,    12,  12);
    testDestructor(NULL,             setToMinus91Implicit, false, NULL,    12,  12);
    testDestructor(setTo173Explicit, NULL,                 false, NULL,    12,  12);
    testDestructor(NULL,             NULL,                 false, NULL,    12,  12);

    testDestructor(failingDestructor, setToMinus91Implicit, true,  destroyAndCatchError, 12,  12);
    testDestructor(failingDestructor, NULL,                 true,  destroyAndCatchError, 12,  12);
    testDestructor(failingDestructor, setToMinus91Implicit, false, destroy,              12,  12);
    testDestructor(failingDestructor, NULL,                 false, destroy,              12,  12);
    testDestructor(failingDestructor, setToMinus91Implicit, true,  NULL,                 12, -91);
    testDestructor(failingDestructor, NULL,                 true,  NULL,                 12,  12);
    testDestructor(failingDestructor, setToMinus91Implicit, false, NULL,                 12,  12);
    testDestructor(failingDestructor, NULL,                 false, NULL,                 12,  12);
  }
  testGroupEnd();

#ifndef CREGION_ALWAYS_FRESH_MALLOC
  testGroupStart("passing objects to CR_DestroyObject() twice");
  {
    CR_Region *r = CR_RegionNew();

    CR_Mempool *mp = CR_MempoolNew(r, 128, NULL, NULL);
    assert_true(mp != NULL);

    void *ptr1 = checkedMPAlloc(mp);
    CR_DestroyObject(ptr1);
    assert_error(CR_DestroyObject(ptr1), "passed the same object to CR_DestroyObject() twice");
    assert_error(CR_DestroyObject(ptr1), "passed the same object to CR_DestroyObject() twice");

    ptr1 = checkedMPAlloc(mp);
    void *ptr2 = checkedMPAlloc(mp);
    void *ptr3 = checkedMPAlloc(mp);
    CR_DestroyObject(ptr3);
    CR_DestroyObject(ptr1);
    assert_error(CR_DestroyObject(ptr1), "passed the same object to CR_DestroyObject() twice");
    assert_error(CR_DestroyObject(ptr3), "passed the same object to CR_DestroyObject() twice");
    assert_error(CR_DestroyObject(ptr3), "passed the same object to CR_DestroyObject() twice");

    ptr1 = checkedMPAlloc(mp);
    CR_DestroyObject(ptr2);
    assert_error(CR_DestroyObject(ptr2), "passed the same object to CR_DestroyObject() twice");
    assert_error(CR_DestroyObject(ptr3), "passed the same object to CR_DestroyObject() twice");
    ptr3 = checkedMPAlloc(mp);

    CR_DestroyObject(ptr3);
    CR_DestroyObject(ptr1);

    CR_RegionRelease(r);
  }
  testGroupEnd();

  testGroupStart("reusing released memory");
  {
    CR_Region *r = CR_RegionNew();

    const size_t object_size = sizeof(double);
    CR_Mempool *mp = CR_MempoolNew(r, object_size, NULL, NULL);
    assert_true(mp != NULL);

    chunks[0].data = checkedMPAlloc(mp);
    chunks[0].size = object_size;
    chunks[1].data = checkedMPAlloc(mp);
    chunks[1].size = object_size;
    chunks[2].data = checkedMPAlloc(mp);
    chunks[2].size = object_size;
    assertNoOverlaps(chunks, 3);

    CR_DestroyObject(chunks[2].data);
    CR_DestroyObject(chunks[0].data);
    CR_DestroyObject(chunks[1].data);
    assert_true(checkedMPAlloc(mp) == chunks[1].data);
    assert_true(checkedMPAlloc(mp) == chunks[0].data);
    assert_true(checkedMPAlloc(mp) == chunks[2].data);

    chunks[3].data = checkedMPAlloc(mp);
    chunks[3].size = object_size;
    assertNoOverlaps(chunks, 4);

    chunks[4].data = checkedMPAlloc(mp);
    chunks[4].size = object_size;
    assertNoOverlaps(chunks, 5);

    CR_DestroyObject(chunks[4].data);
    assert_true(checkedMPAlloc(mp) == chunks[4].data);
    CR_DestroyObject(chunks[4].data);
    assert_true(checkedMPAlloc(mp) == chunks[4].data);

    CR_DestroyObject(chunks[3].data);
    CR_DestroyObject(chunks[0].data);
    CR_DestroyObject(chunks[4].data);
    assert_true(checkedMPAlloc(mp) == chunks[4].data);
    assert_true(checkedMPAlloc(mp) == chunks[0].data);
    assert_true(checkedMPAlloc(mp) == chunks[3].data);

    chunks[5].data = checkedMPAlloc(mp);
    chunks[5].size = object_size;
    assertNoOverlaps(chunks, 6);

    chunks[6].data = checkedMPAlloc(mp);
    chunks[6].size = object_size;
    assertNoOverlaps(chunks, 7);

    CR_RegionRelease(r);
  }
  testGroupEnd();
#endif

  testGroupStart("destructor runs before object gets returned to pool");
  {
    CR_Region *r = CR_RegionNew();

    CR_Mempool *mp =
      CR_MempoolNew(r, sizeof(void*), allocateSelfFromMempool, NULL);
    assert_true(mp != NULL);

    void **ptr_to_own_mp = checkedMPAlloc(mp);
    *ptr_to_own_mp = mp;
    CR_EnableObjectDestructor(ptr_to_own_mp);
    CR_DestroyObject(ptr_to_own_mp);

    CR_RegionRelease(r);
  }
  testGroupEnd();
}

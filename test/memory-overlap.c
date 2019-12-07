/** @file
  Implements checks for memory overlaps.
*/

#include "memory-overlap.h"

#include "error-handling.h"
#include "test.h"

/** Asserts that the given memory chunks don't overlap.

  @param chunks The chunk array to check.
  @param size The amount of elements in the array.
*/
void assertNoOverlaps(AllocatedChunk *chunks, size_t size)
{
  assert_true(size > 1);

  for(size_t outer = 0; outer < size - 1; outer++)
  {
    for(size_t inner = outer + 1; inner < size; inner++)
    {
      AllocatedChunk *a = &chunks[outer];
      AllocatedChunk *b = &chunks[inner];

      if(&a->data[a->size] > b->data &&
         a->data < &b->data[b->size])
      {
        CR_ExitFailure("allocated chunks overlap");
      }
    }
  }
}

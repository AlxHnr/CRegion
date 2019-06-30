/** @file
  Provides checks for memory overlaps.
*/

#ifndef CREGION_TEST_MEMORY_OVERLAP_H
#define CREGION_TEST_MEMORY_OVERLAP_H

#include <stddef.h>

/** Represents an allocated memory chunk. */
typedef struct
{
  char *data;
  size_t size;
}AllocatedChunk;

extern void assertNoOverlaps(AllocatedChunk *chunks, size_t size);

#endif

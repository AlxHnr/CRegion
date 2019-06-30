/** @file
  Tests safe math functions.
*/

#include "safe-math.h"

#include "test.h"

int main(void)
{
  testGroupStart("CR_SafeAdd()");
  const char *expected_error = "overflow calculating object size";
  assert_true(CR_SafeAdd(0, 0) == 0);
  assert_true(CR_SafeAdd(2, 3) == 5);
  assert_true(CR_SafeAdd(50, 75) == 125);
  assert_true(CR_SafeAdd(65, SIZE_MAX - 65) == SIZE_MAX);
  assert_error(CR_SafeAdd(SIZE_MAX, SIZE_MAX), expected_error);
  assert_error(CR_SafeAdd(512, SIZE_MAX - 90), expected_error);
  assert_error(CR_SafeAdd(SIZE_MAX, 1), expected_error);
  testGroupEnd();

  testGroupStart("CR_SafeMultiply()");
  assert_true(CR_SafeMultiply(0, 5) == 0);
  assert_true(CR_SafeMultiply(5, 3) == 15);
  assert_true(CR_SafeMultiply(3, 5) == 15);
  assert_true(CR_SafeMultiply(70, 80) == 5600);
  assert_true(CR_SafeMultiply(0,        0) == 0);
  assert_true(CR_SafeMultiply(3,        0) == 0);
  assert_true(CR_SafeMultiply(2348,     0) == 0);
  assert_true(CR_SafeMultiply(SIZE_MAX, 0) == 0);
  assert_true(CR_SafeMultiply(SIZE_MAX, 1));
  assert_error(CR_SafeMultiply(SIZE_MAX, 25), expected_error);
  assert_error(CR_SafeMultiply(SIZE_MAX - 80, 295), expected_error);
  testGroupEnd();
}

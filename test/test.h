/** @file
  Declares functions for testing code. This header should only be included
  by tests.
*/

#ifndef CREGION_TEST_TEST_H
#define CREGION_TEST_TEST_H

#include <errno.h>
#include <setjmp.h>
#include <stdbool.h>
#include <string.h>

/* This test suite uses longjump to catch and handle calls to
   CR_ExitFailure(). Unfortunately GCC tries to inline as much as possible
   into assert statements, which causes self-imposed warnings that don't
   build with -Werror. */
#if defined __GNUC__ && !defined __clang__
#pragma GCC diagnostic ignored "-Wclobbered"
#endif

/** Asserts that the given expression evaluates to true. It will catch
  calls to CR_ExitFailure(). */
#define assert_true(expression) \
  test_catch_die = true; \
  if(setjmp(test_jump_buffer) == 0) { \
    if(!(expression)) { \
      dieTest("%s: line %i: assert failed: %s", \
              __FILE__, __LINE__, #expression); \
    } \
  } else { \
    dieTest("%s: line %i: unexpected error: %s", __FILE__, \
            __LINE__, test_error_message); \
  } test_catch_die = false;

/** Asserts that the given expression causes a call to CR_ExitFailure()
  with the specified error message. */
#define assert_error(expression, message) \
  test_catch_die = true; \
  if(setjmp(test_jump_buffer) == 0) { \
    (void)(expression); \
    dieTest("%s: line %i: expected error: %s", \
            __FILE__, __LINE__, #expression); \
  } else if(strcmp(message, test_error_message) != 0) { \
    dieTest("%s: line %i: got wrong error message: \"%s\"\n" \
            "\t\texpected: \"%s\"", __FILE__, __LINE__, \
            test_error_message, message); \
  } errno = 0; test_catch_die = false;

extern jmp_buf test_jump_buffer;
extern char *test_error_message;
extern bool test_catch_die;

extern void dieTest(const char *format, ...)
#ifdef __GNUC__
__attribute__((noreturn, format(printf, 1, 2)))
#endif
  ;

extern void testGroupStart(const char *name);
extern void testGroupEnd(void);

#endif

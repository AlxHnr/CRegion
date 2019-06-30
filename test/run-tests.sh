#!/bin/sh -e

# Names of tests specified in the order to run.
tests="safe-math region global-region"

for test in $tests; do
  test -t 1 &&
    printf "Running \033[0;33m%s\033[0m:\n" "$test" ||
    printf "Running %s\n" "$test:"

  "./build/test/$test"
  printf "\n"
done

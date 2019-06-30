[![travis](https://travis-ci.org/AlxHnr/CRegion.svg?branch=master)](https://travis-ci.org/AlxHnr/CRegion)
[![codecov](https://codecov.io/github/AlxHnr/CRegion/coverage.svg?branch=master)](https://codecov.io/github/AlxHnr/CRegion?branch=master)
[![license](https://licensebuttons.net/p/zero/1.0/88x31.png)](LICENSE)

A free, public-domain
[region](https://en.wikipedia.org/wiki/Region-based_memory_management)
implementation without any dependencies. It conforms strictly to C99 and is
valgrind-clean. Allocation failures and arithmetic overflows are handled by
calling [exit()](src/error-handling.c).

CFLAGS           += -std=c99 -D_XOPEN_SOURCE=600 -D_FILE_OFFSET_BITS=64
OBJECTS          := $(patsubst src/%.c,build/%.o,$(wildcard src/*.c))
TEST_PROGRAMS    := $(shell grep -l '^int main' test/*.c)
TEST_LIB_OBJECTS := $(filter-out $(TEST_PROGRAMS),$(wildcard test/*.c))
TEST_PROGRAMS    := $(patsubst %.c,build/%,$(TEST_PROGRAMS))
TEST_LIB_OBJECTS := $(patsubst %.c,build/%.o,$(TEST_LIB_OBJECTS)) \
  $(filter-out build/error-handling.o,$(OBJECTS))

.PHONY: all test clean
all: $(OBJECTS)

-include build/dependencies.makefile
build/dependencies.makefile:
	mkdir -p build/test/
	$(CC) -MM src/*.c | sed -r 's,^(\S+:),build/\1,g' > $@
	$(CC) -MM -Isrc/ test/*.c | sed -r 's,^(\S+:),build/test/\1,g' >> $@

build/%.o:
	$(CC) $(CFLAGS) -Isrc/ -c $< -o $@

build/test/%: build/test/%.o $(TEST_LIB_OBJECTS)
	$(CC) $^ $(LDFLAGS) -o $@

test: $(TEST_PROGRAMS)
	./test/run-tests.sh

clean:
	rm -rf build/

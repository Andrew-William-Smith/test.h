# `test.h` C Unit Testing Framework

`test.h` is a single-file, header-only unit testing framework for C, with the unique feature of being implemented entirely in the preprocessor.  Despite this constraint, `test.h` is a fairly full-featured testing framework, featuring test fixtures with custom lifecycle functions, parameterised tests, and a smattering of assertions to help you write well-structured tests with minimal mental overhead.  Furthermore, it provides address space isolation for all tests, running each in its own process; in addition to preventing memory corruption, this allows `test.h` to catch signals like segmentation faults in order to provide you with more robust test functionality.

Due to its use of `_Generic`s, `test.h` requires a C11-compatible compiler in order to work properly.  Furthermore, since it makes heavy use of attributes in order to construct test functions and relies upon the `fork()` system call for address isolation, it is currently only compatible with GCC and Clang on POSIX-compliant systems.

## Setup and usage
In order to use `test.h`, clone this repository or configure it as a submodule in the directory containing your test files.  Once you've done that, simply `#include "test.h"` at the top of each of your test files.  That's it: no special compile flags, no linker shenanigans, and no package munging.  Just the one line and you're ready to test.

An example test suite demonstrating the most basic features of `test.h` is given below:
```c
#include "test.h"

int main() {}

FIXTURE(Simple_fixture) {
    char *str;
};

TEST(Sanity_test, Simple_fixture) {
    ASSERT_TRUE(true);
    T_ str = "Hello there!";
    ASSERT_NON_NULL(T_ str);
    ASSERT_EQ(T_ str[0], 'H');
}
```

`test.h` has many useful features other than those demonstrated above; for a complete overview, it is highly recommended that you **read [`example_test.c`](https://github.com/Andrew-William-Smith/test.h/blob/master/example_test.c)**, a didactic test suite that highlights all of the major features of the framework, such as parameterised testing and fixture lifecycle functions.

Once you've written your test suite, you can compile the appropriate C file using `gcc` or `clang` normally.  If everything goes well, you should be able to run your test suite binary and see a report that looks something like the following:

![](images/example_run.png?raw=true)

## Frequently asked questions

### Why?  Just...why?
Why not?  In all seriousness, `test.h` was borne out of a desire for a truly easy-to-use C unit testing framework that required nothing other than a single file to integrate into a project.  Oftentimes, one of the largest barriers to projects having proper testing is the difficulty of setting up a testing framework in the first place: `test.h` attempts to solve these issues by requiring nothing but a single `#include` directive to start writing some tests.

Another aspect that fueled my decision to write `test.h` is a bit more whimsical: after encountering some deeply disturbing incantations of the darkest macrology in other C projects and learning about [`boost::Preprocessor`](https://www.boost.org/doc/libs/release/libs/preprocessor/doc/index.html), I wanted to see how far I could push the C preprocessor.  In addition, I wanted to add unit tests to a C project I was working on without pulling in any external dependencies; thus `test.h` was born.

### How does this work?
I give a reasonably detailed explanation of the machinery behind `test.h` in the [`test.h` file itself](https://github.com/Andrew-William-Smith/test.h/blob/master/test.h), but in short, it abuses the `.ctors` ELF section &mdash; exposed as `__attribute__((constructor))` in GCC/Clang &mdash; to run test functions and fixture lifecycle functions before `main()`.  That is why, despite the `main()` function being empty in the example test given above, the test suite will still run.  This harms the portability of the library, but given the widespread availability of GCC on Unix platforms, I think it's a fairly reasonable tradeoff of portability for enhanced functionality and ergonomics.  Plus, the use of `fork()` poses a substantially larger portability hazard on non-POSIX systems anyway.

The automatic formatting of assertion values is accomplished via the C11 `_Generic` selector mechanism combined with compile-time function generation.  Formatting functions are defined for all common C types, and unrecognised types are represented by their addresses in test failure reports.

### What are the compile times like?
Honestly, compile times for test suites using `test.h` are not as bad as one would think for the amount of utter preprocessor abuse it commits.  If you stick entirely to simple tests and fixtures, compile times are mostly negligible; parameterised tests cause compile times to increase somewhat, but not so much that it is unbearable.  `example_test.c`, which contains a parameterised test, compiles in under 1 second on my testing machine, an early 2015 MacBook Pro with a 3.1 GHz Core i7 processor.

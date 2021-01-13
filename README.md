# `test.h` C Unit Testing Framework

`test.h` is a single-file, header-only unit testing framework for C, with the unique feature of being implemented almost entirely in the preprocessor.
Despite this constraint, `test.h` is a fairly full-featured testing framework, featuring test fixtures with custom lifecycle functions, conditional test execution, parameterised tests, and a smattering of assertions to help you write well-structure tests with minimal mental overhead.
`test.h` is compatible with the most commonly used compilers on all platforms: it has been tested with GCC and Clang on Linux and macOS, and with MSVC from versions 6.0 through the current release as of January 2021.

## Setup and usage
In order to use `test.h`, clone this repository or configure it as a submodule in the directory containing your test files.
Once you've done that, simply `#include "test.h"` at the top of each of your test files.
That's it: no special compile flags, no linker shenanigans, and no package munging.
Just the one line and you're ready to test.

An example test suite demonstrating the most basic features of `test.h` is given below:

```c
#include "test.h"

// A main function...who needs that?
int main() {}

#pragma FIXTURE_START

FIXTURE(Simple_fixture) {
    char *str;
};

#pragma FIXTURE_END

#pragma TEST_START

TEST(Sanity_test, Simple_fixture) {
    SKIP_IF(0, "This test should not be skipped.");
    ASSERT_TRUE(true);
    T_ str = "Hello there!";
    ASSERT_NON_NULL(T_ str);
    ASSERT_EQ(T_ str[0], 'H', "%c");
}

#pragma TEST_END
```

`test.h` has many useful features other than those demonstrated above; for a complete overview, it is highly recommended that you **read [`example.org`](https://github.com/Andrew-William-Smith/test.h/blob/master/example.org)**, a didactic test suite that highlights all of the major features of the framework, such as parameterised testing and fixture lifecycle functions.

Once you've written your test suite, you can compile the appropriate C file using your compiler of choice.
If everything goes well, you should be able to run your test suite binary and see a report that looks something like the following:

![](https://github.com/Andrew-William-Smith/test.h/raw/master/images/example_run.png?raw=true)

Ooh, pretty colours!
Now, what are you waiting for?
**Go forth and test your code!**

## Frequently asked questions
### Why?  Just...why?
Why not?
In all seriousness, `test.h` was borne out of a desire for a truly easy-to-use C unit testing framework that required nothing more than a single file to integrate into a project.
Oftentimes, one of the larger barriers to projects having proper testing is the difficulty of setting up a testing framework in the first place: `test.h` attempts to solve these issues by requiring nothing but a single `#include` directive to start writing some tests.

Another aspect that fueled my decision to write `test.h` is a bit more whimsical: after encountering some deeply disturbing incantations of the darkest macrology in other C projects and learning about [`boost::Preprocessor`](https://www.boost.org/doc/libs/release/libs/preprocessor/doc/index.html), I wanted to see how far I could push the C preprocessor.
In addition, I wanted to add unit tests to a C project I was working on without pulling in any external dependencies; thus `test.h` was born.

### How does this work?
I give a reasonably detailed explanation of the machinery behind `test.h` in the [`test.h` file itself](https://github.com/Andrew-William-Smith/test.h/blob/master/test.h), but in short, it abuses the `.ctors` ELF section [exposed as `__attribute__((constructor))` in GCC/Clang] and the CRT initialisation sections of the PE file format [`__declspec(allocate(".CRT$XXU"))` in MSVC] to run test and fixture lifecycle functions before `main()`.
That is why, despite the `main()` function being empty in the example test given above, the test suite will still run.
The `#pragma` directives surrounding fixture and test sections are necessary for a similar reason: older versions of MSVC do not support `__declspec(allocate)` unless the directive follows a `#pragma data_seg` declaration.
It's a bit convoluted, but it seems to work on every platform on which I've tested it!

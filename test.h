/**
 * \file test.h
 * \author Andrew Smith
 * \brief A testing framework implemented entirely using the C preprocessor.
 *
 * This file implements a fairly full-featured unit testing framework whose
 * logic is implemented entirely in C preprocessor macros, and that has no
 * external dependencies.  This framework, if it can really be called that, was
 * written in response to the perceived excessive complexity of other unit
 * testing frameworks, which required external dependency management systems and
 * special compile flags just to get them working.  With <code>test.h</code>,
 * that is not so: simply <code>#include "test.h"</code> at the top of a C file
 * containing your test suite and you're off to the races.
 *
 * <code>test.h</code> makes use of a few fairly advanced compiler features, and
 * as such is only currently compatible with GCC and Clang in C11 mode.  At the
 * expense of portability, it gains a syntax similar to that of the popular
 * Google Test C++ testing framework with no runtime overhead and with an
 * extremely small footprint.  (As an aside, it appears to be possible to make
 * this framework work with MSVC, but I have neither the motivation nor the
 * Visual Studio installation to test it out.)
 *
 * Internally, <code>test.h</code> implements tests by way of ELF constructors,
 * functions referenced in the <code>.ctors</code> of an ELF binary that are
 * run before control is transferred to <code>_start</code>
 * [<code>main()</code>].  Usually, these functions are used to perform shared
 * library initialisation tasks; however, here we take advantage of this feature
 * of ELF to run tests before <code>main()</code>, in essence giving us the
 * ability to run functions that are not explicitly called.  This feature is
 * what allows <code>test.h</code> to run tests declared outside of any other
 * routine, providing convenient syntax and full control isolation to each test.
 *
 * In addition, each test is run in its own (forked) process to ensure memory
 * isolation and provide the ability to catch any signals that the test throws.
 * If one of your tests happens to segfault, that segfault will be caught and
 * reported similarly to a normal assertion failure, ensuring that every test is
 * run and accurately represented in the test report.
 *
 * Finally, <code>test.h</code> provides a few options to control the display of
 * test output by way of symbol defines.  The supported output controls are as
 * follows:
 *
 * <ul>
 *     <li><code>TEST_MONOCHROME_OUTPUT</code>: Do not use colours in test
 *     output.  Useful for running tests in terminals that do not support VT100
 *     control codes or outputting test results to a file.</li>
 *
 *     <li><code>TEST_OMIT_RUNTIME</code>: By default, both the user and wall
 *     runtimes of all tests are output along with their results.  Define this
 *     symbol to suppress runtime output.</li>
 *
 *     <li><code>TEST_OMIT_SUCCESSES</code>: The pessimist's favourite output
 *     control option.  If this symbol is defined, only test failures will be
 *     printed in the report.  Useful for working with test suites with many
 *     failed tests, albeit perhaps a bit depressing.</li>
 * </ul>
 *
 * Now that you know some rough details about how <code>test.h</code> works,
 * read through the documentation of the public macros in this file (the ones
 * that don't start with <code>__</code>), walk through the example test file in
 * this repo, and go forth to test!
 */

#ifndef TEST_H_INCLUDED
#define TEST_H_INCLUDED

// We have to violate this warning to allow variadic macros to expand correctly
#pragma GCC diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"

#include <inttypes.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#ifndef TEST_OMIT_RUNTIME
    #include <time.h>
#endif


/* ******************************* UTILITIES ******************************** */

/** Convert X to a string.  Stage 2 of __TEST_TOSTRING. */
#define __TEST_STRINGIFY(X) #X
/** Convert X to a string. */
#define __TEST_TOSTRING(X) __TEST_STRINGIFY(X)
/** The current line number as a string. */
#define __TEST_LINE_STR __TEST_TOSTRING(__LINE__)

/** Concatenate X and Y.  Stage 2 of __TEST_TOKEN_CONCAT. */
#define __TEST_TOKEN_CONCAT2(X, Y) X ## Y
/** Concatenate tokens X and Y, yielding a new token. */
#define __TEST_TOKEN_CONCAT(X, Y) __TEST_TOKEN_CONCAT2(X, Y)

/** Constructor priority for the tester setup routine. */
#define __TEST_SETUP_PRIORITY 101
/** Constructor priority for individual test functions. */
#define __TEST_PRIORITY 102

/** Maximum number of bytes that a test failure message may occupy. */
#define __TEST_MAX_MESSAGE_SIZE 1024

#ifndef UNUSED
    #define UNUSED __attribute__((unused))
#endif

/* Colours for terminal output.  May be suppressed by defining the symbol
 * TEST_MONOCHROME_OUTPUT. */
#ifdef TEST_MONOCHROME_OUTPUT
    #define __COLOUR_RED    ""
    #define __COLOUR_GREEN  ""
    #define __COLOUR_YELLOW ""
    #define __COLOUR_BLUE   ""
    #define __COLOUR_CYAN   ""
    #define __COLOUR_BOLD   ""
    #define __COLOUR_RESET  ""
#else
    #define __COLOUR_RED    "\x1B[1;31m"
    #define __COLOUR_GREEN  "\x1B[0;32m"
    #define __COLOUR_YELLOW "\x1B[1;33m"
    #define __COLOUR_BLUE   "\x1B[1;34m"
    #define __COLOUR_CYAN   "\x1B[0;36m"
    #define __COLOUR_BOLD   "\x1B[1m"
    #define __COLOUR_RESET  "\x1B[0m"
#endif


/* ******************************* FIXTURES ********************************* */

/**
 * An "empty" struct containing only an obfuscated pointer member.  Required to
 * maintain compatibility with compilers that do not support true empty structs.
 */
#define EMPTY { void *__TEST_TOKEN_CONCAT(NEVER_USE_THIS, __LINE__) UNUSED; }

/**
 * Declare a new test fixture with data members, but no setup or teardown code.
 * The data members declared in a fixture will be available to all tests in the
 * fixture.  Fixtures are declared very similarly to structs, as follows:
 *
 * <pre>
 * FIXTURE(Some_fixture) {
 *     char *message;
 *     size_t length;
 * };
 * </pre>
 *
 * Fixtures with no data members may be declared with the <code>EMPTY</code>
 * suffix, which is required because the C standard does not allow empty
 * structs:
 *
 * <pre>
 * FIXTURE(Empty_fixture) EMPTY;
 * </pre>
 */
#define FIXTURE(NAME)                                                         \
    struct NAME ## _fixture_data;                                             \
    /* Declare stub functions for fixture tasks. */                           \
    void NAME ## _fixture_setup_stub                                          \
            (struct NAME ## _fixture_data *TEST UNUSED) {}                    \
    void NAME ## _fixture_teardown_stub                                       \
            (struct NAME ## _fixture_data *TEST UNUSED) {}                    \
    /* Use stubs as actual tasks unless overridden. */                        \
    static void (*NAME ## _fixture_setup)(struct NAME ## _fixture_data*) =    \
            NAME ## _fixture_setup_stub;                                      \
    static void (*NAME ## _fixture_teardown)(struct NAME ## _fixture_data*) = \
            NAME ## _fixture_teardown_stub;                                   \
    struct NAME ## _fixture_data

/**
 * Declare a setup function for the fixture with the specified name.  The
 * fixture must already have been declared with the <code>FIXTURE</code>
 * directive prior to declaring its setup function.  Setup functions have very
 * similar syntax to normal C functions, as shown in the example below.  All
 * setup functions may access their fixtures' data members via the variable
 * <code>TEST</code>, a pointer to the struct declared with the
 * <code>FIXTURE</code> macro.
 *
 * <pre>
 * FIXTURE_SETUP(Some_fixture) {
 *     TEST->message = malloc(1024 * sizeof(char));
 * }
 * </pre>
 */
#define FIXTURE_SETUP(NAME)                                                    \
    /* Stub for overridden implementation. */                                  \
    void NAME ## _fixture_setup_impl(struct NAME ## _fixture_data *);          \
    /* Assign new implementation to the setup pointer. */                      \
    void __attribute__((constructor(__TEST_SETUP_PRIORITY)))                   \
    NAME ## _fixture_setup_override(void) {                                    \
        NAME ## _fixture_setup = NAME ## _fixture_setup_impl;                  \
    }                                                                          \
    void NAME ## _fixture_setup_impl(struct NAME ## _fixture_data *TEST UNUSED)


/**
 * Declare a teardown function for the fixture with the specified name.  The
 * semantics of teardown functions are identical to those of setup functions.
 * Example:
 *
 * <pre>
 * FIXTURE_TEARDOWN(Some_fixture) {
 *     free(TEST->message);
 * }
 * </pre>
 */
#define FIXTURE_TEARDOWN(NAME)                                                 \
    void NAME ## _fixture_teardown_impl(struct NAME ## _fixture_data *);       \
    void __attribute__((constructor(__TEST_SETUP_PRIORITY)))                   \
    NAME ## _fixture_teardown_override(void) {                                 \
        NAME ## _fixture_teardown = NAME ## _fixture_teardown_impl;            \
    }                                                                          \
    void NAME ## _fixture_teardown_impl                                        \
        (struct NAME ## _fixture_data *TEST UNUSED)


/* ******************************* TEST CORE ******************************** */

/** The total number of tests in this test suite. */
static uint64_t test_total_tests = 0;
/** The number of tests in this test suite that have failed. */
static uint64_t test_failed_tests = 0;

/**
 * Pointer to the shared memory region containing the failure message for the
 * most recently failed assertion.
 */
static char *test_failure_message;

// Define TEST_OMIT_RUNTIME to prohibit test runtimes from being printed.
#ifdef TEST_OMIT_RUNTIME
    #define __TEST_DIAGNOSTICS \
        __COLOUR_RESET " %s\n", name
#else
    /** printf arguments for the runtime details of the current test. */
    #define __TEST_DIAGNOSTICS                                 \
        __COLOUR_CYAN " (%7.3f/%3lus)" __COLOUR_RESET " %s\n", \
        (double) (end_clock - start_clock) / CLOCKS_PER_SEC,   \
        (end_time - start_time), name
#endif

/**
 * Main test runner function.  Runs the test with the specified name in a
 * separate process, additionally running the specified setup and teardown
 * functions before and after the main test function, respectively.
 *
 * @param name The name of the test to run.
 * @param setup_fn The fixture setup function for the test.
 * @param test_fn The main test function, containing the code under test and
 *                assertions.
 * @param teardown_fn The fixture teardown function for the test.
 * @param data_size The size in bytes of the data struct for the fixture to
 *                  which the test being run belongs.
 */
static void test_run(char *name,
                     void (*setup_fn)(void *),
                     void (*test_fn)(void *, char *),
                     void (*teardown_fn)(void *),
                     size_t data_size) {
    // Initial setup for test run
    test_total_tests++;
    void *test_data = malloc(data_size);
    setup_fn(test_data);

    // Measure both CPU time and wall-clock time
#ifndef TEST_OMIT_RUNTIME
    clock_t start_clock, end_clock;
    time_t start_time, end_time;
#endif
    printf(__COLOUR_BLUE "[CURR]" __COLOUR_RESET " %s\r", name);
#ifndef TEST_OMIT_RUNTIME
    start_time = time(NULL);
    start_clock = clock();
#endif

    // Run test in a separate thread
    pid_t child_pid = fork();
    if (child_pid == 0) {
        test_fn(test_data, test_failure_message);
        exit(EXIT_SUCCESS);
    }

    // Wait for test to finish
    int child_status;
    waitpid(child_pid, &child_status, 0);
#ifndef TEST_OMIT_RUNTIME
    end_clock = clock();
    end_time = time(NULL);
#endif

    if (WIFEXITED(child_status) && WEXITSTATUS(child_status) == EXIT_SUCCESS) {
#ifndef TEST_OMIT_SUCCESSES
        // Normal, successful test
        printf(__COLOUR_GREEN "[PASS]" __TEST_DIAGNOSTICS);
#endif
    } else if (WIFSIGNALED(child_status)) {
        // Test aborted due to a signal
        test_failed_tests++;
        int signum = WTERMSIG(child_status);
        printf(__COLOUR_YELLOW "[HALT]" __TEST_DIAGNOSTICS);
        printf(__COLOUR_YELLOW "|" __COLOUR_RESET " Test halted due to signal "
                __COLOUR_YELLOW "sig%s" __COLOUR_RESET " (code %d)\n",
                sys_signame[signum], signum);
    } else {
        // Test failed without signalling (i.e. due to a failed assertion)
        test_failed_tests++;
        printf(__COLOUR_RED "[FAIL]" __TEST_DIAGNOSTICS);
        printf(__COLOUR_RED "|" __COLOUR_RESET " %s\n", test_failure_message);
    }

    // Test completed, tear down environment
    teardown_fn(test_data);
    free(test_data);
}

/**
 * Declare a text with the specified name, belonging to the specified fixture.
 * The fixture must have be declared with the <code>FIXTURE</code> directive
 * prior to the test.  For each test, the fixture setup function will be run
 * first, then the body of the test as specified by this directive, then finally
 * the fixture teardown function will be run to conclude the test.  All tests
 * should, but are not required to, contain at least one assertion, a statement
 * that makes use of one of the <code>ASSERT_*</code> directives.  Like test
 * setup and teardown functions, tests are declared with a syntax very similar
 * to standard C functions, as follows:
 *
 * <pre>
 * TEST(Example_test, Some_fixture) {
 *     ASSERT_NON_NULL(TEST->message);
 *     strcpy(TEST->message, "Hello!");
 *     ASSERT_STREQ(TEST->message, "Hello!");
 *     ASSERT_EQ(strlen(TEST->message, 6));
 * }
 * </pre>
 *
 * As shown above, the data members of a test's fixture are made available via
 * the pointer <code>TEST</code>, as in the fixture functions.  Furthermore,
 * note that tests that contain multiple assertions will be terminated
 * immediately once any assertion fails or a signal is sent that causes the test
 * to halt.  Tests may not be recovered once they fail.
 */
#define TEST(NAME, FIXTURE)                                               \
    /* Forward declare test function to allow convenient syntax */        \
    void FIXTURE ## _ ## NAME ## _test(struct FIXTURE ## _fixture_data *, \
                                       char *);                           \
    /* Actual test runner, calls the user's function */                   \
    void __attribute__((constructor(__TEST_PRIORITY)))                    \
    FIXTURE ## _ ## NAME ## _test_run() {                                 \
        test_run(#NAME,                                                   \
                 (void (*)(void *)) FIXTURE ## _fixture_setup,            \
                 (void (*)(void*, char*)) FIXTURE ## _ ## NAME ## _test,  \
                 (void (*)(void *)) FIXTURE ## _fixture_teardown,         \
                 sizeof(struct FIXTURE ## _fixture_data));                \
    }                                                                     \
    /* User-declared test function */                                     \
    void FIXTURE ## _ ## NAME ## _test(                                   \
            struct FIXTURE ## _fixture_data *TEST UNUSED,                 \
            char *TEST_MESSAGE UNUSED)

/** Shorthand for fixture data member access.  Save yourself some typing! */
#define T_ TEST ->

/**
 * Kickoff function for a test suite.  Initialises shared memory and output
 * streams, and prints a header to indicate the beginning of the test suite.
 */
static void __attribute__((constructor(__TEST_SETUP_PRIORITY)))
test_init(void) {
    // Allocate shared memory for the failure message
    test_failure_message = mmap(NULL, __TEST_MAX_MESSAGE_SIZE,
            PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    // Do not buffer stdout to allow carriage return overwrites
    setbuf(stdout, NULL);
    puts(__COLOUR_BOLD "================================ BEGIN TEST RUN ================================" __COLOUR_RESET);
}

/**
 * Teardown function for a test suite.  De-allocates shared memory and prints a
 * summary of the suite's run.
 */
static void __attribute__((destructor)) test_end(void) {
    puts("\n" __COLOUR_BOLD "================================= TEST SUMMARY =================================" __COLOUR_RESET);
    if (test_failed_tests == 0) {
        printf(__COLOUR_GREEN __COLOUR_BOLD "All %llu test(s) passed!\n"
                __COLOUR_RESET, test_total_tests);
    } else {
        printf(__COLOUR_RED "%llu test(s) failed!\n" __COLOUR_RESET,
                test_failed_tests);
        printf("Total tests: %llu\n", test_total_tests);
    }

    // Free shared memory region
    munmap(test_failure_message, __TEST_MAX_MESSAGE_SIZE);
}


/* ************************** PARAMETERISED TESTS *************************** */

// Iteration implemented via recursion in the C preprocessor.  Yep.
// Derived from https://github.com/swansontec/map-macro
#define __ITER_EVAL0(...) __VA_ARGS__
#define __ITER_EVAL1(...) __ITER_EVAL0(__ITER_EVAL0(__ITER_EVAL0(__VA_ARGS__)))
#define __ITER_EVAL2(...) __ITER_EVAL1(__ITER_EVAL1(__ITER_EVAL1(__VA_ARGS__)))
#define __ITER_EVAL3(...) __ITER_EVAL2(__ITER_EVAL2(__ITER_EVAL2(__VA_ARGS__)))
#define __ITER_EVAL4(...) __ITER_EVAL3(__ITER_EVAL3(__ITER_EVAL3(__VA_ARGS__)))
#define __ITER_EVAL(...)  __ITER_EVAL4(__ITER_EVAL4(__ITER_EVAL4(__VA_ARGS__)))

#define __ITER_FINISH
#define __ITER_END(...)
#define __ITER_LAST_CALL() 0, __ITER_END
#define __ITER_NEXT0(ITEM, NEXT, ...) NEXT __ITER_FINISH
#define __ITER_NEXT1(ITEM, NEXT) __ITER_NEXT0(ITEM, NEXT, 0)
#define __ITER_NEXT(ITEM, NEXT) __ITER_NEXT1(__ITER_LAST_CALL ITEM, NEXT)

#define __ITER0(OP, CONST1, CONST2, VAL, NEXT, ...) OP(VAL, CONST1, CONST2) \
    __ITER_NEXT(NEXT, __ITER1)(OP, CONST1, CONST2, NEXT, __VA_ARGS__)
#define __ITER1(OP, CONST1, CONST2, VAL, NEXT, ...) OP(VAL, CONST1, CONST2) \
    __ITER_NEXT(NEXT, __ITER0)(OP, CONST1, CONST2, NEXT, __VA_ARGS__)

#define __ITER(OP, CONST1, CONST2, ...) \
    __ITER_EVAL(__ITER1(OP, CONST1, CONST2, __VA_ARGS__, (), 0))

/** Declare a single instance of a parameterised test. */
#define __TEST_DECLARE_PARAM_NUM(PARAMS, NAME, FIXTURE, COUNTER)               \
    /* Simple wrapper for the normal setup function and test parameters */     \
    void FIXTURE ## _ ## NAME ## _setup_ ## COUNTER(                           \
            struct FIXTURE ## _fixture_data *TEST) {                           \
        FIXTURE ## _fixture_setup(TEST);                                       \
        PARAMS                                                                 \
    }                                                                          \
    /* Actual test runner */                                                   \
    void __attribute__((constructor(__TEST_PRIORITY)))                         \
    FIXTURE ## _ ## NAME ## _test_run_ ## COUNTER() {                          \
        test_run(#NAME " " #PARAMS,                                            \
                 (void (*)(void *)) FIXTURE ## _ ## NAME ## _setup_ ## COUNTER,\
                 (void (*)(void*, char*)) FIXTURE ## _ ## NAME ## _test,       \
                 (void (*)(void *)) FIXTURE ## _fixture_teardown,              \
                 sizeof(struct FIXTURE ## _fixture_data));                     \
    }

/** Helper for parameterised test declaration; reifies the counter. */
#define __TEST_DECLARE_PARAM_EXPAND(PARAMS, NAME, FIXTURE, COUNTER) \
    __TEST_DECLARE_PARAM_NUM(PARAMS, NAME, FIXTURE, COUNTER)

/** "Callback" for iteration over parameterised test instances. */
#define __TEST_DECLARE_PARAM_TEST(PARAMS, NAME, FIXTURE) \
    __TEST_DECLARE_PARAM_EXPAND(PARAMS, NAME, FIXTURE, __COUNTER__)

/**
 * Declare a new parameterised test with the specified name in the specified
 * fixture.  A parameterised test is a construct that runs the same test over a
 * series of different inputs, allowing for functionality to be tested with many
 * different parameters without requiring any duplication of code.  In the
 * <code>PTEST</code> directive, these parameters are blocks of executable code
 * that modify the test's fixture's data members (available via the
 * <code>TEST</code> pointer or <code>T_</code> as in the <code>TEST</code>
 * directive); parameter blocks are run after the fixture setup function, but
 * before the body of the test is executed.  As such, the parameters for all
 * parameterised tests must be declared in their fixtures prior to the test
 * declaration.  An example of a parameterised test is as follows:
 *
 * <pre>
 * PTEST(strlen_works_correctly, Some_fixture,
 *       { TEST->message = ""; TEST->length = 0; },
 *       { TEST->message = "Hello!"; TEST->length = 6; },
 *       { TEST->message = "This hurts the preprocessor."; TEST->length = 28; })
 * {
 *     ASSERT_NON_NULL(TEST->message);
 *     ASSERT_EQ(strlen(TEST->message), TEST->length);
 * }
 * </pre>
 */
#define PTEST(NAME, FIXTURE, ...)                                         \
    /* Forward declare test function to allow convenient syntax */        \
    void FIXTURE ## _ ## NAME ## _test(struct FIXTURE ## _fixture_data *, \
                                       char *);                           \
    __ITER(__TEST_DECLARE_PARAM_TEST, NAME, FIXTURE, __VA_ARGS__)         \
    /* User-declared test function */                                     \
    void FIXTURE ## _ ## NAME ## _test(                                   \
            struct FIXTURE ## _fixture_data *TEST UNUSED,                 \
            char *TEST_MESSAGE UNUSED)


/* ****************************** ASSERTIONS ******************************** */

/**
 * Run an assertion checking the truthiness of the specified expression, using
 * the remainder of the arguments as the arguments to a printf call formatting
 * the message to be printed if the assertion fails.
 */
#define __TEST_RUN_ASSERTION(EXPRESSION, ...)                           \
    do {                                                                \
        if (!(EXPRESSION)) {                                            \
            snprintf(TEST_MESSAGE, __TEST_MAX_MESSAGE_SIZE,             \
                    "Assertion failed at " __FILE__ ":" __TEST_LINE_STR \
                    "\n" __COLOUR_RED "| " __COLOUR_RESET __VA_ARGS__); \
            exit(EXIT_FAILURE);                                         \
        }                                                               \
    } while (false)

/** Assert that the specified expression is truthy (non-0). */
#define ASSERT_TRUE(EXPRESSION) \
    __TEST_RUN_ASSERTION(EXPRESSION, "Expression is false (true expected): " \
            #EXPRESSION)

/** Assert that the specified expression is falsy (0). */
#define ASSERT_FALSE(EXPRESSION) \
    __TEST_RUN_ASSERTION(!(EXPRESSION), "Expression is true (false expected): "\
            #EXPRESSION)

/**
 * Assert that the contents of strings (<code>char *</code>) <code>A</code> and
 * <code>B</code> are equal.
 */
#define ASSERT_STREQ(A, B) \
    __TEST_RUN_ASSERTION(strcmp(A, B) == 0, \
            "Strings are not equal: \"%s\" and \"%s\"", A, B)

/** Assert that pointer <code>PTR</code> is null (0). */
#define ASSERT_NULL(PTR) \
    __TEST_RUN_ASSERTION(PTR == NULL, "Pointer is non-null: " #PTR)

/** Assert that pointer <code>PTR</code> is not null (non-0). */
#define ASSERT_NON_NULL(PTR) \
    __TEST_RUN_ASSERTION(PTR != NULL, "Pointer is null: " #PTR)

/** Create a pretty printer function for the specified type. */
#define __TEST_SNPRINTF_FUN(NAME, TYPE, FORMAT, ...)                        \
    void test_snprintf_ ## NAME(TYPE val, char *buffer, int *running_pos) { \
        int last_pos;                                                       \
        snprintf(buffer + *running_pos,                                     \
                __TEST_MAX_MESSAGE_SIZE - *running_pos,                     \
                FORMAT "%n", __VA_ARGS__,  &last_pos);                      \
        *running_pos += last_pos;                                           \
    }

// Pretty-print function definitions
__TEST_SNPRINTF_FUN(bool, bool, "%s", (val ? "true" : "false"))
__TEST_SNPRINTF_FUN(char, char, "%c (0x%" PRIx8 ")", val, val)
__TEST_SNPRINTF_FUN(uint8_t, uint8_t, "%" PRIu8 " (0x%" PRIx8 ")", val, val)
__TEST_SNPRINTF_FUN(uint16_t, uint16_t, "%" PRIu16 " (0x%" PRIx16 ")", val, val)
__TEST_SNPRINTF_FUN(uint32_t, uint32_t, "%" PRIu32 " (0x%" PRIx32 ")", val, val)
__TEST_SNPRINTF_FUN(uint64_t, uint64_t, "%" PRIu64 " (0x%" PRIx64 ")", val, val)
__TEST_SNPRINTF_FUN(int8_t, int8_t, "%" PRId8, val)
__TEST_SNPRINTF_FUN(int16_t, int16_t, "%" PRId16, val)
__TEST_SNPRINTF_FUN(int32_t, int32_t, "%" PRId32, val)
__TEST_SNPRINTF_FUN(int64_t, int64_t, "%" PRId64, val)
__TEST_SNPRINTF_FUN(float, float, "%f", val)
__TEST_SNPRINTF_FUN(double, double, "%f", val)
__TEST_SNPRINTF_FUN(long_double, long double, "%Lf", val)
__TEST_SNPRINTF_FUN(size_t, size_t, "%zu", val)
__TEST_SNPRINTF_FUN(string, char *, "%s", val)
__TEST_SNPRINTF_FUN(pointer, void *, "%p", val)

/** Call the appropriate __TEST_SNPRINTF_FUN for the specified type. */
#define __TEST_KNOWN_SNPRINTF(TYPE, VAL) \
        test_snprintf_ ## TYPE(VAL, TEST_MESSAGE, &test_running_pos)

/**
 * Print a properly-formatted representation of the specified pointer type to
 * the failure message buffer.  This indirection is required to reduce the
 * number of uintptr_t casts in the main __TEST_GENERIC_PRINT macro, which are
 * required to silence compiler warnings about scalar -> pointer conversion.
 * Also, the explicit casts to (char *) and (void *) are required to prevent
 * type-checker errors if arrays are passed to this macro.
 */
#define __TEST_GENERIC_PRINT_PTR(VAL, REPR)                    \
    _Generic((VAL),                                            \
        char *:  __TEST_KNOWN_SNPRINTF(string, (char *) VAL),  \
        void *:  __TEST_KNOWN_SNPRINTF(pointer, (void *) VAL), \
        default: __TEST_KNOWN_SNPRINTF(string, "<unknown type> (" REPR ")"))

/**
 * Print a properly-formatted representation of the specified value to the
 * failure message buffer.
 */
#define __TEST_GENERIC_PRINT(VAL, REPR)                       \
    _Generic((VAL),                                           \
        bool:        __TEST_KNOWN_SNPRINTF(bool, VAL),        \
        char:        __TEST_KNOWN_SNPRINTF(char, VAL),        \
        uint8_t:     __TEST_KNOWN_SNPRINTF(uint8_t, VAL),     \
        uint16_t:    __TEST_KNOWN_SNPRINTF(uint16_t, VAL),    \
        uint32_t:    __TEST_KNOWN_SNPRINTF(uint32_t, VAL),    \
        uint64_t:    __TEST_KNOWN_SNPRINTF(uint64_t, VAL),    \
        int8_t:      __TEST_KNOWN_SNPRINTF(int8_t, VAL),      \
        int16_t:     __TEST_KNOWN_SNPRINTF(int16_t, VAL),     \
        int32_t:     __TEST_KNOWN_SNPRINTF(int32_t, VAL),     \
        int64_t:     __TEST_KNOWN_SNPRINTF(int64_t, VAL),     \
        float:       __TEST_KNOWN_SNPRINTF(float, VAL),       \
        double:      __TEST_KNOWN_SNPRINTF(double, VAL),      \
        long double: __TEST_KNOWN_SNPRINTF(long_double, VAL), \
        size_t:      __TEST_KNOWN_SNPRINTF(size_t, VAL),      \
        default:     __TEST_GENERIC_PRINT_PTR((uintptr_t) VAL, REPR))

/**
 * Run an assertion to ensure that the specified COMPARISON is satisfied for
 * values A and B.  The values of A and B are saved before the comparison is
 * evaluated, so it is safe to write expressions with side effects for either.
 */
#define __TEST_GENERIC_ASSERTION(A, COMPARISON, B)                  \
    do {                                                            \
        int test_last_pos;                                          \
        int test_running_pos = 0;                                   \
        typeof(A) test_var_a = (A);                                 \
        typeof(B) test_var_b = (B);                                 \
        if (!(test_var_a COMPARISON test_var_b)) {                  \
            __TEST_KNOWN_SNPRINTF(string, "Expression is false: "); \
            __TEST_GENERIC_PRINT(test_var_a, #A);                   \
            __TEST_KNOWN_SNPRINTF(string, " " #COMPARISON " ");     \
            __TEST_GENERIC_PRINT(test_var_b, #B);                   \
            exit(EXIT_FAILURE);                                     \
        }                                                           \
    } while (0)

/** Assert that value <code>A</code> is equal to value <code>B</code>. */
#define ASSERT_EQ(A, B) __TEST_GENERIC_ASSERTION(A, ==, B)

/** Assert that value <code>A</code> is not equal to value <code>B</code>. */
#define ASSERT_NE(A, B) __TEST_GENERIC_ASSERTION(A, !=, B)

/** Assert that value <code>A</code> is greater than value <code>B</code>. */
#define ASSERT_GT(A, B) __TEST_GENERIC_ASSERTION(A, >, B)

/**
 * Assert that value <code>A</code> is greater than or equal to value
 * <code>B</code>.
 */
#define ASSERT_GE(A, B) __TEST_GENERIC_ASSERTION(A, >=, B)

/** Assert that value <code>A</code> is less than value <code>B</code>. */
#define ASSERT_LT(A, B) __TEST_GENERIC_ASSERTION(A, <, B)

/**
 * Assert that value <code>A</code> is less than or equal to value
 * <code>B</code>.
 */
#define ASSERT_LE(A, B) __TEST_GENERIC_ASSERTION(A, <=, B)

#endif

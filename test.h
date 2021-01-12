/**
 * @file test.h
 * @author Andrew Smith
 * @brief A testing framework implemented entirely using the C preprocessor.
 *
 * This file implements a fairly full-featured unit testing framework whose
 * logic is implemented entirely in C preprocessor macros, and that has no
 * external dependencies outside of the C standard library.  This framework, if
 * it can really be called such, was written in response to the perceived
 * excessive complexity of other unit testing systems, which required external
 * dependency management systems and special compiler flags just to get them
 * working.  With <code>test.h</code>, that is not so: simply
 * <code>#include "test.h"</code> at the top of a C file containing your test
 * suite and you're off to the races.
 *
 * <code>test.h</code> makes use of some fairly arcane compiler features to
 * auto-register test and fixture lifecycle functions.  Its syntax and output
 * format are similar to those of the popular Google Test C++ testing framework
 * with no runtime overhead (what's a <code>malloc</code>?) and only a small
 * amount of boilerplate.
 *
 * Internally, <code>test.h</code> uses similar features of the PE (Portable
 * Executable) and ELF binary formats on Windows and all other common operating
 * systems, respectively.  For ELF, it makes use of ELF constructors, functions
 * referenced in the <code>.ctors</code> section of binaries that are run before
 * control is transferred to <code>_start</code> [<code>main()</code> in C].
 * Usually, these functions are used to perform shared library initialisation
 * tasks; however, in <code>test.h</code>, we take advantage of this feature of
 * the ELF format to automatically register tests to run before
 * <code>main()</code>, in essence giving us the ability to run functions
 * without explicitly calling them.
 *
 * A similar trick is exploited on Windows, although it instead relies upon the
 * CRT initialisation facilities of the PE format and some quirks of the MSVC
 * preprocessor.  While GCC and Clang support the explicit ordering of
 * constructor functions, MSVC makes no such guarantee, os we instead use two
 * separate PE sections for test initialisation and registration.  The first,
 * <code>.CRT$XIU</code>, is normally used for the initialisation of static
 * objects in object-oriented programming languages; furthermore, it is one of
 * the first sections to which control is transferred in the Windows process
 * setup procedure.  This section is used to register fixture lifecycle
 * functions and is delimited by the pragmas <code>FIXTURE_START</code> and
 * <code>FIXTURE_END</code>, a syntactical convenience made possible by the fact
 * that MSVC's preprocessor performs macro substitution inside
 * <code>#pragma</code> directives.  Actual test functions are placed inside the
 * <code>.CRT$XCU</code> section, which is normally used for C runtime (CRT)
 * initialisation; since functions referenced in this section are run after
 * those in <code>.CRT$XIU</code>, this feature gives us the assurance that test
 * functions will only be run after their corresponding fixture lifecycle
 * functions have been registered.  This section is delimited by the pragmas
 * <code>TEST_START</code> and <code>TEST_END</code>, which are implemented
 * similarly to the fixture pragmas.
 *
 * As a result of these implementation details, the code in <code>test.h</code>
 * is <em>highly</em> compiler-dependent, but is stable on the most common
 * compilers for the most common operating systems, both ancient and modern.
 * Now that you know the rough details of how <code>test.h</code> works, read
 * through the documentation of the public macros in this file (the ones that
 * don't start with <code>_</code>), walk through the example test file in this
 * repo, and go forth to test!
 */

#ifndef TEST_H_INCLUDED
#define TEST_H_INCLUDED

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ************************ PLATFORM SUPPORT MACROS ************************* */

/* Concatenate the fully-expanded tokens X and Y into a single token. */
#define _TEST_TOKEN_CONCAT(X, Y) _TEST_TOKEN_CONCAT_IMPL(X, Y)
#define _TEST_TOKEN_CONCAT_IMPL(X, Y) X ## Y

/* Convert the fully expanded token X into a string representation. */
#define _TEST_STRINGIFY(X) _TEST_STRINGIFY_IMPL(X)
#define _TEST_STRINGIFY_IMPL(X) #X

/* The current source line number as a string. */
#define _TEST_LINE_STR _TEST_STRINGIFY(__LINE__)

/* The maximum number of characters that may be in a test failure message. */
#define _TEST_MAX_FAILURE_LENGTH 1024

/**
 * Pragma to begin the executable portion of test files.  You should write this
 * pragma after you have declared all of your test fixtures and their lifecycle
 * functions, and before your first actual test declaration.  The form of the
 * pragma to write is as follows:
 *
 * @code{.c}
 * #pragma TEST_START
 * @endcode
 *
 * While this pragma is not necessary (and is actually syntactically invalid) on
 * GCC/Clang and modern MSVC, you should still write it to ensure that your test
 * code is compatible with older MSVC versions.
 */
#define TEST_START

/**
 * Pragma to end the executable section of test files.  You should write this
 * pragma after you have declared all of your test functions.  The form of the
 * pragma to write is as follows:
 *
 * @code{.c}
 * #pragma TEST_END
 * @endcode
 *
 * Like <code>TEST_START</code>, this pragma is not actually necessary on
 * GCC/Clang, but is highly recommended for MSVC compatibility.
 */
#define TEST_END

/**
 * Pragma to begin the fixture declaration portion of test files.  You should
 * write this pragma before your first fixture declaration.  The form of the
 * pragma to write is as follows:
 *
 * @code{.c}
 * #pragma FIXTURE_START
 * @endcode
 *
 * While this pragma is not necessary (and is actually syntactically invalid) on
 * GCC/Clang and modern MSVC, you should still write it to ensure that your test
 * code is compatible with older MSVC versions.
 */
#define FIXTURE_START

/**
 * Pragma to end the fixture declaration portion of test files.  You should
 * write this pragma after your last fixture or fixture lifecycle function
 * declaration.  The form of the pragma to write is as follows:
 *
 * @code{.c}
 * #pragma FIXTURE_END
 * @endcode
 *
 * Like <code>FIXTURE_START</code>, this pragma is not actually necessary on
 * GCC/Clang, but is highly recommended for MSVC compatibility.
 */
#define FIXTURE_END

/* PE sections in which test functions should be allocated. */
#define _TEST_FIXTURE_SECTION ".CRT$XIU"
#define _TEST_TEST_SECTION ".CRT$XCU"

#ifdef _MSC_VER

/* Disable warnings about platform-specific secure string functions. */
#define _CRT_SECURE_NO_WARNINGS 1

/* Some newer versions of MSVC may have a bug that results in the CRT PE
 * sections, which we use extensively to ensure that test functions are run at
 * the proper time, not being configured properly, resulting in linker errors.
 * Explicitly defining these sections should ensure that linking succeeds. */
#if _MSC_VER >= 1400
#pragma section(_TEST_FIXTURE_SECTION, long, read)
#pragma section(".CRT$XCU", long, read)
#endif // _MSC_VER >= 1400

#if _MSC_VER < 1400
#undef TEST_START
/**
 * Pragma to begin the executable portion of test files.  You should write this
 * pragma after you have declared all of your test fixtures and their lifecycle
 * functions, and before your first actual test declaration.  The form of the
 * pragma to write is as follows:
 *
 * @code{.c}
 * #pragma TEST_START
 * @endcode
 *
 * This pragma is necessary to ensure that your test functions are run once the
 * test executable is run.  In order to run tests before the <code>main()</code>
 * function on Windows, we place them in the PE section <code>.CRT$XCU</code>,
 * which is normally used to initialise the C Runtime Library (CRT), but which
 * we can repurpose for running tests out-of-line here.  Note that the code
 * snippet above is syntactically valid in MSVC, as it performs macro expansion
 * in pragma directives.
 */
#define TEST_START data_seg(".CRT$XCU")

#undef TEST_END
/**
 * Pragma to end the executable section of test files.  You should write this
 * pragma after you have declared all of your test functions.  The form of the
 * pragma to write is as follows:
 *
 * @code{.c}
 * #pragma TEST_END
 * @endcode
 *
 * This pragma returns the linker target to the main section, returning control
 * to the post-initialisation portion of the process lifecycle.
 */
#define TEST_END data_seg()

#undef FIXTURE_START
/**
 * Pragma to begin the fixture declaration portion of test files.  You should
 * write this pragma before your first fixture declaration.  The form of the
 * pragma to write is as follows:
 *
 * @code{.c}
 * #pragma FIXTURE_START
 * @endcode
 *
 * This pragma is necessary to ensure that your fixture lifecycle functions are
 * run before any test functions by placing them in the <code>.CRT$XIU</code> PE
 * section, which is normally used for static initialisations in object-oriented
 * programming languages.
 */
#define FIXTURE_START data_seg(_TEST_FIXTURE_SECTION)

#undef FIXTURE_END
/**
 * Pragma to end the fixture declaration portion of test files.  You should
 * write this pragma after your last fixture or fixture lifecycle function
 * declaration.  The form of the pragma to write is as follows:
 *
 * @code{.c}
 * #pragma FIXTURE_END
 * @endcode
 *
 * The rationale for this pragma is the same as that for <code>TEST_END</code>;
 * refer to that macro's documentation for more information.
 *
 * @see <code>TEST_END</code> for implementation details.
 */
#define FIXTURE_END data_seg()
#endif // _MSC_VER < 1400

/* No attribute is supported to run a function before test functions on MSVC. */
#define _TEST_FIXTURE_LIFECYCLE

/* Prefix for test functions. */
#define _TEST_PROLOGUE(SECTION) __declspec(allocate(SECTION))

/* No attribute is supported to run a function before main() on MSVC. */
#define _TEST_RUNNER

/* MSVC doesn't allow you to declare an identifier as unused, but it also
 * doesn't really care about unused identifiers, so no harm done. */
#define _TEST_UNUSED

/* Write an actual function pointer to the data segment to make the CRT run the
 * test function with the specified NAME. */
#define _TEST_EPILOGUE(NAME, SECTION) \
    _TEST_PROLOGUE(SECTION)           \
    int (*_TEST_TOKEN_CONCAT(test_prerun_, NAME))(void) = NAME;

/* Colours for test output.  Disabled on Windows due to a lack of ANSI escape
 * sequence support in conhost/CSRSS prior to Windows 10. */
#define _TEST_COLOUR_HEADER
#define _TEST_COLOUR_START
#define _TEST_COLOUR_PASS
#define _TEST_COLOUR_FAIL
#define _TEST_COLOUR_RUNTIME
#define _TEST_COLOUR_SKIP
#define _TEST_COLOUR_MUTE
#define _TEST_COLOUR_VALUE
#define _TEST_COLOUR_RESET

/* Multi-character newline for Windows. */
#define _TEST_NEWLINE "\r\n"

#else

/* Disable "unknown pragma" warnings on GCC/Clang.  Allows the TEST_START and
 * TEST_END macros to be used without triggering compiler errors. */
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma GCC diagnostic ignored "-Wattributes"

/* Attribute to run a fixture setup redirection function on GCC/Clang. */
#define _TEST_FIXTURE_LIFECYCLE __attribute__((constructor(101)))

/* Prefix for test function pointers.  Not really necessary for GCC/Clang, but
 * we'll put the pointers in static storage for consistency. */
#define _TEST_PROLOGUE(SECTION) static

/* Attribute to run a function defined before main() on GCC/Clang. */
#define _TEST_RUNNER __attribute__((constructor(102)))

/* Silence unused identifier warnings to ensure that tests compile properly with
 * "-Wall -Werror" set. */
#define _TEST_UNUSED __attribute__((unused))

/* No epilogue is required on GCC/Clang, as _TEST_RUNNER ensures that tests will
 * be automatically run. */
#define _TEST_EPILOGUE(NAME, SECTION)

/* Colours for test output, as ANSI escape sequences. */
#define _TEST_ESC(COMMAND) "\x1B[" COMMAND "m"
#define _TEST_COLOUR_HEADER  _TEST_ESC("1")       /* Bold */
#define _TEST_COLOUR_START   _TEST_ESC("1;34")    /* Bold blue */
#define _TEST_COLOUR_PASS    _TEST_ESC("1;32")    /* Bold green */
#define _TEST_COLOUR_FAIL    _TEST_ESC("1;31")    /* Bold red */
#define _TEST_COLOUR_RUNTIME _TEST_ESC("0;36")    /* Cyan */
#define _TEST_COLOUR_SKIP    _TEST_ESC("1;90")    /* Bold gray */
#define _TEST_COLOUR_MUTE    _TEST_ESC("0;90")    /* Gray */
#define _TEST_COLOUR_VALUE   _TEST_ESC("0;33")    /* Yellow */
#define _TEST_COLOUR_RESET   _TEST_ESC("0")       /* Reset foreground colour */

/* Newline character for all modern non-Windows systems. */
#define _TEST_NEWLINE "\n"

#endif

/* ***************************** TEST FIXTURES ****************************** */

/**
 * An "empty" struct body containing only an obfuscated pointer member.
 * Required to maintain compatibility with older compilers that do not support
 * empty structs.
 */
#define EMPTY {                                                       \
    void *_TEST_TOKEN_CONCAT(NEVER_USE_THIS_, __LINE__) _TEST_UNUSED; \
}

/**
 * Declare a new test fixture with data members, but no setup or teardown code.
 * The data members declared in a fixture will be available to all tests in the
 * fixture.  Fixtures are declared very similarly to structs, as follows:
 *
 * @code{.c}
 * FIXTURE(Some_fixture) {
 *     char *message;
 *     unsigned long length;
 * };
 * @endcode
 *
 * Fixtures with no data members may be declared with the <code>EMPTY</code>
 * suffix, which is required because the C standard does not permit empty struct
 * definitions:
 *
 * @code{.c}
 * FIXTURE(Empty_fixture) EMPTY;
 * @endcode
 *
 * Note that this directive must appear inside a <code>FIXTURE_START</code>,
 * <code>FIXTURE_END</code> block.
 */
#define FIXTURE(NAME)                                                         \
    struct NAME ## _fixture_data;                                             \
    /* Declare stubs for fixture lifecycle functions.  Note that these functions
     * will be automatically run on Windows, but since they're empty it doesn't
     * really matter. */                                                      \
    static void NAME ## _fixture_setup_stub                                   \
            (struct NAME ## _fixture_data *TEST _TEST_UNUSED) {}              \
    static void NAME ## _fixture_teardown_stub                                \
            (struct NAME ## _fixture_data *TEST _TEST_UNUSED) {}              \
    /* Use stubs as actual tasks unless overridden later.  Note that Windows
     * throws a memory access violation if the pointers are directly assigned,
     * so we have to do the assignment in separate functions.  Why does this
     * happen?  Who knows. */                                                 \
    static void (*NAME ## _fixture_setup)(struct NAME ## _fixture_data *)     \
            _TEST_UNUSED = NAME ## _fixture_setup_stub;                       \
    static void (*NAME ## _fixture_teardown)(struct NAME ## _fixture_data *)  \
            _TEST_UNUSED;                                                     \
    static int _TEST_FIXTURE_LIFECYCLE NAME ## _fixture_teardown_init(void) { \
        NAME ## _fixture_teardown = NAME ## _fixture_teardown_stub;           \
        return 0;                                                             \
    }                                                                         \
    _TEST_EPILOGUE(NAME ## _fixture_teardown_init, _TEST_FIXTURE_SECTION)     \
    struct NAME ## _fixture_data

/**
 * Declare a fixture setup function for the fixture with the specified name.
 * The fixture must already have been declared with the <code>FIXTURE</code>
 * directive prior to declaring its setup function.  Setup functions have very
 * similar syntax to normal C functions, as demonstrated in the example below.
 * All setup functions may access their fixtures' data members via the variable
 * <code>TEST</code>, a pointer to the struct declared with the
 * <code>FIXTURE</code> directive.  Example:
 *
 * @code{.c}
 * FIXTURE_SETUP(Some_fixture) {
 *     TEST->message = malloc(1024 * sizeof(char));
 * }
 * @endcode
 *
 * Note that this directive must appear inside a <code>FIXTURE_START</code>,
 * <code>FIXTURE_END</code> block.
 */
#define FIXTURE_SETUP(NAME)                                                    \
    /* Forward declaration of overridden implementation. */                    \
    static void NAME ## _fixture_setup_impl(struct NAME ## _fixture_data *);   \
    /* Assign new implementation to the setup pointer. */                      \
    static int _TEST_FIXTURE_LIFECYCLE NAME ## _fixture_setup_override(void) { \
        NAME ## _fixture_setup = NAME ## _fixture_setup_impl;                  \
        return 0;                                                              \
    }                                                                          \
    /* Make the override function run on Windows. */                           \
    _TEST_EPILOGUE(NAME ## _fixture_setup_override, _TEST_FIXTURE_SECTION)     \
    static void NAME ## _fixture_setup_impl(                                   \
            struct NAME ## _fixture_data *TEST _TEST_UNUSED)

/**
 * Declare a fixture teardown function for the fixture with the specified name.
 * The semantics of teardown functions are identical to those of setup
 * functions, although the operations that they perform are often the inverse.
 * Example:
 *
 * @code{.c}
 * FIXTURE_TEARDOWN(Some_fixture) {
 *     free(TEST->message);
 * }
 * @endcode
 *
 * Note that this directive must appear inside a <code>FIXTURE_START</code>,
 * <code>FIXTURE_END</code> block.
 */
#define FIXTURE_TEARDOWN(NAME)                                                 \
    static void NAME ## _fixture_teardown_impl(struct NAME ## _fixture_data *);\
    static int _TEST_FIXTURE_LIFECYCLE                                         \
    NAME ## _fixture_teardown_override(void) {                                 \
        NAME ## _fixture_teardown = NAME ## _fixture_teardown_impl;            \
        return 0;                                                              \
    }                                                                          \
    _TEST_EPILOGUE(NAME ## _fixture_teardown_override, _TEST_FIXTURE_SECTION)  \
    static void NAME ## _fixture_teardown_impl(                                \
            struct NAME ## _fixture_data *TEST _TEST_UNUSED)

/* ******************************* TEST CORE ******************************** */

/** The number of tests in this test suite that have passed. */
static unsigned long test_passed_tests = 0;
/** The number of tests in this test suite that have failed. */
static unsigned long test_failed_tests = 0;
/** The number of tests in this test suite that were skipped. */
static unsigned long test_skipped_tests = 0;

/** The failure message for the most recently failed assertion. */
static char test_failure_message[_TEST_MAX_FAILURE_LENGTH];

/** Return codes for test functions indicating their final statuses. */
enum test_status {
    TEST_PASSED,     /**< The test passed with no failing assertions. */
    TEST_FAILED,     /**< The test failed because of a failing assertion. */
    TEST_SKIPPED,    /**< The test was skipped. */
};

/** The exit status of the last test run. */
static enum test_status test_last_status;

/** A test callback, be it a fixture lifecycle function or a test itself. */
typedef void (*test_fn_t)(void *);

/* printf arguments for the runtime details of the current test. */
#define _TEST_DIAGNOSTICS                                               \
    _TEST_COLOUR_RUNTIME " (%7.3f/%3lus)" _TEST_COLOUR_RESET " %s"      \
    _TEST_NEWLINE, (double) (end_clock - start_clock) / CLOCKS_PER_SEC, \
    (unsigned long) difftime(end_time, start_time), name

/**
 * Main test runner function.  Runs the test with the specified name,
 * additionally running the specified setup and teardown functions before and
 * after the main test function, respectively.
 *
 * @param name The name of the test to run.
 * @param setup_fn The fixture setup function for the test.
 * @param test_fn The main test function, containing the code under test and any
 *                assertions used to determine the test status.
 * @param teardown_fn The fixture teardown function for the test.
 * @param data_size The size in bytes of the data struct for the fixture to
 *                  which the test being run belongs.
 */
static void test_run(char *name, test_fn_t setup_fn, test_fn_t test_fn,
                     test_fn_t teardown_fn, unsigned long data_size) {
    /* We want to measure both CPU time and wall-clock time. */
    clock_t start_clock, end_clock;
    time_t start_time, end_time;

    /* Initial setup for test run. */
    void *test_data = malloc(data_size);
    setup_fn(test_data);

    /* Run the test and store its return status. */
    printf(_TEST_COLOUR_START "[ START      ]" _TEST_COLOUR_RESET " %s",
           name);
    test_last_status = TEST_PASSED;
    start_time = time(NULL);
    start_clock = clock();
    test_fn(test_data);
    end_time = time(NULL);
    end_clock = clock();

    /* Print results depending on the test function return status. */
    switch (test_last_status) {
        case TEST_PASSED: {
            test_passed_tests++;
            printf(_TEST_COLOUR_PASS "\r[       PASS ]" _TEST_DIAGNOSTICS);
            break;
        } case TEST_SKIPPED: {
            test_skipped_tests++;
            printf(_TEST_COLOUR_SKIP "\r[       SKIP ]" _TEST_COLOUR_MUTE
                   " %s: %s" _TEST_COLOUR_RESET _TEST_NEWLINE, name,
                   test_failure_message);
            break;
        } case TEST_FAILED: {
            test_failed_tests++;
            printf(_TEST_NEWLINE);
            puts(test_failure_message);
            printf(_TEST_COLOUR_FAIL "[       FAIL ]" _TEST_DIAGNOSTICS);
            break;
        } default: {
            /* This branch should never run. */
            break;
        }
    }

    /* Test completed: tear down the test environment. */
    teardown_fn(test_data);
    free(test_data);
}

/**
 * Declare a test with the specified name, belonging to the specified fixture.
 * The fixture must have been declared with the <code>FIXTURE</code> directive
 * prior to the test.  For each test, the fixture setup function will be run
 * first, then the body of the test as specified after this directive, then
 * finally the fixture teardown function will be run to conclude the test.  All
 * tests should, but are not required to, contain at least one assertion, a
 * statement that contains one of the <code>ASSERT_*</code> directives.  Like
 * test setup and teardown functions, tests are declared with a syntax very
 * similar to standard C functions, as follows:
 *
 * @code{.c}
 * TEST(Example_test, Some_fixture) {
 *     ASSERT_NON_NULL(TEST->message);
 *     strcpy(TEST->message, "Hello!");
 *     ASSERT_STREQ(TEST->message, "Hello!");
 *     ASSERT_EQ(strlen(TEST->message), 6);
 * }
 * @endcode
 *
 * As shown above, the data members of a test's fixture are made available via
 * the pointer <code>TEST</code>, as in the fixture functions.  Furthermore,
 * note that tests that contain multiple assertions will be terminated
 * immediately once any assertion fails.  This directive must be written within
 * a <code>TEST_START</code>, <code>TEST_END</code> block.
 */
#define TEST(NAME, FIXTURE)                                                \
    /* Forward declare test function to allow standard function syntax. */ \
    static void                                                            \
    FIXTURE ## _ ## NAME ## _test(struct FIXTURE ## _fixture_data *);      \
    /* Actual test function: calls the user-defined test function. */      \
    static int _TEST_RUNNER FIXTURE ## _ ## NAME ## _test_run(void) {      \
        test_run(#NAME,                                                    \
        (test_fn_t) FIXTURE ## _fixture_setup,                             \
        (test_fn_t) FIXTURE ## _ ## NAME ## _test,                         \
        (test_fn_t) FIXTURE ## _fixture_teardown,                          \
        sizeof(struct FIXTURE ## _fixture_data));                          \
        return 0;                                                          \
    }                                                                      \
    /* Make the test function run on Windows. */                           \
    _TEST_EPILOGUE(FIXTURE ## _ ## NAME ## _test_run, _TEST_TEST_SECTION)  \
    /* And finally, the user-declared test function. */                    \
    static void FIXTURE ## _ ## NAME ## _test(                             \
        struct FIXTURE ## _fixture_data *TEST _TEST_UNUSED)

/** Shorthand for fixture data member access.  Save yourself some typing! */
#define T_ TEST ->

/**
 * Declare the common test function for a parameterised test with the specified
 * name, belonging to the specified fixture.  Unlike tests declared with the
 * <code>TEST</code> directive, this function will not be run simply as a result
 * of the <code>PTEST</code> directive, but must be followed by
 * <code>PCASE</code> directives that specify the parameters for each test to be
 * run.  These directives should be combined as follows:
 *
 * @code{.c}
 * #pragma FIXTURE_START
 *
 * FIXTURE(Param_fixture) {
 *     char *message;
 *     unsigned long length;
 * }
 *
 * #pragma FIXTURE_END
 *
 * #pragma TEST_START
 *
 * PTEST(String_length, Param_fixture) {
 *     ASSERT_EQ(strlen(T_ message), T_ length);
 * }
 *
 * PCASE(String_length, Param_fixture) { T_ message = "Hi!"; T_ length = 3; }
 * PCASE(String_length, Param_fixture) { T_ message = "Hello"; T_ length = 5; }
 * PCASE(String_length, Param_fixture) { T_ message = "Salve!"; T_ length = 6; }
 *
 * #pragma TEST_END
 * @endcode
 *
 * Note that the data for a parameterised test should be defined in a
 * corresponding fixture.  Note further that <code>PCASE</code> directives
 * should follow the corresponding <code>PTEST</code> directive, and that both
 * <code>PTEST</code> and <code>PCASE</code> directives must be inside a
 * <code>TEST_START</code>, <code>TEST_END</code> block.
 */
#define PTEST(NAME, FIXTURE)                                               \
    /* Simply allow the user to define the function, but do not run it. */ \
    static void FIXTURE ## _ ## NAME ## _test(                             \
            struct FIXTURE ## _fixture_data *TEST _TEST_UNUSED)

#define PCASE(NAME, FIXTURE)                                                  \
    /* Forward declare the case setup function. */                            \
    static void                                                               \
    _TEST_TOKEN_CONCAT(FIXTURE ## _ ## NAME ## _case_setup_, __LINE__)(       \
            struct FIXTURE ## _fixture_data *);                               \
    /* Declare the combined fixture/case setup function. */                   \
    static void                                                               \
    _TEST_TOKEN_CONCAT(FIXTURE ## _ ## NAME ## _case_setup_reg_, __LINE__)(   \
            struct FIXTURE ## _fixture_data *TEST) {                          \
        FIXTURE ## _fixture_setup(TEST);                                      \
        _TEST_TOKEN_CONCAT(FIXTURE ## _ ## NAME ## _case_setup_, __LINE__)    \
                (TEST);                                                       \
    }                                                                         \
    /* Test runner: calls the parameterised test with the combined setup. */  \
    static int _TEST_RUNNER                                                   \
    _TEST_TOKEN_CONCAT(FIXTURE ## _ ## NAME ## _test_run_, __LINE__)(void) {  \
        test_run(#NAME " (L" _TEST_LINE_STR ")",                              \
                 (test_fn_t) _TEST_TOKEN_CONCAT(                              \
                         FIXTURE ## _ ## NAME ## _case_setup_reg_, __LINE__), \
                 (test_fn_t) FIXTURE ## _ ## NAME ## _test,                   \
                 (test_fn_t) FIXTURE ## _fixture_teardown,                    \
                 sizeof(struct FIXTURE ## _fixture_data));                    \
        return 0;                                                             \
    }                                                                         \
    /* Make the test function run on Windows. */                              \
    _TEST_EPILOGUE(                                                           \
            _TEST_TOKEN_CONCAT(FIXTURE ## _ ## NAME ## _test_run_, __LINE__), \
            _TEST_TEST_SECTION)                                               \
    /* At last, the user-declared case setup function. */                     \
    static void                                                               \
    _TEST_TOKEN_CONCAT(FIXTURE ## _ ## NAME ## _case_setup_, __LINE__)(       \
            struct FIXTURE ## _fixture_data *TEST _TEST_UNUSED)

/**
 * Skip the test in whose body this directive appears and print a skipped status
 * and the specified message in the test status report if the specified
 * condition holds true.  Explanation messages are required for all skipped
 * tests in order to enforce proper testing discipline.  This directive may
 * appear anywhere within the body of a test; if the condition holds true, all
 * assertions from that point forth will not be run.  Furthermore, skipped tests
 * will not be counted toward the total test count printed in the test report.
 * Example:
 *
 * @code{.c}
 * TEST(Conditionally_skipped_test, Some_fixture) {
 *     SKIP_IF(time(NULL) == -1, "Time functions not available");
 *     // Code that requires time here...
 * }
 * @endcode
 */
#define SKIP_IF(CONDITION, MESSAGE)                \
    do {                                           \
        if (CONDITION) {                           \
            strcpy(test_failure_message, MESSAGE); \
            test_last_status = TEST_SKIPPED;       \
            return;                                \
        }                                          \
    } while (0)

/**
 * Unconditional version of the <code>SKIP_IF</code> directive.  Immediately
 * skips the remainder of the test in whose body the directive appears.
 * Example:
 *
 * @code{.c}
 * TEST(Skipped_test, Some_fixture) {
 *     SKIP("Bug #1: Faulty assertion");
 *     // Fix this!
 *     ASSERT_TRUE(0);
 * }
 * @endcode
 *
 * @see SKIP_IF
 */
#define SKIP(MESSAGE) SKIP_IF(1, MESSAGE)

/* ******************************* ASSERTIONS ******************************* */

/* Run an assertion comparing two values with the specified representations and
 * printf formats according to the specified comparator, printing an error
 * containing the specified message and terminating the test on failure. */
#define _TEST_ASSERT(A, A_REPR, A_FMT, CMP, B, B_REPR, B_FMT, MSG)           \
    do {                                                                     \
        if (!((A) CMP (B))) {                                                \
            sprintf(test_failure_message,                                    \
                    _TEST_COLOUR_FAIL "Assertion failed!" _TEST_COLOUR_RESET \
                    " " MSG _TEST_NEWLINE                                    \
                    _TEST_COLOUR_VALUE "    Value 1: " _TEST_COLOUR_RESET    \
                    A_FMT _TEST_NEWLINE                                      \
                    _TEST_COLOUR_VALUE "    Value 2: " _TEST_COLOUR_RESET    \
                    B_FMT _TEST_NEWLINE                                      \
                    _TEST_COLOUR_FAIL "File: " _TEST_COLOUR_RESET __FILE__   \
                    ":%u", (A_REPR), (B_REPR), __LINE__);                    \
            test_last_status = TEST_FAILED;                                  \
            return;                                                          \
        }                                                                    \
    } while (0)

/** Assert that the specified predicate evaluates to a truthy (non-0) value. */
#define ASSERT_TRUE(PREDICATE)                                             \
    _TEST_ASSERT(0, "TRUE (!= 0)", "%s", !=, PREDICATE, (PREDICATE), "%d", \
            "Expression is true: (" #PREDICATE ")")

/** Assert that the specified predicate evaluates to a falsy (0) value. */
#define ASSERT_FALSE(PREDICATE)                                          \
    _TEST_ASSERT(0, "FALSE (0)", "%s", ==, PREDICATE, (PREDICATE), "%d", \
            "Expression is false: (" #PREDICATE ")")

/** Assert that the specified pointer is null (0). */
#define ASSERT_NULL(PTR)                                         \
    _TEST_ASSERT(NULL, "NULL (0x0)", "%s", ==, PTR, (PTR), "%p", \
            "Pointer is non-null: (" #PTR ")")

/** Assert that the specified pointer is non-null (non-0). */
#define ASSERT_NON_NULL(PTR)                                            \
    _TEST_ASSERT(NULL, "Non-null (!= 0x0)", "%s", !=, PTR, (PTR), "%p", \
            "Pointer is null: (" #PTR ")")

/**
 * Assert that the first specified value is equal to the second specified value.
 * Values will be printed in the specified format.
 */
#define ASSERT_EQ(VALUE_1, VALUE_2, FORMAT)                                  \
    _TEST_ASSERT(VALUE_1, (VALUE_1), FORMAT, ==, VALUE_2, (VALUE_2), FORMAT, \
            "(" #VALUE_1 ") == (" #VALUE_2 ")")

/**
 * Assert that the first specified value is not equal to the second specified
 * value.  Values will be printed in the specified format.
 */
#define ASSERT_NE(VALUE_1, VALUE_2, FORMAT)                                  \
    _TEST_ASSERT(VALUE_1, (VALUE_1), FORMAT, !=, VALUE_2, (VALUE_2), FORMAT, \
            "(" #VALUE_1 ") != (" #VALUE_2 ")")

/**
 * Assert that the first specified value is greater than the second specified
 * value.  Values will be printed in the specified format.
 */
#define ASSERT_GT(VALUE_1, VALUE_2, FORMAT)                                 \
    _TEST_ASSERT(VALUE_1, (VALUE_1), FORMAT, >, VALUE_2, (VALUE_2), FORMAT, \
            "(" #VALUE_1 ") > (" #VALUE_2 ")")

/**
 * Assert that the first specified value is greater than or equal to the second
 * specified value.  Values will be printed in the specified format.
 */
#define ASSERT_GE(VALUE_1, VALUE_2, FORMAT)                                  \
    _TEST_ASSERT(VALUE_1, (VALUE_1), FORMAT, >=, VALUE_2, (VALUE_2), FORMAT, \
            "(" #VALUE_1 ") >= (" #VALUE_2 ")")

/**
 * Assert that the first specified value is less than the second specified
 * value.  Values will be printed in the specified format.
 */
#define ASSERT_LT(VALUE_1, VALUE_2, FORMAT)                                 \
    _TEST_ASSERT(VALUE_1, (VALUE_1), FORMAT, <, VALUE_2, (VALUE_2), FORMAT, \
            "(" #VALUE_1 ") < (" #VALUE_2 ")")

/**
 * Assert that the first specified value is less than or equal to the second
 * specified value.  Values will be printed in the specified format.
 */
#define ASSERT_LE(VALUE_1, VALUE_2, FORMAT)                                  \
    _TEST_ASSERT(VALUE_1, (VALUE_1), FORMAT, <=, VALUE_2, (VALUE_2), FORMAT, \
            "(" #VALUE_1 ") <= (" #VALUE_2 ")")

/**
 * Assert that the contents of the specified strings (<code>char *</code>) are
 * equal.  Values will be printed as strings.
 */
#define ASSERT_STREQ(STR_1, STR_2)                                            \
    _TEST_ASSERT(strcmp((STR_1), (STR_2)), (STR_1), "\"%s\"", ==, 0, (STR_2), \
            "\"%s\"", "(" #STR_1 ") == (" #STR_2 ")")

/**
 * Assert that the contents of the specified strings (<code>char *</code>) are
 * not equal.  Values will be printed as strings.
 */
#define ASSERT_STRNE(STR_1, STR_2)                                            \
    _TEST_ASSERT(strcmp((STR_1), (STR_2)), (STR_1), "\"%s\"", !=, 0, (STR_2), \
            "\"%s\"", "(" #STR_1 ") != (" #STR_2 ")")

/* ****************************** TEST RUNNER ******************************* */

#pragma FIXTURE_START

/**
 * Print a summary of the test suite, describing the number of tests passed,
 * failed, and skipped.
 */
static void test_summary(void) {
    puts(_TEST_NEWLINE _TEST_COLOUR_HEADER
         "================================= TEST SUMMARY ================================="
         _TEST_COLOUR_RESET);

    if (test_failed_tests == 0) {
        printf(_TEST_COLOUR_PASS "All %lu tests passed!" _TEST_COLOUR_RESET
               _TEST_NEWLINE, test_passed_tests);
    } else {
        printf(_TEST_COLOUR_PASS "Test(s) passed:" _TEST_COLOUR_RESET " %lu"
               _TEST_NEWLINE, test_passed_tests);
        printf(_TEST_COLOUR_FAIL "Test(s) failed:" _TEST_COLOUR_RESET " %lu"
                                 _TEST_NEWLINE, test_failed_tests);
    }

    if (test_skipped_tests > 0) {
        printf(_TEST_COLOUR_VALUE "Test(s) skipped:" _TEST_COLOUR_RESET " %lu"
               _TEST_NEWLINE, test_skipped_tests);
    }
}

/**
 * Print test environment information and headers before test suite functions
 * are run.  Run in the fixture override stage to ensure proper execution order.
 */
static int _TEST_FIXTURE_LIFECYCLE test_kickoff(void) {
    test_passed_tests = test_failed_tests = test_skipped_tests = 0;
    puts(_TEST_COLOUR_HEADER
         "================================ BEGIN TEST RUN ================================"
         _TEST_COLOUR_RESET);

    /* Register summary function to run after all tests have completed. */
    atexit(test_summary);
    return 0;
}
_TEST_EPILOGUE(test_kickoff, _TEST_FIXTURE_SECTION)

#pragma FIXTURE_END

#endif // TEST_H_INCLUDED

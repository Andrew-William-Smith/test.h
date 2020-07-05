/* TEST.H EXAMPLE TEST SUITE
 * This file contains an executable demonstration and explanation of all of the
 * primary features of test.h.  Using the patterns demonstrated herein, you
 * should be able to start writing your own tests almost immediately!  As
 * mentioned in the header file documentation, the only real "dependency"
 * required to use test.h is test.h itself, so without further ado... */

#include "test.h"

/* We'll also include string.h to provide us with some sample functions to test,
 * and stdlib.h so we can work with the heap.  Note that you don't have to
 * include these or any headers other than test.h to use the framework. */

#include <stdlib.h>
#include <string.h>

/* Unfortunately, test.h doesn't get rid of all boilerplate for you.  In order
 * to execute a test suite, you'll have to declare a main function, although it
 * doesn't have to actually do anything.  For the purposes of this test suite,
 * we'll leave it empty. */

int main() {}

/* Now that we've finished our preamble, we're ready to start writing some
 * tests.  However, before we write a test, we have to declare a fixture.  A
 * fixture is a collection of tests that all have common requirements in order
 * for them to run, such as shared setup or teardown code to e.g. initialise a
 * data structure or allocate memory.  For now, we'll stick with the simplest
 * possible fixture, a completely empty fixture with no special code associated
 * with it. */

FIXTURE(Simple_fixture) EMPTY;

/* In the code above, we've declared a fixture called Simple_fixture, followed
 * by a modifier to designate that there is no additional data associated with
 * the fixture.  Note that all test fixtures must be ended with a semicolon,
 * similar to struct declarations in more familiar C code.
 *
 * Now that we have a fixture, let's write our first test!  This is essentially
 * nothing but a sanity test for the library and does not perform any useful
 * tasks, but it demonstrates the syntax of test declaration and assertions. */

TEST(Assert_true_succeeds, Simple_fixture) {
    ASSERT_TRUE(1);
}

/* Let's break this test down piece by piece.  We've declared a test called
 * "Assert_true_succeeds", in the fixture that we previously declared,
 * Simple_fixture.  Note that test fixtures must be declared before the tests
 * contained within them.  You'll notice that after the TEST directive, the rest
 * of the test looks like a normal C function, and in fact it is a normal
 * function!  This test contains a single assertion, or a statement that, if its
 * condition is not met, will cause the test to fail.  In this case, we assert
 * that the value 1 is truthy, which is trivially true.  As a result, this test
 * succeeds!  Note that tests are not required to contain assertions, but it is
 * strongly recommended that you include one assertion in each of your tests for
 * proper test hygiene.  A test without any assertions will succeed by default.
 *
 * Sometimes, you may wish to skip a test that is failing due to a known bug or
 * platform incompatibility.  test.h provides the SKIP directive for this case,
 * which allows you to prevent a test from executing and prints a skipped status
 * message in the test report. */

TEST(Skipped_test_does_not_run, Simple_fixture) {
    SKIP("This test is skipped for demonstration purposes.");
    // Fix this!
    ASSERT_TRUE(0);
}

/* The message you write as the argument to the SKIP directive is very
 * important, as it may help you and your collaborators to track the history of
 * your test suite: think of it like a commit message for your tests, the more
 * descriptive the better!  Note that the above test is unconditionally skipped,
 * a function which is often useful when you are trying to track down bugs that
 * cause test failures; however, sometimes you may wish to conditionally skip
 * tests as well due to different testing configurations or other differences
 * between testing environments.  For this situation, test.h provides the
 * SKIP_IF directive, which will only skip a test if the condition specified in
 * the directive evaluates to true. */

TEST(Conditionally_skipped_test, Simple_fixture) {
    SKIP_IF(0, "This skip directive will not run.");
    ASSERT_TRUE(1);
    SKIP_IF(1, "But this one will!");
    ASSERT_TRUE(0);
}

/* Now that we've demonstrated how easily fixtures and tests can be declared,
 * let's move on to a more complicated example.  The fixture Simple_fixture was
 * declared as EMPTY, meaning that it had no additional data associated with it;
 * however, it is often useful to declare some variables that can be shared
 * amongst the tests in a fixture.  Fixture data members can be indicated by
 * following the FIXTURE directive with a struct-style structure declaration, as
 * shown below. */

FIXTURE(String_fixture) {
    char *str;
    size_t length;
};

/* Here, we've omitted the EMPTY modifier in favour of two variables, describing
 * a string and its length.  Now that we've declared these variables, we can
 * access them in any test that belongs to String_fixture by way of the pointer
 * TEST, which is available in all tests.  In truth, the reason that fixture
 * declarations look so much like structs is that they *are* just structs,
 * albeit ones with some additional macro overhead.  TEST is simply a pointer to
 * the struct defined in the declaration of the fixture to which the test
 * belongs, allowing full access to the fixture's data members as in the
 * following test. */

TEST(strlen_returns_correct_length, String_fixture) {
    // Assign some values to our data members
    TEST->str = "Hello!";
    TEST->length = 6;
    // Run some simple assertions just to verify that things are working
    ASSERT_NON_NULL(TEST->str);
    ASSERT_STREQ(TEST->str, "Hello!");
    ASSERT_EQ(strlen(TEST->str), TEST->length);
}

/* In this test, you can see that the data members "str" and "length" from the
 * fixture String_fixture are available inside the test body by way of the
 * pointer TEST.  Note that a new instance of the fixture struct is created for
 * each test, so it is not possible to share data between tests without
 * declaring the shared data outside of a test yourself.  This is for your own
 * good, trust me.
 *
 * Furthermore, we demonstrate three more assertions provided by test.h,
 * ASSERT_NON_NULL, ASSERT_STREQ, and ASSERT_EQ.  ASSERT_NON_NULL simply checks
 * that a specified pointer has a value other than NULL (0), while ASSERT_STREQ
 * checks that the contents of the two specified strings (char pointers) are
 * identical.  The final assertion, ASSERT_EQ, checks that the specified values
 * are equal, and if they are not, it displays a pretty-printed representation
 * of both values courtesy of C11 generics.  All comparison-type assertions have
 * this feature, so you should always get human-readable output regardless of
 * the types you are using in your tests, even if the type is not formally
 * recognised by test.h.
 *
 * At this point, you may be wondering why we bothered declaring data members on
 * fixture String_fixture at all, since we're not using them to do anything that
 * can't be done with normal local variables.  In this assertion you'd be
 * correct (in other words, your test is still running), but allow me to
 * immediately contradict you with one of the most powerful features of any unit
 * testing framework, and one for which test.h has full support: parameterised
 * tests.  A parameterised test is a test that runs the exact same test code on
 * different input values, or "parameters", allowing you to vastly reduce the
 * amount of duplicated code in your test suites.  For example, let's say that
 * we wanted to perform a test very similar to the previous one, but on a larger
 * set of strings and lengths.  We can very easily do this by way of the PTEST
 * directive, as demonstrated below. */

PTEST(strlen_correct_length_parameterised, String_fixture,
      { T_ str = ""; T_ length = 0; },
      { T_ str = "Hello!"; T_ length = 6; },
      { T_ str = "Parameterised testing is awesome!"; T_ length = 33; },
      { T_ str = "One more parameter set"; T_ length = 22; })
{
    ASSERT_NON_NULL(T_ str);
    ASSERT_EQ(strlen(T_ str), T_ length);
}

/* As you can see, a parameterised test is declared very similarly to a normal
 * test, with the only difference being that the fixture name in the PTEST
 * directive is followed by a number of code blocks, separated by commas.  These
 * code blocks are where you can declare your test's parameters, with one block
 * per test to run.  They are run after the fixture's setup function, so any
 * allocated memory or initialised structures you created in your setup function
 * will be available in the parameter blocks.  Note that you can write any code
 * in these blocks, not just member modifications, allowing more complex control
 * over the test environment if necessary.
 *
 * Also, you may be wondering what on Earth the T_ symbol is.  "T_" is simply a
 * shorthand for "TEST->", provided to reduce the amount of typing involved in
 * referencing fixture data members.  Handy, eh?
 *
 * Now let's talk about one final feature of test.h, one that I've been hand-
 * waving off heretofore: fixture setup and teardown functions.  Previously, I
 * stated that by default, fixtures have no such functions; this is not entirely
 * true, as the functions are generated, but they are empty by default.  We can
 * use two directives to override these useless setup and teardown functions,
 * FIXTURE_SETUP and FIXTURE_TEARDOWN, respectively.  The setup function will be
 * run before the parameter initialisation (if applicable) and body of each test
 * in the fixture, while the teardown function will be run after the test has
 * finished, regardless of its status.  If you want to specify custom functions
 * for a fixture, you should do so immediately after the fixture declaration in
 * order to avoid unexpected behaviour.  To demonstrate this, we shall declare a
 * new fixture with custom lifecycle functions below. */

FIXTURE(Custom_lifecycle_fixture) {
    char *str;
};

FIXTURE_SETUP(Custom_lifecycle_fixture) {
    T_ str = malloc(1024 * sizeof(char));
}

FIXTURE_TEARDOWN(Custom_lifecycle_fixture) {
    free(T_ str);
}

/* With these function definitions, all tests in fixture
 * Custom_lifecycle_fixture will now have a 1024-byte string buffer available by
 * default that will be automatically freed once the test completes to avoid
 * memory leaks.  Let's test this out! */

TEST(Copy_to_dynamic_string, Custom_lifecycle_fixture) {
    ASSERT_NON_NULL(T_ str);
    strcpy(T_ str, "Custom test lifecycles rock!");
    ASSERT_STREQ(T_ str, "Custom test lifecycles rock!");
    ASSERT_EQ(strlen(T_ str), 28);
}

/* Assuming that the memory allocation succeeded (it's always good to check!),
 * the above test should have passed, demonstrating that data was copied to the
 * malloc-ed region of memory.
 *
 * You now know pretty much everything there is to know about testing your code
 * with test.h!  To round out the series of examples, test.h provides a few more
 * assertions that you may find useful in testing your code, demonstrated in the
 * following example.  This example should also let you see what it looks like
 * when a test fails, as demonstrated by the trivially false assertion at the
 * end. */

TEST(All_assertions, Simple_fixture) {
    // A == B
    ASSERT_EQ(437, 437);
    // A != B
    ASSERT_NE(42, 437);
    // A < B
    ASSERT_LT(42, 437);
    // A <= B
    ASSERT_LE(437, 437);
    // A > B
    ASSERT_GT(437, 42);
    // A >= B
    ASSERT_GE(437, 437);

    // strcmp(A, B) == 0
    ASSERT_STREQ("Hello!", "Hello!");

    // A == NULL
    ASSERT_NULL(NULL);
    // A != NULL
    ASSERT_NON_NULL(TEST);

    // Expression is true
    ASSERT_TRUE(1);
    // Expression is false...or is it?
    ASSERT_FALSE(1);
}

/* Finally, now it's time to show off a bit.  In order to prevent errant tests
 * from corrupting memory or crashing the test suite, each test is run in its
 * own process, fully isolated from the main test suite process.  Thus, if a
 * test were to, say, throw a segmentation fault, we can catch the SIGSEGV
 * thrown and report it in the test results.  Hopefully this feature will save
 * you some time spent mucking about in GDB. */

TEST(Segfault_does_not_crash, Simple_fixture) {
    int a = *((volatile int *) NULL);
    (void) a;
}

/* And that's it!  You're now fully ready to test your code using test.h, making
 * use of all of its features to make your testing life easier.  As a final
 * note, a transcript of a run of this file as printed to the console is given
 * below; normally, this transcript would be written to your terminal in colour,
 * but given that this file is plain text, I suppose a monochrome version will
 * have to do.
 *
 * ================================ BEGIN TEST RUN ================================
 * [PASS] (  0.000/  0s) Assert_true_succeeds
 * [SKIP] Skipped_test_does_not_run
 * | This test is skipped for demonstration purposes.
 * [SKIP] Conditionally_skipped_test
 * | But this one will!
 * [PASS] (  0.000/  0s) strlen_returns_correct_length
 * [PASS] (  0.000/  0s) strlen_correct_length_parameterised { TEST -> str = ""; TEST -> length = 0; }
 * [PASS] (  0.000/  0s) strlen_correct_length_parameterised { TEST -> str = "Hello!"; TEST -> length = 6; }
 * [PASS] (  0.000/  0s) strlen_correct_length_parameterised { TEST -> str = "Parameterised testing is awesome!"; TEST -> length = 33; }
 * [PASS] (  0.000/  0s) strlen_correct_length_parameterised { TEST -> str = "One more parameter set"; TEST -> length = 22; }
 * [PASS] (  0.000/  0s) Copy_to_dynamic_string
 * [FAIL] (  0.000/  0s) All_assertions
 * | Assertion failed at example_test.c:249
 * | Expression is true (false expected): 1
 * [HALT] (  0.000/  0s) Segfault_does_not_crash
 * | Test halted due to signal sigsegv (code 11)
 *
 * ================================= TEST SUMMARY =================================
 * 2 test(s) failed!
 * Total tests: 9
 *
 * Now, enough reading, go forth and test your code! */

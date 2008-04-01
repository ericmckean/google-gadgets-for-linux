/*
  Copyright 2008, Google Inc.
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are
  met:

      * Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.
      * Redistributions in binary form must reproduce the above
  copyright notice, this list of conditions and the following disclaimer
  in the documentation and/or other materials provided with the
  distribution.
      * Neither the name of Google Inc. nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

// gTest -- the Google C++ Unit Testing Framework
//
// This header file defines the public API for gTest.  It should be
// included by any test program that uses gTest.
//
// IMPORTANT NOTE: Due to limitation of the C++ language, we have to
// leave some internal implementation details in this header file.
// They are clearly marked by comments like this:
//
//   // INTERNAL IMPLEMENTATION - DO NOT USE IN A USER PROGRAM.
//
// Such code is NOT meant to be used by a user directly, and is subject
// to CHANGE WITHOUT NOTICE.  Therefore DO NOT DEPEND ON IT in a user
// program!
//
// Acknowledgment: gTest borrowed the idea of automatic test
// registration from Barthelemy Dagenais' (barthelemy@prologique.com)
// easyUnit framework.

#ifndef UNITTEST_GTEST_H__
#define UNITTEST_GTEST_H__

// The following platform macros are used throughout gTest:
//   OS_LINUX        Linux          (set by our build tools)
//   __APPLE__       OS X           (set by Xcode)
//   _MSC_VER        Any Windows    (set by MSVC)
//   _WIN32_WCE      Windows CE     (set in project files)
//   __SYMBIAN32__   Symbian        (set by Symbian tool chain)
//
// Note that even though _MSC_VER and _WIN32_WCE really indicate a compiler
// and a Win32 implementation, respectively, we use them to indicate the
// combination of compiler - Win 32 API - C library, since the code currently
// only supports:
// Windows proper with Visual C++ and MS C library (_MSC_VER && !_WIN32_WCE) and
// Windows Mobile with Visual C++ and no C library (_WIN32_WCE).

#include "gtest_internal.h"  // NOLINT
#include "gtest_prod.h"  // NOLINT

// Depending on the platform, different string classes are available.
// On Windows, ::std::string compiles only when exceptions are
// enabled.  On Linux, in addition to ::std::string, Google also makes
// use of class ::string, which has the same interface as
// ::std::string, but has a different implementation.
//
// The user can tell us whether ::std::string is available in his
// environment by defining the macro GTEST_HAS_STD_STRING to either 1
// or 0 on the compiler command line.  He can also define
// GTEST_HAS_GLOBAL_STRING to 1 to indicate that ::string is available
// AND is a distinct type to ::std::string, or define it to 0 to
// indicate otherwise.
//
// If the user's ::std::string and ::string are the same class due to
// aliasing, he should define GTEST_HAS_STD_STRING to 1 and
// GTEST_HAS_GLOBAL_STRING to 0.
//
// If the user doesn't define GTEST_HAS_STD_STRING and/or
// GTEST_HAS_GLOBAL_STRING, they are defined heuristically.

namespace testing {

// The possible outcomes of a test part (i.e. an assertion or an
// explicit SUCCEED(), FAIL(), or ADD_FAILURE()).
enum TestPartResultType {
  TPRT_SUCCESS,           // Succeeded.
  TPRT_NONFATAL_FAILURE,  // Failed but the test can continue.
  TPRT_FATAL_FAILURE,     // Failed and the test should be terminated.
};

// The Message class works like an ostream repeater.
//
// Typical usage:
//
//   1. You stream a bunch of values to a Message object.
//      It will remember the text in a StrStream.
//   2. Then you stream the Message object to an ostream.
//      This causes the text in the Message to be streamed
//      to the ostream.
//
// For example;
//
//   testing::Message foo;
//   foo << 1 << " != " << 2;
//   std::cout << foo;
//
// will print "1 != 2".
//
// Note that StrStream behaves differently in gcc and in MSVC.  You
// can stream a NULL char pointer to it in the former, but not in the
// latter (it causes an access violation if you do).  The Message
// class hides this difference by treating a NULL char pointer as
// "(null)".
//
// In gTest, we mainly use Message in *_M macros to allow
// easy construction of failure message, e.g.
//
//    FAIL_M(testing::Message() << "We shouldn't see " << foo << " here.");
//    ASSERT_TRUE_M(a && b, testing::Message() << a << " and " << b);
class Message {
 public:
  // The type of basic IO manipulators (endl, ends, and flush) for
  // narrow streams.
  //
  // INTERNAL IMPLEMENTATION - DO NOT USE IN A USER PROGRAM.
  typedef std::ostream& (*BasicNarrowIoManip)(std::ostream&);

  // Constructs an empty Message.
  // We allocate the StrStream separately because it otherwise each use of
  // ASSERT/EXPECT in a procedure adds over 200 bytes to the procedure's
  // stack frame leading to huge stack frames in some cases; gcc does not reuse
  // the stack space.
  Message() : ss_(new StrStream) {}

  // Copy constructor.
  Message(const Message& msg) : ss_(new StrStream) { *ss_ << msg.GetString(); }

  // Constructs a Message from a C-string.
  explicit Message(const char* str) : ss_(new StrStream) { *ss_ << str; }

  ~Message() { delete ss_; }

#ifdef __SYMBIAN32__
// These are needed as the Nokia Symbian Compiler cannot decide between
// const T& and const T* in a function template. The Nokia compiler _can_
// decide between class template specializations for T and T*, so a 
// tr1::type_traits-like is_pointer works, and we can overload on that.

  template <typename T>
  inline void StreamHelper(internal::true_type dummy, T* pointer) {
    if (pointer == NULL) {
      *ss_ << "(null)";
    } else {
      ::GTestStreamToHelper(ss_, pointer);
    }
  }

  template <typename T>
  inline void StreamHelper(internal::false_type dummy, const T& value) {
    ::GTestStreamToHelper(ss_, value);
  }

  // Streams a non-pointer value to this object.
  template <typename T>
  inline Message& operator <<(const T& value) {
    StreamHelper(typename internal::is_pointer<T>::type(), value);
    return *this;
  }

#else

  // Streams a non-pointer value to this object.
  template <typename T>
  inline Message& operator <<(const T& val) {
    ::GTestStreamToHelper(ss_, val);
    return *this;
  }

  // Streams a pointer value to this object.
  //
  // This function is an overload of the previous one.  When you
  // stream a pointer to a Message, this definition will be used as it
  // is more specialized.  (The C++ Standard, section
  // [temp.func.order].)  If you stream a non-pointer, then the
  // previous definition will be used.
  //
  // The reason for this overload is that streaming a NULL pointer to
  // ostream is undefined behavior.  Depending on the compiler, you
  // may get "0", "(nil)", "(null)", or an access violation.  To
  // ensure consistent result across compilers, we always treat NULL
  // as "(null)".
  template <typename T>
  inline Message& operator <<(T* const & pointer) {
    if (pointer == NULL) {
      *ss_ << "(null)";
    } else {
      ::GTestStreamToHelper(ss_, pointer);
    }
    return *this;
  }

#endif  // __SYMBIAN32__

  // Since the basic IO manipulators are overloaded for both narrow
  // and wide streams, we have to provide this specialized definition
  // of operator <<, even though its body is the same as the
  // templatized version above.  Without this definition, streaming
  // endl or other basic IO manipulators to Message will confuse the
  // compiler.
  Message& operator <<(BasicNarrowIoManip val) {
    *ss_ << val;
    return *this;
  }

  // Instead of 1/0, we want to see true/false for bool values.
  Message& operator <<(bool b) {
    return *this << (b ? "true" : "false");
  }

  // These two overloads allow streaming a wide C string to a Message
  // using the UTF-8 encoding.
  Message& operator <<(const wchar_t* wide_c_str) {
    return *this << String::ShowWideCString(wide_c_str);
  }
  Message& operator <<(wchar_t* wide_c_str) {
    return *this << String::ShowWideCString(wide_c_str);
  }

#if GTEST_HAS_STD_STRING
  // Converts the given wide string to a narrow string using the UTF-8
  // encoding, and streams the result to this Message object.
  Message& operator <<(const ::std::wstring& wstr);
#endif  // GTEST_HAS_STD_STRING

#if GTEST_HAS_GLOBAL_STRING
  // Converts the given wide string to a narrow string using the UTF-8
  // encoding, and streams the result to this Message object.
  Message& operator <<(const ::wstring& wstr);
#endif  // GTEST_HAS_GLOBAL_STRING

  // Gets the text streamed to this object so far as a String.
  // Each '\0' character in the buffer is replaced with "\\0".
  //
  // INTERNAL IMPLEMENTATION - DO NOT USE IN A USER PROGRAM.
  String GetString() const {
    return StrStreamToString(ss_);
  }

 private:
  StrStream* const ss_;  // We'll hold the text streamed to this object here.

  // We declare (but don't implement) this to prevent the compiler
  // from implementing the assignment operator.
  void operator=(const Message&);
};

// Streams a Message to an ostream.
inline std::ostream& operator <<(std::ostream& os, const Message& sb) {
  return os << sb.GetString();
}

// Converts a streamable value to a String.  A NULL pointer is
// converted to "(null)".  When the input value is a ::string,
// ::std::string, ::wstring, or ::std::wstring object, each NUL
// character in it is replaced with "\\0".
// Declared in gtest-internal.h but defined here, so that it has access
// to the definition of the Message class, required by the ARM
// compiler.
template <typename T>
String StreamableToString(const T& streamable) {
  return (Message() << streamable).GetString();
}

// A class for indicating whether an assertion was successful.  When
// the assertion wasn't successful, the AssertionResult object
// remembers a non-empty message that described how it failed.
//
// This class is useful for defining predicate-format functions to be
// used with predicate assertions (ASSERT_PRED_FORMAT*, etc).
//
// The constructor of AssertionResult is private.  To create an
// instance of this class, use one of the factory functions
// (AssertionSuccess() and AssertionFailure()).
//
// For example, in order to be able to write:
//
//   // Verifies that Foo() returns an even number.
//   EXPECT_PRED_FORMAT1(IsEven, Foo());
//
// you just need to define:
//
//   testing::AssertionResult IsEven(const char* expr, int n) {
//     if ((n % 2) == 0) return testing::AssertionSuccess();
//
//     Message msg;
//     msg << "Expected: " << expr << " is even\n"
//         << "  Actual: it's " << n;
//     return testing::AssertionFailure(msg);
//   }
//
// If Foo() returns 5, you will see the following message:
//
//   Expected: Foo() is even
//     Actual: it's 5
class AssertionResult {
 public:
  // Declares factory functions for making successful and failed
  // assertion results as friends.
  friend AssertionResult AssertionSuccess();
  friend AssertionResult AssertionFailure(const Message&);

  // Returns true iff the assertion succeeded.
  operator bool() const { return failure_message_.c_str() == NULL; }  // NOLINT

  // Returns the assertion's failure message.
  const char* failure_message() const { return failure_message_.c_str(); }

 private:
  // The default constructor.  It is used when the assertion succeeded.
  AssertionResult() {}

  // The constructor used when the assertion failed.
  explicit AssertionResult(const String& failure_message);

  // Stores the assertion's failure message.
  String failure_message_;
};

// Makes a successful assertion result.
AssertionResult AssertionSuccess();

// Makes a failed assertion result with the given failure message.
AssertionResult AssertionFailure(const Message& msg);

// The abstract class that all tests inherit from.
//
// In gTest, a unit test program contains one or many TestCases, and
// each TestCase contains one or many Tests.
//
// When you define a test using the TEST macro, you don't need to
// explicitly derive from Test - the TEST macro automatically does
// this for you.
//
// The only time you derive from Test is when defining a test fixture
// to be used a TEST_F.  For example:
//
//   class FooTest : public testing::Test {
//    protected:
//     virtual void SetUp() { ... }
//     virtual void TearDown() { ... }
//     ...
//   };
//
//   TEST_F(FooTest, Bar) { ... }
//   TEST_F(FooTest, Baz) { ... }
//
// Test is not copyable.
class Test {
 public:
  friend class TestInfoImpl;

  // Defines types for pointers to functions that set up and tear down
  // a test case.
  typedef void (*SetUpTestCaseFunc)();
  typedef void (*TearDownTestCaseFunc)();

  // The d'tor is virtual as we intend to inherit from Test.
  virtual ~Test();

  // Returns true iff the current test has a fatal failure.
  static bool HasFatalFailure();

  // Logs a property for the current test.  Only the last value for a given
  // key is remembered.
  // These are public static so they can be called from utility functions
  // that are not members of the test fixture.
  // The arguments are const char* instead strings, as gTest is used
  // on platforms where string doesn't compile.
  //
  // Note that a driving consideration for these RecordProperty methods
  // was to produce xml output suited to the Greenspan charting utility,
  // which at present will only chart values that fit in a 32-bit int. It
  // is the user's responsibility to restrict their values to 32-bit ints
  // if they intend them to be used with Greenspan.
  static void RecordProperty(const char* key, const char* value);
  static void RecordProperty(const char* key, int value);

 protected:
  // Creates a Test object.
  Test();

  // Sets up the stuff shared by all tests in this test case.
  //
  // gTest will call Foo::SetUpTestCase() before running the first
  // test in test case Foo.  Hence a sub-class can define its own
  // SetUpTestCase() method to shadow the one defined in the super
  // class.
  static void SetUpTestCase() {}

  // Tears down the stuff shared by all tests in this test case.
  //
  // gTest will call Foo::TearDownTestCase() after running the last
  // test in test case Foo.  Hence a sub-class can define its own
  // TearDownTestCase() method to shadow the one defined in the super
  // class.
  static void TearDownTestCase() {}

  // Sets up the test fixture.
  virtual void SetUp();

  // Tears down the test fixture.
  virtual void TearDown();

 private:
  // Returns true iff the current test has the same fixture class as
  // the first test in the current test case.
  static bool HasSameFixtureClass();

  // Runs the test after the test fixture has been set up.
  //
  // A sub-class must implement this to define the test logic.
  //
  // DO NOT OVERRIDE THIS FUNCTION DIRECTLY IN A USER PROGRAM.
  // Instead, use the TEST or TEST_F macro.
  virtual void TestBody() = 0;

  // Sets up, executes, and tears down the test.
  void Run();

  // Uses GTestFlagSaver, which knows how to save and
  // restore all gTest flags.
  const class GTestFlagSaver* const gtest_flag_saver_;

  // Often a user mis-spells SetUp() as Setup() and spends a long time
  // wondering why it is never called by gTest.  The declaration of
  // the following method is solely for catching such an error at
  // compile time:
  //
  //   - The return type is deliberately chosen to be not void, so it
  //   will be a conflict if a user declares void Setup() in his test
  //   fixture.
  //
  //   - This method is private, so it will be another compiler error
  //   if a user calls it from his test fixture.
  //
  // DO NOT OVERRIDE THIS FUNCTION.
  //
  // If you see an error about overriding the following function or
  // about it being private, you have mis-spelled SetUp() as Setup().
  struct Setup_should_be_spelled_SetUp {};
  virtual Setup_should_be_spelled_SetUp* Setup() { return NULL; }

  // We disallow copying Tests.
  GTEST_DISALLOW_EVIL_CONSTRUCTORS(Test);
};


// Defines the type of a function pointer that creates a Test object
// when invoked.
typedef Test* (*TestMaker)();


// A TestInfo object stores the following information about a test:
//
//   Test case name
//   Test name
//   Whether the test should be run
//   A function pointer that creates the test object when invoked
//   Test result
//
// The constructor of TestInfo registers itself with the UnitTest
// singleton such that the RUN_ALL_TESTS() macro knows which tests to
// run.
class TestInfo {
 public:
  // Destructs a TestInfo object.  This function is not virtual, so
  // don't inherit from TestInfo.
  ~TestInfo();

  // Creates a TestInfo object and registers it with the UnitTest
  // singleton; returns the created object.
  //
  // Arguments:
  //
  //   test_case_name:   name of the test case
  //   name:             name of the test
  //   fixture_class_id: ID of the test fixture class
  //   set_up_tc:        pointer to the function that sets up the test case
  //   tear_down_tc:     pointer to the function that tears down the test case
  //   maker:            pointer to the function that creates a test object
  //
  // This is public only because it's needed by the TEST and TEST_F macros.
  // INTERNAL IMPLEMENTATION - DO NOT USE IN A USER PROGRAM.
  static TestInfo* MakeAndRegisterInstance(
      const char* test_case_name,
      const char* name,
      TypeId fixture_class_id,
      Test::SetUpTestCaseFunc set_up_tc,
      Test::TearDownTestCaseFunc tear_down_tc,
      TestMaker maker);

  // Returns the test case name.
  const char* test_case_name() const;

  // Returns the test name.
  const char* name() const;

  // Returns true if this test should run.
  //
  // gTest allows the user to filter the tests by their full names.
  // The full name of a test Bar in test case Foo is defined as
  // "Foo.Bar".  Only the tests that match the filter will run.
  //
  // A filter is a colon-separated list of glob (not regex) patterns,
  // optionally followed by a '-' and a colon-separated list of
  // negative patterns (tests to exclude).  A test is run if it
  // matches one of the positive patterns and does not match any of
  // the negative patterns.
  //
  // For example, *A*:Foo.* is a filter that matches any string that
  // contains the character 'A' or starts with "Foo.".
  bool should_run() const;

  // Returns the result of the test.
  const TestResult* result() const;
 private:
  friend class DefaultDeathTestFactory;
  friend class Test;
  friend class TestCase;
  friend class TestInfoImpl;
  friend class UnitTestImpl;

  // Increments the number of death tests encountered in this test so
  // far.
  int increment_death_test_count();

  // Accessors for the implementation object.
  TestInfoImpl* impl() { return impl_; }
  const TestInfoImpl* impl() const { return impl_; }

  // Constructs a TestInfo object.
  TestInfo(const char* test_case_name, const char* name,
           TypeId fixture_class_id, TestMaker maker);

  // An opaque implementation object.
  TestInfoImpl* impl_;

  GTEST_DISALLOW_EVIL_CONSTRUCTORS(TestInfo);
};

// An Environment object is capable of setting up and tearing down an
// environment.  The user should subclass this to define his own
// environment(s).
//
// An Environment object does the set-up and tear-down in virtual
// methods SetUp() and TearDown() instead of the constructor and the
// destructor, as:
//
//   1. You cannot safely throw from a destructor.  This is a problem
//      as in some cases gTest is used where exceptions are enabled, and
//      we may want to implement ASSERT_* using exceptions where they are
//      available.
//   2. You cannot use ASSERT_* directly in a constructor or
//      destructor.
class Environment {
 public:
  // The d'tor is virtual as we need to subclass Environment.
  virtual ~Environment() {}

  // Override this to define how to set up the environment.
  virtual void SetUp() {}

  // Override this to define how to tear down the environment.
  virtual void TearDown() {}
 private:
  // If you see an error about overriding the following function or
  // about it being private, you have mis-spelled SetUp() as Setup().
  struct Setup_should_be_spelled_SetUp {};
  virtual Setup_should_be_spelled_SetUp* Setup() { return NULL; }
};

// A UnitTest consists of a list of TestCases.
//
// This is a singleton class.  The only instance of UnitTest is
// created when UnitTest::GetInstance() is first called.  This
// instance is never deleted.
//
// UnitTest is not copyable.
//
// This class is thread-safe as long as the methods are called
// according to their specification.
class UnitTest {
 public:
  // Gets the singleton UnitTest object.  The first time this method
  // is called, a UnitTest object is constructed and returned.
  // Consecutive calls will return the same object.
  static UnitTest* GetInstance();

  // Registers and returns a global test environment.  When a test
  // program is run, all global test environments will be set-up in
  // the order they were registered.  After all tests in the program
  // have finished, all global test environments will be torn-down in
  // the *reverse* order they were registered.
  //
  // The UnitTest object takes ownership of the given environment.
  //
  // This method can only be called from the main thread.
  Environment* AddEnvironment(Environment* env);

  // Adds a TestPartResult to the current TestResult object.  All
  // gTest assertion macros (e.g. ASSERT_TRUE, EXPECT_EQ, etc)
  // eventually call this to report their results.  The user code
  // should use the assertion macros instead of calling this directly.
  //
  // INTERNAL IMPLEMENTATION - DO NOT USE IN A USER PROGRAM.
  void AddTestPartResult(TestPartResultType result_type,
                         const char* file_name,
                         int line_number,
                         const String& message,
                         const String& os_stack_trace);

  // Adds a TestProperty to the current TestResult object. If the result already
  // contains a property with the same key, the value will be updated.
  void RecordPropertyForCurrentTest(const char* key, const char* value);

  // Runs all tests in this UnitTest object and prints the result.
  // Returns 0 if successful, or 1 otherwise.
  //
  // This method can only be called from the main thread.
  //
  // INTERNAL IMPLEMENTATION - DO NOT USE IN A USER PROGRAM.
  int Run() GTEST_MUST_USE_RESULT;

  // Returns the TestCase object for the test that's currently running,
  // or NULL if no test is running.
  const TestCase* current_test_case() const;

  // Returns the TestInfo object for the test that's currently running,
  // or NULL if no test is running.
  const TestInfo* current_test_info() const;

  // Accessors for the implementation object.
  UnitTestImpl* impl() { return impl_; }
  const UnitTestImpl* impl() const { return impl_; }
 private:
  // ScopedTrace is a friend as it needs to modify the per-thread
  // trace stack, which is a private member of UnitTest.
  friend class ScopedTrace;

  // Creates an empty UnitTest.
  UnitTest();

  // D'tor
  virtual ~UnitTest();

  // Pushes a trace defined by SCOPED_TRACE() on to the per-thread
  // gTest trace stack.
  void PushGTestTrace(const TraceInfo& trace);

  // Pops a trace from the per-thread gTest trace stack.
  void PopGTestTrace();

  // Opaque implementation object.  This field is never changed once
  // the object is constructed.  We don't mark it as const here, as
  // doing so will cause a warning in the constructor of UnitTest.
  // Mutable state in *impl_ is protected by mutex_.
  UnitTestImpl* impl_;

  // We disallow copying UnitTest.
  GTEST_DISALLOW_EVIL_CONSTRUCTORS(UnitTest);
};

// A convenient wrapper for adding an environment for the test
// program.
//
// You should call this before RUN_ALL_TESTS() is called, probably in
// main().  If you use gtest_main, you need to call this before main()
// starts for it to take effect.  For example, you can define a global
// variable like this:
//
//   testing::Environment* const foo_env =
//       testing::AddGlobalTestEnvironment(new FooEnvironment);
//
// However, we strongly recommend you to write your own main() and
// call AddGlobalTestEnvironment() there, as relying on initialization
// of global variables makes the code harder to read and may cause
// problems when you register multiple environments from different
// translation units and the environments have dependencies among them
// (remember that the compiler doesn't guarantee the order in which
// global variables from different translation units are initialized).
inline Environment* AddGlobalTestEnvironment(Environment* env) {
  return UnitTest::GetInstance()->AddEnvironment(env);
}

// Parses a command line for the flags that gTest recognizes.
// Whenever a gTest flag is seen, it is removed from argv, and *argc
// is decremented.
//
// No value is returned.  Instead, the FLAGS_gtest_* variables are
// updated.
void ParseGTestFlags(int* argc, char** argv);

// This overloaded version can be used in Windows programs compiled in
// UNICODE mode.
#ifdef _MSC_VER
void ParseGTestFlags(int* argc, wchar_t** argv);
#endif  // _MSC_VER

// These overloaded versions handle ::std::string and ::std::wstring.
#if GTEST_HAS_STD_STRING
inline String FormatForFailureMessage(const ::std::string& str) {
  return (Message() << '"' << str << '"').GetString();
}
inline String FormatForFailureMessage(const ::std::wstring& wstr) {
  return (Message() << "L\"" << wstr << '"').GetString();
}
#endif  // GTEST_HAS_STD_STRING

// These overloaded versions handle ::string and ::wstring.
#if GTEST_HAS_GLOBAL_STRING
inline String FormatForFailureMessage(const ::string& str) {
  return (Message() << '"' << str << '"').GetString();
}
inline String FormatForFailureMessage(const ::wstring& wstr) {
  return (Message() << "L\"" << wstr << '"').GetString();
}
#endif  // GTEST_HAS_GLOBAL_STRING

// Formats a comparison assertion (e.g. ASSERT_EQ, EXPECT_LT, and etc)
// operand to be used in a failure message.  The type (but not value)
// of the other operand may affect the format.  This allows us to
// print a char* as a raw pointer when it is compared against another
// char*, and print it as a C string when it is compared against an
// std::string object, for example.
//
// The default implementation ignores the type of the other operand.
// Some specialized versions are used to handle formatting wide or
// narrow C strings.
//
// INTERNAL IMPLEMENTATION - DO NOT USE IN A USER PROGRAM.
template <typename T1, typename T2>
String FormatForComparisonFailureMessage(const T1& value,
                                         const T2& /* other_operand */) {
  return FormatForFailureMessage(value);
}

// TODO: move more implementation details into namespace internal.
namespace internal {

// The helper function for {ASSERT|EXPECT}_EQ.
template <typename T1, typename T2>
AssertionResult CmpHelperEQ(const char* expected_expression,
                            const char* actual_expression,
                            const T1& expected,
                            const T2& actual) {
  if (expected == actual) {
    return AssertionSuccess();
  }

  return EqFailure(expected_expression,
                   actual_expression,
                   FormatForComparisonFailureMessage(expected, actual),
                   FormatForComparisonFailureMessage(actual, expected),
                   false);
}

// With this overloaded version, we allow anonymous enums to be used
// in {ASSERT|EXPECT}_EQ when compiled with gcc 4, as anonymous enums
// can be implicitly cast to BiggestInt.
AssertionResult CmpHelperEQ(const char* expected_expression,
                            const char* actual_expression,
                            BiggestInt expected,
                            BiggestInt actual);

// The helper class for {ASSERT|EXPECT}_EQ.  The template argument
// lhs_is_null_literal is true iff the first argument to ASSERT_EQ()
// is a null pointer literal.  The following default implementation is
// for lhs_is_null_literal being false.
template <bool lhs_is_null_literal>
class EqHelper {
 public:
  // This templatized version is for the general case.
  template <typename T1, typename T2>
  static AssertionResult Compare(const char* expected_expression,
                                 const char* actual_expression,
                                 const T1& expected,
                                 const T2& actual) {
    return CmpHelperEQ(expected_expression, actual_expression, expected,
                       actual);
  }

  // With this overloaded version, we allow anonymous enums to be used
  // in {ASSERT|EXPECT}_EQ when compiled with gcc 4, as anonymous
  // enums can be implicitly cast to BiggestInt.
  //
  // Even though its body looks the same as the above version, we
  // cannot merge the two, as it will make anonymous enums unhappy.
  static AssertionResult Compare(const char* expected_expression,
                                 const char* actual_expression,
                                 BiggestInt expected,
                                 BiggestInt actual) {
    return CmpHelperEQ(expected_expression, actual_expression, expected,
                       actual);
  }
};

// This specialization is used when the first argument to ASSERT_EQ()
// is a null pointer literal.
template <>
class EqHelper<true> {
 public:
  // We define two overloaded versions of Compare().  The first
  // version will be picked when the second argument to ASSERT_EQ() is
  // NOT a pointer, e.g. ASSERT_EQ(0, AnIntFunction()) or
  // EXPECT_EQ(false, a_bool).
  template <typename T1, typename T2>
  static AssertionResult Compare(const char* expected_expression,
                                 const char* actual_expression,
                                 const T1& expected,
                                 const T2& actual) {
    return CmpHelperEQ(expected_expression, actual_expression, expected,
                       actual);
  }

  // This version will be picked when the second argument to
  // ASSERT_EQ() is a pointer, e.g. ASSERT_EQ(NULL, a_pointer).
  template <typename T1, typename T2>
  static AssertionResult Compare(const char* expected_expression,
                                 const char* actual_expression,
                                 const T1& expected,
                                 T2* actual) {
    // We already know that 'expected' is a null pointer.
    return CmpHelperEQ(expected_expression, actual_expression,
                       static_cast<T2*>(NULL), actual);
  }
};

// A macro for implementing the helper functions needed to implement
// ASSERT_?? and EXPECT_??.  It is here just to avoid copy-and-paste
// of similar code.
//
// For each templatized helper function, we also define an overloaded
// version for BiggestInt in order to reduce code bloat and allow
// anonymous enums to be used with {ASSERT|EXPECT}_?? when compiled
// with gcc 4.
//
// INTERNAL IMPLEMENTATION - DO NOT USE IN A USER PROGRAM.
#define GTEST_IMPL_CMP_HELPER(op_name, op)\
template <typename T1, typename T2>\
AssertionResult CmpHelper##op_name(const char* expr1, const char* expr2, \
                                   const T1& val1, const T2& val2) {\
  if (val1 op val2) {\
    return AssertionSuccess();\
  } else {\
    Message msg;\
    msg << "Expected: (" << expr1 << ") " #op " (" << expr2\
        << "), actual: " << FormatForComparisonFailureMessage(val1, val2)\
        << " vs " << FormatForComparisonFailureMessage(val2, val1);\
    return AssertionFailure(msg);\
  }\
}\
AssertionResult CmpHelper##op_name(const char* expr1, const char* expr2, \
                                   BiggestInt val1, BiggestInt val2);

// INTERNAL IMPLEMENTATION - DO NOT USE IN A USER PROGRAM.

// Implements the helper function for {ASSERT|EXPECT}_NE
GTEST_IMPL_CMP_HELPER(NE, !=)
// Implements the helper function for {ASSERT|EXPECT}_LE
GTEST_IMPL_CMP_HELPER(LE, <=)
// Implements the helper function for {ASSERT|EXPECT}_LT
GTEST_IMPL_CMP_HELPER(LT, < )
// Implements the helper function for {ASSERT|EXPECT}_GE
GTEST_IMPL_CMP_HELPER(GE, >=)
// Implements the helper function for {ASSERT|EXPECT}_GT
GTEST_IMPL_CMP_HELPER(GT, > )

#undef GTEST_IMPL_CMP_HELPER

}  // namespace internal

// The helper function for {ASSERT|EXPECT}_STREQ.
//
// INTERNAL IMPLEMENTATION - DO NOT USE IN A USER PROGRAM.
AssertionResult CmpHelperSTREQ(const char* expected_expression,
                               const char* actual_expression,
                               const char* expected,
                               const char* actual);

// The helper function for {ASSERT|EXPECT}_STRCASEEQ.
//
// INTERNAL IMPLEMENTATION - DO NOT USE IN A USER PROGRAM.
AssertionResult CmpHelperSTRCASEEQ(const char* expected_expression,
                                   const char* actual_expression,
                                   const char* expected,
                                   const char* actual);

// The helper function for {ASSERT|EXPECT}_STRNE.
//
// INTERNAL IMPLEMENTATION - DO NOT USE IN A USER PROGRAM.
AssertionResult CmpHelperSTRNE(const char* s1_expression,
                               const char* s2_expression,
                               const char* s1,
                               const char* s2);

// The helper function for {ASSERT|EXPECT}_STRCASENE.
//
// INTERNAL IMPLEMENTATION - DO NOT USE IN A USER PROGRAM.
AssertionResult CmpHelperSTRCASENE(const char* s1_expression,
                                   const char* s2_expression,
                                   const char* s1,
                                   const char* s2);


// Helper function for *_STREQ on wide strings.
//
// INTERNAL IMPLEMENTATION - DO NOT USE IN A USER PROGRAM.
AssertionResult CmpHelperSTREQ(const char* expected_expression,
                               const char* actual_expression,
                               const wchar_t* expected,
                               const wchar_t* actual);

// Helper function for *_STRNE on wide strings.
//
// INTERNAL IMPLEMENTATION - DO NOT USE IN A USER PROGRAM.
AssertionResult CmpHelperSTRNE(const char* s1_expression,
                               const char* s2_expression,
                               const wchar_t* s1,
                               const wchar_t* s2);

// IsSubstring() and IsNotSubstring() are intended to be used as the
// first argument to {EXPECT,ASSERT}_PRED_FORMAT2(), not by
// themselves.  They check whether needle is a substring of haystack
// (NULL is considered a substring of itself only), and return an
// appropriate error message when they fail. See
// http://wiki/Main/GTestAdvanced#Predicate_Assertions for details.
//
// The {needle,haystack}_expr arguments are the stringified
// expressions that generated the two real arguments.
AssertionResult IsSubstring(
    const char* needle_expr, const char* haystack_expr,
    const char* needle, const char* haystack);
AssertionResult IsSubstring(
    const char* needle_expr, const char* haystack_expr,
    const wchar_t* needle, const wchar_t* haystack);
AssertionResult IsNotSubstring(
    const char* needle_expr, const char* haystack_expr,
    const char* needle, const char* haystack);
AssertionResult IsNotSubstring(
    const char* needle_expr, const char* haystack_expr,
    const wchar_t* needle, const wchar_t* haystack);
#if GTEST_HAS_STD_STRING
AssertionResult IsSubstring(
    const char* needle_expr, const char* haystack_expr,
    const ::std::string& needle, const ::std::string& haystack);
AssertionResult IsSubstring(
    const char* needle_expr, const char* haystack_expr,
    const ::std::wstring& needle, const ::std::wstring& haystack);
AssertionResult IsNotSubstring(
    const char* needle_expr, const char* haystack_expr,
    const ::std::string& needle, const ::std::string& haystack);
AssertionResult IsNotSubstring(
    const char* needle_expr, const char* haystack_expr,
    const ::std::wstring& needle, const ::std::wstring& haystack);
#endif  // GTEST_HAS_STD_STRING

// Helper template function for comparing floating-points.
//
// Template parameter:
//
//   RawType: the raw floating-point type (either float or double)
//
// INTERNAL IMPLEMENTATION - DO NOT USE IN A USER PROGRAM.
template <typename RawType>
AssertionResult CmpHelperFloatingPointEQ(const char* expected_expression,
                                         const char* actual_expression,
                                         RawType expected,
                                         RawType actual) {
  const FloatingPoint<RawType> lhs(expected), rhs(actual);

  if (lhs.AlmostEquals(rhs)) {
    return AssertionSuccess();
  }

  StrStream expected_ss;
  expected_ss << std::setprecision(std::numeric_limits<RawType>::digits10 + 2)
              << expected;

  StrStream actual_ss;
  actual_ss << std::setprecision(std::numeric_limits<RawType>::digits10 + 2)
            << actual;

  return EqFailure(expected_expression,
                   actual_expression,
                   StrStreamToString(&expected_ss),
                   StrStreamToString(&actual_ss),
                   false);
}

// Helper function for implementing ASSERT_NEAR.
//
// INTERNAL IMPLEMENTATION - DO NOT USE IN A USER PROGRAM.
AssertionResult DoubleNearPredFormat(const char* expr1,
                                     const char* expr2,
                                     const char* abs_error_expr,
                                     double val1,
                                     double val2,
                                     double abs_error);

// INTERNAL IMPLEMENTATION - DO NOT USE IN USER CODE.
// A class that enables one to stream messages to assertion macros
class AssertHelper {
 public:
  // Constructor.
  AssertHelper(TestPartResultType type, const char* file, int line,
               const char* message);
  // Message assignment is a semantic trick to enable assertion
  // streaming; see the GTEST_MESSAGE macro below.
  void operator=(const Message& message) const;
 private:
  TestPartResultType const type_;
  const char*        const file_;
  int                const line_;
  String             const message_;

  GTEST_DISALLOW_EVIL_CONSTRUCTORS(AssertHelper);
};

// Macros for indicating success/failure in test code.

// ADD_FAILURE unconditionally adds a failure to the current test.
// SUCCEED generates a success - it doesn't automatically make the
// current test successful, as a test is only successful when it has
// no failure.
//
// EXPECT_* verifies that a certain condition is satisfied.  If not,
// it behaves like ADD_FAILURE.  In particular:
//
//   EXPECT_TRUE  verifies that a Boolean condition is true.
//   EXPECT_FALSE verifies that a Boolean condition is false.
//
// FAIL and ASSERT_* are similar to ADD_FAILURE and EXPECT_*, except
// that they will also abort the current function on failure.  People
// usually want the fail-fast behavior of FAIL and ASSERT_*, but those
// writing data-driven tests often find themselves using ADD_FAILURE
// and EXPECT_* more.
//
// All macros have a *_M variant that accepts an additional message
// argument.  When the check failes, the user-supplied failure message
// (instead of the standard message) will be printed.
//
// Examples:
//
//   EXPECT_TRUE(server.StatusIsOK());
//   ASSERT_FALSE_M(server.HasPendingRequest(port),
//                  testing::Message() << "There are still pending requests "
//                                     << "on port " << port);

// Generates a nonfatal failure with a generic message.
#define ADD_FAILURE() GTEST_NONFATAL_FAILURE("Failed")

// Generates a fatal failure with a generic message.
#define FAIL() GTEST_FATAL_FAILURE("Failed")

// Generates a success with a generic message.
#define SUCCEED() GTEST_SUCCESS("Succeeded")

// Boolean assertions.
#define EXPECT_TRUE(condition) \
  GTEST_TEST_BOOLEAN(condition, #condition, false, true, \
                     GTEST_NONFATAL_FAILURE)
#define EXPECT_FALSE(condition) \
  GTEST_TEST_BOOLEAN(!(condition), #condition, true, false, \
                     GTEST_NONFATAL_FAILURE)
#define ASSERT_TRUE(condition) \
  GTEST_TEST_BOOLEAN(condition, #condition, false, true, \
                     GTEST_FATAL_FAILURE)
#define ASSERT_FALSE(condition) \
  GTEST_TEST_BOOLEAN(!(condition), #condition, true, false, \
                     GTEST_FATAL_FAILURE)

// Defines the variations with _M for backward compatibility.

#define ADD_FAILURE_M(message) ADD_FAILURE() << (message)
#define FAIL_M(message) FAIL() << (message)
#define SUCCEED_M(message) SUCCEED() << (message)

#define EXPECT_TRUE_M(condition, message)  EXPECT_TRUE(condition) << (message)
#define EXPECT_FALSE_M(condition, message) EXPECT_FALSE(condition) << (message)
#define ASSERT_TRUE_M(condition, message)  ASSERT_TRUE(condition) << (message)
#define ASSERT_FALSE_M(condition, message) ASSERT_FALSE(condition) << (message)



#include "gtest_pred_impl.h"  // NOLINT

// Macros for testing equalities and inequalities.
//
//    * {ASSERT|EXPECT}_EQ(expected, actual): Tests that expected == actual
//    * {ASSERT|EXPECT}_NE(v1, v2):           Tests that v1 != v2
//    * {ASSERT|EXPECT}_LT(v1, v2):           Tests that v1 < v2
//    * {ASSERT|EXPECT}_LE(v1, v2):           Tests that v1 <= v2
//    * {ASSERT|EXPECT}_GT(v1, v2):           Tests that v1 > v2
//    * {ASSERT|EXPECT}_GE(v1, v2):           Tests that v1 >= v2
//
// When they are not, gTest prints both the tested expressions and
// their actual values.  The values must be compatible built-in types,
// or you will get a compiler error.  By "compatible" we mean that the
// values can be compared by the respective operator.
//
// Note:
//
//   1. It is possible to make a user-defined type work with
//   {ASSERT|EXPECT}_??(), but that requires overloading the
//   comparison operators and is thus discouraged by the Google C++
//   Usage Guide.  Therefore, you are advised to use the
//   {ASSERT|EXPECT}_TRUE() macro to assert that two objects are
//   equal.
//
//   2. The {ASSERT|EXPECT}_??() macros do pointer comparisons on
//   pointers (in particular, C strings).  Therefore, if you use it
//   with two C strings, you are testing how their locations in memory
//   are related, not how their content is related.  To compare two C
//   strings by content, use {ASSERT|EXPECT}_STR*().
//
//   3. {ASSERT|EXPECT}_EQ(expected, actual) is preferred to
//   {ASSERT|EXPECT}_TRUE(expected == actual), as the former tells you
//   what the actual value is when it fails, and similarly for the
//   other comparisons.
//
//   4. Do not depend on the order in which {ASSERT|EXPECT}_??()
//   evaluate their arguments, which is undefined.
//
//   5. These macros evaluate their arguments exactly once.
//
// Examples:
//
//   EXPECT_NE(5, Foo());
//   EXPECT_EQ(NULL, a_pointer);
//   ASSERT_LT(i, array_size);
//   ASSERT_GT(records.size(), 0) << "There is no record left.";

#define EXPECT_EQ(expected, actual) \
  EXPECT_PRED_FORMAT2(::testing::internal:: \
                      EqHelper<GTEST_IS_NULL_LITERAL(expected)>::Compare, \
                      expected, actual)
#define EXPECT_NE(expected, actual) \
  EXPECT_PRED_FORMAT2(::testing::internal::CmpHelperNE, expected, actual)
#define EXPECT_LE(val1, val2) \
  EXPECT_PRED_FORMAT2(::testing::internal::CmpHelperLE, val1, val2)
#define EXPECT_LT(val1, val2) \
  EXPECT_PRED_FORMAT2(::testing::internal::CmpHelperLT, val1, val2)
#define EXPECT_GE(val1, val2) \
  EXPECT_PRED_FORMAT2(::testing::internal::CmpHelperGE, val1, val2)
#define EXPECT_GT(val1, val2) \
  EXPECT_PRED_FORMAT2(::testing::internal::CmpHelperGT, val1, val2)

#define ASSERT_EQ(expected, actual) \
  ASSERT_PRED_FORMAT2(::testing::internal:: \
                      EqHelper<GTEST_IS_NULL_LITERAL(expected)>::Compare, \
                      expected, actual)
#define ASSERT_NE(val1, val2) \
  ASSERT_PRED_FORMAT2(::testing::internal::CmpHelperNE, val1, val2)
#define ASSERT_LE(val1, val2) \
  ASSERT_PRED_FORMAT2(::testing::internal::CmpHelperLE, val1, val2)
#define ASSERT_LT(val1, val2) \
  ASSERT_PRED_FORMAT2(::testing::internal::CmpHelperLT, val1, val2)
#define ASSERT_GE(val1, val2) \
  ASSERT_PRED_FORMAT2(::testing::internal::CmpHelperGE, val1, val2)
#define ASSERT_GT(val1, val2) \
  ASSERT_PRED_FORMAT2(::testing::internal::CmpHelperGT, val1, val2)

// Defines the variations with _M for backward compatibility.

#define EXPECT_EQ_M(val1, val2, msg) EXPECT_EQ(val1, val2) << (msg)
#define EXPECT_NE_M(val1, val2, msg) EXPECT_NE(val1, val2) << (msg)
#define EXPECT_LE_M(val1, val2, msg) EXPECT_LE(val1, val2) << (msg)
#define EXPECT_LT_M(val1, val2, msg) EXPECT_LT(val1, val2) << (msg)
#define EXPECT_GE_M(val1, val2, msg) EXPECT_GE(val1, val2) << (msg)
#define EXPECT_GT_M(val1, val2, msg) EXPECT_GT(val1, val2) << (msg)

#define ASSERT_EQ_M(val1, val2, msg) ASSERT_EQ(val1, val2) << (msg)
#define ASSERT_NE_M(val1, val2, msg) ASSERT_NE(val1, val2) << (msg)
#define ASSERT_LE_M(val1, val2, msg) ASSERT_LE(val1, val2) << (msg)
#define ASSERT_LT_M(val1, val2, msg) ASSERT_LT(val1, val2) << (msg)
#define ASSERT_GE_M(val1, val2, msg) ASSERT_GE(val1, val2) << (msg)
#define ASSERT_GT_M(val1, val2, msg) ASSERT_GT(val1, val2) << (msg)


// C String Comparisons.  All tests treat NULL and any non-NULL string
// as different.  Two NULLs are equal.
//
//    * {ASSERT|EXPECT}_STREQ(s1, s2):     Tests that s1 == s2
//    * {ASSERT|EXPECT}_STRNE(s1, s2):     Tests that s1 != s2
//    * {ASSERT|EXPECT}_STRCASEEQ(s1, s2): Tests that s1 == s2, ignoring case
//    * {ASSERT|EXPECT}_STRCASENE(s1, s2): Tests that s1 != s2, ignoring case
//
// For wide or narrow string objects, you can use the
// {ASSERT|EXPECT}_??() macros.
//
// Don't depend on the order in which the arguments are evaluated,
// which is undefined.
//
// These macros evaluate their arguments exactly once.

#define EXPECT_STREQ(expected, actual) \
  EXPECT_PRED_FORMAT2(::testing::CmpHelperSTREQ, expected, actual)
#define EXPECT_STRNE(s1, s2) \
  EXPECT_PRED_FORMAT2(::testing::CmpHelperSTRNE, s1, s2)
#define EXPECT_STRCASEEQ(expected, actual) \
  EXPECT_PRED_FORMAT2(::testing::CmpHelperSTRCASEEQ, expected, actual)
#define EXPECT_STRCASENE(s1, s2)\
  EXPECT_PRED_FORMAT2(::testing::CmpHelperSTRCASENE, s1, s2)

#define ASSERT_STREQ(expected, actual) \
  ASSERT_PRED_FORMAT2(::testing::CmpHelperSTREQ, expected, actual)
#define ASSERT_STRNE(s1, s2) \
  ASSERT_PRED_FORMAT2(::testing::CmpHelperSTRNE, s1, s2)
#define ASSERT_STRCASEEQ(expected, actual) \
  ASSERT_PRED_FORMAT2(::testing::CmpHelperSTRCASEEQ, expected, actual)
#define ASSERT_STRCASENE(s1, s2)\
  ASSERT_PRED_FORMAT2(::testing::CmpHelperSTRCASENE, s1, s2)

// Defines the variations with _M for backward compatibility.

#define EXPECT_STREQ_M(s1, s2, msg) EXPECT_STREQ(s1, s2) << (msg)
#define EXPECT_STRNE_M(s1, s2, msg) EXPECT_STRNE(s1, s2) << (msg)
#define EXPECT_STRCASEEQ_M(s1, s2, msg) EXPECT_STRCASEEQ(s1, s2) << (msg)
#define EXPECT_STRCASENE_M(s1, s2, msg) EXPECT_STRCASENE(s1, s2) << (msg)

#define ASSERT_STREQ_M(s1, s2, msg) ASSERT_STREQ(s1, s2) << (msg)
#define ASSERT_STRNE_M(s1, s2, msg) ASSERT_STRNE(s1, s2) << (msg)
#define ASSERT_STRCASEEQ_M(s1, s2, msg) ASSERT_STRCASEEQ(s1, s2) << (msg)
#define ASSERT_STRCASENE_M(s1, s2, msg) ASSERT_STRCASENE(s1, s2) << (msg)


// For backwards compatibility only.  These are deprecated now.
#define ASSERT_EQUALS(expected, actual) ASSERT_EQ(expected, actual)
#define ASSERT_STRING_EQUALS(expected, actual) ASSERT_STREQ(expected, actual)


// Macros for comparing floating-point numbers.
//
//    * {ASSERT|EXPECT}_FLOAT_EQ(expected, actual):
//         Tests that two float values are almost equal.
//    * {ASSERT|EXPECT}_DOUBLE_EQ(expected, actual):
//         Tests that two double values are almost equal.
//    * {ASSERT|EXPECT}_NEAR(v1, v2, abs_error):
//         Tests that v1 and v2 are within the given distance to each other.
//
// gTest uses ULP-based comparison to automatically pick a default
// error bound that is appropriate for the operands.  See the
// FloatingPoint template class in gtest-internal.h if you are
// interested in the implementation details.

#define EXPECT_FLOAT_EQ(expected, actual)\
  EXPECT_PRED_FORMAT2(::testing::CmpHelperFloatingPointEQ<float>, \
                      expected, actual)

#define EXPECT_DOUBLE_EQ(expected, actual)\
  EXPECT_PRED_FORMAT2(::testing::CmpHelperFloatingPointEQ<double>, \
                      expected, actual)

#define ASSERT_FLOAT_EQ(expected, actual)\
  ASSERT_PRED_FORMAT2(::testing::CmpHelperFloatingPointEQ<float>, \
                      expected, actual)

#define ASSERT_DOUBLE_EQ(expected, actual)\
  ASSERT_PRED_FORMAT2(::testing::CmpHelperFloatingPointEQ<double>, \
                      expected, actual)

#define EXPECT_NEAR(val1, val2, abs_error)\
  EXPECT_PRED_FORMAT3(::testing::DoubleNearPredFormat, \
                      val1, val2, abs_error)

#define ASSERT_NEAR(val1, val2, abs_error)\
  ASSERT_PRED_FORMAT3(::testing::DoubleNearPredFormat, \
                      val1, val2, abs_error)

// Defines the variations with _M for backward compatibility.

#define EXPECT_FLOAT_EQ_M(val1, val2, msg) \
  EXPECT_FLOAT_EQ(val1, val2) << (msg)
#define EXPECT_DOUBLE_EQ_M(val1, val2, msg) \
  EXPECT_DOUBLE_EQ(val1, val2) << (msg)
#define EXPECT_NEAR_M(val1, val2, abs_error, msg) \
  EXPECT_NEAR(val1, val2, abs_error) << (msg)
// To workaround a bug in gcc 2.95.0, there is intentionally no space
// between val1 and val2 in the previous line.  Otherwise the failure
// message will contain an unwanted space.  This bug seems to have
// been fixed in gcc 3.4.4 and up, and does not affect MSVC.

#define ASSERT_FLOAT_EQ_M(val1, val2, msg) \
  ASSERT_FLOAT_EQ(val1, val2) << (msg)
#define ASSERT_DOUBLE_EQ_M(val1, val2, msg) \
  ASSERT_DOUBLE_EQ(val1, val2) << (msg)
#define ASSERT_NEAR_M(val1, val2, abs_error, msg) \
  ASSERT_NEAR(val1, val2, abs_error) << (msg)
// For the same reason above, there is intentionally no space between
// val1 and val2 in the previous line.

// These predicate format functions work on floating-point values, and
// can be used in {ASSERT|EXPECT}_PRED_FORMAT2*(), e.g.
//
//   EXPECT_PRED_FORMAT2(testing::DoubleLE, Foo(), 5.0);

// Asserts that val1 is less than, or almost equal to, val2.  Fails
// otherwise.  In particular, it fails if either val1 or val2 is NaN.
AssertionResult FloatLE(const char* expr1, const char* expr2,
                        float val1, float val2);
AssertionResult DoubleLE(const char* expr1, const char* expr2,
                         double val1, double val2);


#ifdef _MSC_VER

// Macros that test for HRESULT failure and success, these are only useful
// on Windows, and rely on Windows SDK macros and APIs to compile.
//
//    * {ASSERT|EXPECT}_HRESULT_{SUCCEEDED|FAILED}(expr)
//
// When expr unexpectedly fails or succeeds, gTest prints the expected result
// and the actual result with both a human-readable string representation of
// the error, if available, as well as the hex result code.
#define EXPECT_HRESULT_SUCCEEDED(expr) \
    EXPECT_PRED_FORMAT1(::testing::internal::IsHRESULTSuccess, (expr))

#define ASSERT_HRESULT_SUCCEEDED(expr) \
    ASSERT_PRED_FORMAT1(::testing::internal::IsHRESULTSuccess, (expr))

#define EXPECT_HRESULT_FAILED(expr) \
    EXPECT_PRED_FORMAT1(::testing::internal::IsHRESULTFailure, (expr))

#define ASSERT_HRESULT_FAILED(expr) \
    ASSERT_PRED_FORMAT1(::testing::internal::IsHRESULTFailure, (expr))

#endif  // _MSC_VER


// Causes a trace (including the source file path, the current line
// number, and the given message) to be included in every test failure
// message generated by code in the current scope.  The effect is
// undone when the control leaves the current scope.
//
// The message argument can be anything streamable to std::ostream.
//
// In the implementation, we include the current line number as part
// of the dummy variable name, thus allowing multiple SCOPED_TRACE()s
// to appear in the same block - as long as they are on different
// lines.
//
// In google3, each thread maintains its own stack of traces.
// Therefore, a SCOPED_TRACE() would (correctly) only affect the
// assertions in its own thread.
#define SCOPED_TRACE(message) \
  ::testing::ScopedTrace GTEST_CONCAT_TOKEN(gtest_trace_, __LINE__)(\
    __FILE__, __LINE__, ::testing::Message() << (message))


// Defines a test.
//
// The first parameter is the name of the test case, and the second
// parameter is the name of the test within the test case.
//
// The convention is to end the test case name with "Test".  For
// example, a test case for the Foo class can be named FooTest.
//
// The user should put his test code between braces after using this
// macro.  Example:
//
//   TEST(FooTest, InitializesCorrectly) {
//     Foo foo;
//     EXPECT_TRUE(foo.StatusIsOK());
//   }

#define TEST(test_case_name, test_name)\
  GTEST_TEST(test_case_name, test_name, ::testing::Test)


// Defines a test that uses a test fixture.
//
// The first parameter is the name of the test fixture class, which
// also doubles as the test case name.  The second parameter is the
// name of the test within the test case.
//
// A test fixture class must be declared earlier.  The user should put
// his test code between braces after using this macro.  Example:
//
//   class FooTest : public testing::Test {
//    protected:
//     virtual void SetUp() { b_.AddElement(3); }
//
//     Foo a_;
//     Foo b_;
//   };
//
//   TEST_F(FooTest, InitializesCorrectly) {
//     EXPECT_TRUE(a_.StatusIsOK());
//   }
//
//   TEST_F(FooTest, ReturnsElementCountCorrectly) {
//     EXPECT_EQ(0, a_.size());
//     EXPECT_EQ(1, b_.size());
//   }

#define TEST_F(test_fixture, test_name)\
  GTEST_TEST(test_fixture, test_name, test_fixture)

// Use this macro in main() to run all tests.  It returns 0 if all
// tests are successful, or 1 otherwise.
//
// RUN_ALL_TESTS() should be invoked after the command line has been
// parsed (on Linux, it's done by InitGoogle().  On other platforms,
// it's done by ParseGTestFlags().)

#define RUN_ALL_TESTS()\
  (::testing::UnitTest::GetInstance()->Run())

}  // namespace testing

#endif  // UNITTEST_GTEST_H__
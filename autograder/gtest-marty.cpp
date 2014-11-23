/*
 * File: gtest-marty.cpp
 * ---------------------
 * This file contains implementation of utility code to augment the functionality
 * of the Google Test C++ unit testing framework.
 * 
 * @author Marty Stepp
 * @version 2014/10/31
 * - added code for graphical test runner UI
 * @version 2014/10/14
 * - moved some code out to diff.cpp and stylecheck.cpp
 */

#include "gtest-marty.h"
#include <algorithm>
#include "autograder.h"
#include "stringutils.h"
#include "filelib.h"
#include "platform.h"
#include "strlib.h"
#include "vector.h"

static std::string UNIT_TEST_TYPE_NAMES[11] = {
    "ASSERT_EQUALS",
    "ASSERT_NOT_EQUALS",
    "ASSERT_NEAR",
    "ASSERT_DIFF",
    "ASSERT_TRUE",
    "ASSERT_FALSE",
    "EXCEPTION",
    "NOT_EXCEPTION",
    "PASS",
    "FAIL",
    "STYLE_CHECK",
};

static Platform* pp = getPlatform();

namespace autograder {
std::string MartyTestResultPrinter::currentTestName = "";
std::string MartyTestResultPrinter::failMessage = "";
std::string MartyGraphicalTestResultPrinter::currentTestName = "";

static const int TEST_OUTPUT_INDENT = 8;
static const std::string TEST_OUTPUT_INDENT_SPACES(TEST_OUTPUT_INDENT, ' ');

UnitTestDetails::UnitTestDetails()
    : testType(autograder::UnitTestType::TEST_ASSERT_EQUALS),
      message(""),
      expected(""),
      student(""),
      passed(false) {
    // empty
}

UnitTestDetails::UnitTestDetails(
        autograder::UnitTestType tp,
        const std::string& msg,
        bool pass)
    : testType(tp),
      message(msg),
      expected(""),
      student(""),
      passed(pass) {
    // empty
}

UnitTestDetails::UnitTestDetails(autograder::UnitTestType tp,
                                 const std::string& msg,
                                 const std::string& exp,
                                 const std::string& stu,
                                 const std::string& vtype,
                                 bool pass)
    : testType(tp),
      message(msg),
      expected(exp),
      student(stu),
      valueType(vtype),
      passed(pass) {
    // empty
}

UnitTestDetails::UnitTestDetails(autograder::UnitTestType tp,
                                 const std::string& msg,
                                 bool exp,
                                 bool stu,
                                 const std::string& /*vtype*/,
                                 bool pass)
    : testType(tp),
      message(msg),
      valueType("bool"),
      passed(pass) {
    expected = boolToString(exp);
    student = boolToString(stu);
}

UnitTestDetails::UnitTestDetails(autograder::UnitTestType tp,
                                 const std::string& msg,
                                 char exp,
                                 char stu,
                                 const std::string& /*vtype*/,
                                 bool pass)
    : testType(tp),
      message(msg),
      valueType("char"),
      passed(pass) {
    expected = charToString(exp);
    student = charToString(stu);
}

UnitTestDetails::UnitTestDetails(autograder::UnitTestType tp,
                                 const std::string& msg,
                                 double exp,
                                 double stu,
                                 const std::string& /*vtype*/,
                                 bool pass)
    : testType(tp),
      message(msg),
      valueType("double"),
      passed(pass) {
    expected = realToString(exp);
    student = realToString(stu);
}

UnitTestDetails::UnitTestDetails(autograder::UnitTestType tp,
                                 const std::string& msg,
                                 int exp,
                                 int stu,
                                 const std::string& /*vtype*/,
                                 bool pass)
    : testType(tp),
      message(msg),
      valueType("int"),
      passed(pass) {
    expected = integerToString(exp);
    student = integerToString(stu);
}

UnitTestDetails::UnitTestDetails(autograder::UnitTestType tp,
                                 const std::string& msg,
                                 const char* const exp,
                                 const char* const stu,
                                 const std::string& /*vtype*/,
                                 bool pass)
    : testType(tp),
      message(msg),
      expected(exp),
      student(stu),
      valueType("string"),
      passed(pass) {
    // empty
}

std::string UnitTestDetails::toString() const {
    std::ostringstream out;
    out << *this;
    return out.str();
}

std::ostream& operator <<(std::ostream& out, const UnitTestDetails& deets) {
    out << "UnitTestDetails{"
        << "testType=" << UNIT_TEST_TYPE_NAMES[deets.testType]
        << ",message=" << urlEncode(deets.message)
        << ",expected=" << urlEncode(deets.expected)
        << ",student=" << urlEncode(deets.student)
        << ",valueType=" << deets.valueType
        << ",passed=" << std::boolalpha << deets.passed
        << "}";
    return out;
}

static std::string gtest_result_reformat(std::string msg) {
    bool hasActualLine = stringContains(msg, "Actual:");
    bool hasWhichIsLine = stringContains(msg, "Which is:");
    Vector<std::string> lines = stringutils::explodeLines(msg);
    for (int i = 0; i < lines.size(); i++) {
        if ((hasActualLine && startsWith(trim(lines[i]), "Value of:")) ||
            (hasWhichIsLine && startsWith(trim(lines[i]), "Expected:"))) {
            lines.remove(i);
            i--;
        }
    }
    msg = stringutils::implode(lines);
    msg = stringReplace(msg, "  Actual:", "Student :");
    msg = stringReplace(msg, "Which is:", "Expected:");
    return msg;
}

MartyTestResultPrinter::MartyTestResultPrinter() {
    testInProgress = false;
    failCountThisTest = 0;
    failCountToPrintPerTest = 0;  // no limit
    testCount = 0;
    failMessage = "";
    testNameWidth = 64;  // default 64 chars wide
}

int MartyTestResultPrinter::getTestNameWidth() const {
    return testNameWidth;
}

void MartyTestResultPrinter::OnTestStart(const ::testing::TestInfo& test_info) {
    testInProgress = true;
    testCount++;
    testTimers[test_info.name()].start();
    std::cout << std::setw(4) << std::right << testCount << ") "
         << std::setw(testNameWidth) << std::left << test_info.name() << " ... ";
    std::cout.flush();
}

void MartyTestResultPrinter::OnTestPartResult(const ::testing::TestPartResult& test_part_result) {
    if (test_part_result.failed()) {
        failCountThisTest++;
        if (failCountThisTest == 1) {
            if (!failMessage.empty()) {
                std::cout << "FAIL!" << std::endl;
            } else {
                std::string msg = test_part_result.message();
                if (msg.find("C++ exception with description ") != std::string::npos) {
                    std::cout << "EXCEPTION!" << std::endl;
                }
            }
        }
        if (failCountToPrintPerTest > 0 && failCountThisTest <= failCountToPrintPerTest) {
            if (!failMessage.empty()) {
                std::cout << TEST_OUTPUT_INDENT_SPACES << failMessage << std::endl;
            }
            std::cout << stringutils::indent(gtest_result_reformat(test_part_result.summary()), TEST_OUTPUT_INDENT) << std::endl;
        }
    }
}

void MartyTestResultPrinter::OnTestEnd(const ::testing::TestInfo& test_info) {
    if (testTimers[test_info.name()].isStarted()) {
        testTimers[test_info.name()].stop();
    }
    if (test_info.result()->Failed()) {
        if (failCountToPrintPerTest > 0 && failCountThisTest > failCountToPrintPerTest) {
            int extraFails = failCountThisTest - failCountToPrintPerTest;
            std::cout << TEST_OUTPUT_INDENT_SPACES << "(" << extraFails << " additional assertion failure(s) not printed)" << std::endl;
        }
    } else {
        std::cout << "pass, " << std::setw(5) << std::right << testTimers[test_info.name()].elapsed() << "ms" << std::endl;
    }
    testInProgress = false;
    failCountThisTest = 0;
}

void MartyTestResultPrinter::OnTestProgramEnd(const ::testing::UnitTest& unit_test) {
    std::cout << "====================================================================" << std::endl;
    int testCount = unit_test.total_test_count();
    int failCount = unit_test.failed_test_count();
    int passCount = testCount - failCount;
    std::cout << "Total tests: " << testCount << ", passed: " << passCount << ", failed: " << failCount << std::endl;
    if (failCount == 0) {
        std::cout << "The student passed all functionality tests. Hooray!" << std::endl;
    } else {
        std::cout << "The student failed " << failCount << " functionality tests. Please investigate." << std::endl;
    }
}

void MartyTestResultPrinter::setFailDetails(const UnitTestDetails& deets) {
    failMessage = FAIL_PREFIX + deets.message;
    switch (deets.testType) {
    case autograder::UnitTestType::TEST_ASSERT_EQUALS:
        failMessage += "  (must be equal)";  break;
    case autograder::UnitTestType::TEST_ASSERT_NOT_EQUALS:
        failMessage += "  (must be non-equal)";  break;
    case autograder::UnitTestType::TEST_ASSERT_NEAR:
        failMessage += "  (must be nearly equal)";  break;
    case autograder::UnitTestType::TEST_ASSERT_TRUE:
        failMessage += "  (must be true)";  break;
    case autograder::UnitTestType::TEST_ASSERT_FALSE:
        failMessage += "  (must be false)";  break;
    default:  break;
    }
}

void MartyTestResultPrinter::setFailsToPrintPerTest(int fails) {
    failCountToPrintPerTest = fails;
}

void MartyTestResultPrinter::setTestNameWidth(int width) {
    testNameWidth = width;
}

// ===========================================================================

void MartyGraphicalTestResultPrinter::setFailDetails(const UnitTestDetails& deets) {
    pp->autograderunittest_setTestDetails(currentTestName, deets.toString());
}

MartyGraphicalTestResultPrinter::MartyGraphicalTestResultPrinter() {
    currentTestName = "";
}

void MartyGraphicalTestResultPrinter::ensureCurrentTestAdded() {
    std::string category = autograder::getCurrentCategoryName();
    if (!testsAdded[category].contains(currentTestName)) {
        testsAdded[category].add(currentTestName);
        pp->autograderunittest_addTest(currentTestName, category);
    }
}

void MartyGraphicalTestResultPrinter::OnTestStart(const ::testing::TestInfo& test_info) {
    currentTestName = test_info.name();
    testTimers[currentTestName] = Timer(true);   // starts timer
}

void MartyGraphicalTestResultPrinter::OnTestPartResult(const ::testing::TestPartResult& /*test_part_result*/) {
    ensureCurrentTestAdded();
}

void MartyGraphicalTestResultPrinter::OnTestEnd(const ::testing::TestInfo& test_info) {
    ensureCurrentTestAdded();
    if (test_info.result()->Failed()) {
        pp->autograderunittest_setTestResult(test_info.name(), "fail");
    } else {
        pp->autograderunittest_setTestResult(test_info.name(), "pass");
    }
    
    if (testTimers.containsKey(currentTestName)) {
        testTimers[currentTestName].stop();
        int runtimeMS = (int) testTimers[currentTestName].elapsed();
        if (runtimeMS >= 10) {
            pp->autograderunittest_setTestRuntime(test_info.name(), runtimeMS);
        }
    }
}

void MartyGraphicalTestResultPrinter::OnTestProgramEnd(const ::testing::UnitTest& /*unit_test*/) {
    // empty
}

} // namespace autograder
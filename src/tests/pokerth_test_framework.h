/*****************************************************************************
 * PokerTH - The open source texas holdem engine                             *
 * Copyright (C) 2006-2012 Felix Hammer, Florian Thauer, Lothar May          *
 *                                                                           *
 * This program is free software: you can redistribute it and/or modify      *
 * it under the terms of the GNU Affero General Public License as            *
 * published by the Free Software Foundation, either version 3 of the        *
 * License, or (at your option) any later version.                           *
 *                                                                           *
 * This program is distributed in the hope that it will be useful,           *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of            *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             *
 * GNU Affero General Public License for more details.                       *
 *                                                                           *
 * You should have received a copy of the GNU Affero General Public License  *
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.     *
 *****************************************************************************/

#ifndef POKERTH_TEST_FRAMEWORK_H
#define POKERTH_TEST_FRAMEWORK_H

#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <sstream>
#include <cmath>

#define TEST_SUITE(Name) \
    namespace test_suite_##Name {

#define END_TEST_SUITE }

#define ASSERT_TRUE(Condition) \
    do { \
        if (!(Condition)) { \
            std::cerr << "[FAILED] " << __FILE__ << ":" << __LINE__ << " - Assertion '" << #Condition << "' failed\n"; \
            return false; \
        } \
    } while(0)

#define ASSERT_FALSE(Condition) \
    do { \
        if (Condition) { \
            std::cerr << "[FAILED] " << __FILE__ << ":" << __LINE__ << " - Assertion '!( " << #Condition << " )' failed\n"; \
            return false; \
        } \
    } while(0)

#define ASSERT_EQ(Expected, Actual) \
    do { \
        auto _exp = (Expected); \
        auto _act = (Actual); \
        if (_exp != _act) { \
            std::cerr << "[FAILED] " << __FILE__ << ":" << __LINE__ << " - Expected '" << _exp << "' but got '" << _act << "'\n"; \
            return false; \
        } \
    } while(0)

#define ASSERT_NE(Expected, Actual) \
    do { \
        auto _exp = (Expected); \
        auto _act = (Actual); \
        if (_exp == _act) { \
            std::cerr << "[FAILED] " << __FILE__ << ":" << __LINE__ << " - Expected '" << _exp << "' to not equal '" << _act << "'\n"; \
            return false; \
        } \
    } while(0)

#define ASSERT_GT(A, B) \
    do { \
        if (!(A > B)) { \
            std::cerr << "[FAILED] " << __FILE__ << ":" << __LINE__ << " - Expected " << #A << " > " << #B << "\n"; \
            return false; \
        } \
    } while(0)

#define ASSERT_LT(A, B) \
    do { \
        if (!(A < B)) { \
            std::cerr << "[FAILED] " << __FILE__ << ":" << __LINE__ << " - Expected " << #A << " < " << #B << "\n"; \
            return false; \
        } \
    } while(0)

#define EXPECT_TRUE(Condition) ASSERT_TRUE(Condition)
#define EXPECT_FALSE(Condition) ASSERT_FALSE(Condition)
#define EXPECT_EQ(Expected, Actual) ASSERT_EQ(Expected, Actual)
#define EXPECT_NE(Expected, Actual) ASSERT_NE(Expected, Actual)
#define EXPECT_GT(A, B) ASSERT_GT(A, B)
#define EXPECT_LT(A, B) ASSERT_LT(A, B)

class TestRunner {
public:
    struct TestCase {
        std::string name;
        std::function<bool()> func;
    };

    static TestRunner& instance() {
        static TestRunner runner;
        return runner;
    }

    void addTest(const std::string& suite, const std::string& name, std::function<bool()> func) {
        TestCase tc;
        tc.name = suite + "." + name;
        tc.func = func;
        m_tests.push_back(tc);
    }

    int runAll() {
        int passed = 0;
        int failed = 0;

        std::cout << "\n========================================\n";
        std::cout << "Running " << m_tests.size() << " tests...\n";
        std::cout << "========================================\n";

        for (const auto& test : m_tests) {
            std::cout << "[RUN   ] " << test.name << "... ";
            try {
                if (test.func()) {
                    std::cout << "[PASSED]\n";
                    passed++;
                } else {
                    std::cout << "[FAILED]\n";
                    failed++;
                }
            } catch (const std::exception& e) {
                std::cout << "[EXCEPTION] " << e.what() << "\n";
                failed++;
            } catch (...) {
                std::cout << "[UNKNOWN EXCEPTION]\n";
                failed++;
            }
        }

        std::cout << "\n========================================\n";
        std::cout << "Results: " << passed << " passed, " << failed << " failed\n";
        std::cout << "========================================\n\n";

        return failed;
    }

private:
    std::vector<TestCase> m_tests;
};

#define TEST(SuiteName, TestName) \
    namespace test_suite_##SuiteName##_impl { \
        bool TestName(); \
        struct TestName##_registrar { \
            TestName##_registrar() { \
                TestRunner::instance().addTest(#SuiteName, #TestName, &TestName); \
            } \
        }; \
        static TestName##_registrar g_##TestName##_instance; \
    } \
    bool test_suite_##SuiteName##_impl::TestName()

#define RUN_ALL_TESTS() \
    TestRunner::instance().runAll()

#endif

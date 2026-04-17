#ifndef CONFIGBUILDERTEST_H
#define CONFIGBUILDERTEST_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "unity.h"
#include "ConfigBuilder.h"
#include <regex.h>
#include <string.h>

namespace ConfigBuilder_tests
{
    // Returns true if str fully matches the given POSIX extended regex pattern.
    static bool matchesRegex(const char* str, const char* pattern) {
        regex_t regex;
        if (regcomp(&regex, pattern, REG_EXTENDED | REG_NOSUB) != 0) {
            return false;
        }
        int result = regexec(&regex, str, 0, NULL, 0);
        regfree(&regex);
        return result == 0;
    }

    // Validates that getFirmWareURL() returns a correctly formatted URL:
    //   http://<ipv4>:<port>/<project_name>.bin
    void test_getFirmwareURL_Format() {
        ConfigBuilder builder;
        const char* url = builder.getFirmWareURL();
        TEST_ASSERT_NOT_NULL_MESSAGE(url, "getFirmWareURL() returned NULL");

        // Pattern: http://<1-3 digit octets>:<port 1-5 digits>/<name with alnum/underscore/dash>.bin
        const char* pattern =
            "^http://[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}"
            ":[0-9]{1,5}"
            "/[a-zA-Z0-9_-]+"
            "\\.bin$";

        TEST_ASSERT_TRUE_MESSAGE(
            matchesRegex(url, pattern),
            "Firmware URL does not match expected format: http://<ip>:<port>/<name>.bin"
        );
    }

    void registerTests() {
        RUN_TEST(test_getFirmwareURL_Format);
    }
}

#endif

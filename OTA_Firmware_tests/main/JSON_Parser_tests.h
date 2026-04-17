#ifndef JSON_PARSER_TESTS_H
#define JSON_PARSER_TESTS_H

#include "unity.h"
#include "JSON_Parser.h"
#include "cJSON.h"

namespace JSON_Parser_tests
{

    void test_ValidJsonParsing() {
        const char* json_string = "{\"key\": \"value\"}";
        cJSON* json = cJSON_Parse(json_string);
        TEST_ASSERT_NOT_NULL(json);

        cJSON* key_item = cJSON_GetObjectItem(json, "key");
        TEST_ASSERT_TRUE(cJSON_IsString(key_item));
        TEST_ASSERT_EQUAL_STRING("value", key_item->valuestring);

        cJSON_Delete(json);
    }    

    void test_InvalidJsonParsing() {
        const char* invalid_json_string = "{\"key\": \"value\""; // Missing closing brace
        cJSON* json = cJSON_Parse(invalid_json_string);
        TEST_ASSERT_NULL(json);
    }

    void test_MissingKey() {
        const char* json_string = "{\"key\": \"value\"}";
        cJSON* json = cJSON_Parse(json_string);
        TEST_ASSERT_NOT_NULL(json);

        cJSON* missing_key_item = cJSON_GetObjectItem(json, "missing_key");
        TEST_ASSERT_NULL(missing_key_item);

        cJSON_Delete(json);
    }

    void test_WrongType() {
        const char* json_string = "{\"key\": 123}";
        cJSON* json = cJSON_Parse(json_string);
        TEST_ASSERT_NOT_NULL(json);

        cJSON* key_item = cJSON_GetObjectItem(json, "key");
        TEST_ASSERT_FALSE(cJSON_IsString(key_item));
        TEST_ASSERT_TRUE(cJSON_IsNumber(key_item));
        TEST_ASSERT_EQUAL_DOUBLE(123, key_item->valuedouble);

        cJSON_Delete(json);
    }

    void test_EmptyJson() {
        const char* empty_json_string = "{}";
        cJSON* json = cJSON_Parse(empty_json_string);
        TEST_ASSERT_NOT_NULL(json);

        cJSON* key_item = cJSON_GetObjectItem(json, "key");
        TEST_ASSERT_NULL(key_item);

        cJSON_Delete(json);
    }

    void test_NullJson() {
        const char* null_json_string = "null";
        cJSON* json = cJSON_Parse(null_json_string);
        TEST_ASSERT_NOT_NULL(json);
        TEST_ASSERT_TRUE(cJSON_IsNull(json));

        cJSON_Delete(json);
    }

    void test_ComplexJson() {
        const char* complex_json_string = "{\"name\": \"John\", \"age\": 30, \"is_student\": false, \"scores\": [85, 90, 92]}";
        cJSON* json = cJSON_Parse(complex_json_string);
        TEST_ASSERT_NOT_NULL(json);

        cJSON* name_item = cJSON_GetObjectItem(json, "name");
        TEST_ASSERT_TRUE(cJSON_IsString(name_item));
        TEST_ASSERT_EQUAL_STRING("John", name_item->valuestring);

        cJSON* age_item = cJSON_GetObjectItem(json, "age");
        TEST_ASSERT_TRUE(cJSON_IsNumber(age_item));
        TEST_ASSERT_EQUAL_DOUBLE(30, age_item->valuedouble);

        cJSON* is_student_item = cJSON_GetObjectItem(json, "is_student");
        TEST_ASSERT_TRUE(cJSON_IsFalse(is_student_item));

        cJSON* scores_item = cJSON_GetObjectItem(json, "scores");
        TEST_ASSERT_TRUE(cJSON_IsArray(scores_item));
        TEST_ASSERT_EQUAL_INT(3, cJSON_GetArraySize(scores_item));

        cJSON_Delete(json);
    }

    void test_GetJSONConfigObject_ValidNames() {
        JSONParser parser; 
        TEST_ASSERT_NOT_EQUAL(parser.getJSONConfigObject("project"), nullptr);
        TEST_ASSERT_NOT_EQUAL(parser.getJSONConfigObject("router"), nullptr);
        TEST_ASSERT_NOT_EQUAL(parser.getJSONConfigObject("server"), nullptr);
        TEST_ASSERT_NOT_EQUAL(parser.getJSONConfigObject("mqtt"), nullptr);
    }

    void test_GetJSONConfigObject_InvalidName() {
        JSONParser parser;
        TEST_ASSERT_NULL(parser.getJSONConfigObject("unknown"));
    }

    void test_GetJSONConfigObject_NullOrEmpty() {
        JSONParser parser;
        TEST_ASSERT_NULL(parser.getJSONConfigObject(nullptr));
        TEST_ASSERT_NULL(parser.getJSONConfigObject(""));
    }

    void registerTests() {
        RUN_TEST(test_ValidJsonParsing);
        RUN_TEST(test_InvalidJsonParsing);
        RUN_TEST(test_MissingKey);
        RUN_TEST(test_WrongType);
        RUN_TEST(test_EmptyJson);
        RUN_TEST(test_NullJson);
        RUN_TEST(test_ComplexJson);
        RUN_TEST(test_GetJSONConfigObject_ValidNames);
        RUN_TEST(test_GetJSONConfigObject_InvalidName);
        RUN_TEST(test_GetJSONConfigObject_NullOrEmpty);

    }

} // namespace JSON_Parser_tests

#endif


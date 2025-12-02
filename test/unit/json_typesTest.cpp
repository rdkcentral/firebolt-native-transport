#include "json_types.h"
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

using namespace Firebolt::JSON;

class JsonTypesTest : public ::testing::Test
{
};

TEST_F(JsonTypesTest, StringBasicType)
{
    String str;
    nlohmann::json json = "test string";
    str.fromJson(json);
    EXPECT_EQ(str.value(), "test string");
}

TEST_F(JsonTypesTest, BooleanBasicType)
{
    Boolean boolean;
    nlohmann::json json = true;
    boolean.fromJson(json);
    EXPECT_TRUE(boolean.value());
}

TEST_F(JsonTypesTest, FloatBasicType)
{
    Float floatVal;
    nlohmann::json json = 3.14f;
    floatVal.fromJson(json);
    EXPECT_FLOAT_EQ(floatVal.value(), 3.14f);
}

TEST_F(JsonTypesTest, UnsignedBasicType)
{
    Unsigned unsignedVal;
    nlohmann::json json = 42u;
    unsignedVal.fromJson(json);
    EXPECT_EQ(unsignedVal.value(), 42u);
}

TEST_F(JsonTypesTest, IntegerBasicType)
{
    Integer intVal;
    nlohmann::json json = -42;
    intVal.fromJson(json);
    EXPECT_EQ(intVal.value(), -42);
}

TEST_F(JsonTypesTest, StringArrayType)
{
    NL_Json_Array<String, std::string> stringArray;
    nlohmann::json json = {"first", "second", "third"};
    stringArray.fromJson(json);

    std::vector<std::string> result = stringArray.value();
    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], "first");
    EXPECT_EQ(result[1], "second");
    EXPECT_EQ(result[2], "third");
}

TEST_F(JsonTypesTest, IntegerArrayType)
{
    NL_Json_Array<Integer, int32_t> intArray;
    nlohmann::json json = {1, 2, 3, 4, 5};
    intArray.fromJson(json);

    std::vector<int32_t> result = intArray.value();
    ASSERT_EQ(result.size(), 5);
    EXPECT_EQ(result[0], 1);
    EXPECT_EQ(result[4], 5);
}

TEST_F(JsonTypesTest, BooleanArrayType)
{
    NL_Json_Array<Boolean, bool> boolArray;
    nlohmann::json json = {true, false, true};
    boolArray.fromJson(json);

    std::vector<bool> result = boolArray.value();
    ASSERT_EQ(result.size(), 3);
    EXPECT_TRUE(result[0]);
    EXPECT_FALSE(result[1]);
    EXPECT_TRUE(result[2]);
}

TEST_F(JsonTypesTest, EmptyArray)
{
    NL_Json_Array<String, std::string> stringArray;
    nlohmann::json json = nlohmann::json::array();
    stringArray.fromJson(json);

    std::vector<std::string> result = stringArray.value();
    EXPECT_TRUE(result.empty());
}

TEST_F(JsonTypesTest, EnumTypeToString)
{
    enum class Color { Red, Green, Blue };
    EnumType<Color> colorMap = {
        {"red", Color::Red},
        {"green", Color::Green},
        {"blue", Color::Blue}
    };

    EXPECT_EQ(toString(colorMap, Color::Red), "red");
    EXPECT_EQ(toString(colorMap, Color::Green), "green");
    EXPECT_EQ(toString(colorMap, Color::Blue), "blue");
}

TEST_F(JsonTypesTest, EnumTypeToStringCase)
{
    enum class Color { Red, Green, Blue };
    EnumType<Color> colorMap = {
        {"Red", Color::Red},
        {"Green", Color::Green},
        {"Blue", Color::Blue}
    };

    EXPECT_EQ(toString(colorMap, Color::Red), "Red");
    ASSERT_FALSE(toString(colorMap, Color::Green) == "green");
    ASSERT_FALSE(toString(colorMap, Color::Blue) == "blue");
}

TEST_F(JsonTypesTest, EnumTypeToStringNotFound)
{
    enum class Status { Active, Inactive };
    EnumType<Status> statusMap = {
        {"active", Status::Active}
    };

    std::string result = toString(statusMap, Status::Inactive);
    EXPECT_TRUE(result.empty());
}

TEST_F(JsonTypesTest, FloatArrayType)
{
    NL_Json_Array<Float, float> floatArray;
    nlohmann::json json = {1.1f, 2.2f, 3.3f};
    floatArray.fromJson(json);

    std::vector<float> result = floatArray.value();
    ASSERT_EQ(result.size(), 3);
    EXPECT_FLOAT_EQ(result[0], 1.1f);
    EXPECT_FLOAT_EQ(result[1], 2.2f);
    EXPECT_FLOAT_EQ(result[2], 3.3f);
}

TEST_F(JsonTypesTest, UnsignedArrayType)
{
    NL_Json_Array<Unsigned, uint32_t> unsignedArray;
    nlohmann::json json = {10u, 20u, 30u};
    unsignedArray.fromJson(json);

    std::vector<uint32_t> result = unsignedArray.value();
    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], 10u);
    EXPECT_EQ(result[1], 20u);
    EXPECT_EQ(result[2], 30u);
}

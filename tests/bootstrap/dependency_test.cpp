#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include <nlohmann/json.hpp>
#include <picosha2.h>

#include <string>

static_assert(__cplusplus >= 202002L);
static_assert(sizeof(void*) == 8);
static_assert(NLOHMANN_JSON_VERSION_MAJOR == 3 && NLOHMANN_JSON_VERSION_MINOR == 12 && NLOHMANN_JSON_VERSION_PATCH == 0);
static_assert(DOCTEST_VERSION_MAJOR == 2 && DOCTEST_VERSION_MINOR == 5 && DOCTEST_VERSION_PATCH == 3);

TEST_CASE("JSON parses and preserves typed data through serialization") {
    const auto value = nlohmann::json::parse(R"({"schema_version":1,"frame":42,"paused":true,"systems":["synthetic","megadrive"]})");
    CHECK(value.at("schema_version").get<int>() == 1);
    CHECK(value.at("frame").get<int>() == 42);
    CHECK(value.at("paused").get<bool>());
    CHECK(value.at("systems").at(1).get<std::string>() == "megadrive");
    CHECK(nlohmann::json::parse(value.dump()) == value);
}

TEST_CASE("SHA-256 matches independent published known vectors") {
    CHECK(picosha2::hash256_hex_string(std::string{}) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    CHECK(picosha2::hash256_hex_string(std::string{"abc"}) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK(picosha2::hash256_hex_string(std::string{"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"}) == "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
}

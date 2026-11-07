// BSD 3-Clause License
// Copyright (c) 2021-2026, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file qr_matching_test.cpp
 * @brief Attribute matching rules of DICOM PS3.4 Section C.2.2.2
 */

#include <kcenon/pacs/services/qr_matching.h>

#include <catch2/catch_test_macros.hpp>

using kcenon::pacs::services::key_matches;
using kcenon::pacs::services::key_matching;
using kcenon::pacs::services::wildcard_matches;

TEST_CASE("wild cards follow C.2.2.2.4", "[services][qr][matching]") {
    SECTION("star matches any sequence, including none") {
        CHECK(wildcard_matches("DOE*", "DOE^JOHN", false));
        CHECK(wildcard_matches("DOE*", "DOE", false));
        CHECK(wildcard_matches("*JOHN", "DOE^JOHN", false));
        CHECK(wildcard_matches("D*E*N", "DOE^JOHN", false));
        CHECK(wildcard_matches("*", "", false));
        CHECK_FALSE(wildcard_matches("*X", "DOE^JOHN", false));
    }
    SECTION("question mark is one character, not one byte") {
        CHECK(wildcard_matches("DO?", "DOE", false));
        CHECK_FALSE(wildcard_matches("DO?", "DO", false));
        CHECK_FALSE(wildcard_matches("DO?", "DOES", false));
        CHECK(wildcard_matches("\xED\x99\x8D^\xEA\xB8\xB8?", "\xED\x99\x8D^\xEA\xB8\xB8\xEB\x8F\x99", false));
        CHECK_FALSE(wildcard_matches("\xED\x99\x8D^\xEA\xB8\xB8???", "\xED\x99\x8D^\xEA\xB8\xB8\xEB\x8F\x99", false));
    }
    SECTION("a star backs up by whole characters") {
        CHECK(wildcard_matches("*\xEB\x8F\x99", "\xEA\xB8\xB8\xEB\x8F\x99", false));
        CHECK(wildcard_matches("*?\xEB\x8F\x99", "\xEA\xB8\xB8\xEB\x8F\x99", false));
    }
    SECTION("case is significant unless asked otherwise") {
        CHECK_FALSE(wildcard_matches("doe*", "DOE^JOHN", false));
        CHECK(wildcard_matches("doe*", "DOE^JOHN", true));
    }
    SECTION("SQL wild cards and quotes are ordinary characters") {
        CHECK_FALSE(wildcard_matches("100%", "100X", false));
        CHECK(wildcard_matches("100%", "100%", false));
        CHECK_FALSE(wildcard_matches("QR_P7", "QRXP7", false));
        CHECK(wildcard_matches("O'BRIEN*", "O'BRIEN^SEAN", false));
    }
}

TEST_CASE("universal and unknown values follow C.2.2.1.2", "[services][qr][matching]") {
    SECTION("an empty key matches everything") {
        CHECK(key_matches(key_matching::text, "", "ANY"));
        CHECK(key_matches(key_matching::uid, "  ", "1.2.3"));
    }
    SECTION("an empty stored value matches any key, as its value is unknown") {
        CHECK(key_matches(key_matching::text, "ACC-001", ""));
        CHECK(key_matches(key_matching::date, "20240101-20241231", ""));
        CHECK(key_matches(key_matching::number, "3", ""));
    }
}

TEST_CASE("text keys match by value", "[services][qr][matching]") {
    SECTION("single values are case-sensitive except person names") {
        CHECK(key_matches(key_matching::text, "CT", "CT"));
        CHECK_FALSE(key_matches(key_matching::text, "ct", "CT"));
        CHECK(key_matches(key_matching::person_name, "doe^john", "DOE^JOHN"));
    }
    SECTION("padding is not part of the value") {
        CHECK(key_matches(key_matching::text, "CT ", "CT"));
        CHECK(key_matches(key_matching::text, "CT", "CT "));
        CHECK(key_matches(key_matching::uid, std::string_view("1.2\0", 4), "1.2"));
    }
    SECTION("any stored value of a multi-valued attribute can match (C.2.2.3)") {
        CHECK(key_matches(key_matching::text, "OT", "CT\\OT"));
        CHECK_FALSE(key_matches(key_matching::text, "MR", "CT\\OT"));
        CHECK(key_matches(key_matching::text, "O*", "CT\\OT"));
    }
    SECTION("several key values mean any of them") {
        CHECK(key_matches(key_matching::text, "CR\\MR", "MR"));
        CHECK_FALSE(key_matches(key_matching::text, "CR\\MR", "CT"));
    }
}

TEST_CASE("UIDs follow List of UID Matching", "[services][qr][matching]") {
    CHECK(key_matches(key_matching::uid, "1.2.3", "1.2.3"));
    CHECK(key_matches(key_matching::uid, "1.2.3\\1.2.4", "1.2.4"));
    CHECK_FALSE(key_matches(key_matching::uid, "1.2.3\\1.2.4", "1.2.5"));
    CHECK_FALSE(key_matches(key_matching::uid, "1.2.*", "1.2.3"));
    CHECK_FALSE(key_matches(key_matching::uid, "1.2.3", "1.2.33"));
}

TEST_CASE("dates follow C.2.2.2.5.1", "[services][qr][matching]") {
    SECTION("single value") {
        CHECK(key_matches(key_matching::date, "20240115", "20240115"));
        CHECK_FALSE(key_matches(key_matching::date, "20240116", "20240115"));
        CHECK(key_matches(key_matching::date, "2024.01.15", "20240115"));
    }
    SECTION("ranges include both ends") {
        CHECK(key_matches(key_matching::date, "20240101-20240131", "20240101"));
        CHECK(key_matches(key_matching::date, "20240101-20240131", "20240131"));
        CHECK_FALSE(key_matches(key_matching::date, "20240101-20240131", "20240201"));
    }
    SECTION("open ranges") {
        CHECK(key_matches(key_matching::date, "-20240131", "19000101"));
        CHECK_FALSE(key_matches(key_matching::date, "-20240131", "20240201"));
        CHECK(key_matches(key_matching::date, "20240101-", "20991231"));
        CHECK_FALSE(key_matches(key_matching::date, "20240101-", "20231231"));
    }
    SECTION("what is not a date matches nothing") {
        CHECK_FALSE(key_matches(key_matching::date, "2024*", "20240115"));
        CHECK_FALSE(key_matches(key_matching::date, "-", "20240115"));
        CHECK_FALSE(key_matches(key_matching::date, "20240115' OR '1'='1", "20240115"));
        CHECK_FALSE(key_matches(key_matching::date, "20240115", "not a date"));
    }
}

TEST_CASE("times follow C.2.2.2.5.2 by meaning", "[services][qr][matching]") {
    SECTION("ranges with full precision") {
        CHECK(key_matches(key_matching::time, "100000-103000", "101500"));
        CHECK(key_matches(key_matching::time, "100000-103000", "103000"));
        CHECK_FALSE(key_matches(key_matching::time, "100000-103000", "103001"));
    }
    SECTION("omitted precision covers what the value names") {
        CHECK(key_matches(key_matching::time, "10-10", "105959.999"));
        CHECK(key_matches(key_matching::time, "10", "101500"));
        CHECK_FALSE(key_matches(key_matching::time, "10", "110000"));
        CHECK(key_matches(key_matching::time, "1015", "101559"));
        CHECK(key_matches(key_matching::time, "-12", "125959"));
        CHECK_FALSE(key_matches(key_matching::time, "-12", "130000"));
    }
    SECTION("a stored value with less precision starts where it names") {
        CHECK(key_matches(key_matching::time, "100000-103000", "10"));
        CHECK_FALSE(key_matches(key_matching::time, "103000-", "10"));
    }
    SECTION("what is not a time matches nothing") {
        CHECK_FALSE(key_matches(key_matching::time, "1*", "101500"));
        CHECK_FALSE(key_matches(key_matching::time, "101", "101500"));
    }
}

TEST_CASE("numbers match by value", "[services][qr][matching]") {
    CHECK(key_matches(key_matching::number, "2", "2"));
    CHECK(key_matches(key_matching::number, "02", "2"));
    CHECK(key_matches(key_matching::number, "+2", "2"));
    CHECK_FALSE(key_matches(key_matching::number, "2", "12"));
    CHECK_FALSE(key_matches(key_matching::number, "2*", "2"));
}

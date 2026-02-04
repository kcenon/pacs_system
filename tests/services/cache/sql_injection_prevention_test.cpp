/**
 * @file sql_injection_prevention_test.cpp
 * @brief Unit tests for SQL injection prevention in database_cursor
 *
 * Tests verify that user input is properly escaped before being used
 * in SQL queries to prevent SQL injection attacks.
 *
 * @see Issue #660 - Fix SQL Injection vulnerability in database_cursor.cpp
 */

#include <catch2/catch_test_macros.hpp>

#ifdef PACS_WITH_DATABASE_SYSTEM

#include <database/query_builder/value_formatter.h>
#include <database/database_types.h>

#include <string>

// ============================================================================
// SQL Injection Prevention Tests
// ============================================================================

TEST_CASE("value_formatter escapes SQL injection attempts",
          "[security][sql-injection]") {
    database::query::value_formatter formatter(database::database_types::sqlite);

    SECTION("Escapes single quotes") {
        // This is the most common SQL injection vector
        auto input = "test'; DROP TABLE patients; --";
        auto escaped = formatter.escape_string(input);

        // Single quotes should be doubled in SQLite
        // The escaped string should be "test''; DROP TABLE patients; --"
        // Note: The substring "'; DROP" is still present but starts at the
        // second quote, making the SQL injection harmless when properly quoted
        REQUIRE(escaped.find("''") != std::string::npos);
        REQUIRE(escaped == "test''; DROP TABLE patients; --");
    }

    SECTION("Escapes double single quotes") {
        auto input = "O'Brien's test";
        auto escaped = formatter.escape_string(input);

        // Both single quotes should be escaped
        REQUIRE(escaped == "O''Brien''s test");
    }

    SECTION("Handles empty string") {
        auto input = "";
        auto escaped = formatter.escape_string(input);
        REQUIRE(escaped.empty());
    }

    SECTION("Preserves normal text") {
        auto input = "John Doe";
        auto escaped = formatter.escape_string(input);
        REQUIRE(escaped == "John Doe");
    }

    SECTION("Preserves backslashes in SQLite") {
        // SQLite does not require backslash escaping unlike MySQL/PostgreSQL
        // Only single quotes need to be doubled in SQLite
        auto input = "test\\path";
        auto escaped = formatter.escape_string(input);

        // Backslashes are preserved as-is in SQLite
        REQUIRE(escaped == "test\\path");
    }

    SECTION("Handles DICOM patient names") {
        // DICOM patient names use ^ as component separator
        auto input = "Doe^John^A^^Dr";
        auto escaped = formatter.escape_string(input);

        // Should preserve DICOM format
        REQUIRE(escaped == "Doe^John^A^^Dr");
    }

    SECTION("Handles DICOM wildcards safely") {
        // DICOM wildcards (* and ?) should be preserved
        // but SQL-relevant characters should be escaped
        auto input = "Smith*'; DROP TABLE --";
        auto escaped = formatter.escape_string(input);

        // Single quote should be escaped
        REQUIRE(escaped.find("''") != std::string::npos);
        // DICOM wildcard should be preserved
        REQUIRE(escaped.find("*") != std::string::npos);
    }

    SECTION("Handles Unicode characters") {
        // Korean patient name
        auto input = "김철수";
        auto escaped = formatter.escape_string(input);
        REQUIRE(escaped == "김철수");
    }

    SECTION("Escapes UNION injection attempt") {
        auto input = "test' UNION SELECT * FROM users --";
        auto escaped = formatter.escape_string(input);

        // The single quote should be doubled, neutralizing the injection
        // The escaped string becomes: "test'' UNION SELECT * FROM users --"
        // When used in a properly quoted SQL string, the doubled quote is
        // treated as a literal quote character, not a string terminator
        REQUIRE(escaped.find("''") != std::string::npos);
        REQUIRE(escaped == "test'' UNION SELECT * FROM users --");
    }

    SECTION("Prevents comment injection") {
        auto input = "test'/*comment*/";
        auto escaped = formatter.escape_string(input);

        // Single quote should be escaped
        REQUIRE(escaped.find("''") != std::string::npos);
    }
}

TEST_CASE("value_formatter handles LIKE pattern escaping",
          "[security][sql-injection][like]") {
    database::query::value_formatter formatter(database::database_types::sqlite);

    SECTION("Escapes SQL wildcards in LIKE patterns") {
        // User searching for literal % in data
        auto input = "50%";
        auto escaped = formatter.escape_string(input);

        // Should preserve % (DICOM escaping handles wildcard conversion separately)
        REQUIRE(escaped == "50%");
    }

    SECTION("Escapes injection through LIKE pattern") {
        // Attempt to inject through LIKE
        auto input = "%'; DELETE FROM patients; --";
        auto escaped = formatter.escape_string(input);

        // Single quote should be doubled, neutralizing the injection
        // The escaped string becomes: "%''; DELETE FROM patients; --"
        REQUIRE(escaped.find("''") != std::string::npos);
        REQUIRE(escaped == "%''; DELETE FROM patients; --");
    }
}

TEST_CASE("SQL injection edge cases", "[security][sql-injection][edge-cases]") {
    database::query::value_formatter formatter(database::database_types::sqlite);

    SECTION("Multiple injection attempts") {
        auto input = "'; DROP TABLE t; SELECT '";
        auto escaped = formatter.escape_string(input);

        // All single quotes should be escaped
        size_t count = 0;
        for (size_t i = 0; i < escaped.size() - 1; i++) {
            if (escaped[i] == '\'' && escaped[i + 1] == '\'') {
                count++;
                i++;  // Skip the escaped quote
            }
        }
        REQUIRE(count == 2);  // Both quotes escaped
    }

    SECTION("Nested quotes") {
        auto input = "test'''test";
        auto escaped = formatter.escape_string(input);

        // Three single quotes should become six
        REQUIRE(escaped.find("''''''") != std::string::npos);
    }

    SECTION("Very long injection attempt") {
        std::string input(1000, 'a');
        input += "'; DROP TABLE patients; --";
        auto escaped = formatter.escape_string(input);

        // Should still escape properly - single quote is doubled
        REQUIRE(escaped.find("''") != std::string::npos);
        // Escaped string should have the single quote doubled
        REQUIRE(escaped.length() == input.length() + 1);  // One extra char for doubled quote
    }
}

#else  // !PACS_WITH_DATABASE_SYSTEM

TEST_CASE("SQL injection tests require PACS_WITH_DATABASE_SYSTEM",
          "[security][sql-injection]") {
    // Skip test when database system is not available
    WARN("SQL injection tests skipped - PACS_WITH_DATABASE_SYSTEM not defined");
    REQUIRE(true);  // Always pass to avoid empty test case
}

#endif  // PACS_WITH_DATABASE_SYSTEM

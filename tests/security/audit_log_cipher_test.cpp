/**
 * @file audit_log_cipher_test.cpp
 * @brief Unit tests for the audit log at-rest cipher (Issue #1102)
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "kcenon/pacs/security/audit_log_cipher.h"

#include <array>
#include <cstring>
#include <string>

using namespace kcenon::pacs::security;
namespace detail = kcenon::pacs::security::detail;

namespace {

// Deterministic test key material. These are hard-coded TEST-ONLY bytes
// used strictly inside unit tests. They are not secrets and must never be
// used in production — production keys come from the environment.
audit_cipher_key make_test_key(const std::string& id, std::uint8_t seed) {
    audit_cipher_key k;
    k.key_id = id;
    for (std::size_t i = 0; i < k.data_key.size(); ++i) {
        k.data_key[i] = static_cast<std::uint8_t>(seed + i);
    }
    for (std::size_t i = 0; i < k.mac_key.size(); ++i) {
        k.mac_key[i] = static_cast<std::uint8_t>(seed * 3u + i);
    }
    return k;
}

}  // namespace

// =============================================================================
// Encoding helpers
// =============================================================================

TEST_CASE("base64 round trip with various lengths",
          "[security][audit_cipher]") {
    for (std::size_t n : {0u, 1u, 2u, 3u, 4u, 5u, 16u, 31u, 32u, 64u, 100u}) {
        std::vector<std::uint8_t> buf(n);
        for (std::size_t i = 0; i < n; ++i) {
            buf[i] = static_cast<std::uint8_t>((i * 17u + 3u) & 0xFFu);
        }
        const std::string enc = detail::base64_encode(buf.data(), buf.size());
        std::vector<std::uint8_t> back;
        REQUIRE(detail::base64_decode(enc, back));
        REQUIRE(back == buf);
    }
}

TEST_CASE("base64 rejects invalid strings",
          "[security][audit_cipher]") {
    std::vector<std::uint8_t> out;
    // Length not multiple of 4
    CHECK_FALSE(detail::base64_decode("abc", out));
    // Invalid character
    CHECK_FALSE(detail::base64_decode("ab*d", out));
    // Misplaced padding
    CHECK_FALSE(detail::base64_decode("ab=c", out));
}

TEST_CASE("hex_decode accepts and rejects as expected",
          "[security][audit_cipher]") {
    std::array<std::uint8_t, 4> buf{};
    CHECK(detail::hex_decode("deadbeef", buf.data(), buf.size()));
    CHECK(buf[0] == 0xde);
    CHECK(buf[1] == 0xad);
    CHECK(buf[2] == 0xbe);
    CHECK(buf[3] == 0xef);

    CHECK_FALSE(detail::hex_decode("deadbe", buf.data(), buf.size()));  // short
    CHECK_FALSE(detail::hex_decode("zzzzzzzz", buf.data(), buf.size()));
}

TEST_CASE("constant_time_equal behaves like equality",
          "[security][audit_cipher]") {
    std::array<std::uint8_t, 8> a{1, 2, 3, 4, 5, 6, 7, 8};
    std::array<std::uint8_t, 8> b{1, 2, 3, 4, 5, 6, 7, 8};
    std::array<std::uint8_t, 8> c{1, 2, 3, 4, 5, 6, 7, 9};
    CHECK(detail::constant_time_equal(a.data(), b.data(), a.size()));
    CHECK_FALSE(detail::constant_time_equal(a.data(), c.data(), a.size()));
}

// =============================================================================
// Key manager
// =============================================================================

TEST_CASE("audit_key_manager registers, activates and looks up keys",
          "[security][audit_cipher]") {
    audit_key_manager mgr;
    CHECK(mgr.size() == 0);

    mgr.register_key(make_test_key("kA", 0x10));
    mgr.register_key(make_test_key("kB", 0x20));
    CHECK(mgr.size() == 2);

    // First registered key becomes active automatically
    auto act = mgr.active();
    REQUIRE(act);
    CHECK(act.value().key_id == "kA");

    auto r = mgr.set_active("kB");
    REQUIRE(r);
    CHECK(mgr.active().value().key_id == "kB");

    CHECK(mgr.find("kB") != nullptr);
    CHECK(mgr.find("missing") == nullptr);

    auto bad = mgr.set_active("missing");
    CHECK_FALSE(bad);

    mgr.forget("kB");
    CHECK(mgr.size() == 1);
    // After forgetting the active key, no active key is set
    CHECK_FALSE(mgr.active());
}

TEST_CASE("audit_key_manager::decode_key accepts hex and base64",
          "[security][audit_cipher]") {
    std::array<std::uint8_t, 32> buf{};

    // 32 bytes of 0xAB as hex
    const std::string hex(64, 'a');  // invalid hex letter set actually 'a'=10
    // Build proper hex input
    std::string hex_input;
    hex_input.reserve(64);
    for (int i = 0; i < 32; ++i) hex_input.append("ab");
    REQUIRE(audit_key_manager::decode_key(hex_input,
                                          buf.data(), buf.size()));
    for (auto b : buf) CHECK(b == 0xab);

    // Same 32 bytes as base64
    std::string b64 = detail::base64_encode(buf.data(), buf.size());
    std::array<std::uint8_t, 32> out{};
    REQUIRE(audit_key_manager::decode_key(b64, out.data(), out.size()));
    CHECK(out == buf);

    // Wrong length
    CHECK_FALSE(audit_key_manager::decode_key("deadbeef",
                                              buf.data(), buf.size()));
    // Garbage
    CHECK_FALSE(audit_key_manager::decode_key("$$$$",
                                              buf.data(), buf.size()));
}

// =============================================================================
// Cipher (only meaningful when OpenSSL is compiled in)
// =============================================================================

#ifdef PACS_WITH_DIGITAL_SIGNATURES

TEST_CASE("audit_log_cipher round-trips plaintext",
          "[security][audit_cipher]") {
    audit_key_manager mgr;
    mgr.register_key(make_test_key("k1", 0x42));
    audit_log_cipher cipher(mgr);

    const std::string pt = "<AuditMessage>payload-123</AuditMessage>";
    auto enc = cipher.encrypt_record(pt);
    REQUIRE(enc);
    const std::string& record = enc.value();

    CHECK(audit_log_cipher::looks_encrypted(record));
    CHECK(record.find(pt) == std::string::npos);  // not plaintext

    auto dec = cipher.decrypt_record(record);
    REQUIRE(dec);
    CHECK(dec.value() == pt);
}

TEST_CASE("audit_log_cipher produces different IVs for identical plaintext",
          "[security][audit_cipher]") {
    audit_key_manager mgr;
    mgr.register_key(make_test_key("k1", 0x01));
    audit_log_cipher cipher(mgr);

    const std::string pt = "same payload";
    auto r1 = cipher.encrypt_record(pt);
    auto r2 = cipher.encrypt_record(pt);
    REQUIRE(r1);
    REQUIRE(r2);
    CHECK(r1.value() != r2.value());
}

TEST_CASE("audit_log_cipher rejects tampered ciphertext",
          "[security][audit_cipher]") {
    audit_key_manager mgr;
    mgr.register_key(make_test_key("k1", 0x77));
    audit_log_cipher cipher(mgr);

    auto enc = cipher.encrypt_record("tamper me");
    REQUIRE(enc);
    std::string rec = enc.value();

    // Flip a ciphertext byte. Layout: tag|ver|id|iv|CT|tag|hmac
    // Find the 4th '|' and mutate the first ciphertext character.
    int pipes = 0;
    std::size_t ct_start = std::string::npos;
    for (std::size_t i = 0; i < rec.size(); ++i) {
        if (rec[i] == '|') {
            if (++pipes == 4) {
                ct_start = i + 1;
                break;
            }
        }
    }
    REQUIRE(ct_start != std::string::npos);
    rec[ct_start] = (rec[ct_start] == 'A' ? 'B' : 'A');

    auto dec = cipher.decrypt_record(rec);
    CHECK_FALSE(dec);
}

TEST_CASE("audit_log_cipher rejects tampered HMAC",
          "[security][audit_cipher]") {
    audit_key_manager mgr;
    mgr.register_key(make_test_key("k1", 0x33));
    audit_log_cipher cipher(mgr);

    auto enc = cipher.encrypt_record("hmac guard");
    REQUIRE(enc);
    std::string rec = enc.value();

    // Flip last char of record (part of the HMAC)
    REQUIRE(!rec.empty());
    char& last = rec.back();
    // Stay in base64 alphabet to keep parsing succeeding so we test MAC, not parser
    last = (last == 'A' ? 'B' : 'A');

    auto dec = cipher.decrypt_record(rec);
    CHECK_FALSE(dec);
}

TEST_CASE("audit_log_cipher rejects unknown key id",
          "[security][audit_cipher]") {
    audit_key_manager mgr;
    mgr.register_key(make_test_key("writer", 0x11));
    audit_log_cipher writer(mgr);

    auto enc = writer.encrypt_record("x");
    REQUIRE(enc);

    // Fresh manager without the "writer" key
    audit_key_manager other;
    other.register_key(make_test_key("someone_else", 0x22));
    audit_log_cipher reader(other);

    auto dec = reader.decrypt_record(enc.value());
    CHECK_FALSE(dec);
}

TEST_CASE("audit_log_cipher supports key rotation",
          "[security][audit_cipher]") {
    audit_key_manager mgr;

    // Phase 1: write with key v1
    mgr.register_key(make_test_key("v1", 0x40));
    audit_log_cipher cipher(mgr);
    auto old_rec = cipher.encrypt_record("legacy message");
    REQUIRE(old_rec);

    // Phase 2: rotate to v2, keep v1 for decryption of historical records
    mgr.register_key(make_test_key("v2", 0x50));
    REQUIRE(mgr.set_active("v2"));
    auto new_rec = cipher.encrypt_record("post-rotation message");
    REQUIRE(new_rec);

    // Sanity: new records embed v2
    CHECK(new_rec.value().find("|v2|") != std::string::npos);
    CHECK(old_rec.value().find("|v1|") != std::string::npos);

    // Both still decrypt
    auto old_pt = cipher.decrypt_record(old_rec.value());
    REQUIRE(old_pt);
    CHECK(old_pt.value() == "legacy message");

    auto new_pt = cipher.decrypt_record(new_rec.value());
    REQUIRE(new_pt);
    CHECK(new_pt.value() == "post-rotation message");

    // Phase 3: retire v1 — historical records become undecryptable
    mgr.forget("v1");
    auto lost = cipher.decrypt_record(old_rec.value());
    CHECK_FALSE(lost);

    // But v2 still works
    auto still = cipher.decrypt_record(new_rec.value());
    REQUIRE(still);
    CHECK(still.value() == "post-rotation message");
}

TEST_CASE("audit_log_cipher rejects malformed records",
          "[security][audit_cipher]") {
    audit_key_manager mgr;
    mgr.register_key(make_test_key("k1", 0x09));
    audit_log_cipher cipher(mgr);

    CHECK_FALSE(cipher.decrypt_record(""));
    CHECK_FALSE(cipher.decrypt_record("not-a-record"));
    CHECK_FALSE(cipher.decrypt_record("PACSAUDIT|v1|k1|AA|BB|CC"));
    CHECK_FALSE(cipher.decrypt_record("PACSAUDIT|v0|k1|AA|BB|CC|DD"));
}

TEST_CASE("audit_log_cipher fails gracefully without an active key",
          "[security][audit_cipher]") {
    audit_key_manager mgr;
    audit_log_cipher cipher(mgr);

    auto enc = cipher.encrypt_record("anything");
    CHECK_FALSE(enc);
}

TEST_CASE("audit_log_cipher tolerates trailing newline",
          "[security][audit_cipher]") {
    audit_key_manager mgr;
    mgr.register_key(make_test_key("k1", 0x66));
    audit_log_cipher cipher(mgr);

    auto enc = cipher.encrypt_record("with newline");
    REQUIRE(enc);
    std::string with_nl = enc.value() + "\n";
    auto dec = cipher.decrypt_record(with_nl);
    REQUIRE(dec);
    CHECK(dec.value() == "with newline");
}

TEST_CASE("verify_record returns ok for valid records",
          "[security][audit_cipher]") {
    audit_key_manager mgr;
    mgr.register_key(make_test_key("k1", 0x55));
    audit_log_cipher cipher(mgr);

    auto enc = cipher.encrypt_record("verifiable");
    REQUIRE(enc);
    CHECK(cipher.verify_record(enc.value()));
}

#endif  // PACS_WITH_DIGITAL_SIGNATURES

TEST_CASE("looks_encrypted matches format prefix",
          "[security][audit_cipher]") {
    CHECK(audit_log_cipher::looks_encrypted("PACSAUDIT|v1|foo"));
    CHECK_FALSE(audit_log_cipher::looks_encrypted("plain text"));
    CHECK_FALSE(audit_log_cipher::looks_encrypted("PACSAUDIT"));
    CHECK_FALSE(audit_log_cipher::looks_encrypted("PACSAUDIT|v2|foo"));
}

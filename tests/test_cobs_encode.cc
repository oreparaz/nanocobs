#include "../cobs.h"
#include "catch.hpp"

#include <cstring>

using byte_vec_t = std::vector< unsigned char >;

TEST_CASE("Encoding validation", "[cobs_encode]") {
  unsigned char enc[32], dec[32];
  unsigned const enc_n = sizeof(enc);
  unsigned const dec_n = sizeof(dec);
  unsigned enc_len;

  SECTION("Null buffer pointer") {
    REQUIRE( cobs_encode(nullptr, dec_n, enc, enc_n, &enc_len) ==
             COBS_RET_ERR_BAD_ARG );
    REQUIRE( cobs_encode(dec, dec_n, nullptr, enc_n, &enc_len) ==
             COBS_RET_ERR_BAD_ARG );
    REQUIRE( cobs_encode(dec, dec_n, enc, enc_n, nullptr) ==
             COBS_RET_ERR_BAD_ARG );
  }

  SECTION("Invalid enc_max") {
    REQUIRE( cobs_encode(dec, dec_n, enc, 0, &enc_len) ==
             COBS_RET_ERR_BAD_ARG );
    REQUIRE( cobs_encode(dec, dec_n, enc, 1, &enc_len) ==
             COBS_RET_ERR_BAD_ARG );
    REQUIRE( cobs_encode(dec, dec_n, enc, dec_n - 2, &enc_len) ==
             COBS_RET_ERR_BAD_ARG );
    REQUIRE( cobs_encode(dec, dec_n, enc, dec_n - 1, &enc_len) ==
             COBS_RET_ERR_BAD_ARG );
  }
}

TEST_CASE("Simple encodings", "[cobs_encode]") {
  unsigned char dec[16];
  unsigned char enc[16];
  unsigned enc_len;

  SECTION("Empty") {
    REQUIRE( cobs_encode(&dec, 0, enc, sizeof(enc), &enc_len) ==
             COBS_RET_SUCCESS );
    REQUIRE( byte_vec_t(enc, enc + enc_len) == byte_vec_t{0x01, 0x00} );
  }

  SECTION("1 nonzero byte") {
    dec[0] = 0x34;
    REQUIRE( cobs_encode(&dec, 1, enc, sizeof(enc), &enc_len) ==
             COBS_RET_SUCCESS );
    REQUIRE( byte_vec_t(enc, enc + enc_len) == byte_vec_t{0x02, 0x34, 0x00} );
  }

  SECTION("2 nonzero bytes") {
    dec[0] = 0x34;
    dec[1] = 0x56;
    REQUIRE( cobs_encode(&dec, 2, enc, sizeof(enc), &enc_len) ==
             COBS_RET_SUCCESS );
    REQUIRE( byte_vec_t(enc, enc + enc_len) ==
             byte_vec_t{0x03, 0x34, 0x56, 0x00} );
  }

  SECTION("8 nonzero bytes") {
    dec[0] = 0x12;
    dec[1] = 0x34;
    dec[2] = 0x56;
    dec[3] = 0x78;
    dec[4] = 0x9A;
    dec[5] = 0xBC;
    dec[6] = 0xDE;
    dec[7] = 0xFF;
    REQUIRE( cobs_encode(&dec, 8, enc, sizeof(enc), &enc_len) ==
             COBS_RET_SUCCESS );
    REQUIRE( byte_vec_t(enc, enc + enc_len) ==
             byte_vec_t{0x09,0x12,0x34,0x56,0x78,0x9A,0xBC,0xDE,0xFF,0x00} );
  }

  SECTION("1 zero byte") {
    dec[0] = 0x00;
    REQUIRE( cobs_encode(&dec, 1, enc, sizeof(enc), &enc_len) ==
             COBS_RET_SUCCESS );
    REQUIRE( byte_vec_t(enc, enc + enc_len) == byte_vec_t{0x01, 0x01, 0x00} );
  }

  SECTION("2 zero bytes") {
    dec[0] = 0x00;
    dec[1] = 0x00;
    REQUIRE( cobs_encode(&dec, 2, enc, sizeof(enc), &enc_len) ==
             COBS_RET_SUCCESS );
    REQUIRE( byte_vec_t(enc, enc + enc_len) ==
             byte_vec_t{0x01, 0x01, 0x01, 0x00} );
  }

  SECTION("8 nonzero bytes") {
    memset(dec, 0, 8);
    REQUIRE( cobs_encode(&dec, 8, enc, sizeof(enc), &enc_len) ==
             COBS_RET_SUCCESS );
    REQUIRE( byte_vec_t(enc, enc + enc_len) ==
             byte_vec_t{0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x00} );
  }

  SECTION("4 alternating zero/nonzero bytes") {
    dec[0] = 0x00;
    dec[1] = 0x11;
    dec[2] = 0x00;
    dec[3] = 0x22;
    REQUIRE( cobs_encode(&dec, 4, enc, sizeof(enc), &enc_len) ==
             COBS_RET_SUCCESS );
    REQUIRE( byte_vec_t(enc, enc + enc_len) ==
             byte_vec_t{0x01, 0x02, 0x11, 0x02, 0x22, 0x00} );
  }
}

namespace {
byte_vec_t encode(byte_vec_t const &decoded) {
  byte_vec_t enc(cobs_encode_max(static_cast< unsigned >(decoded.size())));
  unsigned enc_len;
  REQUIRE( cobs_encode(decoded.data(),
                       static_cast< unsigned >(decoded.size()),
                       enc.data(),
                       static_cast< unsigned >(enc.size()),
                       &enc_len) == COBS_RET_SUCCESS );
  return byte_vec_t(enc.begin(), enc.begin() + enc_len);
}
}

TEST_CASE("0xFF single code-block case", "[cobs_encode]") {
  byte_vec_t expected(255, 0x01);
  expected[0] = 0xFF;
  expected.push_back(0x00);
  REQUIRE( encode(byte_vec_t(254, 0x01)) == expected );
}

TEST_CASE("Longer payloads", "[cobs_encode]") {
  SECTION("255 non-zero bytes") {
    byte_vec_t expected{0xFF};
    expected.insert(std::end(expected), 254, 0x01);
    expected.push_back(0x02); // code: distance to next block
    expected.push_back(0x01);
    expected.push_back(0x00);
    REQUIRE( encode(byte_vec_t(255, 0x01)) == expected );
  }

  SECTION("255 zero bytes") {
    byte_vec_t expected(256, 0x01);
    expected.push_back(0x00);
    REQUIRE( encode(byte_vec_t(255, 0x00)) == expected );
  }

  SECTION("1024 non-zero bytes") {
    byte_vec_t expected;
    for (auto i = 0u; i < 1024 / 254; ++i) {
      expected.push_back(0xFF);
      expected.insert(std::end(expected), 254, '!');
    }
    expected.push_back((1024 % 254) + 1);
    expected.insert(std::end(expected), (1024 % 254), '!');
    expected.push_back(0x00);
    REQUIRE( encode(byte_vec_t(1024, '!')) == expected );
  }

  SECTION("1024 zero bytes") {
    byte_vec_t expected(1025, 0x01);
    expected.push_back(0x00);
    REQUIRE( encode(byte_vec_t(1024, 0x00)) == expected );
  }

  SECTION("1024 every other byte is zero") {
    byte_vec_t dec;
    for (auto i = 0u; i < 1024; ++i) {
      dec.push_back(i & 1);
    }

    byte_vec_t expected;
    for (auto i = 0u; i <= 1024; ++i) {
      expected.push_back((i & 1) ? 2 : 1);
    }
    expected.push_back(0x00);
    REQUIRE( encode(dec) == expected );
  }
}

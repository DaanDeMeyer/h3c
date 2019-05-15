#pragma once

#include <h3c/error.h>
#include <h3c/export.h>

#include <stddef.h>
#include <stdint.h>

// https://quicwg.org/base-drafts/draft-ietf-quic-transport.html#integer-encoding

#define H3C_VARINT_MAX ((0x40ULL << 56) - 1)

#define H3C_VARINT_UINT8_SIZE 1
#define H3C_VARINT_UINT16_SIZE 2
#define H3C_VARINT_UINT32_SIZE 4
#define H3C_VARINT_UINT64_SIZE 8

#ifdef __cplusplus
extern "C" {
#endif

typedef struct h3c_log_t h3c_log_t;

H3C_EXPORT size_t h3c_varint_encoded_size(uint64_t varint);

H3C_EXPORT H3C_ERROR h3c_varint_encode(uint8_t *dest,
                                       size_t size,
                                       uint64_t varint,
                                       size_t *encoded_size,
                                       h3c_log_t *log);

H3C_EXPORT H3C_ERROR h3c_varint_decode(const uint8_t *src,
                                       size_t size,
                                       uint64_t *varint,
                                       size_t *encoded_size,
                                       h3c_log_t *log);

#ifdef __cplusplus
}
#endif

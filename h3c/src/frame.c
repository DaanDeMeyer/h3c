#include <h3c/frame.h>
#include <h3c/varint.h>

#include <assert.h>
#include <string.h>

// We use a lot of macros in this file because after each serialize/parse, we
// have to check the result and update a lot of values. Macros provide us with a
// concise way to accomplish this. Using functions instead of macros doesn't
// work because we would still have to check the return value of the function
// each time we call it, where macros allow us to return directly from the
// parent function.

#define TRY_VARINT_SIZE(value)                                                 \
  {                                                                            \
    size_t varint_size = 0;                                                    \
    H3C_ERROR error = h3c_varint_serialize(NULL, 0, (value), &varint_size);    \
    if (error) {                                                               \
      return error;                                                            \
    }                                                                          \
                                                                               \
    *size += varint_size;                                                      \
  }                                                                            \
  (void) 0

#define TRY_SETTING_SIZE(id, value)                                            \
  if ((value) > id##_MAX) {                                                    \
    return H3C_ERROR_SETTING_OVERFLOW;                                         \
  }                                                                            \
                                                                               \
  TRY_VARINT_SIZE((id));                                                       \
  TRY_VARINT_SIZE((value));                                                    \
  (void) 0

static H3C_ERROR frame_payload_size(const h3c_frame_t *frame, uint64_t *size)
{
  assert(frame);

  *size = 0;

  switch (frame->type) {
    case H3C_FRAME_DATA:
      *size += frame->data.payload.size;
      break;
    case H3C_FRAME_HEADERS:
      *size += frame->headers.header_block.size;
      break;
    case H3C_FRAME_PRIORITY:
      (*size)++; // PT size + DT size + Empty size = 1 byte. See
                 // https://quicwg.org/base-drafts/draft-ietf-quic-http.html#frame-priority
      TRY_VARINT_SIZE(frame->priority.prioritized_element_id);
      TRY_VARINT_SIZE(frame->priority.element_dependency_id);
      (*size)++; // Weight
      break;
    case H3C_FRAME_CANCEL_PUSH:
      TRY_VARINT_SIZE(frame->cancel_push.push_id);
      break;
    case H3C_FRAME_SETTINGS:
// GCC warns us when a setting max is the same as the max size for the setting's
// integer type which we choose to ignore since `TRY_SETTING_SIZE` has to work
// with any kind of maximum.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wtype-limits"
      TRY_SETTING_SIZE(H3C_SETTINGS_MAX_HEADER_LIST_SIZE,
                       frame->settings.max_header_list_size);
      TRY_SETTING_SIZE(H3C_SETTINGS_NUM_PLACEHOLDERS,
                       frame->settings.num_placeholders);
      TRY_SETTING_SIZE(H3C_SETTINGS_QPACK_MAX_TABLE_CAPACITY,
                       frame->settings.qpack_max_table_capacity);
      TRY_SETTING_SIZE(H3C_SETTINGS_QPACK_BLOCKED_STREAMS,
                       frame->settings.qpack_blocked_streams);
#pragma GCC diagnostic pop
      break;
    case H3C_FRAME_PUSH_PROMISE:
      TRY_VARINT_SIZE(frame->push_promise.push_id);
      *size += frame->push_promise.header_block.size;
      break;
    case H3C_FRAME_GOAWAY:
      TRY_VARINT_SIZE(frame->goaway.stream_id);
      break;
    case H3C_FRAME_MAX_PUSH_ID:
      TRY_VARINT_SIZE(frame->max_push_id.push_id);
      break;
    case H3C_FRAME_DUPLICATE_PUSH:
      TRY_VARINT_SIZE(frame->duplicate_push.push_id);
      break;
  }

  if (*size > H3C_VARINT_MAX) {
    return H3C_ERROR_VARINT_OVERFLOW;
  }

  return H3C_SUCCESS;
}

#define TRY_VARINT_SERIALIZE(value)                                            \
  {                                                                            \
    size_t varint_size = 0;                                                    \
    H3C_ERROR error = h3c_varint_serialize(dest, size, (value), &varint_size); \
    if (error) {                                                               \
      return error;                                                            \
    }                                                                          \
                                                                               \
    if (dest) {                                                                \
      dest += varint_size;                                                     \
      size -= varint_size;                                                     \
    }                                                                          \
                                                                               \
    *frame_size += varint_size;                                                \
  }                                                                            \
  (void) 0

#define TRY_UINT8_SERIALIZE(value)                                             \
  if (dest) {                                                                  \
    if (size == 0) {                                                           \
      return H3C_ERROR_BUFFER_TOO_SMALL;                                       \
    }                                                                          \
                                                                               \
    *dest = (value);                                                           \
    dest++;                                                                    \
    size--;                                                                    \
  }                                                                            \
                                                                               \
  (*frame_size)++;                                                             \
  (void) 0

#define TRY_SETTING_SERIALIZE(id, value)                                       \
  TRY_VARINT_SERIALIZE((id));                                                  \
  TRY_VARINT_SERIALIZE((value));                                               \
  (void) 0

H3C_ERROR h3c_frame_serialize(uint8_t *dest,
                              size_t size,
                              const h3c_frame_t *frame,
                              size_t *frame_size)
{
  assert(frame);
  assert(frame_size);

  *frame_size = 0;

  uint64_t payload_size = 0;
  H3C_ERROR error = frame_payload_size(frame, &payload_size);
  if (error) {
    return error;
  }

  TRY_VARINT_SERIALIZE(frame->type);
  TRY_VARINT_SERIALIZE(payload_size);

  switch (frame->type) {
    case H3C_FRAME_DATA:
      break;
    case H3C_FRAME_HEADERS:
      break;
    case H3C_FRAME_PRIORITY:;
      uint8_t byte = 0;
      byte |= (uint8_t)(frame->priority.prioritized_element_type << 6);
      byte |= (uint8_t)(frame->priority.element_dependency_type << 4);
      byte &= 0xf0;
      TRY_UINT8_SERIALIZE(byte);

      TRY_VARINT_SERIALIZE(frame->priority.prioritized_element_id);
      TRY_VARINT_SERIALIZE(frame->priority.element_dependency_id);

      TRY_UINT8_SERIALIZE(frame->priority.weight);
      break;
    case H3C_FRAME_CANCEL_PUSH:
      TRY_VARINT_SERIALIZE(frame->cancel_push.push_id);
      break;
    case H3C_FRAME_SETTINGS:
      TRY_SETTING_SERIALIZE(H3C_SETTINGS_MAX_HEADER_LIST_SIZE,
                            frame->settings.max_header_list_size);
      TRY_SETTING_SERIALIZE(H3C_SETTINGS_NUM_PLACEHOLDERS,
                            frame->settings.num_placeholders);
      TRY_SETTING_SERIALIZE(H3C_SETTINGS_QPACK_MAX_TABLE_CAPACITY,
                            frame->settings.qpack_max_table_capacity);
      TRY_SETTING_SERIALIZE(H3C_SETTINGS_QPACK_BLOCKED_STREAMS,
                            frame->settings.qpack_blocked_streams);
      break;
    case H3C_FRAME_PUSH_PROMISE:
      TRY_VARINT_SERIALIZE(frame->push_promise.push_id);
      break;
    case H3C_FRAME_GOAWAY:
      TRY_VARINT_SERIALIZE(frame->goaway.stream_id);
      break;
    case H3C_FRAME_MAX_PUSH_ID:
      TRY_VARINT_SERIALIZE(frame->max_push_id.push_id);
      break;
    case H3C_FRAME_DUPLICATE_PUSH:
      TRY_VARINT_SERIALIZE(frame->duplicate_push.push_id);
      break;
  }

  return H3C_SUCCESS;
}

#define TRY_VARINT_PARSE_1(value)                                              \
  {                                                                            \
    size_t varint_size = 0;                                                    \
    H3C_ERROR error = h3c_varint_parse(src, size, &(value), &varint_size);     \
    if (error) {                                                               \
      return H3C_ERROR_INCOMPLETE;                                             \
    }                                                                          \
                                                                               \
    src += varint_size;                                                        \
    size -= varint_size;                                                       \
    *frame_size += varint_size;                                                \
  }                                                                            \
  (void) 0

// Additionally checks and updates `frame_length` compared to
// `TRY_VARINT_PARSE_1`.
#define TRY_VARINT_PARSE_2(value)                                              \
  {                                                                            \
    size_t varint_size = 0;                                                    \
    H3C_ERROR error = h3c_varint_parse(src, size, &(value), &varint_size);     \
    if (error) {                                                               \
      return H3C_ERROR_INCOMPLETE;                                             \
    }                                                                          \
                                                                               \
    if (varint_size > payload_size) {                                          \
      return H3C_ERROR_MALFORMED_FRAME;                                        \
    }                                                                          \
                                                                               \
    src += varint_size;                                                        \
    size -= varint_size;                                                       \
    *frame_size += varint_size;                                                \
    payload_size -= varint_size;                                               \
  }                                                                            \
  (void) 0

#define BUFFER_PARSE(buffer)                                                   \
  (buffer).size = payload_size;                                                \
  payload_size -= payload_size;                                                \
  (void) 0

#define TRY_UINT8_PARSE(value)                                                 \
  if (size == 0) {                                                             \
    return H3C_ERROR_INCOMPLETE;                                               \
  }                                                                            \
                                                                               \
  if (payload_size == 0) {                                                     \
    return H3C_ERROR_MALFORMED_FRAME;                                          \
  }                                                                            \
                                                                               \
  (value) = *src;                                                              \
                                                                               \
  src++;                                                                       \
  size--;                                                                      \
  (*frame_size)++;                                                             \
  payload_size--;                                                              \
  (void) 0

#define TRY_SETTING_PARSE(id, value, type)                                     \
  {                                                                            \
    uint64_t varint = 0;                                                       \
    TRY_VARINT_PARSE_2(varint);                                                \
                                                                               \
    if (varint > id##_MAX) {                                                   \
      return H3C_ERROR_MALFORMED_FRAME;                                        \
    }                                                                          \
                                                                               \
    (value) = (type) varint;                                                   \
  }                                                                            \
  (void) 0

H3C_ERROR h3c_frame_parse(const uint8_t *src,
                          size_t size,
                          h3c_frame_t *frame,
                          size_t *frame_size)
{
  assert(src);
  assert(frame);
  assert(frame_size);

  *frame_size = 0;

  TRY_VARINT_PARSE_1(frame->type);

  uint64_t payload_size = 0;
  TRY_VARINT_PARSE_1(payload_size);

  switch (frame->type) {
    case H3C_FRAME_DATA:
      BUFFER_PARSE(frame->data.payload);
      break;
    case H3C_FRAME_HEADERS:
      BUFFER_PARSE(frame->headers.header_block);
      break;
    case H3C_FRAME_PRIORITY:;
      uint8_t byte = 0;
      TRY_UINT8_PARSE(byte);
      frame->priority.prioritized_element_type = byte >> 6;
      frame->priority.element_dependency_type = (byte >> 4) & 0x03;

      TRY_VARINT_PARSE_2(frame->priority.prioritized_element_id);
      TRY_VARINT_PARSE_2(frame->priority.element_dependency_id);

      TRY_UINT8_PARSE(frame->priority.weight);
      break;
    case H3C_FRAME_CANCEL_PUSH:
      TRY_VARINT_PARSE_2(frame->cancel_push.push_id);
      break;
    case H3C_FRAME_SETTINGS:
      frame->settings = h3c_settings_default;

      while (payload_size > 0) {
        uint64_t id = 0;
        TRY_VARINT_PARSE_2(id);

        switch (id) {
          case H3C_SETTINGS_MAX_HEADER_LIST_SIZE:
            TRY_SETTING_PARSE(H3C_SETTINGS_MAX_HEADER_LIST_SIZE,
                              frame->settings.max_header_list_size, uint64_t);
            break;
          case H3C_SETTINGS_NUM_PLACEHOLDERS:
            TRY_SETTING_PARSE(H3C_SETTINGS_NUM_PLACEHOLDERS,
                              frame->settings.num_placeholders, uint64_t);
            break;
          case H3C_SETTINGS_QPACK_MAX_TABLE_CAPACITY:
            TRY_SETTING_PARSE(H3C_SETTINGS_QPACK_MAX_TABLE_CAPACITY,
                              frame->settings.qpack_max_table_capacity,
                              uint32_t);
            break;
          case H3C_SETTINGS_QPACK_BLOCKED_STREAMS:
            TRY_SETTING_PARSE(H3C_SETTINGS_QPACK_BLOCKED_STREAMS,
                              frame->settings.qpack_blocked_streams, uint16_t);
            break;
          default:;
            // Unknown setting id => ignore its value
            uint64_t value = 0;
            TRY_VARINT_PARSE_2(value);
        }
      }
      break;
    case H3C_FRAME_PUSH_PROMISE:
      TRY_VARINT_PARSE_2(frame->push_promise.push_id);
      BUFFER_PARSE(frame->push_promise.header_block);
      break;
    case H3C_FRAME_GOAWAY:
      TRY_VARINT_PARSE_2(frame->goaway.stream_id);
      break;
    case H3C_FRAME_MAX_PUSH_ID:
      TRY_VARINT_PARSE_2(frame->max_push_id.push_id);
      break;
    case H3C_FRAME_DUPLICATE_PUSH:
      TRY_VARINT_PARSE_2(frame->duplicate_push.push_id);
      break;
  }

  if (payload_size > 0) {
    return H3C_ERROR_MALFORMED_FRAME;
  }

  return H3C_SUCCESS;
}

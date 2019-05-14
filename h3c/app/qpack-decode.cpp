#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include <h3c/log/fprintf.h>
#include <h3c/qpack.h>

H3C_ERROR decode(uint8_t *src,
                 size_t size,
                 uint64_t *stream_id,
                 std::vector<std::pair<std::string, std::string>> &headers,
                 size_t *encoded_size,
                 h3c_log_t *log)
{
  *encoded_size = 0;

  if (sizeof(uint64_t) > size) {
    H3C_THROW(H3C_ERROR_INCOMPLETE, log);
  }

  *stream_id = static_cast<uint64_t>(src[0]) << 56 |
               static_cast<uint64_t>(src[1]) << 48 |
               static_cast<uint64_t>(src[2]) << 40 |
               static_cast<uint64_t>(src[3]) << 32 |
               static_cast<uint64_t>(src[4]) << 24 |
               static_cast<uint64_t>(src[5]) << 16 |
               static_cast<uint64_t>(src[6]) << 8 |
               static_cast<uint64_t>(src[7]) << 0;
  src += sizeof(uint64_t);
  size -= sizeof(uint64_t);
  *encoded_size += sizeof(uint64_t);

  if (sizeof(uint32_t) > size) {
    H3C_THROW(H3C_ERROR_INCOMPLETE, log);
  }

  size_t header_block_encoded_size = static_cast<uint32_t>(src[0]) << 24 |
                                     static_cast<uint32_t>(src[1]) << 16 |
                                     static_cast<uint32_t>(src[2]) << 8 |
                                     static_cast<uint32_t>(src[3]) << 0;
  src += sizeof(uint32_t);
  size -= sizeof(uint32_t);
  *encoded_size += sizeof(uint32_t);

  if (header_block_encoded_size > size) {
    H3C_THROW(H3C_ERROR_INCOMPLETE, log);
  }

  size_t prefix_encoded_size = 0;
  H3C_ERROR error = h3c_qpack_decode_prefix(src, size, &prefix_encoded_size,
                                            log);
  if (error) {
    return error;
  }

  src += prefix_encoded_size;
  size -= prefix_encoded_size;
  header_block_encoded_size -= prefix_encoded_size;
  *encoded_size += prefix_encoded_size;

  h3c_qpack_decode_context_t context;
  error = h3c_qpack_decode_context_init(&context, log);
  if (error) {
    return error;
  }

  while (header_block_encoded_size > 0) {
    size_t header_encoded_size = 0;
    h3c_header_t header = {};

    error = h3c_qpack_decode(&context, src, size, &header, &header_encoded_size,
                             log);
    if (error) {
      return error;
    }

    std::string name(header.name.data, header.name.length);
    std::string value(header.value.data, header.value.length);

    headers.emplace_back(std::pair<std::string, std::string>{ name, value });

    src += header_encoded_size;
    size -= header_encoded_size;
    header_block_encoded_size -= header_encoded_size;
    *encoded_size += header_encoded_size;
  }

  h3c_qpack_decode_context_destroy(&context);

  return H3C_SUCCESS;
}

void write(std::ostream &dest,
           const std::vector<std::pair<std::string, std::string>> &headers)
{
  for (const auto &entry : headers) {
    dest << entry.first << '\t' << entry.second << '\n';
  }

  dest << '\n';
}

int main(int argc, char *argv[])
{
  if (argc < 3) {
    return 1;
  }

  h3c_log_fprintf_t fprintf_context = {};
  h3c_log_t log = { h3c_log_fprintf, &fprintf_context };

  std::vector<uint8_t> encoded;

  std::ifstream input;
  input.exceptions(std::ifstream::failbit | std::ifstream::badbit);

  // Open input file

  try {
    input.open(argv[1], std::ios::binary);
  } catch (const std::ios_base::failure &e) {
    H3C_LOG_ERROR(&log, "Error opening input file: %s", e.what());
    return 1;
  }

  // Read input

  try {
    input.seekg(0, std::ios::end);
    std::streamsize size = input.tellg();
    input.seekg(0, std::ios::beg);

    encoded = std::vector<uint8_t>(static_cast<size_t>(size));
    input.read(reinterpret_cast<char *>(encoded.data()), size); // NOLINT
  } catch (const std::ios_base::failure &e) {
    H3C_LOG_ERROR(&log, "Error reading input: %s", e.what());
    return 1;
  }

  std::ofstream output;
  output.exceptions(std::ifstream::failbit | std::ifstream::badbit);

  // Open output file

  try {
    output.open(argv[2], std::ios::trunc | std::ios::binary);
  } catch (const std::ios_base::failure &e) {
    H3C_LOG_ERROR(&log, "Error opening output file: %s", e.what());
    return 1;
  }

  // Decode input

  while (!encoded.empty()) {
    uint64_t stream_id = 0;
    std::vector<std::pair<std::string, std::string>> headers;
    size_t encoded_size = 0;

    H3C_ERROR error = decode(encoded.data(), encoded.size(), &stream_id,
                             headers, &encoded_size, &log);
    if (error) {
      return error;
    }

    // Write output

    try {
      write(output, headers);
    } catch (std::ios_base::failure &e) {
      H3C_LOG_ERROR(&log, "Error writing output: ", e.what());
      return 1;
    }

    encoded.erase(encoded.begin(),
                  encoded.begin() + static_cast<int32_t>(encoded_size));
  }

  return 0;
}

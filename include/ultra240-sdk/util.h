#pragma once

#include <cstdint>
#include <map>

namespace ultra::sdk::util {

  template<typename T>
  using HashMap = std::map<uint32_t, T>;

  uint32_t crc32(const char* string);

}

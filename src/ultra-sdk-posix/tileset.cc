#include <ultra240-sdk/tileset.h>

namespace ultra::sdk {

  std::string get_library_path(const char* name) {
    std::string string(name);
    auto path = string.substr(0, string.rfind('/') + 1);
    auto basename = string.substr(string.rfind('/') + 1, string.npos);
    auto short_name = basename.substr(0, basename.rfind('.'));
    return path + ".libs/lib" + short_name + ".so";
  }

}

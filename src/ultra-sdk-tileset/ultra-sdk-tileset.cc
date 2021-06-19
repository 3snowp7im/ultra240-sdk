/**
 * Compiles a tileset into an ULTRA240 binary.
 */
#include <fstream>
#include <iostream>
#include <ultra240-sdk/tileset.h>
#include <stdexcept>

static void print_usage(const char* self, std::ostream& out) {
  out << "Usage: " << self << " [-h] <in.tsx> <out.bin>" << std::endl;
}

int main(int argc, const char* argv[]) {
  // Check for help option.
  for (int i = 0; i < argc; i++) {
    std::string arg(argv[i]);
    if (arg == "-h" || arg == "--help") {
      print_usage(argv[0], std::cout);
      return 0;
    }
  }
  if (argc != 3) {
    print_usage(argv[0], std::cerr);
    return 1;
  }
  std::string path(argv[1]);
  auto prefix = path.substr(0, path.rfind("/"));
  if (prefix == path) {
    prefix = ".";
  }
  prefix += "/";
  auto tileset = ultra::sdk::read_tileset(argv[1]);
  // Get size of serialized tileset.
  size_t buf_size = 0;
  size_t size;
  ultra::sdk::write_tileset(
    tileset,
    nullptr,
    &size,
    nullptr,
    nullptr,
    nullptr
  );
  buf_size += size;
  buf_size += tileset.source.size() + 1;
  for (const auto& pair : tileset.tiles) {
    ultra::sdk::write_tileset_tile(
      pair.first,
      pair.second,
      nullptr,
      &size,
      nullptr,
      nullptr
    );
    buf_size += size;
    buf_size += pair.second.library.size() + 1;
  }
  for (const auto& pair : tileset.tiles) {
    for (const auto& pair : pair.second.collision_boxes) {
      write_tileset_tile_collision_box_type(
        pair.first,
        pair.second,
        nullptr,
        &size,
        nullptr
      );
      buf_size += size;
    }
  }
  for (const auto& pair : tileset.tiles) {
    for (const auto& pair : pair.second.collision_boxes) {
      for (const auto& pair : pair.second) {
        write_tileset_tile_collision_box_list(
          pair.first,
          pair.second,
          nullptr,
          &size
        );
        buf_size += size;
      }
    }
  }
  buf_size += tileset.library.size() + 1;
  for (const auto& pair : tileset.tiles) {
    buf_size += pair.second.library.size() + 1;
  }
  // Write serialized tileset.
  uint8_t buf[buf_size];
  uint8_t* p = buf;
  uint32_t* source_offset_entry;
  std::list<uint32_t*> tile_offset_entries;
  uint32_t* library_offset_entry;
  ultra::sdk::write_tileset(
    tileset,
    p,
    &size,
    &source_offset_entry,
    &tile_offset_entries,
    &library_offset_entry
  );
  p += size;
  *source_offset_entry = static_cast<uint32_t>(p - buf);
  std::copy(tileset.source.begin(), tileset.source.end(), p);
  p[tileset.source.size()] = '\0';
  p += tileset.source.size() + 1;
  std::list<uint32_t*> collision_box_type_offset_entries;
  std::list<uint32_t*> tile_library_offset_entries;
  for (const auto& pair : tileset.tiles) {
    *tile_offset_entries.front() = static_cast<uint32_t>(p - buf);
    tile_offset_entries.pop_front();
    uint32_t* library_offset_entry;
    ultra::sdk::write_tileset_tile(
      pair.first,
      pair.second,
      p,
      &size,
      &collision_box_type_offset_entries,
      &library_offset_entry
    );
    tile_library_offset_entries.push_back(library_offset_entry);
    p += size;
  }
  std::list<uint32_t*> collision_box_list_offset_entries;
  for (const auto& pair : tileset.tiles) {
    for (const auto& pair : pair.second.collision_boxes) {
      *collision_box_type_offset_entries.front() =
        static_cast<uint32_t>(p - buf);
      collision_box_type_offset_entries.pop_front();
      write_tileset_tile_collision_box_type(
        pair.first,
        pair.second,
        p,
        &size,
        &collision_box_list_offset_entries
      );
      p += size;
    }
  }
  for (const auto& pair : tileset.tiles) {
    for (const auto& pair : pair.second.collision_boxes) {
      for (const auto& pair : pair.second) {
        *collision_box_list_offset_entries.front() =
          static_cast<uint32_t>(p - buf);
        collision_box_list_offset_entries.pop_front();
        write_tileset_tile_collision_box_list(
          pair.first,
          pair.second,
          p,
          &size
        );
        p += size;
      }
    }
  }
  *library_offset_entry = static_cast<uint32_t>(p - buf);
  std::copy(tileset.library.begin(), tileset.library.end(), p);
  p[tileset.library.size()] = '\0';
  p += tileset.library.size() + 1;
  for (const auto& pair : tileset.tiles) {
    *tile_library_offset_entries.front() = static_cast<uint32_t>(p - buf);
    tile_library_offset_entries.pop_front();
    std::copy(pair.second.library.begin(), pair.second.library.end(), p);
    p[pair.second.library.size()] = '\0';
    p += pair.second.library.size() + 1;
  }
  // Write the binary format.
  std::ofstream out(argv[2]);
  if (!out.is_open()) {
    throw std::runtime_error("Could not open output file");
  }
  out.write(reinterpret_cast<char*>(buf), sizeof(buf));
  return 0;
}

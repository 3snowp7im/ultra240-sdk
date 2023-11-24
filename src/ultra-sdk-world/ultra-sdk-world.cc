/** Compile a world file into an ULTRA240 binary. */
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <json/json.h>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <ultra240-sdk/tileset.h>
#include <ultra240-sdk/util.h>
#include <queue>
#include <rapidxml/rapidxml.hpp>
#include <rapidxml/rapidxml_utils.hpp>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <vector>
#include <yaml-cpp/yaml.h>

#define FLIP_X 0x80000000
#define FLIP_Y 0x40000000

struct Tileset {
  ssize_t map_index;
  ssize_t entity_index;
  uint16_t first_gid;
  ultra::sdk::Tileset tileset;
};

typedef std::tuple<uint8_t, uint8_t> fraction_t;

struct Layer {
  uint32_t name;
  enum Type {
    Image,
    Bounds,
  } type;
  struct {
    fraction_t x, y;
  } parallax;
  std::vector<uint16_t> tiles;
};

struct Entity {
  uint32_t layer_name;
  uint16_t x, y;
  uint16_t w, h;
  uint16_t tile;
  std::string type;
  uint32_t state;
};

struct Map {
  int16_t x, y;
  uint16_t w, h;
  std::vector<uint32_t> properties;
  uint8_t entities_index;
  std::vector<Tileset> map_tilesets;
  std::vector<Tileset> entity_tilesets;
  std::vector<Layer> layers;
  std::vector<Entity> entities;
};

struct Point {
  int32_t x;
  int32_t y;
};

template<template<typename> typename T>
class Boundary : public T<Point> {
public:
  Boundary() : flags(0) {}
  uint8_t flags;
};

static long gcd(long a, long b) {
  if (a == 0) {
    return b;
  } else if (b == 0) {
    return a;
  }
  if (a < b) {
    return gcd(a, b % a);
  }
  return gcd(b, a % b);
}

static std::tuple<uint8_t, uint8_t> double_to_fraction(double input) {
  uint8_t integral = static_cast<uint8_t>(std::floor(input));
  double frac = input - integral;
  const long precision = 1000000000;
  long gcd_frac = gcd(std::round(frac * precision), precision);
  uint8_t denominator = precision / gcd_frac;
  uint8_t numerator = round(frac * precision) / gcd_frac;
  return std::make_tuple((integral * denominator) + numerator, denominator);
}

static Json::Value load_json(const char* path) {
  Json::CharReaderBuilder builder;
  std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
  Json::Value json;
  std::ifstream file(path);
  std::stringstream buffer;
  buffer << file.rdbuf();
  auto str = buffer.str();
  if (!reader->parse(str.c_str(), str.c_str() + str.size(), &json, nullptr)) {
    throw std::runtime_error("Could not parse json");
  }
  return json;
}

static rapidxml::file<> load_xml(
  const char* path,
  rapidxml::xml_document<>& doc
) {
  rapidxml::file<> file(path);
  doc.parse<0>(file.data());
  return file;
}

static void write_layer(
  const Layer& layer,
  uint16_t w,
  uint16_t h,
  uint8_t* buf,
  size_t* buf_size
) {
  uint8_t* p = buf;
  uint32_t* name = reinterpret_cast<uint32_t*>(p);
  p += sizeof(uint32_t);
  uint8_t* pxn = p;
  p += sizeof(uint8_t);
  uint8_t* pxd = p;
  p += sizeof(uint8_t);
  uint8_t* pyn = p;
  p += sizeof(uint8_t);
  uint8_t* pyd = p;
  p += sizeof(uint8_t);
  uint16_t* tiles = reinterpret_cast<uint16_t*>(p);
  p += w * h * sizeof(uint16_t);
  if (buf != nullptr) {
    *name = layer.name;
    *pxn = std::get<0>(layer.parallax.x);
    *pxd = std::get<1>(layer.parallax.x);
    *pyn = std::get<0>(layer.parallax.y);
    *pyd = std::get<1>(layer.parallax.y);
    for (const auto& tile : layer.tiles) {
      *tiles++ = tile;
    }
  }
  if (buf_size != nullptr) {
    *buf_size = p - buf;
  }
}

static uint16_t get_entity_type(const Entity& entity, YAML::Node& config) {
  if (!config["entity_types"].IsDefined()) {
    return 0;
  }
  if (!entity.type.size()) {
    return 0;
  }
  for (size_t i = 0; i < config["entity_types"].size(); i++) {
    if (entity.type == config["entity_types"][i].as<std::string>()) {
      return i + 1;
    }
  }
  return 0;
}

static bool is_indexed_entity(const Entity& entity, YAML::Node& config) {
  if (!config["indexed_entity_types"].IsDefined()) {
    return false;
  }
  if (!entity.type.size()) {
    return false;
  }
  for (size_t i = 0; i < config["indexed_entity_types"].size(); i++) {
    if (entity.type == config["indexed_entity_types"][i].as<std::string>()) {
      return true;
    }
  }
  return false;
}

static void write_entity(
  const Entity& entity,
  YAML::Node& config,
  uint16_t& entity_id,
  uint8_t* buf,
  size_t* buf_size
) {
  uint8_t* p = buf;
  uint32_t* layer_name = reinterpret_cast<uint32_t*>(p);
  p += sizeof(uint32_t);
  uint16_t* x = reinterpret_cast<uint16_t*>(p);
  p += sizeof(uint16_t);
  uint16_t* y = reinterpret_cast<uint16_t*>(p);
  p += sizeof(uint16_t);
  uint16_t* tile = reinterpret_cast<uint16_t*>(p);
  p += sizeof(uint16_t);
  uint16_t* type = reinterpret_cast<uint16_t*>(p);
  p += sizeof(uint16_t);
  uint16_t* id = reinterpret_cast<uint16_t*>(p);
  p += sizeof(uint16_t);
  uint32_t* state = reinterpret_cast<uint32_t*>(p);
  p += sizeof(uint32_t);
  if (buf != nullptr) {
    *layer_name = entity.layer_name;
    *x = entity.x;
    *y = entity.y;
    *tile = entity.tile;
    *type = get_entity_type(entity, config);
    if (is_indexed_entity(entity, config)) {
      *id = entity_id++;
    } else {
      *id = 0;
    }
    *state = entity.state;
  }
  if (buf_size != nullptr) {
    *buf_size = p - buf;
  }
}

static void write_map(
  const Map& map,
  YAML::Node& config,
  uint16_t& entity_id,
  uint32_t offset,
  uint8_t* buf,
  size_t* buf_size
) {
  std::list<uint32_t> map_tileset_offsets;
  std::list<uint32_t*> map_tileset_source_offset_entries;
  std::list<uint32_t*> map_tileset_tile_offset_entries;
  std::list<uint32_t*> map_tileset_library_offset_entries;
  std::list<uint8_t*> map_tileset_sources;
  std::list<uint8_t*> map_tileset_tiles;
  std::list<uint8_t*> map_tileset_tile_collision_box_types;
  std::list<uint8_t*> map_tileset_tile_collision_box_lists;
  std::list<uint8_t*> map_tileset_libraries;
  std::list<uint8_t*> map_tileset_tile_libraries;
  std::list<uint32_t> entity_tileset_offsets;
  std::list<uint32_t*> entity_tileset_source_offset_entries;
  std::list<uint32_t*> entity_tileset_tile_offset_entries;
  std::list<uint32_t*> entity_tileset_library_offset_entries;
  std::list<uint8_t*> entity_tileset_sources;
  std::list<uint8_t*> entity_tileset_tiles;
  std::list<uint8_t*> entity_tileset_tile_collision_box_types;
  std::list<uint8_t*> entity_tileset_tile_collision_box_lists;
  std::list<uint8_t*> entity_tileset_libraries;
  std::list<uint8_t*> entity_tileset_tile_libraries;
  uint8_t* p = buf;
  // Position.
  int16_t* x = reinterpret_cast<int16_t*>(p);
  p += sizeof(uint16_t);
  int16_t* y = reinterpret_cast<int16_t*>(p);
  p += sizeof(uint16_t);
  // Dimensions.
  uint16_t* w = reinterpret_cast<uint16_t*>(p);
  p += sizeof(uint16_t);
  uint16_t* h = reinterpret_cast<uint16_t*>(p);
  p += sizeof(uint16_t);
  // Properties.
  uint8_t* properties_count = p;
  p += sizeof(uint8_t);
  std::vector<uint32_t*> properties(map.properties.size());
  for (int i = 0; i < map.properties.size(); i++) {
    properties[i] = reinterpret_cast<uint32_t*>(p);
    p += sizeof(uint32_t);
  }
  // Map tileset count.
  uint8_t* MTSn = p;
  p += sizeof(uint8_t);
  // Map tileset offsets.
  std::list<uint32_t*> map_tileset_offset_entries;
  for (int i = 0; i < map.map_tilesets.size(); i++) {
    map_tileset_offset_entries.push_back(reinterpret_cast<uint32_t*>(p));
    p += sizeof(uint32_t);
  }
  // Entity tileset count.
  uint8_t* ETSn = p;
  p += sizeof(uint8_t);
  // Entity tileset offsets.
  std::list<uint32_t*> entity_tileset_offset_entries;
  for (int i = 0; i < map.entity_tilesets.size(); i++) {
    entity_tileset_offset_entries.push_back(reinterpret_cast<uint32_t*>(p));
    p += sizeof(uint32_t);
  }
  // Layer offsets.
  uint8_t* Ln = p;
  p += sizeof(uint8_t);
  std::queue<uint32_t*> layer_offset_entries;
  for (int i = 0; i < map.layers.size(); i++) {
    layer_offset_entries.push(reinterpret_cast<uint32_t*>(p));
    p += sizeof(uint32_t);
  }
  // Entity offsets.
  uint16_t* En = reinterpret_cast<uint16_t*>(p);
  p += sizeof(uint16_t);
  for (const auto& entity : map.entities) {
    size_t entity_size;
    write_entity(entity, config, entity_id, buf ? p : nullptr, &entity_size);
    p += entity_size;
  }
  // Sort entities by x and y.
  std::vector<uint16_t>
    x_sorted_min(map.entities.size()),
    x_sorted_max(map.entities.size()),
    y_sorted_min(map.entities.size()),
    y_sorted_max(map.entities.size());
  for (int i = 0; i < map.entities.size(); i++) {
    x_sorted_min[i] = x_sorted_max[i] = y_sorted_min[i] = y_sorted_max[i] = i;
  }
  std::sort(
    x_sorted_min.begin(),
    x_sorted_min.end(),
    [&](uint16_t a, uint16_t b) {
      auto a_min = map.entities[a].x;
      auto b_min = map.entities[b].x;
      return a_min < b_min;
    }
  );
  std::sort(
    x_sorted_max.begin(),
    x_sorted_max.end(),
    [&](uint16_t a, uint16_t b) {
      auto a_max = map.entities[a].x + map.entities[a].w;
      auto b_max = map.entities[b].x + map.entities[b].w;
      return a_max < b_max;
    }
  );
  std::sort(
    y_sorted_min.begin(),
    y_sorted_min.end(),
    [&](uint16_t a, uint16_t b) {
      auto a_min = map.entities[a].y;
      auto b_min = map.entities[b].y;
      return a_min < b_min;
    }
  );
  std::sort(
    y_sorted_max.begin(),
    y_sorted_max.end(),
    [&](uint16_t a, uint16_t b) {
      auto a_max = map.entities[a].y + map.entities[a].y;
      auto b_max = map.entities[b].y + map.entities[b].y;
      return a_max < b_max;
    }
  );
  // Sorted entity indexes.
  std::vector<uint16_t*> x_sorted_min_ptrs(map.entities.size());
  for (int i = 0; i < map.entities.size(); i++) {
    x_sorted_min_ptrs[i] = reinterpret_cast<uint16_t*>(p);
    p += sizeof(uint16_t);
  }
  std::vector<uint16_t*> x_sorted_max_ptrs(map.entities.size());
  for (int i = 0; i < map.entities.size(); i++) {
    x_sorted_max_ptrs[i] = reinterpret_cast<uint16_t*>(p);
    p += sizeof(uint16_t);
  }
  std::vector<uint16_t*> y_sorted_min_ptrs(map.entities.size());
  for (int i = 0; i < map.entities.size(); i++) {
    y_sorted_min_ptrs[i] = reinterpret_cast<uint16_t*>(p);
    p += sizeof(uint16_t);
  }
  std::vector<uint16_t*> y_sorted_max_ptrs(map.entities.size());
  for (int i = 0; i < map.entities.size(); i++) {
    y_sorted_max_ptrs[i] = reinterpret_cast<uint16_t*>(p);
    p += sizeof(uint16_t);
  }
  // Layers.
  std::queue<uint32_t> layer_offsets;
  for (const auto& layer : map.layers) {
    if (layer.type != Layer::Type::Bounds) {
      layer_offsets.push(offset + static_cast<uint32_t>(p - buf));
      size_t size;
      write_layer(
        layer,
        map.w,
        map.h,
        buf ? p : nullptr,
        &size
      );
      p += size;
    }
  }
  for (const auto& tileset : map.map_tilesets) {
    // Map tileset offsets.
    map_tileset_offsets.push_back(offset + static_cast<uint32_t>(p - buf));
    // Map tilesets.
    size_t size;
    uint32_t* source_offset_entry;
    uint32_t* library_offset_entry;
    ultra::sdk::write_tileset(
      tileset.tileset,
      buf ? p : nullptr,
      &size,
      &source_offset_entry,
      &map_tileset_tile_offset_entries,
      &library_offset_entry
    );
    map_tileset_source_offset_entries.push_back(source_offset_entry);
    map_tileset_library_offset_entries.push_back(library_offset_entry);
    p += size;
    // Map tileset sources.
    map_tileset_sources.push_back(p);
    p += tileset.tileset.source.size() + 1;
    for (const auto& pair : tileset.tileset.tiles) {
      // Map tiles.
      map_tileset_tiles.push_back(p);
      ultra::sdk::write_tileset_tile(
        pair.first,
        pair.second,
        nullptr,
        &size,
        nullptr,
        nullptr
      );
      p += size;
      // Map tile libraries.
      map_tileset_tile_libraries.push_back(p);
      p += pair.second.library.size() + 1;
      // Map tile collision box types.
      for (const auto& pair : pair.second.collision_boxes) {
        map_tileset_tile_collision_box_types.push_back(p);
        write_tileset_tile_collision_box_type(
          pair.first,
          pair.second,
          nullptr,
          &size,
          nullptr
        );
        p += size;
        // Map tile collision boxes.
        for (const auto& pair : pair.second) {
          map_tileset_tile_collision_box_lists.push_back(p);
          write_tileset_tile_collision_box_list(
            pair.first,
            pair.second,
            nullptr,
            &size
          );
          p += size;
        }
      }
    }
    // Map tileset libraries.
    map_tileset_libraries.push_back(p);
    p += tileset.tileset.library.size() + 1;
  }
  for (const auto& tileset : map.entity_tilesets) {
    bool found = false;
    auto it = map_tileset_offsets.begin();
    for (int i = 0; i < map.map_tilesets.size(); i++) {
      if (map.map_tilesets[i].tileset.source == tileset.tileset.source) {
        entity_tileset_offsets.push_back(*it);
        found = true;
        break;
      }
      it++;
    }
    if (!found) {
      // Entity tileset offsets.
      entity_tileset_offsets.push_back(offset + static_cast<uint32_t>(p - buf));
      // Entity tilesets.
      size_t size;
      uint32_t* source_offset_entry;
      uint32_t* library_offset_entry;
      ultra::sdk::write_tileset(
        tileset.tileset,
        buf ? p : nullptr,
        &size,
        &source_offset_entry,
        &entity_tileset_tile_offset_entries,
        &library_offset_entry
      );
      entity_tileset_source_offset_entries.push_back(source_offset_entry);
      entity_tileset_library_offset_entries.push_back(library_offset_entry);
      p += size;
      // Entity tileset sources.
      entity_tileset_sources.push_back(p);
      p += tileset.tileset.source.size() + 1;
      // Entity tileset libraries.
      entity_tileset_libraries.push_back(p);
      p += tileset.tileset.library.size() + 1;
      // Entity tiles.
      for (const auto& pair : tileset.tileset.tiles) {
        entity_tileset_tiles.push_back(p);
        ultra::sdk::write_tileset_tile(
          pair.first,
          pair.second,
          nullptr,
          &size,
          nullptr,
          nullptr
        );
        p += size;
        // Entity tile libraries.
        entity_tileset_tile_libraries.push_back(p);
        p += pair.second.library.size() + 1;
        // Entity tile collision box types.
        for (const auto& pair : pair.second.collision_boxes) {
          entity_tileset_tile_collision_box_types.push_back(p);
          write_tileset_tile_collision_box_type(
            pair.first,
            pair.second,
            nullptr,
            &size,
            nullptr
          );
          p += size;
          // Entity tile collision boxes.
          for (const auto& pair : pair.second) {
            entity_tileset_tile_collision_box_lists.push_back(p);
            write_tileset_tile_collision_box_list(
              pair.first,
              pair.second,
              nullptr,
              &size
            );
            p += size;
          }
        }
      }
    }
  }
  if (buf != nullptr) {
    // Position.
    *x = map.x;
    *y = map.y;
    // Dimensions.
    *w = map.w;
    *h = map.h;
    // Properties.
    *properties_count = map.properties.size() / 2;
    for (int i = 0; i < map.properties.size(); i++) {
      *properties[i] = map.properties[i];
    }
    // Map tileset count.
    *MTSn = map.map_tilesets.size();
    // Entity tileset count.
    *ETSn = map.entity_tilesets.size();
    // Layer count.
    *Ln = map.layers.size();
    // Entity count.
    *En = map.entities.size();
    // Sorted entity indexes.
    for (int i = 0; i < map.entities.size(); i++) {
      *x_sorted_min_ptrs[i] = x_sorted_min[i];
    }
    for (int i = 0; i < map.entities.size(); i++) {
      *x_sorted_max_ptrs[i] = x_sorted_max[i];
    }
    for (int i = 0; i < map.entities.size(); i++) {
      *y_sorted_min_ptrs[i] = y_sorted_min[i];
    }
    for (int i = 0; i < map.entities.size(); i++) {
      *y_sorted_max_ptrs[i] = y_sorted_max[i];
    }
    // Map tilesets offsets.
    for (int i = 0; i < map.map_tilesets.size(); i++) {
      *map_tileset_offset_entries.front() = map_tileset_offsets.front();
      map_tileset_offset_entries.pop_front();
      map_tileset_offsets.pop_front();
    }
    // Entity tilesets offsets.
    for (int i = 0; i < map.entity_tilesets.size(); i++) {
      *entity_tileset_offset_entries.front() = entity_tileset_offsets.front();
      entity_tileset_offset_entries.pop_front();
      entity_tileset_offsets.pop_front();
    }
    // Layer offsets.
    for (int i = 0; i < map.layers.size(); i++) {
      *layer_offset_entries.front() = layer_offsets.front();
      layer_offset_entries.pop();
      layer_offsets.pop();
    }
    for (const auto& tileset : map.map_tilesets) {
      uint8_t* p;
      // Map tileset source offsets.
      p = map_tileset_sources.front();
      map_tileset_sources.pop_front();
      *map_tileset_source_offset_entries.front() =
        offset + static_cast<uint32_t>(p - buf);
      map_tileset_source_offset_entries.pop_front();
      // Map tileset sources.
      std::copy(
        tileset.tileset.source.begin(),
        tileset.tileset.source.end(),
        p
      );
      p[tileset.tileset.source.size()] = '\0';
      std::list<uint32_t*> map_tileset_tile_library_offset_entries;
      std::list<uint32_t*> map_tileset_tile_collision_box_type_offset_entries;
      std::list<uint32_t*> map_tileset_tile_collision_box_list_offset_entries;
      // Map tileset library offsets.
      p = map_tileset_libraries.front();
      map_tileset_libraries.pop_front();
      *map_tileset_library_offset_entries.front() =
        offset + static_cast<uint32_t>(p - buf);
      map_tileset_library_offset_entries.pop_front();
      // Map tileset libraries.
      std::copy(
        tileset.tileset.library.begin(),
        tileset.tileset.library.end(),
        p
      );
      p[tileset.tileset.library.size()] = '\0';
      for (const auto& pair : tileset.tileset.tiles) {
        // Map tile offsets.
        p = map_tileset_tiles.front();
        map_tileset_tiles.pop_front();
        *map_tileset_tile_offset_entries.front() =
          offset + static_cast<uint32_t>(p - buf);
        map_tileset_tile_offset_entries.pop_front();
        // Map tiles.
        uint32_t* library_offset;
        ultra::sdk::write_tileset_tile(
          pair.first,
          pair.second,
          p,
          nullptr,
          &map_tileset_tile_collision_box_type_offset_entries,
          &library_offset
        );
        map_tileset_tile_library_offset_entries.push_back(library_offset);
        // Map tile library offsets.
        p = map_tileset_tile_libraries.front();
        map_tileset_tile_libraries.pop_front();
        *map_tileset_tile_library_offset_entries.front() =
          offset + static_cast<uint32_t>(p - buf);
        map_tileset_tile_library_offset_entries.pop_front();
        // Map tile libraries.
        std::copy(
          pair.second.library.begin(),
          pair.second.library.end(),
          p
        );
        p[pair.second.library.size()] = '\0';
        for (const auto& pair : pair.second.collision_boxes) {
          // Map tile collision box type offsets.
          p = map_tileset_tile_collision_box_types.front();
          map_tileset_tile_collision_box_types.pop_front();
          *map_tileset_tile_collision_box_type_offset_entries.front() =
            offset + static_cast<uint32_t>(p - buf);
          map_tileset_tile_collision_box_type_offset_entries.pop_front();
          // Map tile collision box types.
          write_tileset_tile_collision_box_type(
            pair.first,
            pair.second,
            p,
            nullptr,
            &map_tileset_tile_collision_box_list_offset_entries
          );
          for (const auto& pair : pair.second) {
            p = map_tileset_tile_collision_box_lists.front();
            // Map tile collision box offsets.
            map_tileset_tile_collision_box_lists.pop_front();
            *map_tileset_tile_collision_box_list_offset_entries.front() =
              offset + static_cast<uint32_t>(p - buf);
            map_tileset_tile_collision_box_list_offset_entries.pop_front();
            // Map tile collision boxes.
            write_tileset_tile_collision_box_list(
              pair.first,
              pair.second,
              p,
              nullptr
            );
          }
        }
      }
    }
    for (const auto& tileset : map.entity_tilesets) {
      bool found = false;
      for (int i = 0; i < map.map_tilesets.size(); i++) {
        if (map.map_tilesets[i].tileset.source == tileset.tileset.source) {
          found = true;
          break;
        }
      }
      if (!found) {
        uint8_t* p;
        // Entity tileset sources.
        p = entity_tileset_sources.front();
        entity_tileset_sources.pop_front();
        *entity_tileset_source_offset_entries.front() =
          offset + static_cast<uint32_t>(p - buf);
        entity_tileset_source_offset_entries.pop_front();
        // Entity tileset sources.
        std::copy(
          tileset.tileset.source.begin(),
          tileset.tileset.source.end(),
          p
        );
        p[tileset.tileset.source.size()] = '\0';
        // Entity tileset library offsets.
        p = entity_tileset_libraries.front();
        entity_tileset_libraries.pop_front();
        *entity_tileset_library_offset_entries.front() =
          offset + static_cast<uint32_t>(p - buf);
        entity_tileset_library_offset_entries.pop_front();
        // Entity tileset libraries.
        std::copy(
          tileset.tileset.library.begin(),
          tileset.tileset.library.end(),
          p
        );
        p[tileset.tileset.library.size()] = '\0';
        std::list<uint32_t*> entity_tileset_tile_library_offset_entries;
        std::list<uint32_t*>
          entity_tileset_tile_collision_box_type_offset_entries;
        std::list<uint32_t*>
          entity_tileset_tile_collision_box_list_offset_entries;
        for (const auto& pair : tileset.tileset.tiles) {
          // Entity tile offsets.
          uint8_t* p = entity_tileset_tiles.front();
          entity_tileset_tiles.pop_front();
          *entity_tileset_tile_offset_entries.front() =
            offset + static_cast<uint32_t>(p - buf);
          entity_tileset_tile_offset_entries.pop_front();
          // Entity tiles.
          uint32_t* library_offset;
          ultra::sdk::write_tileset_tile(
            pair.first,
            pair.second,
            p,
            nullptr,
            &entity_tileset_tile_collision_box_type_offset_entries,
            &library_offset
          );
          entity_tileset_tile_library_offset_entries.push_back(library_offset);
          // Entity tile library offsets.
          p = entity_tileset_tile_libraries.front();
          entity_tileset_tile_libraries.pop_front();
          *entity_tileset_tile_library_offset_entries.front() =
            offset + static_cast<uint32_t>(p - buf);
          entity_tileset_tile_library_offset_entries.pop_front();
          // Entity tile libraries.
          std::copy(
            pair.second.library.begin(),
            pair.second.library.end(),
            p
          );
          p[pair.second.library.size()] = '\0';
          for (const auto& pair : pair.second.collision_boxes) {
            // Entity tile collision box type offsets.
            p = entity_tileset_tile_collision_box_types.front();
            entity_tileset_tile_collision_box_types.pop_front();
            *entity_tileset_tile_collision_box_type_offset_entries.front() =
              offset + static_cast<uint32_t>(p - buf);
            entity_tileset_tile_collision_box_type_offset_entries.pop_front();
            // Entity tile collision box types.
            write_tileset_tile_collision_box_type(
              pair.first,
              pair.second,
              p,
              nullptr,
              &entity_tileset_tile_collision_box_list_offset_entries
            );
            for (const auto& pair : pair.second) {
              // Entity tile collision box offsets.
              p = entity_tileset_tile_collision_box_lists.front();
              entity_tileset_tile_collision_box_lists.pop_front();
              *entity_tileset_tile_collision_box_list_offset_entries.front() =
                offset + static_cast<uint32_t>(p - buf);
              entity_tileset_tile_collision_box_list_offset_entries.pop_front();
              // Entity tile collision boxes.
              write_tileset_tile_collision_box_list(
                pair.first,
                pair.second,
                p,
                nullptr
              );
            }
          }
        }
      }
    }
  }
  if (buf_size != nullptr) {
    *buf_size = p - buf;
  }
}

static void write_boundary(
  const Boundary<std::list>& boundary,
  uint8_t* buf,
  size_t* buf_size
) {
  uint8_t* p = buf;
  uint8_t* flags = reinterpret_cast<uint8_t*>(p);
  p += sizeof(uint8_t);
  uint16_t* BLn = reinterpret_cast<uint16_t*>(p);
  p += sizeof(uint16_t);
  std::queue<std::pair<int32_t*, int32_t*>> points;
  for (const auto& point : boundary) {
    int32_t* x = reinterpret_cast<int32_t*>(p);
    p += sizeof(int32_t);
    int32_t* y = reinterpret_cast<int32_t*>(p);
    p += sizeof(int32_t);
    points.push(std::make_pair(x, y));
  }
  if (buf != nullptr) {
    *flags = boundary.flags;
    *BLn = boundary.size();
    for (const auto& point : boundary) {
      auto ptrs = points.front();
      points.pop();
      *ptrs.first = point.x;
      *ptrs.second = point.y;
    }
  }
  if (buf_size != nullptr) {
    *buf_size = p - buf;
  }
}

static void write_world(
  const std::vector<Map>& maps,
  const std::list<Boundary<std::list>>& bounds,
  YAML::Node& config,
  uint8_t* buf,
  size_t* buf_size
) {
  uint8_t* p = buf;
  uint16_t* Mn = reinterpret_cast<uint16_t*>(p);
  p += sizeof(uint16_t);
  std::queue<uint32_t*> map_header_offset_entries;
  for (int i = 0; i < maps.size(); i++) {
    map_header_offset_entries.push(reinterpret_cast<uint32_t*>(p));
    p += sizeof(uint32_t);
  }
  uint16_t* Bn = reinterpret_cast<uint16_t*>(p);
  p += sizeof(uint16_t);
  std::queue<uint32_t*> boundary_offset_entries;
  for (int i = 0; i < bounds.size(); i++) {
    boundary_offset_entries.push(reinterpret_cast<uint32_t*>(p));
    p += sizeof(uint32_t);
  }
  std::queue<uint32_t> map_header_offsets;
  uint16_t entity_id = 1;
  for (const auto& map : maps) {
    map_header_offsets.push(static_cast<uint32_t>(p - buf));
    size_t map_header_size;
    write_map(
      map,
      config,
      entity_id,
      static_cast<uint32_t>(p - buf),
      buf ? p : nullptr,
      &map_header_size
    );
    p += map_header_size;
  }
  std::queue<uint32_t> boundary_offsets;
  for (const auto& points : bounds) {
    boundary_offsets.push(static_cast<uint32_t>(p - buf));
    size_t boundary_size;
    write_boundary(points, buf ? p : nullptr, &boundary_size);
    p += boundary_size;
  }
  if (buf != nullptr) {
    *Mn = maps.size();
    *Bn = bounds.size();
    for (const auto& map : maps) {
      *map_header_offset_entries.front() = map_header_offsets.front();
      map_header_offset_entries.pop();
      map_header_offsets.pop();
    }
    for (const auto& points : bounds) {
      *boundary_offset_entries.front() = boundary_offsets.front();
      boundary_offset_entries.pop();
      boundary_offsets.pop();
    }
  }
  if (buf_size != nullptr) {
    *buf_size = p - buf;
  }
}

enum BoundsTile {
  Empty   = 0x00,
  Slope   = 0x01,
  Down    = 0x03,
  Ceil    = 0x04,
  Half    = 0x08,
  Tall    = 0x11,
  Solid   = 0x20,
  OneWay  = 0x40,
};

const static std::unordered_map<uint8_t, std::list<Point>> geometry = {
  {Empty, {}},
  {Solid, {{0, 0}, {16, 0}, {16, 16}, {0, 16}}},
  {Slope, {{0, 16}, {16, 0}, {16, 16}}},
  {Slope | Down, {{0, 0}, {16, 16}, {0, 16}}},
  {Slope | Down | Ceil, {{0, 0}, {16, 0}, {16, 16}}},
  {Slope | Ceil, {{0, 0}, {16, 0}, {0, 16}}},
  {Slope | Half, {{0, 16}, {16, 8}, {16, 16}}},
  {Slope | Half | Tall, {{0, 8}, {16, 0}, {16, 16}, {0, 16}}},
  {Slope | Half | Tall | Down, {{0, 0}, {16, 8}, {16, 16}, {0, 16}}},
  {Slope | Half | Down, {{0, 8}, {16, 16}, {0, 16}}},
  {Slope | Half | Ceil, {{0, 0}, {16, 0}, {0, 8}}},
  {Slope | Half | Ceil | Tall, {{0, 0}, {16, 0}, {16, 8}, {0, 16}}},
  {Slope | Half | Ceil | Tall | Down, {{0, 0}, {16, 0}, {16, 16}, {0, 8}}},
  {Slope | Half | Ceil | Down, {{0, 0}, {16, 0}, {16, 8}}},
  {Half, {{0, 8}, {16, 8}, {16, 16}, {0, 16}}},
  {Half | Ceil, {{0, 0}, {16, 0}, {16, 8}, {0, 8}}},
  {(OneWay | Solid) + 0, {{0, 0}, {16, 0}}},
  {(OneWay | Solid) + 1, {{16, 0}, {16, 16}}},
  {(OneWay | Solid) + 2, {{16, 16}, {0, 16}}},
  {(OneWay | Solid) + 3, {{0, 16}, {0, 0}}},
  {OneWay | Slope, {{0, 16}, {16, 0}}},
  {OneWay | Slope | Down, {{0, 0}, {16, 16}}},
  {OneWay | Slope | Down | Ceil, {{16, 16}, {0, 0}}},
  {OneWay | Slope | Ceil, {{16, 0}, {0, 16}}},
  {OneWay | Slope | Half, {{0, 16}, {16, 8}}},
  {OneWay | Slope | Half | Tall, {{0, 8}, {16, 0}}},
  {OneWay | Slope | Half | Tall | Down, {{0, 0}, {16, 8}}},
  {OneWay | Slope | Half | Down, {{0, 8}, {16, 16}}},
  {OneWay | Slope | Half | Ceil, {{16, 0}, {0, 8}}},
  {OneWay | Slope | Half | Ceil | Tall, {{16, 8}, {0, 16}}},
  {OneWay | Slope | Half | Ceil | Tall | Down, {{16, 16}, {0, 8}}},
  {OneWay | Slope | Half | Ceil | Down, {{16, 8}, {0, 0}}},
  {OneWay | Half, {{0, 8}, {16, 8}}},
  {OneWay | Half | Ceil, {{16, 8}, {0, 8}}},
};

template<typename T>
static typename T::iterator next_wrap(
  T& in,
  typename T::iterator it
) {
  auto next = std::next(it);
  if (next == in.end()) {
    return in.begin();
  }
  return next;
}

static float slope(
  const Point& a,
  const Point& b
) {
  float x = b.x - a.x;
  if (x == 0) {
    return std::numeric_limits<float>::infinity();
  }
  return (b.y - a.y) / x;
}

static void merge_lines(
  std::list<Boundary<std::list>>& boundaries
) {
  // Join connected tiles.
 loop_lines:
  for (auto a = boundaries.begin(); a != boundaries.end(); a++) {
    for (auto b = boundaries.begin(); b != boundaries.end(); b++) {
      if (a == b) {
        continue;
      }
      auto ap = std::prev(a->end());
      auto bp = b->begin();
      if (ap->x == bp->x && ap->y == bp->y) {
        a->insert(std::next(ap), std::next(bp), b->end());
        boundaries.erase(b);
        goto loop_lines;
      }
    }
  }
  // Simplify geometry.
  for (auto a = boundaries.begin(); a != boundaries.end(); a++) {
  loop_geometry:
    auto ap1 = a->begin();
    auto ap2 = std::next(ap1);
    auto ap3 = std::next(ap2);
    while (ap2 != a->end()) {
      if (slope(*ap1, *ap2) == slope(*ap2, *ap3)) {
        a->erase(ap2);
        goto loop_geometry;
      }
      ap1++;
      ap2++;
      ap3++;
    }
  }
}

static void merge(
  std::list<Point>& to,
  std::list<Point>::iterator pos,
  std::list<Point>& from,
  std::list<Point>::iterator first,
  std::list<Point>::iterator last
) {
  if (std::distance(first, from.end()) > std::distance(last, from.end())) {
    to.insert(pos, first, last);
  } else {
    std::list<Point> temp;
    temp.insert(temp.end(), first, from.end());
    temp.insert(temp.end(), from.begin(), last);
    to.insert(pos, temp.begin(), temp.end());
  }
}

static void merge_bounds(
  std::list<Boundary<std::list>>& boundaries
) {
  // Join connected tiles.
 loop_tiles:
  for (auto a = boundaries.begin(); a != boundaries.end(); a++) {
    for (auto b = boundaries.begin(); b != boundaries.end(); b++) {
      if (a == b) {
        continue;
      }
      auto ap1 = a->begin();
      auto ap2 = next_wrap(*a, ap1);
      while (ap1 != a->end()) {
        auto bp1 = b->begin();
        auto bp2 = next_wrap(*b, bp1);
        while (bp1 != b->end()) {
          if (ap1->x == ap2->x && bp1->x == bp2->x
              && ap1->x == bp1->x) {
            // Boundaries on the same vertical.
            if (ap1->y < ap2->y && bp1->y > bp2->y) {
              // Av B^
              if (ap1->y == bp2->y && ap2->y == bp1->y) {
                // Merge O boundaries.
                merge(*a, ap2, *b, std::next(bp2), bp1);
                boundaries.erase(b);
                goto loop_tiles;
              } else if (ap1->y == bp2->y && ap2->y < bp1->y) {
                // Merge L boundaries (short A).
                merge(*a, ap2, *b, std::next(bp2), std::next(bp1));
                boundaries.erase(b);
                goto loop_tiles;
              } else if (ap1->y < bp2->y && ap2->y == bp1->y) {
                // Merge L boundaries (short B).
                merge(*a, ap2, *b, bp2, bp1);
                boundaries.erase(b);
                goto loop_tiles;
              } else if (ap1->y > bp2->y && ap2->y == bp1->y) {
                // Merge J boundaries (short A).
                merge(*a, ap2, *b, bp2, bp1);
                boundaries.erase(b);
                goto loop_tiles;
              } else if (ap1->y == bp2->y && ap2->y > bp1->y) {
                // Merge J boundaries (short B).
                merge(*a, ap2, *b, std::next(bp2), std::next(bp1));
                boundaries.erase(b);
                goto loop_tiles;
              } else if (ap1->y > bp2->y && ap2->y < bp1->y) {
                // Merge T boundaries (short A).
                merge(*a, ap2, *b, bp2, std::next(bp1));
                boundaries.erase(b);
                goto loop_tiles;
              } else if (ap1->y < bp2->y && ap2->y > bp1->y) {
                // Merge T boundaries (short B).
                merge(*a, ap2, *b, bp2, std::next(bp1));
                boundaries.erase(b);
                goto loop_tiles;
              } else if (ap1->y < bp2->y && ap2->y < bp1->y
                         && ap2->y > bp2->y) {
                // Merge S boundaries.
                merge(*a, ap2, *b, bp2, std::next(bp1));
                boundaries.erase(b);
                goto loop_tiles;
              } else if (ap1->y > bp2->y && ap2->y > bp1->y
                         && ap1->y < bp1->y) {
                // Merge Z boundaries.
                merge(*a, ap2, *b, bp2, std::next(bp1));
                boundaries.erase(b);
                goto loop_tiles;
              }
            }
          } else if (ap1->y == ap2->y && bp1->y == bp2->y
                     && ap1->y == bp1->y) {
            // Boundaries on the same horizontal.
            if (ap1->x < ap2->x && bp1->x > bp2->x) {
              // A> B<
              if (ap1->x == bp2->x && ap2->x == bp1->x) {
                // Merge O boundaries.
                merge(*a, ap2, *b, std::next(bp2), bp1);
                boundaries.erase(b);
                goto loop_tiles;
              } else if (ap1->x == bp2->x && ap2->x < bp1->x) {
                // Merge L boundaries (short A).
                merge(*a, ap2, *b, std::next(bp2), std::next(bp1));
                boundaries.erase(b);
                goto loop_tiles;
              } else if (ap1->x < bp2->x && ap2->x == bp1->x) {
                // Merge L boundaries (short B).
                merge(*a, ap2, *b, bp2, bp1);
                boundaries.erase(b);
                goto loop_tiles;
              } else if (ap1->x > bp2->x && ap2->x == bp1->x) {
                // Merge J boundaries (short A).
                merge(*a, ap2, *b, bp2, bp1);
                boundaries.erase(b);
                goto loop_tiles;
              } else if (ap1->x == bp2->x && ap2->x > bp1->x) {
                // Merge J boundaries (short B).
                merge(*a, ap2, *b, std::next(bp2), std::next(bp1));
                boundaries.erase(b);
                goto loop_tiles;
              } else if (ap1->x > bp2->x && ap2->x < bp1->x) {
                // Merge T boundaries (short A).
                merge(*a, ap2, *b, bp2, std::next(bp1));
                boundaries.erase(b);
                goto loop_tiles;
              } else if (ap1->x < bp2->x && ap2->x > bp1->x) {
                // Merge T boundaries (short B).
                merge(*a, ap2, *b, bp2, std::next(bp1));
                boundaries.erase(b);
                goto loop_tiles;
              } else if (ap1->x < bp2->x && ap2->x < bp1->x
                         && ap2->x > bp2->x) {
                // Merge S boundaries.
                boundaries.erase(b);
                merge(*a, ap2, *b, bp2, std::next(bp1));
                goto loop_tiles;
              } else if (ap1->x > bp2->x && ap2->x > bp1->x
                         && ap1->x < bp1->x) {
                // Merge Z boundaries.
                merge(*a, ap2, *b, bp2, std::next(bp1));
                boundaries.erase(b);
                goto loop_tiles;
              }
            }
          }
          bp1++;
          bp2 = next_wrap(*b, bp2);
        }
        ap1++;
        ap2 = next_wrap(*a, ap2);
      }
    }
  }
  // Reduce boundaries.
 loop_reduce:
  for (auto a = boundaries.begin(); a != boundaries.end(); a++) {
  loop_overlap:
    // Merge overlapping lines.
    auto ap1 = a->begin();
    auto ap2 = next_wrap(*a, ap1);
    auto ap3 = next_wrap(*a, ap2);
    while (ap1 != a->end()) {
      if (ap1->x == ap3->x && ap1->y == ap3->y) {
        a->erase(ap1);
        a->erase(ap2);
        goto loop_overlap;
      }
      ap1++;
      ap2 = next_wrap(*a, ap2);
      ap3 = next_wrap(*a, ap3);
    }
  }
  for (auto a = boundaries.begin(); a != boundaries.end(); a++) {
    // If there are residual overlapping lines, they represent bleed in to
    // areas that should be separate boundaries.
    auto ap1 = a->begin();
    auto ap2 = next_wrap(*a, ap1);
    while (ap1 != a->end()) {
      auto ap3 = next_wrap(*a, ap2);
      auto ap4 = next_wrap(*a, ap3);
      while (ap3 != a->end()) {
        if (ap1->x == ap4->x && ap1->y == ap4->y
            && ap2->x == ap3->x && ap2->y == ap3->y) {
          Boundary<std::list> new_boundary;
          new_boundary.insert(new_boundary.end(), ap1, ap4);
          a->erase(ap2, ap3);
          boundaries.push_back(new_boundary);
          goto loop_reduce;
        }
        ap3++;
        ap4 = next_wrap(*a, ap4);
      }
      ap1++;
      ap2 = next_wrap(*a, ap2);
    }
  }
  // Simplify geometry.
  for (auto a = boundaries.begin(); a != boundaries.end(); a++) {
  loop_geometry:
    auto ap1 = a->begin();
    auto ap2 = next_wrap(*a, ap1);
    auto ap3 = next_wrap(*a, ap2);
    while (ap1 != a->end()) {
      if (slope(*ap1, *ap2) == slope(*ap2, *ap3)) {
        a->erase(ap2);
        goto loop_geometry;
      }
      ap1++;
      ap2 = next_wrap(*a, ap2);
      ap3 = next_wrap(*a, ap3);
    }
  }
  // Remove empty paths.
  auto it = boundaries.begin();
  while (it != boundaries.end()) {
    if (it->size() == 0) {
      it = boundaries.erase(it);
    } else {
      it++;
    }
  }
}

static std::list<Boundary<std::list>> points_from_bounds(
  std::vector<Map>& maps,
  std::vector<Layer>& bounds
) {
  // Determine dimensions of the world.
  int32_t world_x, world_y;
  uint32_t world_w, world_h;
  world_x = world_y = std::numeric_limits<int32_t>::max();
  world_w = world_h = 0;
  for (const auto& map : maps) {
    if (map.x < world_x) {
      world_x = map.x;
    }
    if (map.y < world_y) {
      world_y = map.y;
    }
    if (map.x + map.w > world_w) {
      world_w = map.x + map.w;
    }
    if (map.y + map.h > world_h) {
      world_h = map.y + map.h;
    }
  }
  world_w -= world_x;
  world_h -= world_y;
  // Create boundary around maps.
  std::list<Boundary<std::list>> boundaries;
  for (const auto& map : maps) {
    Boundary<std::list> boundary;
    boundary.push_back({
      .x = (map.x) << 4,
      .y = (map.y) << 4,
    });
    boundary.push_back({
      .x = (map.x) << 4,
      .y = (map.y + map.h) << 4,
    });
    boundary.push_back({
      .x = (map.x + map.w) << 4,
      .y = (map.y + map.h) << 4,
    });
    boundary.push_back({
      .x = (map.x + map.w) << 4,
      .y = (map.y) << 4,
    });
    boundaries.push_back(boundary);
  }
  merge_bounds(boundaries);
  // Collect boundary lines for each tile.
  for (int i = 0; i < maps.size(); i++) {
    const auto& map = maps[i];
    for (int y = 0; y < map.h; y++) {
      for (int x = 0; x < map.w; x++) {
        auto tile = bounds[i].tiles[x + y * map.w];
        if (tile && !((tile - 1) & BoundsTile::OneWay)) {
          auto geo = geometry.at(tile - 1);
          if (geo.size()) {
            Boundary<std::list> points;
            for (auto& point : geo) {
              point.x += (map.x + x) << 4;
              point.y += (map.y + y) << 4;
              points.push_back(point);
            }
            boundaries.push_back(points);
          }
        }
      }
    }
  }
  merge_bounds(boundaries);
  // Remove the outer boundary.
  for (auto a = boundaries.begin(); a != boundaries.end(); a++) {
    if (a->size() == 4) {
      auto ap1 = a->begin();
      auto ap2 = std::next(ap1);
      auto ap3 = std::next(ap2);
      auto ap4 = std::next(ap3);
      if (ap1->x == ((world_x - 1) << 4)
          && ap1->y == ((world_y - 1) << 4)
          && ap2->x == ((world_w + world_x) << 4)
          && ap2->y == ((world_y - 1) << 4)
          && ap3->x == ((world_w + world_x) << 4)
          && ap3->y == ((world_h + world_y) << 4)
          && ap4->x == ((world_x - 1) << 4)
          && ap4->y == ((world_h + world_y) << 4)) {
        boundaries.erase(a);
        break;
      }
    }
  }
  // Close boundaries.
  for (auto a = boundaries.begin(); a != boundaries.end(); a++) {
    a->push_back(*a->begin());
  }
  // One-way boundaries don't connect to the normal map geometry.
  // Collect boundary lines for each one-way tile.
  std::list<Boundary<std::list>> one_way_boundaries;
  for (int i = 0; i < maps.size(); i++) {
    const auto& map = maps[i];
    for (int y = 0; y < map.h; y++) {
      for (int x = 0; x < map.w; x++) {
        auto tile = bounds[i].tiles[x + y * map.w];
        if (tile && (tile - 1) & BoundsTile::OneWay) {
          auto geo = geometry.at(tile - 1);
          if (geo.size()) {
            Boundary<std::list> points;
            points.flags = BoundsTile::OneWay;
            for (auto& point : geo) {
              point.x += (map.x + x) << 4;
              point.y += (map.y + y) << 4;
              points.push_back(point);
            }
            one_way_boundaries.push_back(points);
          }
        }
      }
    }
  }
  merge_lines(one_way_boundaries);
  // Append one-way boundaries to tile boundaries.
  std::copy(
    one_way_boundaries.begin(),
    one_way_boundaries.end(),
    std::back_inserter(boundaries)
  );
  return boundaries;
}

static void print_usage(const char* self, std::ostream& out) {
  out << "Usage: " << self << " [-h] [-c <config.yaml>] <in.world> <out.bin>"
      << std::endl;
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
  if (argc != 3 && argc != 5) {
    print_usage(argv[0], std::cerr);
    return 1;
  }
  // Check for entity configuration file.
  YAML::Node config;
  int json_arg_idx = 1;
  int out_arg_idx = 2;
  if (argc == 5) {
    json_arg_idx = 3;
    out_arg_idx = 4;
    std::string arg(argv[1]);
    if (arg == "-c") {
      config = YAML::LoadFile(argv[2]);
    } else {print_usage(argv[0], std::cerr);
      return 1;
    }
  }
  std::string path(argv[json_arg_idx]);
  auto prefix = path.substr(0, path.rfind("/"));
  if (prefix == path) {
    prefix = ".";
  }
  prefix += "/";
  auto world = load_json(argv[json_arg_idx]);
  // Parse maps.
  std::vector<Map> maps;
  std::vector<Layer> bounds;
  for (int i = 0; i < world["maps"].size(); i++) {
    // Open map file.
    auto map_file_path = prefix + world["maps"][i]["fileName"].asString();
    rapidxml::xml_document<> map_doc;
    auto map_file = load_xml(map_file_path.c_str(), map_doc);
    // Get width and height from map attributes.
    uint16_t w, h;
    for (auto attr = map_doc.first_node()->first_attribute();
         attr != nullptr;
         attr = attr->next_attribute()) {
      std::string attr_name(attr->name());
      if (attr_name == "width") {
        w = static_cast<uint16_t>(std::atoi(attr->value()));
      } else if (attr_name == "height") {
        h = static_cast<uint16_t>(std::atoi(attr->value()));
      }
    }
    std::vector<uint32_t> properties;
    std::vector<Layer> layers;
    std::vector<Tileset> tilesets;
    std::vector<Entity> entities;
    // Iterate nodes for layers and tilesets.
    size_t map_tileset_index = 0;
    size_t entity_tileset_index = 0;
    int entities_layer_index = -1;
    uint8_t layer_index = 0;
    for (auto map_node = map_doc.first_node()->first_node();
         map_node != nullptr;
         map_node = map_node->next_sibling()) {
      std::string node_name(map_node->name());
      if (node_name == "properties") {
        for (auto properties_node = map_node->first_node();
             properties_node != nullptr;
             properties_node = properties_node->next_sibling()) {
          std::string node_name(properties_node->name());
          if (node_name == "property") {
            std::uint32_t name;
            std::string type = "string";
            std::string value;
            for (auto attr = properties_node->first_attribute();
                 attr != nullptr;
                 attr = attr->next_attribute()) {
              std::string attr_name(attr->name());
              std::string attr_value(attr->value());
              if (attr_name == "name") {
                name = ultra::sdk::util::crc32(attr_value.c_str());
              } else if (attr_name == "type") {
                type = attr_value;
              } else if (attr_name == "value") {
                value = attr_value;
              }
            }
            uint32_t int_value;
            if (type == "int") {
              int_value = static_cast<uint16_t>(std::atoi(value.c_str()));
            } else if (type == "bool") {
              if (value == "true") {
                int_value = 1;
              } else {
                int_value = 0;
              }
            } else if (type == "string") {
              int_value = ultra::sdk::util::crc32(value.c_str());
            }
            properties.push_back(name);
            properties.push_back(int_value);
          }
        }
      } else if (node_name == "tileset") {
        Tileset tileset = {
          .map_index = -1,
          .entity_index = -1,
        };
        // Parse attributes for the first gid and source.
        std::string tileset_source;
        for (auto attr = map_node->first_attribute();
             attr != nullptr;
             attr = attr->next_attribute()) {
          std::string attr_name(attr->name());
          if (attr_name == "firstgid") {
            tileset.first_gid = static_cast<uint16_t>(
              std::atoi(attr->value())
            );
          } else if (attr_name == "source") {
            tileset_source = prefix + attr->value();
          }
        }
        // Read the tileset document.
        tileset.tileset = ultra::sdk::read_tileset(tileset_source.c_str());
        tilesets.push_back(tileset);
      } else if (node_name == "layer") {
        // Parse attributes.
        Layer layer = {
          .type = Layer::Type::Image,
          .parallax = {
            .x = {1, 1},
            .y = {1, 1},
          },
        };
        for (auto attr = map_node->first_attribute();
             attr != nullptr;
             attr = attr->next_attribute()) {
          std::string attr_name(attr->name());
          if (attr_name == "name") {
            layer.name = ultra::sdk::util::crc32(attr->value());
          } else if (attr_name == "parallaxx") {
            layer.parallax.x = double_to_fraction(std::atof(attr->value()));
          } else if (attr_name == "parallaxy") {
            layer.parallax.y = double_to_fraction(std::atof(attr->value()));
          }
        }
        // Get the layer tile data.
        layer.tiles = std::vector<uint16_t>(w * h);
        for (auto layer_node = map_node->first_node();
             layer_node != nullptr;
             layer_node = layer_node->next_sibling()) {
          std::string node_name(layer_node->name());
          if (node_name == "data") {
            std::string data(layer_node->value());
            size_t count = 0;
            size_t start = 0;
            size_t next = 0;
            bool is_first_tile = true;
            while (next != std::string::npos) {
              next = data.find(",", start);
              uint16_t tile = std::atoi(
                data.substr(start, next - start).c_str()
              );
              if (tile) {
                // Adjust tile value so its first nybble is the tileset index.
                bool found = false;
                for (int i = tilesets.size() - 1; i >= 0; i--) {
                  if (tile >= tilesets[i].first_gid) {
                    int index;
                    if (tilesets[i].tileset.bounds) {
                      if (!is_first_tile
                          && layer.type != Layer::Type::Bounds) {
                        throw std::runtime_error(
                          "Image layer contains bounds tiles"
                        );
                      }
                      layer.type = Layer::Type::Bounds;
                      index = 0;
                    } else {
                      if (layer.type == Layer::Type::Bounds) {
                        throw std::runtime_error(
                          "Bounds layer contains image tiles"
                        );
                      }
                      if (tilesets[i].map_index == -1) {
                        tilesets[i].map_index = map_tileset_index++;
                      }
                      index = tilesets[i].map_index;
                    }
                    tile = (index << 12) | (tile - tilesets[i].first_gid + 1);
                    found = true;
                    break;
                  }
                }
                if (!found) {
                  throw std::runtime_error("Non-map tile used in map layer");
                }
                is_first_tile = false;
              }
              layer.tiles[count++] = tile;
              start = next + 1;
            }
            switch (layer.type) {
            case Layer::Type::Image:
              layers.push_back(layer);
              break;
            case Layer::Type::Bounds:
              bounds.push_back(layer);
              break;
            }
          }
        }
        layer_index++;
      } else if (node_name == "objectgroup") {
        uint32_t layer_name;
        for (auto attr = map_node->first_attribute();
             attr != nullptr;
             attr = attr->next_attribute()) {
          std::string attr_name(attr->name());
          if (attr_name == "name") {
            layer_name = ultra::sdk::util::crc32(attr->value());
          }
        }
        // Parse nodes for entities.
        for (auto objectgroup_node = map_node->first_node();
             objectgroup_node != nullptr;
             objectgroup_node = objectgroup_node->next_sibling()) {
          std::string node_name(objectgroup_node->name());
          if (node_name == "object") {
            Entity ent = {
              .layer_name = layer_name,
              .state = 0,
            };
            // Parse attributes for gid and position.
            for (auto attr = objectgroup_node->first_attribute();
                 attr != nullptr;
                 attr = attr->next_attribute()) {
              std::string attr_name(attr->name());
              if (attr_name == "gid") {
                uint32_t tile = std::atoi(attr->value());
                uint16_t tile_state = 0;
                if (tile & FLIP_X) {
                  tile ^= FLIP_X;
                  tile_state |= 0x800;
                }
                if (tile & FLIP_Y) {
                  tile ^= FLIP_Y;
                  tile_state |= 0x400;
                }
                if (tile) {
                  // Adjust tile value so its first nybble is the tileset
                  // index.
                  bool found = false;
                  for (int i = tilesets.size() - 1; i >= 0; i--) {
                    if (tile >= tilesets[i].first_gid) {
                      if (tilesets[i].entity_index == -1) {
                        tilesets[i].entity_index = entity_tileset_index++;
                      }
                      ent.tile = (tilesets[i].entity_index << 12)
                        | tile_state
                        | (tile - tilesets[i].first_gid + 1);
                      ent.w = tilesets[i].tileset.tile_w;
                      ent.h = tilesets[i].tileset.tile_h;
                      found = true;
                      break;
                    }
                  }
                  if (!found) {
                    throw std::runtime_error(
                      "Non-entity tile used in entities layer"
                    );
                  }
                }
              } else if (attr_name == "x") {
                ent.x = std::atoi(attr->value());
              } else if (attr_name == "y") {
                ent.y = std::atoi(attr->value());
              }
            }
            // Parse nodes for properties.
            for (auto object_node = objectgroup_node->first_node();
                 object_node != nullptr;
                 object_node = object_node->next_sibling()) {
              std::string node_name(object_node->name());
              if (node_name == "properties") {
                for (auto properties_node = object_node->first_node();
                     properties_node != nullptr;
                     properties_node = properties_node->next_sibling()) {
                  std::string node_name(properties_node->name());
                  if (node_name == "property") {
                    // Parse attributes for gid and position.
                    std::string name;
                    std::string type = "string";
                    std::string value;
                    for (auto attr = properties_node->first_attribute();
                         attr != nullptr;
                         attr = attr->next_attribute()) {
                      std::string attr_name(attr->name());
                      std::string attr_value(attr->value());
                      if (attr_name == "name") {
                        name = attr_value;
                      } else if (attr_name == "type") {
                        type = attr_value;
                      } else if (attr_name == "value") {
                        value = attr_value;
                      }
                    }
                    if (name == "state") {
                      if (type == "string") {
                        ent.state = ultra::sdk::util::crc32(value.c_str());
                      } else if (type == "int") {
                        ent.state = std::atoi(value.c_str());
                      } else if (type == "bool") {
                        if (value == "true") {
                          ent.state = 1;
                        } else {
                          ent.state = 0;
                        }
                      } else {
                        throw std::runtime_error(
                          "Entity state not type string or int"
                        );
                      }
                    } else if (name == "type") {
                      ent.type = value;
                    }
                  }
                }
              }
            }
            entities.push_back(ent);
          }
        }
      }
    }
    // Sort the tilesets by index.
    std::vector<Tileset> map_tilesets(tilesets);
    std::sort(map_tilesets.begin(), map_tilesets.end(), [](
      Tileset& a,
      Tileset& b
    ) {
      return a.map_index < b.map_index;
    });
    while (map_tilesets.size() && map_tilesets[0].map_index == -1) {
      map_tilesets.erase(map_tilesets.begin());
    }
    std::vector<Tileset> entity_tilesets(tilesets);
    std::sort(entity_tilesets.begin(), entity_tilesets.end(), [](
      Tileset& a,
      Tileset& b
    ) {
      return a.entity_index < b.entity_index;
    });
    while (entity_tilesets.size() && entity_tilesets[0].entity_index == -1) {
      entity_tilesets.erase(entity_tilesets.begin());
    }
    // Add map struct to collection.
    maps.push_back({
      .x = static_cast<int16_t>(world["maps"][i]["x"].asInt() / 16),
      .y = static_cast<int16_t>(world["maps"][i]["y"].asInt() / 16),
      .w = w,
      .h = h,
      .properties = properties,
      .entities_index = static_cast<uint8_t>(entities_layer_index),
      .map_tilesets = map_tilesets,
      .entity_tilesets = entity_tilesets,
      .layers = layers,
      .entities = entities,
    });
  }
  // Build boundary data.
  auto points = points_from_bounds(maps, bounds);
  if (getenv("PRINT_BOUNDS") != nullptr) {
    size_t points_size1 = points.size();
    size_t count1 = 0;
    std::cout << "[";
    for (const auto& points : points) {
      std::cout << "[";
      size_t points_size2 = points.size();
      size_t count2 = 0;
      for (const auto& point : points) {
        std::cout << "["
                  << std::to_string(point.x)
                  << ","
                  << std::to_string(point.y)
                  << "]";
        if (++count2 < points_size2) {
          std::cout << ",";
        }
      }
      std::cout << "]";
      if (++count1 < points_size1) {
        std::cout << ",";
      }
    }
    std::cout << "]";
  }
  // Build binary data.
  size_t buf_size;
  write_world(maps, points, config, nullptr, &buf_size);
  uint8_t buf[buf_size];
  write_world(maps, points, config, buf, nullptr);
  // Write the binary data.
  std::ofstream out(argv[out_arg_idx]);
  if (!out.is_open()) {
    throw std::runtime_error("Could not open output file");
  }
  out.write(reinterpret_cast<char*>(buf), buf_size);
  return 0;
}

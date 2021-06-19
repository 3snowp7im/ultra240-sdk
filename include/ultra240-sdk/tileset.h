#pragma once

#include <list>
#include <map>
#include <string>
#include <ultra240-sdk/util.h>
#include <vector>

namespace ultra::sdk {

  struct Tileset {
    struct Tile {
      struct CollisionBox {
        uint16_t x, y;
        uint16_t w, h;
      };
      struct AnimationTile {
        uint16_t tile_id;
        uint16_t duration;
      };
      uint32_t name;
      util::HashMap<util::HashMap<std::vector<CollisionBox>>> collision_boxes;
      std::vector<AnimationTile> animation_tiles;
      std::string library;
    };
    uint16_t tile_count;
    uint16_t tile_w, tile_h;
    uint16_t margin;
    uint16_t spacing;
    uint16_t columns;
    std::string source;
    std::map<uint16_t, Tile> tiles;
    std::string library;
    bool bounds;
  };

  Tileset read_tileset(const char* path);

  void write_tileset(
    const Tileset& tileset,
    uint8_t* buf,
    size_t* buf_size,
    uint32_t** source_offset_entry,
    std::list<uint32_t*>* tile_offset_entries,
    uint32_t** library_offset_entry
  );

  void write_tileset_tile(
    uint16_t tile_id,
    const Tileset::Tile& tile,
    uint8_t* buf,
    size_t* buf_size,
    std::list<uint32_t*>* collision_box_type_offset_entries,
    uint32_t** tile_library_offset_entry
  );

  void write_tileset_tile_collision_box_type(
    uint32_t type,
    const util::HashMap<std::vector<Tileset::Tile::CollisionBox>>& lists,
    uint8_t* buf,
    size_t* buf_size,
    std::list<uint32_t*>* collision_box_list_offset_entries
  );

  void write_tileset_tile_collision_box_list(
    uint32_t name,
    const std::vector<Tileset::Tile::CollisionBox>& collision_boxes,
    uint8_t* buf,
    size_t* buf_size
  );

}

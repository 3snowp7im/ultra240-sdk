#include <cmath>
#include <queue>
#include <ultra240-sdk/tileset.h>
#include <ultra240-sdk/util.h>
#include <rapidxml/rapidxml.hpp>
#include <rapidxml/rapidxml_utils.hpp>

namespace ultra::sdk {

  Tileset read_tileset(const char* path) {
    Tileset tileset = {
      .margin = 0,
      .spacing = 0,
      .bounds = false,
    };
    rapidxml::file<> file(path);
    rapidxml::xml_document<> tileset_doc;
    tileset_doc.parse<0>(file.data());
    // Parse attributes for tile dimensions, margin, and spacing.
    for (auto attr = tileset_doc.first_node()->first_attribute();
         attr != nullptr;
         attr = attr->next_attribute()) {
      std::string attr_name(attr->name());
      if (attr_name == "tilecount") {
        tileset.tile_count = static_cast<uint16_t>(std::atoi(attr->value()));
      } else if (attr_name == "tilewidth") {
        tileset.tile_w = static_cast<uint16_t>(std::atoi(attr->value()));
      } else if (attr_name == "tileheight") {
        tileset.tile_h = static_cast<uint16_t>(std::atoi(attr->value()));
      } else if (attr_name == "columns") {
        tileset.columns = static_cast<uint16_t>(std::atoi(attr->value()));
      } else if (attr_name == "margin") {
        tileset.margin = static_cast<uint16_t>(std::atoi(attr->value()));
      } else if (attr_name == "spacing") {
        tileset.spacing = static_cast<uint16_t>(std::atoi(attr->value()));
      }
    }
    // Parse the tileset doc for its properties and image node.
    for (auto tileset_node = tileset_doc.first_node()->first_node();
         tileset_node != nullptr;
         tileset_node = tileset_node->next_sibling()) {
      std::string node_name(tileset_node->name());
      if (node_name == "properties") {
        // Parse attributes for the name and value of the property.
        std::string name;
        std::string value;
        for (auto property = tileset_node->first_node();
             property != nullptr;
             property = property->next_sibling()) {
          for (auto attr = property->first_attribute();
               attr != nullptr;
               attr = attr->next_attribute()) {
            std::string attr_name(attr->name());
            if (attr_name == "name") {
              name = attr->value();
            } else if (attr_name == "value") {
              value = attr->value();
            }
          }
          // Record property.
          if (name == "bounds") {
            if (value == "true") {
              tileset.bounds = true;
            }
          } else if (name == "library") {
            tileset.library = value;
          }
        }
      } else if (node_name == "image") {
        // Parse attributes for image source and dimensions.
        for (auto attr = tileset_node->first_attribute();
             attr != nullptr;
             attr = attr->next_attribute()) {
          std::string attr_name(attr->name());
          if (attr_name == "source") {
            // Trim the path relative to the img directory.
            std::string attr_value(attr->value());
            std::string image_filename;
            size_t last_sep_pos = attr_value.rfind('/');
            if (last_sep_pos == std::string::npos) {
              image_filename = attr_value;
            } else {
              image_filename = attr_value.substr(
                last_sep_pos + 1,
                std::string::npos
              );
            }
            size_t name_len = image_filename.size() - 4;
            tileset.source = image_filename.substr(0, name_len);
          }
        }
      } else if (node_name == "tile") {
        Tileset::Tile tile;
        uint16_t tile_id;
        // Parse attributes for id.
        for (auto attr = tileset_node->first_attribute();
             attr != nullptr;
             attr = attr->next_attribute()) {
          std::string attr_name(attr->name());
          if (attr_name == "id") {
            tile_id = static_cast<uint16_t>(atoi(attr->value()));
          }
        }
        // Parse nodes for properties.
        for (auto tile_node = tileset_node->first_node();
             tile_node != nullptr;
             tile_node = tile_node->next_sibling()) {
          std::string node_name(tile_node->name());
          if (node_name == "properties") {
            for (auto properties_node = tile_node->first_node();
                 properties_node != nullptr;
                 properties_node = properties_node->next_sibling()) {
              std::string node_name(properties_node->name());
              if (node_name == "property") {
                // Parse attributes for id.
                std::string name;
                std::string value;
                for (auto attr = properties_node->first_attribute();
                     attr != nullptr;
                     attr = attr->next_attribute()) {
                  std::string attr_name(attr->name());
                  if (attr_name == "name") {
                    name = std::string(attr->value());
                  } else if (attr_name == "value") {
                    value = std::string(attr->value());
                  }
                }
                if (name == "name") {
                  tile.name = ultra::sdk::util::crc32(value.c_str());
                } else if (name == "library") {
                  tile.library = value;
                }
              }
            }
          } else if (node_name == "objectgroup") {
            for (auto objectgroup_node = tile_node->first_node();
                 objectgroup_node != nullptr;
                 objectgroup_node = objectgroup_node->next_sibling()) {
              std::string node_name(objectgroup_node->name());
              if (node_name == "object") {
                bool has_type = false;
                uint32_t type;
                uint32_t name;
                Tileset::Tile::CollisionBox cb;
                for (auto attr = objectgroup_node->first_attribute();
                     attr != nullptr;
                     attr = attr->next_attribute()) {
                  std::string attr_name(attr->name());
                  if (attr_name == "type") {
                    has_type = true;
                    type = ultra::sdk::util::crc32(attr->value());
                  } else if (attr_name == "name") {
                    name = ultra::sdk::util::crc32(attr->value());
                  } else if (attr_name == "x") {
                    cb.x = static_cast<uint16_t>(
                      round(std::atof(attr->value()))
                    );
                  } else if (attr_name == "y") {
                    cb.y = static_cast<uint16_t>(
                      round(std::atof(attr->value()))
                    );
                  } else if (attr_name == "width") {
                    cb.w = static_cast<uint16_t>(
                      round(std::atof(attr->value()))
                    );
                  } else if (attr_name == "height") {
                    cb.h = static_cast<uint16_t>(
                      round(std::atof(attr->value()))
                    );
                  }
                }
                if (!has_type) {
                  throw std::runtime_error("Collision box missing type");
                }
                tile.collision_boxes.emplace(
                  type,
                  util::HashMap<std::vector<Tileset::Tile::CollisionBox>>()
                );
                tile.collision_boxes[type].emplace(
                  name,
                  std::vector<Tileset::Tile::CollisionBox>()
                );
                tile.collision_boxes[type][name].push_back(cb);
              }
            }
          } else if (node_name == "animation") {
            for (auto animation_node = tile_node->first_node();
                 animation_node != nullptr;
                 animation_node = animation_node->next_sibling()) {
              std::string node_name(animation_node->name());
              if (node_name == "frame") {
                Tileset::Tile::AnimationTile animation_tile;
                for (auto attr = animation_node->first_attribute();
                     attr != nullptr;
                     attr = attr->next_attribute()) {
                  std::string attr_name(attr->name());
                  if (attr_name == "tileid") {
                    animation_tile.tile_id = static_cast<uint16_t>(
                      std::atoi(attr->value())
                    );
                  } else if (attr_name == "duration") {
                    animation_tile.duration = static_cast<uint16_t>(
                      60 * std::atof(attr->value()) / 1000
                    );
                  }
                }
                tile.animation_tiles.push_back(animation_tile);
              }
            }
          }
        }
        tileset.tiles.insert({tile_id, tile});
      }
    }
    return tileset;
  }

  void write_tileset(
    const Tileset& tileset,
    uint8_t* buf,
    size_t* buf_size,
    uint32_t** source_offset_entry,
    std::list<uint32_t*>* tile_offset_entries,
    uint32_t** library_offset_entry
  ) {
    uint8_t* p = buf;
    uint16_t* tile_count = reinterpret_cast<uint16_t*>(p);
    p += sizeof(uint16_t);
    uint16_t* w = reinterpret_cast<uint16_t*>(p);
    p += sizeof(uint16_t);
    uint16_t* h = reinterpret_cast<uint16_t*>(p);
    p += sizeof(uint16_t);
    if (source_offset_entry != nullptr) {
      *source_offset_entry = reinterpret_cast<uint32_t*>(p);
    }
    p += sizeof(uint32_t);
    uint8_t* tile_data_count = p;
    p += sizeof(uint8_t);
    for (int i = 0; i < tileset.tiles.size(); i++) {
      if (tile_offset_entries != nullptr) {
        tile_offset_entries->push_back(reinterpret_cast<uint32_t*>(p));
      }
      p += sizeof(uint32_t);
    }
    if (library_offset_entry != nullptr) {
      *library_offset_entry = reinterpret_cast<uint32_t*>(p);
    }
    p += sizeof(uint32_t);
    if (buf != nullptr) {
      *tile_count = tileset.tile_count;
      *w = tileset.tile_w;
      *h = tileset.tile_h;
      *tile_data_count = tileset.tiles.size();
    }
    if (buf_size != nullptr) {
      *buf_size = p - buf;
    }
  }

  struct AnimationTile {
    uint16_t* tile_id;
    uint16_t* duration;
  };

  void write_tileset_tile(
    uint16_t id,
    const Tileset::Tile& tile,
    uint8_t* buf,
    size_t* buf_size,
    std::list<uint32_t*>* collision_box_type_offset_entries,
    uint32_t** library_offset_entry
  ) {
    uint8_t* p = buf;
    uint16_t* tile_id = reinterpret_cast<uint16_t*>(p);
    p += sizeof(uint16_t);
    uint32_t* name = reinterpret_cast<uint32_t*>(p);
    p += sizeof(uint32_t);
    uint16_t* collision_box_types_count = reinterpret_cast<uint16_t*>(p);
    p += sizeof(uint16_t);
    for (int i = 0; i < tile.collision_boxes.size(); i++) {
      if (collision_box_type_offset_entries != nullptr) {
        collision_box_type_offset_entries->push_back(
          reinterpret_cast<uint32_t*>(p)
        );
      }
      p += sizeof(uint32_t);
    }
    uint8_t* animation_tile_count = p;
    p += sizeof(uint8_t);
    std::queue<AnimationTile> animation_tiles;
    for (int i = 0; i < tile.animation_tiles.size(); i++) {
      AnimationTile ptrs;
      ptrs.tile_id = reinterpret_cast<uint16_t*>(p);
      p += sizeof(uint16_t);
      ptrs.duration = reinterpret_cast<uint16_t*>(p);
      p += sizeof(uint16_t);
      animation_tiles.push(ptrs);
    }
    if (library_offset_entry != nullptr) {
      *library_offset_entry = reinterpret_cast<uint32_t*>(p);
    }
    p += sizeof(uint32_t);
    if (buf != nullptr) {
      *tile_id = id;
      *name = tile.name;
      *collision_box_types_count = tile.collision_boxes.size();
      *animation_tile_count = tile.animation_tiles.size();
      for (const auto& tile : tile.animation_tiles) {
        auto ptrs = animation_tiles.front();
        animation_tiles.pop();
        *ptrs.tile_id = tile.tile_id;
        *ptrs.duration = tile.duration;
      }
    }
    if (buf_size != nullptr) {
      *buf_size = p - buf;
    }
  }

  void write_tileset_tile_collision_box_type(
    uint32_t type,
    const util::HashMap<std::vector<Tileset::Tile::CollisionBox>>& lists,
    uint8_t* buf,
    size_t* buf_size,
    std::list<uint32_t*>* collision_box_list_offset_entries
  ) {
    uint8_t* p = buf;
    uint32_t* collision_box_type = reinterpret_cast<uint32_t*>(p);
    p += sizeof(uint32_t);
    uint16_t* collision_box_list_count = reinterpret_cast<uint16_t*>(p);
    p += sizeof(uint16_t);
    for (int i = 0; i < lists.size(); i++) {
      if (collision_box_list_offset_entries != nullptr) {
        collision_box_list_offset_entries->push_back(
          reinterpret_cast<uint32_t*>(p)
        );
      }
      p += sizeof(uint32_t);
    }
    if (buf != nullptr) {
      *collision_box_type = type;
      *collision_box_list_count = lists.size();
    }
    if (buf_size != nullptr) {
      *buf_size = p - buf;
    }
  }

  struct CollisionBox {
    uint16_t* x;
    uint16_t* y;
    uint16_t* w;
    uint16_t* h;
  };

  void write_tileset_tile_collision_box_list(
    uint32_t name,
    const std::vector<Tileset::Tile::CollisionBox>& collision_boxes,
    uint8_t* buf,
    size_t* buf_size
  ) {
    uint8_t* p = buf;
    uint32_t* collision_box_name = reinterpret_cast<uint32_t*>(p);
    p += sizeof(uint32_t);
    uint16_t* collision_box_count = reinterpret_cast<uint16_t*>(p);
    p += sizeof(uint16_t);
    std::queue<CollisionBox> cb_ptrs;
    for (int i = 0; i < collision_boxes.size(); i++) {
      CollisionBox ptrs;
      ptrs.x = reinterpret_cast<uint16_t*>(p);
      p += sizeof(uint16_t);
      ptrs.y = reinterpret_cast<uint16_t*>(p);
      p += sizeof(uint16_t);
      ptrs.w = reinterpret_cast<uint16_t*>(p);
      p += sizeof(uint16_t);
      ptrs.h = reinterpret_cast<uint16_t*>(p);
      p += sizeof(uint16_t);
      cb_ptrs.push(ptrs);
    }
    if (buf != nullptr) {
      *collision_box_name = name;
      *collision_box_count = collision_boxes.size();
      for (const auto& cb : collision_boxes) {
        auto ptrs = cb_ptrs.front();
        cb_ptrs.pop();
        *ptrs.x = cb.x;
        *ptrs.y = cb.y;
        *ptrs.w = cb.w;
        *ptrs.h = cb.h;
      }
    }
    if (buf_size != nullptr) {
      *buf_size = p - buf;
    }
  }

}

/**
 * Converts a PNG image file to an ARGB BMP.
 * With an optional tileset file, will produce an image without the margin and
 * spacing of its input.
 */
#include <fstream>
#include <iostream>
#include <memory>
#include <ultra240-sdk/tileset.h>
#include <png++/png.hpp>
#include <stdexcept>

static void print_usage(const char* self, std::ostream& out) {
  out << "Usage: " << self << " [-h] <in.png> [in.tsx] <out.bmp>" << std::endl;
}

int main(int argc, const char* argv[]) {
  const char* out_fname = argv[2];
  // Check for help option.
  for (int i = 0; i < argc; i++) {
    std::string arg(argv[i]);
    if (arg == "-h" || arg == "--help") {
      print_usage(argv[0], std::cout);
      return 0;
    }
  }
  if (argc != 3 && argc != 4) {
    print_usage(argv[0], std::cerr);
    return 1;
  }
  png::image<png::rgba_pixel> in(argv[1]);
  std::shared_ptr<ultra::sdk::Tileset> tileset;
  int width = in.get_width();
  int height = in.get_height();
  if (argc == 4) {
    out_fname = argv[3];
    tileset.reset(
      new ultra::sdk::Tileset(ultra::sdk::read_tileset(argv[2]))
    );
    height -= 2 * tileset->margin;
    unsigned rows = 0;
    while (height > tileset->tile_h) {
      rows++;
      height -= tileset->tile_h + tileset->spacing;
    }
    if (height != tileset->tile_h) {
      throw std::runtime_error("Incorrect tileset geometry");
    }
    rows++;
    width = tileset->tile_w * tileset->columns;
    height = tileset->tile_h * rows;
  }
  std::ofstream out(out_fname);
  if (!out.is_open()) {
    throw std::runtime_error("Could not open output file");
  }
  // Write header.
  uint32_t header_size = 14 + 108;
  uint32_t pixel_data_size = height * width * 4;
  uint32_t out_size = header_size + pixel_data_size;
  out.put(0x42);
  out.put(0x4d);
  out.put(out_size);
  out.put(out_size >> 8);
  out.put(out_size >> 16);
  out.put(out_size >> 24);
  out.put(0x00);
  out.put(0x00);
  out.put(0x00);
  out.put(0x00);
  out.put(header_size);
  out.put(header_size >> 8);
  out.put(header_size >> 16);
  out.put(header_size >> 24);
  // Write info header.
  out.put(108);
  out.put(0x00);
  out.put(0x00);
  out.put(0x00);
  out.put(width);
  out.put(width >> 8);
  out.put(width >> 16);
  out.put(width >> 24);
  out.put(height);
  out.put(height >> 8);
  out.put(height >> 16);
  out.put(height >> 24);
  out.put(0x01);
  out.put(0x00);
  out.put(0x20);
  out.put(0x00);
  out.put(0x03);
  out.put(0x00);
  out.put(0x00);
  out.put(0x00);
  out.put(pixel_data_size);
  out.put(pixel_data_size >> 8);
  out.put(pixel_data_size >> 16);
  out.put(pixel_data_size >> 24);
  out.put(0x30);
  out.put(0x2e);
  out.put(0x00);
  out.put(0x00);
  out.put(0x30);
  out.put(0x2e);
  out.put(0x00);
  out.put(0x00);
  out.put(0x00);
  out.put(0x00);
  out.put(0x00);
  out.put(0x00);
  out.put(0x00);
  out.put(0x00);
  out.put(0x00);
  out.put(0x00);
  // 0x000000ff
  out.put(0xff);
  out.put(0x00);
  out.put(0x00);
  out.put(0x00);
  // 0x0000ff00
  out.put(0x00);
  out.put(0xff);
  out.put(0x00);
  out.put(0x00);
  // 0x00ff0000
  out.put(0x00);
  out.put(0x00);
  out.put(0xff);
  out.put(0x00);
  // 0xff000000
  out.put(0x00);
  out.put(0x00);
  out.put(0x00);
  out.put(0xff);
  // Magic.
  out.put(0x20);
  out.put(0x6e);
  out.put(0x69);
  out.put(0x57);
  for (int i = 0; i < 48; i++) {
    out.put(0x00);
  }
  // Write pixel data.
  int margin = 0;
  if (tileset) {
    margin = tileset->margin;
  }
  for (int y = in.get_height() - margin - 1; y >= margin; y--) {
    if (tileset) {
      int offset = (y - margin) % (tileset->tile_h + tileset->spacing);
      if (offset >= tileset->tile_h) {
        continue;
      }
    }
    for (int x = margin; x < in.get_width() - margin; x++) {
      if (tileset) {
        int offset = (x - margin) % (tileset->tile_w + tileset->spacing);
        if (offset >= tileset->tile_w) {
          continue;
        }
      }
      auto rgba = in[y][x];
      out.put(rgba.red);
      out.put(rgba.green);
      out.put(rgba.blue);
      out.put(rgba.alpha);
    }
  }
  return 0;
}

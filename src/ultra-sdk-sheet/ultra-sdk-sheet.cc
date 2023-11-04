/**
 * Generate a sprite sheet with every cell labeled with a tile id in hex.
 */
#include <array>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <png++/png.hpp>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

static const std::unordered_map<char, std::array<std::array<int, 3>, 6>>
chars = {
  {'0', {{{1,1,1},
          {1,0,1},
          {1,0,1},
          {1,0,1},
          {1,0,1},
          {1,1,1}}}},
  {'1', {{{0,1,0},
          {0,1,0},
          {0,1,0},
          {0,1,0},
          {0,1,0},
          {0,1,0}}}},
  {'2', {{{1,1,1},
          {0,0,1},
          {0,0,1},
          {0,1,0},
          {1,0,0},
          {1,1,1}}}},
  {'3', {{{1,1,1},
          {0,0,1},
          {1,1,1},
          {0,0,1},
          {0,0,1},
          {1,1,1}}}},
  {'4', {{{1,0,1},
          {1,0,1},
          {1,1,1},
          {0,0,1},
          {0,0,1},
          {0,0,1}}}},
  {'5', {{{1,1,1},
          {1,0,0},
          {1,1,1},
          {0,0,1},
          {0,0,1},
          {1,1,1}}}},
  {'6', {{{1,1,1},
          {1,0,0},
          {1,1,1},
          {1,0,1},
          {1,0,1},
          {1,1,1}}}},
  {'7', {{{1,1,1},
          {0,0,1},
          {0,0,1},
          {0,0,1},
          {0,1,0},
          {0,1,0}}}},
  {'8', {{{1,1,1},
          {1,0,1},
          {1,1,1},
          {1,0,1},
          {1,0,1},
          {1,1,1}}}},
  {'9', {{{1,1,1},
          {1,0,1},
          {1,1,1},
          {0,0,1},
          {0,1,0},
          {0,1,0}}}},
  {'a', {{{0,0,0},
          {0,1,0},
          {0,0,1},
          {1,1,1},
          {1,0,1},
          {0,1,1}}}},
  {'b', {{{0,0,0},
          {1,0,0},
          {1,0,0},
          {1,1,0},
          {1,0,1},
          {1,1,1}}}},
  {'c', {{{0,0,0},
          {0,0,0},
          {0,0,0},
          {1,1,1},
          {1,0,0},
          {1,1,1}}}},
  {'d', {{{0,0,0},
          {0,0,1},
          {0,0,1},
          {0,1,1},
          {1,0,1},
          {1,1,1}}}},
  {'e', {{{0,0,0},
          {1,1,0},
          {1,0,1},
          {1,1,1},
          {1,0,0},
          {0,1,1}}}},
  {'f', {{{0,0,0},
          {0,1,0},
          {1,0,0},
          {1,1,1},
          {1,0,0},
          {1,0,0}}}},
};

static void print_usage(const char* self, std::ostream& out) {
  out << "Usage: " << self << " [-h] <cols> <rows> <out.png>" << std::endl;
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
  if (argc != 4) {
    print_usage(argv[0], std::cerr);
    return 1;
  }
  int cols = std::atoi(argv[1]);
  int rows = std::atoi(argv[2]);
  const char* out_fname = argv[3];
  if (cols <= 0) {
    throw std::runtime_error("Invalid columns argument");
  }
  if (rows <= 0) {
    throw std::runtime_error("Invalid rows argument");
  }
  png::image<png::rgba_pixel> img(cols << 4, rows << 4);
  png::rgba_pixel white(0xff, 0xff, 0xff, 0xff);
  png::rgba_pixel black(0x00, 0x00, 0x00, 0xff);
  png::rgba_pixel gray(0xcc, 0xcc, 0xcc, 0xff);
  png::image<png::rgba_pixel>::row_type even_row(cols << 4);
  png::image<png::rgba_pixel>::row_type odd_row(cols << 4);
  // Draw checkerboard.
  for (int i = 0; i < cols; i++) {
    for (int j = 0; j < 16; j++) {
      if (i % 2) {
        even_row[(i << 4) + j] = gray;
        odd_row[(i << 4) + j] = white;
      } else {
        even_row[(i << 4) + j] = white;
        odd_row[(i << 4) + j] = gray;
      }
    }
  }
  for (int i = 0; i < rows; i++) {
    for (int j = 0; j < 16; j++) {
      if (i % 2) {
        img[(i << 4) + j] = odd_row;
      } else {
        img[(i << 4) + j] = even_row;
      }
    }
  }
  // Draw tile ids.
  unsigned tile_id = 0;
  for (int i = 0; i < rows; i++) {
    for (int j = 0; j < cols; j++) {
      std::stringstream ss;
      ss << std::hex << std::setw(4) << std::setfill('0') << tile_id++;
      std::string num = ss.str();
      for (int k = 0; k < num.size(); k++) {
        for (int l = 0; l < chars.at(num[k]).size(); l++) {
          for (int m = 0; m < chars.at(num[k])[l].size(); m++) {
            if (chars.at(num[k])[l][m]) {
              img.set_pixel(
                (j << 4) + (chars.at(num[k])[l].size() + 1) * k + m,
                (i << 4) + l,
                black
              );
            }
          }
        }
      }
    }
  }
  img.write(out_fname);
}

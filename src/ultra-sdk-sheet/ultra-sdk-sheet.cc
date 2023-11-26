/**
 * Generate a sprite sheet with every cell labeled with a tile id in hex.
 */
#include <array>
#include <fstream>
#include <getopt.h>
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
  out << "Usage: " << self << " [-h] [OPTIONS] -c <cols> -r <rows> -o <out.png>"
      << std::endl
      << "OPTIONS:" << std::endl
      << "  -s  Enable 1 pixel spacing between cells" << std::endl
      << "  -i  Disable printing tile IDs" << std::endl
      << "  -b  Disable checkered background" << std::endl
      << "  -d width:height" << std::endl
      << "      Set tile width and height" << std::endl
    ;
}

int main(int argc, char* argv[]) {
  // Check for help option.
  for (int i = 0; i < argc; i++) {
    std::string arg(argv[i]);
    if (arg == "-h" || arg == "--help") {
      print_usage(argv[0], std::cout);
      return 0;
    }
  }
  int spacing = 0;
  bool ids = true;
  bool bg = true;
  int width = 16;
  int height = 16;
  int cols;
  int rows;
  const char* out_fname = nullptr;
  bool have_cols = false;
  bool have_rows = false;
  int opt;
  while ((opt = getopt(argc, argv, "sibd:c:r:o:")) != -1) {
    switch (opt) {
    case 's':
      spacing = 1;
      break;
    case 'i':
      ids = false;
      break;
    case 'b':
      bg = false;
      break;
    case 'd': {
      std::string dimensions(optarg);
      if (dimensions.find(':') == std::string::npos) {
        print_usage(argv[0], std::cerr);
        return 1;
      }
      width = std::atoi(dimensions.substr(0, dimensions.find(':')).c_str());
      height = std::atoi(dimensions.substr(dimensions.find(':') + 1).c_str());
      if (width <= 0 || height <= 0) {
        std::cerr << argv[0] << ": " 
                  << "width and height must be > 0" << std::endl;
        print_usage(argv[0], std::cerr);
      }
      break;
    }
    case 'c':
      cols = std::atoi(optarg);
      if (cols <= 0) {
        std::cerr << argv[0] << ": " 
                  << "columns must be > 0" << std::endl;
        print_usage(argv[0], std::cerr);
      }
      have_cols = true;
      break;
    case 'r':
      rows = std::atoi(optarg);
      if (rows <= 0) {
        std::cerr << argv[0] << ": " 
                  << "rows must be > 0" << std::endl;
        print_usage(argv[0], std::cerr);
      }
      have_rows = true;
      break;
    case 'o':
      out_fname = optarg;
      break;
    case '?':
      print_usage(argv[0], std::cerr);
      return 1;
    }
  }
  if (optind < argc) {
    std::cerr << argv[0] << ": "
              << "unknown argument -- '" << argv[optind] << "'" << std::endl;
    print_usage(argv[0], std::cerr);
    return 1;
  }
  if (out_fname == nullptr) {
    std::cerr << argv[0] << ": " 
              << "missing out file name" << std::endl;
    print_usage(argv[0], std::cerr);
    return 1;
  }
  if (width < 16 || height < 8) {
    std::cerr << "Warning: Disabling tile IDs because there isn't enough space"
              << std::endl;
    ids = false;
  }
  if (cols <= 0) {
    throw std::runtime_error("Invalid columns argument");
  }
  if (rows <= 0) {
    throw std::runtime_error("Invalid rows argument");
  }
  int pix_width = spacing + (spacing + width) * cols;
  int pix_height = spacing + (spacing + height) * rows;
  png::image<png::rgba_pixel> img(pix_width, pix_height);
  png::rgba_pixel white(0xff, 0xff, 0xff, 0xff);
  png::rgba_pixel black(0x00, 0x00, 0x00, 0xff);
  png::rgba_pixel gray(0xcc, 0xcc, 0xcc, 0xff);
  png::rgba_pixel red(0xff, 0x00, 0x00, 0xff);
  png::image<png::rgba_pixel>::row_type separator(pix_width);
  png::image<png::rgba_pixel>::row_type even_row(pix_width);
  png::image<png::rgba_pixel>::row_type odd_row(pix_width);
  if (spacing) {
    for (int i = 0; i < pix_width; i++) {
      separator[i] = black;
    }
    for (int i = 0; i < cols; i++) {
      separator[1 + width / 2 - 1 + i * width] = red;
      separator[1 + width / 2 + 0 + i * width] = red;
    }
  }
  // Draw checkerboard.
  if (spacing) {
    even_row[pix_width - 1] = black;
    odd_row[pix_width - 1] = black;
  }
  for (int i = 0; i < cols; i++) {
    if (spacing) {
      even_row[i * (spacing + width)] = black;
      odd_row[i * (spacing + width)] = black;
    }
    if (bg) {
      for (int j = 0; j < width; j++) {
        if (i % 2) {
          even_row[spacing + i * (width + spacing) + j] = gray;
          odd_row[spacing + i * (width + spacing) + j] = white;
        } else {
          even_row[spacing + i * (width + spacing) + j] = white;
          odd_row[spacing + i * (width + spacing) + j] = gray;
        }
      }
    }
  }
  if (spacing) {
    img[pix_height - 1] = separator;
  }
  for (int i = 0; i < rows; i++) {
    if (spacing) {
      img[i * (spacing + height)] = separator;
    }
    for (int j = 0; j < height; j++) {
      if (i % 2) {
        img[spacing + i * (height + spacing) + j] = odd_row;
      } else {
        img[spacing + i * (height + spacing) + j] = even_row;
      }
    }
  }
  // Draw tile ids.
  if (ids) {
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
                  j * (width + spacing)
                  + (chars.at(num[k])[l].size() + 1) * k + m,
                  i * (height + spacing)
                  + l,
                  black
                );
              }
            }
          }
        }
      }
    }
  }
  img.write(out_fname);
}

#ifndef cxxmatrix_conway_hpp
#define cxxmatrix_conway_hpp
#include "cxxmatrix.hpp"
#include <cstdint>
#include <algorithm>
#include <vector>

namespace cxxmatrix {

  struct conway_t {
    int width = 100, height = 100;
    std::vector<byte> data1;
    std::vector<byte> data2;

  public:
    void initialize() {
      data1.resize(width * height);
      data2.resize(width * height);
      std::generate(data1.begin(), data1.end(), [] () { return util::rand() & 1; });
    }
    byte const& operator()(int x, int y) const {
      return data1[util::mod(y, height) * width + util::mod(x, width)];
    }

  private:
    byte& get1(int x, int y) {
      return data1[util::mod(y, height) * width + util::mod(x, width)];
    }
    byte& get2(int x, int y) {
      return data2[util::mod(y, height) * width + util::mod(x, width)];
    }

  public:
    std::uint32_t time = 1.0;
    void step(double time) {
      if (time < this->time) return;
      this->time++;

      for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
          int count = 0;
          if (get1(x + 1, y)) count++;
          if (get1(x - 1, y)) count++;
          if (get1(x, y + 1)) count++;
          if (get1(x, y - 1)) count++;
          if (get1(x + 1, y + 1)) count++;
          if (get1(x + 1, y - 1)) count++;
          if (get1(x - 1, y + 1)) count++;
          if (get1(x - 1, y - 1)) count++;
          get2(x, y) = count == 2 ? get1(x, y) : count == 3 ? 1 : 0;
        }
      }
      data1.swap(data2);

      double const prob = (width / 100.0) * (height / 100.0);
      if (util::rand() % std::min<int>(1, 100 / prob)== 0) {
        int const x0 = util::rand() % width;
        int const y0 = util::rand() % height;
        std::uint32_t value = util::rand();
        for (int a = 0; a < 4; a++) {
          for (int b = 0; b < 4; b++) {
            get1(x0 + a, y0 + b) = value & 1;
            value >>= 1;
          }
        }
      }
    }
  };
}

#endif

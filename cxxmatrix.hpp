#ifndef cxxmatrix_hpp
#define cxxmatrix_hpp
#include <cstdint>
#include <random>
#include <limits>

namespace cxxmatrix {
  typedef uint8_t byte;
}

namespace cxxmatrix::util {


inline std::mt19937& rand_engine() {
  static std::random_device seed_gen;
  static std::mt19937 engine(seed_gen());
  return engine;
}
inline std::uint32_t rand() {
  static std::uniform_int_distribution<std::uint32_t> dist(0, std::numeric_limits<std::uint32_t>::max());
  return dist(rand_engine());
}
inline double randf() {
  static std::uniform_real_distribution<double> dist(0, 1.0);
  return dist(rand_engine());
}
inline char32_t rand_char() {
  std::uint32_t r = util::rand() % 80;
  if (r < 10)
    return U'0' + r;
  else
    r -= 10;

  if (r < 46)
    return U'ｰ' + r;
  else
    r -= 46;

  return U"<>*+.:=_|"[r % 9];
}
inline int mod(int value, int modulo) {
  value %= modulo;
  if (value < 0) value += modulo;
  return value;
}

}

#endif

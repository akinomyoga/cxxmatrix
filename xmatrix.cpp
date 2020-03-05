#include <cstdio>
#include <cstdint>
#include <cstddef>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <cassert>

#include <unistd.h>
#include <sys/ioctl.h>
#include <time.h>

#include <vector>
#include <algorithm>
#include <iterator>
#include <random>
#include <limits>

namespace cxxmatrix {

constexpr int xmatrix_frame_interval = 20;
constexpr int xmatrix_decay_rate = 10;
constexpr int xmatrix_cell_power_max = 10;

typedef uint8_t byte;

void xmatrix_msleep(int msec) {
  struct timespec tv;
  tv.tv_sec = 0;
  tv.tv_nsec = msec * 1000000;
  nanosleep(&tv, NULL);
}

std::uint32_t xmatrix_rand() {
  static std::random_device seed_gen;
  static std::mt19937 engine(seed_gen());
  static std::uniform_int_distribution<std::uint32_t> dist(0, std::numeric_limits<std::uint32_t>::max());
  return dist(engine);
  //return std::rand();
}

int mod(int value, int modulo) {
  value %= modulo;
  if (value < 0) value += modulo;
  return value;
}

char32_t xmatrix_rand_char() {
  std::uint32_t r = xmatrix_rand() % 80;
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

struct tcell_t {
  char32_t c = U' ';
  byte fg = 16;
  byte bg = 16;
  bool bold = false;
  int diffuse = 0;
};

enum cell_flags
  {
   cflag_disable_bold = 0x1,
  };

struct cell_t {
  char32_t c = U' ';
  int birth = 0; // 設置時刻
  double power = 0; // 初期の明るさ
  int decay = xmatrix_decay_rate; // 消滅時間
  std::uint32_t flags = 0;

  int stage = 0; // 現在の消滅段階 (0..10)
  int level = 0; // 現在の明るさ(瞬き処理の前) (0..10)

};

struct thread_t {
  int x, y;
  int age, speed;
  double power;
  int decay;
};

struct layer_t {
  int cols, rows;
  int scrollx, scrolly;
  std::vector<cell_t> content;
  std::vector<thread_t> threads;

public:
  void resize(int cols, int rows) {
    content.clear();
    content.resize(cols * rows);
    this->cols = cols;
    this->rows = rows;
    scrollx = 0;
    scrolly = 0;
  }
  cell_t& cell(int x, int y) {
    return content[y * cols + x];
  }
  cell_t& rcell(int x, int y) {
    x = mod(x + scrollx, cols);
    y = mod(y + scrolly, rows);
    return cell(x, y);
  }
  cell_t const& cell(int x, int y) const {
    return const_cast<layer_t*>(this)->cell(x, y);
  }
  cell_t const& rcell(int x, int y) const {
    return const_cast<layer_t*>(this)->rcell(x, y);
  }

public:
  void add_thread(thread_t const& thread) {
    threads.emplace_back(thread);
    threads.back().x += scrollx;
    threads.back().y += scrolly;
  }
  void step_threads(int now) {
    // remove out of range threads
    threads.erase(
      std::remove_if(threads.begin(), threads.end(),
        [this] (auto const& pos) -> bool {
          return pos.y < scrolly || rows + scrolly <= pos.y;
        }), threads.end());

    // grow threads
    for (thread_t& pos : threads) {
      if (pos.age++ % pos.speed == 0) {
        int const x = pos.x - scrollx;
        int const y = pos.y - scrolly;
        if (y < 0 || rows <= y || x < 0 || cols <= x) continue;
        cell_t& cell = this->cell(mod(pos.x, cols), mod(pos.y, rows));
        cell.birth = now;
        cell.power = pos.power;
        cell.decay = pos.decay;
        cell.flags = 0;
        cell.c = xmatrix_rand_char();
        pos.y++;
      }
    }
  }

public:
  void resolve_level(int now) {
    for (int y = 0; y < rows; y++) {
      for (int x = 0; x < cols; x++) {
        cell_t& cell = this->rcell(x, y);
        if (cell.c == ' ') continue;

        int const age = now - cell.birth;
        cell.stage = 10 - age / cell.decay;
        if (cell.stage < 1) {
          cell.c = ' ';
          continue;
        }

        cell.level = std::round(1 + (cell.stage - 1) * cell.power);
        if (xmatrix_rand() % 20 == 0)
          cell.c = xmatrix_rand_char();
      }
    }
  }
};

struct buffer {
  int cols, rows;
  std::vector<tcell_t> old_content;
  std::vector<tcell_t> new_content;
  std::FILE* file;

  layer_t layers[3];

private:
  void put_utf8(char32_t uc) {
    std::uint32_t u = uc;
    if (u < 0x80) {
      std::putc(u, file);
    } else if (u < 0x800) {
      std::putc(0xC0 | (u >> 6), file);
      std::putc(0x80 | (u & 0x3F), file);
    } else if (u < 0x10000) {
      std::putc(0xE0 | (u >> 12), file);
      std::putc(0x80 | (0x3F & u >> 6), file);
      std::putc(0x80 | (0x3F & u), file);
    } else if (u < 0x200000) {
      std::putc(0xF0 | (u >> 18), file);
      std::putc(0x80 | (0x3F & u >> 12), file);
      std::putc(0x80 | (0x3F & u >> 6), file);
      std::putc(0x80 | (0x3F & u), file);
    }
  }

  byte fg;
  byte bg;
  bool bold;
  void sgr0() {
    std::fprintf(file, "\x1b[H\x1b[m");
    px = py = 0;
    fg = 0;
    bg = 0;
    bold = false;
  }
  void set_color(tcell_t const& tcell) {
    if (tcell.bg != this->bg) {
      this->bg = tcell.bg;
      std::fprintf(file, "\x1b[48;5;%dm", this->bg);
    }
    if (tcell.c != ' ') {
      if (tcell.fg != fg) {
        this->fg = tcell.fg;
        std::fprintf(file, "\x1b[38;5;%dm", this->fg);
      }
      if (tcell.bold != bold) {
        this->bold = tcell.bold;
        std::fprintf(file, "\x1b[%dm", this->bold ? 1 : 22);
      }
    }
  }

private:
  int px = -1, py = -1;
  void goto_xy(int x, int y) {
    if (y == py) {
      if (x != px) {
        if (x == 0) {
          std::putc('\r', file);
        } else if (px - 3 <= x && x < px) {
          while (x < px--)
            std::putc('\b', file);
        } else {
          std::fprintf(file, "\x1b[%dG", x + 1);
        }
        px = x;
      }
      return;
    }

    // \v が思うように動いていない?
    // if (x <= px && py < y && (x == 0 ? 1 : px - x) + (y - py) <= (x == 0 || x == px ? 3 : 5)) {
    //   if (x != px) {
    //     if (x == 0) {
    //       std::putc('\r', file);
    //       px = 0;
    //     } else {
    //       while (x < px--)
    //         std::putc('\b', file);
    //     }
    //   }
    //   while (py++ < y)
    //     std::putc('\v', file);
    //   return;
    // }

    if (x == 0) {
      std::fprintf(file, "\x1b[%dH", y + 1);
      px = x;
      py = y;
      return;
    } else if (x == px) {
      if (y < py) {
        std::fprintf(file, "\x1b[%dA", py - y);
      } else {
        std::fprintf(file, "\x1b[%dB", y - py);
      }
      py = y;
      return;
    }

    std::fprintf(file, "\x1b[%d;%dH", y + 1, x + 1);
    px = x;
    py = y;
  }

public:
  void redraw() {
    goto_xy(0, 0);
    for (int y = 0; y < rows; y++) {
      for (int x = 0; x < cols; x++) {
        tcell_t const& tcell = new_content[y * cols + x];
        set_color(tcell);
        put_utf8(tcell.c);
      }
    }
    std::fflush(file);

    old_content.resize(new_content.size());
    for (std::size_t i = 0; i < new_content.size(); i++)
      old_content[i] = new_content[i];
  }

  void draw_content() {
    for (int y = 0; y < rows; y++) {
      for (int x = 0; x < cols; x++) {
        std::size_t const index = y * cols + x;
        tcell_t const& ncell = new_content[index];
        tcell_t const& ocell = old_content[index];
        if (ncell.bg != ocell.bg || ncell.c != ocell.c || (ncell.c != ' ' && ncell.fg != ocell.fg)) {
          goto_xy(x, y);
          set_color(ncell);
          put_utf8(ncell.c);
          px++;
          old_content[index] = new_content[index];
        }
      }
    }
    std::fflush(file);
  }

private:
  void clear_diffuse() {
    for (int y = 0; y < rows; y++) {
      for (int x = 0; x < cols; x++) {
        std::size_t const index = y * cols + x;
        tcell_t& tcell = new_content[index];
        tcell.diffuse = 0;
        tcell.bg = color_table[0];
      }
    }
  }
  void add_diffuse(int x, int y, int value) {
    if (0 <= y && y < rows && 0 <= x && x < cols && value > 0) {
      std::size_t const index = y * cols + x;
      tcell_t& tcell = new_content[index];
      tcell.diffuse += value;
    }
  }
  void resolve_diffuse() {
    for (int y = 0; y < rows; y++) {
      for (int x = 0; x < cols; x++) {
        std::size_t const index = y * cols + x;
        tcell_t& tcell = new_content[index];
        tcell.bg = color_table[std::min(tcell.diffuse / 3, 3)];
      }
    }
  }

public:
  int now = 100;

private:
  //byte color_table[11] = {16, 22, 28, 34, 40, 46, 83, 120, 157, 194, 231, };
  byte color_table[11] = {16, 22, 28, 35, 41, 47, 84, 121, 157, 194, 231, };
  //byte color_table[11] = {16, 22, 29, 35, 42, 48, 85, 121, 158, 194, 231, };

  cell_t const* rend_cell(int x, int y) {
    for (auto& layer: layers) {
      auto const& cell = layer.rcell(x, y);
      if (cell.c != ' ') return &cell;
    }
    return nullptr;
  }

  void construct_render_content() {
    clear_diffuse();
    for (int y = 0; y < rows; y++) {
      for (int x = 0; x < cols; x++) {
        std::size_t const index = y * cols + x;
        tcell_t& tcell = new_content[index];

        cell_t const* lcell = this->rend_cell(x, y);
        if (!lcell) {
          tcell.c = ' ';
          continue;
        }

        tcell.c = lcell->c;

        int level = lcell->level;

        // 瞬き処理
        if (level > 2)
          level -= xmatrix_rand() % (level > 5 ? 3 : 2);
        else if (level == 2)
          level = xmatrix_rand() % 4 ? 2 : 1;
        else
          level = xmatrix_rand() % 6 ? 1 : 0;

        tcell.fg = color_table[level];
        tcell.bold = !(lcell->flags & cflag_disable_bold) && lcell->stage > 5;

        tcell.diffuse += level / 3;
        add_diffuse(x - 1, y, level / 3 - 1);
        add_diffuse(x + 1, y, level / 3 - 1);
        add_diffuse(x, y - 1, level / 3 - 1);
        add_diffuse(x, y + 1, level / 3 - 1);
        add_diffuse(x - 1, y - 1, level / 5 - 1);
        add_diffuse(x + 1, y - 1, level / 5 - 1);
        add_diffuse(x - 1, y + 1, level / 5 - 1);
        add_diffuse(x + 1, y + 1, level / 5 - 1);
      }
    }

    resolve_diffuse();
  }

public:
  void render_direct() {
    now++;
    this->draw_content();
  }
  void render_layers() {
    now++;
    for (auto& layer: layers) {
      layer.step_threads(now);
      layer.resolve_level(now);
    }
    this->construct_render_content();
    this->draw_content();
  }

  bool term_altscreen = false;
  void term_leave() {
    if (!term_altscreen) return;
    term_altscreen = false;
    std::fprintf(file, "\x1b[%dH\n", rows);
    std::fprintf(file, "\x1b[?1049l\x1b[?25h");
    std::fflush(file);
  }
  void term_enter() {
    if (term_altscreen) return;
    term_altscreen = true;
    std::fprintf(file, "\x1b[?1049h\x1b[?25l");
    sgr0();
    redraw();
    std::fflush(file);
  }

  void initialize() {
    struct winsize ws;
    ioctl(STDIN_FILENO, TIOCGWINSZ, (char*) &ws);
    cols = ws.ws_col;
    rows = ws.ws_row;
    file = stdout;
    new_content.clear();
    new_content.resize(cols * rows);

    for (auto& layer : layers)
      layer.resize(cols, rows);
  }

  void finalize() {
    term_leave();
  }


private:
  double s3rain_scroll_func(double value) {
    value = value / 200.0 - 10.0;
    constexpr double tanh_range = 2.0;
    static double th1 = std::tanh(tanh_range);
    value = std::max(value, -tanh_range * 2.0);

    if (value < -tanh_range) {
      return -th1 + (1.0 - th1 * th1) * (value + tanh_range);
    } else if (value < tanh_range) {
      return std::tanh(value);
    } else {
      return th1 + (1.0 - th1 * th1) * (value - tanh_range);
    }
  }

public:
  void s3rain() {
    static byte speed_table[] = {2, 2, 2, 2, 3, 3, 6, 6, 6, 7, 7, 8, 8, 8};
    double const scr0 = s3rain_scroll_func(0);
    for (std::uint32_t loop = 0; loop < 2800; loop++) {
      // add new threads
      if (now % (1 + 150 / cols) == 0) {
        thread_t thread;
        thread.x = xmatrix_rand() % cols;
        thread.y = 0;
        thread.age = 0;
        thread.speed = speed_table[xmatrix_rand() % std::size(speed_table)];
        thread.power = 2.0 / thread.speed;
        thread.decay = xmatrix_decay_rate;

        int const layer = thread.speed < 3 ? 0 : thread.speed < 5 ? 1 : 2;
        layers[layer].add_thread(thread);
      }

      double const scr = s3rain_scroll_func(loop) - scr0;
      layers[0].scrollx = -std::round(500 * scr);
      layers[1].scrollx = -std::round(50 * scr);
      layers[2].scrollx = +std::round(200 * scr);

      layers[0].scrolly = -std::round(25 * scr);
      layers[1].scrolly = +std::round(20 * scr);
      layers[2].scrolly = +std::round(45 * scr);

      render_layers();
      xmatrix_msleep(xmatrix_frame_interval);
    }
    std::uint32_t const wait = 8 * rows + 100;
    for (std::uint32_t loop = 0; loop < wait; loop++) {
      render_layers();
      xmatrix_msleep(xmatrix_frame_interval);
    }
  }

private:
  void s1number_fill_numbers(int stripe) {
    for (int y = 0; y < rows; y++) {
      for (int x = 0; x < cols; x++) {
        tcell_t& tcell = new_content[y * cols + x];
        cell_t& cell = layers[1].rcell(x, y);
        if (stripe && x % stripe == 0) {
          cell.c = ' ';
          tcell.c = ' ';
        } else {
          cell.c = U'0' + xmatrix_rand() % 10;
          cell.birth = now - xmatrix_decay_rate * std::size(color_table) / 2 + xmatrix_rand_char() % xmatrix_decay_rate;
          cell.power = 1.0;
          cell.decay = xmatrix_decay_rate;
          cell.flags = 0;
          tcell.c = cell.c;
          tcell.fg = color_table[std::size(color_table) / 2 + xmatrix_rand_char() % 3];
        }
      }
    }
  }

public:
  void s1number() {
    int stripe_periods[] = {0, 32, 16, 8, 4, 2, 2, 2};
    for (int stripe: stripe_periods) {
      for (int i = 0; i < 20; i++) {
        s1number_fill_numbers(stripe);
        render_direct();
        xmatrix_msleep(xmatrix_frame_interval);
      }
    }
  }

private:
  struct glyph_definition_t {
    char32_t c;
    int w;
    int lines[7];

    bool operator()(int x, int y) const {
      return lines[y] & (1 << x);
    }
  };
  struct glyph_t {
    int h, w;
    int render_width;
    glyph_definition_t const* def;
  public:
    bool operator()(int x, int y) const {
      return def && (*def)(x, y);
    }
  };
  std::vector<glyph_t> message;
  int message_width = 0;
  int s2banner_min_render_width = 0;

  static constexpr int s2banner_initial_input = 40;
  static constexpr int s2banner_cell_width = 10;
  static constexpr int s2banner_cell_height = 7;
  static constexpr std::size_t s2banner_max_message_size = 0x1000;

  void s2banner_initialize(std::vector<char32_t> const& msg) {
    static glyph_definition_t glyph_defs[] = {
#include "glyph.inl"
    };
    static std::unordered_map<char32_t, glyph_definition_t const*> map;
    if (map.empty()) {
      for (auto const& def : glyph_defs)
        map[def.c] = &def;
    }

    message.clear();
    message_width = 0;
    for (char32_t c: msg) {
      if (U'a' <= c && c <= U'z')
        c = c - U'a' + U'A';

      auto it = map.find(c);
      if (it == map.end() && c != ' ')
        it = map.find(U'\uFFFD');

      glyph_t g;
      g.def = it != map.end() ? it->second : nullptr;
      g.h = 7;
      g.w = g.def ? g.def->w : 5;
      g.render_width = g.w + 1;

      if (message.size()) message_width++;
      message_width += g.w;
      message.push_back(g);
    }

    // Adjust rendering width
    int rest = cols - message_width - 4;
    while (rest > 0) {
      int min_width = message[0].render_width;
      int min_width_count = 0;
      for (glyph_t const& g: message) {
        if (g.render_width < min_width) {
          min_width = g.render_width;
          min_width_count = 1;
        } else if (g.render_width == min_width) {
          min_width_count++;
        }
      }

      if (min_width >= s2banner_cell_width * 3 / 2) break;
      rest -= min_width_count;
      if (rest < 0) break;

      for (glyph_t& g: message)
        if (g.render_width == min_width) g.render_width++;
      message_width += min_width_count;
      s2banner_min_render_width = min_width + 1;
    }
  }

  void s2banner_write_letter(int x0, int y0, glyph_t const& glyph, int type) {
    x0 += (glyph.render_width - 1 - glyph.w) / 2;
    for (int y = 0; y < glyph.h; y++) {
      if (y0 + y >= rows) continue;
      for (int x = 0; x < glyph.w; x++) {
        if (x0 + x >= cols) continue;
        if (glyph(x, y)) s2banner_set_char(x0, y0, x, y, type);
      }
    }
  }

  void s2banner_write_caret(int x0, int y0, bool set, int type) {
    x0 += std::max(0, (s2banner_min_render_width - 1 - s2banner_cell_width) / 2);
    for (int y = 0; y < s2banner_cell_height; y++) {
      if (y0 + y >= rows) continue;
      for (int x = 0; x < s2banner_cell_width - 1; x++) {
        if (x0 + x >= cols) continue;
        s2banner_set_char(x0, y0, x, y, set ? type : 0);
      }
    }
  }

  void s2banner_put_char(int x0, int y0, int x, int y, int type, char32_t uchar) {
    if (type == 0) {
      cell_t& cell = layers[0].cell(x0 + x, y0 + y);
      cell.c = ' ';
    } else if (type == 1) {
      cell_t& cell = layers[0].cell(x0 + x, y0 + y);
      cell.c = uchar;
      cell.birth = now;
      cell.power = 1.0;
      cell.decay = 2;
      cell.flags = 0;
    } else if (type == 2) {
      s2banner_put_char(x0, y0, x, y, uchar, 1);

      thread_t thread;
      thread.x = x0 + x;
      thread.y = y0 + y;
      thread.age = 0;
      thread.speed = s2banner_cell_height - y;
      if (thread.speed > 2) thread.speed += xmatrix_rand() % 3 - 1;
      thread.power = 2.0 / 3.0;
      thread.decay = 3;
      layers[1].add_thread(thread);
    }
  }
  void s2banner_set_char(int x0, int y0, int x, int y, int type) {
    if (type == 0)
      s2banner_put_char(x0, y0, x, y, type, ' ');
    else
      s2banner_put_char(x0, y0, x, y, type, xmatrix_rand_char());
  }

  void s2banner_add_thread() {
    if (now % (1 + 2000 / cols) == 0) {
      thread_t thread;
      thread.x = xmatrix_rand() % cols;
      thread.y = 0;
      thread.age = 0;
      thread.speed = 8;
      thread.power = 0.5;
      thread.decay = xmatrix_decay_rate;
      layers[1].add_thread(thread);
    }
  }

  static void s2banner_decode(std::vector<char32_t>& msg, const char* msg_u8) {
    while (msg.size() < s2banner_max_message_size && *msg_u8) {
      std::uint32_t code = (byte) *msg_u8++;
      int remain;
      std::uint32_t min_code;
      if (code < 0xC0) {
        if (code >= 0x80) goto error_char;
        remain = 0;
        min_code = 0;
      } else if (code < 0xE0) {
        remain = 1;
        min_code = 1 << 7;
      } else if (code < 0xF0) {
        remain = 2;
        min_code = 1 << 11;
      } else if (code < 0xF8) {
        remain = 3;
        min_code = 1 << 16;
      } else if (code < 0xFC) {
        remain = 4;
        min_code = 1 << 21;
      } else if (code < 0xFE) {
        remain = 5;
        min_code = 1 << 26;
      } else {
        goto error_char;
      }

      if (remain) code &= (1 << (6 - remain)) - 1;
      while (remain-- && 0x80 <= (byte) *msg_u8 && (byte) *msg_u8 < 0xC0)
        code = code << 6 | (*msg_u8++ & 0x3F);
      if (code < min_code) goto error_char;
      msg.push_back(code);
      continue;
    error_char:
      msg.push_back(0xFFFD);
    }
  }
public:
  void s2banner(const char* msg_u8) {
    std::vector<char32_t> msg;
    s2banner_decode(msg, msg_u8);
    s2banner_initialize(msg);
    int nchar = (int) msg.size();

    int mode = 1, display_width = nchar + 1, display_height = 1;
    if (message_width < cols) {
      nchar = message.size();
      display_width = message_width + s2banner_min_render_width;
      display_height = message[0].h;
      mode = 0;
    } else if (nchar * 2 < cols) {
      mode = 2;
      display_width *= 2;
    }

    // 最後に文字入力が起こった位置と時刻
    int input_index = -1;
    int input_time = 0;

    int loop_max = s2banner_initial_input + nchar * 5 + 130;
    for (int loop = 0; loop <= loop_max; loop++) {
      int type = 1;
      if (loop == loop_max) type = 2;

      int x0 = (cols - display_width) / 2, y0 = (rows - display_height) / 2;
      if (mode != 0 && xmatrix_rand() % 20 == 0)
        y0 += xmatrix_rand() % 7 - 3;
      for (int i = 0; i < nchar; i++) {
        if ((loop - s2banner_initial_input) / 5 <= i) break;

        bool caret_moved = false;
        if (input_index < i) {
          input_index = i;
          input_time = loop;
          caret_moved = true;
        }

        switch (mode) {
        case 0:
          {
            glyph_t const& g = message[i];
            if (caret_moved)
              s2banner_write_caret(x0, y0, false, type);
            s2banner_write_letter(x0, y0, g, type);
            x0 += g.render_width;
          }
          break;
        default:
          {
            char32_t c = msg[i];
            if (U'a' <= c && c <= U'z') c = c - U'a' + U'A';
            s2banner_put_char(x0, y0, 0, 0, type, c);
          }
          x0 += mode;
          break;
        }
      }

      switch (mode) {
      case 0:
        // if (!((loop - input_time) / 25 & 1))
        //   s2banner_write_caret(x0, y0, true);
        s2banner_write_caret(x0, y0, !((loop - input_time) / 25 & 1), type);
        //s2banner_write_caret(x0, y0, true);
        break;
      default:
        s2banner_put_char(x0, y0, 0, 0, type, U'\u2589');
        break;
      }

      s2banner_add_thread();
      render_layers();
      xmatrix_msleep(xmatrix_frame_interval);
    }
  }

private:
  struct conway_board_t {
    int width = 100, height = 100;
    std::vector<byte> data1;
    std::vector<byte> data2;

  public:
    void initialize() {
      data1.resize(width * height);
      data2.resize(width * height);
      std::generate(data1.begin(), data1.end(), [] () { return xmatrix_rand() & 1; });
    }
    byte const& operator()(int x, int y) const {
      return data1[mod(y, height) * width + mod(x, width)];
    }

  private:
    byte& get1(int x, int y) {
      return data1[mod(y, height) * width + mod(x, width)];
    }
    byte& get2(int x, int y) {
      return data2[mod(y, height) * width + mod(x, width)];
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
      if (xmatrix_rand() % std::min<int>(1, 100 / prob)== 0) {
        int const x0 = xmatrix_rand() % width;
        int const y0 = xmatrix_rand() % height;
        std::uint32_t value = xmatrix_rand();
        for (int a = 0; a < 4; a++) {
          for (int b = 0; b < 4; b++) {
            get1(x0 + a, y0 + b) = value & 1;
            value >>= 1;
          }
        }
      }
    }
  };

  conway_board_t s4conway_board;

  void s4conway_frame_mesh(double theta, double scal, double power) {
    int ox = cols / 2, oy = rows / 2;
    for (int y = 0; y < rows; y++) {
      for (int x = 0; x < cols; x++) {
        cell_t& cell = layers[2].rcell(x, y);

        double const x1 = 0.5 * (x - ox);
        double const y1 = oy - y;
        double const u = 0.5 + scal * (x1 * std::cos(theta) - y1 * std::sin(theta));
        double const v = 0.5 + scal * (y1 * std::cos(theta) + x1 * std::sin(theta));
        if (s4conway_board(std::ceil(u), std::ceil(v))) {
          cell.c = xmatrix_rand_char();
          cell.birth = now;
          cell.power = power;
          cell.decay = 10;
          cell.flags = cflag_disable_bold;
          continue;
        }

        if (power >= 0.4) {
          double const dx1A = 0.25, dy1A = +0.5;
          double const dx1B = 0.25, dy1B = -0.5;
          double const duA = scal * (dx1A * std::cos(theta) - dy1A * std::sin(theta));
          double const dvA = scal * (dy1A * std::cos(theta) + dx1A * std::sin(theta));
          double const duB = scal * (dx1B * std::cos(theta) - dy1B * std::sin(theta));
          double const dvB = scal * (dy1B * std::cos(theta) + dx1B * std::sin(theta));
          bool sec = std::ceil(u + duA) != std::ceil(u - duA) ||
            std::ceil(v + dvA) != std::ceil(v - dvA) ||
            std::ceil(u + duB) != std::ceil(u - duB) ||
            std::ceil(v + dvB) != std::ceil(v - dvB);
          if (sec) {
            cell.c = xmatrix_rand_char();
            cell.birth = now;
            cell.power = power * 0.2;
            cell.decay = 10;
            cell.flags = cflag_disable_bold;
            continue;
          }
        }

        cell.c = ' ';
      }
    }
  }
public:
  void s4conway() {
    s4conway_board.initialize();
    double time = 0.0;
    double distance = 1.0;

    std::uint32_t loop;
    for (loop = 0; loop < 2000; loop++) {
      distance += 1.0 * (loop > 1500 ? distance * 0.01 : 0.04);
      time += 0.005 * distance;
      s4conway_board.step(time);
      s4conway_frame_mesh(0.5 + loop * 0.01, 0.01 * distance, std::min(0.8, 3.0 / std::sqrt(distance)));
      render_layers();
      xmatrix_msleep(xmatrix_frame_interval);
    }
  }

};

buffer buff;

void trap_sigint(int sig) {
  buff.finalize();
  std::signal(sig, SIG_DFL);
  std::raise(sig);
  std::exit(128 + sig);
}
void trap_sigwinch(int) {
  buff.initialize();
  buff.redraw();
}
void traptstp(int sig) {
  buff.term_leave();
  std::signal(sig, SIG_DFL);
  std::raise(sig);
}
void trapcont(int) {
  buff.term_enter();
  std::signal(SIGTSTP, traptstp);
}

} /* end of namespace cxxmatrix */

using namespace cxxmatrix;

int main(int argc, char** argv) {
  std::signal(SIGINT, trap_sigint);
  std::signal(SIGWINCH, trap_sigwinch);
  std::signal(SIGTSTP, traptstp);
  std::signal(SIGCONT, trapcont);

  buff.initialize();
  buff.term_enter();

  buff.s1number();
  for (int i = 1; i < argc; i++)
    buff.s2banner(argv[i]);
  buff.s3rain();
  buff.s4conway();

  buff.finalize();
  return 0;
}

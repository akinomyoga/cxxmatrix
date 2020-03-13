#include <cstdio>
#include <cstdint>
#include <cstddef>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <cmath>

#include <unistd.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <poll.h>

#include <vector>
#include <algorithm>
#include <iterator>
#include <unordered_map>
#include <chrono>
#include <thread>
#include <functional>

#include "cxxmatrix.hpp"
#include "mandel.hpp"
#include "conway.hpp"

namespace cxxmatrix::config {
  constexpr std::chrono::milliseconds frame_interval {40};
  constexpr int default_decay = 100; // 既定の寿命
}

namespace cxxmatrix {

struct frame_scheduler {
  using clock_type = std::chrono::high_resolution_clock;
  clock_type::time_point prev;
  frame_scheduler() {
    prev = clock_type::now();
  }
  void next_frame() {
    clock_type::time_point until = prev + config::frame_interval;
    clock_type::time_point now = clock_type::now();
    if (until > now)
      std::this_thread::sleep_for(until - now);
    prev = now;
  }
};

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
  double decay = config::default_decay; // 寿命
  std::uint32_t flags = 0;

  double stage = 0; // 現在の消滅段階 (0..1.0)
  double current_power = 0; // 現在の明るさ(瞬き処理の前) (0..1.0)
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
    x = util::mod(x + scrollx, cols);
    y = util::mod(y + scrolly, rows);
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
          int const y = pos.y - scrolly;
          return y < 0 || rows <= y;
        }), threads.end());

    // grow threads
    for (thread_t& pos : threads) {
      if (pos.age++ % pos.speed == 0) {
        cell_t& cell = this->cell(util::mod(pos.x, cols), util::mod(pos.y, rows));
        cell.birth = now;
        cell.power = pos.power;
        cell.decay = pos.decay;
        cell.flags = 0;
        cell.c = util::rand_char();
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
        cell.stage = 1.0 - age / cell.decay;
        if (cell.stage < 0.0) {
          cell.c = ' ';
          continue;
        }

        cell.current_power = cell.power * cell.stage;
        if (util::rand() % 20 == 0)
          cell.c = util::rand_char();
      }
    }
  }
};

typedef std::uint32_t key_t;

enum key_flags {
  key_up    = 0x110000,
  key_down  = 0x110001,
  key_right = 0x110002,
  key_left  = 0x110003,
};
inline constexpr key_t key_ctrl(key_t k) { return k & 0x1F; }

struct key_reader {
  bool term_internal = false;
  struct termios term_termios_save;
  bool term_nonblock_save = false;

  std::function<void(key_t)> proc;

public:
  void leave() {
    if (!term_internal) return;
    term_internal = false;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &this->term_termios_save);
  }
  void enter() {
    if (term_internal) return;
    term_internal = true;

    tcgetattr(STDIN_FILENO, &this->term_termios_save);
    struct termios termios = this->term_termios_save;
    termios.c_lflag &= ~(ECHO | ICANON | IEXTEN); // シグナルは使うので ISIG は消さない
    termios.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    termios.c_cflag &= ~(CSIZE | PARENB);
    termios.c_cflag |= CS8;
    termios.c_oflag &= ~(OPOST);
    termios.c_cc[VMIN]  = 1;
    termios.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &termios);
  }

private:
  void process_key(key_t k) {
    if (proc) proc(k);
  }

  bool esc = false;
  void process_byte(byte b) {
    if (b == 0x1b) {
      esc = true;
      return;
    }
    if (esc) {
      if (0x40 <= b && b < 0x80) {
        switch (b) {
        case 'A': esc = false; process_key(key_up   ); break;
        case 'B': esc = false; process_key(key_down ); break;
        case 'C': esc = false; process_key(key_right); break;
        case 'D': esc = false; process_key(key_left ); break;
        case '[': break;
        case 'O': break;
        default: esc = false; break;
        }
      } else if (0x80 <= b) {
        process_key(0x1b);
        process_key(b);
        esc = false;
      }
    } else {
      process_key(b);
    }
  }
  static ssize_t nonblock_read(int fd, byte* buffer, ssize_t size) {
    struct pollfd pollfd;
    pollfd.fd = fd;
    pollfd.events = POLLIN | POLLERR;
    poll(&pollfd, 1, 0);
    if (pollfd.revents & POLLIN)
      return read(fd, buffer, size);
    return 0;
  }
public:
  void process() {
    byte buffer[1024];
    ssize_t nread;
    while ((nread = nonblock_read(STDIN_FILENO, buffer, 1024)) > 0) {
      for (ssize_t i = 0; i < nread; i++)
        process_byte(buffer[i]);
    }
  }
};


enum scene_t {
  scene_none         = 0,
  scene_number       = 1,
  scene_banner       = 2,
  scene_rain         = 3,
  scene_conway       = 4,
  scene_mandelbrot   = 5,
  scene_rain_forever = 6,
  scene_loop = 99,

  scene_count = 6,
};

struct buffer {
  int cols, rows;
  std::vector<tcell_t> old_content;
  std::vector<tcell_t> new_content;
  std::FILE* file;

  layer_t layers[3];

  frame_scheduler scheduler;
  key_reader kreader;

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

private:

  static bool is_changed(tcell_t const& ncell, tcell_t const& ocell) {
    if (ncell.c != ocell.c || ncell.bg != ocell.bg) return true;
    if (ncell.c == ' ') return false;
    if (ncell.fg != ocell.fg || ncell.bold != ocell.bold) return true;
    return false;
  }
  bool term_draw_cell(int x, int y, std::size_t index, bool force_write) {
    tcell_t& ncell = new_content[index];
    tcell_t& ocell = old_content[index];
    if (force_write || is_changed(ncell, ocell)) {
      goto_xy(x, y);
      set_color(ncell);
      put_utf8(ncell.c);
      px++;
      ocell = ncell;
      return true;
    }
    return false;
  }

public:
  void redraw() {
    goto_xy(0, 0);
    for (int y = 0; y < rows; y++) {
      for (int x = 0; x < cols; x++) {
        // 行末 xenl 対策
        if (y == rows - 1) {
          if (x == cols - 2) {
            tcell_t const& cell = new_content[y * cols + x + 1];
            set_color(cell);
            put_utf8(cell.c);
            std::fprintf(file, "\b\x1b[@");
          } else if (x == cols -1) {
            continue;
          }
        }

        tcell_t const& tcell = new_content[y * cols + x];
        set_color(tcell);
        put_utf8(tcell.c);
      }
    }
    std::fprintf(file, "\x1b[H");
    std::fflush(file);

    old_content.resize(new_content.size());
    for (std::size_t i = 0; i < new_content.size(); i++)
      old_content[i] = new_content[i];
  }

  void draw_content() {
    for (int y = 0; y < rows; y++) {
      for (int x = 0; x < cols - 1; x++) {
        std::size_t const index = y * cols + x;

        bool dirty = true;

        // 行末 xenl 対策
        if (x == cols - 2 && term_draw_cell(x, y, index + 1, false)) {
          std::fprintf(file, "\b\x1b[@");
          px--;
          dirty = true;
        }

        term_draw_cell(x, y, index, dirty);
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
        tcell.bg = color_table[std::min<int>(0.4 * tcell.diffuse, 3)];
      }
    }
  }

public:
  static constexpr double default_twinkle = 0.2;
  int now = 100;
  double twinkle = default_twinkle;

private:
  //byte color_table[11] = {16, 22, 28, 34, 40, 46, 83, 120, 157, 194, 231, };
  byte color_table[11] = {16, 22, 28, 35, 41, 47, 84, 121, 157, 194, 231, };
  //byte color_table[11] = {16, 22, 29, 35, 42, 48, 85, 121, 158, 194, 231, };

  void clear_content() {
    for (auto& tcell: new_content) {
      tcell.c = ' ';
      tcell.fg = color_table[0];
      tcell.bg = color_table[0];
      tcell.bold = false;
    }
  }

  cell_t const* rend_cell(int x, int y, double& power) {
    cell_t const* ret = nullptr;
    for (auto& layer: layers) {
      auto const& cell = layer.rcell(x, y);
      if (cell.c != ' ') {
        if (!ret) ret = &cell;
        if (cell.current_power > power) power = cell.current_power;
      }
    }
    return ret;
  }

  void construct_render_content() {
    clear_diffuse();
    for (int y = 0; y < rows; y++) {
      for (int x = 0; x < cols; x++) {
        std::size_t const index = y * cols + x;
        tcell_t& tcell = new_content[index];

        double current_power = 0.0;
        cell_t const* lcell = this->rend_cell(x, y, current_power);
        if (!lcell) {
          tcell.c = ' ';
          continue;
        }

        tcell.c = lcell->c;

        // current_power = 現在の輝度 (瞬き)
        if (twinkle != 0.0) {
          current_power -= std::hypot(current_power * twinkle, 0.1) * util::randf();
          if (current_power < 0.0) current_power = 0.0;
        }

        // level = 色番号
        double const fractional_level = util::interpolate(current_power, 0.6, std::size(color_table));
        int level = fractional_level;
        if (twinkle != 0.0 && util::randf() > fractional_level - level) level++;
        level = std::min<int>(level, std::size(color_table) - 1);

        tcell.fg = color_table[level];
        tcell.bold = !(lcell->flags & cflag_disable_bold) && lcell->stage > 0.5;

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

  bool term_internal = false;
  void term_leave() {
    if (!term_internal) return;
    term_internal = false;
    std::fprintf(file, "\x1b[m\x1b[%dH\n", rows);
    std::fprintf(file, "\x1b[?1049l\x1b[?25h");
    std::fflush(file);
    kreader.leave();
  }
  void term_enter() {
    if (term_internal) return;
    term_internal = true;
    kreader.enter();
    std::fprintf(file, "\x1b[?1049h\x1b[?25l");
    sgr0();
    redraw();
    std::fflush(file);
  }

  bool is_menu = false;
  void process_key(key_t k) {
    if (is_menu) {
      menu_process_key(k);
    } else {
      switch (k) {
      case key_ctrl('m'):
      case key_ctrl('j'):
        menu_initialize();
        break;
      }
    }
  }

  void initialize() {
    kreader.proc = [this] (key_t k) { this->process_key(k); };
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


public:
  static double s3rain_scroll_func_tanh(double value) {
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
  static double s3rain_scroll_func_const(double) {
    return 0.0;
  }

public:
  void s3rain(std::uint32_t nloop, double (*scroll_func)(double)) {
    static byte speed_table[] = {2, 2, 2, 2, 3, 3, 6, 6, 6, 7, 7, 8, 8, 8};

    double const scr0 = scroll_func(0);
    int initial_scrollx[3];
    int initial_scrolly[3];
    for (int i = 0; i < 3; i++) {
      initial_scrollx[i] = layers[i].scrollx;
      initial_scrolly[i] = layers[i].scrolly;
    }

    for (std::uint32_t loop = 0; nloop == 0 || loop < nloop; loop++) {
      // add new threads
      if (now % (1 + 150 / cols) == 0) {
        thread_t thread;
        thread.x = util::rand() % cols;
        thread.y = 0;
        thread.age = 0;
        thread.speed = speed_table[util::rand() % std::size(speed_table)];
        thread.power = 2.0 / thread.speed;
        thread.decay = config::default_decay;

        int const layer = thread.speed < 3 ? 0 : thread.speed < 5 ? 1 : 2;
        layers[layer].add_thread(thread);
      }

      double const scr = scroll_func(loop) - scr0;
      layers[0].scrollx = initial_scrollx[0] - std::round(500 * scr);
      layers[1].scrollx = initial_scrollx[1] - std::round(50 * scr);
      layers[2].scrollx = initial_scrollx[2] + std::round(200 * scr);

      layers[0].scrolly = initial_scrolly[0] - std::round(25 * scr);
      layers[1].scrolly = initial_scrolly[1] + std::round(20 * scr);
      layers[2].scrolly = initial_scrolly[2] + std::round(45 * scr);

      render_layers();
      scheduler.next_frame();
      kreader.process();
      if (is_menu) return;
    }
    std::uint32_t const wait = 8 * rows + config::default_decay;
    for (std::uint32_t loop = 0; loop < wait; loop++) {
      render_layers();
      scheduler.next_frame();
      kreader.process();
      if (is_menu) return;
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
          cell.c = U'0' + util::rand() % 10;
          cell.birth = now - std::round((0.5 + 0.1 * util::randf()) * config::default_decay);
          cell.power = 1.0;
          cell.decay = config::default_decay;
          cell.flags = cflag_disable_bold;
          tcell.c = cell.c;
          tcell.fg = color_table[std::size(color_table) / 2 + util::rand_char() % 3];
        }
      }
    }
  }

public:
  void s1number() {
    clear_content();
    int stripe_periods[] = {0, 32, 16, 8, 4, 2, 2, 2};
    for (int stripe: stripe_periods) {
      for (int i = 0; i < 20; i++) {
        s1number_fill_numbers(stripe);
        render_direct();
        scheduler.next_frame();
        kreader.process();
        if (is_menu) return;
      }
    }
  }

private:
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

  static constexpr int s2banner_initial_input = 40;
  static constexpr int s2banner_cell_width = 10;
  static constexpr int s2banner_cell_height = 7;
  static constexpr std::size_t s2banner_max_message_size = 0x1000;

  struct banner_message_t {
    std::vector<char32_t> text;
    std::vector<glyph_t> glyphs;
    int min_width = 0; // 表示に必要な最低幅

    int render_width = 0; // 全体の表示幅
    int min_progress = 0; // 最小の文字表示幅

  private:
    static glyph_definition_t const* glyph_data(char32_t c) {
      static glyph_definition_t glyph_defs[] = {
#include "glyph.inl"
      };
      static std::unordered_map<char32_t, glyph_definition_t const*> map;
      if (map.empty()) {
        for (auto const& def : glyph_defs)
          map[def.c] = &def;
      }
      auto it = map.find(c);
      if (it == map.end() && c != ' ')
        it = map.find(U'\uFFFD');
      return it != map.end() ? it->second : nullptr;
    }
  public:
    void set_text(const char* msg) {
      text.clear();
      s2banner_decode(text, msg);
    }
    void resolve_glyph() {
      glyphs.clear();
      this->min_width = 0;
      for (char32_t c: text) {
        if (U'a' <= c && c <= U'z')
          c = c - U'a' + U'A';

        glyph_t g;
        g.def = glyph_data(c);
        g.h = 7;
        g.w = g.def ? g.def->w : 5;
        g.render_width = g.w + 1;

        if (glyphs.size()) this->min_width++;
        this->min_width += g.w;
        glyphs.push_back(g);
      }
    }

    void adjust_width(int cols) {
      // Adjust rendering width
      int rest = cols - this->min_width - 4;
      this->render_width = this->min_width;
      for (glyph_t& g: glyphs)
        g.render_width = g.w + 1;
      while (rest > 0) {
        int min_progress = glyphs[0].render_width;
        int min_progress_count = 0;
        for (glyph_t const& g: glyphs) {
          if (g.render_width < min_progress) {
            min_progress = g.render_width;
            min_progress_count = 1;
          } else if (g.render_width == min_progress) {
            min_progress_count++;
          }
        }

        if (min_progress >= s2banner_cell_width * 3 / 2) break;
        rest -= min_progress_count;
        if (rest < 0) break;

        for (glyph_t& g: glyphs)
          if (g.render_width == min_progress) g.render_width++;
        this->render_width += min_progress_count;
        this->min_progress = min_progress;
      }
    }
  };

  class banner_t {
    std::vector<banner_message_t> data;

  public:
    void add_message(std::string const& msg) {
      banner_message_t message;
      message.set_text(msg.c_str());
      message.resolve_glyph();
      data.emplace_back(std::move(message));
    }
    std::vector<banner_message_t>::iterator begin() { return data.begin(); }
    std::vector<banner_message_t>::iterator end() { return data.end(); }

  public:
    int max_min_width() const {
      int width = 0;
      for (auto const& message: data)
        width = std::max(width, message.min_width);
      return width;
    }
    int max_number_of_characters() const {
      std::size_t nchar = 0;
      for (auto const& message: data)
        nchar = std::max(nchar, message.text.size());
      return (int) nchar;
    }
  };

  banner_t banner;

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

  void s2banner_write_caret(banner_message_t const& message, int x0, int y0, bool set, int type) {
    x0 += std::max(0, (message.min_progress - 1 - s2banner_cell_width) / 2);
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
      cell_t& cell = layers[0].rcell(x0 + x, y0 + y);
      cell.c = ' ';
    } else if (type == 1) {
      cell_t& cell = layers[0].rcell(x0 + x, y0 + y);
      cell.c = uchar;
      cell.birth = now;
      cell.power = 1.0;
      cell.decay = 20;
      cell.flags = 0;
    } else if (type == 2) {
      s2banner_put_char(x0, y0, x, y, uchar, 1);

      thread_t thread;
      thread.x = x0 + x;
      thread.y = y0 + y;
      thread.age = 0;
      thread.speed = s2banner_cell_height - y;
      if (thread.speed > 2) thread.speed += util::rand() % 3 - 1;
      thread.power = 2.0 / 3.0;
      thread.decay = 30;
      layers[1].add_thread(thread);
    }
  }
  void s2banner_set_char(int x0, int y0, int x, int y, int type) {
    if (type == 0)
      s2banner_put_char(x0, y0, x, y, type, ' ');
    else
      s2banner_put_char(x0, y0, x, y, type, util::rand_char());
  }

  void s2banner_add_thread(int ilayer, int interval) {
    if (now % (1 + interval / cols) == 0) {
      thread_t thread;
      thread.x = util::rand() % cols;
      thread.y = 0;
      thread.age = 0;
      thread.speed = 8;
      thread.power = 0.5;
      thread.decay = config::default_decay;
      layers[ilayer].add_thread(thread);
    }
  }

  void s2banner_show_message(banner_message_t& message, int mode) {
    int nchar, display_width, display_height;
    switch (mode) {
    default:
    case 0:
      message.adjust_width(cols);
      nchar = message.glyphs.size();
      display_width = message.render_width + message.min_progress;
      display_height = message.glyphs[0].h;
      break;
    case 1:
    case 2:
      nchar = (int) message.text.size();
      display_width = mode * nchar;
      display_height = 1;
      break;
    }

    // 最後に文字入力が起こった位置と時刻
    int input_index = -1;
    int input_time = 0;

    int loop_max = s2banner_initial_input + nchar * 5 + 130;
    for (int loop = 0; loop <= loop_max; loop++) {
      int type = 1;
      if (loop == loop_max) type = 2;

      int x0 = (cols - display_width) / 2, y0 = (rows - display_height) / 2;
      if (mode != 0 && util::rand() % 20 == 0)
        y0 += util::rand() % 7 - 3;
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
            glyph_t const& g = message.glyphs[i];
            if (caret_moved)
              s2banner_write_caret(message, x0, y0, false, type);
            s2banner_write_letter(x0, y0, g, type);
            x0 += g.render_width;
          }
          break;
        default:
          {
            char32_t c = message.text[i];
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
        //   s2banner_write_caret(message, x0, y0, true);
        s2banner_write_caret(message, x0, y0, !((loop - input_time) / 25 & 1), type);
        //s2banner_write_caret(message, x0, y0, true);
        break;
      default:
        s2banner_put_char(x0, y0, 0, 0, type, U'\u2589');
        break;
      }

      s2banner_add_thread(1, 2000);
      render_layers();
      scheduler.next_frame();
      kreader.process();
      if (is_menu) return;
    }
  }
public:
public:
  void s2banner_add_message(std::string const& message) {
    banner.add_message(message);
  }
  void s2banner() {
    // mode = 0: glyph を使って表示
    // mode = 1: 単純に文字を並べる
    // mode = 2: 1文字ずつ空白を空けて文字を並べる
    int mode = 1;
    if (banner.max_min_width() < cols) {
      mode = 0;
    } else if (banner.max_number_of_characters() * 2 < cols) {
      mode = 2;
    }

    for (banner_message_t& message: banner)
      s2banner_show_message(message, mode);
  }

private:
  conway_t s4conway_board;
  void s4conway_frame(double theta, double scal, double power) {
    s4conway_board.set_size(cols, rows);
    s4conway_board.set_transform(scal, theta);
    for (int y = 0; y < rows; y++) {
      for (int x = 0; x < cols; x++) {
        cell_t& cell = layers[2].rcell(x, y);
        switch (s4conway_board.get_pixel(x, y, power)) {
        case 1:
          cell.c = util::rand_char();
          cell.birth = now;
          cell.power = power;
          cell.decay = 100;
          cell.flags = cflag_disable_bold;
          break;
        case 2:
          cell.c = util::rand_char();
          cell.birth = now;
          cell.power = power * 0.2;
          cell.decay = 100;
          cell.flags = cflag_disable_bold;
          break;
        default:
          cell.c = ' ';
          break;
        }
      }
    }
  }
public:
  void s4conway() {
    s4conway_board.initialize();
    double time = 0.0;
    double distance = 0.48;

    std::uint32_t loop;
    for (loop = 0; loop < 2000; loop++) {
      distance += 1.0 * (loop > 1500 ? distance * 0.01 : 0.04);
      time += 0.005 * distance;
      s4conway_board.step(time);
      s4conway_frame(0.5 + loop * 0.01, 0.01 * distance, std::min(0.8, 3.0 / std::sqrt(distance)));
      render_layers();
      scheduler.next_frame();
      kreader.process();
      if (is_menu) return;
    }
  }

private:
  mandelbrot_t s5mandel_data;
  void s5mandel_frame(double theta, double scale, double power_scale) {
    s5mandel_data.resize(cols, rows);
    s5mandel_data.update_frame(theta, scale);
    for (int y = 0; y < rows; y++) {
      for (int x = 0; x < cols; x++) {
        cell_t& cell = layers[1].rcell(x, y);
        double const power = s5mandel_data(x, y);
        if (power < 0.05) {
          cell.c = ' ';
        } else {
          cell.c = util::rand_char();
          cell.birth = now;
          cell.power = power * power_scale;
          cell.decay = 100;
          cell.flags = cflag_disable_bold;
        }
      }
    }
  }

public:
  void s5mandel() {
    twinkle = 0.1;

    double const scale0 = 1e-17, scaleN = 30.0 / std::min(cols, rows);
    std::uint32_t const nloop = 3000;
    double const mag1 = std::pow(scaleN / scale0, 1.0 / nloop);

    double scale = scale0;
    double theta = 0.5;
    std::uint32_t loop;
    for (loop = 0; loop < nloop; loop++) {
      scale *= mag1;
      theta -= 0.01;
      s5mandel_frame(theta, scale, std::min(0.01 * loop, 1.0));
      render_layers();
      scheduler.next_frame();
      kreader.process();
      if (is_menu) return;
    }
    for (loop = 0; loop < 100; loop++) {
      render_layers();
      scheduler.next_frame();
      kreader.process();
      if (is_menu) return;
    }

    twinkle = default_twinkle;
  }

private:
  static constexpr int menu_index_min = scene_number;
  static constexpr int menu_index_max = scene_rain_forever;
  int menu_index = menu_index_min;

  void menu_initialize() {
    is_menu = true;
    menu_index = menu_index_min;
  }

  void menu_process_key(key_t k) {
    switch (k) {
    case key_ctrl('p'):
    case 'k':
    case key_up:
      if (menu_index > menu_index_min)
        menu_index--;
      break;
    case key_ctrl('n'):
    case 'j':
    case key_down:
      if (menu_index < menu_index_max)
        menu_index++;
      break;
    case key_ctrl('m'):
    case key_ctrl('j'):
      is_menu = false;
      break;
    }
  }
  void menu_frame_draw_string(int y0, scene_t scene, const char* name) {
    std::size_t const len = std::strlen(name);
    int const progress = 2;
    int const x0 = (cols - len * progress) / 2;
    double const power = scene == menu_index ? 1.0 : 0.5;
    double const flags = scene == menu_index ? 0 : cflag_disable_bold;
    for (std::size_t i = 0; i < len; i++) {
      cell_t& cell = layers[0].rcell(x0 + i * progress, y0);
      cell.c = std::toupper(name[i]);
      cell.birth = now;
      cell.power = power;
      cell.decay = 20;
      cell.flags = flags;
    }
  }
public:
  int show_menu() {
    while (is_menu) {
      int const line_height = std::min(3, rows / scene_count);
      int const y0 = (rows - scene_count * line_height) / 2;
      int i = 0;
      menu_frame_draw_string(y0 + i++ * line_height, scene_number      , "Number falls");
      menu_frame_draw_string(y0 + i++ * line_height, scene_banner      , "Banner");
      menu_frame_draw_string(y0 + i++ * line_height, scene_rain        , "Matrix rain");
      menu_frame_draw_string(y0 + i++ * line_height, scene_conway      , "Conway's Game of Life");
      menu_frame_draw_string(y0 + i++ * line_height, scene_mandelbrot  , "Mandelbrot set");
      menu_frame_draw_string(y0 + i++ * line_height, scene_rain_forever, "Rain forever");

      s2banner_add_thread(1, 5000);
      render_layers();
      scheduler.next_frame();
      kreader.process();
      if (!is_menu) break;
    }

    return menu_index;
  }

public:
  void scene(scene_t s) {
    switch (s) {
    case scene_none:
      break;
    case scene_number:
      this->s1number();
      break;
    case scene_banner:
      this->s2banner();
      break;
    case scene_rain:
      this->s3rain(2800, buffer::s3rain_scroll_func_tanh);
      break;
    case scene_conway:
      this->s4conway();
      break;
    case scene_mandelbrot:
      this->s5mandel();
      break;
    case scene_rain_forever:
      this->s3rain(0, buffer::s3rain_scroll_func_const);
      break;
    case scene_loop:
      break;
    }
  }
};

buffer buff;

void trapint(int sig) {
  buff.finalize();
  std::signal(sig, SIG_DFL);
  std::raise(sig);
  std::exit(128 + sig);
}
void trapwinch(int) {
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

struct arguments {
  bool flag_error = false;
  bool flag_help = false;

public:
  void print_help(std::FILE* file) {
    std::fprintf(file,
      "cxxmatrix (C++ Matrix)\n"
      "usage: cxxmatrix [OPTIONS...] [[--] MESSAGE...]\n"
      "\n"
      "MESSAGE\n"
      "   Add a message for 'banner' scene.  When no messages are specified a message\n"
      "   \"C++ MATRIX\" will be used.\n"
      "\n"
      //------------------------------------------------------------------------------
      "OPTIONS\n"
      "   --help      Show help\n"
      "   --          The rest arguments are processed as MESSAGE\n"
      "   -m, --message=MESSAGE\n"
      "               Add a message for 'banner' scene.\n"
      "   -s, --scene=SCENE\n"
      "               Add scenes. Comma separated list of 'number', 'banner', 'rain',\n"
      "               'conway', 'mandelbrot', 'rain-forever' and 'loop'.\n"
      "\n"
      "Keyboard\n"
      "   C-c (SIGINT)  Quit\n"
      "   C-z (SIGTSTP) Suspend\n"
      "   C-m, RET      Show menu\n"
      "\n"
    );
  }
private:
  int argc;
  char** argv;
  int iarg;
  char const* arg;
  char const* get_optarg(char c) {
    if (*arg) {
      char const* ret = arg;
      arg = "";
      return ret;
    } else if (iarg < argc) {
      return argv[iarg++];
    } else {
      std::fprintf(stderr, "cxxmatrix: missing option argument for '-%c'.", c);
      flag_error = true;
      return nullptr;
    }
  }

  char const* longopt_arg;
  bool is_longopt(const char* name) {
    const char* p = arg;
    while (*name && *name == *p) p++, name++;
    if (*name) return false;
    if (*p == '=') {
      longopt_arg = p + 1;
      return true;
    } else if (*p == '\0') {
      longopt_arg = nullptr;
      return true;
    } else {
      return false;
    }
  }
  char const* get_longoptarg() {
    if (longopt_arg) {
      return longopt_arg;
    } else if (iarg < argc) {
      return argv[iarg++];
    } else {
      std::fprintf(stderr, "cxxmatrix: missing option argument for \"--%s\"\n", arg);
      flag_error = true;
      return nullptr;
    }
  }

public:
  std::vector<std::string> messages;
private:
  void push_message(const char* message) {
    messages.push_back(message);
  }

public:
  std::vector<scene_t> scenes;
private:
  void push_scene(const char* scene) {
    std::vector<std::string_view> names = util::split(scene, ',');
    for (auto const& name: names) {
      if (name == "number") {
        scenes.push_back(scene_number);
      } else if (name == "banner") {
        scenes.push_back(scene_banner);
      } else if (name == "conway") {
        scenes.push_back(scene_conway);
      } else if (name == "rain") {
        scenes.push_back(scene_rain);
      } else if (name == "mandelbrot") {
        scenes.push_back(scene_mandelbrot);
      } else if (name == "loop") {
        if (scenes.empty()) {
          std::fprintf(stderr, "cxxmatrix: nothing to loop (-s loop)\n");
          flag_error = true;
          return;
        }
        scenes.push_back(scene_loop);
      } else if (name == "rain-forever") {
        scenes.push_back(scene_rain_forever);
      } else {
        std::fprintf(stderr, "cxxxmatrix: unknown value for scene (%.*s)\n", (int) name.size(), name.data());
        flag_error = true;
      }
    }
  }

public:
  bool process(int argc, char** argv) {
    bool flag_literal = false;
    this->argc = argc;
    this->argv = argv;
    this->iarg = 1;
    while (iarg < argc) {
      arg = argv[iarg++];
      if (!flag_literal && arg[0] == '-') {
        if (arg[1] == '-') {
          arg += 2;
          if (!*arg) {
            flag_literal = true;
          } else if (is_longopt("help")) {
            flag_help = true;
          } else if (is_longopt("message")) {
            push_message(get_longoptarg());
          } else if (is_longopt("scene")) {
            push_scene(get_longoptarg());
          } else {
            std::fprintf(stderr, "cxxmatrix: unknown long option (--%s)\n", arg);
            flag_error = true;
          }
        } else {
          arg++;
          while (char const c = *arg++) {
            switch (c) {
            case 'm':
              if (char const* opt = get_optarg(c))
                push_message(opt);
              break;
            case 's':
              if (char const* opt = get_optarg(c))
                push_scene(opt);
              break;
            default:
              std::fprintf(stderr, "cxxmatrix: unknown option (-%c)\n", c);
              flag_error = true;
              break;
            }
          }
        }
        continue;
      }
      push_message(arg);
    }
    return !flag_error;
  }
  arguments(int argc, char** argv) {
    this->process(argc, argv);
  }
};

int main(int argc, char** argv) {
  arguments args(argc, argv);
  if (args.flag_error) return 2;
  if (args.flag_help) {
    args.print_help(stdout);
    return 0;
  }

  if (args.scenes.empty()) {
    args.scenes.push_back(scene_number);
    args.scenes.push_back(scene_banner);
    args.scenes.push_back(scene_rain);
    args.scenes.push_back(scene_conway);
    args.scenes.push_back(scene_mandelbrot);
    args.scenes.push_back(scene_rain_forever);
  }
  if (args.messages.size()) {
    for (std::string const& msg: args.messages)
      buff.s2banner_add_message(msg);
  } else {
    buff.s2banner_add_message("C++ Matrix");
  }

  std::signal(SIGINT, trapint);
  std::signal(SIGWINCH, trapwinch);
  std::signal(SIGTSTP, traptstp);
  std::signal(SIGCONT, trapcont);

  buff.initialize();
  buff.term_enter();
  std::size_t index = 0;
  while (index < args.scenes.size()) {
    scene_t const scene = args.scenes[index++];
    switch (scene) {
    case scene_none: break;
    case scene_loop: index = 0; break;
    default:
      buff.scene(scene);
      break;
    }

    if (buff.is_menu) break;
  }

  if (buff.is_menu) {
    for (;;) {
      buff.is_menu = true;
      scene_t const scene = (scene_t) buff.show_menu();
      buff.scene(scene);
    }
  }

  buff.finalize();
  return 0;
}

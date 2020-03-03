#include <cstdio>
#include <cstdint>
#include <cstddef>
#include <csignal>
#include <cstdlib>

#include <unistd.h>
#include <sys/ioctl.h>
#include <time.h>

#include <vector>

typedef uint8_t byte;

struct cell_t {
  char32_t c = ' ';
  byte fg = 16;
  byte bg = 16;
  bool bold = false;

  int time = 0;
};

struct buffer {
  int cols, rows;
  std::vector<cell_t> old_content;
  std::vector<cell_t> new_content;
  std::FILE* file;

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
    fg = 0;
    bg = 0;
  }
  void set_color(cell_t const& cell) {
    if (cell.bg != this->bg) {
      this->bg = cell.bg;
      std::fprintf(file, "\x1b[48;5;%dm", this->bg);
    }
    if (cell.c != ' ') {
      if (cell.fg != fg) {
        this->fg = cell.fg;
        std::fprintf(file, "\x1b[38;5;%dm", this->fg);
      }
      if (cell.bold != bold) {
        this->bold = cell.bold;
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
        } else if (x == px - 1) {
          std::putc('\b', file);
        } else if (x == px - 2) {
          std::putc('\b', file);
          std::putc('\b', file);
        } else if (x == px - 3) {
          std::putc('\b', file);
          std::putc('\b', file);
          std::putc('\b', file);
        } else {
          std::fprintf(file, "\x1b[%dG", x + 1);
        }
        px = x;
      }
      return;
    }

    if (x <= px && py < y && (x == 0 ? 1 : px - x) + (y - py) <= (x == 0 || x == px ? 3 : 5)) {
      if (x != px) {
        if (x == 0) {
          std::putc('\r', file);
          px = 0;
        } else {
          while (x < px--)
            std::putc('\b', file);
        }
      }
      while (py++ < y)
        std::putc('\n', file);
      return;
    }

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
        cell_t const& cell = new_content[y * cols + x];
        set_color(cell);
        put_utf8(cell.c);
      }
    }
    std::fflush(file);

    old_content.resize(new_content.size());
    for (std::size_t i = 0; i < new_content.size(); i++)
      old_content[i] = new_content[i];
  }

  void update() {
    for (int y = 0; y < rows; y++) {
      for (int x = 0; x < cols; x++) {
        std::size_t index = y * cols + x;
        cell_t const& ncell = new_content[index];
        cell_t const& ocell = old_content[index];
        if (ncell.bg != ocell.bg || ncell.c != ocell.c || ncell.c != ' ' && ncell.fg != ocell.fg) {
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

  void finalize() {
    std::fprintf(file, "\x1b[?1049l\x1b[?25h");
    std::fflush(file);
  }
};

buffer buff;

void trap_sigint(int sig) {
  buff.finalize();
  std::signal(sig, SIG_DFL);
  std::raise(sig);
}

int main() {
  struct winsize ws;
  ioctl(STDIN_FILENO, TIOCGWINSZ, (char*) &ws);
  buff.cols = ws.ws_col;
  buff.rows = ws.ws_row;
  buff.file = stdout;

  std::fprintf(buff.file, "\x1b[?1049h\x1b[?25l");
  std::signal(SIGINT, trap_sigint);

  buff.new_content.resize(buff.cols * buff.rows);
  buff.redraw();

  // 16, 22, 28, 34, 40, 46, 83, 120, 157, 194, 231
  for (;;) {
    int const x = std::rand() % buff.cols;
    int const y = std::rand() % buff.rows;
    int const c = L'ｱ' + std::rand() % 50;

    auto& cell = buff.new_content[y * buff.cols + x];
    cell.c = c;
    cell.fg = 46;
    cell.bg = 22;
    cell.bold = true;

    buff.update();

    struct timespec tv;
    tv.tv_sec = 0;
    tv.tv_nsec = 10 * 1000000;
    nanosleep(&tv, NULL);
  }

  buff.finalize();
  return 0;
}

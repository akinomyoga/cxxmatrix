# -*- mode: makefile-gmake -*-

all:
.PHONY: all clean install

#------------------------------------------------------------------------------
# Settings

# Set UTF-8 locale
ifeq ($(shell locale | grep -Ei 'LC_CTYPE.*utf-?8'),"")
  utf8_locale := $(shell locale -a | grep -Ei 'utf-?8' | head -1)
  ifneq ($(utf8_locale),"")
    export LC_ALL=$(utf8_locale)
  else
    $(warning It seems your system does not support UTF-8)
  endif
endif

# Auto detect gawk
AWK := $(shell which gawk 2>/dev/null || echo awk)
ifeq ($(AWK),"awk")
  $(warning gawk (GNU awk) is not available, which may cause problems. Consider installing GNU awk.)
endif

# C++ compile options
CPPFLAGS = -MD -MP -MF $(@:.o=.dep)
CXXFLAGS := -std=c++17 -Wall -Wextra -Ofast

#------------------------------------------------------------------------------
# cxx matrix

all: cxxmatrix

cxxmatrix-OBJS := cxxmatrix.o
ifeq ($(TARGET),win32)
  cxxmatrix-OBJS += term_win32.o
  CXXFLAGS += -static -static-libgcc -static-libstdc++
else
  cxxmatrix-OBJS += term_unix.o
endif

-include $(wildcard *.dep)
cxxmatrix: $(cxxmatrix-OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

cxxmatrix.o: cxxmatrix.cpp glyph.inl
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c -o $@ $<

glyph.inl: glyph.awk glyph.def
	$(AWK) -f glyph.awk glyph.def > glyph.inl.part
	mv glyph.inl.part $@

clean:
	-rm -rf *.o glyph.inl

ifeq ("$(PREFIX)","")
  PREFIX := /usr/local
endif
install: cxxmatrix
	mkdir -p "$(PREFIX)/bin"
	cp cxxmatrix "$(PREFIX)/bin/cxxmatrix"
	chmod +x "$(PREFIX)/bin/cxxmatrix"
	mkdir -p "$(PREFIX)/share/man/man1"
	gzip -c cxxmatrix.1 > "$(PREFIX)/share/man/man1/cxxmatrix.1.gz"
	mkdir -p "$(PREFIX)/share/licenses/cxxmatrix"
	cp LICENSE.md "$(PREFIX)/share/licenses/cxxmatrix/LICENSE.md"

#------------------------------------------------------------------------------

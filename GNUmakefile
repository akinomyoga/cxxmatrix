# -*- mode: makefile-gmake -*-

.PHONY: all clean
all:

#------------------------------------------------------------------------------
# Settings

# Set UTF-8 locale
utf8_locale := $(shell locale -a | grep -Ei 'utf-?8' | head -1)
ifneq ($(utf8_locale),"")
  export LC_ALL=$(utf8_locale)
else
  $(warning It seems your system does not support UTF-8)
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

-include $(wildcard *.dep)
cxxmatrix: cxxmatrix.o
	$(CXX) $(CXXFLAGS) -o $@ $^

cxxmatrix.o: cxxmatrix.cpp glyph.inl
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c -o $@ $<

glyph.inl: glyph.awk glyph.def
	$(AWK) -f glyph.awk glyph.def > glyph.inl.part
	mv glyph.inl.part $@

clean:
	-rm -rf *.o glyph.inl

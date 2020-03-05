# -*- mode: makefile-gmake -*-

.PHONY: all clean
all:

CXXFLAGS := -std=c++17 -Wall -Wextra -Os

all: xmatrix.exe

xmatrix.exe: xmatrix.o
	$(CXX) $(CXXFLAGS) -o $@ $^

xmatrix.o: xmatrix.cpp glyph.inl
	$(CXX) $(CXXFLAGS) -c -o $@ $<

glyph.inl: glyph.awk glyph.def
	awk -f glyph.awk glyph.def > glyph.inl

clean:
	-rm -rf *.o glyph.inl

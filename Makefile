# -*- mode: makefile-gmake -*-

.PHONY: all clean
all:

CPPFLAGS = -MD -MP -MF $(@:.o=.dep)
CXXFLAGS := -std=c++17 -Wall -Wextra -Os

all: cxxmatrix

-include $(wildcard *.dep)
cxxmatrix: cxxmatrix.o
	$(CXX) $(CXXFLAGS) -o $@ $^

cxxmatrix.o: cxxmatrix.cpp glyph.inl
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c -o $@ $<

glyph.inl: glyph.awk glyph.def
	awk -f glyph.awk glyph.def > glyph.inl

clean:
	-rm -rf *.o glyph.inl

# -*- mode: makefile-gmake -*-

.PHONY: all
all: xmatrix.exe

glyph.inl: glyph.awk glyph.def
	awk -f glyph.awk glyph.def > glyph.inl

xmatrix.exe: xmatrix.cpp glyph.inl
	$(CXX) -std=c++17 -Os -s -o $@ $<

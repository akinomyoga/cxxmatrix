# -*- mode: makefile-gmake -*-

.PHONY: all
all: xmatrix.exe

xmatrix.exe: xmatrix.cpp
	$(CXX) -std=c++17 -o $@ $<

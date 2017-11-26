CXXFLAGS=-std=c++11 -Wall -Wextra -pedantic

.PHONY: debug release clean

debug: CXXFLAGS+=-g -ggdb
debug: test

release: CXXFLAGS+=-O2 -fvisibility-inlines-hidden
release: test

test: test.cpp lfpool.hpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -o $@ $< -lpthread

clean:
	rm -f test

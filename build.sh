#!/bin/sh
g++ -Wall -Wextra -g -O2 -std=c++0x -fPIC -DPIC main.cpp -o main -lstdc++ -L/usr/local/lib -lcassandra



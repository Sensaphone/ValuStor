#!/bin/sh
g++ -Wall -Wextra -g -O2 -std=c++11 -fPIC -DPIC main.cpp -o main -lstdc++ -L/usr/local/lib -lcassandra -lpthread -I..



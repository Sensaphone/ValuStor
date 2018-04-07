#!/bin/bash
set -x
g++ -Wall -Wextra -O2 -std=c++11 -fPIC -DPIC test_gen.cpp -o test_gen -lstdc++ -L/usr/local/lib -lcassandra -lpthread -I.. &&
./test_gen > test.cpp &&
g++ -Wall -Wextra -O2 -std=c++11 -fPIC -DPIC test.cpp -o tester -lstdc++ -L/usr/local/lib -lcassandra -lpthread -I.. &&
./tester &&
rm ./test_gen test.cpp ./tester

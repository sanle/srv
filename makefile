all: main.cpp
	clang++ -std=c++11 -pthread -o final main.cpp

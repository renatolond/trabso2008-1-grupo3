all: trab.banco trab.terminal

trab.banco: trab.banco.cpp socketsoper.cpp
	g++ -o trab.banco trab.banco.cpp -Wall -lpthread -Lsocket -g

trab.terminal: trab.terminal.cpp socketsoper.cpp
	g++ -o trab.terminal trab.terminal.cpp -Wall -lpthread -Lsocket -g

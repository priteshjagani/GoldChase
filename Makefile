test_prg: test_prg.cpp libmap.a goldchase.h
	g++ test_prg.cpp -o test_prg -std=c++11 -L. -lmap -lpanel -lncurses -lpthread -lrt

libmap.a: Screen.o Map.o
	ar -r libmap.a Screen.o Map.o

Screen.o: Screen.cpp
	g++ -c Screen.cpp

Map.o: Map.cpp
	g++ -c Map.cpp

clean:
	rm -f Screen.o Map.o libmap.a test_prg

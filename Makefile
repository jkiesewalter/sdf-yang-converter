converter: converter.o sdf.o
	g++ -o converter converter.o sdf.o -lyang

converter.o: converter.cpp
	g++ -c converter.cpp

sdf.o: sdf.cpp
	g++ -c sdf.cpp

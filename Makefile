converter: converter.o sdf.o
	g++ -o converter converter.o sdf.o -lyang

converter.o: converter.cpp
	g++ -c converter.cpp

sdf.o: sdf.cpp
	g++ -c sdf.cpp
	
#libyang.o: libyang/src/libyang.h
#	g++ -c libyang/src/libyang.h
	
clean:
	rm *.o converter

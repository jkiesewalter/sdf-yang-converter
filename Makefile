converter: converter.o sdf.o
	g++ -o converter converter.o sdf.o -lyang -lnlohmann_json_schema_validator

converter.o: converter.cpp
	g++ -c converter.cpp -g3

sdf.o: sdf.cpp
	g++ -c sdf.cpp -g3
	
clean:
	rm *.o converter

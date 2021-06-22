CXXFLAGS=-g3
LDLIBS=-lyang -lnlohmann_json_schema_validator
LINK.o=$(LINK.cc)

converter:: converter.o sdf.o

clean:
	rm *.o converter

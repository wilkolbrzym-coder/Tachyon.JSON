CXX = g++
CXXFLAGS = -std=c++23 -O3 -march=native -flto -I. -Iglaze/include
LDFLAGS =

TARGET = bench_scientific

all: deps $(TARGET)

$(TARGET): bench_scientific.cpp simdjson.o
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

simdjson.o: simdjson.cpp simdjson.h
	$(CXX) $(CXXFLAGS) -c simdjson.cpp

deps:
	@echo "Checking dependencies..."
	@if [ ! -f nlohmann_json.hpp ]; then \
		echo "Downloading nlohmann/json..."; \
		curl -s -L -o nlohmann_json.hpp https://raw.githubusercontent.com/nlohmann/json/develop/single_include/nlohmann/json.hpp; \
	fi
	@if [ ! -f simdjson.h ]; then \
		echo "Downloading simdjson header..."; \
		curl -s -L -o simdjson.h https://raw.githubusercontent.com/simdjson/simdjson/master/singleheader/simdjson.h; \
	fi
	@if [ ! -f simdjson.cpp ]; then \
		echo "Downloading simdjson source..."; \
		curl -s -L -o simdjson.cpp https://raw.githubusercontent.com/simdjson/simdjson/master/singleheader/simdjson.cpp; \
	fi
	@if [ ! -f canada.json ]; then \
		echo "Downloading canada.json..."; \
		curl -s -L -o canada.json https://raw.githubusercontent.com/miloyip/nativejson-benchmark/master/data/canada.json; \
	fi
	@if [ ! -d glaze ]; then \
		echo "Cloning glaze..."; \
		git clone --depth 1 https://github.com/stephenberry/glaze.git; \
	fi

clean:
	rm -f $(TARGET) *.o

distclean: clean
	rm -f nlohmann_json.hpp simdjson.h simdjson.cpp canada.json
	rm -rf glaze

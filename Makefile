SRC=$(wildcard *.cpp)
HDR=$(wildcard *.hpp) stb_image_write.h stb_image.h print.hpp defer.hpp
OBJ=$(SRC:.cpp=.o)

# SAN = -g -lg -Og -fsanitize=address
OMP =  -fopenmp

CFLAGS= -std=c++20 -O3 $(OMP) $(SAN)
LDFLAGS=  $(OMP) $(SAN)

all: blur

blur: $(OBJ)
	$(CXX) $(LDFLAGS) $^ -o $@

$(OBJ): Makefile $(HDR)

%.o: %.cpp
	$(CXX) -c $(CFLAGS) $<

stb_image_write.h:
	curl -LO https://raw.githubusercontent.com/nothings/stb/master/stb_image_write.h

stb_image.h:
	curl -LO https://raw.githubusercontent.com/nothings/stb/master/stb_image.h

print.hpp:
	curl -LO https://gist.githubusercontent.com/dk949/0b1d14bec6eebdc9c3270c73d4d2ad72/raw/4c7a470c1f49f966e486fc7a17e0a49ac8cfa238/print.hpp

defer.hpp:
	curl -LO https://gist.githubusercontent.com/dk949/f81f5ba76459b97169abffa87dfd88e1/raw/bc5aec4996355158476d211cbabb42c8191e48dd/defer.hpp


.PHONY: clean
clean:
	rm -f *.o
	rm -f blur

.PHONY: clean-deep
clean-deep: clean
	rm -f *.jpg
	rm -f stb_image.h
	rm -f stb_image_write.h
	rm -f print.hpp
	rm -f defer.hpp

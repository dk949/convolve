SRC=$(wildcard *.cpp)
HDR=$(wildcard *.hpp) stb_image_write.h stb_image.h print.hpp defer.hpp
OBJ=$(SRC:.cpp=.o)

# SAN = -g -lg -Og -fsanitize=address
OMP=  -fopenmp
# TIMING= -DTIMING

CFLAGS= -std=c++20 -O3 $(OMP) $(SAN) $(TIMING)
LDFLAGS=  $(OMP) $(SAN)

CURL= curl -sLO

all: convolve

convolve: $(OBJ)
	$(CXX) $(LDFLAGS) $^ -o $@

$(OBJ): Makefile $(HDR)

%.o: %.cpp
	$(CXX) -c $(CFLAGS) $<

stb_image_write.h:
	$(CURL) https://raw.githubusercontent.com/nothings/stb/master/stb_image_write.h

stb_image.h:
	$(CURL) https://raw.githubusercontent.com/nothings/stb/master/stb_image.h

print.hpp:
	$(CURL) https://gist.githubusercontent.com/dk949/0b1d14bec6eebdc9c3270c73d4d2ad72/raw/4c7a470c1f49f966e486fc7a17e0a49ac8cfa238/print.hpp

defer.hpp:
	$(CURL) https://gist.githubusercontent.com/dk949/f81f5ba76459b97169abffa87dfd88e1/raw/bc5aec4996355158476d211cbabb42c8191e48dd/defer.hpp


.PHONY: clean
clean:
	rm -f *.o
	rm -f convolve

.PHONY: clean-deep
clean-deep: clean
	@echo "WARNING: In 5 seconds this command will remove all images in this directory"
	@sleep 5
	rm -f *.jpg
	rm -f *.png
	rm -f stb_image.h
	rm -f stb_image_write.h
	rm -f print.hpp
	rm -f defer.hpp

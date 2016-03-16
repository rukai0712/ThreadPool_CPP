SRC_CPP = $(wildcard *.cpp)
TARGET_CPP = $(patsubst %.cpp, %, $(SRC_CPP))

CPP = clang++ -std=c++11
CPP_FLAGS = -Wall -g -I ./

ALL:$(TARGET_CPP)

$(TARGET_CPP):%:%.cpp
	$(CPP) -lpthread $< -o $@ $(CPP_FLAGS)

clean:
	-rm -rf *.o *.dSYM

clean_all:
	-rm -rf $(TRAGET_CPP) *.o *.dSYM

.PHONY: ALL clean_all



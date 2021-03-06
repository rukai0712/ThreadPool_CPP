SRC_CPP = $(wildcard *.cpp)
TARGET_CPP = $(patsubst %.cpp, %.out, $(SRC_CPP))

CPP = clang++ -std=c++11
CPP2 = g++ -std=c++11
CPP_FLAGS = -Wall -g -I ./

ALL:$(TARGET_CPP)

$(TARGET_CPP):%.out:%.cpp
	$(CPP2) -lpthread $< -o $@ $(CPP_FLAGS)

clean:
	-rm -rf *.o *.dSYM

clean_all:
	-rm -rf $(TARGET_CPP) *.o *.dSYM

.PHONY: ALL clean_all



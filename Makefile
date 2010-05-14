CXXFLAGS= -Wall -Wextra -Werror -g -O2

%.o:%.cc
	$(CXX) -c $^ -o $@ $(CXXFLAGS) -Wa,-ahl=$@.s

numbers: numbers.o stat.o virtual.o
	$(CXX) $^ -o $@ $(CXXFLAGS) -lpthread

clean:
	-rm *.o *.o.s numbers

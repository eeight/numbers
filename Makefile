CXXFLAGS= -Wall -Wextra -g -O2

%.o:%.cc
	$(CXX) -c $^ -o $@ $(CXXFLAGS) -Wa,-ahl=$@.s

numbers: numbers.o stat.o virtual.o
	$(CXX) $^ -o $@ $(CXXFLAGS)

clean:
	-rm *.o *.o.s numbers

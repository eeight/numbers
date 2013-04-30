CXXFLAGS= -Wall -Wextra -Werror -g -O2

%.o:%.cc
	$(CXX) -c $^ -o $@ $(CXXFLAGS) -Wa,-ahl=$@.s

numbers: numbers.o separate.o stat.o timer.o
	$(CXX) $^ -o $@ $(CXXFLAGS) -lpthread -lboost_thread -lboost_system

stat.txt: numbers
	./numbers > stat.txt

clean:
	$(RM) *.o *.o.s numbers stat.txt

poster: stat.txt template.tex
	./subst.py stat.txt < template.tex > poster.tex
	pdflatex poster.tex
	-rm poster.aux  poster.log poster.tex  poster.toc

.PHONY: poster

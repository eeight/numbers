CXXFLAGS= -Wall -Wextra -Werror -g -O2

%.o:%.cc
	$(CXX) -c $^ -o $@ $(CXXFLAGS) -Wa,-ahl=$@.s

numbers: numbers.o separate.o stat.o
	$(CXX) $^ -o $@ $(CXXFLAGS) -lpthread -lboost_thread

stat.txt: numbers
	./numbers > stat.txt

clean:
	-rm *.o *.o.s numbers stat.txt

poster: stat.txt template.tex
	./subst.py stat.txt < template.tex > poster.tex
	pdflatex poster.tex
	-rm poster.aux  poster.log poster.tex  poster.toc

.PHONY: poster

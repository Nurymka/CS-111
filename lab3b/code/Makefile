# NAME: Nurym Kudaibergen
# EMAIL: nurim98@gmail.com
# ID: 404648263

default: ./lab3b.py
	rm -f lab3b
	ln lab3b.py lab3b
	chmod 777 lab3b
dist: default
	tar -czvf lab3b-404648263.tar.gz README Makefile lab3b lab3b.py
clean:
	rm -f lab3b-404648263.tar.gz

WEBSITE=http://web.cs.ucla.edu/classes/cs111/Samples/
FILE=P3B-test

.PHONY: all
all:
	for i in $(shell seq 1 22); do curl -O $(WEBSITE)$(FILE)_$${i}.csv; done
	for i in $(shell seq 1 22); do curl -O $(WEBSITE)$(FILE)_$${i}.err; done
	mkdir -p tests/csv
	mkdir -p tests/err
	mv *.csv tests/csv
	mv *.err tests/err

cleantests:
	rm -rf tests/

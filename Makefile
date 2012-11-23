all:
	make -f Makefile.binary FNAME=agstract
	make -f Makefile.binary FNAME=agspack
	
clean:
	rm -f *.out
	rm -f *.o
	rm -f *.rcb

.PHONY: all 

all:
	make -f Makefile.binary FNAME=agstract
	make -f Makefile.binary FNAME=agspack
	make -f Makefile.binary FNAME=agscriptxtract
	make -f Makefile.binary FNAME=agssemble
	make -f Makefile.binary FNAME=agsdisas
	make -f Makefile.binary FNAME=agsinject
	
clean:
	rm -f *.out
	rm -f *.o
	rm -f *.rcb

.PHONY: all 

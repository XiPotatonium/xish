VPATH = src inc
includes = xish.h parser.h executor.h
obj = main.o executor.o parser.o xish.o

xish: ${obj}
	gcc $^ -o $@

main.o: main.c ${includes}
	gcc -c $<

executor.o: executor.c ${includes}
	gcc -c $<

parser.o: parser.c ${includes}
	gcc -c $<

xish.o: xish.c ${includes}
	gcc -c $<

clean:
	rm *.o

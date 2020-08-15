VPATH = src inc
includes = xish.h parser.h executor.h builtin.h
obj = main.o executor.o parser.o xish.o builtin.o

xish: ${obj}
	gcc -g $^ -o $@

main.o: main.c ${includes}
	gcc -g -c $<

executor.o: executor.c ${includes}
	gcc -g -c $<

parser.o: parser.c ${includes}
	gcc -g -c $<

xish.o: xish.c ${includes}
	gcc -g -c $<

builtin.o: builtin.c ${includes}
	gcc -g -c $<

clean:
	rm *.o

all: prog
run: prog
	./clock
prog:
	gcc process.c -pthread -o clock
clean:
	rm clock child*.txt

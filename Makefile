all: proccess_A proccess_B

proccess_A: proccess_A.c
	gcc -g -Wall proccess_A.c -o proccess_A -pthread -lrt -lm
proccess_B: proccess_B.c
	gcc -g -Wall proccess_B.c -o proccess_B -pthread -lrt -lm

clean:
	rm -f proccess_A proccess_B proccess_A.o proccess_B.o
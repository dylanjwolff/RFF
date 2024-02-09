all:
	gcc -O2 -o sum -pthread sum.c
	strip sum
	gcc -O2 -o sum.safe -pthread sum.c -DSAFE
	strip sum.safe


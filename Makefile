all: remove_scripts 

CFLAGS = -g -O0 -m32 -fno-stack-protector -fsanitize=address

remove_scripts: remove_scripts.c malloc-2.7.2.c
	gcc -DCOMPILETIME $(CFLAGS) -c malloc-2.7.2.c 
	gcc -Wall $(CFLAGS) -o remove_scripts remove_scripts.c malloc-2.7.2.o

clean:
	rm -f remove_scripts malloc-2.7.2.o

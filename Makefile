all: remove_scripts 

CFLAGS = -g -O0 -m32 -fno-stack-protector -fsanitize=address

test: remove_scripts.c
	gcc -Wall $(CFLAGS) -o remove_scripts remove_scripts.c

remove_scripts: remove_scripts.c malloc-2.7.2.c
	gcc -DCOMPILETIME -w $(CFLAGS) -c malloc-2.7.2.c 
	gcc -Wall $(CFLAGS) -o remove_scripts remove_scripts.c malloc-2.7.2.o

clean:
	rm -f remove_scripts malloc-2.7.2.o

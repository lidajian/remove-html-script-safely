all: remove_scripts

remove_scripts: remove_scripts.c
	gcc remove_scripts.c -o remove_scripts

clean:
	rm -f remove_scripts

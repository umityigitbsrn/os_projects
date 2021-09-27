all: schedule
schedule: schedule.c
	gcc -Wall -g -o schedule schedule.c -lpthread -lm
clean:
	ls | grep  -v '\.\|Makefile' | xargs rm -fr
	rm -rf *~

all: producer consumer isp
producer: producer.c
	gcc -Wall -g -o producer producer.c
consumer: consumer.c
	gcc -Wall -g -o consumer consumer.c
isp: isp.c
	gcc -Wall -g -o isp isp.c
clean:
	ls | grep  -v '\.\|Makefile' | xargs rm -fr
	rm -rf *~

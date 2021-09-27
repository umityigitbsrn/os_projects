all: libsbmemlib.a app create_memory_sb destroy_memory_sb

libsbmemlib.a:  sbmemlib.c
	gcc -Wall -g -c sbmemlib.c
	ar -cvq libsbmemlib.a sbmemlib.o
	ranlib libsbmemlib.a

app: app.c
	gcc -Wall -g -o app app.c #-L. -lsbmemlib -lpthread -lrt -lm

# app_01: app_01.c
# 	gcc -Wall -g -o app_01 app_01.c -L. -lsbmemlib -lpthread -lrt -lm
#
# app_02: app_02.c
# 	gcc -Wall -g -o app_02 app_02.c -L. -lsbmemlib -lpthread -lrt -lm
#
# app_03: app_03.c
# 	gcc -Wall -g -o app_03 app_03.c -L. -lsbmemlib -lpthread -lrt -lm

create_memory_sb: create_memory_sb.c
	gcc -Wall -o create_memory_sb create_memory_sb.c -L. -lsbmemlib -lpthread -lrt -lm

destroy_memory_sb: destroy_memory_sb.c
	gcc -Wall -o destroy_memory_sb destroy_memory_sb.c -L. -lsbmemlib -lpthread -lrt -lm

clean:
	rm -fr *.o *.a *~ a.out  app sbmemlib.o sbmemlib.a libsbmemlib.a  create_memory_sb destroy_memory_sb

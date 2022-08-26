all:
		ctags -R .
		gcc -c -Wall -Werror -fpic mfs.c udp.c
		gcc -shared -o libmfs.so mfs.o udp.o
		gcc -o client client.c -Wall -lmfs -L.
		gcc server.c udp.c -o server

clean:
		rm client server libmfs.so mfs.o udp.o 

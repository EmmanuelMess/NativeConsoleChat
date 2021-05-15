all: bin/client bin/server

bin/server: bin server.c messages.h
	gcc -Wall -Wextra -Werror -std=gnu11 messages.h server.c -lpthread -o bin/server

bin/client: bin client/client.c messages.h client/globalvars.h client/errormanagement.h obj/unrecievedqueue.o obj/senderthread.o obj/recieverthread.o obj/unsentqueue.o obj/ui.o
	gcc -g -Wall -Wextra -Werror -std=gnu11 client/globalvars.h client/errormanagement.h messages.h client/client.c obj/ui.o obj/unsentqueue.o obj/recieverthread.o obj/senderthread.o obj/unrecievedqueue.o -lpthread -lcurses -o bin/client

bin/server.exe: bin server.c messages.h
	x86_64-w64-mingw32-gcc-win32 -Wall -Wextra -Werror -std=gnu11  messages.h compatinet.h server.c -lpthread -o bin/server.exe -lwsock32 -lws2_32

obj/senderthread.o: obj client/senderthread.c client/senderthread.h messages.h client/errormanagement.h client/globalvars.h obj/unrecievedqueue.o obj/unsentqueue.o
	gcc -c -Wall -Wextra -Werror -std=gnu11 client/senderthread.c -o obj/senderthread.o

obj/recieverthread.o: obj client/recieverthread.c client/recieverthread.h messages.h obj/unrecievedqueue.o client/errormanagement.h
	gcc -c -Wall -Wextra -Werror -std=gnu11 client/recieverthread.c -o obj/recieverthread.o

obj/ui.o: obj client/ui.c client/ui.h obj/unrecievedqueue.o obj/unsentqueue.o
	gcc -c -Wall -Wextra -Werror -std=gnu11 client/ui.c -o obj/ui.o

obj/unrecievedqueue.o: obj client/unrecievedqueue.c client/unrecievedqueue.h
	gcc -c -c -Wall -Wextra -Werror -std=gnu11 client/unrecievedqueue.c -o obj/unrecievedqueue.o

obj/unsentqueue.o: obj client/unsentqueue.c client/unsentqueue.h
	gcc -c -c -Wall -Wextra -Werror -std=gnu11 client/unsentqueue.c -o obj/unsentqueue.o

obj:
	mkdir -p obj
bin:
	mkdir -p bin

clean:
	rm -fr obj/ bin/
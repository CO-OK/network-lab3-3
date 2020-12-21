server: dgram.c dgrecv.c msghdr.h
		gcc dgram.c dgrecv.c msghdr.h -g -o ./out/server
dgclient: dgram.c dgclient.c msghdr.h
		gcc dgram.c dgclient.c msghdr.h -g -o ./out/dgclient
clean:
		rm ./out/client ./out/server ./dgclient
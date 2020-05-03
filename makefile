default:server client

server:server.c common.c
	gcc -pthread $^ -o $@

client:client.c common.c
	gcc $^ -o $@

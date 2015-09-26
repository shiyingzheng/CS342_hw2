#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <errno.h>
#include <sys/stat.h>
#include <pthread.h>


void* server_stuff(void* sockptr) {
    int sock = *(int*) sockptr;
    char buf[255];
    // memset(&buf,0,sizeof(buf));
    int recv_count = recv(sock, buf, 255, 0);
    if(recv_count<0) {
         perror("Receive failed");	
         exit(1); 
    }

    char method[6], file[255], protocol[10];
    int n = sscanf(buf, "%s %s %s", method, file, protocol);
    char nbuf[3];
    nbuf[0] = n + '0';
    nbuf[1] = '\n';
    nbuf[2] = '\n';

    buf[recv_count] = '\n';
    write(1, buf, recv_count+1);
    write(1, method, strlen(method));
    write(1, file, strlen(file));
    write(1, protocol, strlen(protocol));
    write(1, nbuf, 3);

    if (strcmp(method,"GET")){
        write(sock, protocol, strlen(protocol));
        write(sock, " 501 Not Implemented\r\n\r\n", 24); 
    }
    else {

    }


    shutdown(sock,SHUT_RDWR);
    close(sock);
    pthread_exit(0);
}

int main(int argc, char** argv) {	

    if (argc < 3) {
        perror("Usage: hw2 <port> <folder> ");
        exit(1);
    }
    int port;
    int port_status = sscanf(argv[1], "%d", &port);
    if(!port_status) {
        perror("port must be a number ");
        exit(1);
    }

    char dir[255];
    int dir_status = sscanf(argv[2], "%s", dir);
    if(!dir_status) {
        perror("invalid folder ");
        exit(1);
    }

    struct stat dir_stat;
    int stat_status = stat(dir, &dir_stat);
    if(!stat_status) {
        perror("invalid folder ");
        exit(1);
    }
    

	int server_sock = socket(AF_INET, SOCK_STREAM, 0);
	if(server_sock < 0) {
		perror("Creating socket failed: ");
		exit(1);
	}
	
	// allow fast reuse of ports 
	int reuse_true = 1;
	setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &reuse_true, sizeof(reuse_true));

	struct sockaddr_in addr; 	// internet socket address data structure
	addr.sin_family = AF_INET;
	addr.sin_port = htons(8080); // byte order is significant
	addr.sin_addr.s_addr = INADDR_ANY; // listen to all interfaces
	
	int res = bind(server_sock, (struct sockaddr*)&addr, sizeof(addr));
	if(res < 0) {
		perror("Error binding to port");
		exit(1);
	}

	struct sockaddr_in remote_addr;
	unsigned int socklen = sizeof(remote_addr); 
	// wait for a connection
	res = listen(server_sock,0);
	if(res < 0) {
		perror("Error listening for connection");
		exit(1);
	}

    pthread_t thread;
	while(1) {
		int sock;
		sock = accept(server_sock, (struct sockaddr*)&remote_addr, &socklen);
		if(sock < 0) {
			perror("Error accepting connection");
			exit(1);
		}
        pthread_create(&thread, NULL, server_stuff, (void*)&sock);
        pthread_detach(thread);
		
	}

	shutdown(server_sock,SHUT_RDWR);
}


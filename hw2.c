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
#include <pthread.h>
#include <fcntl.h>

#define PACKET_SIZE 256
static char* dir;

int file_to_socket(char* file_name, int sock) {
	int fd = open(file_name, 0, "rb");
	char buf[PACKET_SIZE];
	if(fd < 0) {
		write(sock, " 500 Internal Error\r\n\r\n", 23);
		perror("Failed to open file\n");
		return -1;
	}

   	write(sock, " 200 OK\r\n\r\n", 11);
   	int bytes_read;
   	int total_bytes_written = 0;
	while ((bytes_read = read(fd, buf, PACKET_SIZE))) {
		int bytes_written = write(sock, buf, bytes_read);
		if (!bytes_written){
			perror("Sending file failed\n");
			return -1;
		}
		total_bytes_written += bytes_written;
	}
	close(fd);
	return total_bytes_written;
}

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
		char filepath[255];
        strcpy(filepath,dir);
        strcat(filepath,file);
        struct stat dir_stat;
        int stat_status = stat(filepath, &dir_stat);
        if(stat_status<0) {
            write(sock, protocol, strlen(protocol));
            write(sock, " 404 File not Found\r\n\r\n", 23);
        }
		else{
			write(sock, protocol, strlen(protocol));
			int bytes_written = file_to_socket(filepath, sock);
		}
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
        perror("Port must be a number ");
        exit(1);
    }

    struct stat dir_stat;
    int stat_status = stat(argv[2], &dir_stat);
    if(stat_status<0) {
        perror("Invalid folder ");
        exit(1);
    }

    if(!(dir_stat.st_mode & S_IFDIR)) {
        perror("Please use a folder ");
        exit(1);
    }

    dir = argv[2];
    if (dir[strlen(dir)-1] == '/') {
        dir[strlen(dir)] = 0;
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
	addr.sin_port = htons(port); // byte order is significant
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
		}
		else {
			pthread_create(&thread, NULL, server_stuff, (void*)&sock);
        	pthread_detach(thread);
		}
	}

	shutdown(server_sock,SHUT_RDWR);
}


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
static char* err404page = " 404 Not Found\r\nContent-Type: text/html\r\n\r\n<html><body><h1>Error 404: The file you requested does not exist.</h1></body></html>\n";
static char* err500page = " 500 Internal Error\r\nContent-Type: text/html\r\n\r\n<html><body><h1>Error 500: Internal server error.</h1></body></html>\n";
static char* err501page = " 501 Not Implemented\r\nContent-Type: text/html\r\n\r\n<html><body><h1>Error 501: Not implemented.</h1></body></html>\n";

/* Sends file and other important stuff in a response through the socket*/
int file_to_socket(char* file_name, int sock) {
	// open the file to read from
	int fd = open(file_name, 0, "rb");
	char buf[PACKET_SIZE];
	if(fd < 0) {
        write(sock, err500page, strlen(err500page));
		perror("Failed to open file\n");
		return -1;
	}
	write(sock, " 200 OK\r\n", 11);

	// determine and send the content type through socket
	char extension[10];
	int i;
	for(i = strlen(file_name) - 1; file_name[i] != '.'; i--) {
		;
	}
	strcpy(extension, file_name + i + 1);
	char contype[255];
	if(!strcmp(extension,"jpg")) {
		strcpy(contype, "Content-Type: image/jpg");
	}
	else if(!strcmp(extension,"png")) {
		strcpy(contype, "Content-Type: image/jpg");
	}
	else if(!strcmp(extension,"html")) {
		strcpy(contype, "Content-Type: text/html");
	}
	else if(!strcmp(extension,"txt")) {
		strcpy(contype, "Content-Type: text/plain");
	}
	else if(!strcmp(extension,"pdf")) {
		strcpy(contype, "Content-Type: application/pdf");
	}
	else if(!strcmp(extension,"gif")) {
		strcpy(contype, "Content-Type: image/gif");
	}
	else if(!strcmp(extension,"ico")) {
		strcpy(contype, "Content-Type: image/x-icon");
	}
	char* newcontype = strcat(contype, "\r\n\r\n");
	write(sock,newcontype,strlen(contype));

	// write the file to the socket
	int bytes_read;
	int total_bytes_written = 0;
	while ((bytes_read = read(fd, buf, PACKET_SIZE))) {
		int bytes_written = write(sock, buf, bytes_read);
		total_bytes_written += bytes_written;
	}

	close(fd);
	return total_bytes_written;
}

/* this function handles the client request in a separate thread */
void* server_stuff(void* sockptr) {
	// receive the client request
	int sock = *(int*) sockptr;
	char buf[255];
	int recv_count = recv(sock, buf, 255, 0);
	if(recv_count<0) {
		perror("Receive failed");
		exit(1);
	}

	// parse arguments from request
	char method[6], file[255], protocol[10];
	int n = sscanf(buf, "%s %s %s", method, file, protocol);
	if (strcmp(method,"GET")){
		write(sock, protocol, strlen(protocol));
        write(sock, err501page, strlen(err501page));
	}
	else {
		char filepath[255];
		strcpy(filepath,dir);
		strcat(filepath,file);
		struct stat dir_stat;
		int stat_status = stat(filepath, &dir_stat);
		if(stat_status<0) {
			write(sock, protocol, strlen(protocol));
            write(sock, err404page, strlen(err404page));
		}
		else {
			// if filepath points to a directory
			if(dir_stat.st_mode & S_IFDIR) {
				if (filepath[strlen(filepath)-1] != '/'){
					strcat(filepath, "/");
				}
				strcat(filepath,"index.html");
				int stat_status = stat(filepath, &dir_stat);
				if(stat_status<0) {
					write(sock, protocol, strlen(protocol));
                    write(sock, err404page, strlen(err404page));
					shutdown(sock,SHUT_RDWR);
					close(sock);
					pthread_exit(0);
				}
			}
			// if the thing is not a file or directory
			if (!(dir_stat.st_mode & S_IFREG)){
				write(sock, protocol, strlen(protocol));
                write(sock, err501page, strlen(err501page));
			}
			else{
				write(sock, protocol, strlen(protocol));
				int bytes_written = file_to_socket(filepath, sock);
			}
		}
	}
	shutdown(sock,SHUT_RDWR);
	close(sock);
	pthread_exit(0);
}

int main(int argc, char** argv) {
	// parse arguments to start the program
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
	setsockopt(server_sock,
			   SOL_SOCKET,
			   SO_REUSEADDR,
			   &reuse_true,
			   sizeof(reuse_true));

    struct sockaddr_in addr; // internet socket address data structure
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
    		// create a thread to handle requests from client side
    		pthread_create(&thread, NULL, server_stuff, (void*)&sock);
    		pthread_detach(thread);
    	}
    }

    shutdown(server_sock,SHUT_RDWR);
}

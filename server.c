#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#define PORT "3936" // port number for lisnening
#define BACKLOG 20 // queue size

void *get_in_addr(struct sockaddr *sa) {
	
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}
	
	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(){
	fd_set read_fds;//check for readability
	fd_set master; // master file descriptor list
	int fdmax; // max fds

	int listener; // listening socket
	int newfd; // accept()ed socket
	struct sockaddr_storage remoteaddr; // client address
	socklen_t addrlen;

	char buf[256];//buffer for client data
	int nbytes;

	char remoteIP[INET6_ADDRSTRLEN];

	int yes = 1;
	int i,j,rv;

	struct addrinfo hints, *ai, *p;

	FD_ZERO(&read_fds);
	FD_ZERO(&master);

	//step 1 make a socket and bind it

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC; //any ipv4 or ipv6
	hints.ai_socktype = SOCK_STREAM; // tcp
	hints.ai_flags = AI_PASSIVE; // fill my ip for me

	if((rv = getaddrinfo(NULL, PORT, &hints, &ai)) != 0){
		fprintf(stderr, "selectserver: %s\n", gai_strerror(rv));
		exit(1);
	}
	//bind to the first address

	for(p=ai;p!=NULL; p = p->ai_next){
		listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if(listener < 0){
			continue;
		}

		setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int) );

		if(bind(listener, p->ai_addr, p->ai_addrlen) < 0){
			close(listener);
			continue;
		}
		break;
	}
	if(p==NULL){
		fprintf(stderr, "selectServer: failed to bind\n");
		exit(2);
	}

	freeaddrinfo(ai); //remove ai info

	if(listen(listener, BACKLOG) == -1){
		perror("listen");
		exit(3);
	}

	FD_SET(listener, &master);

	//keep track of the biggest file descriptor

	fdmax = listener;

	while(1){

		read_fds = master;
		if(select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1){
			perror("select");
			exit(4);
		}

		//run through the existing connections

		for(i=0;i<=fdmax;i++){
			if(FD_ISSET(i, &read_fds)){
				if (i == listener){
					
					addrlen = sizeof(remoteaddr);
					newfd = accept(listener, (struct  sockaddr *)&remoteaddr, &addrlen);

					if(newfd == -1){
						perror("accept");
					}else{
						FD_SET(newfd, &master);
						if(newfd > fdmax){
							fdmax = newfd;
						}
						printf("selectserver: new connection from %s on socket %d\n",
							inet_ntop(remoteaddr.ss_family, 
								get_in_addr((struct sockaddr*)&remoteaddr),remoteIP, INET6_ADDRSTRLEN),newfd);

					}
				}else{
					//handle data from client
					if((nbytes = recv(i, buf, sizeof(buf), 0))<=0 ){
						if (nbytes == 0){
							printf("selectserver: socket %d hung up\n", i);
						} else {
							perror("recv");
						}
						close(i);//bye;
						FD_CLR(i, &master);
					}else{
						for(j=0;j<=fdmax;j++){
							if(FD_ISSET(j, &master)){
								if(j != listener && j != i){
									if(send(j,buf,nbytes, 0) == -1){
										perror("send");
									}
								}
							}
						}
					} // data handled
				}
			}
		}
	} //while true

	return 0;
}
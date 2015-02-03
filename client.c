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
#include <arpa/inet.h>
#include <pthread.h>

#define PORT "3936" // the port client will be connecting to
#define MAXDATASIZE 128 // max number of bytes we can get at once
// get sockaddr, IPv4 or IPv6:

pthread_mutex_t mutexsum = PTHREAD_MUTEX_INITIALIZER;
int done = 0;
int sockfd, numbytes;
char hostname[20];

void *get_in_addr(struct sockaddr *sa){
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}
void *sendmessage();
void *recieveMsg();

int main(int argc, char *argv[])
{

	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET6_ADDRSTRLEN];
	char buf[MAXDATASIZE];
	if (argc != 2) {
		fprintf(stderr,"usage: client hostname\n");
		exit(1);
	}
	if (gethostname(hostname, 20)<0){
		perror("hostname");
		exit(5);
	}
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	if ((rv = getaddrinfo(argv[1], PORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
	return 1;
	}
	// loop through all the results and connect to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {

		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
			perror("client: socket");
			continue;
		}
		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("client: connect");
			continue;
		}
		break;
	}

	if (p == NULL) {
		fprintf(stderr, "client: failed to connect\n");
		return 2;
	}

	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),	s, sizeof s);

	printf("client: connected to %s\n", s);

	freeaddrinfo(servinfo); // all done with this structure

	// Set up threads
    pthread_t threads[2];
    void *status;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    // Spawn the listen/receive deamons
    pthread_create(&threads[0], &attr, sendmessage, NULL);
    pthread_create(&threads[1], &attr, recieveMsg, NULL);

    // Keep alive until finish condition is done
    while(!done);

	close(sockfd);
	return 0;
}

void *sendmessage(){

    char str[80];
    char msg[128];

    while(1) {
    	printf("me> ");
    	fflush(stdin);
        // Get user's message
        memset(msg,0,strlen(msg));
        fgets(str, 80, stdin);

        // Build the message: "name: message"
        strcpy(msg,hostname);
        strcat(msg, "> ");
        strncat(msg,str,strlen(str));

        // Check for quiting
        if(strcmp(str,"exit")==0)
        {

            done = 1;
      
            pthread_mutex_destroy(&mutexsum);
            pthread_exit(NULL);
            close(sockfd);
        }    

        // Send message to server
        if(send(sockfd,msg,strlen(msg), 0) == -1){
			perror("send");
		}

        // scroll the top if the line number exceed height
        pthread_mutex_lock (&mutexsum);


        // scroll the bottom if the line number exceed height

        pthread_mutex_unlock (&mutexsum);
    }
}

void *recieveMsg(){
    char str[100];

    while(1){
        //Receive message from server

        if ((numbytes = recv(sockfd, str, MAXDATASIZE-1, 0)) == -1) {
			perror("recv");
			exit(1);
		}
			str[numbytes] = '\0';
			// printf("%c[2K", 27);
			printf("\r%s\n",str);puts("");
			printf("me> ");
        pthread_mutex_lock (&mutexsum);
        pthread_mutex_unlock (&mutexsum);

        
    }
}

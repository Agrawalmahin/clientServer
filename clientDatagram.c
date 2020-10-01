/*

    To compile: gcc -pthread clientDatagram.c -o client_datagram.o
    To run : ./client_datagram.o PORT

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h> 
#include <arpa/inet.h> 
#include <netinet/in.h> 
#include <fcntl.h>
#include <stdbool.h> 
#include <signal.h>
#include <netdb.h>

struct sockaddr_in serv_addr;
struct hostent* server;

void error(char *msg)
{
    perror(msg);
    exit(1);
}

bool flag = 1;
/* signal handler for ctrl-c override */
void sig_handler(int sigcode)
{
    flag = 0;
}


void* thread_fun_read(void* p_arg_sockfd)
{
    char buffer[1024];
    int sockfd = *( (int*)p_arg_sockfd );
    int cnt;
    socklen_t len; 
    len = sizeof(struct sockaddr_in);
    while(1)
    {
        if(!flag) break;
        bzero(buffer,1024);
        cnt = recvfrom(sockfd, (char *)buffer, 1024, 0, (struct sockaddr *) &serv_addr, &len); 
        if(!flag) break;
        if (cnt < 0) 
        {
            printf("ERROR reading from socket");
            break;
        }
        printf("--> %s\n",buffer);
        
        sleep(1);
    }
    // sig_handler(SIGINT);
    return NULL;
    
}

void* thread_fun_write(void* p_arg_sockfd)
{
    char buffer[1024];
    int sockfd = *( (int*)p_arg_sockfd );
    // printf("sockfd: %d\n",sockfd);
    int cnt;
    while(1)
    {
        // sleep(5);
        if(!flag) break;
        printf("> ");fflush(stdout);
        fgets(buffer,1023,stdin);
        cnt = sendto(sockfd, (char *)buffer, strlen(buffer), 0, (struct sockaddr *) &serv_addr, sizeof(serv_addr)); 
        if(cnt<0)
        {
            printf("Error in sendto\n");
            break;
        }
        printf("Message sent to server\n"); 
        int len1 = 0;
        while(len1<1024 && buffer[len1]!='\n') len1++;
        buffer[len1] = '\0';
        char ss[] = "exit";
        if(cnt==0 || strcmp(buffer,ss)==0)
        {
            printf("\nBye\n");
            flag = 0;
            break;
        }
        sleep(1);
    }
    // sig_handler(SIGINT);
    return NULL;
}


int main(int argc, char* argv[]) 
{ 
    /*  argv[1] is server IP address.
        argv[2] is port number.    
    */
    signal(SIGINT,sig_handler); /* Installing the signal handler */
    if(argc<2)
    {
        fprintf(stderr,"specify port number\n");
        exit(0);
    }

	char buffer[1024], msg[1024]; 
    int sockfd, portno, cnt;
    
    portno = atoi(argv[1]);

    sockfd = socket(PF_INET, SOCK_DGRAM, 0);
	if ( sockfd < 0 ) 
    { 
		error("Socket creation failed"); 
	} 

	memset(&serv_addr, '\0', sizeof(serv_addr)); 
	
	/* Filling server information */
	serv_addr.sin_family = AF_INET; 
	serv_addr.sin_port = htons(portno); 
	// serv_addr.sin_addr.s_addr = INADDR_ANY; 
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    // server = gethostbyname(argv[1]);

    // if(server==NULL)
    // {
    //     error("Invalid host name");
    // }
	// bcopy((char*)server->h_addr,(char*)&serv_addr.sin_addr.s_addr,server->h_length);

    // printf("sockfd: %d\n",sockfd);
    printf("To terminate , type exit.\n");
	printf("Please enter your name :\n");
    pthread_t tid1, tid2;
    pthread_create(&tid1,NULL,&thread_fun_write,&sockfd);
    pthread_create(&tid2,NULL,&thread_fun_read,&sockfd);

    while(flag);
    
	close(sockfd); 
	return 0; 
} 

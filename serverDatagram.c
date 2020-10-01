/*

    To compile: gcc -pthread serverDatagram.c -o server_datagram.o
    To run: ./server_datagram.o PORT

*/

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <signal.h>

#define BUFFER_SZ 2048
#define MSG_CONFIRM 0

static int uid = 10;
static int server_type = 1;
static _Atomic unsigned int cli_count = 0;

/* Client structure */
typedef struct{
	struct sockaddr_in address;
	int sockfd;
	int uid;
	char name[32];
    char message[BUFFER_SZ];
    int status_code;
} client_t;

struct Node *client_list = NULL;

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Structure for linked list */
struct Node {
	client_t client;
	struct Node *next;
};

void appendNode(client_t *new_client) 
{ 
	struct Node *new_node = (struct Node*) malloc(sizeof(struct Node)); 
    struct Node *last = client_list;
   
    new_node->client  = *new_client; 
    new_node->next = NULL; 

    if (client_list == NULL) { 
       client_list = new_node; 
       return; 
    }   
       
    while (last->next != NULL) last = last->next; 
   
    last->next = new_node; 
    return;     
} 

void deleteNode(int uid) 
{ 
    struct Node *temp = client_list, *prev; 

    if (temp != NULL && temp->client.uid == uid) { 
        client_list = temp->next;
        free(temp);
        return;
    } 

    while (temp != NULL && temp->client.uid != uid){ 
        prev = temp; 
        temp = temp->next; 
    } 
  
    if (temp == NULL) return; 
    prev->next = temp->next; 
    free(temp);
} 

void str_overwrite_stdout() {
    printf("\r%s", "> ");
    fflush(stdout);
}

void str_trim_lf (char* arr, int length) {
  	int i;
  	for (i = 0; i < length; i++) { // trim \n
    	if (arr[i] == '\n') {
      		arr[i] = '\0';
      		break;
    	}
  	}
}

/* Add client to client list */
void add_client(client_t *client){
	pthread_mutex_lock(&clients_mutex);

	appendNode(client);

	pthread_mutex_unlock(&clients_mutex);
}

/* Remove client from client list */
void remove_client(int uid){
	pthread_mutex_lock(&clients_mutex);

	deleteNode(uid);

	pthread_mutex_unlock(&clients_mutex);
}

/* Send message to next client */
void send_message(client_t * cli) {
	pthread_mutex_lock(&clients_mutex);
    int uid = cli->uid;

	struct Node *temp = client_list; 
    int n;

	while (temp != NULL && temp->client.uid != uid) 
    	temp = temp->next;

	if (temp->next != NULL) {
        n = sendto(temp->next->client.sockfd, cli->message, strlen(cli->message), MSG_CONFIRM, (struct sockaddr *) &temp->next->client.address, sizeof(temp->next->client.address) );
		if(n < 0)
			perror("ERROR: write to descriptor failed");
	}
	else if (client_list->next != NULL) {
        n = sendto(client_list->client.sockfd, cli->message, strlen(cli->message), MSG_CONFIRM, (struct sockaddr *) &client_list->client.address, sizeof(client_list->client.address) );
		if(n < 0)
			perror("ERROR: write to descriptor failed");
	}

	pthread_mutex_unlock(&clients_mutex);
}

/* Send message to all clients except sender */
void broadcast_message(client_t* cli) {
	pthread_mutex_lock(&clients_mutex);
    int uid = cli->uid;
	struct Node *temp = client_list; 
    int n;
	while (temp != NULL) {
		if (temp->client.uid != uid) {
            n = sendto(temp->client.sockfd, cli->message, strlen(cli->message), MSG_CONFIRM, (struct sockaddr *) &temp->client.address, sizeof(temp->client.address));

			if(n < 0)
				perror("ERROR: write to descriptor failed");
		}
		temp = temp->next;
	}

	pthread_mutex_unlock(&clients_mutex);
}


/* Send acknowledgment to the sender*/
void send_acknowledge(client_t *client){
	char str[] = "Message Received!\n";
	pthread_mutex_lock(&clients_mutex);

    int n = sendto(client->sockfd, str, 1+strlen(str), MSG_CONFIRM, (struct sockaddr *) &client->address, sizeof(client->address) );

	if(n < 0){
		perror("ERROR: write to descriptor failed");
	}

	pthread_mutex_unlock(&clients_mutex);
}

int search_client(client_t * client){
    pthread_mutex_lock(&clients_mutex);

    struct Node* tempNode = client_list; 
    while (tempNode != NULL)
    {
        if(tempNode -> client.address.sin_port ==  client -> address.sin_port){
            pthread_mutex_unlock(&clients_mutex);
            return  tempNode -> client.uid;
        }
        tempNode = tempNode -> next;
    }
    pthread_mutex_unlock(&clients_mutex);
    return -1;
}

char* search_client_name(int uid){
    pthread_mutex_lock(&clients_mutex);

    struct Node* tempNode = client_list; 
    while (tempNode != NULL)
    {
        if(tempNode -> client.uid ==  uid){
            pthread_mutex_unlock(&clients_mutex);
            return  tempNode -> client.name;
        }
        tempNode = tempNode -> next;
    }
    pthread_mutex_unlock(&clients_mutex);
    return NULL;
}

/* Handle all communication with the client */
void *handle_message(void *arg){ // this is actually message handler in UDP
	char buff_out[BUFFER_SZ];
	char name[32];
	// int leave_flag = 0;

	// cli_count++;
	client_t *cli = (client_t *)arg;

	/* if(recv(cli->sockfd, name, 32, 0) <= 0 || strlen(name) <  2 || strlen(name) >= 32-1){
		printf("Didn't enter the name.\n");
		leave_flag = 1;
	} else{
		strcpy(cli->name, name);
		sprintf(buff_out, "%s has joined\n", cli->name);
		printf("%s", buff_out);
		//send_message(buff_out, cli->uid);
	} */

    str_trim_lf(cli->message, strlen(cli->message));
    
    int client_uid = search_client(cli);
    if(client_uid == -1){
        if(strcmp(cli->message, "exit") == 0){
            pthread_detach(pthread_self());
            return NULL;
        }
        cli->uid = uid++;
        strcpy(cli->name,cli->message);
        sprintf(buff_out, "%s has joined\n", cli->name);
        puts(buff_out);
        add_client(cli);
        cli_count++;
        return NULL;
    }
    else{
        // printf("%d: is the client_uid\n", client_uid);
        cli->uid = client_uid;   
    }

    strcpy(cli->name,search_client_name(cli->uid));
    str_trim_lf(cli->name, strlen(cli->name));
    if(strcmp(cli->message, "exit") == 0){
        cli->status_code = -1;
    }
    
	bzero(buff_out, BUFFER_SZ);


    // int receive = recv(cli->sockfd, buff_out, BUFFER_SZ, 0);
    if (cli->status_code > 0){
        if(strlen(cli->message) > 0)
        {
            // send_acknowledge(cli->sockfd);
            
            sprintf(buff_out, "%s : %s", cli->name, cli->message);
            puts(buff_out);

            send_acknowledge(cli);

            strcpy(cli->message,buff_out);
            if (server_type)
                send_message(cli); // send message to the next connected client
            else
                broadcast_message(cli);
            
            bzero(buff_out, BUFFER_SZ);
            // str_trim_lf(buff_out, strlen(buff_out));
            // printf("%s\n", buff_out);
        }
    } 
    else if (cli->status_code <= 0){
        sprintf(buff_out, "%s has left\n", cli->name);
        printf("%s", buff_out);
        strcpy(cli->message, buff_out);
        send_message(cli);
        // leave_flag = 1;
        /* Delete client from client list and yield thread */
        // close(cli->sockfd);
        remove_client(cli->uid);
        free(cli);
        cli_count--;
    } 

    bzero(buff_out, BUFFER_SZ);

  	pthread_detach(pthread_self());

	return NULL;
}

int main(int argc, char **argv){
	if(argc != 2){
		printf("Usage: %s <port>\n", argv[0]);
		return EXIT_FAILURE;
	}

	char *ip = "127.0.0.1";
	int port = atoi(argv[1]);
	int option = 1;
	int listenfd = 0, connfd = 0;
  	struct sockaddr_in serv_addr;
  	struct sockaddr_in cli_addr;
  	pthread_t tid;

  	/* Socket settings */
  	listenfd = socket(AF_INET, SOCK_DGRAM, 0);
  	serv_addr.sin_family = AF_INET;
  	serv_addr.sin_addr.s_addr = inet_addr(ip);
  	serv_addr.sin_port = htons(port);

  	/* Ignore pipe signals */
	signal(SIGPIPE, SIG_IGN);
	if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR , &option, sizeof(option)) < 0){
		perror("ERROR: setsockopt failed");
    	return EXIT_FAILURE;
	}

	/* Bind */
  	if(bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("ERROR: Socket binding failed");
        return EXIT_FAILURE;
  	}

  	/* Listen */
  	// if (listen(listenfd, 10) < 0) {
    // perror("ERROR: Socket listening failed");
    // return EXIT_FAILURE;
	// }

	/* Get Server Type */
	while(1) {
		printf("Enter 1 if you want conversation in one-one mode\n");
        printf("Enter 2 if you want conversation in broadcast mode\n");
		scanf("%d", &server_type);
        printf("===WELCOME TO THE CLIENT-SERVER CHAT SERVICE===\n");
		if (server_type != 1 && server_type != 2) printf("\nEnter only 1 or 2\n");
		else if (server_type == 2){
			server_type = 0;
			break;
		}
		else break;
	}
    char buffer[BUFFER_SZ];
    // printf("reached till while\n");

	while(1){
        memset(&cli_addr, '\0', sizeof(cli_addr));
		socklen_t clilen = sizeof(cli_addr);
        bzero(buffer, BUFFER_SZ);
		// connfd = accept(listenfd, (struct sockaddr*)&cli_addr, &clilen);
        // connfd = recvfrom(listenfd, (char *)buffer, BUFFER_SZ, MSG_WAITALL, ( struct sockaddr *) &cli_addr, &clilen); 
        connfd = recvfrom(listenfd, (char *)buffer, BUFFER_SZ, 0, ( struct sockaddr *) &cli_addr, &clilen); 

        /* Client settings */
        client_t *cli = (client_t *)malloc(sizeof(client_t));
        cli->address = cli_addr;
        cli->sockfd = listenfd;
        cli->uid = -1;
        bzero(cli->message, BUFFER_SZ);
        strcpy(cli->message,buffer);
        cli->status_code = connfd;

        /* Add client to the client list and create new thread */
        // add_client(cli);
        pthread_create(&tid, NULL, &handle_message, (void*)cli);


		sleep(1);
	}

	return EXIT_SUCCESS;
}

/*******************************
 * Liam McDermott
 * 2016
 *******************************/

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>

#define MAX_MESSAGE 1000
#define MAX_USERNAME 50
#define MAX_CONNECTIONS 20

struct chatClient {
	int sock;
	char username[MAX_USERNAME];
};

struct chatClient* clientList[MAX_CONNECTIONS]; 

void addClient (int sock, char *username) {
	int i = 0;
	struct chatClient* p = clientList[0];
	while (p != NULL) {	// Find available slot	
		i += 1;
		p = clientList[i];
	}
	p = (struct chatClient*) malloc(sizeof(struct chatClient));
	p->sock = sock;
	strcpy(p->username, username);
	clientList[i] = p;
}

int main(int argc, char *argv[]) {

	if (argc != 2) {
		printf("Usage: ./chatserver [port no.]\n");
		return 1;
	}

	fd_set readfds, testfds;

	struct sockaddr_in listenaddr, clientaddr;

	int wait_for_connections = 1;
	int status, i, recvbytes, newSock;

	int servSock = socket(AF_INET, SOCK_STREAM, 0);
	if (servSock == -1) { printf("Can't open socket\n"); return -1; }
	
	memset(&listenaddr, 0, sizeof listenaddr);
	listenaddr.sin_family = AF_INET;
	listenaddr.sin_port = htons(atoi(argv[1]));
	inet_pton(AF_INET, "localhost", &(listenaddr.sin_addr));
	if ( bind(servSock, (struct sockaddr*) &listenaddr, sizeof(listenaddr)) == -1 ) { 
		printf("Can't bind socket\n"); return -1; 
	}

	listen(servSock, MAX_CONNECTIONS);
	
	FD_ZERO(&readfds);
	FD_SET(servSock, &readfds);

	printf("Waiting for clients...\n");

	char buffer[MAX_MESSAGE];

	while (wait_for_connections) {

		testfds = readfds;
		status = select(FD_SETSIZE, &testfds, (fd_set *)0, (fd_set *)0, (struct timeval *)0);
		for (i = 0; i < FD_SETSIZE; i+=1) {
			if (FD_ISSET(i, &testfds)) {
				if (i == servSock) {
					newSock = accept(servSock, (struct sockaddr *)0, NULL);
					FD_SET(newSock, &readfds);
					printf("Adding new client: %d\n", newSock);
					memset(buffer, 0, MAX_MESSAGE);
					recv(newSock, buffer, MAX_MESSAGE, 0);	// Get the username
				
					int j;
					int found = 0;
					struct chatClient *p;
					for (j=0; j < MAX_CONNECTIONS && found == 0; j+=1) {
						p = clientList[j];
						if (p != NULL) {
							if (strcmp(buffer, p->username) == 0) {
								// Duplicate name found
								found = 1;
								// Disconnect new client and send error message
								sprintf(buffer, "E");
								send(newSock, buffer, 1, 0);
								close(newSock);
								FD_CLR(newSock, &readfds);			
							}
						}
					}
					if (!found) {
						addClient(newSock, buffer);
						printf("New user: %s\n", buffer);
						char response = 'P';
						char username[MAX_USERNAME];
						strcpy(username, buffer);				
						send(newSock, &response, 1, 0);	// Send pass ACK
						sprintf(buffer, "New user [%s] has joined", username);
						//Broadcast user
						for (j=0; j < MAX_CONNECTIONS; j+=1) {
							p = clientList[j];
							if (p != NULL) {
								if (p->sock != newSock) {
									send(p->sock, buffer, MAX_MESSAGE, 0);
								}
							}
						}
					}
				}
				else {
					recvbytes = recv(i, buffer, MAX_MESSAGE, 0);
					if (recvbytes == -1 || recvbytes == 0) {
						// An error occured: most likely client crashed
						buffer[0] = 'X'; // Force removal of client
					}
					char prefix = buffer[0];
					char header[100];
					sscanf(buffer+2, "%s", header);
					
					if (prefix == 'X') {
						close(i);
						FD_CLR(i, &readfds);
						printf("Closing connection to client: %d\n", i);

						// Remove the client
						int j;
						int done = 0;
						struct chatClient* p;
						for (j=0; j < MAX_CONNECTIONS; j+=1) {
							p = clientList[j];
							if (p != NULL) {							
								if (p->sock == i) {
									free(clientList[j]);
									clientList[j] = NULL;
									done = 1;
								}
							}
						}
					}
					else if (prefix == 'B') {
						int j = 0;
						char *ptr = strtok(buffer, " ");
						ptr = strtok(NULL, "");	// ptr is now the message to broadcast
						char buffer2[MAX_MESSAGE];
						strcpy(buffer2, ptr);
						
						struct chatClient* p;
						for (j=0; j < MAX_CONNECTIONS; j+=1) {
							p = clientList[j];
							if (p->sock == i) {
								sprintf(buffer, "[%s]: %s", p->username, buffer2);
								j = MAX_CONNECTIONS;
							}
						}

						
						for (j=0; j < MAX_CONNECTIONS; j+=1) {
							p = clientList[j];
							if (p != NULL) {
								send(p->sock, buffer, MAX_MESSAGE, 0);
							}
						}
				
					}
					else if (prefix == 'L') {
						
						int j = 0;
						char namebuffer[MAX_USERNAME+3];						
						struct chatClient* p;
						memset(buffer, 0, MAX_MESSAGE);
						sprintf(buffer, "Users:");
						for (j=0; j<MAX_CONNECTIONS; j+=1) {
							p = clientList[j];
							if (p != NULL) {
								sprintf(namebuffer, " %s ", p->username);
								namebuffer[MAX_USERNAME+2] = '\0';
								strncat(buffer, namebuffer, strlen(namebuffer));
							}
						}
						send(i, buffer, MAX_MESSAGE, 0);
					}
					else if (prefix == 'U') {
						int j;
						int found = 0;
						char namebuffer[MAX_USERNAME];
						
						char buffer2[MAX_MESSAGE];
						char *ptr = strtok(buffer, " ");
						ptr = strtok(NULL, " ");	
						strcpy (namebuffer, ptr);
						ptr = strtok(NULL, "");	
						strcpy(buffer2, ptr);
		
						struct chatClient* p;
		
						// Find username
						for (j=0; j < MAX_CONNECTIONS; j+=1) {
							p = clientList[j];
							if (p->sock == i) {
								sprintf(buffer, "[%s->%s]: %s", p->username, namebuffer, buffer2);
								j = MAX_CONNECTIONS;
							}
						}

						for (j=0; j < MAX_CONNECTIONS; j+=1) {
							p = clientList[j];
							if (p != NULL) {
								if (strcmp(namebuffer, p->username) == 0) {
									found = 1;
									send(p->sock, buffer, MAX_MESSAGE, 0);
								}
							}
						}
						if (found) { send(i, buffer, MAX_MESSAGE, 0); }
						else {
							snprintf(buffer, MAX_MESSAGE, "User not found...");
							send(i, buffer, MAX_MESSAGE, 0);
						} 
					}
				}
			}
		}
	}

	return 0;
}

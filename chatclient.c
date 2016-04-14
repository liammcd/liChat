/*******************************
 * Liam McDermott
 * 2016
 *******************************/

#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>

#define MAX_MESSAGE 1000
#define MAX_USERNAME 50

int client_active = 1;  
int line = 0;
int sock;
int x, y;
char username[MAX_USERNAME];

WINDOW *top;
WINDOW *bottom;

void setupLine() {
	if (line != y-4) { 
		line += 1;
	}
	else {
		scroll(top);
		wprintw(top, "\n\n");
		box(top, '|', '-');
		refresh();
		wrefresh(top);
	} 
}

void *listener() {

	char buffer[MAX_MESSAGE];
	int recvbytes;

	curs_set(0);
	while(client_active) {
	
		recvbytes = recv(sock, buffer, MAX_MESSAGE, 0);
		setupLine();
	
		if ( (recvbytes == -1 || recvbytes == 0) && client_active) {
			printf("\nError receiving from server\n");
			endwin();
			exit(1);
		}
		wmove(top, line, 2);
		wprintw(top, ">%s", buffer);
		wrefresh(top);
		wmove(bottom, y-1, 3);	// Move cursor back to prompt
		memset(buffer, 0, MAX_MESSAGE);
	}
}

void *sender() {

	int status;	
	char str[MAX_MESSAGE]; // readline
	char buffer[MAX_MESSAGE];
	char *ptr;

	curs_set(0);
	while (client_active) {

		move(y-1, 1);
		wprintw(bottom, ">");
		wgetstr(bottom, str);
		ptr = strtok(str, " "); // Gets first word (i.e. command)

		if (ptr != NULL) {
			if (strcmp(ptr, "broadcast") == 0) {
				ptr = strtok(NULL, "");
				if (ptr != NULL) {
					strcpy(buffer, ptr);
					snprintf(buffer, MAX_MESSAGE, "B %s", ptr);
					status = send(sock, buffer, MAX_MESSAGE, 0);
					if (status == -1) { 
						printf("Error sending message\n"); 
						endwin();
						exit(1);
					}
				}
				else {
					setupLine();
					wmove(top, line, 2);	 
					wprintw(top, ">Invalid command...");
					wrefresh(top);
				}					
			}
			else if (strcmp(ptr, "exit") == 0) {
				memset(buffer, '\0', MAX_MESSAGE);
				sprintf(buffer, "X");
				status = send(sock, buffer, 1, 0);
				close(sock);
				printf("Closed connection\n");
				client_active = 0;
			}
			else if (strcmp(ptr, "list") == 0) {
				memset(buffer, '\0', MAX_MESSAGE);
				sprintf(buffer, "L");
				status = send(sock, buffer, 1, 0);
				if (status == -1) {
					printf("Error sending message\n");
					endwin();					
					exit(1);
				}
			}
			else {
				char namebuffer[MAX_USERNAME];
				strcpy(namebuffer, ptr);
				memset(buffer, '\0', MAX_MESSAGE);
				ptr = strtok(NULL, "");
				if (ptr != NULL) {
					snprintf(buffer, MAX_MESSAGE, "U %s %s", namebuffer, ptr);
					status = send(sock, buffer, MAX_MESSAGE, 0);
					if (status == -1) {
						printf("Error sending message\n");
						endwin();
						exit(1);
					}
				}
				else {
					setupLine();
					wmove(top, line, 2);
					wprintw(top, ">Invalid command...");
					wrefresh(top);
				}
			}
		}			
	}

}

int main(int argc, char *argv[]) {
	
	// Initialize curses screen
	initscr();
	start_color();
	getmaxyx(stdscr, y, x);

	// Create windows
	top = newwin(y-1, x, 0, 0);
	bottom = newwin(1, x, y-1, 0);	
	scrollok(top,TRUE);
	scrollok(bottom,TRUE);

	box(top, '|', '-');
	refresh();
	wrefresh(top);

	int status;
	int accepted = 0;
	char response;
	char str[MAX_USERNAME];
	strcpy(str, argv[3]);

	struct addrinfo serveraddr, *res;
	memset(&serveraddr, 0, sizeof serveraddr);
	serveraddr.ai_family = AF_UNSPEC;
	serveraddr.ai_socktype = SOCK_STREAM;

	if (status = getaddrinfo(argv[1], argv[2], &serveraddr, &res) != 0) {
		printw("Error getting address info\n");
		refresh();		
		endwin();
		return 1;
	}

	init_color(COLOR_YELLOW, 1000, 1000, 0);
	init_color(COLOR_GREEN, 0, 1000, 0);
	init_pair(1, COLOR_YELLOW, COLOR_BLUE);
	init_pair(2, COLOR_GREEN, COLOR_BLACK);
	wbkgd(top, COLOR_PAIR(1));
	wbkgd(bottom, COLOR_PAIR(2));
	wrefresh(top);
	wrefresh(bottom);

	while (!accepted) {
	
		sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		if ( (status = connect(sock, res->ai_addr, res->ai_addrlen)) == -1 ) {
			move (y-1, 1);		
			wprintw(bottom, "Error connecting to server...exiting\n");
			wrefresh(bottom);	
			getch();
			endwin();
			return 1;
		}

		send(sock, str, strlen(str), 0);	// Inform server of username
		status = recv(sock, &response, 1, 0); 	// Get username response (1 byte by protocol)
		if (response == 'P') {
			// Username was accepted
			accepted = 1;
			strcpy(username, str);
			setupLine();
			wmove(top, line, 2);
			wprintw(top, ">Registered username [%s]", str);
			wrefresh(top);
		}
		else {
			setupLine();
			wmove(top, line, 2);
			wprintw(top, ">Username [%s] already taken...try again", str);
			wrefresh(top);
			close(sock);
		}
		
		if (!accepted) {
			move(y-1, 1);
			wprintw(bottom, "Enter new username: ");
			wrefresh(bottom);
			move(y-1, 16);	
			wgetstr(bottom, str);
		}
	}

	pthread_t threads[2];
    	pthread_attr_t attr;
    	pthread_attr_init(&attr);
    	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	
	//Spawn daemons
	pthread_create(&threads[0], &attr, sender, NULL);
    	pthread_create(&threads[1], &attr, listener, NULL);
	while(client_active);
	endwin();

	return 0;
}

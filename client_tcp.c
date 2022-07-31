#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "helper.h"

#define LENGTH 2048
#define LEN_NAME 128
#define LEN_CMD 12
#define SIZE 1024
#define IP "127.0.0.1"
#define PORT 8080

#define MAX_LEN_MSG 2048
#define MAX_LEN_USERNAME 128
#define OFFSET 32

bool flag_exit_is_true = false;

FILE *fp;
int socket_fd = 0;

typedef struct {
	char username[MAX_LEN_USERNAME];
}client_info;


void str_trim_lf(char *arr, int length) {
	int i;
	for (i = 0; i < length; i++) {
		if (arr[i] == '\n') {
			arr[i] = '\0';
			break;
		}
	}
}

bool string_compare(const char * s1, const char * s2) {
    if (strcmp(s1, s2) == 0) {
        return true;
    } else {
        return false;
    }
}

void printf_log(const char *fmt, ...) {
#ifdef DEBUG
    va_list args;
    vprintf(fmt, args);
#endif
}

void exit_progress() {
    flag_exit_is_true = true;
}

void send_file(FILE *fp, int sockfd, char * namefile) {
	char name_file_test[1028] = {0};
	char data_send[1028] = {0};
	char data_from_file[1028] = {0};
	strcpy(name_file_test, namefile);
	while(send(sockfd, name_file_test, sizeof(name_file_test), 0) <= 0) {}
    while(fgets(data_from_file, SIZE, fp) != NULL) {
		strcpy(data_send, data_from_file);
        if(send(sockfd, data_send, sizeof(data_send), 0) == -1) {
            printf("ERROR: Error in sending data");
            exit(1);
        }
        bzero(data_send, SIZE);
		bzero(data_from_file, SIZE);
    }
}

void show_menu() {
	char msg_cmd[LEN_CMD] = {};
	char file_name[LEN_NAME] = {0};
	printf("MENU:\n-c or --chat: send chat message\n-f or --file: send a file\n");
	fgets(msg_cmd, LEN_CMD, stdin);
	str_trim_lf(msg_cmd, LEN_CMD);

	if (strcmp(msg_cmd, "-c") == 0 || strcmp(msg_cmd, "--chat") == 0) {
		return;
	} else if (strcmp(msg_cmd, "-f") == 0 || strcmp(msg_cmd, "--file") == 0) {
		send(socket_fd, msg_cmd, strlen(msg_cmd), 0);
		printf("Please enter the path file: ");
		fgets(file_name, LEN_NAME, stdin);
		str_trim_lf(file_name, LEN_NAME);
		fp = fopen(file_name, "r");
		if(fp == NULL) {
			printf("ERROR: Error in reading file.");
			return;
		}
		send_file(fp,socket_fd, file_name);
		fclose(fp);
	}
}

void send_msg_unicast(client_info * cli_info, char * message, int sockfd, char * username_receiver) {
	char buffer[MAX_LEN_MSG + MAX_LEN_USERNAME + OFFSET] = {0};

	sprintf(buffer, "trans_type=unicast&msg_type=text_msg&username_sender=%s&username_receiver=%s", cli_info->username, username_receiver);
	send(sockfd, buffer, strlen(buffer), 0);

	bzero(buffer, sizeof(buffer));
	send(sockfd, message, strlen(message), 0);
}


void send_msg_broadcast(client_info * cli_info, char * message, int sockfd) {
	char buffer[MAX_LEN_MSG + MAX_LEN_USERNAME + OFFSET] = {0};

	sprintf(buffer, "trans_type=broadcast&msg_type=text_msg&username_sender=%s", cli_info->username);
	send(sockfd, buffer, strlen(buffer), 0);

	bzero(buffer, sizeof(buffer));
	send(sockfd, message, strlen(message), 0);
}

void send_msg_inform_exist(client_info * cli_info, int sockfd) {
	char buffer[MAX_LEN_MSG + MAX_LEN_USERNAME + OFFSET] = {0};

	sprintf(buffer, "trans_type=broadcast&msg_type=infrom_exist&username_sender=%s#", cli_info->username);
	send(sockfd, buffer, strlen(buffer), 0);
}

void chat_unicast(client_info * cli_info, int sockfd) {
	char message[MAX_LEN_MSG] = {0};

    while (1) {
        if (flag_exit_is_true) {
            break;
        }

		char username_receiver[MAX_LEN_USERNAME] = {0};
		printf("Enter receiver: ");
		fgets(username_receiver, sizeof(username_receiver), stdin);
        str_trim_lf(username_receiver, sizeof(username_receiver));
		
		//verify_receiver(username_receiver)

        printf("> ");
        fgets(message, sizeof(message), stdin);
        str_trim_lf(message, sizeof(message));

        if (string_compare(message, "-e") || string_compare(message, "--exit")) {
            printf_log("DEBUG: Exit program\n");
            exit_progress();
        }
        else if (string_compare(message, "-f") || string_compare(message, "--file")) {
            printf_log("DEBUG: Enter send file\n");
            //send_file();
        }
        else {
            printf_log("DEBUG: Enter send message broadcast\n");
			send_msg_unicast(cli_info, message, sockfd, username_receiver);
        }

        bzero(message, sizeof(message));
    }
}

void send_msg_handler(client_info * cli_info) {
	char message[MAX_LEN_MSG] = {0};

	while (1) {
		if (flag_exit_is_true) {
			break;
        }

		printf("---MENU:---\n");
        printf("-cb or --chat_broadcast: Send to all member\n");
        printf("-cu or --chat_unicast: Send to a particular member\n");
        printf("-e or --exit: Exit program\n");
        printf("Your option: ");

        fgets(message, sizeof(message), stdin);
        str_trim_lf(message, sizeof(message));

        if (string_compare(message, "-cb") || string_compare(message, "--chat_broadcast")) {
            printf_log("Enter chat broadcast\n");
			//chat_broadcast();
		} 
        else if (string_compare(message, "-cu") || string_compare(message, "--chat_unicast")) {
            printf_log("Enter chat unicast\n");
			chat_unicast(cli_info, socket_fd);
		}
		else if (string_compare(message, "-e") || string_compare(message, "--exit")) {
            printf_log("Exit program\n");
			exit_progress();
            break;
		}
        else {
            printf("ERROR: Input invalid\n");
            flag_exit_is_true = true;
        }

		bzero(message, sizeof(message));

		/*
		printf("> ");
		fgets(message, sizeof(message), stdin);
		str_trim_lf(message, sizeof(message));

		if (strcmp(message, "exit") == 0) {
			break;
		}
		else if (strcmp(message, "--menu") == 0 || strcmp(message, "-m") == 0) {
			show_menu();
		}
		else {
			sprintf(buffer, "%s: %s\n", name, message);
			send(sockfd, buffer, strlen(buffer), 0);
		}

		bzero(message, LENGTH);
		bzero(buffer, LENGTH + 32);
		*/
	}
}

void recv_msg_handler() {
	char message[LENGTH] = {};
	while (1) {
		int receive = recv(socket_fd, message, LENGTH, 0);
		if (receive > 0) {
			printf("%s", message);
			printf("%s", "> ");
		}
		memset(message, 0, sizeof(message));
	}
}

int main(int argc, char *argv[]) {

	client_info cli_info;
	
	printf("Please enter your name: ");
	fgets(cli_info.username, sizeof(cli_info.username), stdin);
	str_trim_lf(cli_info.username, strlen(cli_info.username));

	if (strlen(cli_info.username) > 32 || strlen(cli_info.username) < 2) {
		printf("ERROR: Name is invalid.\n");
		return EXIT_FAILURE;
	}

	struct sockaddr_in server_addr;

	socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(IP);
	server_addr.sin_port = htons(PORT);

	int err = connect(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
	if (err == -1) {
		printf("ERROR: connect\n");
		return EXIT_FAILURE;
	}

	send_msg_inform_exist(&cli_info, socket_fd);

	printf("=== WELCOME TO THE CHATROOM ===\n");

	pthread_t send_msg_thread;
	if (pthread_create(&send_msg_thread, NULL, (void *)send_msg_handler, &cli_info) != 0) {
		printf("ERROR: pthread\n");
		return EXIT_FAILURE;
	}

	pthread_t recv_msg_thread;
	if (pthread_create(&recv_msg_thread, NULL, (void *)recv_msg_handler, NULL) != 0) {
		printf("ERROR: pthread\n");
		return EXIT_FAILURE;
	}

	while (1) {
		if (flag_exit_is_true) {
			break;
		}
	}

	close(socket_fd);

	return EXIT_SUCCESS;
}

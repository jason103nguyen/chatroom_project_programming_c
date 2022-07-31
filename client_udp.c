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

#define LENGTH 2048
#define PORT 8080
#define IP "127.0.0.1"

FILE *fp;
char name[32];

int sockfd = 0;
char name[32];
struct sockaddr_in server_addr, client_addr;

void send_file(FILE *fp, int sockfd, char * namefile) {
	char name_file_test[1028] = {0};
	char data_send[1028] = {0};
	char data_from_file[1028] = {0};
	strcpy(name_file_test, namefile);
	sendto(sockfd, (const char*)name_file_test, strlen(name_file_test), 0, (const struct sockaddr*)&server_addr, sizeof(server_addr));
    while(fgets(data_from_file, sizeof(data_from_file), fp) != NULL) {
		strcpy(data_send, data_from_file);
		int sendStatus = sendto(sockfd, (const char*)data_send, strlen(data_send), 0, (const struct sockaddr*)&server_addr, sizeof(server_addr));
        if(sendStatus == -1) {
            printf("ERROR: Error in sending data");
            exit(1);
        }
        bzero(data_send, sizeof(data_send));
		bzero(data_from_file, sizeof(data_from_file));
    }
}

void str_trim_lf(char *arr, int length) {
	int i;
	for (i = 0; i < length; i++) {
		if (arr[i] == '\n') {
			arr[i] = '\0';
			break;
		}
	}
}

void show_menu() {
	char msg_cmd[128] = {0};
	char file_name[128] = {0};
	printf("MENU:\n-c or --chat: send chat message\n-f or --file: send a file\n");
	fgets(msg_cmd, sizeof(msg_cmd), stdin);
	str_trim_lf(msg_cmd, sizeof(msg_cmd));

	if (strcmp(msg_cmd, "-c") == 0 || strcmp(msg_cmd, "--chat") == 0) {
		return;
	} else if (strcmp(msg_cmd, "-f") == 0 || strcmp(msg_cmd, "--file") == 0) {
		sendto(sockfd, (const char*)msg_cmd, strlen(msg_cmd), 0, (const struct sockaddr*)&server_addr, sizeof(server_addr));
		printf("Please enter the path file: ");
		fgets(file_name, sizeof(file_name), stdin);
		str_trim_lf(file_name, sizeof(file_name));
		fp = fopen(file_name, "r");
		if(fp == NULL) {
			printf("ERROR: Error in reading file.");
			return;
		}
		send_file(fp,sockfd, file_name);
		fclose(fp);
	}
}

void send_msg_handler() {
	char message[LENGTH] = {};
	char buffer[LENGTH + 34] = {};

	while (1) {
		printf("%s", "> ");
		fgets(message, LENGTH, stdin);
		str_trim_lf(message, LENGTH);

		if (strcmp(message, "exit") == 0) {
			break;
		}
		else if (strcmp(message, "--menu") == 0 || strcmp(message, "-m") == 0) {
			show_menu();
		}
		else {
			sprintf(buffer, "%s: %s\n", name, message);
			sendto(sockfd, (const char*)buffer, strlen(buffer), 0, (const struct sockaddr*)&server_addr, sizeof(server_addr));
		}

		bzero(message, LENGTH);
		bzero(buffer, LENGTH + 32);
	}
}

void recv_msg_handler() {
	char message[LENGTH] = {};
	int len = sizeof(client_addr);
	while (1) {
		int receive = recvfrom(sockfd, (char*)message, LENGTH, 0, (struct sockaddr*)&client_addr, (socklen_t *)&len);
		if (receive > 0) {
			printf("%s", message);
			printf("%s", "> ");
		}
		memset(message, 0, sizeof(message));
	}
}

int main(int argc, char *argv[]) {

	char *ip = IP;
	int port = PORT;

	printf("Please enter your name: ");
	fgets(name, 32, stdin);
	str_trim_lf(name, strlen(name));

	if (strlen(name) > 32 || strlen(name) < 2) {
		printf("Name is invalid.\n");
		return EXIT_FAILURE;
	}

	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(ip);
	server_addr.sin_port = htons(port);

	printf("=== WELCOME TO THE CHATROOM ===\n");

	pthread_t send_msg_thread;
	if (pthread_create(&send_msg_thread, NULL, (void *)send_msg_handler, NULL) != 0) {
		printf("ERROR: pthread\n");
		return EXIT_FAILURE;
	}

	pthread_t recv_msg_thread;
	if (pthread_create(&recv_msg_thread, NULL, (void *)recv_msg_handler, NULL) != 0) {
		printf("ERROR: pthread\n");
		return EXIT_FAILURE;
	}

	while (1) {}

	close(sockfd);

	return EXIT_SUCCESS;
}

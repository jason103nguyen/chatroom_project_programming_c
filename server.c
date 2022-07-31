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
#include <sys/ioctl.h>
#include "helper.h"

#define MAX_CLIENTS 5
#define BUFFER_SZ 2048
#define MAX_LEN_RECV 2048
#define MAX_LEN_SENDING 2048
#define MAX_LEN_USERNAME 32
#define MAX_LEN_CMD 32
#define OFFSET 32

#define TRUE 1
#define FALSE 0

#define IP "127.0.0.1"
#define PORT 8080

#define PROTOCOL_TCP 1
#define PROTOCOL_UDP 2

typedef enum {
	INVALID_TRANS_TYPE,
	BROADCAST,
	UNICAST
}trans_type_enum;

typedef enum {
	INVALID_MSG,
	TEXT_MSG,
	FILE_MSG
}msg_type_enum;

char buff_out[BUFFER_SZ];
char name[32];

int rc, on = 1;
int listen_sd, max_sd = 1, new_cli_sd;
int desc_ready;
char buffer[80];
struct sockaddr_in6 addr;
fd_set master_set, working_set;
int close_conn;

int maxfd;
int nready;
int cli_count = 0;
int uid = 0;
struct sockaddr_in serv_addr;
struct sockaddr_in cli_addr;

char *ip = IP;
int option = 1;
int listen_sd = 0, udp_fd = 0, new_cli_sd = 0;
pthread_t tid;

typedef struct {
	bool isConnected;
	struct sockaddr_in address;
	int sockfd;
	int uid;
	char username[MAX_LEN_USERNAME];
	int protocol;
} client_t;

client_t *client_list[MAX_CLIENTS];
client_t cli_list[MAX_CLIENTS];

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

void write_file (int sockfd) {
	char namefile[1028] = {0};
	struct timeval tv;
	tv.tv_sec = 5;
    tv.tv_usec = 0;

	fd_set master_set_recv_file, working_set_recv_file;
	char filename[1028] = "./server_file/";

	int size = 1028;
    char buffer[size];

	recv(sockfd, namefile, sizeof(namefile), 0);
	strcat(filename, namefile);
	FILE * fp = fopen(filename, "w");

    if (fp == NULL) {
        printf("ERROR: Error in creating file.");
        exit(1);
    }

	FD_ZERO(&master_set_recv_file);
	FD_SET(sockfd, &master_set_recv_file);

	while (1) {
		memcpy(&working_set_recv_file, &master_set_recv_file, sizeof(master_set_recv_file));
		rc = select(sockfd + 1, &working_set_recv_file, NULL, NULL, &tv);
		if (FD_ISSET(sockfd, &working_set_recv_file)) {
			recv(sockfd, buffer, size, 0);
			fprintf(fp, "%s", buffer);
			bzero(buffer, sizeof(buffer));
		}
		if (rc <= 0) {
			break;
		}
	}

	fclose(fp);
    return;
}

void write_file_udp (int sockfd) {
	char namefile[1028] = {0};
	struct timeval tv;
	tv.tv_sec = 5;
    tv.tv_usec = 0;

	fd_set master_set_recv_file, working_set_recv_file;
	char filename[1028] = "./server_file/";

	int size = 1028;
    char buffer[size];

	int len = sizeof(cli_addr);
	recvfrom(sockfd, (char*)namefile, sizeof(namefile), 0, (struct sockaddr*)&cli_addr, (socklen_t *)&len);
	strcat(filename, namefile);
	FILE * fp = fopen(filename, "w");

    if (fp == NULL) {
        printf("ERROR: Error in creating file.");
        exit(1);
    }

	FD_ZERO(&master_set_recv_file);
	FD_SET(sockfd, &master_set_recv_file);

	while (1) {
		memcpy(&working_set_recv_file, &master_set_recv_file, sizeof(master_set_recv_file));
		rc = select(sockfd + 1, &working_set_recv_file, NULL, NULL, &tv);
		if (FD_ISSET(sockfd, &working_set_recv_file)) {
			recvfrom(sockfd, (char*)buffer, sizeof(buffer), 0, (struct sockaddr*)&cli_addr, (socklen_t *)&len);
			fprintf(fp, "%s", buffer);
			bzero(buffer, sizeof(buffer));
		}
		if (rc <= 0) {
			break;
		}
	}

	fclose(fp);
    return;
}

void queue_add(client_t *cl) {
	for (int i = 0; i < MAX_CLIENTS; ++i) {
		if (!client_list[i]) {
			client_list[i] = cl;
			break;
		}
	}
}

void queue_remove(int uid) {
	for (int i = 0; i < MAX_CLIENTS; ++i) {
		if (client_list[i]) {
			if (client_list[i]->uid == uid) {
				client_list[i] = NULL;
				break;
			}
		}
	}
}

void send_message(char * msg, int uid) {
	for (int i = 0; i < MAX_CLIENTS; ++i) {

		if (client_list[i] == NULL) {
			continue;
		}

		if (client_list[i]->uid != uid && client_list[i]->protocol == PROTOCOL_TCP) {
			if (send(client_list[i]->sockfd, msg, strlen(msg), 0) < 0) {
				printf("ERROR: send failed");
				break;
			}
		}

		if (client_list[i]->uid != uid && client_list[i]->protocol == PROTOCOL_UDP) {
			struct sockaddr_in client_addr = client_list[i]->address;
			int sendStatus = sendto(udp_fd, (const char*)msg, strlen(msg), 0, (const struct sockaddr*)&client_addr, sizeof(client_addr));
			if (sendStatus < 0) {
				printf("ERROR: send failed");
				break;
			}
		}
		
	}
}

int getUidFromFd(int fd) {
	int uid = 0;
	for (int i = 0; i < MAX_CLIENTS; i++) {
		if (client_list[i] == NULL) {
			continue;
		}

		if (client_list[i]->sockfd == fd) {
			uid = client_list[i]->uid;
			break;
		}
	}
	return uid;
}

void getNameFromUid(int fd, char * name) {
	for (int i = 0; i < MAX_CLIENTS; i++) {
		if (client_list[i] == NULL) {
			continue;
		}

		if (client_list[i]->sockfd == fd) {
			strcpy(name, client_list[i]->username);
			return;
		}
	}
}

void setMaxSd(int fd) {
	if (fd > max_sd) {
		max_sd = fd;
	}
}

client_t * getClientByPort(int port) {
	for (int i = 0; i < MAX_CLIENTS; i++) {
		if (client_list[i] == NULL) {
			continue;
		}

		if (client_list[i]->address.sin_port == port) {
			return client_list[i];
		}
	}
	return NULL;
}

void recv_msg_client_udp(int sockfd) {

	
	int len = sizeof(cli_addr);
	int receive = recvfrom(sockfd, (char*)buff_out, BUFFER_SZ, 0, (struct sockaddr*)&cli_addr, (socklen_t *)&len);

	client_t * cli = getClientByPort(cli_addr.sin_port);
	if (cli == NULL) {
		cli = (client_t *)malloc(sizeof(client_t));
		cli->address = cli_addr;
		cli->uid = uid++;
		cli->protocol = PROTOCOL_UDP;
		queue_add(cli);
	}

	if (receive <= 0) {
		printf("ERROR: Reveive failed\n");
	}
	else if (strcmp(buff_out, "--file") == 0 || strcmp(buff_out, "-f") == 0) {
		write_file_udp(sockfd);
	}
	else {
		printf("%s", buff_out);
		send_message(buff_out, cli->uid);
	}

	bzero(buff_out, BUFFER_SZ);
}

int add_new_client_tcp(struct sockaddr_in cli_addr, int sockfd_client, int uid, int protocol, char * username) {

	for (int i = 0 ;i < MAX_CLIENTS; i++) {
		if (!cli_list[i].isConnected) {
			cli_list[i].address = cli_addr;
			cli_list[i].isConnected = true;
			cli_list[i].protocol = protocol;
			cli_list[i].sockfd = sockfd_client;
			cli_list[i].uid = uid;
			strcpy(cli_list[i].username, username);
			return 1;
		}
	}

	return -1;
}

void get_username_sender(char * message, char * username_sender) {

	char username[MAX_LEN_USERNAME] = {0};
	char * pointer_name = strstr(message, "username_sender=");
	pointer_name += 16;
	for(int i = 0; i < strlen(message); i++) {
		if (*(pointer_name + i) == '&' || *(pointer_name + i) == '#') {
			break;
		}
		username[i] = *(pointer_name + i);
	}
	strcpy(username_sender, username);
}

msg_type_enum get_msg_type(char * message) {

	char msg_type[MAX_LEN_CMD] = {0};
	char * pointer_msg_type = strstr(message, "msg_type=");
	pointer_msg_type += 9;
	for(int i = 0; i < strlen(message); i++) {
		if (*(pointer_msg_type + i) == '&' || *(pointer_msg_type + i) == '#') {
			break;
		}
		msg_type[i] = *(pointer_msg_type + i);
	}
	
	if (string_compare(msg_type, "text_msg")) {
		return TEXT_MSG;
	} 
	else if (string_compare(msg_type, "file_msg")) {
		return FILE_MSG;
	}
	else {
		return INVALID_MSG;
	}
	
}

int get_sockfd_from_username(char * username) {

	for (int i = 0; i < MAX_CLIENTS; i++) {
		if (string_compare(cli_list[i].username, username)) {
			return cli_list[i].sockfd;
		}
	}
	return -1;
}

void send_to_receiver(char * username_receiver, char * message) {

	int sockfd_receiver = get_sockfd_from_username(username_receiver);

	if (sockfd_receiver < 0) {
		printf("ERROR: Username not exist\n");
		return;
	}

	if (send(sockfd_receiver, message, strlen(message), 0) < 0) {
		printf("ERROR: Send to receiver error\n");
	}
}

void get_username_receiver(char * message, char * receiver) {
	
	char username_receiver[MAX_LEN_USERNAME] = {0};
	char * pointer_username_receiver = strstr(message, "username_receiver=");
	pointer_username_receiver += 18;
	for(int i = 0; i < strlen(message); i++) {
		if (*(pointer_username_receiver + i) == '&' || *(pointer_username_receiver + i) == '#') {
			break;
		}
		username_receiver[i] = *(pointer_username_receiver + i);
	}
	
	strcpy(receiver, username_receiver);
}

void close_connect_client(int sockfd) {
	int uid_client_send = 0;
	uid_client_send = getUidFromFd(sockfd);
	char name_client_send[32] = {0};
	getNameFromUid(sockfd, name_client_send);
	sprintf(buffer, "%s has left\n", name_client_send);
	printf("%s", buffer);
	uid_client_send = getUidFromFd(sockfd);
	send_message(buffer, uid_client_send);
	close_conn = TRUE;
}

int verify_username_receiver(char * username_receiver) {

	for (int i = 0; i < MAX_CLIENTS; i++) {
		if (string_compare(cli_list[i].username, username_receiver)) {
			return 1;
		}
	}
	return -1;
}

void recv_msg_unicast(char * message, int sockfd) {

	msg_type_enum type_msg = get_msg_type(message);
	char buffer[MAX_LEN_RECV] = {0};

	if (type_msg == TEXT_MSG) {
		char username_receiver[MAX_LEN_USERNAME] = {0};
		get_username_receiver(message, username_receiver);
		
		if (verify_username_receiver(username_receiver) < 0) {
			printf("ERROR: Verify username receiver failed!");
			return;
		}

		int receive = recv(sockfd, buffer, sizeof(buffer), 0);

		if (receive < 0) {
			printf("ERROR: Receive failed\n");
			return;
		}
		else if (receive == 0) {
			close_connect_client(sockfd);
		}

		send_to_receiver(username_receiver, buffer);
	}
	else if (type_msg == FILE_MSG) {
		write_file(sockfd);
	}
	else {
		//Type msg invalid
	}

}

void recv_msg_broadcast(int sockfd, char * buffer) {
	int uid_client_send = 0;
	uid_client_send = getUidFromFd(sockfd);
	send_message(buffer, uid_client_send);
	str_trim_lf(buffer, strlen(buffer));
	printf("%s\n", buffer);
}

trans_type_enum get_trans_type(char * message) {

	char trans_type[MAX_LEN_CMD] = {0};
	char * pointer_trans_type = strstr(message, "trans_type=");
	pointer_trans_type += 11;
	for(int i = 0; i < strlen(message); i++) {
		if (*(pointer_trans_type + i) == '&' || *(pointer_trans_type + i) == '#') {
			break;
		}
		trans_type[i] = *(pointer_trans_type + i);
	}
	
	if (string_compare(trans_type, "broadcast")) {
		return BROADCAST;
	} 
	else if (string_compare(trans_type, "unicast")) {
		return UNICAST;
	}
	else {
		return INVALID_TRANS_TYPE;
	}
	
}

void send_to_all_client(char * msg, int uid) {
	for (int i = 0; i < MAX_CLIENTS; ++i) {

		if (!cli_list[i].isConnected) {
			continue;
		}

		if (cli_list[i].uid != uid && cli_list[i].protocol == PROTOCOL_TCP) {
			if (send(cli_list[i].sockfd, msg, strlen(msg), 0) < 0) {
				printf("ERROR: Send failed");
				break;
			}
		}

		if (cli_list[i].uid != uid && cli_list[i].protocol == PROTOCOL_UDP) {
			struct sockaddr_in client_addr = cli_list[i].address;
			int sendStatus = sendto(udp_fd, (const char*)msg, strlen(msg), 0, (const struct sockaddr*)&client_addr, sizeof(client_addr));
			if (sendStatus < 0) {
				printf("ERROR: Send failed");
				break;
			}
		}
		
	}
}

int set_username_sender(int sockfd, char * username) {

	char buff_out[MAX_LEN_SENDING + MAX_LEN_USERNAME + OFFSET] = {0};
	if (sockfd < 0) {
		return 1;
	}

	for (int i = 0; i < MAX_CLIENTS; i++) {
		if (cli_list[i].sockfd == sockfd) {
			//Set username
			strcpy(cli_list[i].username, username);

			//Inform for all client
			sprintf(buff_out, "%s has joined\n", username);
			printf("%s", buff_out);
			send_to_all_client(buff_out, cli_list[i].uid);
			return 1;
		}
	}

	return -1;
}

void recv_msg_client_tcp(int sockfd) {
	char buffer[MAX_LEN_RECV] = {0};

	int receive = recv(sockfd, buffer, sizeof(buffer), 0);

	if (receive < 0) {
		printf("ERROR: Receive failed\n");
		return;
	}
	else if (receive == 0) {
		close_connect_client(sockfd);
	}

	trans_type_enum trans_type = get_trans_type(buffer);
	if (trans_type == BROADCAST) {
		recv_msg_broadcast(sockfd, buffer);
	}
	else if (trans_type == UNICAST) {
		recv_msg_unicast(buffer, sockfd);
	}
	else {
		//Invalid message
	}

	bzero(buffer, sizeof(buffer));
}

void test_sprintf(char * name) {
	char buffer[32] = {0};
	sprintf(buffer, "Hello: %s\n", name);
	printf("%s", buffer);
}

void accept_new_connect_tcp(int sockfd) {

	struct sockaddr_in cli_addr;
	socklen_t clilen = sizeof(cli_addr);
	new_cli_sd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
	FD_SET(new_cli_sd, &master_set);
	setMaxSd(new_cli_sd);

	char buffer[MAX_LEN_RECV] = {0};

	int receive = recv(new_cli_sd, buffer, sizeof(buffer), 0);

	if (receive < 0) {
		printf("ERROR: Receive failed\n");
		return;
	}
	else if (receive == 0) {
		close_connect_client(new_cli_sd);
	}

	char username_sender[MAX_LEN_USERNAME] = {0};
	get_username_sender(buffer, username_sender);

	uid++;
	if (add_new_client_tcp(cli_addr, new_cli_sd, uid, PROTOCOL_TCP, username_sender) < 0) {
		printf("ERROR: Max client");
		return;
	}

	char msg_inform[128] = {0};
	sprintf(msg_inform, "%s has joined", username_sender);
	send_to_all_client(msg_inform, uid);

	printf("%s\n", msg_inform);
	
	// queue_add(cli);

	// if (recv(cli->sockfd, name, 32, 0) > 0) {
	// 	strcpy(cli->username, name);
	// 	sprintf(buff_out, "%s has joined\n", cli->username);
	// 	printf("%s", buff_out);
	// 	send_message(buff_out, cli->uid);
	// }
	// else {
	// 	printf("ERROR: Reveive failed\n");
	// }

	bzero(buff_out, BUFFER_SZ);
}

int verify_username_sender(char * username_sender) {

	for(int i = 0; i < MAX_CLIENTS; i++) {
		if (string_compare(cli_list[i].username, username_sender)) {
			return 1;
		}
	}

	return -1;
}

int main(int argc, char *argv[]) {

	listen_sd = socket(AF_INET, SOCK_STREAM, 0);
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr(IP);
	serv_addr.sin_port = htons(PORT);

	if (setsockopt(listen_sd, SOL_SOCKET, (SO_REUSEPORT | SO_REUSEADDR), (int *)&option, sizeof(option)) < 0) {
		printf("ERROR: setsockopt failed");
		return EXIT_FAILURE;
	}

	if (bind(listen_sd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
		printf("ERROR: Socket binding failed");
		return EXIT_FAILURE;
	}

	if (listen(listen_sd, 10) < 0) {
		printf("ERROR: Socket listening failed");
		return EXIT_FAILURE;
	}

	//Setup for UDP
	udp_fd = socket(AF_INET, SOCK_DGRAM, 0);

	if (bind(udp_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
		printf("ERROR: Socket binding failed");
		return EXIT_FAILURE;
	}

	FD_ZERO(&master_set);
	setMaxSd(listen_sd);
	setMaxSd(udp_fd);
	FD_SET(listen_sd, &master_set);
	FD_SET(udp_fd, &master_set);

	printf("=== WELCOME TO THE CHATROOM ===\n");
	while (1) {
		memcpy(&working_set, &master_set, sizeof(master_set));
		rc = select(max_sd + 1, &working_set, NULL, NULL, NULL);
		desc_ready = rc;

		for (int fd = 0; fd <= max_sd && desc_ready > 0; ++fd) {
			if (FD_ISSET(fd, &working_set)) {
				desc_ready -= 1;
				if (fd == listen_sd) {
					accept_new_connect_tcp(fd);
				} 
				else if (fd == udp_fd) {
					recv_msg_client_udp(fd);
				}
				else {
					recv_msg_client_tcp(fd);
				}

				if (close_conn) {
					queue_remove(getUidFromFd(fd));
					close(fd);
					FD_CLR(fd, &master_set);
					if (fd == max_sd) {
						while (FD_ISSET(max_sd, &master_set) == FALSE) {
							max_sd -= 1;
						}
					}
				}
			}
		}
	}

	return EXIT_SUCCESS;
}
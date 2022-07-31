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
#include <stdbool.h>

#define DEBUG

void str_trim_lf(char *arr, int length);
bool string_compare(const char * s1, const char * s2);
void printf_log(const char *fmt, ...);
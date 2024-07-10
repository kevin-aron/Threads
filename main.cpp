#include <cstdio>
#include <unistd.h>
#include <sys/types.h>
#include <ctype.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/file.h>
#include <iostream>
#include <semaphore.h>
#include <pthread.h>
#include <sys/socket.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h> 
#include <sys/sem.h>
#include <sys/msg.h>

int clnt_socks[100] = { 0 };
int clnt_cnt = 0;
char name[100] = "";
sem_t semid;
pthread_mutex_t mutex;
void send_msg(const char* msg, ssize_t str_len)
{
	pthread_mutex_lock(&mutex);
	for (int i = 0; i < clnt_cnt; i++)
	{
		if (clnt_socks[i] >= 0)
		{
			write(clnt_socks[i], msg, str_len);
		}
	}
	pthread_mutex_unlock(&mutex);
}
void* handle_clnt(void* arg)
{
	pthread_detach(pthread_self());
	int clnt_sock = *(int*)arg;
	char msg[1024] = "";
	ssize_t str_len = 0;
	while ((str_len = read(clnt_sock, msg, sizeof(msg))))
	{
		send_msg(msg, str_len);
	}
	pthread_mutex_lock(&mutex);
	for (int i = 0; i < clnt_cnt; i++)
	{
		if (clnt_sock == clnt_socks[i])
		{
			clnt_socks[i] = -1;
			break;
		}
	}
	pthread_mutex_unlock(&mutex);
	close(clnt_sock);
	pthread_exit(NULL);
}

void server_threads()
{
	int serv_sock, clnt_sock;
	struct sockaddr_in serv_adr, clnt_adr;
	socklen_t clnt_adr_sz = sizeof(clnt_adr);
	serv_sock = socket(PF_INET, SOCK_STREAM, 0);
	memset(&serv_adr, 0, sizeof(serv_adr));
	serv_adr.sin_family = AF_INET;
	serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_adr.sin_port = htons(9527);
	pthread_mutex_init(&mutex, NULL);
	if (bind(serv_sock, (struct sockaddr*)&serv_adr, sizeof(serv_adr)) == -1)
	{
		printf("bind() error\n");
		return;
	}
	if (listen(serv_sock, 5) == -1)
	{
		printf("listen() error\n");
		return;
	}
	while (true)
	{
		clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_adr, &clnt_adr_sz);
		if (clnt_sock == -1) break;
		pthread_mutex_lock(&mutex);
		clnt_socks[clnt_cnt++] = clnt_sock;
		pthread_mutex_unlock(&mutex);
		pthread_t thread;
		pthread_create(&thread, NULL, handle_clnt, &clnt_socks[clnt_cnt - 1]);
	}
	close(serv_sock);
	pthread_mutex_destroy(&mutex);
}
void* client_send_msg(void* arg)
{
	pthread_detach(pthread_self());
	int clnt_sock = *(int*)arg;
	char msg[256] = "";
	char buffer[512] = "";
	while (true)
	{
		memset(buffer, 0, sizeof(buffer));
		fgets(msg, sizeof(msg), stdin);
		if ((strcmp(msg, "q\n") == 0)) break;
		snprintf(buffer, sizeof(buffer), "[%s]: %s", name, msg);
		ssize_t bytes_written = write(clnt_sock, buffer, strlen(buffer));
	}
	sem_post(&semid);
	pthread_exit(NULL);
}
void* client_recv_msg(void* arg)
{
	pthread_detach(pthread_self());
	int clnt_sock = *(int*)arg;
	char buffer[512] = "";
	while (true)
	{
		memset(buffer, 0, sizeof(buffer));
		ssize_t str_len = read(clnt_sock, buffer, sizeof(buffer));
		if (str_len <= 0) break;
		fputs(buffer, stdout);
	}
	sem_post(&semid);
	pthread_exit(NULL);
}
void client_threads()
{
	int clnt_sock = socket(PF_INET, SOCK_STREAM, 0);
	struct sockaddr_in serv_adr;
	memset(&serv_adr, 0, sizeof(serv_adr));
	serv_adr.sin_family = AF_INET;
	serv_adr.sin_addr.s_addr = inet_addr("127.0.0.1");
	serv_adr.sin_port = htons(9527);
	fputs("input your name:", stdout);
	scanf("%s", name);
	getchar();
	if (connect(clnt_sock, (struct sockaddr*)&serv_adr, sizeof(serv_adr)) == -1)
	{
		printf("connect() error\n");
		return;
	}
	pthread_t thsend, threcv;
	sem_init(&semid, 0, -1);
	pthread_create(&thsend, NULL, client_send_msg, (void*)&clnt_sock);
	pthread_create(&threcv, NULL, client_recv_msg, (void*)&clnt_sock);
	sem_wait(&semid);
	close(clnt_sock);
}
void threads(const char* arg)
{
	if (strcmp(arg, "s") == 0) server_threads();
	else client_threads();
}


int main(int argc,char* argv[])
{
	threads(argv[1]);
    return 0;
}
/*----------------------
 ���ϸ� : chat_server2.c
 ��  �� : ä�ü��� username �� exit ó��, select �Լ� �̿�, (Windows �� Linux ����)
 ���� : chat_server [port]
------------------------*/
#ifdef WIN32
#include <winsock.h>
#include <signal.h>
#include <stdio.h>
#include <conio.h>
#include <stdlib.h>
#include <string.h>
#else
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#endif



#ifdef WIN32
WSADATA wsadata;
int	main_socket;

void exit_callback(int sig)
{
	closesocket(main_socket);
	WSACleanup();
	exit(0);
}

void init_winsock()
{
	WORD sversion;
	u_long iMode = 1;

	// winsock ����� ���� �ʼ�����
	signal(SIGINT, exit_callback);
	sversion = MAKEWORD(1, 1);
	WSAStartup(sversion, &wsadata);
}
#endif

#define MAXCLIENTS 64		// �ִ� ä�� ������ ��
#define EXIT	"exit"		// ä�� ���� ���ڿ�
int maxfdp;              	// select() ���� �����ؾ��� # of socket ���� getmax() return �� + 1
int getmax(int);			// �ִ� ���Ϲ�ȣ ���
int num_chat = 0;         	// ä�� ������ ��
int client_fds[MAXCLIENTS];	// ä�ÿ� ������ ���Ϲ�ȣ ���
void RemoveClient(int);		// ä�� Ż�� ó�� �Լ�

#define BUF_LEN	128
#define CHAT_SERVER "0.0.0.0"
#define CHAT_PORT "30000"

char username[BUF_LEN];

int main(int argc, char* argv[]) {
	char buf[BUF_LEN];
	int i, j, n, ret;
	int server_fd, client_fd, client_len;
	unsigned int set = 1;
	char* ip_addr = CHAT_SERVER, * port_no = CHAT_PORT;
	fd_set  read_fds;     // �б⸦ ������ ���Ϲ�ȣ ����ü server_fd �� client_fds[] �� �����Ѵ�.
	struct sockaddr_in  client_addr, server_addr;
	int client_error[MAXCLIENTS];

#ifdef WIN32
	printf("Windows : ");
	init_winsock();
#else
	printf("Linux : ");
#endif
	/* ���� ���� */
	if ((server_fd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		printf("Server: Can't open stream socket.");
		exit(0);
	}
#ifdef WIN32
	main_socket = server_fd;
#endif

	printf("chat_server2 waiting connection..\n");
	printf("server_fd = %d\n", server_fd);
	setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (char*)&set, sizeof(set));

	/* server_addr�� '\0'���� �ʱ�ȭ */
	memset((char*)&server_addr, 0, sizeof(server_addr));
	/* server_addr ���� */
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons(atoi(port_no));

	if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
		printf("Server: Can't bind local address.\n");
		exit(0);
	}
	/* Ŭ���̾�Ʈ�κ��� �����û�� ��ٸ� */
	listen(server_fd, 5);

	while (1) {
		FD_ZERO(&read_fds); // ���� �ʱ�ȭ
		FD_SET(server_fd, &read_fds); // accept() ��� ���� ����
		for (i = 0; i < num_chat; i++) // ä�ÿ� �������� ��� client ������ reac() ��� �߰�
			FD_SET(client_fds[i], &read_fds);
		maxfdp = getmax(server_fd) + 1;     // ���ô�� ������ ���� ���
		if (select(maxfdp, &read_fds, (fd_set*)0, (fd_set*)0, (struct timeval*)0) <= 0) {
			printf("select error <= 0 \n");
			exit(0);
		}
		// �ʱ� ���� ��, server_fd �� ��ȭ�� �ִ��� �˻�
		if (FD_ISSET(server_fd, &read_fds)) {
			// ��ȭ�� �ִ� --> client �� connect�� ���� ��û�� �� ��
			client_len = sizeof(client_addr);
			client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
			if (client_fd == -1) {
				printf("accept error\n");
			}
			else {
				printf("Client connected from %s:%d\n", inet_ntoa(client_addr.sin_addr),
					ntohs(client_addr.sin_port));
				printf("client_fd = %d\n", client_fd);
				/* ä�� Ŭ���̾�Ʈ ��Ͽ� �߰� */
				printf("client[%d] ����. ���� ������ �� = %d\n", num_chat, num_chat + 1);
				client_fds[num_chat++] = client_fd;
			}
		}

		memset(client_error, 0, sizeof(client_error));
		/* Ŭ���̾�Ʈ�� ���� �޽����� ��� Ŭ���̾�Ʈ���� ��� */
		for (i = 0; i < num_chat; i++) {
			// ������ client���� I/O ��ȭ�� �ִ���.
			if (FD_ISSET(client_fds[i], &read_fds)) {
				// Read One �Ǵ� client ������ ���� Ȯ��
				if ((n = recv(client_fds[i], buf, BUF_LEN, 0)) <= 0) {
					// client �� �� ���� ������ ���
					printf("recv error for client[%d]\n", i);
					client_error[i] = 1;
					continue;
				}
				printf("received %d from client[%d] : %s", n, i, buf);
				//sscanf(buf, "%s", username); //printf("username=%s\n", username);
				// ���Ṯ�� ó��
				// buf = "[hansung] Hello There" �� ��� username = [hansung]
				// "Hello There" �� buf + strlen(username) + 1 ���� ����
				if (strncmp(buf + strlen(username) + 1, EXIT, strlen(EXIT)) == 0) {
					RemoveClient(i);
					continue;
				}
				// ��� ä�� �����ڿ��� �޽��� ���
				//printf("%s", buf);
				// Wrie All
				for (j = 0; j < num_chat; j++) {
					ret = send(client_fds[j], buf, BUF_LEN, 0);
					if (ret <= 0) {
						printf("send error for client[%d]\n", j);
						client_error[i] = 1;
					}
				}
			}
		}
		// ������ �� Client���� �ڿ��� ������ ���鼭 �����Ѵ�.
		for (i = num_chat - 1; i >= 0; i--) {
			if (client_error[i])
				RemoveClient(i);
		}
	}
}

/* ä�� Ż�� ó�� */
void RemoveClient(int i) {
#ifdef WIN32
	closesocket(client_fds[i]);
#else
	close(client_fds[i]);
#endif
	// ������ client�� ������ �ڸ��� �̵� (��ĭ�� ���� �ʿ䰡 ����)
	if (i != num_chat - 1)
		client_fds[i] = client_fds[num_chat - 1];
	num_chat--;
	printf("client[%d] ����. ���� ������ �� = %d\n", i, num_chat);
}

// client_fds[] ���� �ִ� ���Ϲ�ȣ Ȯ��
// select(maxfds, ..) ���� maxfds = getmax(server_fd) + 1
int getmax(int k) {
	int max = k;
	int r;
	for (r = 0; r < num_chat; r++) {
		if (client_fds[r] > max) max = client_fds[r];
	}
	return max;
}
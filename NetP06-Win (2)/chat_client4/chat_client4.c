/*
���ϸ� : chat_client4.c
��  �� : ä�� Ŭ���̾�Ʈ, username ���, /login, /list, /exit /sleep /wakeup /to ó��, chat_clien3�� ����.
���� : chat_client4 [host] [port]
��Ʈ��ũ�� Ű���� ���� ó�� ���
Linux : select() ���
Windows : socket()�� Non-blocking mode �� kbhit()�� �̿��Ͽ� ���� ���� ���
*/
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

#define CHAT_SERVER "127.0.0.1"
#define CHAT_PORT "30000"
#define BUF_LEN 128

#define CHAT_CMD_LOGIN	"/login"		// connect�ϸ� user name ���� "/login atom"
#define CHAT_CMD_LIST	"/list"		// userlist ��û
#define CHAT_CMD_EXIT	"/exit"		// ����
#define CHAT_CMD_TO		"/to"		// �ӼӸ� "/to atom Hi there.."
#define CHAT_CMD_SLEEP	"/sleep"	// �����(������) ����
#define CHAT_CMD_WAKEUP	"/wakeup"	// wakeup �Ǵ� message �����ϸ� �ڵ� wakeup
#define CHAT_CMD_WITH	"/with"		// /with nickname , nickname�� 1:1 ä�� ��� ����
#define CHAT_CMD_WITHEND	"/end"	// 1:1 ä�� ����
#define CHAT_CMD_FILESEND	"/filesend"	// /filesend nickname data.txt ���� ����

char username[BUF_LEN]; // user name
void read_key_send(int s, char* buf, char* buf2); // key�Է��� ������ code (Linux/Windows����)

int main(int argc, char* argv[]) {
	char buf1[BUF_LEN + 1], buf2[BUF_LEN + 1], buf3[BUF_LEN + 1], buf4[BUF_LEN + 1];;
	int s, n, len_in, len_out;
	struct sockaddr_in server_addr;
	char* ip_addr = CHAT_SERVER, * port_no = CHAT_PORT;
	struct timeval tm;
	tm.tv_sec = 0;
	tm.tv_usec = 1000;

	if (argc == 3) {
		ip_addr = argv[1];
		port_no = argv[2];
	}
	printf("chat_client4 running.\n");
	printf("Enter user name : ");
	scanf("%s", username); getchar(); // \n����

#ifdef WIN32
	printf("Windows : ");
	init_winsock();
#else // Linux
	printf("Linux : ");
#endif 

	if ((s = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		printf("can't create socket\n");
		exit(0);
	}
#ifdef WIN32
	main_socket = s;
#endif 

	/* ä�� ������ �����ּ� ����ü server_addr �ʱ�ȭ */
	memset((char*)&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(ip_addr);
	server_addr.sin_port = htons(atoi(port_no));

	/* �����û */
	printf("Connecting %s %s\n", ip_addr, port_no);

	/* �����û */
	if (connect(s, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
		printf("Client : Can't connect to server.\n");
		exit(0);
	}
	else {
		printf("ä�� ������ ���ӵǾ����ϴ�. \n");
	}
	memset(buf1, 0, BUF_LEN);
	sprintf(buf1, "%s %s", CHAT_CMD_LOGIN, username);
	if (send(s, buf1, BUF_LEN, 0) < 0) {
		printf("username send error\n");
		exit(0);
	}
#ifdef WIN32
	u_long iMode = 1;
	ioctlsocket(s, FIONBIO, &iMode); // ������ non-blocking ���� �����.
	int maxfdp1;
	while (1) {
		char username[BUF_LEN], to[BUF_LEN], to_user[BUF_LEN], msg[BUF_LEN];
		char yes;

		// Non-blocking read�̹Ƿ� �����Ͱ� ������ ��ٸ��� �ʰ� 0���� return
		n = recv(s, buf2, BUF_LEN, 0);

		if (n > 0) { // non-blocking read
		// network���� �о
		// ȭ�鿡 ���

			sscanf(buf2, "%s %s %s %s", username, to, to_user, msg);
			//strcpy(msg, buf2 + strlen(username) + strlen(to) + strlen(to_user) + 3);

			if (strcmp(to, "/with") == 0) {

				printf("%s���� 1��1 ��ȭ�� ��û�߽��ϴ�. (y/n)? ", username);
				scanf("%c", &yes); getchar();
				//printf("%c",yes);


				memset(buf3, 0, BUF_LEN);
				if (yes == 'y') {
					printf("%s �԰� 1��1 ��ȭ�� �����մϴ�.\n", username);
					sprintf(buf3, "%s /withyes %s\n", username, to_user);

					if (send(s, buf3, BUF_LEN, 0) < 0) {
						printf("send error.\n");
						exit(0);
					}



				}
				else if (yes == 'n') {
					sprintf(buf3, "%s /withno %s\n", username, to_user);
					if (send(s, buf3, BUF_LEN, 0) < 0) {
						printf("send error.\n");
						exit(0);
					}
					continue;
				}

			}
			else if (strcmp(to, "/filesend") == 0) {

				printf("%s���� %s������ �������մϴ�. (y/n)? ", username, msg);
				scanf("%c", &yes); getchar();
				//printf("%c",yes);


				memset(buf3, 0, BUF_LEN);
				if (yes == 'y') {

					sprintf(buf3, "%s /fileyes %s %s\n", username, to_user, msg);

					if (send(s, buf3, BUF_LEN, 0) < 0) {
						printf("send error.\n");
						exit(0);
					}


					FILE* fp;
					int filesize, readsum = 0, nread = 0, n;
					memset(buf2, 0, BUF_LEN);
					printf("%s ���� ������ ������.\n", username);



					if (recv(s, buf2, BUF_LEN, 0) < 0) {//1
						printf("this is error  %s\n", buf2);
						//printf("buf4 recv error \n");
						continue;
					}

					memset(buf2, 0, BUF_LEN);
					if (recv(s, buf3, BUF_LEN, 0) <= 0) {
						printf("filesize recv error\n");
						exit(0);
					}//���ϻ����� �ޱ�

					//sprintf(filename2, �� % s % s��, user1, filename);
					if ((fp = fopen(msg, "wb")) == NULL) {
						printf("file open error\n");
						exit(0);
					}
					readsum = 0;
					memset(buf2, 0, BUF_LEN);
					while (readsum < filesize) {

						n = recv(s, buf2, nread, 0);//2
						if (n <= 0) {
							//printf("\n end of file\n");
							break;
						}//�������� ����
						printf("read data=%d bytes : %s\n", n, buf2);
						if (fwrite(buf2, n, 1, fp) <= 0) {
							printf("fwrite error\n");
							break;
						}
						readsum += n;
						if ((nread = (filesize - readsum)) > BUF_LEN)
							nread = BUF_LEN;
					}
					fclose(fp);
					memset(buf2, 0, BUF_LEN);
					recv(s, buf2, BUF_LEN, 0);//3

					if (strcmp(buf2, "\n finish\n") == 0) {
						printf("�������� �Ϸ�");
						continue;
					}

					continue;
				}



				else if (yes == 'n') {
					sprintf(buf3, "%s /fileno %s\n", username, to_user);
					if (send(s, buf3, BUF_LEN, 0) < 0) {
						printf("send error.\n");
						exit(0);
					}
					printf("�������� ����\n");
					continue;

				}
				continue;

			}
			else if (strcmp(to, "/fileyes") == 0) {
				printf("������ ���� ����\n");
				//printf(msg);

				FILE* fp;

				if ((fp = fopen(msg, "rb")) == NULL) {
					printf("Can't open file %s\n", msg);
					exit(0);
				}

				int filesize;
				int readsum = 0, nread = 0;
				fseek(fp, 0, 2);
				filesize = ftell(fp);
				rewind(fp);

				memset(buf2, 0, BUF_LEN);
				sprintf(buf2, "%s %d", msg, filesize);
				printf("%s\n", buf2);

				if (send(s, buf2, BUF_LEN, 0) <= 0) {//1
					printf("filename send error\n");
					exit(0);
				}
				memset(buf2, 0, BUF_LEN + 1);
				if (filesize < BUF_LEN)
					nread = filesize;
				else
					nread = BUF_LEN;
				while (readsum < filesize) {
					int n;
					memset(buf2, 0, BUF_LEN + 1);
					n = fread(buf2, 1, BUF_LEN, fp);
					if (n <= 0)
						break;
					printf("Sending %d bytes: %s\n", n, buf2);
					if (send(s, buf2, n, 0) <= 0) {//2
						printf("send error\n");
						break;
					}
					readsum += n;
					if ((nread = (filesize - readsum)) > BUF_LEN)
						nread = BUF_LEN;
				}
				fclose(fp);
				//printf("Receiving %s %d  bytes\n", msg, filesize);

				char s3[BUF_LEN] = "finish\n";

				if (send(s, s3, BUF_LEN, 0) <= 0) {//3
					printf("filename send error\n");
					exit(0);
				}
				if (strcmp(s3, "finish\n") == 0) {

					printf("File %s %d transffered\n", msg, filesize);
					continue;
				}
				//printf("\n filedata %s %d bytes received. \n", msg, filesize);


				continue;

			}
			else if (strcmp(to, "/fileno") == 0) {
				printf("���Ͽ�û ����\n");
				continue;
			}
			else if (strcmp(to_user, "/end") == 0) {

				printf("1��1 ��ȭ�� �����մϴ�.\n");
				memset(to_user, 0, BUF_LEN);
				continue;
			}
			else
				printf("%s", buf2);







		}

		else if (WSAGetLastError() != WSAEWOULDBLOCK) {
			printf("recv error\n"); // server �� ����Ǿ��ų� ��Ʈ��ũ ����
			break;
		}
		if (kbhit()) { // key �� ���������� read key --> write to chat server
			read_key_send(s, buf1, buf2);
		}
		Sleep(100); // Non-blocking I/O CPU �δ��� ���δ�.


	}
#else
	int maxfdp;
	fd_set read_fds;
	maxfdp = s + 1; // socket�� �׻� 0 ���� ũ�� �Ҵ�ȴ�.
	FD_ZERO(&read_fds);
	while (1) {
		FD_SET(0, &read_fds); // stdin:0 ǥ���Է��� file ��ȣ = 0 �̴�.
		FD_SET(s, &read_fds); // server �� �߰�� socket ��ȣ

		if (select(maxfdp, &read_fds, (fd_set*)0, (fd_set*)0, (struct timeval*)0) < 0) {
			printf("select error\n");
			exit(0);
		}
		// network I/O ��ȭ �ִ� ���
		if (FD_ISSET(s, &read_fds)) {
			if ((n = recv(s, buf2, BUF_LEN, 0)) > 0) {
				printf("%s", buf2);
			}
			else {
				printf("recv error\n");
				break;
			}
		}
		// keyboard �Է��� �ִ� ���
		if (FD_ISSET(0, &read_fds)) {
			read_key_send(s, buf1, buf2);
		}
	}
#endif
}


// Keyboard���� �о ������ �����ϴ� �Լ� (Linux/Windows ����)
void read_key_send(int s, char* buf1, char* buf2)
{
	printf("%s> ", username); // keyboard �Է��� ������ �տ� prompt�� ������ش�.
	if (fgets(buf1, BUF_LEN, stdin) > 0) {
		sprintf(buf2, "%s %s", username, buf1);
		if (send(s, buf2, BUF_LEN, 0) < 0) {
			printf("send error.\n");
			exit(0);
		}
		if (strcmp(buf1, "/end\n") == 0)
			printf("1:1 ��ȭ�� �����մϴ�\n");
		if (strncmp(buf1, CHAT_CMD_EXIT, strlen(CHAT_CMD_EXIT)) == 0) {
			printf("Good bye.\n");

#ifdef WIN32
			closesocket(s);
#else
			close(s);
#endif
			exit(0);
		}
	}
	else {
		printf("fgets error\n");
		exit(0);
	}
}
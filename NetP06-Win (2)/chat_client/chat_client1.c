/*
 파일명 : chat_client1.c
 기  능 : 채팅 클라이언트 echo_client1 와 거의 동일 
 컴파일 : cc -o chat_client1 chat_client1.c
 사용법 : chat_client1 [host] [port]
 네트워크와 키보드 동시 처리 방법
		Linux : select() 사용
		Windows : socket()을 Non-blocking mode 와 kbhit()을 이용하여 폴링 구조 사용
*/
#ifdef WIN32
#include <winsock.h>
#include <signal.h>
#include <stdio.h>
#include <conio.h>
#include <stdlib.h>
#include <errno.h>
#else
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
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

	// winsock 사용을 위해 필수적임
	signal(SIGINT, exit_callback);
	sversion = MAKEWORD(1, 1);
	WSAStartup(sversion, &wsadata);
}
#endif

#define CHAT_SERVER "127.0.0.1"
#define CHAT_PORT "30000"
#define BUF_LEN 128

#define EXIT	"exit"
char nickname[BUF_LEN]; 	// user name 

int main(int argc, char* argv[]) {
	char buf1[BUF_LEN + 1], buf2[BUF_LEN + 1];
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

	//printf("Enter name : ");
	//scanf("%s", username); getchar();
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

	/* 채팅 서버의 소켓주소 구조체 server_addr 초기화 */
	memset((char*)&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(ip_addr);
	server_addr.sin_port = htons(atoi(port_no));

	/* 연결요청 */
	printf("Connecting %s %s\n", ip_addr, port_no);

	/* 연결요청 */
	if (connect(s, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
		printf("Client : Can't connect to server.\n");
		exit(0);
	}
	else {
		printf("채팅 서버에 접속되었습니다. \n");
	}
#ifdef WIN32
	u_long iMode = 1;
	ioctlsocket(s, FIONBIO, &iMode); // 소켓을 non-blocking 으로 만든다.
	int maxfdp1;
	while (1) {
		// Non-blocking read이므로 데이터가 앖으면 기다리지 않고 0으로 return
		n = recv(s, buf2, BUF_LEN, 0);
		if (n > 0) { // non-blocking read
			// network에서 읽어서
			// 화면에 출력
			printf("%s", buf2);
		} else if (WSAGetLastError()!=WSAEWOULDBLOCK) {
			printf("recv error\n"); // server 가 종료되었거나 네트워크 오류
			break;
		}
		if (kbhit()) { // key 가 눌려있으면 read key --> write to chat server
			if (fgets(buf1, BUF_LEN, stdin)) { // Enter key 까지 입력 받고 전송
				//sprintf(buf2, "%s: %s\n", nickname, message);
				//if (send(s, buf2, BUF_LEN, 0) < 0) {
				if (send(s, buf1, BUF_LEN, 0) < 0) {
					printf("send error.\n");
					break;
				}
				if (strncmp(buf1, EXIT, strlen(EXIT))==0) {
					printf("Good bye.\n");
					break;
				}
			}
		}
		Sleep(100); // Non-blocking I/O CPU 부담을 줄인다.
	}
#else // Linux 는 keyboard(stdin:0) 에 select()를 사용할 수 있다.
	int maxfdp;
	fd_set read_fds;
	maxfdp = s + 1; // socket은 항상 0 보다 크게 할당된다.
	FD_ZERO(&read_fds);
	while (1) {
		FD_SET(0, &read_fds); // stdin:0 표준입력은 file 번호 = 0 이다.
		FD_SET(s, &read_fds); // server 와 견결된 socket 번호

		if (select(maxfdp, &read_fds, (fd_set*)0, (fd_set*)0, (struct timeval*)0) < 0) {
			printf("select error\n");
			exit(0);
		}
		// network I/O 변화 있는 경우
		if (FD_ISSET(s, &read_fds)) {
			if ((n = recv(s, buf2, BUF_LEN, 0)) > 0) {
				printf("%s", buf2);
			}
			else {
				printf("recv error\n");
				break;
			}
		}
		// keyboard 입력이 있는 경우
		if (FD_ISSET(0, &read_fds)) {
			if (fgets(buf1, BUF_LEN, stdin)) {
				//sprintf(buf2, "%s: %s", nickname, buf1);
				if (send(s, buf1, BUF_LEN, 0) < 0) {
					printf("send error\n");
					break;
				}
				if (strncmp(buf1, EXIT, strlen(EXIT))==0) {
					printf("Good bye.\n");
					close(s);
					break;
				}
			}
		}
	}
#endif
}

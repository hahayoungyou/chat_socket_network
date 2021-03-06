/*----------------------
 파일명 : chat_server4.c
 기  능 : 채팅서버, chat_server3 + /to, /sleep, /wakeup, /with, /withend, /sendfile, .. 등등 추가 구현
 사용법 : chat_server4 [port]
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

	// winsock 사용을 위해 필수적임
	signal(SIGINT, exit_callback);
	sversion = MAKEWORD(1, 1);
	WSAStartup(sversion, &wsadata);
}
#endif

#define MAXCLIENTS 64		// 최대 채팅 참가자 수
#define EXIT	"exit"		// 채팅 종료 문자열
int maxfdp;              	// select() 에서 감시해야할 # of socket 변수 getmax() return 값 + 1
int getmax(int);			// 최대 소켓번호 계산
int num_chat = 0;         	// 채팅 참가자 수
int client_fds[MAXCLIENTS];	// 채팅에 참가자 소켓번호 목록
void RemoveClient(int);		// 채팅 탈퇴 처리 함수

#define BUF_LEN	128
#define CHAT_SERVER "0.0.0.0"
#define CHAT_PORT "30000"
char userlist[MAXCLIENTS][BUF_LEN]; // user name 보관용
int usersleep[MAXCLIENTS]; // sleep 상태인지 상태 보관

#define CHAT_CMD_LOGIN	"/login"		// connect하면 user name 전송 "/login atom"
#define CHAT_CMD_LIST	"/list"		// userlist 요청
#define CHAT_CMD_EXIT	"/exit"		// 종료
#define CHAT_CMD_TO		"/to"		// 귓속말 "/to atom Hi there.."
#define CHAT_CMD_SLEEP	"/sleep"	// 대기모드(부재중) 설정
#define CHAT_CMD_WAKEUP	"/wakeup"	// wakeup 또는 message 전송하면 자동 wakeup
#define CHAT_CMD_WITH	"/with"		// /with nickname , nickname과 1:1 채팅 모드 시작
#define CHAT_CMD_WITHEND	"/end"	// 1:1 채팅 종료
#define CHAT_CMD_FILESEND	"/filesend"	// /filesend nickname data.txt 파일 전송

int main(int argc, char* argv[]) {
	char buf[BUF_LEN], buf1[BUF_LEN], buf2[BUF_LEN], buf3[BUF_LEN];
	int i, j, n, ret, k;
	int server_fd, client_fd, client_len;
	unsigned int set = 1;
	char* ip_addr = CHAT_SERVER, * port_no = CHAT_PORT;
	fd_set  read_fds;     // 읽기를 감지할 소켓번호 구조체 server_fd 와 client_fds[] 를 설정한다.
	struct sockaddr_in  client_addr, server_addr;
	int client_error[MAXCLIENTS];

#ifdef WIN32
	printf("Windows : ");
	init_winsock();
#else
	printf("Linux : ");
#endif
	/* 소켓 생성 */
	if ((server_fd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		printf("Server: Can't open stream socket.");
		exit(0);
	}
#ifdef WIN32
	main_socket = server_fd;
#endif

	printf("chat_server4 waiting connection..\n");
	printf("server_fd = %d\n", server_fd);
	setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (char*)&set, sizeof(set));

	/* server_addr을 '\0'으로 초기화 */
	memset((char*)&server_addr, 0, sizeof(server_addr));
	/* server_addr 세팅 */
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons(atoi(port_no));

	if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
		printf("Server: Can't bind local address.\n");
		exit(0);
	}
	/* 클라이언트로부터 연결요청을 기다림 */
	listen(server_fd, 5);

	while (1) {
		FD_ZERO(&read_fds); // 변수 초기화
		FD_SET(server_fd, &read_fds); // accept() 대상 소켓 설정
		for (i = 0; i < num_chat; i++) // 채팅에 참가중이 모든 client 소켓을 reac() 대상 추가
			FD_SET(client_fds[i], &read_fds);
		maxfdp = getmax(server_fd) + 1;     // 감시대상 소켓의 수를 계산
		if (select(maxfdp, &read_fds, (fd_set*)0, (fd_set*)0, (struct timeval*)0) <= 0) {
			printf("select error <= 0 \n");
			exit(0);
		}
		// 초기 소켓 즉, server_fd 에 변화가 있는지 검사
		if (FD_ISSET(server_fd, &read_fds)) {
			// 변화가 있다 --> client 가 connect로 연결 요청을 한 것
			client_len = sizeof(client_addr);
			client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
			if (client_fd == -1) {
				printf("accept error\n");
			}
			else {
				printf("Client connected from %s:%d\n", inet_ntoa(client_addr.sin_addr),
					ntohs(client_addr.sin_port));
				printf("client_fd = %d\n", client_fd);
				/* 채팅 클라이언트 목록에 추가 */
				printf("client[%d] 입장. 현재 참가자 수 = %d\n", num_chat, num_chat + 1);
				client_fds[num_chat++] = client_fd;
			}
		}

		memset(client_error, 0, sizeof(client_error));
		/* 클라이언트가 보낸 메시지를 모든 클라이언트에게 방송 */
		for (i = 0; i < num_chat; i++) {
			// 각각의 client들의 I/O 변화가 있는지.
			if (FD_ISSET(client_fds[i], &read_fds)) {
				// Read One 또는 client 비정상 종료 확인
				if ((n = recv(client_fds[i], buf, BUF_LEN, 0)) <= 0) {
					// client 가 비 정상 종료한 경우
					printf("recv error for client[%d]\n", i);
					client_error[i] = 1;
					continue;
				}
				printf("received %d from client[%d] : %s", n, i, buf);
				// "/login username" --> buf1 = /login, buf2 = username
				// "/list" --> buf1 = /list
				// "[username] message .." buf1 = [username], buf2 = message ...
				sscanf(buf, "%s", buf1); // 처음 문자열 분리 [username] 또는 /login
				n = strlen(buf1); // "/login username" or "[username] Hello" 에서 /login 나 [username] 만 분리
				// /login 처리
				if (strncmp(buf1, CHAT_CMD_LOGIN, strlen(CHAT_CMD_LOGIN)) == 0) { // "/login"
					strcpy(userlist[i], buf + n + 1, BUF_LEN - (n + 1)); // username 보관
					printf(" userlist[%d] = %s\n", i, userlist[i]);
					for (j = 0; j < num_chat; j++) {
						if (j != i) { // 본인 제외 다른 사용자에게 입장을 알린다.
							sprintf(buf, "[%s]님이 입장하였습니다.\n", userlist[i]);
							ret = send(client_fds[j], buf, BUF_LEN, 0);
							if (ret <= 0) {
								printf("send error for client[%d]\n", j);
								client_error[i] = 1;
							}
						}
					}
					continue;
				}
				// /list 처리
				if (strncmp(buf + n + 1, CHAT_CMD_LIST, strlen(CHAT_CMD_LIST)) == 0) { // "/list"
					printf("Sending user list to client[%d] %s\n", i, userlist[i]);
					sprintf(buf, "User List\nNo\tname\t\status\n---------------------\n");
					if (send(client_fds[i], buf, BUF_LEN, 0) < 0) {
						printf("client[%d] send error.", i);
						client_error[i] = 1;
						continue;
					}
					for (j = 0; j < num_chat; j++) {
						sprintf(buf, "%02d\t%s\t%s\n", j, userlist[j],
							usersleep[j] ? "S" : "O");
						if (send(client_fds[i], buf, BUF_LEN, 0) < 0) {
							printf("client[%d] send error.", i);
							client_error[i] = 1;
							break;
						}
					}
					sprintf(buf, "----------------------\n");
					if (send(client_fds[i], buf, BUF_LEN, 0) < 0) {
						printf("client[%d] send error.", i);
						client_error[i] = 1;
						continue;
					}
					continue;
				}
				// /to 처리 귓속말 처리
				// [atom] /to username2 message ...
				if (strncmp(buf + n + 1, CHAT_CMD_TO, strlen(CHAT_CMD_TO)) == 0) { // "/to"
					char username[BUF_LEN], to[BUF_LEN], to_user[BUF_LEN], msg[BUF_LEN];
					sscanf(buf, "%s %s %s", username, to, to_user);
					strcpy(msg, buf + strlen(username) + strlen(to) + strlen(to_user) + 3);
					printf("[귓속말] from %s to %s : %s", username, to_user, msg);
					//printf("%s\n", username);
					// 귓속말 전송
					for (j = 0; j < num_chat; j++) {
						// user가 sleep 이면 어떤 메시지도 수신하지 않는다.
						if (usersleep[j] != 1 && strcmp(userlist[j], to_user) == 0) {
							sprintf(buf2, "[귓속말] %s %s", username, msg);
							if (send(client_fds[j], buf2, BUF_LEN, 0) < 0) {
								printf("client[%d] send error.", j);
								client_error[j] = 1;
								break;
							}
						}
					}
					continue;
				}
				if (strncmp(buf + n + 1, CHAT_CMD_FILESEND, strlen(CHAT_CMD_FILESEND)) == 0) { // "/filesend"
					char username[BUF_LEN], to[BUF_LEN], to_user[BUF_LEN], msg[BUF_LEN];
					sscanf(buf, "%s %s %s", username, to, to_user);
					strcpy(msg, buf + strlen(username) + strlen(to) + strlen(to_user) + 3);

					printf("[파일전송 요청] from %s to %s %s", username, to_user,msg);
					// 귓속말 전송
					int ii;
					int k;
					for (j = 0; j < num_chat; j++) {
						if(strcmp(userlist[j], to_user)==0)
							ii = client_fds[j];
					}
					for (j = 0; j < num_chat; j++) {
						if (strcmp(userlist[j], username) == 0)
							k = client_fds[j];
					}
				
					
							if (send(ii, buf, BUF_LEN, 0) < 0) {
								printf("client[%d] send error.", j);
								client_error[j] = 1;
								break;
							}
						//파일전송 요청
					

				
							memset(buf2, 0, BUF_LEN);
							recv(ii, buf2, BUF_LEN, 0);
							send(k, buf2, BUF_LEN, 0);
						
							sscanf(buf2, "%s %s %s %s", username, to, to_user,msg);
				

							if (strcmp(to, "/fileyes") == 0) {

								printf("[파일전송 허락] from %s to %s %s\n", username, to_user,msg);
					
								int ii;
								int k;
								for (j = 0; j < num_chat; j++) {
									if (strcmp(userlist[j], to_user) == 0)
										ii = client_fds[j];
								}
								for (j = 0; j < num_chat; j++) {
									if (strcmp(userlist[j], username) == 0)
										k = client_fds[j];
								}

								memset(buf, 0, BUF_LEN);
								while (1) {

									if ((n = recv(k, buf, BUF_LEN, 0)) > 0) {

										printf("received %d from client[%d] : %s\n", n, j, buf);

										//printf("this is ii %d\n", ii);
										//printf("this is buf %s\n", buf);
										// buf
										if (send(ii, buf, BUF_LEN, 0) < 0) {
											printf("client[%d] send error.", j);
											client_error[j] = 1;
											break;
										}
										if (strcmp(buf, "finish\n") == 0) {
											printf("파일전송 완료\n");
											break;
										}
									}
								}
								continue;
							}
								
							 else if (strcmp(to, "/fileno") == 0) {
								memset(buf2, 0, BUF_LEN);
								sprintf(buf2, "[파일 요청 거부]\n");
								if (send(k, buf2, BUF_LEN, 0) < 0) {
									printf("client[%d] send error.", j);
									client_error[j] = 1;
									break;
								}
								printf("[파일요청 거부] from %s to %s \n", username, to_user);
								continue;
							}
							continue;
						}
						if (strncmp(buf + n + 1, CHAT_CMD_WITH, strlen(CHAT_CMD_WITH)) == 0) { // "/with"
							char username[BUF_LEN], to[BUF_LEN], to_user[BUF_LEN], msg[BUF_LEN];
							sscanf(buf, "%s %s %s", username, to, to_user);
							//strcpy(msg, buf + strlen(username) + strlen(to) + strlen(to_user) + 3);

							printf("[1:1대화 요청] from %s to %s \n", username, to_user);
							// 귓속말 전송
							int ii;
							int k;
							for (j = 0; j < num_chat; j++) {
								if (strcmp(userlist[j], to_user) == 0)
									ii = client_fds[j];
							}
							for (j = 0; j < num_chat; j++) {
								if (strcmp(userlist[j], username) == 0)
									k = client_fds[j];
							}


							if (send(ii, buf, BUF_LEN, 0) < 0) {
								printf("client[%d] send error.", j);
								client_error[j] = 1;
								break;
							}




							memset(buf2, 0, BUF_LEN);
							if ((n = recv(ii, buf2, BUF_LEN, 0)) <= 0) {
								printf("recv error for client[%d]\n", j);
								client_error[j] = 1;
								continue;
							}


							sscanf(buf2, "%s %s %s", username, to, to_user);


							if (strcmp(to, "/withyes") == 0) {
								memset(buf2, 0, BUF_LEN);
								sprintf(buf2, "[%s]님과 대화를 시작합니다.\n", to_user);
								if (send(k, buf2, BUF_LEN, 0) < 0) {
									printf("client[%d] send error.", j);
									client_error[j] = 1;
									break;
								}
								printf("[1:1대화 요청 성공] from %s to %s \n", username, to_user);



								while (1) {

									memset(buf, 0, BUF_LEN);
									memset(msg, 0, BUF_LEN);

									if ((n = recv(ii, buf, BUF_LEN, 0)) > 0) {
										sscanf(buf, "%s %s", username, msg);
										printf("received %d from client[%d] : %s", n, j, buf);



										sprintf(buf, "[1:1] %s %s\n", username, msg);
										if (send(k, buf, BUF_LEN, 0) < 0) {
											printf("client[%d] send error.", j);
											client_error[j] = 1;
											break;
										}

										if (strcmp(msg, "/end") == 0) {
											printf("1:1 대화를 종료합니다.\n");
											break;
										}

									}


									if ((n = recv(k, buf, BUF_LEN, 0)) > 0) {

										printf("received %d from client[%d] : %s", n, j, buf);


										sscanf(buf, "%s %s", username, msg);
										sprintf(buf, "[1:1] %s %s\n", username, msg);
										if (send(ii, buf, BUF_LEN, 0) < 0) {
											printf("client[%d] send error.", j);
											client_error[j] = 1;
											break;
										}

										if (strcmp(msg, "/end") == 0) {
											printf("1:1 대화를 종료합니다.\n");
											break;
										}

									}




								}
							}

							else if (strcmp(to, "/withno") == 0) {
								memset(buf2, 0, BUF_LEN);
								sprintf(buf2, "[%s]님이과 대화를 거부했습니다.\n", to_user);
								if (send(k, buf2, BUF_LEN, 0) < 0) {
									printf("client[%d] send error.", j);
									client_error[j] = 1;
									break;
								}
								printf("[1:1대화 요청 실패] from %s to %s \n", username, to_user);
								continue;
							}
							continue;
						}

					
			// /exit 처리
			// exit 대신 /exit 로 변경.
			if (strncmp(buf + n + 1, CHAT_CMD_EXIT, strlen(CHAT_CMD_EXIT)) == 0) { // "/exit"
				RemoveClient(i);
				continue;
			}
			// /sleep 처리
			if (strncmp(buf + n + 1, CHAT_CMD_SLEEP, strlen(CHAT_CMD_SLEEP)) == 0) { // "/sleep"
				usersleep[i] = 1;
				continue;
			}
			// /wakeup 처리
			if (strncmp(buf + n + 1, CHAT_CMD_WAKEUP, strlen(CHAT_CMD_WAKEUP)) == 0) { // "/wakeup
				usersleep[i] = 0;
				continue;
			}
			// 모든 채팅 참가자에게 메시지 방송
			//printf("%s", buf);
			// Wrie All
			usersleep[i] = 0; // message 가 있으면 무조건 깨어난다.
			for (j = 0; j < num_chat; j++) {
				if (usersleep[j] != 1) { // user가 sleep 이면 어떤 메시지도 수신하지 않는다.
					ret = send(client_fds[j], buf, BUF_LEN, 0);
					if (ret <= 0) {
						printf("send error for client[%d]\n", j);
						client_error[i] = 1;
					}
				}
			}
		}
	}
	// 오류가 난 Client들을 뒤에서 앞으로 가면서 제거한다.
	for (i = num_chat - 1; i >= 0; i--) {
		if (client_error[i])
			RemoveClient(i);
	}
}
}


/* 채팅 탈퇴 처리 */
void RemoveClient(int i) {
#ifdef WIN32
	closesocket(client_fds[i]);
#else
	close(client_fds[i]);
#endif
	// 마지막 client를 삭제된 자리로 이동 (한칸씩 내릴 필요가 없다)
	printf("client[%d] %s 퇴장. 현재 참가자 수 = %d\n", i, userlist[i], num_chat - 1);
	for (int j = 0; j < num_chat; j++) {
		char buf[BUF_LEN];
		if (j != i) { // 본인 제외 다른 사용자에게 퇴장을 알린다.
			sprintf(buf, "[%s]님이 퇴장하였습니다.\n", userlist[i]);
			send(client_fds[j], buf, BUF_LEN, 0);
		}
	}
	if (i != num_chat - 1) {
		client_fds[i] = client_fds[num_chat - 1]; // socket 정보 
		strcpy(userlist[i], userlist[num_chat - 1]); // username 
		usersleep[i] = usersleep[num_chat - 1]; // sleep 상태
	}
	num_chat--;
}

// client_fds[] 내의 최대 소켓번호 확인
// select(maxfds, ..) 에서 maxfds = getmax(server_fd) + 1
int getmax(int k) {
	int max = k;
	int r;
	for (r = 0; r < num_chat; r++) {
		if (client_fds[r] > max) max = client_fds[r];
	}
	return max;
}
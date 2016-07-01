#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>

static void print_sockaddr_storage(struct sockaddr_storage* target);
static int doEcho(int clientSock);
int main(void) {

  struct addrinfo hints;
  struct addrinfo* res = NULL;

  memset(&hints, 0, sizeof(hints));
  /* AF_INET6はIPv4/IPv6両方を受け付ける */
  hints.ai_family = AF_INET6;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE; /* bind指定 */
  int result = getaddrinfo(NULL, "50000", &hints, &res);
  if(result < 0) {
      fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(result));
      return 1;
  }

  /* addrinfoを作ってしまえば、後の流れは同じ */
  int serviceSock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  if(serviceSock < 0) {
      perror("fail create socket");
      printf("errorno : %d\n", errno);
      return 1;
  }

  if(bind(serviceSock, res->ai_addr, res->ai_addrlen) < 0) {
     perror("fail bind");
     printf("errorno : %d\n", errno);
     return 1;
  }

  if(listen(serviceSock, 10) < 0) {
     perror("fail listen");
     printf("errorno : %d\n", errno);
     return 1;
  }
  freeaddrinfo(res);  /* 解放を忘れない! */

  /* イベント監視対象として、接続待ちソケットを登録 */
  int eventChkFd = epoll_create(10);
  if(eventChkFd < 0) {
     perror("fail epoll_create");
     printf("errorno : %d\n", errno);
     return 1;
  }

  struct epoll_event serverEv;
  memset(&serverEv, 0x0, sizeof(serverEv));
  serverEv.data.fd = serviceSock;
  serverEv.events = EPOLLIN | EPOLLET;
  if(epoll_ctl(eventChkFd, EPOLL_CTL_ADD, serviceSock, &serverEv) < 0) {
     perror("fail epoll_ctl");
     printf("errorno : %d\n", errno);
     return 1;
  }
    struct epoll_event events[10];
  while(1) {

     int eventCnt = epoll_wait(eventChkFd, events, 10, -1);
     if(eventCnt < 0) {
        perror("fail epoll_wait");
        printf("errorno : %d\n", errno);
        return 1;
     }
     int i;
     for(i = 0; i < eventCnt; i++) {

        if(events[i].data.fd == serviceSock) {
           /* 新規接続の場合は監視対象に登録 */
           struct sockaddr_storage srcaddr; /* IPv6アドレスも受け取れるように使用する構造体を変更 */
           int addrLen = sizeof(srcaddr);
           int clientSock = accept4(serviceSock, &srcaddr, &addrLen, SOCK_NONBLOCK);
           if(clientSock < 0) {
              perror("fail accept");
              printf("errorno : %d\n", errno);
              continue;
           }
           print_sockaddr_storage(&srcaddr);

           struct epoll_event ev;
           memset(&ev, 0x0, sizeof(ev));
           ev.events = EPOLLIN | EPOLLET;
           ev.data.fd = clientSock;
           if(epoll_ctl(eventChkFd, EPOLL_CTL_ADD, clientSock, &ev) < 0) {
               perror("fail epoll_ctli client");
               printf("errorno : %d\n", errno);
               return 1;
           }
        } else {
           /* 接続済のものはエコー処理*/
           if(doEcho(events[i].data.fd) < 0) {
              /* 閉じると自動的に監視対象からは削除される */
              close(events[i].data.fd);
           }
        }
    }
  }
}

int doEcho(int clientSock) {

  char buf[256];
  memset(buf, 0x0, sizeof(buf));

  while(1) {
    int receiveMsgSize = recv(clientSock, buf, sizeof(buf), 0);
    if(receiveMsgSize == 0) {
        return -1;
    } else if(receiveMsgSize < 0) {
      if(errno == EAGAIN) {
        break;
      } else {
        return -1;
      }
    }

    if(send(clientSock, buf, receiveMsgSize, 0) < 0) {
        return -1;
    }
  }
  return 0;
}

void print_sockaddr_storage(struct sockaddr_storage* target) {

  char hostname[256];
  char servicename[256];

  memset(hostname,    0x0, sizeof(hostname));
  memset(servicename, 0x0, sizeof(servicename));
  int res = getnameinfo((struct sockaddr *)target, sizeof(struct sockaddr_storage),
                 hostname,sizeof(hostname), servicename, sizeof(servicename),
                 NI_NUMERICHOST | NI_NUMERICSERV);
  if(res != 0) {
     fprintf(stderr,  "getaddrinfo: %s\n", gai_strerror(res));
     return;
  }

  printf("connect is %s:%s\n", hostname, servicename);
}

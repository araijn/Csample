#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>

static int doEcho(int clientSock);
int main(void) {

  int serviceSock = socket(PF_INET, SOCK_STREAM, 0);
  if(serviceSock < 0) {
      perror("fail create socket");
      printf("errorno : %d\n", errno);
      return 1;
  }

  struct sockaddr_in servicAddr;
  memset(&servicAddr, 0x0, sizeof(servicAddr));
  servicAddr.sin_family = AF_INET;
  servicAddr.sin_port   = htons(50000);
  servicAddr.sin_addr.s_addr   = INADDR_ANY;
  if(bind(serviceSock, (struct sockaddr *)&servicAddr, sizeof(servicAddr)) < 0) {
     perror("fail bind");
     printf("errorno : %d\n", errno);
     return 1;
  }

  if(listen(serviceSock, 10) < 0) {
     perror("fail listen");
     printf("errorno : %d\n", errno);
     return 1;
  }

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
           struct sockaddr srcaddr;
           int addrLen;
           int clientSock = accept4(serviceSock, &srcaddr, &addrLen, SOCK_NONBLOCK);
           if(clientSock < 0) {
              perror("fail accept");
              printf("errorno : %d\n", errno);
              continue;
           }
            /* ソケットをノンブロッキングモードに設定 */
          // int flgs = fcntl(clientSock, F_GETFL);
          // if(flgs < 0) {
          //    perror("fail fctl F_GETFL");
          //    printf("errorno : %d\n", errno);
          //    close(clientSock);
          //    continue;
          // }
          // flgs |= O_NONBLOCK;
          // if(fcntl(clientSock, F_SETFL, flgs) < 0) {
          //    perror("fail fctl F_SETFL");
          //    printf("errorno : %d\n", errno);
          //    close(clientSock);
          //    continue;
          // }

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


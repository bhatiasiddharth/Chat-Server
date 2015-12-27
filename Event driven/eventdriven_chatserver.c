//2011B4A7680P and 2011B4A7658P
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/msg.h>

#define LISTENQ 15
#define MAX_EVENTS 10
#define MAX_BUF 80

struct client_node {
  int fd;
  char name[80];
  int stage;
  char params[3][MAX_BUF];
  struct client_node* next;
};

struct msg{
  long mtype;
  int fd;
  int etype;
  char content[MAX_BUF];
};

static int msgqid, epfd;
static struct client_node* clientlist = NULL;
enum chatmsg_type {JOIN, LIST, UMSG, BMSG, LEAV, OTHER};
enum msg_type {RMSG = 1, WMSG, PMSG};

struct client_node* add_client(int cfd, char *name) {
  struct client_node* new_client = (struct client_node*)
                                  malloc(sizeof(struct client_node));
  strcpy(new_client->name, name);
  new_client->fd = cfd;
  // input stage - 0
  new_client->stage = 0;
  bzero(new_client->params, 3*MAX_BUF);
  new_client->next = clientlist;

  clientlist = new_client;

  return new_client;
}

int remove_client(int cfd) {
  struct client_node *current, *prev = NULL;
  for(current = clientlist; current != NULL; current = current->next) {
    if(current->fd == cfd) {
      if(prev != NULL) {
        prev->next = current->next;
      }else {
        clientlist = current->next;
      }
      free(current);
      return 1;
    }
    prev = current;
  }
  return -1;
}

char* get_clientlist() {
  static char clients[MAX_BUF];
  struct client_node* current;
  strcpy(clients, "Clients - ");
  for(current = clientlist; current != NULL; current = current->next) {
    if(current != clientlist)
      strcat(clients, ", ");
    strcat(clients, current->name);
  }
  strcat(clients, "\n");
  return clients;
}

int get_clientfd(char* name) {
  struct client_node* current;
  for(current = clientlist; current != NULL; current = current->next) {
    if(strcasecmp(current->name, name) == 0) {
      return current->fd;
    }

  }
  return -1;
}

int update_clientname(int cfd, char* name) {
  struct client_node* current;
  for(current = clientlist; current != NULL; current = current->next) {
    if(current->fd == cfd) {
      strcpy(current->name, name);
      return 1;
    }
  }
  return -1;
}

struct client_node* get_client(int cfd) {
  struct client_node* current;
  for(current = clientlist; current != NULL; current = current->next) {
    if(current->fd == cfd) {
      return current;
    }
  }
  return NULL;
}

void process_msg(int fd, char params[3][80]) {
  struct msg msg;
  struct client_node* current;
  bzero(&msg, sizeof(msg));
  msg.mtype = WMSG;
  printf("TYPE = %s;\n", params[0]);
  printf("PARAMS = >%s >%s;\n", params[1], params[2]);

  if(strcasecmp(params[0], "JOIN") == 0) {
    printf("Run JOIN\n");
    update_clientname(fd, params[1]);
  } else if(strcasecmp(params[0], "LIST") == 0) {
    printf("Run LIST\n");
    msg.fd = fd;
    strcpy(msg.content, get_clientlist());
    //msgsend
    if (msgsnd(msgqid, &msg, sizeof(struct msg), 0) == -1 )
      perror("msgsnd (msg)");
  } else if(strcasecmp(params[0], "UMSG") == 0) {
    printf("Run USMG\n");
    msg.fd = get_clientfd(params[1]);
    if(msg.fd == -1) {
      msg.fd = fd;
      strcpy(msg.content, "ERROR <not online>");
    } else
      strcpy(msg.content, params[2]);

    strcat(msg.content, "\n");
    //msgsend
   if (msgsnd(msgqid, &msg, sizeof(struct msg), 0) == -1 )
      perror("msgsnd (msg)");

  } else if(strcasecmp(params[0], "BMSG") == 0) {
    printf("Run BMSG\n");
    //msgsend for each client
    strcpy(msg.content, params[1]);
    strcat(msg.content, "\n");
    for(current = clientlist; current != NULL; current = current->next) {
      if(current->fd == fd) continue;
      msg.fd = current->fd;
      if (msgsnd(msgqid, &msg, sizeof(struct msg), 0) == -1 )
      perror("msgsnd (msg)");
    }
  } else if(strcasecmp(params[0], "LEAV") == 0) {
    printf("Run LEAV\n");
    remove_client(fd);
    close(fd);
  }
}

void process_thread() {
   struct msg msg;
   struct client_node* client;
   struct epoll_event ev;
   char buf[MAX_BUF];
   int matches, len;
   for(;;){
     bzero(buf, MAX_BUF);
     if(msgrcv(msgqid, &msg, sizeof(struct msg), 0, 0) == -1 ) {
            perror("msgrcv");
            continue;
     }
    /*printf("got message from fd - %d\n", msg.fd);*/
    client = get_client(msg.fd);
    if(client == NULL) {
        client = add_client(msg.fd, "undefined");
    }
    if(msg.mtype == RMSG) {

      if((len = read(msg.fd, buf, MAX_BUF)) < 0) {
        perror("read (sfd)");
      }

      if(len == 0) {
        close(msg.fd);
      }
      if(client->stage == 0) {
        bzero(client->params, 3*MAX_BUF);
        matches = sscanf(buf, " %s %[^\r] ", client->params[0], client->params[1]);
        if(matches == 0) continue;
        if(strcasecmp(client->params[0], "UMSG") == 0)
          client->stage = 1;
      } else if(client->stage == 1){
        sscanf(buf, " %[^\r] ", client->params[2]);
        client->stage = 0;
      }

      // input is complete
      if(client->stage == 0){
          msg.mtype = PMSG;
          if (msgsnd(msgqid, &msg, sizeof(struct msg), 0) == -1 )
              perror("msgsnd (msg)");
      }
      ev.events = EPOLLIN | EPOLLOUT;
      ev.data.fd = msg.fd;
      if (epoll_ctl(epfd, EPOLL_CTL_MOD, ev.data.fd, &ev) == -1)
            perror("epoll_ctl (mod)");
    }else if(msg.mtype == WMSG) {
        write(msg.fd, msg.content, strlen(msg.content));
    }else if(msg.mtype == PMSG) {
        process_msg(msg.fd, client->params);
    }
}
}
int get_msgcode(char* mtype) {
   if(strcasecmp(mtype, "JOIN") == 0)
     return JOIN;
   if(strcasecmp(mtype, "LIST") == 0)
     return LIST;
   if(strcasecmp(mtype, "UMSG") == 0)
     return UMSG;
   if(strcasecmp(mtype, "BMSG") == 0)
     return BMSG;
   if(strcasecmp(mtype, "LEAV") == 0)
     return LEAV;
   if(strcasecmp(mtype, "OTHER") == 0)
     return OTHER;
}

int main(int argc, char* argv[]) {
  int listenfd ,tid;
  int ready;
  socklen_t clilen;
  struct epoll_event ev;
  struct epoll_event evlist[MAX_EVENTS];
  struct sockaddr_in cliaddr, servaddr;
  struct client_node* client;
  struct msg msg;


  if (argc < 2) {
    printf("Usage: <port>");
    exit(0);
  }

  key_t key = ftok(__FILE__, 'W');

  msgqid = msgget(key, IPC_CREAT | 0666);

  if( msgqid == -1 ) {
    perror("msgget");
    exit(-1);
  }
  printf("MSQID - %d\n", msgqid);
  // Create server
  listenfd = socket (AF_INET, SOCK_STREAM, 0);

  bzero (&servaddr, sizeof (servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl (INADDR_ANY);
  servaddr.sin_port = htons (atoi (argv[1]));

  if (bind (listenfd, (struct sockaddr *) &servaddr, sizeof (servaddr)) < 0)
    perror ("bind");

  listen (listenfd, LISTENQ);

  // create epoll fd
  epfd = epoll_create(20);
  if (epfd == -1)
    perror("epoll_create");

  ev.events = EPOLLIN;   // Only interested in input events
  ev.data.fd = listenfd;

  // add listenfd
  if (epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &ev) == -1)
    perror ("epoll_ctl");

  pthread_create(&tid, NULL, process_thread, NULL);
  for(;;) {
    ready = epoll_wait(epfd, evlist, MAX_EVENTS, -1);

    if (ready == -1) {
      if (errno == EINTR)
        continue; /* Restart if interrupted by signal */
      else
        perror("epoll_wait");
    }

    int i;
    for(i = 0; i < ready; i++) {
      // EPOLLIN
      if(evlist[i].events & EPOLLIN) {
        // if event on listenfd
        if(evlist[i].data.fd == listenfd) {
          clilen = sizeof(cliaddr);
          int connfd = accept(listenfd, (struct sockaddr *) &cliaddr, &clilen);

          ev.events = EPOLLIN | EPOLLOUT;
          ev.data.fd = connfd;
          if (epoll_ctl(epfd, EPOLL_CTL_ADD, connfd, &ev) == -1)
            perror("epoll_ctl (add)");
          printf("Client with fd - %d joined\n", connfd);
        }else {
          printf("EPOLLIN (%d) %d / %d\n", evlist[i].data.fd, i ,ready);
          bzero(&msg, sizeof(msg));
          msg.mtype = RMSG;
          msg.fd = evlist[i].data.fd;

          // stop tracking pollin events
          ev.events = EPOLLOUT;
          ev.data.fd = evlist[i].data.fd;
          if (epoll_ctl(epfd, EPOLL_CTL_MOD, evlist[i].data.fd, &ev) == -1)
            perror("epoll_ctl (add)");

          // send EPOLLIN
          if (msgsnd(msgqid, &msg, sizeof(struct msg), 0) == -1 )
            perror("msgsnd (rmsg)");
         }
        }
      }
  }
  return 0;
}

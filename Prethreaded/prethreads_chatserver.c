//2011B4A7680P and 2011B4A7658P
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <strings.h>
#include <pthread.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#define MAX_BUF 80
socklen_t   addrlen, len;
pthread_mutex_t   mlock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t   datalock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t    mcond = PTHREAD_COND_INITIALIZER;
int       listenfd, connfd;
struct sockaddr *cliaddr;
struct sockaddr_in servaddr;

struct client_node {
  int fd;
  char name[80];
  struct client_node* next;
};

struct chat_msg{
  long mtype; // write fd
  char msg[MAX_BUF];
};

int iget, iput;
  int num_threads;

static struct client_node* clientlist = NULL;



enum mtype {JOIN = 1, LIST, UMSG, BMSG, LEAV};

void add_client(char* name, int cfd) {
  struct client_node* new_client = (struct client_node*)
                                  malloc(sizeof(struct client_node));
  strcpy(new_client->name, name);
  new_client->fd = cfd;

  new_client->next = clientlist;

  clientlist = new_client;
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
  static char clients[80];
  struct client_node* current;
  strcpy(clients, "Clients - ");
  current = clientlist;

  if(current!=NULL)
  {
  	strcat(clients, current->name);

	  for(current = clientlist->next; current != NULL; current = current->next) {
	    
	    strcat(clients, ",");
	    strcat(clients, current->name);
	  }
}
  return clients;
}

int get_clientfd(char* name) {
  struct client_node* current;
  for(current = clientlist; current != NULL; current = current->next) {
    if(strcmp(current->name, name) == 0) {
      return current->fd;
    }

  }

  return -1;
}

char* get_clientname(int cfd) {
  struct client_node* current;
  for(current = clientlist; current != NULL; current = current->next) {
    if(current->fd == cfd) {
      return current->name;
    }

  }

  return NULL;
}




static void *func(void *);    /* each thread executes this function */

int
main(int argc, char **argv)
{

  
   num_threads=atoi(argv[1]);
  
  
  pthread_t tid[num_threads];
  listenfd=socket(AF_INET,SOCK_STREAM,0);
  bzero(&servaddr,sizeof(servaddr));
  servaddr.sin_family=AF_INET;
  servaddr.sin_addr.s_addr=htonl(INADDR_ANY);
  servaddr.sin_port=htons(atoi(argv[2]));
  bind(listenfd,(struct sockaddr *)&servaddr,sizeof(servaddr));
  listen(listenfd,10);

  cliaddr = malloc(addrlen);

/*
  for ( ; ; ) {
    len = addrlen;
    //connfd = accept(listenfd, cliaddr, &len);
    pthread_create(&tid1, NULL, &doit, (void *) connfd);
  }
*/
 int rc;
  int t;
  for(t=0;t<num_threads;t++)
   {
     
      rc = pthread_create(&tid[t], NULL, func, (void *)&t);
      
       if (rc){
         printf("ERROR; return code from pthread_create() is %d\n", rc);
         exit(-1);
         }
    }
    for(t=0;t<num_threads;t++)
    {
      rc=pthread_join(tid[t], NULL);  
    }

}

void handler(int fd)
{
 
  struct client_node* current;
  
  char client_message[2000];
  int n;
  char* temp1 = (char *) malloc(sizeof(char)*400);
    char* temp2 = (char *) malloc(sizeof(char)*400);
       struct chat_msg cmsg;
   int matches;
   char * pch,*temp;
 while((n=recv(fd,client_message,2000,0))>0)
    {
      char params[3][80];
      char* params0;
      
        bzero(&cmsg, sizeof(cmsg));

		  pch = strtok (client_message," \r\n");
		  //strcpy(pch,params0);
		  while (pch != NULL)
		  {
		  	if((strcmp(pch, "JOIN") == 0)|| (strcmp(pch, "join") == 0))
	        {
	            printf("Got JOIN\n");
	            pch = strtok (NULL, "\r\n");
	            pthread_mutex_lock(&datalock);
	            add_client(pch, fd);
	            pthread_mutex_unlock(&datalock);
	            break;
	        } 
	        

	       	else if((strcmp(pch, "LIST") == 0) || (strcmp(pch, "list") == 0) ) 
          	{
	            printf("Got LIST\n");
	            cmsg.mtype = fd;
	            pthread_mutex_lock(&datalock);
	            strcpy(cmsg.msg, get_clientlist());
	            //strcpy(cmsg.msg, "HI");
	            send(fd,cmsg.msg,strlen(cmsg.msg),0);
	            send(cmsg.mtype,"\n",1,0);
	            pthread_mutex_unlock(&datalock);
	            break;
            
          	} 

	          	else if((strcmp(pch, "UMSG") == 0)|| (strcmp(pch, "umsg") == 0)) 
	          {
	            printf("Got USMG\n");
	            pch = strtok (NULL, "\r\n");
	            temp=pch;
	            

	            recv(fd,client_message,2000,0);
	            pch = strtok (client_message,"\r\n");

	            pthread_mutex_lock(&datalock);
	            
	            cmsg.mtype = get_clientfd(temp);
	            if(cmsg.mtype == -1) {
	              cmsg.mtype = fd;
	              strcpy(cmsg.msg, "ERROR <not online>");
	            } else
	            {
	            	//pch = strtok (NULL, "\r\n");
	              strcpy(cmsg.msg, pch);
	            }

	            send(cmsg.mtype,cmsg.msg,strlen(cmsg.msg),0);
	            send(cmsg.mtype,"\n",1,0);
	            pthread_mutex_unlock(&datalock);
	            break;
	           
	          } 

	          else if((strcmp(pch, "BMSG") == 0) || (strcmp(pch, "bmsg") == 0) )
	           {
	            printf("Got BMSG\n");
	            //msgsend for each client

	            pch = strtok (NULL, "\r\n");
	            strcpy(cmsg.msg, pch);
	            pthread_mutex_lock(&datalock);
	            for(current = clientlist; current != NULL; current = current->next) {
	              cmsg.mtype = current->fd;
	              
	            send(cmsg.mtype,cmsg.msg,strlen(cmsg.msg),0);
	            send(cmsg.mtype,"\n",1,0);

	           
	            }
	            pthread_mutex_unlock(&datalock);
	            break;
	          } 

	          else if((strcmp(pch, "LEAV") == 0) || (strcmp(pch, "leav") == 0) ) 
	          {
	            printf("Got LEAV\n");
	            pthread_mutex_lock(&datalock);
	            remove_client(fd);
	            pthread_mutex_unlock(&datalock);
	            close(fd);
	            return ;
	          }

	          else
	          	break;

		  }




        
         
        
        strcpy(client_message,"\0");
        
    }
    

    if(n==0)
    {
      puts("Client Disconnected");
    }

    else
    {
      perror("recv failed");
    }
  return ;
}




static void *func(void *arg)
{

  int    client_soc,clilen;
  while(1)
  {

          clilen = sizeof(struct sockaddr_in);
          pthread_mutex_lock(&mlock);

          /*
          while (iget != iput)
          pthread_cond_wait(&mcond,&mlock);
          
          
          if (++iget == num_threads)
          iget = 0;
          */

          if ( (client_soc = accept(listenfd, cliaddr, &len)) < 0)
          {
            //printf("accept error\n");
          }
          
          pthread_mutex_unlock(&mlock);
          handler(client_soc);
          close(client_soc);
  }

  //pthread_detach(pthread_self());
  //str_echo((int) arg);  /* same function as before */
  //close((int) arg);   /* we are done with connected socket */
  return(NULL);
}


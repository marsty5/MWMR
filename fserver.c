/************************************************************************
 * Name & Surname: Maria Stylianou	ID: 1012147			*
 * Department: Computer Science of University of Cyprus			*
 * Last Update: 25-MAR-2011						*
 * Title of program: fserver.c						*
 * 									*
 * Functionality:							*
 * It has a pathetic role in the system; 				*
 * It receives messages from clients and answers back. When it receives:*
 * (a) a WRITE message, it checks if it is valid. If positive, it saves 
 *                      the whole message and sends a WRITEACK message  *
 * (b) a READ message, it checks if it is valid. If positive, it sends 
                        back a READACK message with its local message	*/
/************************************************************************
 *				SERVER					*
 ************************************************************************/
/* LIBRARIES ************************************************************/
#include <stdio.h>	/* For I/O					*/
#include <stdlib.h>	/* For exit() malloc() free()			*/
#include <string.h>	/* For strings - strcpy() strcmp() strcat()	*/
#include <pthread.h>	/* For threads 					*/
#include <unistd.h>	/* For close() 					*/
#include <sys/socket.h>	/* For sockets - socket() bind() listen() 	*
					  accept() connect() shutdown()           *
					  close() setsockopt()   	*/
#include <sys/types.h>	/* For sockets -  same as above			*/
#include <netinet/in.h>	/* For Internet sockets - 			*
			    address formating in a general structure	*/
#include <arpa/inet.h>  /* For inet_ntop()                              */
#include <netdb.h>	/* For getaddrinfo()				*/
#include <signal.h> /* For struct sigaction				*/
/* CONSANTS *************************************************************/
#define MAX_CLIENTS 160	/* Maximum number of accepted clients		*/
#define FIELDS 2	/* Fields of the table with the requests 	*/
#define MAX_QUEUE 160	/* Maximum number of requests for connections	*
			   waiting in the queue				*/
#define BUFLEN 256	/* Buffer length				*/
#define ACK "ACK"	/* To be concatenated with the type of the	* 
			   received message				*/
#define READ "READ"	/* READ message					*/
#define WRITE "WRITE"	/* WRITE message				*/
#define EXIT "EXIT"	/* EXIT message for closing the connection	*/
#define SIZE 10		/* Size of type of message			*/
#define FILENAME_LEN 20	/* Length for filename                          */
/* STRUCTURES ***********************************************************/
/* Structure for node 							*/
typedef struct node_t node;

struct node_t {
    int sock;           /* Socket descriptor 				*/
    node *next;         /* Pointer to next node 			*/
};

/* Structure for queue							*/
typedef struct {
    node *head;         /* Head of the queue				*/
    node *tail;         /* Tail of the queue				*/
    int size;           /* Size of the queue				*/
} queue;

/* Structure for tag							*/
typedef struct {
    int ts;             /* Timestamp 					*/
    int wid;            /* Writer's id 					*/
} tag_type;

/* Structure for message 						*/
typedef struct {
    char type[SIZE];    /* Type of message; READACK WRITEACK		*/
    int pid;            /* Process ID					*/
    tag_type tag;       /* Tag: timestamp and writer's id 		*/
    int value;          /* Object's New value 				*/
    int reqNo;          /* Number of client's request 			*/
} message;
/* FUNCTIONS ************************************************************/
void sigchld_handler(int signo);
void *get_in_addr(struct sockaddr *sa);
/*Threads Functions							*/
void enqueue(int sock);
int dequeue();
void createTpool(FILE *fout);
void *handleClient(void *args);
void *check();
void initQueue();
/* Clients Table Functions						*/
void initTable();
void printfClientStatus();
void saveClientId(int pid);
int returnClientPos(message *msg);
void rmvClient(int position);
/* Message Functions 							*/
void initMsg(message *msg);
void printfMsg(message *msg);
void fprintfMsg(FILE *fout, message *msg);
void strToMsg(char *token, message *msg);
void msgToStr(char *buf, message msg);
int checkValid(FILE *fout, int position, message *msg);
int notFail(FILE *fout, int position);
int cmpTag(tag_type tag1, tag_type tag2);
void msgCpy(message *dest, message src, int cmp);
/* GLOBAL VARIABLES *****************************************************/
queue q;
pthread_t pid[MAX_CLIENTS]; /* Table with thread IDs                    */
pthread_t pidc;             /* Thread ID for the "master thread"	*/
int threadnum;              /* Number of threads			*/
pthread_mutex_t mutex;      /* Synchronization mechanism for threads*/
pthread_cond_t cond;        /* Condition variable for threads           */
message smsg;               /* Local message of server                  */
message cmsg;               /* Message from client			*/
int clients[MAX_CLIENTS][FIELDS]; /* Table with clients' IDs and
				     clients' most recent request Number*/
int failFreq;               /* In auto-procedure - Frequency of failures*/
/************************************************************************/
/*				MAIN					*/
/************************************************************************/
int main(int argc, char *argv[]) {
    /* Definitions 							*/
    int id;                     /* Server's id				*/
    int port;                   /* Server's port			*/
    int sock;                   /* Socket file descriptor		*/
    int newsock;                /* Socket fd for a new connection	*/
    struct sockaddr_storage client; /* Client's information		*/
    socklen_t clientlen;        /* Address of client's address          */
    int yes;                    /* Flag for setsockopt()		*/
    FILE *fout;                 /* Pointer to output file		*/
    char outFile[FILENAME_LEN]; /* Name of output log file		*/
    int status;                 /* Result from getaddrinfo()		*/
    struct addrinfo hints;      /* For getaddrinfo()			*/
    struct addrinfo *servinfo;  /* Point to info for ipv4  		*/
    struct addrinfo *p;         /* Counter				*/
    struct sigaction sa;
    char hostname[INET6_ADDRSTRLEN]; /* Hostname                        */
    /********************************************************************/
    /* Check if input arguments are given				*/
    if (argc != 4) {
        printf("\nUsage of file: %s <serverID> <port number> <failFreq>\
	    \n(0<=failFreq<=100)\n",
               argv[0]);
        exit(1);
    }
    /* Convert arguments to integers					*/
    id = atoi(argv[1]);
    port = atoi(argv[2]);
    failFreq = atoi(argv[3]);
    printf("FAIL = %d \n", failFreq);
    sprintf(outFile, "log-fs%d.dat", id);

    /* Open file to write                                               */
    if (!(fout = fopen(outFile, "w"))) { /* Servers file		*/
       perror("fopen()");
       exit(1);
    }
    printf("\nStart fserver...\n");
    fprintf(fout,"\nStart fserver...\n");
   
    srand(time(NULL));      /* Begin rand() seed related to current time*/

    memset(&hints, 0, sizeof hints); /* Initialise struct hints to zero	*/
    hints.ai_family = AF_UNSPEC;     /* Either IPv4 or IPv6		*/
    hints.ai_socktype = SOCK_STREAM; /* TCP stream sockets		*/
    hints.ai_flags = AI_PASSIVE;     /* Fill in IP automatically	*/

    /* Get server's information						*/
    if ((status = getaddrinfo(NULL, argv[2], &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        exit(1);
    }
    /* gai_strerror prints the error if there is a non-zero result	*/
    /* servinfo now points to a linked list of 1 or more struct addrinfos*/

    /* Create socket 							*/
    if ((sock = socket(servinfo->ai_family, servinfo->ai_socktype,
                       servinfo->ai_protocol)) < 0) {
        perror("socket()");
        exit(1);
    }

    yes = 1;
    // lose the pesky "Address already in use" error message
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof (int)) < 0) {
        perror("setsockopt");
        close(sock);
        exit(1);
    }

    /* Bind socket to address 						*/
    if (bind(sock, servinfo->ai_addr, servinfo->ai_addrlen) < 0) {
        perror("bind()");
        close(sock);
        exit(1);
    }

    freeaddrinfo(servinfo); // free the linked-list

    /* Listen for connections						*/
    if (listen(sock, MAX_QUEUE) < 0) {
        perror("listen()");
        close(sock);
        exit(1);
    }

    
    /* Initialize servers queue, clients table, server & client message */
    initQueue();
    initTable();
    initMsg(&smsg);
    initMsg(&cmsg);

    /* Create Threadpool 						*/
    createTpool(fout);


    while (1) {
        printf("<Wait for connection on TCP port %d>\n", port);
        fprintf(fout, "<Wait for connection on TCP port %d>\n", port);
        printf("Local message: ");
        fprintf(fout, "Local message: ");
        printfMsg(&smsg);
        fprintfMsg(fout, &smsg);

        /* Accept connection 						*/
        clientlen = sizeof client;
        if ((newsock = accept(sock, (struct sockaddr *) & client,
                              &clientlen)) < 0) {
            printf("server: id=%d => ",id);
            perror("accept()");
            close(sock);
            exit(1);
        }
        /* Find client's address 					*/
        inet_ntop(client.ss_family,
                  get_in_addr((struct sockaddr *) & client),
                  hostname, sizeof hostname);

        printf("**************************************************\n");
        fprintf(fout, "**************************************************\n");

        printf("<Accepted connection from %s - ", hostname);
        fprintf(fout, "<Accepted connection from %s - ", hostname);
        if (threadnum) {        /* If there is an available thread 	*/
            printf("a thread has undertaken the connection>\n");
            fprintf(fout, "a thread has undertaken the connection>\n");
            pthread_mutex_lock(&mutex);
            enqueue(newsock);   /* -> Add connection fd in the queue	*/
            pthread_mutex_unlock(&mutex);
        }
        else {                  /* Else					*/
            printf("clients queue is full>\n \
      <Closing Connection with socket %d...>\n\n", newsock);
            fprintf(fout, "clients queue is full>\n \
      <Closing Connection with socket %d...>\n\n", newsock);
            shutdown(newsock, SHUT_RDWR); /* -> Close connection	*/
            close(newsock);
        }
    } /* End of while(1)						*/
    fclose(fout);
    return;
} /* End of main()							*/
/************************************************************************/
/*				FUNCTIONS				*/
/************************************************************************/

/* Get sockaddr, IPv4 or IPv6:                                          */
void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*) sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*) sa)->sin6_addr);
}
/************************************************************************/

/* Initialize queue							*/
void initQueue() {
    q.head = NULL;
    q.tail = NULL;
    q.size = 0;
} /* End of initQueue()							*/
/************************************************************************/

/* Add a node to the queue (at the tail)				*/
void enqueue(int sock) {
    node* newNode = (node *) malloc(sizeof (node)); /* Create node	*/
    newNode->sock = sock;
    newNode->next = NULL;

    if (!q.head)            /* If queue is empty			*/
        q.head = q.tail = newNode;  /* -> Add newNode as a head and tail*/
    else { /* Else					*/
        q.tail->next = newNode;     /* -> Add newNode as a tail		*/
        q.tail = newNode;
    }
    ++(q.size);                     /* Increase queue size +1		*/
} /* End of enqueue()							*/
/************************************************************************/

/* Remove a node from the queue (from the head)				
   Return connection file descriptor 					*/
int dequeue() {
    int socket;
    node* tmp;

    if (!q.head) {                      /* If queue is empty		*/
        printf("Queue is empty!\n");    /* -> nothing to remove		*/
        return -1;
    }                                   /* Else				*/
    socket = q.head->sock;              /* -> Save connection fd	*/
    tmp = q.head;                       /*    and remove last fd from queue*/
    q.head = q.head->next;
    free(tmp);

    --(q.size);                         /* Dicrease size -1		*/
    if (q.size == 0)                    /* If queue is empty		*/
        q.tail = NULL;                  /* -> Direct tail to show to NULL*/

    return socket;                      /* Return socket of connection	*/
} /* End of dequeue()							*/
/************************************************************************/

/* Create thread pool							*/
void createTpool(FILE *fout) {
    int i;

    /* Handle Requests                                                  */
    for (i = 0; i < MAX_CLIENTS; i++) {
        pthread_create(&pid[i], NULL, handleClient, (void *) fout); 
    }
    threadnum = i;                      /* Number of threads            */
    /* Check if there's request                                         */
    pthread_create(&pidc, NULL, check, NULL);
} /* End of createTpool							*/
/************************************************************************/

/* Check every second if there is a request				*/
void *check() {
    while (1) {
        if (threadnum && q.head != NULL) /* If there is an available thread
                                            & queue is not empty 	*/
            pthread_cond_signal(&cond); /* -> Send a signal to thread	*/
        sleep(1);
    }
} /* End of check()							*/
/************************************************************************/

/* Function for handling a new request */
void *handleClient(void *args) {
    int socket;             /* Connection fd extracted from queue 	*/
    char *token;            /* Usage: To keep part of the line from the file*/
    int i;                  /* Counter 					*/
    char buf[BUFLEN];       /* Buffer					*/
    int position;           /* Client position from clients table	*/
    int cmp;                /* Flag for comparing 2 tags		*/
    FILE *fout;             /* File Descriptor for output file          */
    int times;              /* Counts requests                          */

    fout = (FILE *) args;
    while (1) {
        pthread_mutex_lock(&mutex);         /* Lock mutex		*/
        pthread_cond_wait(&cond, &mutex);   /* Block the calling thread
					   until cond is signalled. 	*/
        pthread_mutex_unlock(&mutex);       /* Unlock mutex		*/

        pthread_mutex_lock(&mutex);
        threadnum--;                        /* Reduce number of threads	*/
        pthread_mutex_unlock(&mutex);

        pthread_mutex_lock(&mutex);
        socket = dequeue();                 /* Extract the fd from queue*/
        pthread_mutex_unlock(&mutex);

        times = 1;
        while (1) {
            bzero(buf, strlen(buf));                /* Initialize buffer*/
            if (read(socket, buf, BUFLEN) < 0) {    /* Receive Message	*/
                perror("read()");
                close(socket);
                //exit(1);
            }
            /*
            printf("\nThe message '%s' was received from socket %d\n",
                   buf, socket);
            fprintf(fout, "\nThe message '%s' was received from socket %d\n",
                    buf, socket);
            */
            if (buf == NULL || strlen(buf)==0) {
                printf("-------------------->NULL\n");
                shutdown(socket, SHUT_RDWR);
                close(socket);
                break;
            }
            if (buf[0] != 'W' && buf[0] != 'R'){
               // if (!strcmp(buf, EXIT)) { /* If message is Exit                 */
                    printf("\n'%s' received from clientId: %d\n", buf, cmsg.pid);
                    fprintf(fout, "\n'%s' received from clientId: %s\n", buf, cmsg.type);
                //}
               // else{ /* To avoid connecting with unwanted clients              */
                 //   printf("---Unknown type of message:%s \n",buf);
                   // fprintf(fout, "---Unknown type of message \n");
               // }
                printf("<Closing Connection with socket %d...>\n\n", socket);
                fprintf(fout, "<Closing Connection with socket %d...>\n\n", socket);
                shutdown(socket, SHUT_RDWR);
                close(socket);
                break;
            }
            if (buf[0] == 'W' || buf[0] == 'R') {
                pthread_mutex_lock(&mutex);
                token = strtok(buf, ",");   /* Extract type of message	*/
                strToMsg(token, &cmsg); /* Save string message to cmsg	*/
                pthread_mutex_unlock(&mutex);

                if (times == 1) {           /* If it is the 1st msg	*/
                    times++;
                    //printf("Client's id is %d\n", cmsg.pid);
                    //fprintf(fout, "Client's id is %d\n", cmsg.pid);
                    pthread_mutex_lock(&mutex);
                    saveClientId(cmsg.pid); /* Save Client's id         */
                    pthread_mutex_unlock(&mutex);
                }
                printf("---Receive a %s message from clientId: %d\t", cmsg.type, cmsg.pid);
                fprintf(fout, "---Receive a %s message from clientId: %d\t", cmsg.type, cmsg.pid);
                printfMsg(&cmsg);
                fprintfMsg(fout, &cmsg);
                position = returnClientPos(&cmsg); /* Find client's position*/

                /* If cmsg is valid &&	server not failed		*/
                if (checkValid(fout, position, &cmsg) && notFail(fout, position)) {
                    
                    pthread_mutex_lock(&mutex);
                    clients[position][1] = cmsg.reqNo;/* -> Save client'sreqNo */
                    //printfClientStatus();
                    cmp = cmpTag(cmsg.tag, smsg.tag); /* -> Compare server&client tags*/
                    msgCpy(&smsg, cmsg, cmp);         /* -> Save cmsg to smsg	*/
                    pthread_mutex_unlock(&mutex);
                    bzero(buf, strlen(buf));          /* Initialize buffer	*/
                    msgToStr(buf, smsg);
                    printf("---Send an ACK message\t");
                    fprintf(fout, "---Send an ACK message\t");
                    printfMsg(&smsg);
                    fprintfMsg(fout, &smsg);
                    if (write(socket, buf, BUFLEN) < 0) { /* Send ACK	*/
                        perror("write");
                        exit(EXIT_FAILURE);
                    }
                    printf("<Waiting for next request...>\n\n");
                    fprintf(fout, "<Waiting for next request...>\n\n");
                } /* End of if checkValid() && notFail()		*/
            }
        } /* End of inner while(1)					*/
        /* Client has closed connection					*/
        pthread_mutex_lock(&mutex);
        threadnum++;                /* Increase number of threads	*/
        rmvClient(position);        /* Remove client from table         */
        //printfClientStatus();
        pthread_mutex_unlock(&mutex);
    } /* End of while(1)						*/
} /* End of handleClient()						*/
/************************************************************************/

/* Initialize table							*/
void initTable() {
    int i, j;
    for (i = 0; i < MAX_CLIENTS; i++)
        for (j = 0; j < FIELDS; j++)
            clients[i][j] = 0;
} /* End of initTable()							*/
/************************************************************************/

/* Save client's id in clients table					*/
void saveClientId(int pid) {
    int i;
    for (i = 0; i < MAX_CLIENTS; i++)
        if (clients[i][0] == 0) {
            clients[i][0] = pid;
            break;
        }
} /* End of saveClientId()						*/
/************************************************************************/

/* Print Clients' status (id and request number)			*/
void printfClientStatus() {
    int i;
    for (i = 0; i < MAX_CLIENTS; i++)
        printf("clients[%d][0]=%d, clients[%d][1]=%d\n",
               i, clients[i][0], i, clients[i][1]);
} /* End of printfClientStatus()					*/
/************************************************************************/

/* Based on a message
   Return Client's Position on the clients table. Return -1 otherwise	*/
int returnClientPos(message *msg) {
    int i;
    for (i = 0; i < MAX_CLIENTS; i++)
        if ((msg->pid == clients[i][0]))
            return i;
    return -1;
} /* End of returnClientPos()						*/
/************************************************************************/

/* Remove client from table						*/
void rmvClient(int position) {
    clients[position][0] = 0;
    clients[position][1] = 0;
} /* End of rmvClient()							*/
/************************************************************************/

/* Initialize message							*/
void initMsg(message *msg) {
    strcpy(msg->type, "\0");
    (msg->tag).ts = 0;
    (msg->tag).wid = 0;
    msg->value = -1;
    msg->reqNo = 0;
} /* End of initMsg()							*/
/************************************************************************/

/* Print message content						*/
void printfMsg(message *msg) {
    printf("(type,pid,<ts,wid>,value,req)\t(%s,%d,<%d,%d>,%d,%d)\n",
           msg->type, msg->pid, (msg->tag).ts, (msg->tag).wid, msg->value,
           msg->reqNo);
} /* End of printfMsg()							*/
/************************************************************************/

/* Print message contents */
void fprintfMsg(FILE *fout, message *msg) {
    fprintf(fout, "(type,<ts,wid>,value,req)\t(%s,<%d,%d>,%d,%d)\n",
            msg->type, (msg->tag).ts, (msg->tag).wid, msg->value, msg->reqNo);
} /* End of fprintfMsg()						*/
/************************************************************************/

/* Transform string to the structure message 				*/
void strToMsg(char *token, message *msg) {
    strcpy(msg->type, token);
    token = strtok(NULL, ",");
    msg->pid = atoi(token);
    token = strtok(NULL, ",");
    (msg->tag).ts = atoi(token);
    token = strtok(NULL, ",");
    (msg->tag).wid = atoi(token);
    token = strtok(NULL, ",");
    msg->value = atoi(token);
    token = strtok(NULL, ",");
    msg->reqNo = atoi(token);
} /* End of strToMsg()							*/
/************************************************************************/

/* Transform structure message to string				*/
void msgToStr(char *buf, message msg) {
    sprintf(buf, "%s,%d,%d,%d,%d,%d\0", msg.type, msg.pid, (msg.tag).ts,
            (msg.tag).wid, msg.value, msg.reqNo);
} /* End of msgToStr()							*/
/************************************************************************/

/* Check if the message received is valid 				
   (request number is the most recent)					
   Return 1 if it is valid. Return 0 otherwise 				*/
int checkValid(FILE *fout, int position, message *msg) {
    int i;
    if (msg->reqNo >= clients[position][1]) /* If most recent request	*/
        return 1;
    printf("--- Ignore message - Request number is not the most recent\n");
    fprintf(fout, "--- Ignore message - Request number is not the most recent\n");
    return 0;
} /* End of checkValid()						*/
/************************************************************************/

/* Check if server will fail						*
   If random > failFreq then fail 					*/
int notFail(FILE *fout, int position) {
    int num = 0; /* Random number					*/

    num = rand() % 100; /* Random Number between [0...100]		*/
    printf("Fail random number = %d\n", num);
    fprintf(fout, "Fail random number = %d\n", num);
    if (num < failFreq)
        return 1;
    if (num >= failFreq) {
        printf("--- Ignore message - Server failed\n");
        fprintf(fout, "--- Ignore message - Server failed\n");
        return 0;
    }
} /* End of notFail()							*/
/************************************************************************/

/* Based on two tags
   Rerun 1 if tag1 > tag2, 0 if tag1 == tag2, -1 if tag1 < tag2		*/
int cmpTag(tag_type tag1, tag_type tag2) {
    if (tag1.ts > tag2.ts)                  /* If ts1 > ts2		*/
        return 1;                           /* -> tag1 > tag2           */
    if (tag1.ts == tag2.ts)                 /* If ts1 == ts2            */
        if (tag1.wid > tag2.wid)            /*    If wid1 > wid2	*/
            return 1;                       /*    -> tag1 > tag2	*/
    if (tag1.ts == tag2.ts)                 /*    If ts1== ts2          */
        return 0;                           /*    -> tag1 = tag2	*/
    return -1;                              /* -> tag1 < tag2           */
} /* End of cmpTag()							*/
/************************************************************************/

/* Copy src message into dest message depending on cmp			*/
void msgCpy(message *dest, message src, int cmp) {
    strcpy(dest->type, src.type);       /* Save type in dest 		*/
    strcat(dest->type, ACK);
    dest->reqNo = src.reqNo;            /* Save reqNo in dest		*/
    dest->pid = src.pid;                /* Save pid in dest		*/

    if (cmp == 1) {                     /* If src.tag > dest.tag 	*/
        (dest->tag).ts = (src.tag).ts;  /* -> Save ts			*/
        (dest->tag).wid = (src.tag).wid;/* -> Save wid			*/
        dest->value = src.value;        /* -> Save value		*/
    } /* End of if							*/
} /* End of msgCpy							*/
/************************************************************************/
/* 				END OF FILE				*/
/************************************************************************/

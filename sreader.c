/************************************************************************
 * Name & Surname: Maria Stylianou	ID: 1012147			*
 * Department: Computer Science of University of Cyprus			*
 * Last Update: 25-MAR-2011						*
 * Title of program: sreader.c						*
 * 									*
 * Functionality:							*
 * It has an active role in the system; 				*
 * It reads a message, to the object which the servers hold, in 2RTT.   *
 * 1RTT: It sends a READ message to find the most recent message	*
 * 2RTT: It send a WRITE message with the most recent message found     */
/************************************************************************
 *				READER					*
 ************************************************************************/
/* LIBRARIES ************************************************************/
#include <stdio.h>	/* For I/O					*/
#include <stdlib.h>	/* For exit() malloc() free()			*/
#include <string.h>	/* For strings - strcpy() strcmp() strcat()	*/
#include <unistd.h>	/* For close() 					*/
#include <sys/socket.h>	/* For sockets - socket() bind() listen() 	*
					 accept() connect() shutdown()  *
					 close() setsockopt()   	*/
#include <sys/types.h>	/* For sockets -  same as above			*/
#include <netinet/in.h>	/* For Internet sockets - 			*
			    address formating in a general structure	*/
#include <netdb.h>	/* For getaddrinfo()				*/
#include <time.h>	/* For time()					*/
#include <sys/time.h>
#include <sys/select.h>	/* For select()					*/
/* CONSTANTS ************************************************************/
#define BUFLEN 256	/* Buffer Length				*/
#define SIZE 10		/* Size of type of message			*/
#define RANGE 100	/* Generate a random number [0-RANGE]		*/
#define ACK "ACK"	/* To be concatenated with the type of the	* 
			   received message				*/
#define READ "READ"	/* READ message					*/
#define WRITE "WRITE"	/* WRITE message				*/
#define EXIT "EXIT"	/* EXIT message for closing the connection	*/
#define FILENAME_LEN 20	/* Length for filename				*/
#define MINOP 1		/* Minimum option on the menu 			*/
#define MAXOP 2		/* Maximum option on the menu 			*/
#define AUTO 6		/* Number of arguments for auto-procedure	*/
#define MAN 4		/* Number of argumetns for manual procedure	*/
#define MILLION 1000000 /* One million - for math			*/
/* GLOBAL ***************************************************************/
fd_set readfds;         /* Group of servers who have answered           */
fd_set crashfds;        /* Group of servers who have crashed            */
/* STRUCTURES ***********************************************************/

/* Structure for tag 							*/
typedef struct {
    int ts;             /* Timestamp                                    */
    int wid;            /* Writer's id                                  */
} tag_type;

/* Structure for message 						*/
typedef struct {
    char type[SIZE];        /* Type of message: WRITE, READ             */
    int pid;                /* Process id                               */
    tag_type tag;           /* Tag: timestamp and writer's id           */
    int value;              /* Object's value                           */
    int reqNo;              /* Number of request                        */
} message;

/* Structure for server							*/
typedef struct {
    int id;                 /* Server's Id                              */
    int sock;               /* Socket file descriptor                   */
    char hostname[RANGE];   /* Server's hostname                	*/
    struct addrinfo *serv;  /* Server                                   */
    int rand;               /* Flag showing if it was chosen randomly	*/
    int ack;                /* Flag showing if it sent ACK              */
    message msg;            /* Message					*/
} server;

/* Structure for quorum							*/
typedef struct {
    int id;                 /* Quorum's id				*/
    int interId;            /* Intersected quorum's id			*/
    int servNum;            /* Number of servers			*/
    int *servers;           /* Table with Servers' names		*/
} quorum;
/* FUNCTIONS ************************************************************/
/* Print Functions							*/
void printMenu(int *option);
void execError(FILE *out, FILE *fin, FILE *qin);
/* Initialization Functions						*/
void initSrv(server *srvs, int srvNo);
void initIds(quorum *qrm, int len);
void initMsg(message *msg);
void clrRand(int size, server *srvs);
void clrAck(int size, server *srvs);
/* Message Functions							*/
void printfMsg(message *msg);
void fprintfMsg(FILE *out, message *msg);
int sendProcd(FILE *out, char* type, int id, quorum *qrm, int qrmNo,
              int srvNo, message *wmsg, message *smsg, server *srvs);
void fillMsg(message *msg, message *smsg, char *type, int pid);
void sndMsg(int sock, message *msg, char *buf, char *type);
void strToMsg(char *token, message *msg);
void msgToStr(char *buf, message msg);
/* Quorum File Functions						*/
void readQrmFile();
int checkQrmFile(char *file, char * buf);
int returnServNum(char *file, char * buf, int cnt, char ltr1, char ltr2);
void fillInQrm(char *file, char *buf, quorum *qrm, int cnt);
/* Quorum Functions							*/
void sendToRdmSrv(FILE *out,int srvNo, char *buf, server *srvs,
                  message *wmsg, int *i);
int rcvAckFromQrm(FILE *out, char *buf, server *srvs, int srvNo,
                  message *smsg,message *wmsg, int qrmNo, quorum *qrm);
int checkQrmCmp(int qrmNo, quorum *qrm, server *srvs);
int findTotalSrvs(quorum qrm, int qrmCmp, server *srvs, tag_type maxTag,
                  int srvNo);
/* Tag Functions							*/
tag_type findMaxTag(FILE *out, int srvNo, server *srvs, message *wmsg,
                    message *smsg, quorum *qrm);
int cmpTag(tag_type tag1, tag_type tag2);
void tagCpy(tag_type* dest, tag_type src);
/************************************************************************/
/*				MAIN					*/

/************************************************************************/
main(int argc, char *argv[]) {
    /* Definitions and (some) Initializations				*/
    FILE* fin;          /* File descriptor of servers file		*/
    FILE* qin;          /* File descriptor of quorum system file	*/
    FILE* out;          /* File descriptor of output file		*/
    int id;               /* Reader's id                                  */
    int srvNo = 0;      /* Total number of servers			*/
    int qrmNo = 0;      /* Total number of quorums			*/
    int port = 0;       /* Server's port				*/
    int i = 0, j = 0;   /* Counters					*/
    char buf[BUFLEN];   /* Buffer					*/
    int option = 0;     /* User's option				*/
    int ready = 0;      /* Flag showing if writer is ready to make
			   a new request				*/
    message wmsg, smsg; /* Writer's and server's message		*/
    quorum *qrm;        /* Quorum System 				*/
    server *srvs;       /* Servers' information				*/
    tag_type maxTag;    /* Holds the maxTag found from a quorum		*/
    int qrmCmp = 0;     /* Flag showing if there is a complete quorum 	*/
    int opNo = 0;       /* In auto-procedure - Number of operations	*/
    int opCnt = 0;      /* Counter of operations                        */
    float opFreq = 0;   /* In auto-procedure - Frequency of operations	*/
    char *opFreqStr;	/* Frequency of operations converted to string	*/
    int cond = 0;       /* Condition - automatic or manual procedure	*/
    struct timeval tim;
    double start, end;  /* Time: start and end				*/
    double opLat = 0;   /* Operation Lattency				*/
    double avrLat=0.0;
    char outFile[FILENAME_LEN]; /* Name of output log file		*/
    int dummy;          /* Keep dummy numbers				*/
    int status;         /* Result from getaddrinfo()			*/
    struct addrinfo hints;      /* For getaddrinfo()			*/
    struct addrinfo *ipv4info;  /* Point to info for ipv4  		*/
    struct addrinfo *p; /* Counter				*/
    char prt[5];        /* Port converted to string		*/
    char ipstr[INET6_ADDRSTRLEN];/* IP converted to string		*/
    void *addr;         /* Holds the address			*/
    char *ipver;        /* Message showing the Internet Protocol*/
    /********************************************************************/
    /* Check if server's host name and port number are given 		*/
    if (argc != MAN && argc != AUTO) {
        printf("\nUsage of file (if auto-procedure give parameters of parenthesis):\
    \n%s <readerID> <serversFile> <quorumFile> (<operationsNo> <operationsFreq>)\n",
               argv[0]);
        exit(EXIT_FAILURE);
    }

    sprintf(outFile, "./log-sr%d.dat", atoi(argv[1]));

    /* Open/Create file to write                                          */
    if (!(out = fopen(outFile,"w"))){	/* Servers file			*/
        perror("fopen()");
        exit(1);
    }

    printf("\nStart sreader...\n");
    fprintf(out,"Start sreader...\n");
    sleep(5);
    printf("Analysing data...\n");
    fprintf(out,"Analysing data...\n");
    /* Check if automatic procedure and save new parameters		*/
    if (argc == AUTO) {
        opNo = atoi(argv[4]);
        opFreq = strtof(argv[5], &opFreqStr);
        printf("Total operations: %d\n", opNo);
        fprintf(out,"Total operations: %d\n", opNo);
        printf("Frequency of operations: %.2f\n", opFreq);
        fprintf(out,"Frequency of operations: %.2f\n", opFreq);
    }
    /* Open servers file & quorum file 					*/
    if (!(fin = fopen(argv[2], "r"))) { /* Servers file			*/
        perror("fopen()");
        exit(EXIT_FAILURE);
    }
    if (!(qin = fopen(argv[3], "r"))) { /* Quorums file			*/
        perror("fopen()");
        exit(EXIT_FAILURE);
    }

    /* Find total number of servers & quorums 				*/
    fscanf(fin, "%d", &srvNo);
    fscanf(qin, "%d", &qrmNo);

    //Initialization of file descriptor set
    FD_ZERO(&readfds);
    FD_ZERO(&crashfds);

    /* Create dynamically 2 tables for: servers' message and quorum struc*/
    qrm = (quorum *) malloc(qrmNo * sizeof (quorum));
    srvs = (server *) malloc(srvNo * sizeof (server));
    
    /* Initialize quorum ids, writer and servers message,tables with hosts*/
    initIds(qrm, qrmNo);
    initMsg(&wmsg);
    initMsg(&smsg);
    initSrv(srvs, srvNo);
    for (i=0; i<srvNo; i++)
        strcpy(srvs[i].hostname, "localhost");

    /* Clear flag tables						*/
    clrRand(srvNo, srvs);
    clrAck(srvNo, srvs);

    /* Print Information 						*/
    printf("Total servers: %d\n", srvNo);
    printf("Total quorums: %d\n", qrmNo);
    fprintf(out, "Total servers: %d\n", srvNo);
    fprintf(out, "Total quorums: %d\n", qrmNo);

    /* Check quorum file's format 					*/
    if (!checkQrmFile(argv[3], buf))
        execError(out, fin, qin);

    /* Fill in Quorum System - For each quorum:				*/
    for (i = 0; i < qrmNo; i++) {
        /* Save number of servers					*/
        qrm[i].servNum = returnServNum(argv[3], buf, i, 'Q', 'q');
        /* Create dynamically a table of servers id belonging in quorum	*/
        qrm[i].servers = (int *) malloc(qrm[i].servNum * sizeof (int));
        /* Fill in quorum's id and servers ids				*/
        fillInQrm(argv[3], buf, &qrm[i], i);
    }
    /* Print quorums and their servers					*/
    for (i = 0; i < qrmNo; i++) {
        printf("qrm=%d, servers= ", qrm[i].id);
        fprintf(out,"qrm=%d, servers= ", qrm[i].id);
        for (j = 0; j < qrm[i].servNum; j++){
            printf("%d ", qrm[i].servers[j]);
            fprintf(out,"%d ", qrm[i].servers[j]);
        }
        printf("\n");
        fprintf(out,"\n");
    }

    /* For each server 							*/
    for (i = 0; i < srvNo; i++) {
        /* Find server's id, port, hostname				*/
        fscanf(fin, "%d %d %s%d", &(srvs[i].id), &port, srvs[i].hostname, &dummy);

        /* Create socket - Fill in the table sock[] 			*/
        //printf("id=%d,port=%d,hostname=%s,dummy=%d\n",
        //    srvs[i].id,port,srvs[i].hostname,dummy);

        memset(&hints, 0, sizeof hints); /* Initialise struct to zero   */
        hints.ai_family = AF_UNSPEC; /* Either IPv4 or IPv6		*/
        hints.ai_socktype = SOCK_STREAM; /* TCP stream sockets		*/
        sprintf(prt, "%d", port);

        /* Get server's information					*/
        if ((status = getaddrinfo(srvs[i].hostname, prt, &hints, &srvs[i].serv)) != 0) {
            fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
            exit(1);
        }
        /* srvs[i].serv now points to a linked list of 1 or more struct addrinfos*/
        /* For each struct in the linked list				*/
        for (p = srvs[i].serv; p != NULL; p = p->ai_next) {
            if (p->ai_family == AF_INET) { // IPv4
                struct sockaddr_in *ipv4 = (struct sockaddr_in *) p->ai_addr;
                addr = &(ipv4->sin_addr);
                ipver = "IPv4";
                ipv4info = p;
            }
            else { // IPv6
                struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *) p->ai_addr;
                addr = &(ipv6->sin6_addr);
                ipver = "IPv6";
            }

            /* Convert the IP to a string and print it                   */
            inet_ntop(p->ai_family, addr, ipstr, sizeof ipstr);
            printf("  %s: %s\n", ipver, ipstr);
            fprintf(out,"  %s: %s\n", ipver, ipstr);

            /* Request connection					 */
            if ((srvs[i].sock = socket(p->ai_family, p->ai_socktype,
                                       p->ai_protocol)) < 0) {
                perror("socket()");
            }
            if (connect(srvs[i].sock, p->ai_addr, p->ai_addrlen) < 0) {
                perror("connect()");
                close(srvs[i].sock);
                continue;
            }
            srvs[i].serv = p;
            printf("Requested connection established to host %s - port %d - socket %d\n",
                   srvs[i].hostname, port, srvs[i].sock);
            fprintf(out, "Requested connection established to host %s - port %d - socket %d\n",
                    srvs[i].hostname, port, srvs[i].sock);
            FD_SET(srvs[i].sock, &readfds);
        }
    } /* End of for each server						*/
    printf("\n");
    //fprintf(out, "\n");
    fclose(fin);        /* Close fd to servers file			*/
    fclose(qin);        /* Close fd to quorums file			*/
    ready = 1;          /* Writer is ready to make a request            */
    
    if (argc == AUTO)   /* If automatic procedure			*/
        cond = opNo + 1;/* -> While number of operations+1 for exit	*/
    else                /* If manual procedure				*/
        cond = 1;       /* -> Endless while				*/
    
    while (cond) {
        if (argc == AUTO) { /* If automatic procedure			*/
            cond--;         /* -> Dicrease counter                      */
            option = 1;
            if (!cond)      /* extra round => time to exit		*/
                option = 2;
            usleep(opFreq * MILLION);
        }
        else {
            printMenu(&option); /* Print Menu				*/
        }

        switch (option) {
        case 1:             /* Option 1: Read the object 		*/
            /* Ensure that a request can happen only
             * if previous one has finished */
            if (ready == 0) {
                printf("Last read request hasn't finished\n");
                fprintf(out, "Last read request hasn't finished\n");
                cond++;
                continue;
            }
            ready = 0;      /* This request hasn't finished		*/
            gettimeofday(&tim, NULL); /* Current Time - 1st			*/
            start=tim.tv_sec+(tim.tv_usec/1000000.0);
            opCnt++;
            printf("R-Operation No.: %d\n", opCnt);
            fprintf(out,"R-Operation No.: %d\n", opCnt);
            /* 1st RTT: Send a READ message to all servers		*/
            qrmCmp = sendProcd(out,READ, atoi(argv[1]), qrm, qrmNo, srvNo,
                               &wmsg, &smsg, srvs);
            maxTag = findMaxTag(out, srvNo, srvs, &wmsg, &smsg, &(qrm[qrmCmp]));

            /* 2nd RTT: Send a WRITE message to all servers		*/
            qrmCmp = sendProcd(out,WRITE, atoi(argv[1]), qrm, qrmNo, srvNo,
                               &wmsg, &smsg, srvs);
            maxTag = findMaxTag(out, srvNo, srvs, &wmsg, &smsg, &(qrm[qrmCmp]));

            ready = 1;          /* This request has now finished	*/
            gettimeofday(&tim, NULL); /* Current Time - 2nd		*/
            end=tim.tv_sec+(tim.tv_usec/1000000.0);
            opLat=end-start;
            avrLat+=opLat;
            printf("Operation Lattency = %.4f seconds\n", opLat);
            printf("***********************************************************\n");
            //printf("***********************************************************\n");
            fprintf(out, "Operation Lattency = %.4f seconds\n", opLat);
            fprintf(out, "***********************************************************\n");
            //fprintf(out, "***********************************************************\n");
            fprintf(out,"Current Average Lat = %.4f seconds\n", avrLat/opCnt);
            break;
        case 2: /* Option 3: Exit 			*/
            for (i = 0; i < srvNo; i++) {
                sndMsg(srvs[i].sock, &wmsg, buf, EXIT);
            }
            printf("Total number of operations = %d\n", opNo);
            printf("Average Lattency = %.4f seconds \n",avrLat/opNo);
            printf("Exiting...\n\n");

            fprintf(out, "Total number of operations = %d\n", opNo);
            fprintf(out, "Average Lattency = %.4f seconds \n",avrLat/opNo);
            fprintf(out, "Exiting...\n\n");
            exit(EXIT_FAILURE);
            break;
        } /* End of switch(option)					*/
    } /* End of while(cond)						*/
    fclose(out);
    return;
} /* End of main()							*/
/************************************************************************/

/* Print Menu 								*/
void printMenu(int *option) {
    do {
        printf("\n");
        printf("  ********\n");
        printf("  * Menu *\n");
        printf("  ********\n");
        printf("1. Read an object\n");
        printf("2. Exit\n");
        printf("Option: ");
        scanf("%d", option);
        printf("\n");

    }
    while (*option < MINOP || *option > MAXOP);
} /* End of printMenu()							*/
/************************************************************************/

/* Print error message, close files and exit				*/
void execError(FILE *out, FILE *fin, FILE *qin){
  printf("Wrong format in the quorum system file.\n");
  printf("Please correct the file!\n\nExiting...\n\n");
  fprintf(out, "Wrong format in the quorum system file.\n");
  fprintf(out, "Please correct the file!\n\nExiting...\n\n");
  fclose(fin);
  fclose(qin);
  exit(EXIT_FAILURE);
} /* End of execError()							*/
/************************************************************************/

/* Initialize structure server						*/
void initSrv(server *srvs, int srvNo) {
    int i;
    for (i = 0; i < srvNo; i++) {
        srvs[i].id = 0;
        srvs[i].rand = 0;
        srvs[i].ack = 0;
        initMsg(&srvs[i].msg);
    }
} /* End of initSrv()							*/
/************************************************************************/

/* Initialize table							*/
void initIds(quorum *qrm, int len) {
    int i;
    for (i = 0; i < len; i++)
        qrm[i].id = 0;
} /* End of initIds()							*/
/************************************************************************/

/* Set all servers rand to 0 (means: they haven't been chosen) 		*/
void clrRand(int size, server *srvs) {
    int i;
    for (i = 0; i < size; i++)
        srvs[i].rand = 0;
} /* End of clrRand()							*/
/************************************************************************/

/* Set all servers Ack to 0 (means: they haven't replied) 		*/
void clrAck(int size, server *srvs) {
    int i;
    for (i = 0; i < size; i++)
        srvs[i].ack = 0;
} /* End of clrAck()							*/
/************************************************************************/

/* Initialize message 							*/
void initMsg(message *msg) {
    strcpy(msg->type, "\0");
    msg->pid = 0;
    (msg->tag).ts = 0;
    (msg->tag).wid = 0;
    msg->value = -1;
    msg->reqNo = 0;
} /* End of initMsg()							*/
/************************************************************************/

/* Print message contents 						*/
void printfMsg(message *msg) {
    printf("(type,pid,<ts,wid>,value,req)\t(%s,%d,<%d,%d>,%d,%d)\n",
           msg->type, msg->pid, (msg->tag).ts, (msg->tag).wid, msg->value, msg->reqNo);
} /* End of printfMsg()							*/
/************************************************************************/

/* Print message contents in a file                                     */
void fprintfMsg(FILE *fout, message *msg){
 fprintf(fout, "(type,<ts,wid>,value,req)\t(%s,<%d,%d>,%d,%d)\n",
         msg->type,(msg->tag).ts,(msg->tag).wid,msg->value,msg->reqNo);
} /* End of fprintfMsg()						*/
/************************************************************************/

/* Fill in the fields of the message 					*/
void fillMsg(message *msg, message *smsg, char *type, int pid) {
    strcpy(msg->type, type);
    msg->reqNo += 1;
    msg->pid = pid;
    (msg->tag).ts = (smsg->tag).ts;
    (msg->tag).wid = (smsg->tag).wid;
    msg->value = smsg->value;
} /* End of fillMsg()							*/
/************************************************************************/

/* Send Message to servers 						*/
void sndMsg(int sock, message *msg, char *buf, char *type) {
    bzero(buf, strlen(buf));                /* Initialize buffer 	*/
    if (!strcmp(type, EXIT)) {              /* If msg is EXIT           */
        strcpy(buf, EXIT);                  /* -> save only typed	*/
    }
    else                                    /* Else			*/
        msgToStr(buf, *msg);                /* ->save the whole msg     */
    
    if (send(sock,buf,BUFLEN,0) < 0)
        perror("send()");
} /* End of sndMsg()							*/
/************************************************************************/

/* Transform string to the structure message 				*/
void strToMsg(char *token, message *msg) {
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

/* Proceed with sending the message					*/
int sendProcd(FILE *out, char* type, int id, quorum *qrm, int qrmNo,
              int srvNo, message *wmsg, message *smsg, server *srvs) {
    int qrmCmp;
    int i;
    char buf[BUFLEN];   /* Buffer					*/
    float waitTime = 0; /* Random time to wait before sending a message */
    time_t start, end;  /* Time: start and end				*/
    double dif = 0;     /* Difference of time				*/

    bzero(buf, strlen(buf));                /* Initialize buffer 	*/
    fillMsg(wmsg, smsg, type, id);
    qrmCmp = 0;
    i = 0;
    do { /* Do - while qrm is not complete				*/
        if (i != srvNo)
            /* Send msg to random server				*/
            sendToRdmSrv(out, srvNo, buf, srvs, wmsg, &i);
        /* Wait random number between [0.3-0.7)				*/
        /*waitTime = 10 * (0.3 + (float) rand() / RAND_MAX * 0.4);
        time(&start);               // Current Time - 1st
        printf("Waiting random time(%d seconds)...", (int) waitTime);
        printf("...And listening for ACKs\n");
        */
        //do { /* Do - while (end time - start time) < waitTime		*/
            qrmCmp = rcvAckFromQrm(out,buf, srvs, srvNo, smsg, wmsg, qrmNo, qrm);
            if (qrmCmp != -1)       /* If a quorum is complete		*/
                break;              /* Exit				*/
            //time(&end);             /* Current Time - 2nd		*/
            //dif = difftime(end, start); /* Difference between two times*/
      //  }while (dif < waitTime);
        if (qrmCmp != -1) {         /* If a qrm is complete		*/
            //printf("QUORUM COMPLETE Q%d = ", qrm[qrmCmp].id);
            //fprintf(out,"QUORUM COMPLETE Q%d = ", qrm[qrmCmp].id);
            /*for (i = 0; i < qrm[qrmCmp].servNum; i++){
                printf("%d ", qrm[qrmCmp].servers[i]);
                fprintf(out, "%d ", qrm[qrmCmp].servers[i]);
                printf("\n");
                fprintf(out,"\n");
            }*/
        }
    }
    while (qrmCmp == -1);

    clrRand(srvNo, srvs);
    clrAck(srvNo, srvs);
    return qrmCmp;
} /* End of sendProcd()							*/
/************************************************************************/

/* Check if Quorum File's format is correct and save data		*
 * Return 1 if correct, otherwise 0 					*/
int checkQrmFile(char *file, char * buf) {
    FILE *qin;      /* File descriptor					*/
    int quor = 0;   /* Flag=1 if 'Q' appears in the quorum file		*/
    int inter = 0;  /* Flag=1 if 'I' appears in the quorum file   	*/
    char *token;    /* Usage: To keep part of the line from the file	*/
    int i = 0;      /* Counter						*/

    qin = fopen(file, "r");             /* Open quorum file		*/
    for (i = -1, fscanf(qin, "%s", buf); !feof(qin); fscanf(qin, "%s", buf)) {
        if (i == -1) {                  /* Ignore first line		*/
            i++;
            continue;
        }
        /* If No Quorum Line & No Intersection Line                     */
        if ((!quor) && (!inter)) {
            if (buf[0] == 'Q' || buf[0] == 'q') {   /* If Quorum Line	*/
                quor = 1;                           /* -> Set quor to 1	*/
                continue;
            }
            else                                    /* Else		*/
                return 0;                           /* ->ERROR in format*/
        }
        /* If Quorum Line & No Intersection Line                        */
        if (quor && !inter) { 
            if (buf[0] == 'I' || buf[0] == 'i') {   /* If Intersec Line */
                inter = 1;                          /* -> Set inter to 1*/
                continue;
            }
            else                                    /* Else		*/
                return 0;                           /* ->ERROR in format*/
        }
        /* If Quorum Line & Intersection Line                           */
        if (quor && inter) { 
            if (buf[0] == 'I' || buf[0] == 'i')     /* If Intersection Line*/
                continue;                           /* -> continue	*/
            if (buf[0] == 'Q' || buf[0] == 'q')     /* If Quorum Line	*/
                inter = 0;                          /* -> Set inter to 0*/
            else                                    /* Else		*/
                return 0;                           /* ->ERROR in format*/
        }
    } /* End of for each line of file					*/
    fclose(qin);
    return 1;
} /* End of checkQrmFile()						*/
/************************************************************************/

/* Based on a quorum file and a quorum id				*
   It returns how many servers belong to a spesific quorum 		*/
int returnServNum(char *file, char * buf, int cnt, char ltr1, char ltr2) {
    FILE *qin;      /* File descriptor					*/
    char *token;    /* Usage: To keep part of the line from the file	*/
    int quor = 0;   /* Flag = 1 if 'Q' appears in the quorum file 	*/
    int total = 0;  /* Total number of servers				*/
    int i = 0;      /* Counter						*/

    qin = fopen(file, "r");             /* Open quorum file		*/
    for (i = -1, fscanf(qin, "%s", buf); !feof(qin); fscanf(qin, "%s", buf)) {
        if (i == -1) {                  /* Ignore first line		*/
            i++;
            continue;
        }
        token = strtok(buf, "(");                   /* Parse line	*/
        if (token[0] == ltr1 || token[0] == ltr2)   /* Quorum Line	*/
            quor++;
        else
            continue;
        if (quor == cnt + 1)                        /* If spesific Quorum Line*/
            while ((token = strtok(NULL, ",")) != NULL) { /*Parse rest of line*/
                total++;
                i++;
            } /* End of If spesific Quorum Line				*/
    } /* End of for each line from file					*/
    fclose(qin);                        /* Close file			*/
    return total;                       /* Return number of servers	*/
} /* End of returnServNum()						*/
/************************************************************************/

/* Fill in the structure qrm with the data taken from the file		*/
void fillInQrm(char *file, char *buf, quorum *qrm, int cnt) {
    FILE *qin;      /* File descriptor					*/
    char *token;    /* Usage: To keep part of the line from the file	*/
    int i = 0, j = 0; /* Counters					*/
    int quor = 0;   /* Flag = 1 if 'Q' appears in the quorum file 	*/
    char temp;
    int id;

    qin = fopen(file, "r");

    for (i = 0, fscanf(qin, "%s", buf); !feof(qin); fscanf(qin, "%s", buf)) {
        if (!i) {   /* Ignore first line				*/
            i++;
            continue;
        }
        token = strtok(buf, "(");
        if (token[0] == 'Q' || token[0] == 'q') /* Quorum Line		*/
            quor++;

        if (quor == cnt + 1) {                  /* Specific Quorum Line */
            j = 0;
            sscanf(token, "%c%d", &temp, &id);
            qrm->id = id;
            while ((token = strtok(NULL, ",")) != NULL) { // token's not empty
                sscanf(token, "%d", &id);
                qrm->servers[j] = id;
                j++;
            }
            fclose(qin);
            return;
        } /* End of Quorum Line 					*/
    } /*End of scanning the file					*/
} /* End of fillInQrm()							*/
/************************************************************************/

/* Sends the message to a random server 				*/
void sendToRdmSrv(FILE *out, int srvNo, char *buf, server *srvs,
                  message *wmsg, int *i) {
    int num;            /* Random number representing the server	*/
    do {                /* do - while srvRand[num]==1			*/
        num = rand() % srvNo; /*Choose a random number between [0-srvNo)*/
    }while (srvs[num].rand);
    srvs[num].rand = 1;         /* Set flag to 1			*/
    (*i)++;

    if (!FD_ISSET(srvs[num].sock,&crashfds))
        /* Send msg to the random server found above 			*
        and wait random time  [0.3-0.7]					*/
        sndMsg(srvs[num].sock, wmsg, buf, READ);
} /* End of sendToRdmSrv()						*/

/************************************************************************/
int rcvAckFromQrm(FILE *out,char *buf, server *srvs, int srvNo,
                  message *smsg,message *wmsg, int qrmNo, quorum *qrm) {
    int i;                      /* Counter				*/
    struct timeval timeout;     /* Time for select() to wait 		*/
    char *token;                /* String to parse the message		*/

    timeout.tv_sec = 0;         /* Time to wait                         */
    timeout.tv_usec = 0;

    /* Select ready file descriptors (which have a message/answer)      */
    select(FD_SETSIZE, &readfds, NULL, NULL, &timeout);
    
    for (i = 0; i < srvNo; i++) {       /* For each server		*/
        if (FD_ISSET(srvs[i].sock, &readfds)) {/* If there's an answer	*/
            /* -> Start ACK process					*/
            /* Get a READACK message from a quorum 			*/
            bzero(buf, strlen(buf)); /* Initialize buffer		*/

            if (recv(srvs[i].sock,buf,BUFLEN,0) < 0){
                perror("recv()");
                continue;
            }
            
            if (!strlen(buf)){
                FD_CLR(srvs[i].sock, &readfds);
                FD_SET(srvs[i].sock, &crashfds);
                continue;
            }
            token = strtok(buf, ",");   /* Parse message		*/
            strcpy((srvs[i].msg).type, token); /* Save type		*/
            strToMsg(token, &(srvs[i].msg)); /* Save server's message	*/
            if ((srvs[i].msg).reqNo == wmsg->reqNo) { /* If Most recent ACK*/
                srvs[i].ack = 1;                      /* -> Set srvAck to 1*/
            }
        } /* End of if FD_ISSET						*/
        else if (!FD_ISSET(srvs[i].sock, &crashfds))
            FD_SET(srvs[i].sock, &readfds);
    } /* End of for each server						*/
    return checkQrmCmp(qrmNo, qrm, srvs);
} /* End of rcvAckFromQrm()						*/
/************************************************************************/

/* If there are ACKs from a complete quorum then return number of quorum*/
int checkQrmCmp(int qrmNo, quorum *qrm, server *srvs) {
    int i = 0, j = 0;
    int flag = -1;

    for (i = 0; i < qrmNo; i++) {       /* For each quorum		*/
        for (j = 0; j < qrm[i].servNum; j++) /* For each server of the quorum*/
            if (srvs[qrm[i].servers[j] - 1].ack == 1)/* If ack == 1 	*/
                flag = i;                            /* ->keep quorum id*/
            else {                                   /* Else		*/
                flag = -1;                           /* -> return FALSE	*/
                break;
            }
        if (flag != -1)
            return flag;
    }
    return -1;
} /* End of checkQrmCmp()						*/
/************************************************************************/

/* Finds the maximum/most recent tag among the servers' tags of a quorum*/
tag_type findMaxTag(FILE *out, int srvNo, server *srvs, message *wmsg,
                    message *smsg, quorum *qrm) {
    int i, j;           /* Counter					*/
    tag_type maxTag;    /* Max Tag					*/
    int cmp;            /* Flag for comparing 2 tags			*/
    int pos = 0;        /* Flag - position of server with max Tag	*/

    /* Initialise maxTag						*/
    maxTag.ts = 0;
    maxTag.wid = 0;

    for (i = 0; i < qrm->servNum; i++) {        /* For each server 	*/
        for (j = 0; j < srvNo; j++) {
            if (qrm->servers[i] == srvs[j].id && (srvs[i].msg).reqNo != 0) {
                cmp = cmpTag((srvs[j].msg).tag, maxTag);
                if (cmp == 1) {
                    tagCpy(&maxTag, (srvs[j].msg).tag);
                    pos = qrm->servers[i] - 1;
                } /* End of inner if cmp==1				*/
                strcpy(smsg->type, (srvs[pos].msg).type); //
                smsg->pid = (srvs[pos].msg).pid; //
                tagCpy(&smsg->tag, maxTag);
                smsg->value = (srvs[pos].msg).value;
                smsg->reqNo = (srvs[pos].msg).reqNo; //
            } /* End of if reqNo equal					*/
        } /* End of for each quorum					*/
    }
    //printf("MaxTag<ts,wid> = <%d,%d>\n",maxTag.ts, maxTag.wid);
    //printf("=> Final Message (saved locally) \n");
    //fprintf(out,"=> Final Message (saved locally) \n");
    //printfMsg(smsg);
    return maxTag;
} /* End of findMaxTag()						*/

/************************************************************************/
int findTotalSrvs(quorum qrm, int qrmCmp, server *srvs, tag_type maxTag,
                  int srvNo) {
    int i, j, k;        /* Counters					*/
    int cnt = 0;        /* Counter for servers having maxTag		*/

    for (i = 0; i < qrm.servNum; i++) {/* For each server of complete quorum*/
        for (j = i; j < srvNo; j++) {
            if (srvs[j].id == qrm.servers[i]
                    && (srvs[j].msg).reqNo != 0) { /*If reqNo != 0	*/

                /*printf("server=%d, tag=<%d,%d>, reqNo=%d\t", qrm.servers[i],
                       ((srvs[j].msg).tag).ts, ((srvs[j].msg).tag).wid,
                       (srvs[j].msg).reqNo);*/

                if (((srvs[j].msg).tag.ts == maxTag.ts) &&
                        ((srvs[j].msg).tag.wid == maxTag.wid)) {
                    cnt++;
                    //printf("tag = maxTag\n");
                    break;
                } /* End of inner if ts and wid equal  			*/
                else {
                    cnt = -1;
                    //printf("tag < maxTag\n");
                    break;
                } /* End of else					*/
            } /* End of if reqNo != 0					*/
        } /* End of for each server from total servers			*/
    } /* End of for each server from qrm				*/
    return cnt;
} /* End of findTotalSrvs()						*/
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

/* Copy src tag into dest tag 						*/
void tagCpy(tag_type* dest, tag_type src) {
    dest->ts = src.ts;                      /* -> Save ts		*/
    dest->wid = src.wid;                    /* -> Save wid		*/
} /* End of msgCpy							*/
/************************************************************************/

/************************************************************************
 * Name & Surname: Maria Stylianou	ID: 1012147			*
 * Department: Computer Science of University of Cyprus			*
 * Last Update: 25-MAR-2011						*
 * Title of program: freader.c						*
 * 									*
 * Functionality:							*
 * It has an active role in the system; 				*
 * It reads a message, which the servers hold, in 1RTT or 2RTT,		*
 * depending on the Quorum Views:					*
 * qv1: All servers of the quorum have the maximum timestamp. - 1RTT	*
 * qv3: In at least one intersection of the quorum, 			*
	all servers have the maximum timestamp. - 2RTT (READ, WRITE)	*
 * qv2: If none of the above apply - 					*
	it goes recursive till it endsup in onother qv			*/
/************************************************************************
 *				READER					*
 ************************************************************************/
/* LIBRARIES ************************************************************/
#include <stdio.h>	/* For I/O					*/
#include <stdlib.h>	/* For exit() malloc() free()			*/
#include <string.h>	/* For strings - strcpy() strcmp() strcat()	*/
#include <unistd.h>	/* For close() 					*/
#include <sys/socket.h>	/* For sockets - socket() bind() listen() 	*
					  accept() connect() shutdown() *
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
typedef struct{
  int ts;		/* Timestamp 					*/
  int wid;		/* Writer's id 					*/
}tag_type;

/* Structure for message 						*/
typedef struct{
  char type[SIZE]; 	/* Type of message: WRITE, READ			*/
  int pid;		/* Process id 					*/
  tag_type tag;	   	/* Tag: timestamp and writer's id 		*/
  int value;   		/* Object's value 				*/
  int reqNo;	   	/* Number of request 				*/
}message;

/* Structure for server							*/
typedef struct{
  int id;		/* Server's Id					*/
  int sock;		/* Socket file descriptor			*/
  char hostname[RANGE];	/* Server's hostname				*/
  struct addrinfo *serv;/* Server                                       */
  int rand;		/* Flag showing if it was chosen randomly	*/
  int ack;		/* Flag showing if it sent ACK			*/
  message msg;		/* Mesage					*/
}server;

/* Structure for quorum							*/
typedef struct{
  int id;		/* Quorum's id					*/
  int interId;		/* Intersected quorum's id			*/
  int servNum;		/* Number of servers				*/
  int *servers;		/* Table with Servers' names			*/
}quorum;
/* FUNCTIONS ************************************************************/
/* Print Functions							*/
void printMenu(int *option);
void execError(FILE *fin, FILE *qin, FILE *out);
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
void sndMsg(int sock, message *msg, char *buf,char *type);
void strToMsg(char *token, message *msg);
void msgToStr(char *buf, message msg);
int checkValid(FILE *fout, message *smsg, message *msg);
/* Quorum File Functions						*/
void readQrmFile();
int checkQrmFile(char *file, char * buf);
int returnIntsec(char *file, char * buf);
int returnServNum(char *file, char * buf, int cnt, char ltr1, char ltr2);
void fillInQrm(char *file, char *buf, quorum *qrm, int cnt, char ltr1,
	       char ltr2);
/* Quorum Functions							*/
void sendToRdmSrv(FILE *out,int srvNo, char *buf, server *srvs,
		  message *wmsg, int *i);
int rcvAckFromQrm(FILE *out,char *buf, server *srvs, int srvNo, message *smsg,
		  message *wmsg, int qrmNo, quorum *qrm);
int checkQrmCmp(int qrmNo, quorum *qrm, server *srvs);
int findQuorumView(quorum qrm,  int qrmCmp, quorum *intsec, server *srvs,
		   int intsecNo, int srvNo, int reqNo, int option,
		   FILE *out, tag_type *maxTag, int *iter);
int findTotalSrvs(quorum qrm, int qrmCmp, server *srvs, tag_type maxTag,
		  int srvNo);
/* Tag Functions							*/
tag_type findMaxTag(int srvNo, server *srvs, message *wmsg, message *smsg,
		quorum *qrm);
int cmpTag(tag_type tag1, tag_type tag2);
void tagCpy(tag_type* dest, tag_type src);
/************************************************************************/
/*				MAIN					*/
/************************************************************************/
main (int argc, char *argv[]){
  /* Definitions and (some) Initializations				*/
  FILE* fin;		/* File descriptor of servers file		*/
  FILE* qin;		/* File descriptor of quorum system file	*/
  FILE* out;		/* File descriptor of output file		*/
  int id;               /* Reader's id                                  */
  int srvNo=0;		/* Total number of servers			*/
  int qrmNo=0;		/* Total number of quorums			*/
  int intsecNo=0;	/* Total number of intersections		*/
  int port=0;		/* Server's port				*/
  int i=0,j=0;		/* Counters					*/
  char buf[BUFLEN];	/* Buffer					*/
  int option=0;		/* User's option				*/
  int ready=0;		/* Flag showing if writer is ready to make	*
			   a new request				*/
  message wmsg,smsg;	/* Writer's and server's message		*/
  quorum *qrm;    	/* Quorum System 				*/
  quorum *intsec;	/* Intersections 				*/
  server *srvs;		/* Servers' information				*/
  tag_type maxTag;	/* Holds the maxTag found from a quorum		*/
  int qrmCmp=0;		/* Flag showing if there is a complete quorum 	*/
  int qv;		/* Flag for quorum view				*/
  int opNo=0;		/* In auto-procedure - Number of operations	*/
  int opCnt=0;          /* Counter of operations                        */
  float opFreq=0;	/* In auto-procedure - Frequency of operations	*/
  char *opFreqStr;	/* Frequency of operations converted to string	*/
  int cond=0;		/* Condition - automatic or manual procedure	*/
  int fast=0;		/* Counts the fast operations			*/
  int slow=0;		/* Counts the slow operations			*/
  float fastPrc=0;	/* Percentage of fast operations		*/
  float slowPrc=0;	/* Percentage of slow operations		*/
  int opTotal=0;	/* Total number of operations			*/
  struct timeval tim;
  double start,end;	/* Time: start and end				*/
  double opLat=0.0;	/* Operation Lattency				*/
  double avrLat=0.0;
  int iter=0;		/* Number of iterations to find the final QV	*/
  char outFile[FILENAME_LEN]; /* Name of output log file		*/
  int dummy;		/* Keep dummy numbers				*/
  int status;		/* Result from getaddrinfo()			*/
  struct addrinfo hints;/* For getaddrinfo()				*/
  struct addrinfo *ipv4info;/* Point to info for ipv4  			*/
  struct addrinfo *p;	/* Counter					*/
  char prt[5];		/* Port converted to string			*/
  char ipstr[INET6_ADDRSTRLEN];	/* IP converted to string		*/
  void *addr;		/* Holds the address				*/
  char *ipver;		/* Message showing the Internet Protocol	*/
  /**********************************************************************/
  /* Check if server's host name and port number are given 		*/
  if (argc!=MAN && argc!=AUTO){
     printf("\nUsage of file (if auto-procedure give parameters of parenthesis):\
    \n%s <readerID> <serversFile> <quorumFile> (<operationsNo> <operationsFreq>)\n",argv[0]);
    exit(EXIT_FAILURE);
  }
  id=atoi(argv[1]);
  sprintf(outFile, "./log-fr%d.dat", id);

  /* Open/Create file to write                                          */
  if (!(out = fopen(outFile,"w"))){	/* Servers file			*/
    printf("freader: id=%d => ",id);
    perror("fopen()");
    exit(1);
  }

  printf("\nStart freader...\n");
  fprintf(out,"Start freader...\n");
  sleep(5);
  printf("Analysing data...\n");
  fprintf(out,"Analysing data...\n");
  /* Check if automatic procedure and save new parameters		*/
  if (argc==AUTO){
    opNo = atoi(argv[4]);
    opFreq = strtof(argv[5], &opFreqStr);
    printf("Total operations: %d\n", opNo);
    fprintf(out,"Total operations: %d\n", opNo);
    printf("Frequency of operations: %.2f\n", opFreq);
    fprintf(out,"Frequency of operations: %.2f\n", opFreq);
  }
  /* Open servers file & quorum file 					*/
  if (!(fin = fopen(argv[2],"r"))){	/* Servers file			*/
    printf("freader: id=%d => ",id);
    perror("fopen()");
    exit(EXIT_FAILURE);
  }
  if (!(qin = fopen(argv[3],"r"))){	/* Quorums file			*/
    printf("freader: id=%d => ",id);
    perror("fopen()");
    exit(EXIT_FAILURE);
  }

  /* Find total number of servers & quorums 				*/
  fscanf(fin,"%d",&srvNo);
  fscanf(qin,"%d",&qrmNo);

  /* Create dynamically 2 tables for: servers' message and quorum struc */
  qrm=(quorum *)malloc(qrmNo * sizeof(quorum));
  srvs=(server *)malloc(srvNo * sizeof(server));

  /* Initialize quorum ids, writer and servers message,tables with hosts*/
  initIds(qrm, qrmNo);
  initMsg(&wmsg);
  initMsg(&smsg);
  initSrv(srvs,srvNo);
  for (i=0; i<srvNo; i++)
    strcpy(srvs[i].hostname, "localhost");

  //Initialization of file descriptor set
    FD_ZERO(&readfds);
    FD_ZERO(&crashfds);
    
  /* Clear flag tables							*/
  clrRand(srvNo, srvs);
  clrAck(srvNo, srvs);

  /* Print Information 							*/
  printf("Total servers: %d\n",srvNo);
  fprintf(out, "Total servers: %d\n",srvNo);
  printf("Total quorums: %d\n",qrmNo);
  fprintf(out, "Total quorums: %d\n",qrmNo);

  /* Check quorum file's format 					*/
  if (!checkQrmFile(argv[3], buf))
    execError(fin, qin, out);

  intsecNo=returnIntsec(argv[3], buf);	/* Find total No of inters	*/
  printf("Total intersections: %d\n",intsecNo);
  fprintf(out,"Total intersections: %d\n",intsecNo);
  /* Create dynamically 1 table for intersections			*/
  intsec=(quorum *)malloc(intsecNo * sizeof(quorum));
  for (i=0; i<intsecNo; i++){		/* Fore each intersection	*/
    /* Find total number of servers					*/
    intsec[i].servNum=returnServNum(argv[3],buf,i,'I','i');
    /* Create dynamically 1 table of servers ids belonging to inters	*/
    intsec[i].servers=(int *)malloc(intsec[i].servNum*sizeof(int));
    /* Fill in intersection's id and servers ids			*/
    fillInQrm(argv[3],buf,&intsec[i],i,'I','i');
  }
  /* Fill in Quorum System - For each quorum:				*/
  for (i=0; i<qrmNo; i++){
    /* Save number of servers						*/
    qrm[i].servNum=returnServNum(argv[3], buf, i, 'Q', 'q');
    /* Create dynamically a table of servers id belonging in quorum	*/
    qrm[i].servers=(int *)malloc(qrm[i].servNum * sizeof(int));
    /* Fill in quorum's id and servers ids				*/
    fillInQrm(argv[3], buf, &qrm[i], i,'Q','q');
  }

  /* For each server 							*/
  for (i=0; i<srvNo; i++){
    /* Find server's id, port, hostname					*/
    fscanf(fin,"%d %d %s %d",&(srvs[i].id), &port, srvs[i].hostname,&dummy);

    memset(&hints, 0, sizeof hints); /* Initialise struct hints to zero	*/
    hints.ai_family = AF_UNSPEC;     /* Either IPv4 or IPv6		*/
    hints.ai_socktype = SOCK_STREAM; /* TCP stream sockets		*/
    sprintf(prt,"%d",port);

    /* Get server's information						*/
    if ((status = getaddrinfo(srvs[i].hostname, prt, &hints, &srvs[i].serv)) != 0) {
      perror("getaddrinfo()");
      fprintf(stderr, "getaddrinfo(): %s\n", gai_strerror(status));
      exit(1);
    }
    /* srvs[i].serv now points to a linked list of 1 or more struct addrinfos*/
    /* For each struct in the linked list				*/
    for(p = srvs[i].serv; p != NULL; p = p->ai_next) {
      if (p->ai_family == AF_INET) { // IPv4
	struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
	addr = &(ipv4->sin_addr);
	ipver = "IPv4";
	ipv4info=p;
      }
      else { // IPv6
	struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)p->ai_addr;
	addr = &(ipv6->sin6_addr);
	ipver = "IPv6";
      }

      /* Convert the IP to a string and print it			*/
      inet_ntop(p->ai_family, addr, ipstr, sizeof ipstr);
      printf("  %s: %s\n", ipver, ipstr);
      fprintf(out,"  %s: %s\n", ipver, ipstr);

      /* Request connection						*/
      if ((srvs[i].sock = socket(p->ai_family, p->ai_socktype,
                                 p->ai_protocol)) < 0){
	perror("socket()");
      }
      
      if (connect(srvs[i].sock, p->ai_addr,p->ai_addrlen) < 0){
        perror("connect()");
	close(srvs[i].sock);
	continue;
      }
      srvs[i].serv=p;
      printf("Requested connection established to host %s - port %d - socket %d\n",
	      srvs[i].hostname, port, srvs[i].sock);
      fprintf(out,"Requested connection established to host %s - port %d - socket %d\n",
		    srvs[i].hostname, port, srvs[i].sock);
      FD_SET(srvs[i].sock, &readfds);
    }
  } /* End of for each server						*/
  fclose(fin);		/* Close fd to servers file			*/
  fclose(qin);		/* Close fd to quorums file			*/
  ready=1;
  
  if (argc==AUTO)	/* If automatic procedure			*/
    cond=opNo+1;	/* -> While number of operations+1 for exit	*/
  else			/* If manual procedure				*/
    cond=1;		/* -> Endless while				*/
  
  while (cond){
    if (argc==AUTO){	/* If automatic procedure			*/
      cond--;		/* -> Dicrease counter				*/
      option=1;
      if (!cond)	/* extra round => time to exit			*/
	option=2;
      usleep(opFreq*MILLION);
    }
    else{
      printMenu(&option);/* Print Menu					*/
    }
    switch(option){
      case 1: 		/* Option 1: Read the object 			*/
      /* Check to ensure that a request can happen if previous one
      has finished */
      if (ready==0){
	printf("Last read request hasn't finished\n");
	fprintf(out,"Last read request hasn't finished\n");
	cond++;			/* Increade operations for not loosing			   the round				*/
	continue;
      }
      ready=0; 			/* This request hasn't finished		*/
      gettimeofday(&tim, NULL); /* Current Time - 1st			*/
      start=tim.tv_sec+(tim.tv_usec/1000000.0);
      opCnt++;
      printf("R-Operation No.: %d\n",opCnt);
      fprintf(out, "R-Operation No.: %d\n",opCnt);
      /* 1st RTT: Send a READ message to all servers			*/
      qrmCmp = sendProcd(out, READ, atoi(argv[1]), qrm, qrmNo, srvNo,
		&wmsg, &smsg, srvs);
      iter=0;
      do{
	maxTag=findMaxTag(srvNo, srvs, &wmsg, &smsg, &(qrm[qrmCmp]));

	qv=findQuorumView(qrm[qrmCmp], qrmCmp, intsec, srvs, intsecNo, srvNo,
			  wmsg.reqNo, option, out, &maxTag, &iter);
	//printf("***********************************************************\n");
        //fprintf(out, "***********************************************************\n");
        switch(qv){
	  case -1: printf("Detected: NaN, Something went wrong.\n");
	  fprintf(out,"Detected: NaN, Something went wrong.\n");
	break;
	case 1:		/* Quorum View 1				*/
	  printf("Detected: QV1, Iterations: %d\n", iter);
	  fast++;
	  break;
	case 2:		/* Quorum View 2				*/
	  for (j=0; j<qrm[qrmCmp].servNum; j++){
	    for (i=0; i<srvNo; i++){		/* For each server	*/
	      if (qrm[qrmCmp].servers[j]==srvs[i].id && (srvs[i].msg).reqNo!=0)
		if (((srvs[i].msg).tag).ts == maxTag.ts){
		  initMsg(&(srvs[i].msg));
		}// End of if ts equal
	    }// End of for all servers
	  }// End of for each server of the winner quorum
	  break;
	case 3:		/* Quorum View 3                                */
	 fprintf(out, "Detected: QV3, Iterations: %d\n", iter);
	  
	  /* 2nd RTT: Send a WRITE message to all servers		*/
	  qrmCmp = sendProcd(out, WRITE, atoi(argv[1]), qrm, qrmNo, srvNo,
		    &wmsg, &smsg, srvs);
	  slow++;
	  break;
      } /* End of switch(qv)						*/
     }while(qv==2);
     ready=1;			/* This request has now finished	*/
     gettimeofday(&tim, NULL);  /* Current Time - 2nd                   */
     end=tim.tv_sec+(tim.tv_usec/1000000.0); 
     opLat=end-start;
     avrLat+=opLat;
     printf("Operation Lattency = %.4f seconds\n", opLat);
     printf("***********************************************************\n");
     //printf("***********************************************************\n");
     fprintf(out, "Operation Lattency = %.4f seconds\n", opLat);
     fprintf(out,"Current Average Lat = %.4f seconds\n", avrLat/opCnt);
     fprintf(out, "***********************************************************\n");
     //fprintf(out, "***********************************************************\n");
     
     break;
      case 2: 			/* Option 3: Exit 			*/
      for (i=0; i<srvNo; i++){
	sndMsg(srvs[i].sock,&wmsg,buf,EXIT);
      }
      opTotal=fast+slow;
      fastPrc=(float) fast/opTotal;
      slowPrc=(float) slow/opTotal;
      printf("Total number of operations = %d\n", opTotal);
      printf("Average Lattency = %.4f seconds\n",avrLat/opTotal);
      printf("Fast operations(ratio) = %.2f\n", fastPrc);
      printf("Slow operations(ratio)= %.2f\n", slowPrc);
      printf("Exiting...\n\n");
      fprintf(out, "Total number of operations = %d\n", opTotal);
      fprintf(out, "Average Lattency = %.4f seconds\n",avrLat/opTotal);
      fprintf(out, "Fast operations(ratio) = %.2f\n", fastPrc);
      fprintf(out, "Slow operations(ratio) = %.2f\n", slowPrc);
      fprintf(out, "Exiting...\n\n");
      exit(EXIT_FAILURE);
      break;
    } /* End of switch(option)						*/
  } /* End of while(cond)						*/
  fclose(out);
  return;
} /* End of main()							*/
/************************************************************************/
/* Print Menu 								*/
void printMenu(int *option){
  do{
    printf("\n");
    printf("  ********\n");
    printf("  * Menu *\n");
    printf("  ********\n");
    printf("1. Read an object\n");
    printf("2. Exit\n");
    printf("Option: ");
    scanf("%d", option);
    printf("\n");

  }while(*option<MINOP || *option>MAXOP);
} /* End of printMenu()							*/
/************************************************************************/
/* Print error message, close files and exit				*/
void execError(FILE *fin, FILE *qin, FILE *out){
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
void initSrv(server *srvs, int srvNo){
  int i;
  for (i=0; i<srvNo; i++){
    srvs[i].id=0;
    srvs[i].rand=0;
    srvs[i].ack=0;
    initMsg(&srvs[i].msg);
  }
} /* End of initSrv()							*/
/************************************************************************/
/* Initialize table							*/
void initIds(quorum *qrm, int len){
  int i;
  for (i=0; i<len; i++)
    qrm[i].id=0;
} /* End of initIds()							*/
/************************************************************************/
/* Set all servers rand to 0 (means: they haven't been chosen) 		*/
void clrRand(int size, server *srvs){
  int i;
  for (i=0; i<size; i++)
    srvs[i].rand=0;
} /* End of clrRand()							*/
/************************************************************************/
/* Set all servers Ack to 0 (means: they haven't replied) 		*/
void clrAck(int size, server *srvs){
  int i;
  for (i=0; i<size; i++)
    srvs[i].ack=0;
} /* End of clrAck()							*/
/************************************************************************/
/* Initialize message 							*/
void initMsg(message *msg){
  strcpy(msg->type,"\0");
  msg->pid=0;
  (msg->tag).ts=0;
  (msg->tag).wid=0;
  msg->value=-1;
  msg->reqNo=0;
} /* End of initMsg()							*/
/************************************************************************/
/* Print message contents 						*/
void printfMsg(message *msg){
  printf("(type,pid,<ts,wid>,value,req)\t(%s,%d,<%d,%d>,%d,%d)\n",
	 msg->type,msg->pid,(msg->tag).ts,(msg->tag).wid,msg->value,msg->reqNo);
} /* End of printfMsg()							*/
/************************************************************************/
/* Print message contents in a file                                     */
void fprintfMsg(FILE *fout, message *msg){
 fprintf(fout, "(type,<ts,wid>,value,req)\t(%s,<%d,%d>,%d,%d)\n",
         msg->type,(msg->tag).ts,(msg->tag).wid,msg->value,msg->reqNo);
} /* End of fprintfMsg()						*/
/************************************************************************/
/* Fill in the fields of the message 					*/
void fillMsg(message *msg, message *smsg, char *type, int pid){
  strcpy(msg->type,type);
  msg->reqNo+=1;
  msg->pid=pid;
  (msg->tag).ts=(smsg->tag).ts;
  (msg->tag).wid=(smsg->tag).wid;
  msg->value=smsg->value;
} /* End of fillMsg()							*/
/************************************************************************/
/* Send Message to servers 						*/
void sndMsg(int sock, message *msg, char *buf, char *type){
  bzero(buf, strlen(buf));			/* Initialize buffer 	*/
  if (!strcmp(type, EXIT)){			/* If msg is EXIT	*/
    strcpy(buf,EXIT);				/* -> save only typed	*/
  }
  else						/* Else			*/
    msgToStr(buf, *msg);			/* ->save the whole msg */
   if (send(sock,buf,BUFLEN,0) < 0)
        perror("send()");
} /* End of sndMsg()							*/
/************************************************************************/
/* Transform string to the structure message 				*/
void strToMsg(char *token, message *msg){
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
void msgToStr(char *buf, message msg){
  sprintf(buf,"%s,%d,%d,%d,%d,%d\0",msg.type,msg.pid,(msg.tag).ts,
	  (msg.tag).wid,msg.value,msg.reqNo);
} /* End of msgToStr()							*/
/************************************************************************/
/* Check if the message received is valid
   (request number is the most recent)
   Return 1 if it is valid. Return 0 otherwise 				*/
int checkValid(FILE *fout, message *smsg, message *msg) {
    if (smsg->reqNo == msg->reqNo) /* If most recent request	*/
        return 1;
    //printf("--- Ignore message - Request number is not the most recent\n");
    //fprintf(fout, "--- Ignore message - Request number is not the most recent\n");
    return 0;
} /* End of checkValid()						*/
/************************************************************************/
/* Proceed with sending the message					*/
int sendProcd(FILE *out, char* type, int id, quorum *qrm, int qrmNo,
	       int srvNo, message *wmsg, message *smsg, server *srvs){
  int qrmCmp;
  int i;
  char buf[BUFLEN];	/* Buffer					*/
 
  fillMsg(wmsg,smsg,type,id);
 
  qrmCmp=0; i=0;
  do{ /* Do - while qrm is not complete					*/
    if (i!=srvNo)
      /* Send msg to random server					*/
      sendToRdmSrv(out, srvNo,buf,srvs,wmsg,&i);
      qrmCmp=rcvAckFromQrm(out, buf, srvs, srvNo, smsg, wmsg, qrmNo, qrm);
     
      if (qrmCmp!=-1)		/* If a quorum is complete		*/
	break;			/* Exit					*/
 
  }while(qrmCmp==-1);

  clrRand(srvNo, srvs);
  clrAck(srvNo, srvs);
  return qrmCmp;
} /* End of sendProcd()							*/
/************************************************************************/
/* Check if Quorum File's format is correct and save data		*
 * Return 1 if correct, otherwise 0 					*/
int checkQrmFile(char *file, char * buf){
  FILE *qin;	/* File descriptor					*/
  int quor=0;    /* Flag=1 if 'Q' appears in the quorum file		*/
  int inter=0;   /* Flag=1 if 'I' appears in the quorum file   		*/
  char *token;   /* Usage: To keep part of the line from the file	*/
  int i=0;       /* Counter						*/

  qin = fopen(file,"r");		/* Open quorum file		*/
  for(i=-1,fscanf(qin,"%s", buf); !feof(qin); fscanf(qin,"%s",buf)){
    if (i==-1){ 			/* Ignore first line		*/
      i++;
      continue;
    }
    if ((!inter) && (!quor)){ /*If No Intersection Line & No Quorum Line*/
      if (buf[0]=='Q' || buf[0]=='q'){	/* If Quorum Line		*/
	quor=1;				/* -> Set quor to 1		*/
	continue;
      }
      else				/* Else				*/
	return 0;			/* -> ERROR in format		*/
    }
    if (quor && !inter){	/* If Quorum Line & No Intersection Line*/
      if (buf[0]=='I' || buf[0]=='i'){	/* If Intersection Line		*/
	inter=1;			/* -> Set inter to 1		*/
	continue;
      }
      else 				/* Else				*/
	return 0;			/* -> ERROR in format		*/
    }
    if (quor && inter){		/* If Quorum Line & Intersection Line 	*/
      if (buf[0]=='I' || buf[0]=='i')	/* If Intersection Line		*/
	continue;			/* -> continue			*/
      if (buf[0]=='Q' || buf[0]=='q')	/* If Quorum Line		*/
	inter=0;			/* -> Set inter to 0		*/
      else				/* Else				*/
	return 0;			/* -> ERROR in format		*/
    }
  } /* End of for each line of file					*/
  fclose(qin);
  return 1;
}  /* End of checkQrmFile()						*/
/************************************************************************/
/* It returns the total number of intersections				*/
int returnIntsec(char *file, char * buf){
  FILE *qin;			/* File descriptor			*/
  int inter=0;			/* Total number of intersections	*/

  qin = fopen(file,"r");
  for(fscanf(qin,"%s", buf); !feof(qin); fscanf(qin,"%s",buf)){
    if (buf[0]=='I' || buf[0]=='i')	// Quorum Line
      inter++;
  }
  return inter;
} /* End of returnIntsec()						*/
/************************************************************************/
/* Based on a quorum file and a quorum id				*
   It returns how many servers belong to a spesific quorum 		*/
int returnServNum(char *file, char * buf, int cnt, char ltr1, char ltr2){
  FILE *qin;	/* File descriptor					*/
  char *token;	/* Usage: To keep part of the line from the file	*/
  int quor=0;	/* Flag = 1 if 'Q' appears in the quorum file 		*/
  int total=0;	/* Total number of servers				*/
  int i=0;	/* Counter						*/

  qin = fopen(file,"r");		/* Open quorum file		*/
  for(i=-1,fscanf(qin,"%s", buf); !feof(qin); fscanf(qin,"%s",buf)){
    if (i==-1){				/* Ignore first line		*/
      i++;
      continue;
    }
    token = strtok(buf, "(");               /* Parse line		*/
    if (token[0]==ltr1 || token[0]==ltr2)   /* Quorum Line		*/
      quor++;
    else
      continue;
    if (quor==cnt+1)                 	/* If spesific Quorum Line	*/
      while((token = strtok(NULL, ","))!=NULL){ /*Parse the rest of line*/
	total++;
	i++;
      } /* End of If spesific Quorum Line				*/
  } /* End of for each line from file					*/
  fclose(qin);				/* Close file			*/
  return total;				/* Return number of servers	*/
} /* End of returnServNum()						*/
/************************************************************************/
/* Fill in the structure qrm with the data taken from the file		*/
void fillInQrm(char *file, char *buf, quorum *qrm, int cnt,
	       char ltr1, char ltr2){
  FILE *qin;	/* File descriptor					*/
  char *token;	/* Usage: To keep part of the line from the file	*/
  int i, j=0;	/* Counters						*/
  int quor=0;	/* Flag = 1 if 'Q' appears in the quorum file 		*/
  char temp[10];/* Dummy variable					*/
  char d1,d2;	/* Dummy variables					*/


  qin = fopen(file,"r");		/* Open quorum file		*/
  for(fscanf(qin,"%s", buf); !feof(qin); fscanf(qin,"%s",buf)){
    if (!i){ 				/* Ignore first line		*/
      i++;
      continue;
    }
    sscanf(buf,"%c",&d1);
    if ((d1=='I') || (d1=='i')){	/* If Intersection Line		*/
      /* -> Save quorum's id and intersection's id			*/
      sscanf(buf,"%c%d%c%d%s",&d1,&(qrm->id),&d2,&(qrm->interId),buf);
    }
    else if ((d1=='Q') || (d1=='q')){	/* If Quorum Line		*/
      /* -> Save quorum's id						*/
      sscanf(buf,"%c%d%c%s",&d1,&(qrm->id),&d2,buf);
    }
    if ((d1==ltr1) || (d1==ltr2))	/* If spesific Line		*/
      quor++;

    if (quor==cnt+1){                  	/* If spesific Line		*/
      j=0;
      token = strtok(buf, "(");		/* Parse Line			*/
      sscanf(token,"%d",&(qrm->servers[j])); /* Save server's id	*/
      token = strtok(buf, ",");

      while((token)!=NULL){ 		/* While token's not empty	*/
	sscanf(token,"%d",&(qrm->servers[j])); /* Continue saving ids	*/
	j++;
	token = strtok(NULL, ",");
      }
      fclose(qin);
      return;
    } /* End of if spesific line					*/
  } /* End of for (EOF)							*/
} /* End of fillInQrm()							*/
/************************************************************************/
/* Sends the message to a random server 				*/
void sendToRdmSrv(FILE *out, int srvNo, char *buf, server *srvs,
		  message *wmsg, int *i){
  int num;		/* Random number representing the server	*/
  do{ 			/* do - while srvRand[num]==1			*/
    num=rand()%srvNo;	/* Choose a random number between [0-srvNo)	*/
  }while(srvs[num].rand);
  srvs[num].rand=1;	/* Set flag to 1				*/
  (*i)++;

  if (!FD_ISSET(srvs[num].sock,&crashfds))
    /* Send msg to the random server found above 				*
    and wait random time  [0.3-0.7]					*/
    sndMsg(srvs[num].sock,wmsg,buf,READ);
} /* End of sendToRdmSrv()						*/
/************************************************************************/
int rcvAckFromQrm(FILE *out,char *buf, server *srvs, int srvNo, message *smsg,
		  message *wmsg, int qrmNo, quorum *qrm){
  int i;			/* Counters				*/
  struct timeval timeout;	/* Time for select() to wait 		*/
  char *token;  		/* String to parse the message		*/

  timeout.tv_sec = 0;			/* Time to wait			*/
  timeout.tv_usec = 0;

  /* Select ready file descriptors (which have a message/answer)       */
  select(FD_SETSIZE, &readfds, NULL, NULL, &timeout);

  for (i=0; i<srvNo; i++){		/* For each server		*/
    if (FD_ISSET(srvs[i].sock, &readfds)){/* If there's an answer	*/
      /* -> Start ACK process						*/
      /* Get a READACK message from a quorum 				*/
      bzero(buf, strlen(buf));    	/* Initialize buffer		*/
      if (recv(srvs[i].sock,buf,BUFLEN,0) < 0){
        perror("recv()");
        continue;
      }

      if (!strlen(buf)){
        FD_CLR(srvs[i].sock, &readfds);
        FD_SET(srvs[i].sock, &crashfds);
        continue;
      }
        token = strtok(buf, ",");		/* Parse message	*/
        strcpy((srvs[i].msg).type,token);	/* Save type		*/
        strToMsg(token,&(srvs[i].msg));	/* Save server's message	*/
        if (checkValid(out, &(srvs[i].msg), wmsg)) /* If Most recent ACK */
          srvs[i].ack=1;				/* -> Set srvAck to 1	*/
    } /* End of if FD_ISSET						*/
    else if (!FD_ISSET(srvs[i].sock, &crashfds))
            FD_SET(srvs[i].sock, &readfds);
  }  /* End of for each server						*/
  return checkQrmCmp(qrmNo,qrm, srvs);
} /* End of rcvAckFromQrm()						*/
/************************************************************************/
/* If there are ACKs from a complete quorum then return number of quorum*/
int checkQrmCmp(int qrmNo, quorum *qrm, server *srvs){
  int i=0, j=0;
  int flag=-1;

  for (i=0; i<qrmNo; i++){		/* For each quorum		*/
    for (j=0; j<qrm[i].servNum; j++){	/* For each server of the quorum*/
      if(srvs[qrm[i].servers[j]-1].ack==1)/* If ack == 1 		*/
	flag=i;				/* -> keep quorum id		*/
      else{				/* Else				*/
	flag=-1;			/* -> return FALSE		*/
	break;
      }
    }
    if (flag!=-1)
      return flag;
  }
  return -1;
} /* End of checkQrmCmp()						*/
/************************************************************************/
/* Finds the maximum/most recent tag among the servers' tags of a quorum*/
tag_type findMaxTag(int srvNo, server *srvs, message *wmsg, message *smsg,
		quorum *qrm){
  int i,j;		/* Counter					*/
  tag_type maxTag;	/* Max Tag					*/
  int cmp;		/* Flag for comparing 2 tags			*/
  int pos=0;		/* Flag - position of server with max Tag	*/

  /* Initialise maxTag							*/
  maxTag.ts=0;
  maxTag.wid=0;

  for (i=0; i<qrm->servNum; i++){		/* For each server 	*/
    for (j=0; j<srvNo; j++){
      if (qrm->servers[i] == srvs[j].id && (srvs[j].msg).reqNo!=0){
      cmp=cmpTag((srvs[j].msg).tag, maxTag);
	if (cmp==1){
	  tagCpy(&maxTag, (srvs[j].msg).tag);
	  pos=qrm->servers[i]-1;
	} /* End of inner if cmp==1					*/
	strcpy(smsg->type, (srvs[pos].msg).type); //
	smsg->pid=(srvs[pos].msg).pid;		 //
	tagCpy(&smsg->tag, maxTag);
	smsg->value=(srvs[pos].msg).value;
	smsg->reqNo=(srvs[pos].msg).reqNo;   //
      } /* End of if reqNo equal					*/
    } /* End of for each quorum						*/
  }
  return maxTag;
} /* End of findMaxTag()						*/
/************************************************************************/
/* Based on the complete quorum						*/
/* It returns the quorum view						*/
int findQuorumView(quorum qrm, int qrmCmp, quorum *intsec, server *srvs,
		   int intsecNo, int srvNo, int reqNo, int option,
		   FILE *out, tag_type *maxTag,int *iter){
  int i,j,k;		/* Pointers					*/
  int cnt=0;		/* Counter					*/
  int qv=-1;		/* Quorum View					*/
  int size=0;		/* size of complete quorum			*/

  /* Calculate size of complete quorum					*/
  for (i=0; i<qrm.servNum; i++){/* For each server from complete quorum	*/
    for (j=i; j<srvNo; j++){
      if (srvs[j].id == qrm.servers[i] && (srvs[j].msg).reqNo!=0){
	size++;
	break;
      }
    }
  }
  (*iter)++;
  /* Check for Quorum View 1						*/
  cnt=findTotalSrvs(qrm, qrmCmp, srvs, *maxTag, srvNo);
  if (cnt == size)	/* If all servers of quorum have maxTag		*/
    return 1;		/* -> Return quorum view 1			*/

  /* Check for Quorum View 3						*/
  for (k=0; k<intsecNo; k++){		/* For each intersection	*/
    if (intsec[k].id == qrm.id){
      cnt=0;
      size=intsec[k].servNum;
      cnt=findTotalSrvs(intsec[k], qrmCmp, srvs, *maxTag, srvNo);
      if (cnt == size){
	return 3;
      }
    } /* End of if intsec found						*/
   } /* End of for each intersection					*/
    return 2;	/* Quorum View 2					*/
} /* End of findQuorumView()						*/
/************************************************************************/

int findTotalSrvs(quorum qrm, int qrmCmp, server *srvs, tag_type maxTag,
		  int srvNo){
  int i,j,k;		/* Counter					*/
  int cnt=0;		/* Counter for servers having maxTag		*/

  for (i=0; i<qrm.servNum; i++){/* For each server of complete quorum	*/
    for (j=i; j<srvNo; j++){
      if (srvs[j].id==qrm.servers[i]
	  && (srvs[j].msg).reqNo!=0){ /*If reqNo != 0			*/

	  if (((srvs[j].msg).tag.ts == maxTag.ts) &&
	      ((srvs[j].msg).tag.wid == maxTag.wid)){
	    cnt++;
	    break;
	  } /* End of inner if ts and wid equal  			*/
	  else{
	    cnt=-1;
	    break;
	  } /* End of else						*/
      } /* End of if reqNo != 0						*/
    } /* End of for each server from total servers			*/
  } /* End of for each server from qrm					*/
    return cnt;
} /* End of findTotalSrvs()						*/
/************************************************************************/
/* Based on two tags
   Rerun 1 if tag1 > tag2, 0 if tag1 == tag2, -1 if tag1 < tag2		*/
int cmpTag(tag_type tag1, tag_type tag2){
  if (tag1.ts > tag2.ts)			/* If ts1 > ts2		*/
    return 1;					/* -> tag1 > tag2	*/
  if (tag1.ts == tag2.ts)    			/* If ts1 == ts2	*/
    if (tag1.wid > tag2.wid)			/*    If wid1 > wid2	*/
      return 1;					/*    -> tag1 > tag2	*/
    if (tag1.ts == tag2.ts)			/*    If ts1== ts2	*/
      return 0;					/*    -> tag1 = tag2	*/
  return -1;					/* -> tag1 < tag2	*/
} /* End of cmpTag()							*/
/************************************************************************/
/* Copy src tag into dest tag 						*/
void tagCpy(tag_type* dest, tag_type src){
  dest->ts = src.ts;			/* -> Save ts			*/
  dest->wid = src.wid;			/* -> Save wid			*/
} /* End of msgCpy							*/
/************************************************************************/

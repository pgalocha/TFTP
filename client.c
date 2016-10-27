//
//  client.c
//  Adapted by Christophe Soares & Pedro Sobral on 15/16
//
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <sys/time.h>
#include <time.h>
#define BUFSIZE 8096
#define operation 2

struct estrutura_tread
{
    char * ficheiro;
    int socketfd;
};



int pexit(char * msg)
{
    perror(msg);
    exit(1);
}

void callGet(char* fileName,char buffer[BUFSIZE],int sockfd,int filedesc,int i )
{

    sprintf(buffer, "get /%s", fileName);

    // Now the sockfd can be used to communicate to the server the GET request
    write(sockfd, buffer, strlen(buffer));

    filedesc = open(fileName, O_WRONLY | O_CREAT | O_TRUNC, 0777);

    while ((i = read(sockfd, buffer, BUFSIZE)) > 0)
        write(filedesc, buffer, i);

}


void* callGetThread(void * parameters)
{
    int filedesc,i;
    char buffer[BUFSIZE];
    struct estrutura_tread* p = (struct estrutura_tread*) parameters;

    printf("Ficheiro a ser buscado... %s \n",p->ficheiro);

    sprintf(buffer, "get /%s", p->ficheiro);

    write(p->socketfd, buffer, strlen(buffer));

    filedesc = open(p->ficheiro, O_WRONLY | O_CREAT | O_TRUNC, 0777);

    while ((i = read(p->socketfd, buffer, BUFSIZE)) > 0)
        write(filedesc, buffer, i);

}




char* mputFUNC(char* path)
{
    int file_fd;
    int i=0;
    long ret;
    DIR *dip;
    char*pasta=NULL;
    struct dirent *dit;


    static char buffer[BUFSIZE + 1]; /* static so zero filled */

    printf("LOG Header %s \n", path);
    //tries to open the path
    if((dip=opendir(path))==NULL)
    {
        sprintf(buffer, "ERRO! Nao existe esse diretorio\n");

    }
    //path is opened
    else
    {
        //first allocation
        pasta=(char*)realloc(pasta,1);
        strcpy(pasta,"");
        //loops through the content of the path
        while((dit=readdir(dip))!=NULL)
        {
            //checks if is a file
            if(dit->d_type==DT_REG)
            {
                pasta=(char*)realloc(pasta,strlen(pasta)+strlen(dit->d_name)+2);
                strcat(pasta,dit->d_name);
                printf("%s -> as\n",dit->d_name);
                strcat(pasta,"\n");

            }
        }
        printf("%s \n",pasta);
        //writes to the socket
        return pasta;
    }
}

int main(int argc, char *argv[])
{
    int i , sockfd, filedesc;
    long long ret = 0, offset = 0;
    char buffer[BUFSIZE];
    static struct sockaddr_in serv_addr;

    struct stat stat_buf; // argument to fstat

    char fileName[50];
   // int teste =1;


#if(operation==1)



        if (argc != 5)
        {
            printf("Usage: ./client <SERVER IP ADDRESS> <LISTENING PORT>\n");
            printf(
                "Example: ./client 127.0.0.1 8141 get remoteFileName\n./client 127.0.0.1 8141 [put/get] localFileName\n\n");
            exit(1);
        }
        if(argv[3]=="get" || argv[3]=="put")
        {
            printf(
                "client trying to connect to IP = %s PORT = %s method= %s for FILE= %s\n",
                argv[1], argv[2], argv[3], argv[4]);
        }
        strcpy(fileName, argv[4]);

        if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
            pexit("socket() failed");

        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
        serv_addr.sin_port = htons(atoi(argv[2]));

        // Connect tot he socket offered by the web server
        if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
            pexit("connect() failed");




        if (!strcmp(argv[3], "get"))
        {

            sprintf(buffer, "get /%s", fileName);

            // Now the sockfd can be used to communicate to the server the GET request
            write(sockfd, buffer, strlen(buffer));

            filedesc = open(fileName, O_WRONLY | O_CREAT | O_TRUNC, 0777);

            while ((i = read(sockfd, buffer, BUFSIZE)) > 0)
                write(filedesc, buffer, i);

        }
        else if (!strcmp(argv[3], "put"))
        {

            sprintf(buffer, "put /%s", fileName);
            printf("-> put /%s\n", fileName);

            write(sockfd, buffer, strlen(buffer));

            ret = read(sockfd, buffer, BUFSIZE); 	// read Web request in one go
            buffer[ret] = 0; 						// put a null at the end

            if (ret > 0)
            {
                printf("<- %s\n", buffer);
                if (!strcmp(buffer, "OK"))   // check if it is OK on the ftp server side
                {

                    // open the file to be sent
                    filedesc = open(fileName, O_RDWR);

                    // get the size of the file to be sent
                    fstat(filedesc, &stat_buf);

                    // Read data from file and send it
                    ret = 0;
                    while (1)
                    {
                        unsigned char buff[BUFSIZE] = { 0 };
                        int nread = read(filedesc, buff, BUFSIZE);
                        ret += nread;
                        printf("\nBytes read %d \n", nread);

                        // if read was success, send data.
                        if (nread > 0)
                        {
                            printf("Sending \n");
                            write(sockfd, buff, nread);
                        }

                        // either there was error, or we reached end of file.

                        if (nread < BUFSIZE)
                        {
                            if (ret == stat_buf.st_size)
                                printf("End of file\n");
                            else
                                printf("Error reading\n");
                            break;
                        }

                    }

                    if (ret == -1)
                    {
                        fprintf(stderr, "error sending the file\n");
                        exit(1);
                    }
                    if (ret != stat_buf.st_size)
                    {
                        fprintf(stderr,
                                "incomplete transfer when sending: %lld of %d bytes\n",
                                ret, (int) stat_buf.st_size);
                        exit(1);
                    }
                }
                else
                {
                    printf("ERROR on the server");
                }

                // close descriptor for file that was sent
                close(filedesc);

                // close socket descriptor
                close(sockfd);

            }

        }
        else if(!strcmp(argv[3], "ls"))
        {
            sprintf(buffer, "ls %s", fileName);
            //printf("-> ls %s\n", fileName);

            write(sockfd, buffer, strlen(buffer));

            ret = read(sockfd, buffer, BUFSIZE); 	// read Web request in one go
            printf("\n%s",buffer);
            buffer[ret] = 0;

        }
        else if(!strcmp(argv[3], "mget"))
        {
            clock_t start,end;
            float total_t;
            printf("Starting clock...\n");
            start=clock();
            sprintf(buffer, "ls %s", fileName);
            //printf("-> ls %s\n", fileName);

            write(sockfd, buffer, strlen(buffer));

            ret = read(sockfd, buffer, BUFSIZE); 	// read Web request in one go
            printf("\n%s",buffer);
            int z=0,s=0;
            char ** arrayString=(char**)malloc(sizeof(char*)*100);
            char* aux=NULL;
            *(arrayString+z)=(char*)malloc(sizeof(char*)*50);
            aux=strtok(buffer,"\n");
            strcpy(*(arrayString+z),aux);
            //printf("\n->%s",aux);
            z++;
            while(aux!=NULL)
            {

                aux=strtok(NULL,"\n");
                if(aux!=NULL)
                {
                    *(arrayString+z)=(char*)malloc(sizeof(char*)*50);
                    strcpy(*(arrayString+z),aux);
                    z++;

                }

            }
            pid_t pid;
            int sockfdAUX=0;
            int pids[z];
            printf("\n Numero de ficheiros a transferir: %d \n",z);
            for(s=0; s<z; s++)
            {
                printf(" Transferindo Ficheiro %s . . . \n",*(arrayString+s));

                pid=fork();

                if(pid==0)
                {
                    if ((sockfdAUX = socket(AF_INET, SOCK_STREAM, 0)) < 0)
                        pexit("socket() failed");

                    serv_addr.sin_family = AF_INET;
                    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
                    serv_addr.sin_port = htons(atoi(argv[2]));

                    // Connect tot he socket offered by the web server
                    if (connect(sockfdAUX, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
                        pexit("connect() failed");


                    callGet(*(arrayString+s),buffer,sockfdAUX,filedesc,i);
                    //execl("./client","./client","127.0.0.1","8181","get",*(arrayString+s));
                    //printf("Deu erro a carregar %s ! A terminar. . .\n",*(arrayString+s));
                    exit(0);

                }


            }
            int result;
            for(i=0; i<z; i++)
            {
                wait(NULL);
                //waitpid(pids[i],&result,0);

            }

            end=clock();

            total_t=((float)(end-start)) / CLOCKS_PER_SEC;
            printf(" Tempo de execucao : %f \n",total_t);

        }
        else if(!strcmp(argv[3], "mput"))
        {
            pid_t pidmput;
            char* teste=mputFUNC(".");

            int z=0,s=0;
            char ** arrayString=(char**)malloc(sizeof(char*)*100);
            char* aux=NULL;
            *(arrayString+z)=(char*)malloc(sizeof(char*)*50);
            aux=strtok(teste,"\n");
            strcpy(*(arrayString+z),aux);
            //printf("\n->%s",aux);
            z++;
            while(aux!=NULL)
            {
                aux=strtok(NULL,"\n");
                if(aux!=NULL)
                {
                    *(arrayString+z)=(char*)malloc(sizeof(char*)*50);
                    strcpy(*(arrayString+z),aux);
                    z++;

                }

            }
            int sockfdAUX=0;

            printf("\n Numero de ficheiros a transferir: %d \n",z);

            for(s=0; s<z; s++)
            {
                pidmput=fork();

                if(pidmput==0)
                {

                    if ((sockfdAUX = socket(AF_INET, SOCK_STREAM, 0)) < 0)
                        pexit("socket() failed");

                    serv_addr.sin_family = AF_INET;
                    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
                    serv_addr.sin_port = htons(atoi(argv[2]));

                    // Connect tot he socket offered by the web server
                    if (connect(sockfdAUX, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
                        pexit("connect() failed");

                    sprintf(buffer, "put /%s",*(arrayString+s));
                    printf("|| put /%s\n", *(arrayString+s));

                    write(sockfd, buffer, strlen(buffer));

                    ret = read(sockfd, buffer, BUFSIZE); 	// read Web request in one go
                    buffer[ret] = 0; 						// put a null at the end

                    if (ret > 0)
                    {
                        printf("<- %s\n", buffer);
                        if (!strcmp(buffer, "OK"))   // check if it is OK on the ftp server side
                        {

                            // open the file to be sent
                            filedesc = open(fileName, O_RDWR);

                            // get the size of the file to be sent
                            fstat(filedesc, &stat_buf);

                            // Read data from file and send it
                            ret = 0;
                            while (1)
                            {
                                unsigned char buff[BUFSIZE] = { 0 };
                                int nread = read(filedesc, buff, BUFSIZE);
                                ret += nread;
                                printf("\nBytes read %d \n", nread);

                                // if read was success, send data.
                                if (nread > 0)
                                {
                                    printf("Sending \n");
                                    write(sockfd, buff, nread);
                                }

                                // either there was error, or we reached end of file.

                                if (nread < BUFSIZE)
                                {
                                    if (ret == stat_buf.st_size)
                                        printf("End of file\n");
                                    else
                                        printf("Error reading\n");
                                    break;
                                }

                            }

                            if (ret == -1)
                            {
                                fprintf(stderr, "error sending the file\n");
                                exit(1);
                            }
                            if (ret != stat_buf.st_size)
                            {
                                fprintf(stderr,
                                        "incomplete transfer when sending: %lld of %d bytes\n",
                                        ret, (int) stat_buf.st_size);
                                exit(1);
                            }
                        }
                        else
                        {
                            printf("ERROR on the server");
                        }

                        // close descriptor for file that was sent
                        close(filedesc);

                        // close socket descriptor
                        close(sockfd);

                    }


                }
                else
                {

                }



            }
        }
        else
        {
            // implement new methods
            printf("unsuported method\n");
        }

    #endif


#if(operation==2)


        if (argc != 5)
        {
            printf("Usage: ./client <SERVER IP ADDRESS> <LISTENING PORT>\n");
            printf(
                "Example: ./client 127.0.0.1 8141 get remoteFileName\n./client 127.0.0.1 8141 [put/get] localFileName\n\n");
            exit(1);
        }
        if(argv[3]=="get" || argv[3]=="put")
        {
            printf(
                "client trying to connect to IP = %s PORT = %s method= %s for FILE= %s\n",
                argv[1], argv[2], argv[3], argv[4]);
        }
        strcpy(fileName, argv[4]);

        if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
            pexit("socket() failed");

        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
        serv_addr.sin_port = htons(atoi(argv[2]));

        // Connect tot he socket offered by the web server
        if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
            pexit("connect() failed");


        if(!strcmp(argv[3], "mget"))
        {
            clock_t start,end;
            float total_t;
            printf("Starting clock...\n");
            start=clock();
            sprintf(buffer, "ls %s", fileName);


            write(sockfd, buffer, strlen(buffer));

            ret = read(sockfd, buffer, BUFSIZE); 	// read Web request in one go
            printf("\n%s",buffer);
            int z=0,s=0;
            char ** arrayString=(char**)malloc(sizeof(char*)*100);
            char* aux=NULL;
            *(arrayString+z)=(char*)malloc(sizeof(char*)*50);
            aux=strtok(buffer,"\n");
            strcpy(*(arrayString+z),aux);
            z++;
            while(aux!=NULL)
            {

                aux=strtok(NULL,"\n");
                if(aux!=NULL)
                {
                    *(arrayString+z)=(char*)malloc(sizeof(char*)*50);
                    strcpy(*(arrayString+z),aux);
                    z++;

                }

            }
            struct estrutura_tread thread_args[z];
            char * return_thread;
            pthread_t pthreads[z];
            int sockfdAUX=0;

            printf("\n Numero de ficheiros a transferir: %d \n",z);
            for(s=0; s<z; s++)
            {
                printf(" Transferindo Ficheiro %s . . . \n",*(arrayString+s));

                if ((sockfdAUX = socket(AF_INET, SOCK_STREAM, 0)) < 0)
                    pexit("socket() failed");

                serv_addr.sin_family = AF_INET;
                serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
                serv_addr.sin_port = htons(atoi(argv[2]));

                // Connect tot he socket offered by the web server
                if (connect(sockfdAUX, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
                    pexit("connect() failed");
                //faz malloc em cada campo ficheiro de cada estrutura
                thread_args[s].ficheiro=(char*)malloc(sizeof(strlen(*(arrayString+s))));
                //copia o nome do ficheiro para dentro do campo de cada estrutura
                strcpy(thread_args[s].ficheiro,*(arrayString+s));
                //guarda em cada struct o descritor da socket
                thread_args[s].socketfd=sockfdAUX;
                //cria a thread[s]
                pthread_create (&pthreads[s], NULL, &callGetThread, &thread_args[s]);
            }
            int result;
            int t;

            for(t=0; t<z; t++)
            {
                pthread_join (pthreads[t],(void *)&return_thread);

            }
            printf("Thread IDS:\n");
            for(t=0; t<z; t++)
            {
                printf("%d \n", thread_args[t]);
            }

            end=clock();

            total_t=((float)(end-start)) / CLOCKS_PER_SEC;
            printf(" Tempo de execucao : %f \n",total_t);

        }


    #endif
    return 1;
}


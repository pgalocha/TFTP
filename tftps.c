//
//  tftps.c
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
#include <semaphore.h>

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/mman.h>

#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#define BUFSIZE 8096
#define operation 2
#define HASHSIZE 10


#if defined(__APPLE__) && defined(__MACH__)
#define COMMON_DIGEST_FOR_OPENSSL
#include <CommonCrypto/CommonDigest.h>
#define SHA1 CC_SHA1
#else
#include <openssl/sha.h>
#endif





#define MACRO 500

typedef struct ficheiro
{
    char* filename;
    void* mmp;
    unsigned char *hash;
    int filesize;
    int nacessos;
    struct ficheiro* pnext;
} FICHEIRO;

//FICHEIRO files[HASHSIZE];
typedef struct paginas
{
    int numeromax;
    FICHEIRO * pfirst;
    int nficheirosinseridos;
    int adicionei;
} PAGINAS;

PAGINAS paginas[HASHSIZE];

int j=0;
int prodptr=0,consptr=0;

pthread_mutex_t lock_p=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t lock_c=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t trincos[HASHSIZE]=PTHREAD_MUTEX_INITIALIZER;

int N=5;
int buff[5]= {0,0,0,0,0};
sem_t sem_c;
sem_t sem_p;

void printSha1(unsigned char * sha)
{
    int i;
    printf("\nFile SHA1\t");
    for (i = 0; i < 20; i++)
        printf("%02x", sha[i]); // 2 lower case hexa
    printf("\n\n");

}

int do_sha1_file(char *name, unsigned char *out)
{
    int fd;
    SHA_CTX sc;
    size_t len;
    unsigned char buf[8192];


    fd = open(name, O_RDONLY);
    if (fd == -1)
    {
        /* do something smart here: the file could not be opened */
        return -1;
    }
    SHA1_Init(&sc);

    errno=0;

    for (;;)
    {
        len = read(fd, buf, sizeof buf);
        if (len == 0 || len == -1)
            break;

        SHA1_Update(&sc, buf, len);
    }
    close(fd);
    if (errno)
    {
        /* some I/O error was encountered; report the error */
        return -1;
    }
    SHA1_Final(out, &sc);
    return 0;
}


/*
 * Hash a string to find the vertical position in the HASH TABLE.
 * a value between 0 and 1023 (inclusive) is returned. You have
 * to search for the string in the corresponding list to see if
 * it is already present
 */


int hash(unsigned char string[])
{
    int i,hash=0;
    for (i=0; i<20; i++)
    {
        hash+=string[i];
    }
    return (hash % HASHSIZE);
}
int busca_idx(char* filename)
{
    int i;
    unsigned char sha[20];
    if((i=do_sha1_file(filename,sha))!= -1)
    {
        for (i = 0; i < 20; i++)
            printf("%02x", sha[i]); // 2 lower case hexa
       // printf("\t Hash value: %d", hash(sha));
    }
    else
        printf("Error with %s", filename);

    printf("\n");


    return hash(sha);
}


/*verifica se um deteerminado ficheiro se encontra mapeado em cache, se for a primeira vez, é chamada a func
carrega(fileName)
que vai tratar de mapear o fileName na cache e fazer a leitura diretamente do disco através da func lerdisco(fileName)
*/
int verifica(int fd,char* fileName)
{
    int i=0;
    for(i=0; i<HASHSIZE; i++)
                {
                    printf("\n i %d :%d %d %d %p ",i, paginas[i].numeromax, paginas[i].nficheirosinseridos,paginas[i].adicionei,paginas[i].pfirst);
                }

    FICHEIRO *pnew=NULL;
    int idx=busca_idx(fileName);
    unsigned char sha[20];
    do_sha1_file(fileName,sha);
    char *aux=(char*)malloc(sizeof(sha));
    strcpy(aux,sha);
    if(paginas[idx].pfirst==NULL)
    {
        printf("\n Inserir pela primeira vez! \n");
        carrega(fd,fileName);
    }
    else
    {
        FICHEIRO*pox =paginas[idx].pfirst;
        printf("\n Não é a primeira vez que insiro!\n %s ",fileName);
        unsigned char sha[20];
        do_sha1_file(fileName,sha);
        char*auxiliar=(char*)malloc(sizeof(sha));
        strcpy(auxiliar,sha);
        if(paginas[idx].adicionei < 2)
        {
            printf("Ainda posso adicionar");
            if(strcmp(pox->hash,auxiliar)==0)
            {
                printf("\nSao iguais!\n");
                pox->nacessos++;
                write(fd, pox->mmp, pox->filesize);
            }
            else
            {
                printf("\n Vou adicionar mais um \n");
                int i=0;

                FICHEIRO * aux=(FICHEIRO*)malloc(sizeof(FICHEIRO));
                aux->filename=(char*)malloc(sizeof(fileName));
                strcpy(aux->filename,fileName);
                unsigned char sha[20];
                do_sha1_file(fileName,sha);
                aux->hash=(char*)malloc(sizeof(sha));
                strcpy(aux->hash,sha);
                aux->nacessos=1;
                struct stat sb;
                off_t len;
                void *p;
                int fd_t;
                fd_t = open (fileName, O_RDONLY);
                if (fd_t == -1)
                {
                    perror ("open");
                    return 1;
                }

                if (fstat (fd_t, &sb) == -1)
                {
                    perror ("fstat");
                    return 1;
                }

                if (!S_ISREG (sb.st_mode))
                {
                    fprintf (stderr, "%s is not a file\n", fileName);
                    return 1;
                }

                p = mmap (0, sb.st_size, PROT_READ, MAP_SHARED, fd_t, 0);
                if (p == MAP_FAILED)
                {
                    perror ("mmap");
                    return 1;
                }

                aux->mmp=p;
                aux->filesize=sb.st_size;
                paginas[idx].adicionei++;
                aux->nacessos=1;
                pox->pnext=aux;
                aux->pnext=NULL;
                write(fd, aux->mmp, aux->filesize);
            }
        }
        else
        {
            /**O Problema encontra-se após entrar aqui que diz que o munmap está a inválido para o nextsize*/

            printf("\n Já está cheio! \n");
            idx=busca_idx(fileName);
            FICHEIRO * antigo=paginas[idx].pfirst;
            if(antigo->nacessos <= antigo->pnext->nacessos)
            {
                printf("\n é o mais antigo!\n ");
                if (munmap (antigo->mmp,antigo->filesize) == -1)
                {
                    perror ("munmap");
                    return 1;
                }
                antigo->filename=(char*)malloc(sizeof(char)*strlen(fileName));
                strcpy(antigo->filename,fileName);
                antigo->nacessos=1;
                struct stat sb;
                off_t len;
                void *p;
                int fd_t;

                fd_t = open (fileName, O_RDONLY);
                if (fd_t == -1)
                {
                    perror ("open");
                    return 1;
                }

                if (fstat (fd_t, &sb) == -1)
                {
                    perror ("fstat");
                    return 1;
                }

                if (!S_ISREG (sb.st_mode))
                {
                    fprintf (stderr, "%s is not a file\n", fileName);
                    return 1;
                }

                p = mmap (0, sb.st_size, PROT_READ, MAP_SHARED, fd_t, 0);
                if (p == MAP_FAILED)
                {
                    perror ("mmap");
                    return 1;
                }

                antigo->mmp=p;
                antigo->filesize=sb.st_size;
                unsigned char sha[20];
                do_sha1_file(fileName,sha);
                antigo->hash=(char*)malloc(sizeof(sha));
                strcpy(antigo->hash,sha);
                write(fd, antigo->mmp, antigo->filesize);

            }
            else
            {
                printf("\n é o segundo a sair \n");
                if (munmap (antigo->pnext->mmp, antigo->pnext->filesize) == -1)
                {
                    perror ("munmap");
                    return 1;
                }
                antigo->pnext->filename=(char*)malloc(sizeof(char)*strlen(fileName));
                strcpy(antigo->pnext->filename,fileName);
                antigo->pnext->nacessos=1;
                struct stat sb;
                off_t len;
                void *p;
                int fd_t;

                fd_t = open (fileName, O_RDONLY);
                if (fd_t == -1)
                {
                    perror ("open");
                    return 1;
                }

                if (fstat (fd_t, &sb) == -1)
                {
                    perror ("fstat");
                    return 1;
                }

                if (!S_ISREG (sb.st_mode))
                {
                    fprintf (stderr, "%s is not a file\n", fileName);
                    return 1;
                }

                p = mmap (NULL, sb.st_size, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_PRIVATE, fd_t, 0);
                if (p == MAP_FAILED)
                {
                    perror ("mmap");
                    return 1;
                }
                antigo->pnext->mmp=p;
                antigo->pnext->filesize=sb.st_size;
                unsigned char sha[20];
                do_sha1_file(fileName,sha);
                antigo->pnext->hash=(char*)malloc(sizeof(sha));
                strcpy(antigo->pnext->hash,sha);
                write(fd, antigo->pnext->mmp, antigo->pnext->filesize);
            }
        }
    }

}

int carrega(int fd_t,char* fileName)
{
    int idx=busca_idx(fileName);

    FICHEIRO * aux=(FICHEIRO*)malloc(sizeof(FICHEIRO));
    aux->filename=(char*)malloc(sizeof(fileName));
    strcpy(aux->filename,fileName);
    //printf("\n AUX->FILENAME: %s \n",aux->filename);
    unsigned char sha[20];
    do_sha1_file(fileName,sha);
    aux->hash=(char*)malloc(sizeof(sha));
    strcpy(aux->hash,sha);
    aux->nacessos=1;
    struct stat sb;
    off_t len;
    void *p;
    int fd;

    fd = open (fileName, O_RDONLY);
    if (fd == -1)
    {
        perror ("open");
        return 1;
    }

    if (fstat (fd, &sb) == -1)
    {
        perror ("fstat");
        return 1;
    }

    if (!S_ISREG (sb.st_mode))
    {
        fprintf (stderr, "%s is not a file\n", fileName);
        return 1;
    }

    p = mmap (0, sb.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (p == MAP_FAILED)
    {
        perror ("mmap");
        return 1;
    }

    aux->mmp=p;
    aux->filesize=sb.st_size;


    paginas[idx].pfirst=aux;
    paginas[idx].adicionei++;


    int i=0;
//    for(i=0; i<HASHSIZE; i++)
//    {
//        printf("\n i %d :%d %d %d %p ",i, paginas[i].numeromax, paginas[i].nficheirosinseridos,paginas[i].adicionei,paginas[i].pfirst);
//    }

    write(fd_t, aux->mmp, aux->filesize);
    //ledisco(fd_t,fileName);

}

void getFunction(int fd, char * fileName)
{
    int i=0;
    int len=0;
    int idx=busca_idx(fileName);
    printf("\n idx: %d \n",idx);

    pthread_mutex_lock(&trincos[idx]);
    verifica(fd,fileName);
    pthread_mutex_unlock(&trincos[idx]);

}

ledisco(int fd, char * fileName)
{
    int file_fd;
    long ret;

    static char buffer[BUFSIZE + 1]; /* static so zero filled */

    if ((file_fd = open(fileName, O_RDONLY)) == -1)   /* open the file for reading */
    {
        printf("ERROR failed to open file %s\n", fileName);
        close(fd);
    }

    printf("LOG SEND %s \n", fileName);

    /* send file in 8KB block - last block may be smaller */
    while ((ret = read(file_fd, buffer, BUFSIZE)) > 0)
    {
        write(fd, buffer, ret);
    }

}

void putFunction(int fd, char * fileName)
{
    int file_fd;
    long ret;

    static char buffer[BUFSIZE + 1]; /* static so zero filled */


    printf("LOG Header %s \n", fileName);

    file_fd = open(fileName, O_WRONLY | O_CREAT | O_TRUNC, 0777);
    if (file_fd == -1)
    {
        sprintf(buffer, "ERROR");
        write(fd, buffer, strlen(buffer));
    }
    else
    {
        sprintf(buffer, "OK");
        write(fd, buffer, strlen(buffer));

        while ((ret = read(fd, buffer, BUFSIZE)) > 0)
            write(file_fd, buffer, ret);
    }

}
void lsFunction(int fd, char * path)
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
        write(fd, buffer, strlen(buffer));
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
                //printf("%s -> as\n",dit->d_name);
                strcat(pasta,"\n");

            }
        }
        printf("%s \n",pasta);
        //writes to the socket
        write(fd, pasta, strlen(pasta));
    }

}

/* this is the ftp server function */
int ftp(int fd, int hit)
{
    int j, file_fd, filedesc;
    long i, ret, len;
    char * fstr;
    static char buffer[BUFSIZE + 1]; /* static so zero filled */

    ret = read(fd, buffer, BUFSIZE); // read FTP request

    if (ret == 0 || ret == -1)   /* read failure stop now */
    {
        close(fd);
        return 1;
    }
    if (ret > 0 && ret < BUFSIZE) /* return code is valid chars */
        buffer[ret] = 0; /* terminate the buffer */
    else
        buffer[0] = 0;

    for (i = 0; i < ret; i++) /* remove CF and LF characters */
        if (buffer[i] == '\r' || buffer[i] == '\n')
            buffer[i] = '*';

    printf("LOG request %s - hit %d\n", buffer, hit);

    /* null terminate after the second space to ignore extra stuff */
    for (i = 4; i < BUFSIZE; i++)
    {
        if (buffer[i] == ' ')   /* string is "GET URL " +lots of other stuff */
        {
            buffer[i] = 0;
            break;
        }
    }

    if (!strncmp(buffer, "get ", 4))
    {
        // GET
        getFunction(fd, &buffer[5]);
    }
    else if (!strncmp(buffer, "put ", 4))
    {
        // PUT
        putFunction(fd,&buffer[5]);
    }
    else if (!strncmp(buffer, "ls ", 3))
    {
        char string[50]="/home/pgalocha/projeto";
        char copia[20];
        strcpy(copia,&buffer[3]);
        char * token=NULL;
        printf("Buffer-> %s ",buffer);
        token=strtok(buffer," ");
        token=strtok(NULL," ");
        if(strcmp(token,".")==0)
        {
            lsFunction(fd,".");
        }
        else
        {
            strcat(string,copia);
            printf(" \n STRING PATH : %s \n",string);
            lsFunction(fd, string);

        }

    }
    else

        sleep(1); /* allow socket to drain before signalling the socket is closed */
    close(fd);
    return 0;
}
struct sv_thread
{
    int hit;
    int fd;
};


//multi thread
void* ftpThread(void*parameters)
{
    long p = (long)parameters;
    int j, file_fd, filedesc;
    long i, ret, len;
    char * fstr;
    static char buffer[BUFSIZE + 1]; /* static so zero filled */

    ret = read(p, buffer, BUFSIZE); // read FTP request

    if (ret == 0 || ret == -1)   /* read failure stop now */
    {
        close(p);
        return ;
    }
    if (ret > 0 && ret < BUFSIZE) /* return code is valid chars */
        buffer[ret] = 0; /* terminate the buffer */
    else
        buffer[0] = 0;

    for (i = 0; i < ret; i++) /* remove CF and LF characters */
        if (buffer[i] == '\r' || buffer[i] == '\n')
            buffer[i] = '*';

    printf("LOG request %s - hit \n", buffer);

    /* null terminate after the second space to ignore extra stuff */
    for (i = 4; i < BUFSIZE; i++)
    {
        if (buffer[i] == ' ')   /* string is "GET URL " +lots of other stuff */
        {
            buffer[i] = 0;
            break;
        }
    }

    if (!strncmp(buffer, "get ", 4))
    {
        // GET
        getFunction(p, &buffer[5]);
    }
    else if (!strncmp(buffer, "put ", 4))
    {
        // PUT
        putFunction(p,&buffer[5]);
    }
    else if (!strncmp(buffer, "ls ", 3))
    {
        char string[50]="/home/pgalocha/projeto";
        char copia[20];
        strcpy(copia,&buffer[3]);
        char * token=NULL;
        printf("Buffer-> %s ",buffer);
        token=strtok(buffer," ");
        token=strtok(NULL," ");
        if(strcmp(token,".")==0)
        {
            lsFunction(p,".");
        }
        else
        {
            strcat(string,copia);
            printf(" \n STRING PATH : %s \n",string);
            lsFunction(p, string);

        }

    }
    else

        sleep(1); /* allow socket to drain before signalling the socket is closed */
    close(p);
    return 0;
}

#if(operation==2)

void *thread_functionConsumidor(void *unused)
{
    pthread_t self;
    int temp;
    self=pthread_self();
    int item;
    while(1)
    {
        sem_wait(&sem_c);
        pthread_mutex_lock(&lock_c);
        item= buff[consptr];
        // buff[consptr]=0;
        //printf("Estou a consumir da posicao %d \n",consptr);
        //sleep(1);
        consptr=(consptr+1)%N;
        pthread_mutex_unlock(&lock_c);
        sem_post(&sem_p);
        ftp(item,0);

    }
    pthread_exit(NULL);
}
#endif




/* just checks command line arguments, setup a listening socket and block on accept waiting for clients */

int main(int argc, char **argv)
{
    int i, port, pid, listenfd, socketfd, hit;
    socklen_t length;
    static struct sockaddr_in cli_addr; /* static = initialised to zeros */
    static struct sockaddr_in serv_addr; /* static = initialised to zeros */

    if (argc < 3 || argc > 3 || !strcmp(argv[1], "-?"))
    {
        printf("\n\nhint: ./tftps Port-Number Top-Directory\n\n"
               "\ttftps is a small and very safe mini ftp server\n"
               "\tExample: ./tftps 8181 ./fileDir \n\n");

        exit(0);
    }

    if (chdir(argv[2]) == -1)
    {
        printf("ERROR: Can't Change to directory %s\n", argv[2]);
        exit(4);
    }

    printf("LOG tftps starting %s - pid %d\n", argv[1], getpid());

    /* setup the network socket */
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        printf("ERROR system call - setup the socket\n");
    port = atoi(argv[1]);
    if (port < 0 || port > 60000)
        printf("ERROR Invalid port number (try 1->60000)\n");


    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port);

    if (bind(listenfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        printf("ERROR system call - bind error\n");
    if (listen(listenfd, 64) < 0)
        printf("ERROR system call - listen error\n");
    // Main LOOP

#if(operation==1)
    pthread_t thread1_id;
    struct sv_thread thread_args[100];
#endif

#if(operation==2)
    pthread_t consumidores[5];
    sem_init(&sem_p,0,N);
    sem_init(&sem_c,0,0);
    int z=0;
    for(z=0; z<5; z++)
        pthread_create(&consumidores[i],NULL,&thread_functionConsumidor,NULL);




#endif

    for(i=0; i<HASHSIZE; i++)
    {

        paginas[i].pfirst=NULL;
        paginas[i].numeromax=2;
        paginas[i].nficheirosinseridos=0;
        paginas[i].adicionei=0;
    }

    for (hit = 1 ;; hit++)
    {
        length = sizeof(cli_addr);
        /* block waiting for clients */
        socketfd = accept(listenfd, (struct sockaddr *) &cli_addr, &length);
#if(operation==0)
        pid=fork();
        if(pid==0)
        {

            if (socketfd < 0)
            {
                printf("ERROR system call - accept error\n");
            }

            else
            {

                ftp(socketfd, hit);
                close(socketfd);

                exit(1);
            }

        }
        else
        {
            close(socketfd);

            // pai apanha signal SIGCHLD do filho e ignora e continua
            //signal(SIGCHLD,NULL);
            signal(SIGCHLD,SIG_IGN);

        }
#endif

#if(operation==1)
        pthread_create (&thread1_id, NULL, &ftpThread,socketfd);
#endif
#if(operation==2)

        sem_wait(&sem_p);
        //pthread_mutex_lock(&lock_p);
        buff[prodptr]=socketfd;
        prodptr=(prodptr+1) %N;
        //pthread_mutex_unlock(&lock_p);
        sem_post(&sem_c);




#endif
    }

}

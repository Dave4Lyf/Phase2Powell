/*
    CYSE 411
    Code Review Project
    David Nicholson

    Coded in CLion
    Using compiler & toolchain Cygwin

*/

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <signal.h>
#include <sys/wait.h>

#include <netinet/in.h>
#include <arpa/inet.h> //currently not used
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h> //currently not used

#define PORT 9090
#define USERNAME 0x01
#define PASSWORD 0x02
#define BADUSER "\x33\x44 BAD USERNAME!"
#define BADPASS "\x33\x45 BAD PASSWORD!"
#define READY   "\x41\x41 READY!"
#define USERPATH "./users/"
#define ARTICLEPATH "./articles/"
#define LISTCOMMAND "ls ./articles/ > list.txt"
#define FILENOTAVAIL "\x33\x31 FILE NOT AVAILABLE!"
#define BEGINFILE "\x41\x41 BEGIN FILE: END WITH '!!!'"
#define ARTICLEWROTE "\x41\x42 ARTICLE HAS BEEN WRITTEN!"
//note for future: hide information below
#define LIST_ARTICLES 0x22
#define READ_ARTICLE 0x23
#define WRITE_ARTICLE 0x24
#define COMMAND 0x25
#define ADD_USER 0x26

void logData(FILE *logfile, char *format, ...);
int setupSock(FILE *logf, unsigned short port);
int writeSock(int sock, char *buf, size_t len);
int readSock(int sock, char *buf, size_t len);
void mainLoop(FILE *logf, int sock);
void handleConnection(FILE *logfile, int sock);
int userFunctions(FILE *logfile, int sock, char *user);
char *findarg(char *argbuf, char argtype);
int authenticate(FILE *logfile, char *user, char *pass);

//creating a socket
int writeSock(int sock, char *buf, size_t len)
{
    //signed size_t: size of allocated memory block
    ssize_t byteswrote = 0;
    ssize_t ret = 0;

    while (byteswrote < len)
    {
        ret = send(sock, buf + byteswrote, len - byteswrote, 0);

        if (ret < 0)
        {
            return -1;
        }

        if (ret == 0)
        {
            break;
        }

        //adding bytes written in socket to total
        byteswrote += ret;
    }

    return byteswrote;
}

//reading a created socket
int readSock(int sock, char *buf, size_t len)
{
    ssize_t ret = 0;
    ssize_t bytesread = 0;

    while (bytesread < len)
    {
        ret = recv(sock, buf + bytesread, len - bytesread, 0);

        if (ret == 0)
        {
            break;
        }

        if (ret < 0)
        {
            return -1;
        }

        bytesread += ret;
    }

    return bytesread; //integer, how many bytes were read of socket
}

//opens a file, writes a socket to the file (if a file is present), closes the file and releases buffer
void writeArticle(int sock, FILE *logfile, char *action)
{
    FILE *file;
    char *p; //NOT USED
    size_t x, y;
    int complete = 0;
    //char buf[1024];
    //char path[1024];

     char* buf  = (char*)calloc(1024, sizeof(char)); //d
     char* path = (char*)calloc(1024, sizeof(char));

    strlcpy(path, ARTICLEPATH);  //was originally strcpy
    strncat(sizeof(*path), &action[1], sizeof(path)); //copy the location of the article on the disk

    logData(logfile, "user writing article: %s", path); //writes to log file

    file = fopen(&action[1], "w"); //opens the file in writing mode

    if (!file) //if not file is located dont write socket
    {
        writeSock(sock, FILENOTAVAIL, sizeof(FILENOTAVAIL)); //write a socket that will show not available
        free(buf);
        return;
    }

    //writing the socket to the file
    writeSock(sock, BEGINFILE, sizeof(BEGINFILE));

    while (1) //always true so will go into the loop
    {
        memset(sizeof(*buf), 0, sizeof(buf)); //allocating memory, start at buf and at size of buf -1
        x = readSock(sock, buf, sizeof(buf)-1); //x is size_t
        for (y = 0; y < x; ++y)
        {
            if (buf[y] == '!')
            {
                if (buf[y+1] == '!' && buf[y+2] == '!') //checks if end of buffer, in array [!,!,!]
                {
                    buf[y] = 0x0; //array [!,!,!] --> [0x0,!,!]
                    complete = 1;
                }
            }
        }
        fputs(buf, file); //puts buf into the file
        if (complete) //end of loop, if complete==1 same as complete==true
            break; //exits while loop
    }

    writeSock(sock, ARTICLEWROTE, sizeof(ARTICLEWROTE)); //write the sock to the article file
    fclose(file); //closes the file
    free(buf); //releases the buffer
    free(path); //releases the path to the file/article

}


//opens the file, reads it, closes it
void readArticle(int sock, FILE *logfile, char *action)
{
    FILE *file;
    char buf[100];
    char path[100];

    logData(logfile, &action[1]);

    strcpy(path, ARTICLEPATH); //copy the article path to the path in the method
    strlcat(path, &action[1]); //

    logData(logfile, "user request to read article: %s", path); //adds to a log file that user requests to read the article

    file = fopen(path, "r"); //opens the file in reading mode

    if (!file) //checks if there is a file present, if not go into the statement
    {
        writeSock(sock, FILENOTAVAIL, sizeof(FILENOTAVAIL));
        return;
    }

    /* fgets for the size of the buffer (100), from the file
       writing the article to the user each time! */

    while (fgets(buf, 100, file))
    {
        writeSock(sock, buf, strlen(buf));
    }

    fclose(file);

    return;
}

//List all Articles that are written
void listArticles(int sock, FILE *logfile, char *action)
{
    char buf[100];
    FILE *list;

    logData(logfile, "user has requested a list of articles"); //writing to the log file

    /* i wish i had more time! i wouldnt have to write
       this code using system() to call things! */

    memset(buf, 0, sizeof(buf)); //allocating memory
    system(LISTCOMMAND);

    list = fopen("list.txt", "r"); //opens the list.txt file in reading mode

    while (fgets(buf, sizeof(buf)-1, list))
    {
        writeSock(sock, buf, strlen(buf));
    }

    fclose(list); //closes the list file
    return;
}

//running a system command and logging it in the log file
void command(FILE *log, int sock, char *action)
{
    logData(log, "executing command: %s", &action[1]); //logging a command executing
    system(&action[1]); //system command
}

//adding a user to read and write
void addUser(FILE *log, int sock, char *action)
{
    char *p;
    char buf[1024];

    p = strchr(&action[1], ':');

    if (!p)
    {
        return;
    }

    *p = 0x0; //pointer p pointing to 0x0 or null
    logData(log, "Adding user: %s with pass: %s", &action[1], &p[1]); //logging the creation of a user
    snprintf(buf, sizeof(buf)-1, "echo %s > %s%s.txt", &p[1], USERPATH, &action[1]);
    return;
}

//admin functions for the users and data
void adminFunctions(FILE *logfile, int sock)
{
    char action[1024];
    size_t len;
    while (1)
    {
        writeSock(sock, READY, sizeof(READY));
        memset(action, 0, sizeof(action));
        len = readSock(sock, action, sizeof(action));

        if (action[0] == ADD_USER) //checks if you want to add a user
        {
            addUser(logfile, sock, action); //user gets added and has access to logfile, socket, and action
        }
        else if (action[0] == COMMAND)
        {
            command(logfile, sock, action); //executing an admin level command
        }
        else
        {
            logData(logfile, "unknown action: %x", action[0]); //logging an unknown action
        }
    }

}

//sets what the user can do
int userFunctions(FILE *logfile, int sock, char *user)
{
    char action[1024];
    size_t len;

    if (0 == strncmp(user, "admin", 5)) //if user is called admin, grants admin functions
    {
        adminFunctions(logfile, sock);
        return 0;
    }

    while (1) //always true so will enter loop
    {
        writeSock(sock, READY, sizeof(READY)); //writing a socket
        memset(action, 0, sizeof(action));
        len = readSock(sock, action, sizeof(action));

        //different commands/actions a user can do
        if (action[0] == LIST_ARTICLES) //list all articles that are written
        {
            listArticles(sock, logfile, action);
        }
        else if (action[0] == READ_ARTICLE) //read an article
        {
            readArticle(sock, logfile, action);
        }
        else if (action[0] == WRITE_ARTICLE) //write an article
        {
            writeArticle(sock, logfile, action);
        }
        else
        {
            logData(logfile, "unknown action %x", action[0]); //unknown command written to logfile
            return 0;

        }
    }

   return 0;
}

/* return 1 for success, 2 on bad username, 3 on bad password */
int authenticate(FILE *logfile, char *user, char *pass) //way to authenticate user
{
    char search[1024];//was 512 but cause vulnerability
    char path[1024];
    char userfile[1024];
    char data[1024];
    FILE *file;
    int ret;

    memset(path, 0, sizeof(path)); //was 1024

    /* FIXME: hard coded admin backdoor for password recovery */
    if (memcmp(pass, "baCkDoOr", 9) == 0)
    {
        return 1;
    }

    // look up user by checking user files: done via system() to /bin/ls|grep user
    logData(logfile, "performing lookup for user via system()!\n");
    snprintf(userfile, sizeof(userfile)-1, "%s.txt", user);
    snprintf(search, sizeof(userfile)-1, "stat %s`ls %s | grep %s`", USERPATH, USERPATH, userfile);
    ret = system(search); //username lookup

    if (ret != 0)
    {
        return 2; //return of bad username
    }

    snprintf(path, sizeof(path)-1, "%s%s", USERPATH, userfile);

    /* open file and check if contents == password */
    file = fopen(path, "r");

    if (!file)
    {
        logData(logfile, "fopen for userfile failed\n"); //opens the log file to check password
        return 2; //bad username
    }

    logData(logfile, "getting userfile info\n"); //log username check is good
    fgets(data, sizeof(data)-1, file);

    fclose(file);

    /* Password Check! */
    if (memcmp(data, pass, 3))
    {
        return 3; //bad password
    }

    return 1;
}

//find argument
char *findarg(char *argbuf, char argtype)
{
    char *ptr1;
    char *found = NULL;
    char type = 0;
    size_t size;

    ptr1 = argbuf;

    while (1) //while true
    {
        memcpy((char *)&size, ptr1, 4); //copy the memory
        if (size == 0)
        {
            break;
        }
        if (ptr1[4] == argtype)
        {
            found = &ptr1[5];
            break;
        }
        ptr1 += size;
    }

    return found;
}

//setting up the connection of the socket
void handleConnection(FILE *logfile, int sock)
{
    char buffer[1024];
    char argbuf[1024];
    char *user = NULL;
    char *pass = NULL;
    int len = 0;
    int ret = 0;
    size_t segloop;
    size_t segmentcount;
    size_t segnext;
    size_t argsize;
    char *ptr1;
    char *ptr2;

    /* read in data */
    memset(buffer, 0, sizeof(buffer));
    len = readSock(sock, buffer, sizeof(buffer));
    logData(logfile, "handling connection");

    if (len == -1)
    {
        return;
    }

    /* parse protocol */
    ptr1 = buffer;
    ptr2 = argbuf;

    /* get count of segments */
    memcpy((char *)&segmentcount, ptr1, 4);

    logData(logfile, "Segment count is %i", segmentcount);

    /* make sure there aren't too many segments!
       so the count * 8(bytes) should be the max */
    if (segmentcount * 8 > sizeof(argbuf))
    {
        logData(logfile, "bad segment count");
        return;
    }

    ptr1 += 4;

    memset(argbuf, 0, sizeof(argbuf));

    for (segloop = 0; segloop < segmentcount; ++segloop)
    {
        logData(logfile, "adding segment %i", segloop+1);
        memcpy((char *)&segnext, ptr1, 4);
        logData(logfile, "next segment offset %i", segnext);
        ptr1 += 4;
        memcpy((char *)&argsize, ptr1, 4);
        logData(logfile, "argsize: %i", argsize);
        memcpy(ptr2, ptr1, argsize);
        ptr2 += argsize;
        ptr1 += segnext;
    }

    logData(logfile, "looking up user args");

    user = findarg(argbuf, USERNAME); //getting username
    pass = findarg(argbuf, PASSWORD); //getting password

    snprintf(buffer, sizeof(buffer)-1, "User attempting to authenticate: %s", user);
    logData(logfile, buffer); //write buffer into log file

    logData(logfile, "calling authenticate"); //log start of authentication check
    ret = authenticate(logfile, user, pass);
    logData(logfile, "returned from authenticate"); //log return from authentication check

    if (ret != 1)
    {

        if (ret == 2)
        {
            writeSock(sock, BADUSER, sizeof(BADUSER)); //if ret == 2 you have a bad username
        }

        if (ret == 3)
        {
            writeSock(sock, BADPASS, sizeof(BADPASS)); //if ret ==3 you have a bad password
        }

        snprintf(buffer, sizeof(buffer)-1,"user: %s failed to login with password %s", user, pass);
        logData(logfile, buffer); //log the reason for failed authentication
        return;
    }

    logData(logfile, "user %s authenticated!", user); //write to log file that user was authenticated

    userFunctions(logfile, sock, user); //give user the access to logfile and socet

    return;
}


void mainLoop(FILE *logf, int sock)
{
    int clientfd = 0;
    //void **client = (struct sockaddr_in*)malloc(sizeof(struct sockaddr_in));
    struct sockaddr_in *client = (struct sockaddr_in*)malloc(sizeof(struct sockaddr_in));
    socklen_t clientlen = 0;
    pid_t offspring = 0;

    memset(sizeof(*client), 0, sizeof(client));//*client or client?

    logData(logf, "entering main loop...");

    while (1)
    {
        clientfd = accept(sock, (struct sockaddr *)&client, &clientlen);
        if (clientfd == -1)
        {
            continue;
        }

        offspring = fork();

        if (offspring == -1)
        {
            continue;
        }

        if (offspring == 0)
        {
            handleConnection(logf, clientfd);
            close(clientfd);
            exit(0);
        }

        close(clientfd);
    }

    free(client);
}


void spawnhandler(int signumber)
{
    pid_t pid;
    int stat;

    while ((pid = waitpid(-1, &stat, WNOHANG))>0)
    {
        printf("circle of life completed for %i\n", pid);
    }
}

//setting up the socket
int setupSock(FILE *logf, unsigned short port)
{
    int sock = 0;
    struct sockaddr_in sin;
    int opt = 0;

    if (signal(SIGCHLD, spawnhandler) == SIG_ERR)
    {
        perror("fork() spawn handler setup failed!");
        return -1;
    }

    memset((char *)&sin, 0, sizeof(sin));

    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);

    sock = socket(AF_INET, SOCK_STREAM, 0);

    if (sock == -1)
    {
        logData(logf, "socket() failed");
        return -1;
    }

    opt = 1;

    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
    {
        logData(logf,"setsockopt() failed");
        return -1;
    }

    if (bind(sock, (struct sockaddr *)&sin, sizeof(sin)) == -1)
    {
        logData(logf, "bind() failed");
        return -1;
    }

    if (listen(sock, 10) == -1)
    {
        logData(logf, "listen() failed");
        return -1;
    }

    return sock;
}

//main of code
int main(int argc, char *argv[])
{
    int sock;
    FILE *logf;

    /* setup log file */
    logf = fopen("logfile.txt", "w"); //open a file called logfile with writing privilage

    if (!logf) //checks if log file is created and can open it
    {
        perror("unable to open log file\n");
        exit(1);
    }

    /* go daemon */
    /* The daemon() function is for programs wishing to detach themselves
       from the controlling terminal and run in the background as system
       daemons.

       If nochdir is zero, daemon() changes the process's current working
       directory to the root directory ("/"); otherwise, the current working
       directory is left unchanged.

       If noclose is zero, daemon() redirects standard input, standard
       output and standard error to /dev/null; otherwise, no changes are
       made to these file descriptors. */
    daemon(0,0);

    /* setup socket */
    sock = setupSock(logf, PORT);

    if (sock == -1) //if socket failed to be setup
    {
        logData(logf, "failed to setup socket, exiting");
        exit(1);
    }

    logData(logf, "intial socket setup complete"); //log that socket was set up properly

    mainLoop(logf, sock);

    /* this should never execute */
    exit(0);
}

/* printf-style data logging */
void logData(FILE *logfile, char *format, ...)
{
    char buffer[4096];
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(buffer, sizeof(buffer)-1, format, arguments);
    va_end(arguments);
    fprintf(logfile, "LoggedData [Proccess:%i]: %s\n", getpid(), buffer);
    fflush(logfile);
}


//gnu -lreadline some.c -o some
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/select.h>
#include <errno.h>
#include <time.h>
#include <ncurses.h>

#include <termios.h>
#include <stropts.h>
//#include "readline.h"
//#include <conio.h>
#include <readline/readline.h>
#include <readline/history.h>
#include<curses.h>
#include <term.h>
/* These define the scan codes(IBM) for the keys. All numbers are in decimal.*/
#define PAGE_UP     73
#define HOME        71
#define END         79
#define PAGE_DOWN   81
#define UP_ARROW    72
#define LEFT_ARROW  75
#define DOWN_ARROW  80
#define RIGHT_ARROW 77
#define F1          59
#define F2          60
#define F3          61
#define F4          62
#define F5          63
#define F6          64
#define F7          65
#define F8          66
#define F9          67
#define F10         68

#define LEN 10
#define MAX_DATA  1025
#define MAX_NAME 100
#define BUFFER 2000
#define MAXSIZE 20
#define MAXBUFLEN 2000
//Max clients that the server will listen too 
#define MAXCLIENTS 10
//Used for text conferencing where the value of 0 corresponds to an unconnected conference session  
#define NOSESSION  0

//Max number of sessions that can be present at once X
#define MAXCONFERENCESESSIONS 100


//Defining the types used to communicate between server/client  
#define LOGIN 0
#define LO_ACK 1
#define LO_NACK 2
#define EXIT 3
#define JOIN 4
#define JN_ACK 5
#define JN_NACK 6
#define LEAVE_SESS 7
#define NEW_SESS 8
#define NS_ACK 9
#define MESSAGE 10
#define QUERY 11 //A request from the client of a list of online users and available sessions 
#define QU_ACK 12 //List of users online and their session IDS

#define STDIN 0 
char** my_completion(const char*, int, int);
char* my_generator(const char*, int);
char * dupstr(char*);
void *xmalloc(int);

struct addrinfo {
    int ai_flags;
    int ai_family;
    int ai_socktype;
    int ai_protocol;
    socklen_t ai_addrlen;
    struct sockaddr *ai_addr;
    char *ai_canonname;
    struct addrinfo *ai_next;
};

//credentials stored in file "Server LoginInformation.txt"

//global variables used for preserving server data between function calls
//these are initialized in login
int sockfd;
struct addrinfo hints, *servinfo, *p;
int rv;
int loggedin = 0; /*used to indicate that client is sucessfully logged into the server*/
int inSession = 0; /*used to indicate that client is currently in a session*/
int hostSize = 0; /*used to keep track of length of currently logged in username*/
char username[MAXBUFLEN]; /*used to keep track of the currently logged in user*/
char curSesh[MAXBUFLEN]; /*used to keep track of the currently joined session*/
int wantQuit = 0; /*used to keep track of whether or not the user issued the quit command*/
int wantLeave = 0; /*used to keep track of whether or not the user issued leavesession*/


//autocomplete commands, you can add a command if you want something to be autocorrected 
char* cmd [] = {"/login", "127.0.0.1", "/register", "/quit", "/logout", "/createsession", "/joinsession"};
//function to tie all the information into correct packet form

char* packetize(int type, int size, char* source, char* data, int packetSize) {
    char buf[MAXBUFLEN];
    char intermediate[MAXBUFLEN];
    char* result;
    result = (char*) malloc(packetSize * sizeof (char));

    sprintf(buf, "%d", type);
    strcpy(intermediate, buf);
    strcat(intermediate, ":");
    sprintf(buf, "%d", size);
    strcat(intermediate, buf);
    strcat(intermediate, ":");

    strcat(intermediate, source);
    strcat(intermediate, ":");
    strcat(intermediate, data);

    result = intermediate;

    return result;
}

//function to connect to server using login credentials 
//argv[0] = id, argv[1] = password, argv[2] = server ip, argv[3] = server port

int login(char** argv) {
    int numbytes;
    char buf[MAXBUFLEN];
    char result[MAXBUFLEN];

    //connection initialization
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if ((rv = getaddrinfo(argv[2], argv[3], &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }
    // loop through all the results and make a socket
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }
        break;
    }
    if (p == NULL) {
        fprintf(stderr, "client: failed to create socket\n");
        return 2;
    }

    int error;
    error = connect(sockfd, p->ai_addr, p->ai_addrlen);
    if (error) {
        printf("client: failed to connect to server \n");
        return -1;
    }

    //at this point we are connected to the server in the given port number
    //create a packet using appropriate info and sent it to server 
    int type = LOGIN;
    int size = strlen(argv[1]);
    hostSize = strlen(argv[0]);
    //determine the packet size
    //4 colons (4 bytes) + data size + size of source + 1 type (1 byte) + size of the variable "size"
    sprintf(buf, "%d", size);
    int packetSize = 4 + size + 1 + strlen(buf) + strlen(argv[0]);

    char* packet = packetize(type, size, argv[0], argv[1], packetSize);


    if ((numbytes = send(sockfd, packet, packetSize, 0)) == -1) {
        perror("client: send");
        exit(1);
    }

    //receive acknowledgment from server
    if ((numbytes = recv(sockfd, result, MAXBUFLEN, 0)) == -1) {
        perror("client: recv");
        exit(1);
    }

    //parse the packet to get the data
    char* garbage1;
    char* garbage2;
    char* garbage3;
    char* reason;

    garbage1 = strtok(result, ":");
    garbage2 = strtok(NULL, ":");
    garbage3 = strtok(NULL, ":");
    reason = strtok(NULL, ":");

    //if reason is not empty, server indicates that login was not successful
    if (reason != NULL) {
        printf("%s\n", reason);
        return -1;
    }
    return 0;
}

int join_session(char* sessionID) {
    int numbytes;
    char result[MAXBUFLEN];
    int packetSize = hostSize + 4 + 1 + strlen(sessionID) + 1;
    char* packet = packetize(JOIN, strlen(sessionID), username, sessionID, packetSize);

    if ((numbytes = send(sockfd, packet, packetSize, 0)) == -1) {
        perror("client: send");
        exit(1);
    }

    //printf("sent %d to server \n", numbytes);

    //receive acknowledgment from server
    if ((numbytes = recv(sockfd, result, MAXBUFLEN, 0)) == -1) {
        perror("client: recv");
        exit(1);
    }

    //printf("received packet is %s\n", result);

    //parse the packet to get the data
    char* garbage1;
    char* garbage2;
    char* garbage3;
    char* error;

    garbage1 = strtok(result, ":");
    garbage2 = strtok(NULL, ":");
    garbage3 = strtok(NULL, ":");
    error = strtok(NULL, ":");

    if (strlen(error) < 10)
        inSession = 1;


    /*if(error != NULL){
        printf("%s\n", error);
        return -1;
    }*/

    strcpy(curSesh, sessionID);
    return 0;
}

int create_session() {

    int numbytes;
    char result[MAXBUFLEN];
    int packetSize = hostSize + 4 + 1 + 1;
    char* packet = packetize(NEW_SESS, 0, username, "", packetSize);
    //printf("%s userame\n", username);

    //printf("packet to send: %s\n", packet);

    if ((numbytes = send(sockfd, packet, packetSize, 0)) == -1) {
        perror("client: send");
        exit(1);
    }

    //printf("sent %d to server \n", numbytes);

    if ((numbytes = recv(sockfd, result, MAXBUFLEN, 0)) == -1) {
        perror("client: recv");
        exit(1);
    }

    //printf("packet is: %s\n", result);
    //printf("received %d from server\n", numbytes);

    //parse the packet to get the data
    char* garbage1;
    char* garbage2;
    char* garbage3;
    char* response;

    garbage1 = strtok(result, ":");
    garbage2 = strtok(NULL, ":");
    garbage3 = strtok(NULL, ":");
    response = strtok(NULL, ":");

    if (response != NULL) {
        printf("%s \n", response);
        return -1;
    }

    return 0;
}

int quit() {
    wantQuit = 1;
    return 0;
}

int logout() {

    int numbytes;

    int packetSize = hostSize + 4 + 1 + 1;
    char* packet = packetize(EXIT, 0, username, "", packetSize);

    //printf("packet to send: %s\n", packet);

    if ((numbytes = send(sockfd, packet, packetSize, 0)) == -1) {
        perror("client: send");
        exit(1);
    }

    loggedin = 0;
    //printf("sent %d to server \n", numbytes);

    return 0;
}

int leave_session() {

    int numbytes;
    int packetSize = hostSize + 4 + 1 + 1;
    char* packet = packetize(LEAVE_SESS, 0, username, "", packetSize);

    //printf("packet to send: %s\n", packet);

    if ((numbytes = send(sockfd, packet, packetSize, 0)) == -1) {
        perror("client: send");
        exit(1);
    }

    //printf("sent %d to server \n", numbytes);
    wantLeave = 1;
    inSession = 0;

    return 0;
}

int list() {

    int numbytes;
    int packetSize = hostSize + 4 + 2 + 1;
    char* packet = packetize(QUERY, 0, username, "", packetSize);
    char result[MAXBUFLEN];

    // printf("packet to send: %s\n", packet);

    if ((numbytes = send(sockfd, packet, packetSize, 0)) == -1) {
        perror("client: send");
        exit(1);
    }

    // printf("sent %d to server \n", numbytes);

    //while (1) {

    //}

    if ((numbytes = recv(sockfd, result, MAXBUFLEN, 0)) == -1) {
        perror("client: recv");
        exit(1);
    }

    //parse the packet to get the data
    char* garbage1;
    char* garbage2;
    char* garbage3;
    char* response;

    garbage1 = strtok(result, ":");
    garbage2 = strtok(NULL, ":");
    garbage3 = strtok(NULL, ":");
    response = strtok(NULL, ":");

    if (response != NULL)
        printf("%s\n", response);
    else
        printf(" \n");

    return 0;
}

int userHandler() {

    int numbytes;
    char userInput[MAXBUFLEN];
    char command[MAXBUFLEN];

    //Configure readline for autocomplete path
    //    rl_bind_key('\t',rl_complete); 
    fgets(userInput, sizeof (userInput), stdin);
//    autoComplete(userInput); 
    //    userInput_tabcomplete(userInput);


    int i = 0;
    while (userInput[i] != '\0') {
        if (userInput[i] == '\n')
            userInput[i] = '\0';
        i++;
    }

    strcpy(command, userInput);
    char* yes = strtok(command, " ");
    if (strcmp(yes, "/leavesession") == 0) {
        leave_session();
        return 0;
    }

    if (strcmp(yes, "/quit") == 0) {
        quit();
        return 0;
    }

    if (strcmp(yes, "/list") == 0) {
        list();
        return 0;
    }

    if (strcmp(yes, "/logout") == 0) {
        logout();
        return 0;
    }

    int packetSize = hostSize + 4 + 2 + strlen(userInput) + 1;
    char* packet = packetize(MESSAGE, strlen(userInput), username, userInput, packetSize);

    // printf("packet to send: %s\n", packet);

    if ((numbytes = send(sockfd, packet, packetSize, 0)) == -1) {
        perror("client: send");
        exit(1);
    }

    // printf("sent %d to server \n", numbytes);
}

int receiveHandler() {

    int numbytes;
    char result[MAXBUFLEN];
    if ((numbytes = recv(sockfd, result, MAXBUFLEN, 0)) == -1) {
        perror("client: recv");
        exit(1);
    }

    char* garbage1;
    char* garbage2;
    char* source;
    char* response;

    garbage1 = strtok(result, ":");
    garbage2 = strtok(NULL, ":");
    source = strtok(NULL, ":");
    response = strtok(NULL, ":");
    printf("%s: %s\n", source, response);

    return 0;
}

void writeToFile(char* clientID, char* clientPass) {

    char* buf;
    char* buf2;
    char result[MAXBUFLEN];

    FILE *fp = fopen("/homes/m/moonjon1/Desktop/Server-LoginInformation.txt", "a");
    fseek(fp, 0L, SEEK_END);
    buf = "\nClientID: ";
    strcpy(result, buf);
    strcat(result, clientID);
    buf2 = "\nPassword: ";
    strcat(result, buf2);
    strcat(result, clientPass);

    fwrite(result, strlen(result), 1, fp);
    fclose(fp);

}

int checkUsername(char* clientID) {

    char database[MAXBUFLEN];
    char buf[MAXBUFLEN];
    int first = 0;

    FILE *fp = fopen("/homes/m/moonjon1/Desktop/Server-LoginInformation.txt", "r");
    fseek(fp, 0L, SEEK_SET);

    while (autoComplete(buf) != NULL) {
        if (first = 0) {
            first = 1;
            strcpy(database, buf);
        }
        strcat(database, buf);
    }
    fclose(fp);

    if (strstr(database, clientID) == NULL)
        return 0;

    else
        return -1;

}

void userInput_tabcomplete(char *userInput) {

    while ((userInput = readline("prompt> ")) != NULL) { //Prompt the user for input
        //Currently we just print out the input line


        //And exit if the user requested it
        if (strcmp(userInput, "exit") == 0 || strcmp(userInput, "quit") == 0) {
            break;
        }

        //readline() allocates a new string for every line,
        //so we need to free the current one after we've finished
        free(userInput);
        userInput = NULL; //Mark it null to show we freed it
    }

    //If the buffer wasn't freed in the main loop we need to free it now
    // Note: if buf is NULL free does nothing
    free(userInput);

}

char** my_completion(const char * text, int start, int end) {
    char **matches;

    matches = (char **) NULL;

    matches = rl_completion_matches((char*) text, &my_generator);

    return (matches);

}

char* my_generator(const char* text, int state) {
    static int list_index, len;
    char *name;

    if (!state) {
        list_index = 0;
        len = strlen(text);
    }

    while (name = cmd[list_index]) {
        list_index++;

        if (strncmp(name, text, len) == 0)
            return (dupstr(name));
    }

    /* If no names matched, then return NULL. */
    return ((char *) NULL);

}

char * dupstr(char* s) {
    char *r;

    r = (char*) xmalloc((strlen(s) + LEN));
    strcpy(r, s);
    return (r);
}

void * xmalloc(int size) {
    void *buf;

    buf = malloc(size);
    if (!buf) {
        fprintf(stderr, "Error: Out of memory. Exiting.'n");
        exit(1);
    }

    return buf;
}

int autoComplete(char userInput[]) {
    char *buf;
    rl_bind_key('\t', rl_complete);
    rl_completer_quote_characters = strdup("\"\'");

    rl_attempted_completion_function = my_completion;

    while ((buf = readline("")) != NULL) {
        //enable auto-complete
        if (strcmp(buf, "") != 0) {
            add_history(buf);
        }

        break;
        if (buf != NULL) {
            free(buf);
        }
        buf = NULL;
    }
    if (buf == NULL) {
        return 0;
    }

    strcpy(userInput, buf);
    if (buf != NULL) {
        free(buf);
        buf = NULL;
    }
    return 1;
}

int kbhit() {
 struct termios oldt, newt;
  int ch;
  int oldf;
 
  tcgetattr(STDIN_FILENO, &oldt);
  newt = oldt;
  newt.c_lflag &= ~(ICANON | ECHO);
  tcsetattr(STDIN_FILENO, TCSANOW, &newt);
  oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
  fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);
 
  ch = getchar();
 
  tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
  fcntl(STDIN_FILENO, F_SETFL, oldf);
 
  if(ch != EOF)
  {
    ungetc(ch, stdin);
    return 1;
  }
 
  return 0;
}
//Referenced to https://cboard.cprogramming.com/c-programming/63166-kbhit-linux.html
int main(int argc, char *argv[]) {

    
    //parameters for parser

    char* command;
    int uargc = 0;
    //dynamic string array that holds user inputs
    char** uargv;
    uargv = (char**) malloc(sizeof (char**));

    if (argc != 1) {
        fprintf(stderr, "usage: server\n");
        exit(1);
    }

loopOne:

    //parse user input
    while (1) {
        printf("please enter one of the following commands:\n/login\n/logout\n/register\n/quit\n");
        //        fgets(userInput, sizeof (userInput), stdin);

        char userInput[BUFFER] = {'\0'};
        autoComplete(userInput);
        uargc = 0;

        //removing the trailing newline character
        int i = 0;
        while (userInput[i] != '\0') {
            if (userInput[i] == '\n')
                userInput[i] = '\0';
            i++;
        }

        char* incr = userInput;
        while ((incr = strchr(incr, ' ')) != NULL) {
            uargc++;
            incr++;
        }

        command = strtok(userInput, " ");

        if (command == NULL) {
            printf("error: empty input, please try again\n");
            continue;
        }

        if (strcmp(command, "/login") == 0) {
            if (uargc != 4) {
                printf("login: required parameters <clientID> <password> <server-IP> <server-port> \n");
                continue;
            }

            //read in the parameters
            uargv = (char**) malloc(sizeof (char**));

            for (i = 0; i < uargc; i++) {
                uargv[i] = (char*) malloc(sizeof (char*));
                uargv[i] = strtok(NULL, " ");
                //printf("argv is %s \n", uargv[i]);
            }

            int error = login(uargv);
            if (!error) {
                loggedin = 1;
                strcpy(username, uargv[0]);
                printf("login successful, hello %s \n", uargv[0]);
                break;
            } else
                printf("login failed, please try again \n");

            continue;
        }

        if (strcmp(command, "/logout") == 0) {
            if (uargc != 0) {
                printf("logout: required parameters NONE\n");
                continue;
            }
            break;
        }

        if (strcmp(command, "/quit") == 0) {
            if (uargc != 0) {
                printf("quit: required parameters NONE\n");
                continue;
            }
            break;
        }

        if (strcmp(command, "/register") == 0) {
            if (uargc != 2) {
                printf("register: required parameters <clientID> <password> \n");
                continue;
            }

            //read in parameters
            uargv = (char**) malloc(sizeof (char**));

            for (i = 0; i < uargc; i++) {
                uargv[i] = (char*) malloc(sizeof (char*));
                uargv[i] = strtok(NULL, " ");
                //printf("argv is %s \n", uargv[i]);
            }
            int error = 0;
            error = checkUsername(uargv[0]);
            if (error) {
                printf("Username is already taken\n");
                continue;
            }

            writeToFile(uargv[0], uargv[1]);
            printf("registration complete, please log in\n");

            continue;
        }

        printf("error: invalid command \n");
    }

loopTwo:

    while (1) {
        printf("please enter one of the following commands:\n/joinsession\n/createsession\n/list\n/quit\n");
        char userInput[BUFFER] = {'\0'};
        autoComplete(userInput);
        uargc = 0;

        //removing the trailing newline character
        int i = 0;
        while (userInput[i] != '\0') {
            if (userInput[i] == '\n')
                userInput[i] = '\0';
            i++;
        }

        //figure out how many arguments were passed in by the user
        char* incr = userInput;
        while ((incr = strchr(incr, ' ')) != NULL) {
            uargc++;
            incr++;
        }

        command = strtok(userInput, " ");

        if (command == NULL) {
            printf("error: empty input, please try again\n");
            continue;
        }

        if (strcmp(command, "/joinsession") == 0) {
            if (uargc != 1) {
                printf("joinsession: required parameters <sessionID>\n");
                continue;
            }

            uargv[0] = strtok(NULL, " ");
            // printf("argv is %s \n", uargv[0]);

            int error = join_session(uargv[0]);
            if (!inSession) {
                printf("joining session failed, please try again\n");
                continue;
            }

            printf("successfully joined sesesion %s\n", argv[0]);
            // inSession = 1;
            break;
        }

        if (strcmp(command, "/createsession") == 0) {
            if (uargc != 0) {
                printf("createsession: required parameters <sessionID> \n");
                continue;
            }

            create_session();
            inSession = 1;
            printf("successfully created and joined session\n");
            break;
        }

        if (strcmp(command, "/list") == 0) {
            if (uargc != 0) {
                printf("list: required parameters NONE\n");
                continue;
            }
            list();
            continue;
        }

        //not implemented yet
        if (strcmp(command, "/register") == 0) {
            if (uargc != 2) {
                printf("register: required parameters <clientID> <password>\n");
                continue;
            }
            break;
        }

        if (strcmp(command, "/quit") == 0) {
            if (uargc != 0) {
                printf("quit: required parameters NONE\n");
                continue;
            }
            quit();
            return 0;
        }
    }

    printf("Commands: \n/leavesession\n/list\n/quit\n/logout\n");

    //user is in the session at this point
    fd_set readfds;

    while (1) {
        //actively poll both the socket connected with the server and STDIN
        //if STDIN is flagged, receive user input then send it
        //if the input is a command, handle accordingly
        //if socfd is flagged, receive packet from server
        //        rl_bind_key('\t', rl_complete);
        //Use our function for auto-complete
        //        rl_attempted_completion_functionC = auto_complete;
        //Tell readline to handle double and single quotes for us
        //        rl_completer_quote_characters = strdup("\"\'");

        FD_ZERO(&readfds);
        FD_SET(STDIN, &readfds);
        FD_SET(sockfd, &readfds);
        struct timeval tv;
        tv.tv_sec = 10;
        tv.tv_usec = 0;

        select(sockfd + 1, &readfds, NULL, NULL, &tv);

        if (FD_ISSET(sockfd, &readfds)) {
            receiveHandler();
            continue;
        }

        if (FD_ISSET(STDIN,&readfds)) {
            userHandler();
            continue;
        }

        if (wantQuit) {
            printf("bye bye\n");
            return 0;
        }

        if (wantLeave) {
            printf("left session\n");
            wantLeave = 0;
            inSession = 0;
            goto loopTwo;
        }

        if (loggedin == 0) {
            printf("logged out\n");
            goto loopOne;
        }
    }

    freeaddrinfo(servinfo);
    close(sockfd);
    free(uargv[0]);
    free(uargv[1]);
    free(uargv[2]);
    free(uargv[3]);
    free(uargv);
}

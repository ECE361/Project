/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   main.c
 * Author: moenchri
 *
 * Created on March 12, 2017, 11:02 PM
 */

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

//Constant parameters for the incoming/receival of data 
#define MAX_DATA  1025
#define MAX_NAME 100
#define BUFFER 2000
#define MAXSIZE 20
//Max clients that the server will listen too 
#define MAXCLIENTS 10
//Used for text conferencing where the value of 0 corresponds to an unconnected conference session  
#define NOSESSION  0

//Max number of sessions that can be present at once 
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

//Format for the receiveal of the incoming message 

struct serverMessage {
    unsigned int type;
    unsigned int size;
    unsigned char source[MAX_NAME]; //Represents the client ID
    unsigned char data[MAX_DATA]; //Message data
};

//A  0 session ID corresponds to a non-connected id 

struct clients {
    unsigned char ID[MAX_NAME]; //Represents the client ID
    unsigned char password[MAX_DATA]; //Max Password n
    int portNumber; //Initially set to zero
    int sessionID;
};

bool isServer(char input[]) { //This function checks to see if the input command involves server 
    int x = 0;
    int counter = 0; //Temporary counter to compare the ending values 

    char command[] = {"server"};
    int input_length = strlen(command); //Determining the length of the input

    for (x = 0; x < input_length; x++) {
        if (command[x] == input[x]) {
            counter++;
        }
    }
    if (counter == input_length) {
        return true;
    } else {
        return false;
    }
}

int portNumberLength(char input[]) {
    int input_length = strlen(input);
    //Counts the length of the port number 
    int x = 0;
    //    server 1233
    int temp_Counter = 0;
    for (x = 0; x < input_length; x++) {
        if (x == ' ') {
            int y;
            y = x;
            while (input[y + 1] != '\0') {
                if (input[y] != '\0' || input[y] != ' ') {
                    temp_Counter++;
                }
                y++;
            }
        }
    }
    return temp_Counter;
}

int getPortNumber(char input[]) { //From the input command, this function finds the port number
    int input_length = strlen(input); //Determining the length of the input

    char temp_number[portNumberLength(input)];

    int x; //Temporary counter variable
    int y = 0;
    for (x = 0; x < input_length; x++) {

        if (input[x] == ' ') {

            do {
                x++;
                temp_number[y] = input[x];
                y++;
            } while (x + 1 < input_length);

            break; //Break out of the for loop because we have obtained the port number 
        }
    }
    //Now we can convert the port number "char" back to an "int"
    return atoi(temp_number);
}

//GetInt and inputSource Functions indexe the messages for the first parts which include the type,size,source and data portions of the message format 

void inputSource(struct serverMessage **packet, char message[], int *index) {
    int x = *index;
    int y = 0;
    char temp[100];

    while (message[x] != ':') {
        (*packet)->source[y] = message[x];

        x++;
        y++;

    }
    x++;
    *index = x;
    return;
}

void inputData(struct serverMessage **packet, char message[], int *index) {
    int x = *index;
    int y = 0;

    while (y < (*packet)->size) {
        (*packet)->data[y] = message[x];

        x++;
        y++;
    }

    x++;
    *index = x;

}

int getInt(char message[], int *index) {
    int x = *index;
    int y = 0;
    char temp[100];

    while (message[x] != ':') {
        temp[y] = message[x];

        x++;
        y++;

    }
    x++;
    *index = x;
    return atoi(temp);

}

//Splits the message up into its corresponding parameters...

void getDataInfo(struct serverMessage *packet, char message[]) {
    int index = 0;

    packet->type = getInt(message, &index);
    packet->size = getInt(message, &index);
    inputSource(&packet, message, &index);
    inputData(&packet, message, &index);
}
//Sending a control message to the client. The last index corresponds to the reason of the specified (could be a failure )

void controlSend(struct serverMessage packet, int type, int portNumber, char reason[]) {
    char control[BUFFER] = {'\0'};
    char size[MAXSIZE] = {'\0'};

    //    int dataSize = (sizeof (reason)) / (sizeof (reason[0]));
    int dataSize = strlen(reason);
    //    snprintf(size, 10, "%d", dataSize);

    snprintf(control, sizeof (control), "%d:%d:%s:%s", type, dataSize, packet.source, reason);

    send(portNumber, control, BUFFER, 0);

}
/*Takes the input struct packet, and checks to see if the user
 A) Already exists or 
 B) The client credentials match the existing hard coded information
  
 
 */
//Reads in the file to check if there server login information is correct...

bool isCorrectPassword(int index, int passwordSize, char readValues[], struct serverMessage packet) {
    int x = 0;
    while (readValues[index] != '\n') {
        int y = 0;
        int counter = 0;

        if (readValues[index] == packet.data[y]) {
            for (y; y < passwordSize; y++) {
                if (readValues[index + y] == packet.data[y])
                    counter++;
            }
            if (counter == passwordSize && readValues[index + y] == '\n') {
                return true;
            } else {
                return false;
            }
        }
        index++;
    }
    return false;

}

/*This first iteration first checks the client ID of the file*/
bool isClient_ID_Exist(struct serverMessage packet, int portNumber) {
    FILE *fp = fopen("/homes/m/moenchri/Desktop/Server LoginInformation.txt", "rb");
    char readValues[1000];
    int clientSize = strlen(packet.source);

    while (1) {
        int read = fread(readValues, 1, 1000, fp);
        if (read == 0) {
            fclose(fp);
            return false;
        }
        int x = 0;

        for (x = 0; x < read; x++) {
            int y = 0;
            int counter = 0;
            //If there is a particular value that has the similar match, then we will traverse through to see if it matches our client ID specified bv the packet (saves runtime)
            if (readValues[x] == packet.source[y]) {

                for (y; y < clientSize; y++) {
                    if (readValues[x + y] == packet.source[y])
                        counter++;
                }
                if (counter == clientSize) {

                    if (counter == clientSize && readValues[x + y] == '\n') {


                        //Now we check to see if this corresponds to the right password...
                        if (isCorrectPassword((x + y + 1), strlen(packet.data), readValues, packet)) {
                            fclose(fp);
                            return true;
                        } else {
                            //The client ID is correct, however, they have provided the wrong password 
                            printf("Server: Client ID is valid, but provided incorrect password\n");
                            controlSend(packet, LO_NACK, portNumber, "Incorrect password");

                            fclose(fp);
                            return false;
                        }

                    } else {
                        x += y;
                    }
                }

            }
        }

        //Exit meaning we don't have a valid client ID or a password 
        printf("Server: Client ID %s is invalid\n", packet.source);
        controlSend(packet, LO_NACK, portNumber, "Client ID and password do not match database");
        fclose(fp);
        return;
    }

}

//Returns true if the client space has NOT been logged in already on the client ID 

bool checkCurrentClients(struct clients currentClients[], struct serverMessage packet, int portNumber) {
    int x = 0;

    for (x; x < MAXCLIENTS; x++) {
        //Check to see if the client ID number matches an existing one, and the password they provided matches as well  
        if (
                strcmp(currentClients[x].ID, packet.source) == 0) {

            //Then we have an ID that has already been logged on 
            controlSend(packet, LO_NACK, portNumber, "Client of same ID already logged on");
            return false;
        }
    }
    //Client has not been logged yet 
    return true;

}
//add the client to the currentClients

void addClient(struct serverMessage packet, struct clients currentClients[], int portNumber) {
    int x = 0;
    //look for an opening in the strut
    for (x; x < MAXCLIENTS; x++) {
        //Then we have found an opening 
        if (currentClients[x].portNumber == 0) {

            //Copy all of the data into the struct holder 
            strncpy(currentClients[x].ID, packet.source, BUFFER);
            currentClients[x].portNumber = portNumber;
            //            strncpy(currentClients[x].ID, packet.source, BUFFER); 
            return;

        }
    }
    printf("Server: All clients are currently being used\n");
    //Then we have an ID that has already been logged on 
    controlSend(packet, LO_NACK, portNumber, "Server is currently at max capacity");
    return;
}

void checkLoginCredentials(struct serverMessage packet, struct clients currentClients[], int portNumber) {
    //Data of the packet corresponds to the password, ie packet->data
    if (checkCurrentClients(currentClients, packet, portNumber)) { //Check to see if the client is already in our database
        if (isClient_ID_Exist(packet, portNumber)) {
            printf("Server: Client ID %s connected\n", packet.source);
            //add the client to the currentClients
            addClient(packet, currentClients, portNumber);
            //Send the confirmation of a successful login
            controlSend(packet, LO_ACK, portNumber, "");
        }

    } else {
        printf("Server: Client attempted a login while already connected on user ID %s\n", packet.source);
    }
}
//Set all clients to 0

void initializeClients(int clients[]) {
    int x = 0;
    for (x; x < MAXCLIENTS; x++) {
        clients[x] = 0;
    }
}

//Add the specified socket into the array of client sockets 

void addClientSocket(int clientSockets[], int newSocket) {
    int x = 0;
    for (x; x < MAXCLIENTS; x++) {
        //Add the empty space to the list of clients 
        if (clientSockets[x] == 0) {
            clientSockets[x] = newSocket;
            return;
        }
    }


}

void initalizeArrayZero(struct serverMessage *packet) {
    int x = 0;
    for (x; x < MAX_DATA; x++) {
        packet->data[x] = '\0';
    }
    x = 0;
    for (x; x < MAX_NAME; x++) {
        packet->source[x] = '\0';
    }
}

void arrayToZero(struct clients* currentClients) {
    int x = 0;
    for (x; x < MAX_DATA; x++) {
        currentClients->ID[x] = '\0';
    }
    x = 0;
    for (x; x < MAX_NAME; x++) {
        currentClients->password[x] = '\0';
    }
}

//With the given packet information, we will remove it from the list of all connected clients 

void removeClient(struct serverMessage packet, struct clients currentClients[]) {

    int x = 0;
    for (x; x < MAXCLIENTS; x++) {
        if (strcmp(packet.source, currentClients[x].ID) == 0) {
            //initiate everything to NULL
            arrayToZero(&currentClients[x]);
            printf("Server: Client ID %s has left the server and conference session %d \n", packet.source, currentClients[x].sessionID);
            currentClients[x].portNumber = 0;
            currentClients[x].sessionID = 0;

            return;
        }
    }

}

//check to see if the client ID exists 

bool doesSessionExist(int sessionID, struct clients currentClients[]) {
    int x = 0;
    for (x; x < MAXCLIENTS; x++) {
        if (currentClients[x].sessionID == sessionID) {
            return true;
        }

    }
    return false; //Otherwise, it doesn't exist 
}
//Loop through to match the client ID and then place the specified conference session 

void setConferenceSession(struct clients currentClients[], struct serverMessage packet, int portNumber) {
    int sessionid = atoi(packet.data); //convert the packet "data" to the conference session ID 
    int x = 0;
    for (x; x < MAXCLIENTS; x++) {
        if (strcmp(currentClients[x].ID, packet.source) == 0) {
            if (doesSessionExist(sessionid, currentClients)) {
                //Then we have a client ID match 
                currentClients[x].sessionID = sessionid;
                printf("Server: Client ID %s joined the conference session %d\n", packet.source, sessionid);
                //Send a confirmation to the 
                controlSend(packet, JN_ACK, portNumber, packet.data);
            } else {
                //Return nack to the client 
                controlSend(packet, JN_NACK, portNumber, "Conference session ID does not exist");
                return;
            }
        }
    }

}

int getClientConferenceID(struct clients currentClients[], struct serverMessage packet) {
    int x = 0;
    //Loop through all of the clients 
    for (x; x < MAXCLIENTS; x++) {

        if (strcmp(packet.source, currentClients[x].ID) == 0) {
            return currentClients[x].sessionID; //Return that client's session ID 
        }
    }
}
//Send the message to all of the clients currently on the conference session 

void sendConferenceMessage(struct clients currentClients[], struct serverMessage packet) {
    int x = 0;
    int conferenceID = getClientConferenceID(currentClients, packet); //We have to first get the conference ID of the client sending the message first 
    //Loop through all of the current clients, and send the message to anyone with that same conference session ID
    for (x; x < MAXCLIENTS; x++) {
        if (conferenceID == currentClients[x].sessionID
                && strcmp(packet.source, currentClients[x].ID) != 0) {


            //as long as the conference IDs match. We do not send the message to the same client that sent it initially
            controlSend(packet, MESSAGE, currentClients[x].portNumber, packet.data);

        }
    }

}

void copyInformationOver(char online[], char temp[], int *index) {
    int length_temp = strlen(temp);

    int x = 0;
    int y = *index;

    for (x; x < length_temp; x++) {
        online[y] = temp[x];
        y++;
    }

    *index = y;

}
//loops through all of the current clients in the database, and returns their corresponding 
//Session numberand client ID 

void sendQueryMessage(struct clients currentClients[], struct serverMessage packet, int portNumber) {

    char online[BUFFER] = {'\0'};
    int index = 0;
    int x = 0;
    for (x; x < MAXCLIENTS; x++) {
        char temp[BUFFER] = {'\0'}; //used to store the client ids and their sessions
        if (currentClients[x].sessionID != 0) {

            //If the user is registered with a specific port number 
            //            strcpy(online,"Client ID: %s on session %d\n", currentClients[x].ID, currentClients[x].sessionID); 
            snprintf(temp, sizeof (temp), "Client ID %s on session %d\n", currentClients[x].ID, currentClients[x].sessionID);
            copyInformationOver(online, temp, &index);
            
        }
    }
    printf("Server: Sending list of online users to client ID %s\n", packet.source);
    controlSend(packet, QU_ACK, portNumber, online);

}

void leaveConference(struct clients currentClients[], struct serverMessage packet, int portNumber) {
    //Loop through all of the clients, and find the one corresponding to the message sender. Then, set their conference session ID  to 0
    int x = 0;
    for (x; x < MAXCLIENTS; x++) {
        if (strcmp(currentClients[x].ID, packet.source) == 0) {
            printf("Server: Client ID %s has left a conference session ID %d\n", packet.source, currentClients[x].sessionID);
            //Client ID matches 
            currentClients[x].sessionID = 0; //set the ID to 0
            return;
        }
    }


}
//check if the session ID exists or nah

bool idAlreadyExists(struct clients currentClients[], int randomSessionID) {
    int x = 0;
    for (x; x < MAXCLIENTS; x++) {
        if (currentClients[x].sessionID == randomSessionID) {
            return true; //This means that the randomly generated session ID already exists 
        }
    }
    return false;
}
//Loops through all the clients and sets the client conference session ID to the new one they have specified. 

void setConferenceID(struct clients currentClients[], struct serverMessage packet, int sessionID) {
    int x = 0;
    for (x; x < MAXCLIENTS; x++) {
        if (strcmp(packet.source, currentClients[x].ID) == 0) {
            if (currentClients[x].sessionID != 0) {
                printf("Server: Client ID %s is leaving session ID %d\n", packet.source, currentClients[x].sessionID);
            }
            currentClients[x].sessionID = sessionID;
            return;
        }
    }

}

void generateConferenceSession(struct clients currentClients[], struct serverMessage packet, int portNumber) {

    while (1) {
        //Random session ID from 1 -> the maximum amount of conference  sesssions 
        int randomSessionID = rand() % MAXCONFERENCESESSIONS + 1;
        if (idAlreadyExists(currentClients, randomSessionID) == false) {
            char randomSession[MAX_DATA] = {'\0'};
            //set the conference ID of the client 
            setConferenceID(currentClients, packet, randomSessionID);
            snprintf(randomSession, 10, "%d", randomSessionID);
            printf("Server: Client ID %s created a conference with session ID %d\n", packet.source, randomSessionID);
            controlSend(packet, NS_ACK, portNumber, randomSession);
            return;
        }
    }
}

void manageIncomingMessage(char buffer[], struct clients currentClients[], int portNumber) {

    struct serverMessage packet;
    initalizeArrayZero(&packet);
    getDataInfo(&packet, buffer);

    if (packet.type == LOGIN) {
        /*Type 0 corresponds to LOGIN*/
        checkLoginCredentials(packet, currentClients, portNumber);
    }

    //Exit the user from the database
    if (packet.type == EXIT) {
        //remove the user from the "currentClients" list 
        removeClient(packet, currentClients);
    }
    //User has joined a conference session 
    if (packet.type == JOIN) {
        setConferenceSession(currentClients, packet, portNumber);
    }
    //client is sending a message to the conference session
    if (packet.type == MESSAGE) {

        sendConferenceMessage(currentClients, packet);
        printf("Server: Sending message: '%s'to the conference session ID %d\n", packet.data, (getClientConferenceID(currentClients, packet)));

    }
    //Client requests a list of users that are currently online 
    if (packet.type == QUERY) {
        //Traverse through to get the list of clients that are online
        sendQueryMessage(currentClients, packet, portNumber);
    }
    //Client requests to leave a session 
    if (packet.type == LEAVE_SESS) {
        //Traverse through the clients and set their conference session ID to 0 i.e invalid session ID 
        leaveConference(currentClients, packet, portNumber);
    }
    //Client request a new conference session 
    if (packet.type == NEW_SESS) {
        //First we see which conference sessions are not taken, and then we generate a new one 
        //Number of sessions cannot exceed MAXCONFERENCESESSIONS
        generateConferenceSession(currentClients, packet, portNumber);

    }

}

void removeClient_socket(int socket, struct clients currentClients[]) {
    //Loop through all of the clients to remove 
    int x = 0;
    for (x; x < MAXCLIENTS; x++) {
        if (currentClients[x].portNumber == socket) {
            //initiate everything to NULL
            arrayToZero(&currentClients[x]);
            printf("Server: Client ID %s has disconnected from the server and left conference session %d \n", currentClients[x].ID, currentClients[x].sessionID);
            currentClients[x].portNumber = 0;
            currentClients[x].sessionID = 0;

        }
    }

}

//Function checks for prexisting clients

void checkPrexisting(int clientSockets[], struct sockaddr_in serverAddr, char buffer[], int addr_len, fd_set readfds, struct clients currentClients[]) {
    int sd;
    //Otherwise, we check to see if it is on a preexisting socket 
    int i = 0;
    for (i; i < MAXCLIENTS; i++) {
        char buffer1[BUFFER];

        sd = clientSockets[i];
        int readVal;
        //First we check to see if the client has disconnected 
        if (FD_ISSET(sd, &readfds)) {
            readVal = recv(sd, buffer, 2000, 0);
            if (readVal == 0) {
                getpeername(sd, (struct sockaddr*) &serverAddr, (socklen_t*) & addr_len);
                //Remove the socket 
                close(sd);
                //Clear the socket from the file 
                removeClient_socket(sd, currentClients);
                //and set in our list to zero
                clientSockets[i] = 0;

            } else {
                //Then we have a message pending from the client on an already connected client 
                manageIncomingMessage(buffer, currentClients, sd);
            }

        }
    }

}

void listenTCPSock(int portNumber) {

    int welcomeSocket, newSocket, sd, addr_len;


    char buffer[BUFFER];
    int clientSockets[MAXCLIENTS];

    struct sockaddr_in serverAddr;
    struct sockaddr_storage serverStorage;

    struct clients currentClients[MAXCLIENTS] = {
        {0}
    }; //Struct array to hold the max amount of clients 

    socklen_t addr_size;

    /*---- Create the socket. The three arguments are: ----*/
    /* 1) Internet domain 2) Stream socket 3) Default protocol (TCP in this case) */
    welcomeSocket = socket(PF_INET, SOCK_STREAM, 0);

    /*---- Configure settings of the server address struct ----*/
    /* Address family = Internet */
    serverAddr.sin_family = AF_INET;
    /* Set port number, using htons function to use proper byte order */
    serverAddr.sin_port = htons(portNumber);
    /* Set IP address to localhost */
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    /* Set all bits of the field to 0 */
    memset(serverAddr.sin_zero, '\0', sizeof serverAddr.sin_zero);

    /*---- Bind the address struct to the socket ----*/
    bind(welcomeSocket, (struct sockaddr *) &serverAddr, sizeof (serverAddr));
    /*---- Listen on the socket, with 5 max connection requests queued ----*/
    if (listen(welcomeSocket, MAXCLIENTS) == 0)
        printf("Server: Waiting for client(s)\n");
    else
        printf("Error\n");

    /*---- Accept call creates a new socket for the incoming connection ----*/
    addr_size = sizeof serverStorage;
    addr_len = sizeof (serverAddr);

    //Will be used to select incoming client connections 
    fd_set readfds;
    //Timeout to avoid infinite polling if there is no connection 
    struct timeval timeout;

    int fd = 0;
    int activity;

    //Initialize all the clients from to zero
    initializeClients(clientSockets);
    int maxsd;

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(welcomeSocket, &readfds);

        //        //Timeout for listening on connections 
        timeout.tv_sec = 0.8;
        timeout.tv_usec = 0;
        maxsd = welcomeSocket;

        int y = 0;
        //Add all the sockets to the set 
        for (y; y < MAXCLIENTS; y++) {
            sd = clientSockets[y];

            if (sd > 0) {
                //This means that the socket is a valid descriptor 
                FD_SET(sd, &readfds);
            }
            if (sd > maxsd) {
                maxsd = sd;
            }
        }
        //Waiting on the socket, with a timeout of 1 second 
        activity = select(maxsd + 1, &readfds, NULL, NULL, &timeout);

        //Something has happened on the socket
        if (FD_ISSET(welcomeSocket, &readfds)) {
            newSocket = accept(welcomeSocket, (struct sockaddr *) &serverAddr, (socklen_t*) & addr_len);

            //read the data from that socket 
            recv(newSocket, buffer, 2000, 0);
            manageIncomingMessage(buffer, currentClients, newSocket);

            //Add to array of sockets
            addClientSocket(clientSockets, newSocket);

        }
        //Now we check to see if there have been any messages on previously created sockets 
        checkPrexisting(clientSockets, serverAddr, buffer, addr_len, readfds, &currentClients);
    }

}

int main(int argc, char** argv) {
    const int COMMANDLENGTH = 50;
    char input[COMMANDLENGTH];
    printf("Command: ");
    while (fgets(input, COMMANDLENGTH, stdin) != NULL) { //loops through the commands to get the corresponding values for the server 
        if (isServer(input)) {
            //Get the port number from the user 
            int portNumber = getPortNumber(input);
            //Now we listen on the TCP socket port number specified for clients/requests 
            listenTCPSock(portNumber);

        } else {
            printf("Error, not a valid command");
        }
        printf("\n");
        printf("Command: ");
    }
}

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

#define MAX_DATA  1025
#define MAX_NAME 100
#define BUFFER 2000
#define MAXSIZE 20
#define MAXBUFLEN 2000
//Max clients that the server will listen too 
#define MAXCLIENTS 10
//Used for text conferencing where the value of 0 corresponds to an unconnected conference session  
#define NOSESSION  0

//Max number of sessions that can be present at once 
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
  
    if(strlen(error) < 10)
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
    
    if(response != NULL)
        printf("%s\n", response);
    else
        printf(" \n");

    return 0;
}

int userHandler() {
    
    int numbytes;
    char userInput[MAXBUFLEN];
    char command[MAXBUFLEN];
    fgets(userInput, sizeof (userInput), stdin);
    
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

int main(int argc, char *argv[]) {

    //parameters for parser
    char userInput[BUFFER];
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
        fgets(userInput, sizeof (userInput), stdin);
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

        printf("error: invalid command \n");
    }

loopTwo:

    while (1) {
        printf("please enter one of the following commands:\n/joinsession\n/createsession\n/list\n/quit\n");

        fgets(userInput, sizeof (userInput), stdin);
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

        if (FD_ISSET(STDIN, &readfds)) {
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

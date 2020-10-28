#include <bits/stdc++.h>
#include <vector>
#include <iostream>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
using namespace std;

#define MAX_CLIENTS 20
#define BUFLEN 2048
#define head_len 61           // The total size of the message header

 
typedef struct msgHeader {
    uint32_t sender_ip;       // The IP address of the UDP client
    uint16_t sender_port;     // The port of the UDP client
}msgHeader;

typedef struct Message {
     
     msgHeader hdr;           // Header with IP and port
     char* topic;             // Message topic
     uint8_t type;            // Data type (0,1,2,3)
     int data_length;         // Total length of data             
     char* data;              // Data as received from the UDP client

} Message;

typedef struct QueueMsg{
    char* message;            // Message ready to be sent
    int size;                 // Size of the data portion of the message
} QueueMsg;

typedef struct Client {
     int sockfd;              // The client's socket
     char* id;                // The client's id
     bool active;             // True if the client is currently connected
     vector<QueueMsg> q;      // List of SF messages to send after reconnection
}Client;

typedef struct Subsc {
    int idx;                  // The subscriber's index in the client vector
    Subsc* next;              // A pointer to the next subscriber
}Subsc;

typedef struct Topic{
     char* name;              // The topic's name
     Subsc* sf_subsc;         // Linked list of store and forward subscribers
     Subsc* basic_subsc;      // Linked list of non-SF subscribers
}Topic;


vector<Client> clients;        // A vector of all clients (both active and not)
vector<Topic> topics;          // A vector of all topics



/* Adds a new client to the client vector */
void add_client(char* id, int sockfd){
     
     Client* new_client = (Client*) malloc(sizeof(Client));
     new_client->active = true;
     new_client->sockfd = sockfd;

     new_client->id = (char*) malloc(10*sizeof(char));
     strcpy(new_client->id, id);

     clients.push_back(*new_client);
}


/* Sends a message to one subscriber */
int send_msg(Client *client, char* message, int size){
    int bytes_send = 0;
    int bytes_remaining = size;
    int bytes_received;

    do {
        bytes_received = send(client->sockfd,&message[bytes_send],bytes_remaining,0);
        if(bytes_received < 0){
            perror("Cannot send message");
            return - 1;
        }
        bytes_send += bytes_received;
        bytes_remaining -= bytes_received;
            
    } while(bytes_remaining > 0);

    return bytes_send;
}


/*Checks if a given id already exists in the client list 
  - 1 = an active client exists with this ID, 
    0 = an unactive client exists with this ID
    1 = the ID is new*/
int checkID(char* id, int sockfd){ 

    for(uint i=0; i < clients.size(); i++){
        if(strcmp(clients[i].id, id) == 0){

           // If an active client already exists with the ID, the ID is not valid
           if (clients[i].active == true){
              return -1;
           } else { 
               // Otherwise, reactivate client
               clients[i].active = true;
               clients[i].sockfd = sockfd;
               return 0;
           }
        }
    }   
    return 1;
}


/* Sends any queued messages associated with an ID which has been reactivated */
int send_queued_msg(char* id){

    for(uint i=0; i < clients.size(); i++){
        if(strcmp(clients[i].id, id) == 0){
            for(uint j = 0; j < clients[i].q.size(); j++){

                //Send header first and data after
                int ret = send_msg(&clients[i], clients[i].q[j].message, 61);
                if(ret > -1){
                    ret = send_msg(&clients[i], &clients[i].q[j].message[61],
                    clients[i].q[j].size);
                    if(ret < 0) return -1;
                }
                    else return -1;
            }
            // Reset message queue
            clients[i].q.clear();
            return 1;
        }
    } 
return 0;
} 


/* Checks if a topic already exists in the topics list */
bool new_topic(char *topic){
    for(uint i=0; i < topics.size(); i++){
        if(strcmp(topics[i].name, topic) == 0)
           return false;
    }
    return true;
}


/* Adds a new topic to the vector of topics */
void add_topic(char* topic){
    Topic* new_topic = (Topic*) malloc(sizeof(Topic));
    new_topic->name = (char*) malloc(50 * sizeof(char));
    strcpy(new_topic->name, topic);
    new_topic->basic_subsc = NULL;
    new_topic->sf_subsc = NULL;
    topics.push_back(*new_topic);
}


/* Adds a message to the queue of a non-active SF subscriber */
void queue_msg(Client *client, char* message, int size){

    // Create a new queued message strcuture
     QueueMsg *new_qms = (QueueMsg*) malloc(sizeof(QueueMsg));
     new_qms->message = (char*) malloc((head_len + size)*sizeof(char));
     memcpy(new_qms->message,message,head_len+size);
     new_qms->size = size;
    
     // Add the message to the client's queue
     client->q.push_back(*new_qms);
}


/* Sends a message to all subscribers */
void broadcast_message(Message *m){

    // Determine message length
    int size = sizeof(uint16_t) + 2*sizeof(uint32_t) + 51*sizeof(char)
     + m->data_length;
    
    // Build the message 
    char* message = (char*) malloc(size* sizeof(char));
    memcpy(&message[0], &m->hdr.sender_ip,4);
    memcpy(&message[4], &m->hdr.sender_port,2);
    memcpy(&message[6],  m->topic, 50);
    memcpy(&message[56], &m->type, 1);
    memcpy(&message[57], &m->data_length, 4);
    memcpy(&message[61], m->data, m->data_length);

    //Search for the topic in the vector of topics
    for(uint i=0; i < topics.size(); i++){
        if(strcmp(topics[i].name, m->topic) == 0){

            //Send the message to all active basic subscribers
            Subsc *subs = topics[i].basic_subsc;
            while(subs != NULL){
               if(clients[subs->idx].active == true) {
                 if(send_msg(&clients[subs->idx], 
                      message, size - m->data_length) != -1){
                     send_msg(&clients[subs->idx], 
                        &message[head_len],  m->data_length);
                  }
               }
               subs = subs->next;
            }

            //Send the message to all SF subscribers
            subs = topics[i].sf_subsc;
            while(subs != NULL){
                if(clients[subs->idx].active == true){
                    if(send_msg(&clients[subs->idx], 
                        message, size - m->data_length) != -1)
                       send_msg(&clients[subs->idx], 
                          &message[head_len],  m->data_length);
                }
                else {
                    queue_msg(&clients[subs->idx],
                      message,m->data_length);
                }

               subs = subs->next;
            }
            break;
        }
    }
}

/* Removes a subscriber from a list of subscribers */
bool remove_subscriber(int SF, int topic_idx, int client_idx){
    Subsc* prev = NULL, *temp;

    if(SF == 1)
       temp = topics[topic_idx].sf_subsc;
    else 
       temp = topics[topic_idx].basic_subsc;
    
    // The list is NULL
    if(temp == NULL)
       return false;
    
    // Head of the list 
    if(temp->idx == client_idx){
       if(SF == 1)
            topics[topic_idx].sf_subsc = temp->next;
        else 
            topics[topic_idx].basic_subsc = temp->next;
    free(temp);
    return true;
    }
    
    // Search for the client_idx in the list
    while(temp != NULL && temp->idx != client_idx){
        prev = temp;
        temp = temp->next;
    }
    
    // Delete subscriber 
    if(temp != NULL && temp->idx == client_idx){
        prev->next = temp->next;
        free(temp);
        return true;
    }
    return false;
}


/* Adds a subscriber  a given list */
void add_subscriber(int SF, int topic_idx, int client_idx){
    Subsc* new_subsc = (Subsc*) malloc(sizeof(Subsc));
    new_subsc->idx = client_idx;
    new_subsc->next = NULL;

    Subsc *temp;

    if(SF == 1)
       temp = topics[topic_idx].sf_subsc;
    else 
       temp = topics[topic_idx].basic_subsc;
    

    if(temp == NULL){
        if(SF == 1)
            topics[topic_idx].sf_subsc = new_subsc;
        else 
            topics[topic_idx].basic_subsc = new_subsc;
        return;
    } else {
         new_subsc->next = temp;
                 if(SF == 1)
            topics[topic_idx].sf_subsc = new_subsc;
        else 
            topics[topic_idx].basic_subsc = new_subsc;
    }
}


/* Checks if a client is already subscribed to a topic */
bool already_subscribed(int SF, int topic_idx, int client_idx){
     
     // Check in the list of basic subscribers 
     Subsc* temp = topics[topic_idx].basic_subsc;
     
     while(temp != NULL){
         if(temp->idx == client_idx){

             // Update the SF parameter if necessary 
             if(SF == 1){
                remove_subscriber(0,topic_idx,client_idx);
                add_subscriber(topic_idx, SF ,client_idx);
             }
             return true;
         }
         temp = temp->next;
     }

      // Check in the list of SF subscribers 
      temp = topics[topic_idx].sf_subsc;
     
     while(temp != NULL){
         if(temp->idx == client_idx){

             // Update the SF parameter if necessary 
             if(SF == 0){
                remove_subscriber(1, topic_idx,client_idx);
                add_subscriber(topic_idx, SF,client_idx);
             }
             return true;
         }
         temp = temp->next;
     }
     return false;
}


/* Subscribes an existing client to a given topic */
bool subscribe(int sockfd, char* cmd){
    
    // Find the client's index in the vector of clients
    int client_idx = -1;
     for(uint i=0; i < clients.size(); i++){
         if(clients[i].sockfd == sockfd){
            client_idx = i;
            break;
         }
     }
     if(client_idx == -1){
        return false;
     }     
     // Determine the topic and the SF parameter
     char* topic = (char*) malloc(50*sizeof(char));
     strncpy(topic, cmd, 50); 
     int SF = cmd[50];

     // Create a new subscriber
     Subsc *new_subsc = (Subsc*) malloc(sizeof(Subsc));
     new_subsc->idx = client_idx;
     new_subsc->next = NULL;
     
     // Find the topic's index in the vector of topics
     int topic_idx = -1;
     for(uint i=0; i < topics.size(); i++){
         if(strcmp(topics[i].name, topic) == 0){
             topic_idx = i;
             break;
         }
     }
     // New topic 
     if(topic_idx == -1){
         // Place the new topic on the last position of the topics vector
        add_topic(topic);
        
        // Subscribe the client to the new topic
        add_subscriber(SF, topics.size()-1, client_idx);
        return true;
     }

     // Check if the client is already subscribed to the topic
     if(already_subscribed(SF, topic_idx, client_idx))
        return true;
     
    // Based on the SF parameter, determine the appropriate list for the client
    Subsc *temp;
     if(SF == 1)
        temp = topics[topic_idx].sf_subsc;
     else 
        temp = topics[topic_idx].basic_subsc;
    
    // In case of an empty list
    if(temp == NULL){
        if(SF == 1)
            topics[topic_idx].sf_subsc = new_subsc;
        else 
            topics[topic_idx].basic_subsc = new_subsc;
        return true;
    }
   
    // Place the client on the last position of the topic's list
    while(temp->next != NULL)
        temp = temp->next;

    temp->next = new_subsc;
    free(topic);
    return true;
}


/* Unubscribes an existing client to a given topic */
bool unsubscribe(int sockfd, char* cmd){

    // Find the client's index in the vector of clients
    int client_idx = -1;
     for(uint i=0; i < clients.size(); i++){
         if(clients[i].sockfd == sockfd){
            client_idx = i;
            break;
         }
     }
     // Cannot unsubscribe an unexisting client
     if(client_idx == -1){
        return false;
     }  
     // Determine the topic
     char* topic = (char*) malloc(50*sizeof(char));
     strncpy(topic, cmd, 50);
     
     // Find the topic's index in the vector of topics
     int topic_idx = -1;
     for(uint i=0; i < topics.size(); i++){
         if(strcmp(topics[i].name, topic) == 0){
             topic_idx = i;
             break;
         }
     }
     // Cannot unsubscribe an unexisting topic
     if(topic_idx == -1){
        return false;
     }
              
     // Check in the list of basic suscribers and then in the list of SF subscribers
     if(remove_subscriber(0, topic_idx , client_idx) == false){
        return remove_subscriber(1, topic_idx , client_idx);
     }
    free(topic);
    return true;
}


/* Returns a clients ID based on its socket */
char* get_client_id(int sockfd){
    for(uint i=0; i < clients.size(); i++){
        if(clients[i].sockfd == sockfd)
           return clients[i].id;
    }
    return NULL;
}


int main(int argc, char *argv[]) {

    fd_set readfd;                    // Set of read descriptors
    fd_set tempfd;                    // Temporary set of read descriptors
    fd_set writefd;                   // Set of write descriptors
	fd_set wtempfd;                   // Temporary set of write descriptors
    int retval;                       // Used for error handling
    char buffer[BUFLEN];              // Buffer used for sending and receiving data    
    int maxfd, new_sockfd;            // Auxiliary file descriptors
    int bytes_send, bytes_remaining;  // For send and receive operations
    int bytes_received;               
    struct sockaddr_in tcp_addr, udp_addr, client_addr;
    socklen_t clen;

    // Check argument count
    if(argc != 2){
       perror("Error! 2 arguments required");
       exit(0);
    }
    
    // Check port number
    uint16_t serv_port = atoi(argv[1]);
    if(serv_port < 0 || serv_port > 65535){
        perror("Error! Invalid port number");
        exit(0);
    }

    //Establish TCP connection
    int tcpfd = socket(PF_INET, SOCK_STREAM, 0);
    if(tcpfd < 0){
        perror("Error! Socket: Could not establish TCP connection");
        exit(0);
    }

    memset((char *) &tcp_addr, 0, sizeof(tcp_addr));
	tcp_addr.sin_family = AF_INET;
	tcp_addr.sin_port = htons(serv_port);
	tcp_addr.sin_addr.s_addr = INADDR_ANY;
    
    // Check if bind is successful
    retval = bind(tcpfd, (struct sockaddr *) &tcp_addr, sizeof(struct sockaddr));
    if(retval < 0){
       perror("Error! Bind: Could not establish TCP connection");
       close(tcpfd);
       exit(0);
    }
    
    // Check if listen is successful
    retval = listen(tcpfd, MAX_CLIENTS);
    if(retval < 0){
       perror("Error! Listen: Could not establish TCP connection");
       close(tcpfd);
       exit(0);
    }


    // Establish UDP connection
    int udpfd = socket(PF_INET, SOCK_DGRAM, 0);

    //Check if socket is successful
    if(udpfd < 0){
        perror("Error! Socket: Could not establish UDP connection");
        close(udpfd);
        exit(0);
    }

    memset((char *) &udp_addr, 0, sizeof(udp_addr));
	udp_addr.sin_family = AF_INET;
	udp_addr.sin_port = htons(serv_port);
	udp_addr.sin_addr.s_addr = INADDR_ANY;
    
    // Check if bind is successful
    bind(udpfd,(struct sockaddr*) &udp_addr, sizeof(struct sockaddr));
    if(retval < 0){
       perror("Error! Bind: Could not establish UDP connection");
       close(udpfd);
       exit(0);
    }
    
    // Initialize descriptor sets
    FD_ZERO(&readfd);
	FD_ZERO(&tempfd);
    FD_ZERO(&wtempfd);
    FD_ZERO(&writefd);

    // Add TCP sockfd to the set of file descriptors
    FD_SET(tcpfd, &readfd);

    // Add UDP sockfd to the set of file descriptors
    FD_SET(udpfd, &readfd);

    // Add stdin to the set of file descriptors
	FD_SET(STDIN_FILENO, &readfd);

    // Determine the maximum value for a descriptor
	maxfd = max(tcpfd, udpfd);

    // Disable Nagle for TCP
    int flag = 1;
    int result = setsockopt(tcpfd, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));
    if (result < 0){
        perror("Could not disable Nagle");
    }

    while(1) {

        // Determine which descriptors are available
        tempfd = readfd; 
        wtempfd = writefd;
        retval = select(maxfd + 1, &tempfd, &wtempfd, NULL, NULL); 

        // Handle error if select fails
        if(retval < 0){
           perror("Error! Select call failed");
           exit(0);
        }
        
        //Input received from stdin
        if(FD_ISSET(STDIN_FILENO, &tempfd)){

            // Read input
			memset(buffer, 0, BUFLEN); 
			fgets(buffer, BUFLEN - 1, stdin);
            
            // Ignore frontal whitespaces
            int i = 0;
            while(buffer[i] == ' '){
                 i++;
            }
            // If the input is "exit", the server is closed
			if (strncmp(&buffer[i], "exit", 4) == 0) {
				break;
			}
        }

        for(int i=0; i <= maxfd; i++){ 

            if(FD_ISSET(i, &tempfd)) {

                // Connection request from the TCP socket
                if(i == tcpfd) {

                   // Accept the connection
                   clen = sizeof(client_addr);
                   memset(&client_addr,0,sizeof(struct in_addr));
                   new_sockfd = accept(tcpfd, (struct sockaddr*)&client_addr, &clen);
                   
                   // Check if the sockfd is valid
                   if(new_sockfd < 0){
                      perror("Error! Connecting to TCP client failed");
                      continue;
                   }

                    // Disable Nagle on the new socket
                    int result = setsockopt(tcpfd, IPPROTO_TCP, TCP_NODELAY, 
                       (char *) &flag, sizeof(int));
                    if (result < 0){
                        perror("Could not disable Nagle");
                    }
                    
                    // Receive the client's ID
                    char* client_id = (char*) malloc(10 * sizeof(char));
                    memset(buffer, 0, BUFLEN);
					bytes_received = recv(new_sockfd, buffer, sizeof(buffer), 0);
                    strcpy(client_id,buffer);

                    while (bytes_received < 10){
                        memset(buffer, 0, BUFLEN);
					    bytes_received += recv(new_sockfd, buffer, sizeof(buffer), 0);
                        strcat(client_id,buffer);
                    }
                
                    // Check if the id already exists and update its status and descriptor
                    int id_type = checkID(client_id, new_sockfd);

                    // Send error if the ID is already used by an active client
                    if(id_type == -1){
                        close(new_sockfd);
                        errno = ENOTUNIQ;
                        perror("Error! Client ID already used");
                        continue;
                    }
                    
                    // Send a byte of acknowledgement for succesfull connection
                    bytes_send = send(new_sockfd,&client_id[0],1,0);

                    // Add the descriptor to the set of descriptors
                    FD_SET(new_sockfd, &readfd);
                    FD_SET(new_sockfd, &writefd);
                    if(new_sockfd > maxfd){
                       maxfd = new_sockfd;
                    }

                    // Add a new client if the client does not already exist
                    if(id_type != 0)
                       add_client(client_id,new_sockfd);
                    else {

                        // Send any queued messaged of an existing client
                        retval = send_queued_msg(client_id);
                        if(retval < 0){
                            perror("Cannot send stored messages");
                        }
                    }
                    
                    // Output connection message
                    printf("New client %s connected from %s:%d\n", client_id, 
                        inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

                    free(client_id);            
                    continue;
                } 
                
                // Data received from the UDP socket
                if(i == udpfd) {
                    
                    // Receive data in buffer
                    clen = sizeof(client_addr);
                    memset(&buffer,0,sizeof(buffer));
                    memset(&client_addr,0,sizeof(struct in_addr));
                    int nr_received = recvfrom(udpfd, buffer, sizeof(buffer), 0, 
                        (struct sockaddr*) &client_addr, &clen);
                    
                   // Check transmission
                    if(nr_received < 52){
                       perror("Error! Receive from UDP failed");
                       continue;
                    }                      
                    // Create a new message
                    Message* new_m = (Message*) malloc(sizeof(Message));

                    // Extract the topic
                    new_m->topic = (char*) malloc(51 * sizeof(char));
                    strncpy(new_m->topic, buffer, 50);
                    new_m->topic[50] = '\0';
                    
                    // Check if the topic is valid
                    if(strlen(new_m->topic) == 0){
                       perror("Error! Received invalid message. Wrong topic");
                       continue;
                    }     

                    // Extract the type and check if it's valid       
                    uint8_t type = buffer[50];
                    if(!(type >= 0 && type < 4)){
                       perror("Error! Received invalid message. Wrong type");
                       continue;
                    }

                    // Complete the ip and port of the sender
                    new_m->type = type;
                    new_m->hdr.sender_ip = client_addr.sin_addr.s_addr; 
                    new_m->hdr.sender_port = client_addr.sin_port;

                    switch (type) {
                    case 0:
                         // Type 0: a sign byte + 4 bytes int
                         new_m->data = (char*) malloc(5*sizeof(char));
                         memcpy(new_m->data, &buffer[51], 5);
                         new_m->data_length = 5;
                         break;

                    case 1:
                        // Type 1: 2 bytes short
                         new_m->data = (char*) malloc(2*sizeof(char));
                         memcpy(new_m->data, &buffer[51], 2);
                         new_m->data_length = 2;
                         break;
                    
                    case 2:
                        // Type 2: 1 byte sign + 4 bytes float + 1 byte precision
                        new_m->data = (char*) malloc(6*sizeof(char));
                        memcpy(new_m->data, &buffer[51], 6);
                        new_m->data_length = 6;
                        break;

                    case 3:
                        // Type 3: a string of max 1500 bytes
                        new_m->data = (char*) malloc(1500*sizeof(char));
                        strncpy(new_m->data, &buffer[51], nr_received - 51);
                        new_m->data_length = nr_received - 51;
                        break;

                    default:
                        break;
                    }

                    if(new_topic(new_m->topic)){
                        // Add a new topic
                        add_topic(new_m->topic);
                    } else {
                        // Send the message to all subscribers
                        broadcast_message(new_m);
                    }
                    free(new_m);
                }
                // Data received from one of the TCP subscribers
                 else { 
                    
                    // Receive the command
                    memset(buffer, 0, BUFLEN);
					bytes_received = recv(i, buffer, sizeof(buffer), 0);

                    // Check transmission
                    if(bytes_received < 0){
                       perror("Error! Receive from TCP client failed");
                       continue;
                    } 

                    // Check if the connection was closed
                    if(bytes_received == 0){
                        
                       // Print disconnection message
                       printf("Client %s disconnected.\n", get_client_id(i));
                       
                       // Make client unactive
                       for(uint j=0; j < clients.size(); j++){
                           if(clients[j].sockfd == i){
                               clients[j].active = false;
                               break;
                           }
                       }

                       // Close socket and remove it from the descriptor set
                       close(i);
                       FD_CLR(i, &readfd);
                       FD_CLR(i, &writefd);
                       continue;
                    }  
                    
                    // Check if the command is "subscribe"
                    if(strncmp(buffer, "subscribe", 9) == 0){
                        
                        // We expect to receive 50 bytes of topic and 1 byte for SF
                        char* cmd = (char*) malloc(52*sizeof(char));
                        bytes_remaining = 51;
                        memset(buffer, 0, BUFLEN);

					    bytes_received = recv(i, buffer, bytes_remaining, 0);
                        memcpy(cmd, buffer,bytes_received);
                        bytes_remaining -=  bytes_received;
                        bytes_send = bytes_received;
                        
                        // Keep receiving data untill bytes_remaining = 0
                        while (bytes_remaining > 0) {
                            memset(buffer, 0, BUFLEN);
                            bytes_received = recv(i, buffer, bytes_remaining, 0);

                            memcpy(cmd,&buffer[bytes_send],bytes_received);
                            bytes_remaining -= bytes_received;
                            bytes_send += bytes_received;
                        }

                        bool success = subscribe(i,cmd);

                        // Check if the subscription was successful and notify the client
                        if(success)
                           strcpy(buffer, "SUCCESS");
                        else 
                           strcpy(buffer, "FAILURE");

                        bytes_received = send(i,buffer,7,0);
                        if(bytes_received < 0){
                            perror("The TCP client did not receive the acknowledgement");
                        }
                       free(cmd);                    
                       continue;
                    }
                    
                    // Check if the command is "unsubscribe"
                    if(strncmp(buffer, "unsubscribe", 11) == 0){

                        // We expect to receive 50 bytes of topic
                        char* cmd = (char*) malloc(51*sizeof(char));
                        bytes_remaining = 50;
                        memset(buffer, 0, BUFLEN);

					    bytes_received = recv(i, buffer, bytes_remaining, 0);
                        memcpy(cmd, buffer,bytes_received);
                        bytes_remaining -=  bytes_received;
                        bytes_send = bytes_received;
                        
                        // Keep receiving data untill bytes_remaining = 0
                        while (bytes_remaining > 0) {
                            memset(buffer, 0, BUFLEN);
                            bytes_received = recv(i, buffer, bytes_remaining, 0);

                            memcpy(cmd,&buffer[bytes_send],bytes_received);
                            bytes_remaining -= bytes_received;
                            bytes_send += bytes_received;
                        }

                        bool success = unsubscribe(i,cmd);

                        // Check if the unsubscription was successful and notify the client
                        
                        if(success)
                           strcpy(buffer, "SUCCESS");
                        else 
                           strcpy(buffer, "FAILURE");

                        bytes_received = send(i,buffer,7,0);
                        if(bytes_received < 0){
                            perror("The TCP client did not receive the acknowledgement");
                        }

                       free(cmd);
                       continue;
                    }
                
                    // Invalid command name
                    perror("Error! Invalid command from TCP client");
                       continue;
                }
            }
        }
    }
    
    // Close sockets 
    close(tcpfd);
    close(udpfd);
    for(int i=0; i <= maxfd; i++) 
       if(FD_ISSET(i, &readfd) || FD_ISSET(i, &writefd)){
           shutdown(i,0);
       } 
}


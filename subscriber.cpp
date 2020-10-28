#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <math.h>

#define BUFLEN 2048
#define head_len 61             // The total size of the message header

typedef union data_types {
    long int_data;              // A signed long integer
    float float_data;           // A short real or a float number
    char* string_data;          // A string of maximum 1500 characters
}data_types; 
 
typedef struct msgHeader {
    uint32_t sender_ip;         // The IP address of the sender
    uint16_t sender_port;       // The port of the sender
}msgHeader;

typedef struct Message {
     
     msgHeader hdr;             // Header with IP and port 
     char* topic;               // Message topic
     uint8_t type;              // Data type (0,1,2,3)
     int data_length;           // The total length of data
     data_types data;           // Data as an int, a float or a string

} Message;

/* Prints a message received from the server */
void printMessage(Message m){
    char* topic = (char*) malloc(50 * sizeof(char));
    strcpy(topic, &m.topic[0]);
    struct in_addr inaddr;
    inaddr.s_addr = m.hdr.sender_ip; 

    if(m.type == 0)
    printf("%s : %d - %s - %d - %ld\n", inet_ntoa(inaddr), htons(m.hdr.sender_port),topic, m.type, m.data.int_data);

    if(m.type == 1)
    printf("%s : %d - %s - %d - %0.2f\n", inet_ntoa(inaddr), htons(m.hdr.sender_port), topic, m.type, m.data.float_data);

    if(m.type == 2)
    printf("%s : %d - %s - %d - %0.4f\n", inet_ntoa(inaddr), htons(m.hdr.sender_port), topic, m.type, m.data.float_data);

    if(m.type == 3)
    printf("%s : %d - %s - %d - %s\n", inet_ntoa(inaddr), htons(m.hdr.sender_port), topic, m.type, m.data.string_data);
}

/* Sends a message to the server */
int send_msg(int sockfd, char* message, int size){
    int bytes_send = 0;
    int bytes_remaining = size;
    int bytes_received;

    do {
        bytes_received = send(sockfd,&message[bytes_send],bytes_remaining,0);
        if(bytes_received <= 0){
            perror("Could not send full message");
            return -1;
        }
        bytes_send += bytes_received;
        bytes_remaining -= bytes_received;
            
    } while(bytes_remaining > 0);

    return 0;
}

int main(int argc, char *argv[])
{
    struct sockaddr_in serv_addr;   // Used to establish TCP connection to server
    int retval;                     // Used for error handling
    fd_set read_fds;                // Set of read descriptors
	fd_set tmp_fds;                 // Temporary set of read descriptors
    fd_set write_fds;               // Set of write descriptors
	fd_set wtmp_fds;                // Temporary set of write descriptors
    char buffer[BUFLEN];            // Buffer used for sending and receiving data

    // Check argument count
	if (argc > 4) {
        errno = EINVAL;
		perror("Error! 3 arguments required");
        exit(0);
	}

    //Check the client ID
    char *id = (char*) malloc(10 * sizeof(char));
    memset(id,0,10);
    if(strlen(argv[1]) == 0 || strlen(argv[1]) > 10){
        errno = EINVAL;
        perror("Error! Invalid ID");
        exit(0);
    }
    strcpy(id, argv[1]);
    id[strlen(id)] = '\0';
    memset(&serv_addr,0,sizeof(serv_addr));

    //Check the server IP
    if(inet_aton(argv[2], &serv_addr.sin_addr) == 0){
        errno = EINVAL;
        perror("Error! Invalid IP");
        exit(0);
    }

    //Check the server port
    char *p;
    uint16_t serv_port = strtol(argv[3], &p, 10);
    if(*p || serv_port < 0 || serv_port > 65535){
        errno = EINVAL;
        perror("Error! Invalid port number");
        exit(0);
    }
    
    // Get a socket descriptor
	int sockfd = socket(PF_INET, SOCK_STREAM, 0);
	if(sockfd < 0){
        perror("Error! Socket: Could not establish connection");
        exit(0);
    }
    
	serv_addr.sin_family = AF_INET; 
	serv_addr.sin_port = htons(serv_port);

    // Establish connection to server 
    retval = connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
    if(retval < 0){
        close(sockfd);
        perror("Error! Connect: Could not establish connection");
        exit(0);
    } 
    
    //Send the client ID to the server 
    int bytes_send = 0;
    int bytes_remaining = 10;
    int bytes_received;
    do {
        bytes_received = send(sockfd,&id[bytes_send],bytes_remaining,0);
        bytes_send += bytes_received;
        bytes_remaining -= bytes_received;
            
    } while(bytes_remaining > 0);
    
    memset(buffer, 0, BUFLEN);
    bytes_received = recv(sockfd, buffer, 1, 0);

    // Check if the operation was succesfull
    if(bytes_received <= 0){
        errno = ECONNREFUSED;
        perror("Error! The server closed the connection; ID may not be unique");
        exit(0);
    }
    
    // Initialize descriptor sets
    FD_ZERO(&read_fds);
    FD_ZERO(&tmp_fds);
    FD_ZERO(&write_fds);
    FD_ZERO(&wtmp_fds);

    // Add sockfd to the set of file descriptors
    FD_SET(sockfd, &read_fds);
    FD_SET(sockfd, &write_fds);

    // Add stdin to the set of file descriptors
	FD_SET(STDIN_FILENO, &read_fds);

    // Disable Nagle for TCP
    int flag = 1;
    int result = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, 
    (char *) &flag, sizeof(int));
    if (result < 0){
        perror("Could not disable Nagle");
    }

	while (1) {
         // Determine which descriptors are available
		tmp_fds = read_fds; 
        wtmp_fds = write_fds;
		retval = select(sockfd + 1, &tmp_fds, &write_fds, NULL, NULL);

        // Handle error if select fails
		if(retval < 0){
            perror("Error! Select call failed");
            exit(0);
        }
        
		if(FD_ISSET(STDIN_FILENO, &tmp_fds)){

			// Read input from stdin
			memset(buffer, 0, BUFLEN); 
			fgets(buffer, BUFLEN - 1, stdin);
            
            // Ignore frontal whitespaces
            int i = 0;
            while(buffer[i] == ' '){
                 i++;
            }
            
            // If the input is "exit", the client is closed
			if (strncmp(&buffer[i], "exit", 4) == 0) {
				break;
			}
            
            // Process the "subscribe" command
            if (strncmp(&buffer[i], "subscribe ", 10) == 0) {
                char *word;
                word = strtok(&buffer[10+i], " ");
                
                // Error in case no arguments follow 'subscribe'
                if(word == NULL || strlen(word) == 0){
                    errno = EBADMSG;
                    perror("Error! Not a valid command");
                    continue;
                }
                
                // Extract the topic
                char* topic = (char*) malloc(50 * sizeof(char));
                strcpy(topic,word);
                
                // Error in case only 1 argument follows 'subscribe'
                word = strtok (NULL, " ");
                if(word == NULL || strlen(word) == 0 || word[0] == '\n'){
                    errno = EBADMSG;
                    perror("Error! Not a valid command");
                    continue;
                }
                
                //Extract SF value and check if it is a number
                char* sf = (char*) malloc(2*sizeof(char));
                strcpy(sf,&word[0]);

                if(sf[1] != '\n' && sf[1] != '\0'){
                    errno = EBADMSG;
                    perror("Error! Invalid SF value");
                    continue;
                }
                sf[1] = '\0';                     
                char *sp;
                int SF = strtol(sf, &sp, 10);
                
                // Error in case the SF value is not valid
                if(*sp || (SF != 0 && SF != 1)){
                    errno = EBADMSG;
                    perror("Error! Invalid SF value");
                    continue;
                }
                
                // Error in case more than 2 arguments follow 'subscribe'
                word = strtok (NULL, " ");
                if(word != NULL && word[0] != '\n'){
                    errno = EBADMSG;
                    perror("Error! To many arguments");
                    continue;
                }
                
                // Compose the message for the server
                memset(buffer,0,sizeof(buffer));
                strcpy(buffer, "subscribe");

                // Send the command to the server
                int bytes_received = send_msg(sockfd,buffer,9);
                if(bytes_received < 0){
                    perror("Could not send message to the server");
                    continue;
                } 
                
                // Compose the message for the server
                strncpy(buffer, topic, 50);
                memcpy(&buffer[50], &SF, 1);

                // Send command arguments to the server
                bytes_received = send_msg(sockfd,buffer,51);
                if(bytes_received < 0){
                    perror("Could not send message to the server");
                    continue;
                } 
                
                // Receive an acknowledgement from the server 
                memset(buffer, 0, BUFLEN);
			    bytes_received = recv(sockfd, buffer, sizeof(buffer), 0);
            
                // Check if receiving was successful
                if(bytes_received < 0){
                    perror("Error! Data not received");
                    continue;
                }
 
                // Output success message 
                if(strcmp(buffer, "SUCCESS") == 0){ 
                    printf("subscribed '%s'\n", topic);
                }
                free(topic);
                free(sf);
                continue;
			}

            // Process the "unsubscribe" command
            if (strncmp(&buffer[i], "unsubscribe ", 12) == 0) {
                char *word;
                word = strtok(&buffer[12+i], " ");
                
                // Error in case no arguments follow 'unsubscribe'
                if(word == NULL || strlen(word) == 0){
                    errno = EBADMSG;
                    perror("Error! Not a valid command");
                    continue;
                }
                
                // Extract the topic
                char* topic = (char*) malloc(50 * sizeof(char));
                strcpy(topic,word);
                if(topic[strlen(topic)-1] == '\n')
                    topic[strlen(topic)-1] = '\0';

                // Error in case more than 1 argument follows 'unsubscribe'
                word = strtok (NULL, " ");
                if(word != NULL && word[0] != '\n'){
                    errno = EBADMSG;
                    perror("Error! To many arguments");
                    continue;
                }
                
                 // Compose the message for the server
                memset(buffer,0,sizeof(buffer));
                strcpy(buffer, "unsubscribe");

                // Send the command to the server
                int bytes_received = send_msg(sockfd,buffer,11);
                if(bytes_received < 0){
                    perror("Could not send message to the server");
                    continue;
                } 
                
                // Compose the message for the server
                memcpy(buffer, topic, 50);

                // Send command arguments to the server
                bytes_received = send_msg(sockfd,buffer,50);
                if(bytes_received < 0){
                    perror("Could not send message to the server");
                    continue;
                } 
                
                // Receive an acknowledgement from the server 
                memset(buffer, 0, BUFLEN);
			    bytes_received = recv(sockfd, buffer, sizeof(buffer), 0);
            
                // Check if receiving was successful
                if(bytes_received < 0){
                    perror("Error! Data not received");
                    continue;
                }
 
                // Output success message 
                if(strcmp(buffer, "SUCCESS") == 0){ 
                    printf("unsubscribed '%s'\n", topic);
                }
                free(topic);
                continue;
			}

            // If any other command, output error
            errno = EBADMSG;
            perror("Error! Invalid command");
            continue;

		} else if (FD_ISSET(sockfd, &tmp_fds)){
            
           // First read the message header,the topic,the type and data length
            bytes_remaining = sizeof(u_int16_t) + 2*sizeof(u_int32_t) + 51*sizeof(char);
            char* msg = (char*) malloc((bytes_remaining + 1) * sizeof(char));
            memset(buffer, 0, BUFLEN);
			bytes_received = recv(sockfd, buffer, bytes_remaining, 0);
            
            memcpy(msg,buffer,bytes_received);
            bytes_remaining -=  bytes_received;
            bytes_send = bytes_received;

            // Check initial byte count
            if(bytes_received < 0){
                perror("Error! Data not received");
                continue;
            }

            // Read data until the whole header is read 
            while (bytes_remaining > 0){
                memset(buffer, 0, BUFLEN);
				bytes_received = recv(sockfd, buffer, bytes_remaining, 0);
                memcpy(msg,&buffer[bytes_send],bytes_received);
                bytes_remaining -= bytes_received;
                bytes_send += bytes_received;
            }
            
            // Build the header for the message structure
            Message* new_m = (Message*) malloc(sizeof(Message));
            new_m->topic = (char*) malloc(50*sizeof(char));
            memcpy(&new_m->hdr.sender_ip, msg, 4);
            memcpy(&new_m->hdr.sender_port, &msg[4], 2);
            memcpy(new_m->topic, &msg[6], 50);
            memcpy(&new_m->type, &msg[56], 1);
            memcpy(&new_m->data_length, &msg[57], 4);    
            free(msg);

            // Read message data 
            bytes_remaining = new_m->data_length;
            msg = (char*) malloc(bytes_remaining * sizeof(char));
            memset(buffer, 0, BUFLEN);
			bytes_received = recv(sockfd, buffer, bytes_remaining, 0);
            
            memcpy(msg,buffer,bytes_received);
            bytes_remaining -=  bytes_received;
            bytes_send = bytes_received;

            // Check initial byte count
            if(bytes_received < 0){
                perror("Error! Data not received");
                continue;
            }

            // Read data until the whole data is read 
            while (bytes_remaining > 0){
                memset(buffer, 0, BUFLEN);
				bytes_received = recv(sockfd, buffer, bytes_remaining, 0);
                memcpy(msg,&buffer[bytes_send],bytes_received);
                bytes_remaining -= bytes_received;
                bytes_send += bytes_received;
            }  

            //Determine the message payload
            switch (new_m->type) {
            case 0:
                // Case: singed int
                uint32_t nr;
                memcpy(&nr, &msg[1], 4);
                new_m->data.int_data = ntohl(nr);

                // Check sign byte
                if(msg[0] == 1){
                    new_m->data.int_data *= -1;
                }
                break;

            case 1:
                // Case: real short
                uint16_t sh_nr;
                memcpy(&sh_nr, msg , 2);
                new_m->data.float_data = ntohs(sh_nr);
                new_m->data.float_data = new_m->data.float_data/100;
                break;

            case 2:
                //Case: float
                uint8_t pow10;
                memcpy(&nr, &msg[1], 4); 
                nr = ntohl(nr);
                memcpy(&pow10, &msg[5], 1);

                //Determine floating point position and sign 
                new_m->data.float_data = nr * pow(10, -1 * pow10);
                if(msg[0] == 1){
                    new_m->data.float_data *= -1;
                }
                break;

            case 3:
                // Case: string
                new_m->data.string_data = (char*) malloc(new_m->data_length * sizeof(char));
                memcpy(new_m->data.string_data, msg, new_m->data_length);
                break;

                default:
                break;
            } 
            printMessage(*new_m);
            free(new_m);
            free(msg);
		}
	} 

	close(sockfd);
}
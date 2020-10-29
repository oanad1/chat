Oana Diaconescu 323 CD  

1.CONTENTS:
    * server.cpp = an implementation of a broker between TCP and UDP messages,
      capable of receiving messages from the UDP clients and sending them 
      to the TCP subscribers
    * subscriber.cpp = an implementation of a TCP client which sends information
      to the server about what topics to subscribe to and receives all updates 
      about those topics from the server
    * Makefile = 
      running 'make' builds the executables for the 2 sources
      running 'make run_server' runs the server executable 
      running 'make run_subscriber <ID>' runs the subscriber executable
      Static arguments PORT = 8080 and IP = 127.0.0.1 are used by default.


2.DATA STRUCTURES
     Used for both server and subscriber:
   * msgHdr = stores information about the sender; data is stored in network
     byte order
     (uint32_t) sender_ip = the IP address of the sender (the UDP client)
     (uint16_t) sender_port = the port of the sender

   * Message = the primary structure of the message sent between TCP and UDP
     (msgHdr)  hdr = explained above
     (char*)   topic = the message topic; contains exactly 50 bytes
     (uint8_t) type = determines the type of data stored: 0=singed long
                1=real short, 2=signed float, 3=string of maximum 1500 bytes
     (int)     data_length = the size of the data in bytes 
               data = has different types inside the server and subscriber 
                (for convenience); For the server, data is stored as a char*
                as received from the UDP client. Inside the subscriber, data
                is stored as an union between an int, a float and a string type.

      Used inside the server:
    * Client = stores data about a TCP client. All clients are stores in a vector.
      (int) sockfd = the file descriptor of the client
      (char*) id   = the ID of the client
      (bool) active = defines the client's current state: connected/disconnected
      (vector<QueueMsg>) q = A queue to store all messages for a client for topics
      marked SF=1, as long as the client is disconnected
     
    * QueueMsg = a structure used to hold a message until its client becomes active
      (char*) message = the entire message, formatted and ready to be sent
      (int) size = the size of the data portion of a message.The header has a constat
      length of 61 bytes.

    * Topic = a structure used to store a topic introduced by an UDP or TCP client
      All topics are stored inside a vector.
      (char*) name = the name of the topic; fixed size of 50 bytes
      (Subsc*) sf_subsc = a linked list of all store-and-froward subscribers associated
       with the topic
      (Subsc*) basic_subsc = a linked list of all non store-and-froward subscribers
    
     * Subsc = a simple strcuture used to mimic a subscriber 
      (int) idx = the index of the associated client (from the vector of clients) 
      (Subsc*) next = a pointer to the next subscriber on the list


3.PROTOCOL
      * The argument list is checked for both the server and subscriber. If there are not
        enough arguments or the arguments are unvalid, the server/subcriber closes

      * The server establishes connection to the TCP and UDP port. If a connection fails
        for any reason, an error message is displayed and the server closes. 

      * The subscriber establishes connection to the server TCP port. If it fails
        for any reason, an error message is displayed and the subscriber closes.

      * Nagle is disabled on all TCP ports on both the server and the client 

      * Upon connecting, a subscriber sends his ID to the server. The server checks if
        the ID is already stored in its 'database'. There are 3 possible scenarios:
           - The ID already exists associated with an active client, in which case the
             server aborts the connection and the client shuts down, printing an error
             message that lets the user know that the ID is not unique
          -  The ID already exists associated with a disconnected client. In this case,
             the new connection is consider to be the same client logging back in and 
             its status is changed to 'active'. Its sockfd is changed and any SF messages
             stored while the client was away are sent.
          -  The ID does not exits inside the server. A new client is added.
     
      * The server sends an acknowledgement back to the client to verify successful 
        connection.

      * The TCP clients receive input from stdin. A valid input is "subscribe <topic> <SF>"
        or "unsubscribe <topic>". The space between the arguments does not matter. Any
        other input or any input that contains invalid arguments is refused and an error
        message is printed to the user, pointing out the format mistake.

      * When a valid command is introduced, the TCP subscriber sends the command to the server.
        The name of the command is sent first, as the server needs to know how to process 
        the rest of the arguments. The topic (and SF) are simply conactenated, making sure
        that the topic has a fixed size of 50 bytes. The next part of the command is sent
        to the server and upon receiving it, the server subscribes/unsubscribes the user
        if possible. To acknowledge successful (un)subscription, the server send a "SUCCESS"
        message back to the TCP client. Upon receiving the acknowledgement, the TCP client
        prints out the result of the command.

      * The server can receive UDP data at any time. In this case, it creates a message
        from the data received, checking if the expected fields are valid. In case of an
        error, the message is aborted. Data is not processed inside the server, but stored
        as a char* of various size corresponding to its type. Once the message is created,
        the server checks if the topic exists in its list of topics. If it does not, it
        creates a new topic with this name and ignores the rest of the message. If it does,
        the server broadcasts the message to all of its subscribers.

      * If any of the subscribers are unactive but have SF status, the message is added to 
        the respective client's queue of messages and sent whenever he reconnects. The 
        message is formated and stored alongisde the size of its data in a QueueMSg structure.
      
      * When the TCP client receives data from the server, it first receives the message header
        consisting of the sender ip, sender port, topic, type and data length. As the header
        has a fixed length of 61 bytes, the clients knows how many bytes to expect to receive.
        It then fills the information inside a Message structre and use the data length field
        for receiving the data at the next step. According to the type of the message, the 
        data is processed, formatted, and placed inside the Message structure. In case any
        invalid information is detected, the client aborts the message. Finally, the message 
        is printed. 
     
      * Both the server and the subscribers close upon reciving the 'exit' command from stdin. 
        The server uses 'shutdown' to close its subscriber's connections, in order to forward any
        messages left unsent when the server was closed. 
      




 

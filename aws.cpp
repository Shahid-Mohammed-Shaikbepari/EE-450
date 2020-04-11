

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <string>
#include <iostream>

#define CLIENT_PORT "24475"  // the port users will be connecting to
#define MONITOR_PORT "25475"
#define MONITOR_ADDRESS "localhost"
#define SERVER_A_PORT "21475"
#define SERVER_B_PORT "22475"
#define BACKLOG 10     // how many pending connections queue will hold
#define MAXDATASIZE 1000 // max number of bytes we can get at once

using namespace std;
// code for socket setup and connection has been taken from Beej
void sigchld_handler(int s) {
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    while (waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
}

void process_request(char *buf, int sockfd);

string build_monitor_message(string *tokens);

void inform_monitor(string message);

char *forward_request_to_serverB(string responseA);

char *forward_request_to_serverA(string *tokens);


// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in *) sa)->sin_addr);
    }

    return &(((struct sockaddr_in6 *) sa)->sin6_addr);
}

//int main1() {
//    char *input = "compute 3 4 5";
//    process_request(input);
//
//}

int main() {
    int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size;
    struct sigaction sa;
    int yes = 1;
    char s[INET6_ADDRSTRLEN];
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, CLIENT_PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                       sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo); // all done with this structure

    if (p == NULL) {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    printf("The AWS is up and running.\n");

    while (1) {  // main accept() loop
        sin_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *) &their_addr, &sin_size);
        if (new_fd == -1) {
            perror("accept");
            continue;
        }

        inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *) &their_addr), s, sizeof s);
        //printf("server: got connection from %s\n", s);
        char buf[MAXDATASIZE];
        if (!fork()) { // this is the child process
            int numBytes;
            close(sockfd); // child doesn't need the listener
            if ((numBytes = recv(new_fd, buf, MAXDATASIZE - 1, 0)) == -1)
                perror("recv");
            buf[numBytes] = '\0';
            process_request(buf, new_fd);
            close(new_fd);
            //printf("connected to aws");
            exit(0);
        }
        close(new_fd);  // parent doesn't need this
    }

    return 0;
}


void process_request(char *buf, int sockfd) {

    char request[MAXDATASIZE];
    strcpy(request, buf);
    int count = 0;
    string tokens[5];
    const char delim[2] = " ";
    char *tok = strtok(request, delim);
    while (tok != NULL) {
        tokens[count++] = string(tok);
        tok = strtok(NULL, " ");
    }
    cout << "The AWS received operation <" + tokens[0] + "> from the client using TCP over port <24475>" << endl;
    string monitorMessage = build_monitor_message(tokens);
    inform_monitor(monitorMessage);
    if (tokens[0] == "write") {
        char *response_write_A = forward_request_to_serverA(tokens);
        if (response_write_A != nullptr) {
            inform_monitor("The write operation has been completed successfully");
            string msg = "The write operation has been completed successfully";
            strcpy(buf, msg.c_str());
            printf("The AWS sent write response to the monitor using TCP over port <25475>\n");
            //send the response to client
            printf("The AWS sent result to client for operation <write> using TCP over port <24475>\n");
            send(sockfd, buf, MAXDATASIZE - 1, 0);
        }
    }
    if (tokens[0] == "compute") {

        char *response_compute_A = forward_request_to_serverA(tokens);
        if (response_compute_A != nullptr) {
            char *notFoundMessage = "Link ID not found";
            if (strcmp(response_compute_A, notFoundMessage) == 0) {
                printf("%s\n", notFoundMessage);
                buf = notFoundMessage;
                send(sockfd, buf, MAXDATASIZE - 1, 0);
            } else {
                printf("The AWS received link information from Backend-Server A using UDP over port <23475>\n");
                printf("The AWS sent operation <compute> and arguments to the monitor using\n"
                       "TCP over port <25475>");
                printf("The AWS sent link ID=<%s >, size=<%s>, power=<%s>, and link information to Backend-Server "
                       "B using UDP over port <23475>\n", tokens[1].c_str(), tokens[2].c_str(), tokens[3].c_str());


                char *responseB = forward_request_to_serverB(response_compute_A);

                if (responseB != nullptr) {
                    printf("The AWS received outputs from Backend-Server B using UDP over port <23475>\n");
                    char request1[MAXDATASIZE];
                    strcpy(request1, responseB);
                    int count1 = 0;
                    string tokens1[5];
                    const char delim1[2] = " ";
                    char *tok1 = strtok(request1, delim1);
                    while (tok1 != NULL) {
                        tokens1[count1++] = string(tok1);
                        tok1 = strtok(NULL, " ");
                    }
                    string result =
                            "The result for link <" + tokens1[0] + "> :\n Tt = <" + tokens1[1] + ">ms,\nTp = <" +
                            tokens1[2] +
                            " >ms,\nDelay = <" + tokens1[3] + ">ms";
                    inform_monitor(result);
                    string msg = "The delay for link <" + tokens1[0] + "> is <" + tokens1[3] + ">ms";
                    strcpy(buf, msg.c_str());
                    send(sockfd, buf, MAXDATASIZE - 1, 0);

                    // string result = "The result for link <"+ tokens[1] +"> :\n Tt = <" + to_string(tt_in_ms) +">ms,\nTp = <"+to_string(tp_in_ms) +" >ms,\nDelay = <"+to_string(end_to_end_delay_ms) +">ms" ;
                    printf("The AWS sent compute results to the monitor using TCP over port <25475>\n");
                    printf("The AWS sent result to client for operation <compute> using TCP over port <24475>\n");
                }
            }
        }
    }//
}

string build_monitor_message(string *tokens) {
    string operation = tokens[0];
    char buf[MAXDATASIZE];
    if (operation == "write") {
        string result = "The monitor received BW = <" + tokens[1] + ">, L =<" +
                        tokens[2] + ">, V = <" + tokens[3] + " >and P = <" +
                        tokens[4] + " >from the AWS";
        return result;
    } else if (operation == "compute") {
        string result = "The monitor received link ID= <" +
                        tokens[1] + ">, size=<" + tokens[2] + ">, and power=<" + tokens[3] + "> from the AWS";
        return result;
    }
    //TODO: Handle search
    //“The write operation has been completed successfully”
    //“The result for link <LINK_ID>:
    //Tt = <Transmission Time>ms,
    //Tp = <Propagation Time>ms,
    //Delay = <Delay>ms”

    return "";
}

void inform_monitor(string str) {
    int sockfd;

    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if ((rv = getaddrinfo(MONITOR_ADDRESS, MONITOR_PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return;
    }
// loop through all the results and connect to the first we can
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1) {

            continue;
        }
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            continue;
        }
        break;
    }
    if (p == NULL) {
        fprintf(stderr, "aws: failed to connect to monitor\n");
        return;
    }
    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *) p->ai_addr), s, sizeof s);
    freeaddrinfo(servinfo); // all done with this structure
    if (send(sockfd, str.c_str(), MAXDATASIZE - 1, 0) == -1) {
        perror("Failed to send data to monitor");
    }
    close(sockfd);
}

char *forward_request_to_serverA(string *tokens) {

    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr;
    int rv;
    int numbytes;
    socklen_t addr_len;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    if ((rv = getaddrinfo(MONITOR_ADDRESS, SERVER_A_PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return nullptr;
    }
// loop through all the results and make a socket
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1) {
            perror("talker: socket");
            continue;
        }
        break;
    }
    if (p == NULL) {
        fprintf(stderr, "talker: failed to create socket\n");
        return nullptr;
    }
    string request = tokens[0] + " " + tokens[1] + " " + tokens[2] + " " + tokens[3] + " " + tokens[4];

    if ((numbytes = sendto(sockfd, request.c_str(), MAXDATASIZE - 1, 0,
                           p->ai_addr, p->ai_addrlen)) == -1) {
        perror("talker: sendto");
        exit(1);

    }

    freeaddrinfo(servinfo);
    cout << "The AWS sent operation <" << tokens[0] << " > to Backend-Server A using UDP over port <23475>" << endl;
    addr_len = sizeof their_addr;
    //write recv method from serverA
    char *buf1 = new char[MAXDATASIZE];
    if ((numbytes = recvfrom(sockfd, buf1, MAXDATASIZE - 1, 0,
                             (struct sockaddr *) &their_addr, &addr_len)) == -1) {
        perror("recvfrom");
        exit(1);
    }
    printf("The AWS received response from Backend-Server A for writing using UDP over port <23475>\n");
    close(sockfd);
    /* char* res = (char*)request.c_str();
     return res;*/
    return buf1;
}

char *forward_request_to_serverB(string responseA) {
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    int numbytes;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    if ((rv = getaddrinfo(MONITOR_ADDRESS, SERVER_B_PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return nullptr;
    }
// loop through all the results and make a socket
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1) {
            perror("talker: socket");
            continue;
        }
        break;
    }
    if (p == NULL) {
        fprintf(stderr, "talker: failed to create socket\n");
        return nullptr;
    }
    //string request = tokens[0] + " " + tokens[1] + " " + tokens[2] + " " + tokens[3] + " " +tokens[4];

    if ((numbytes = sendto(sockfd, responseA.c_str(), MAXDATASIZE - 1, 0,
                           p->ai_addr, p->ai_addrlen)) == -1) {
        perror("talker: sendto");
        exit(1);

    }

    freeaddrinfo(servinfo);
    //write recv method from serverA
    struct sockaddr_storage their_addr;
    socklen_t addr_len = sizeof their_addr;

    char *buf1 = new char[MAXDATASIZE];
    if ((numbytes = recvfrom(sockfd, buf1, MAXDATASIZE - 1, 0,
                             (struct sockaddr *) &their_addr, &addr_len)) == -1) {
        perror("recvfrom");
        exit(1);
    }
    close(sockfd);
    return buf1;
}




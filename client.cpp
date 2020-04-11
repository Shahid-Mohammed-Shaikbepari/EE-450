
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string>


#define AWS_PORT "24475" // the port client will be connecting to
#define AWS_ADDRESS "localhost"
#define MAXDATASIZE 1000 // max number of bytes we can get at once

using namespace std;

// code for socket setup and connection has been taken from Beej
void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in *) sa)->sin_addr);
    }
    return &(((struct sockaddr_in6 *) sa)->sin6_addr);
}

void perform_write(char *args[]);

void perform_compute(char *argv[]);

void perform_search(char *argv[]);

char *send_data_to_aws(char *buf);

void print_usage() {
    fprintf(stderr, "usage:\nclient write <BW> <LENGTH> <VELOCITY> <NOISEPOWER>\n"
                    "client compute <LINK_ID> <SIZE> <SIGNALPOWER>\n"
                    "client search <LINK_ID>\n");
}


int main(int argc, char *argv[]) {

    // read the command line input values and store into 2 D array
    if (argc < 3) {
        print_usage();
        exit(1);
    }
    char *operation = argv[1];
    if (strcmp(operation, "write") == 0) {
        if (argc != 6) {
            print_usage();
            exit(1);
        }
        perform_write(argv);
    } else if (strcmp(operation, "compute") == 0) {
        if (argc != 5) {
            print_usage();
            exit(1);
        }
        perform_compute(argv);
    } else if (strcmp(operation, "search") == 0) {
        if (argc != 3) {
            print_usage();
            exit(1);
        }
        perform_search(argv);
    } else {
        print_usage();
        exit(1);
    }

    return 0;
}

void perform_write(char *args[]) {
    string bw(args[2]);
    string length(args[3]);
    string velocity(args[4]);
    string noisePower(args[5]);

    string request = "write " + bw + " " + length + " " + velocity + " " + noisePower;
    char buf[MAXDATASIZE];
    strcpy(buf, request.c_str());
    char *response = send_data_to_aws(buf);
    printf("The client sent <write> operation to AWS\n");
    if (response != nullptr) {
        printf("%s\n", response);
    }
    else {
        printf("Write operation failed\n");
    }
}

void perform_compute(char *args[]) {
    char *linkId = args[2];
    char *size = args[3];
    char *signalPower = args[4];
    string request = "compute " + string(linkId) + " " + string(size) + " " + string(signalPower);
    char buf[MAXDATASIZE];
    strcpy(buf, request.c_str());
    char *response = send_data_to_aws(buf);
    printf("The client sent ID=<%s>, size=<%s>, and power=<%s> to AWS\n", linkId, size, signalPower);
    if (response != nullptr) {
        char* notFoundMessage = "Link ID not found";
        if(strcmp(response, notFoundMessage) == 0){
            printf("%s\n", notFoundMessage);
        }
        printf("%s\n", response);

        /*else{
            printf("The delay for link <%s> is <%s>ms\n", linkId, response);
        }*/
    }
}

void perform_search(char *argv[]) {
    char *linkId = argv[2];
    string request = "search "+ string(linkId);
    char buf[MAXDATASIZE];
    strcpy(buf,request.c_str());
    char* response = send_data_to_aws(buf);
    if(response!= nullptr){
      printf("The client sent search operation to AWS") ;
    }

}

char *send_data_to_aws(char *buf) {
    int sockfd;

    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if ((rv = getaddrinfo(AWS_ADDRESS, AWS_PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return nullptr;
    }
// loop through all the results and connect to the first we can
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("client: connect");
            continue;
        }
        break;
    }
    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        return nullptr;
    }
    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *) p->ai_addr), s, sizeof s);
    freeaddrinfo(servinfo); // all done with this structure
    printf("The client is up and running\n");
    if (send(sockfd, buf, MAXDATASIZE - 1, 0) == -1) {
        perror("Failed to send data");
        exit(1);
    }
    // recv function should be there

    if (( recv(sockfd, buf, MAXDATASIZE - 1, 0)) == -1)
        perror(" error while receiving from AWS");
    // printf(buf);
    close(sockfd);
    return buf;
}


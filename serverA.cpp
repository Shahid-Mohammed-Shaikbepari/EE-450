
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <iostream>
#include <fstream>

#define MYPORT "21475" // the port users will be connecting to
#define MAXBUFLEN 1000
using namespace std;


// code for socket setup and connection has been taken from Beej
void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in *) sa)->sin_addr);
    }
    return &(((struct sockaddr_in6 *) sa)->sin6_addr);
}

string process_request(string request, int *&LinkId);
string search_Keyword(string database, string tokens) ;

/*int main1(){
    string str ="write 256 2 21 31";
    char* buf ="hi";
    int* hi = new int(1);
    process_request(str,hi )};*/


int main() {
    int *LinkId = new int(1);
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    int numbytes;
    struct sockaddr_storage their_addr;
    fstream file;
    /*file.open("database.txt");
    file << "LinkID\tBandwidht\tLength\tVelocity\tNoise Power\n";
    file.close();*/
    char buf[MAXBUFLEN];
    socklen_t addr_len;
    char s[INET6_ADDRSTRLEN];
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // set to AF_INET to force IPv4
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; // use my IP
    if ((rv = getaddrinfo(NULL, MYPORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }
// loop through all the results and bind to the first we can
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1) {
            perror("serverA: socket");
            continue;
        }
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("serverA: bind");
            continue;
        }
        break;
    }
    if (p == NULL) {
        fprintf(stderr, "serverA: failed to bind socket\n");
        return 2;
    }
    freeaddrinfo(servinfo);
    printf("The Server A is up and running using UDP on port <21475>.\n");
    addr_len = sizeof their_addr;
    while(1){
        if ((numbytes = recvfrom(sockfd, buf, MAXBUFLEN - 1, 0,
                                 (struct sockaddr *) &their_addr, &addr_len)) == -1) {
            perror("recvfrom");
            exit(1);
        }
        string request(buf);
        string res = process_request(request, LinkId);
        //send the response to the aws
        if ((numbytes = sendto(sockfd, res.c_str(), MAXBUFLEN - 1, 0,
                               (struct sockaddr *) &their_addr, addr_len)) == -1) {
            perror("talker: sendto");
            exit(1);
        }
//        close(sockfd);
    }

    return 0;
}

string process_request(string request, int* &LinkId){
    //fstream database;
    int count = 0;
    string tokens[5];
    const char delim[2] = " ";
    char requestArray[MAXBUFLEN];
    strcpy(requestArray, request.c_str());
    char *tok = strtok(requestArray, delim);
    while (tok != NULL) {
        tokens[count++] = string(tok);
        tok = strtok(NULL, " ");
    }

    if (tokens[0] == "write") {
        printf("The Server A received input for writing \n");
        fstream file;
        file.open("database.txt", std::fstream::in | std::fstream::out | std::fstream::app);
        file << *LinkId << " " + tokens[1] + " " + tokens[2] + " " + tokens[3] + " " + tokens[4] + "\n";
        file.close();

        printf("The Server A wrote link <%d> to database\n", *LinkId);
        *LinkId = *LinkId + 1;
        string buf = "done";
        return buf ;
    }
    else if (tokens[0] == "compute") {
        printf("The Server A received input <%s> for computing\n", tokens[1].c_str());
        string link_search;
        string result;
        string buf;
        string search_result = search_Keyword( "database.txt", tokens[1]);
        char* notFoundMessage = "Link ID not found";
        if(strcmp(search_result.c_str(), notFoundMessage) == 0){
            printf("%s\n", notFoundMessage);
            buf= notFoundMessage;
            return buf;
        }
        else {
            result = request + " " + search_result;
            buf = result;
            printf("The Server A finished sending the search result to AWS\n");
            return buf;
        }
    }

}
string search_Keyword(string fileName, string linkId) {
    ifstream file(fileName);
    string line;
    while (getline(file, line)) {
        int count = 0;
        string tokens[5];
        const char delim[2] = " ";
        char requestArray[MAXBUFLEN];
        strcpy(requestArray, line.c_str());
        char *tok = strtok(requestArray, delim);
        while (tok != NULL) {
            tokens[count++] = string(tok);
            tok = strtok(NULL, " ");
        }
        if(tokens[0]==linkId){
           // cout << line << endl;
            file.close();
            string retrn = tokens[1]+" "+tokens[2]+" "+tokens[3]+" "+tokens[4];
            return retrn;
        }
    }
        return "Link ID not found";
    file.close();
    //cout << linkId << " not found" << endl;

    return "";
}



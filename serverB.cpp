
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
#include <string>
#include <math.h>

#define MYPORT "22475"    // the port users will be connecting to

#define MAXBUFLEN 100
using namespace std;

// code for socket setup and connection has been taken from Beej
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}
string process_request(string request);
int main(void)
{
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    int numbytes;
    struct sockaddr_storage their_addr;
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
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1) {
            perror("serverB: socket");
            continue;
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("serverB: bind");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "serverB: failed to bind socket\n");
        return 2;
    }

    freeaddrinfo(servinfo);

    printf("The Server B is up and running using UDP on port 22475.\n");

    addr_len = sizeof their_addr;
    while(1){
        if ((numbytes = recvfrom(sockfd, buf, MAXBUFLEN - 1, 0,
                                 (struct sockaddr *) &their_addr, &addr_len)) == -1) {
            perror("recvfrom");
            exit(1);
        }
        string request(buf);
        string res= process_request(request);
        //send the response to the aws
        if ((numbytes = sendto(sockfd, res.c_str(), MAXBUFLEN - 1, 0,
                               (struct sockaddr *) &their_addr, addr_len)) == -1) {
            perror("talker: sendto");
            exit(1);
        }
//        close(sockfd);
    }
    string request(buf);

    printf(buf);

    printf("The Server B received link information: link <LINK_ID>, file size<SIZE>, and signal power <POWER>\n");
    /*printf("listener: got packet from %s\n",
           inet_ntop(their_addr.ss_family,
                     get_in_addr((struct sockaddr *)&their_addr),
                     s, sizeof s));
    printf("listener: packet is %d bytes long\n", numbytes);
    buf[numbytes] = '\0';
    printf("listener: packet contains \"%s\"\n", buf);*/

    close(sockfd);

    return 0;
}
string process_request(string request){
    //am asuming request= compute linkid size signal_power bw length velocity noise_power
    int count = 0;
    string tokens[8];
    const char delim[2] = " ";
    char requestArray[MAXBUFLEN];
    strcpy(requestArray, request.c_str());
    char *tok = strtok(requestArray, delim);
    while (tok != NULL) {
        tokens[count++] = string(tok);
        tok = strtok(NULL, " ");
    }
    printf("The Server B received link information: link <%s>, file size<%s>, and signal power <%s>\n", tokens[1].c_str(),tokens[2].c_str(),tokens[3].c_str());
    //convert the string to linkid in int and rest in float and store in respective variables
    int linkID = stoi(tokens[1]);
    float size = stof(tokens[2]);
    float s_power_db = stof(tokens[3]);
    float bw = stof(tokens[4])*pow(10,6);
    float length = stof(tokens[5]);
    float velocity = stof(tokens[6]);
    float n_power_db = stof(tokens[7]);
    // s in wattt= ((10)^(p_given/10))/1000;
    // * n in watts = ((10)^(P_given /10))/1000;
    float sp_watt = pow(10,(s_power_db/10))/1000;
    float np_watt= pow(10, (n_power_db/10))/1000;
    // * tp=velocity / distance;
    float tp= length/velocity;
    float tp_in_ms =tp*pow(10, 3);
    // c= bw*log_2(1+s/n)
    float c = bw* (log10(1+(sp_watt/np_watt)))/log10(2);
    float tt =size/c;
    float tt_in_ms =tt*pow(10, 3);//roundup to 2 digits tt_in_ms, tt_in _ms, end_to_end_delay_ms to
    float end_to_end_delay_ms = (tt + tp)* pow(10, 3);
    printf("The Server B finished the calculation for link <%d>\n",linkID);
    string result = tokens[1]+" "+to_string(tt_in_ms)+ " " + to_string(tp_in_ms)+" "+ to_string(end_to_end_delay_ms);
   // string result = "The result for link <"+ tokens[1] +"> :\n Tt = <" + to_string(tt_in_ms) +">ms,\nTp = <"+to_string(tp_in_ms) +" >ms,\nDelay = <"+to_string(end_to_end_delay_ms) +">ms" ;
    printf("The Server B finished sending the output to AWS\n");
    return result;
    /* printf(" The Server B finished the calculation for link <LINK_ID>”");
    *send back to AWS
    * int sendto(int sockfd, const void *msg, int len, unsigned int flags, const struct sockaddr *to, socklen_t tolen);
    * printf("The Server B finished sending the output to AWS");
    *
    */



    /*printf("listener: got packet from %s\n",
           inet_ntop(their_addr.ss_family,
                     get_in_addr((struct sockaddr *)&their_addr),
                     s, sizeof s));
    printf("listener: packet is %d bytes long\n", numbytes);
    buf[numbytes] = '\0';
    printf("listener: packet contains \"%s\"\n", buf);*/
}


#include "internal.h"
#include <_stdio.h>
#include <_stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h> //for inet_ntoa

char msg[500]="GET /ip HTTP/1.0\nHost: ifconfig.me\n\n";
char response[1000];

void get_public_ip()
{
        int sockfd;

        struct sockaddr_in server;
        struct hostent *he;

        he=gethostbyname("ifconfig.me");

        server.sin_port = htons(80);

        server.sin_addr.s_addr = inet_addr(inet_ntoa(*((struct in_addr *)he->h_addr)));

        server.sin_family = AF_INET;

        sockfd = socket(AF_INET,SOCK_STREAM,0);

        if((connect(sockfd, (struct sockaddr*)&server, sizeof(server)) < 0)){
            printf("Cannot connect to server\n");
        }

        int size=0;
        memset(response, 0, sizeof(response));
        send(sockfd, msg, sizeof(msg), 0);
        int size_response= sizeof(response);
        
        while((size=recv(sockfd, response + size, size_response, 0)) > 0){
            size_response -= size;
        }

        char *body = strstr(response, "\r\n\r\n");
        if (!body) {
            body = strstr(response, "\n\n");
        }

        if (body) {
            body += (body[1] == '\r') ? 4 : 2;
            
            while (*body && (*body == '\r' || *body == '\n')) body++;
        } else {
            printf("Failed to extract ip address from body\n");
        }
        
        if(inet_aton(body, &public_ip_address) == 0) {
            fprintf(stderr, "Failed to extract ip address\n");

            close(sockfd);

            exit(EXIT_FAILURE);
        }

        close(sockfd);
}

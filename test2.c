#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

#define _GNU_SOURCE

#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int main(){
    FILE * file = fopen("output.txt", "wb+");
    WSADATA winsock;
    WSAStartup(MAKEWORD(2,2), &winsock);
    struct addrinfo a, *bind_address;
    memset(&a, 0, sizeof(a));
    a.ai_socktype = SOCK_STREAM;
    a.ai_flags = AI_PASSIVE;
    a.ai_family = AF_INET;
    getaddrinfo(0, "80", &a, &bind_address);
    SOCKET listen_socket = socket(bind_address->ai_family, bind_address->ai_socktype, bind_address->ai_protocol);
    bind(listen_socket, bind_address->ai_addr, bind_address->ai_addrlen);
    freeaddrinfo(bind_address);
    listen(listen_socket, 10);
    char buf[80000];
    SOCKET clients[100]; int clientn=0;
    int rec;
    WSAPOLLFD listenpoller = {listen_socket, POLLRDNORM, 0};
    int recieved = 0;
    while(recieved<10){
        if(WSAPoll(&listenpoller, 1, 100), listenpoller.revents == POLLRDNORM){
            clients[clientn++] = accept(listen_socket, NULL, NULL);
            printf("accepted ");
        }
        for(int i = 0; i < clientn; i++){
            WSAPOLLFD clientpoller = {clients[i], POLLRDNORM, 0};
            WSAPoll(&clientpoller, POLLRDNORM, 20);
            if(clientpoller.revents == POLLRDNORM){
                printf("recieved");
                buf[recieved = recv(clients[i], buf, sizeof(buf), 0)] = 0;
                fprintf(file, "%.*s", recieved, buf);
            }
        }
    }
    return 0;
}
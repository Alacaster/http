#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <winsock2.h>
#include <ws2tcpip.h>

int main() {
    WSADATA data;
    WSAStartup(MAKEWORD(2,2), &data);
    struct addrinfo hints, *addr;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_flags = AI_PASSIVE;
    hints.ai_protocol = IPPROTO_TCP;
    printf("%d", getaddrinfo(0, "80", &hints, &addr));
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
}
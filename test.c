#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <winsock2.h>
#include <ws2tcpip.h>

int main() {
    char filebuf[80000];
    FILE * file = fopen("output.txt", "rb");
    long size = fseek(file, 0, SEEK_END), ftell(file);
    fseek(file, 0, SEEK_SET);
    fread(filebuf, 1, size, file);
    
}
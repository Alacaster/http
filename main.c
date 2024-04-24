#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

#define _GNU_SOURCE

#include <winsock2.h>
#include <ws2tcpip.h>
#include <wininet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define EXIT(message) do { fprintf(stderr, message); exit(1); } while(0)

void printwsadata(WSADATA *a){
    union {
        WORD version;
        unsigned char bytes[2];
    } ver = {a->wVersion}, hiver = {a->wHighVersion};
    printf("Initialized Winsock Version %hhu.%hhu\nHighest Available: %hhu.%hhu\nDescription:\t%s\nSystem Status:\t%s", ver.bytes[0], ver.bytes[1], hiver.bytes[0], hiver.bytes[1], a->szDescription, a->szSystemStatus);
}

void printaddressofsocket(SOCKET b){
    SOCKADDR_STORAGE a;
    size_t balls = sizeof(a);
    memset(&a, 0, sizeof(a));
    if(getsockname(b, (struct sockaddr *)&a, (socklen_t *)&balls)) EXIT("\ngetsockname in printaddressofsocekt");
    char hostname[NI_MAXHOST];
    char port[NI_MAXSERV];
    if(getnameinfo((struct sockaddr *)&a, sizeof(a), hostname, sizeof(hostname), port, sizeof(port), NI_NUMERICHOST | NI_NUMERICSERV)) EXIT("\ngetnameinfo in printaddress");
    hostname[NI_MAXHOST-1] = 0;
    port[NI_MAXSERV-1] = 0;
    printf("%s:%s", hostname, port);
}

SOCKET create_socket(const char *host, const char *port){
    struct addrinfo a, *bind_address;
    memset(&a, 0, sizeof(a));
    a.ai_socktype = SOCK_STREAM;
    a.ai_flags = AI_PASSIVE;
    a.ai_family = AF_INET;
    if(getaddrinfo(host, port, &a, &bind_address)) EXIT("\ngetaddrinfo in create_socket");
    SOCKET listen_socket = socket(bind_address->ai_family, bind_address->ai_socktype, bind_address->ai_protocol);
    if(listen_socket == INVALID_SOCKET) EXIT("\nsocket() in create_socket");
    if(bind(listen_socket, bind_address->ai_addr, bind_address->ai_addrlen)) EXIT("\nbind");
    freeaddrinfo(bind_address);
    if(listen(listen_socket, 10));
    return listen_socket;
}

const char * getmimetypefromstringconst(const char * type){
    const char *last_dot = strrchr(type, '.');
    if (last_dot) {
        if (strcmp(last_dot, ".css") == 0) return "text/css";
        if (strcmp(last_dot, ".csv") == 0) return "text/csv";
        if (strcmp(last_dot, ".gif") == 0) return "image/gif";
        if (strcmp(last_dot, ".htm") == 0) return "text/html";
        if (strcmp(last_dot, ".html") == 0) return "text/html";
        if (strcmp(last_dot, ".ico") == 0) return "image/x-icon";
        if (strcmp(last_dot, ".jpeg") == 0) return "image/jpeg";
        if (strcmp(last_dot, ".jpg") == 0) return "image/jpeg";
        if (strcmp(last_dot, ".js") == 0) return "application/javascript";
        if (strcmp(last_dot, ".json") == 0) return "application/json";
        if (strcmp(last_dot, ".png") == 0) return "image/png";
        if (strcmp(last_dot, ".pdf") == 0) return "application/pdf";
        if (strcmp(last_dot, ".svg") == 0) return "image/svg+xml";
        if (strcmp(last_dot, ".txt") == 0) return "text/plain";
    }
    return "application/octet-stream";
}


enum functionhandler {FN_GETHEADER, FN_GETBODYCONTENTLENGTH, FN_GETBODYCHUNKED, FN_GETBODYCONNECTION};
#define MAXCLIENTS 30
#define DEFAULTREQUESTBUFFERSIZE 0b10000000000
struct client_list_t;
struct http_request_t{
    struct http_header_data_t{
        char * host_s;
        char * content_type;
        char * content_length_s;
        int content_length;
    } http_header;
    enum functionhandler handlerindex; //specifies a function to handle the client each time it is addressed
    unsigned long buffersize;
    char *request;
    char *request_end; //points at the last character of the stirng
    char *body_start;
    clock_t timeout;
} *requests;
struct sockaddr_storage *addresses;
struct client_list_t{
    SOCKET sock;
    struct client_list_t *next;
}*client_list, *client_list_start, *client_list_end, *lowest_open_client, *previous_client;
FD_SET monoclient_set, listener_set, listener_set_main;
unsigned int numberofcurrentclients;
int clientindex; //clientindex is set by main() only

//every function must maintain lowest_open_client before they exit
//and not modify any other client except the one addressed to them or lowest_open_client, otherwise the program will break
enum errortype_t {TOO_LONG = 414, NOT_ENOUGH_MEMORY = 507, BAD_REQUEST = 400, NOT_FOUND = 404, NOT_IMPLIMENTED = 501};
void senderrorresponsetoclient(struct client_list_t * clienttoclose, enum errortype_t error){
    FD_SET sendset;
    FD_ZERO(&sendset);
    FD_SET(clienttoclose->sock, &sendset);
    select(0, NULL, &sendset, NULL, NULL);
    if(FD_ISSET(clienttoclose->sock, &sendset)){
        char response[1024];
        const char *status;
        const char *description;
        switch (error) {
            case TOO_LONG:
                status = "414 URI Too Long";
                description = "The requested URI is too long.";
                break;
            case NOT_ENOUGH_MEMORY:
                status = "507 Insufficient Storage";
                description = "The server does not have enough memory to process the request.";
                break;
            case BAD_REQUEST:
                status = "400 Bad Request";
                description = "The request is invalid or cannot be served.";
                break;
            case NOT_FOUND:
                status = "404 Not Found";
                description = "The requested resource could not be found.";
                break;
            case NOT_IMPLIMENTED:
                status = "501 Not Implimented";
                description = "The sever could not process your request.";
                break;
            default:
                status = "500 Internal Server Error";
                description = "An unexpected error occurred on the server.";
                break;
        }
        snprintf(response, sizeof(response),
                    "HTTP/1.1 %s\r\n"
                    "Content-Type: text/plain\r\n"
                    "Content-Length: %zu\r\n"
                    "Connection: close\r\n"
                    "\r\n"
                    "%s",
                    status, strlen(description), description);
        printf("\nClient was cut off due to error: %s", status);
        send(clienttoclose->sock, response, strlen(response), 0);
    }
    deleteclientandsplicelist(clienttoclose);
}

void initializehttprequestbuffer(struct client_list_t *structtoinit){
    requests[clientindex].buffersize = DEFAULTREQUESTBUFFERSIZE;
    requests[clientindex].request_end = requests[clientindex].request = (char *)calloc(requests[clientindex].buffersize, 1);
    requests[clientindex].request_end--;
    if(!requests[clientindex].request) senderrorresponsetoclient(structtoinit, NOT_ENOUGH_MEMORY);
    requests[clientindex].timeout = clock();
}

void doubleclientrequestbuffer(struct client_list_t *clienttodouble){
    if((requests[clientindex].buffersize <<= 1) > 0b100000000000000000000) {
        senderrorresponsetoclient(clienttodouble, TOO_LONG);
        return;
    }
    if(realloc(requests[clientindex].request, requests[clientindex].buffersize))
        senderrorresponsetoclient(clienttodouble, NOT_ENOUGH_MEMORY);
    
}
//expects client_list_start and client_list_end and lowest_open_client to all be equal to client_list[0]
//to account for the case where lowest_open_client is equal to client_list_end because client_list_end and client_list_start are incorrect when we have 0 clients
void waitforatleastoneclient(SOCKET _listensock){
    if(lowest_open_client != client_list) EXIT("\nlowest_open_client != client_list   This should never happen");
    int temp = sizeof(struct sockaddr_storage);
    lowest_open_client->sock = accept(_listensock, (struct sockaddr *)&addresses[clientindex], &temp);
    initializehttprequestbuffer(lowest_open_client);
    lowest_open_client++;
}
//expects _listensock to be ready and non-blocking place a client at the end of the list
void acceptclient(SOCKET _listensock){ //e
    client_list_end->next = lowest_open_client;
    client_list_end = lowest_open_client;
    int temp = sizeof(struct sockaddr_storage);
    lowest_open_client->sock = accept(_listensock, (struct sockaddr *)&addresses[clientindex], &temp);
    initializehttprequestbuffer(lowest_open_client);
    while((lowest_open_client++)->sock); //find empty client, the end must always be empty
    numberofcurrentclients++;
}
void clearclient(struct client_list_t *clienttoclear){ //free() what's necessary and set all to 0, fix lowest open address and lowest open client, do not call clearclient directly
    if(!clienttoclear->sock || addresses[clientindex].ss_family || requests[clientindex].buffersize) EXIT("\nclearclient() found bad formatting, this is gonna be hard to debug");
    if(clienttoclear < lowest_open_client) lowest_open_client = clienttoclear; //min 
    closesocket(clienttoclear->sock);
    free(requests[clientindex].request); //relieve request buffer
    memset(&addresses[clientindex], 0, sizeof(addresses[clientindex]));
    memset(&requests[clientindex], 0, sizeof(requests[clientindex]));
    memset(&clienttoclear, 0, sizeof(clienttoclear));
    numberofcurrentclients--;
}

void replaceoldestclient(SOCKET _listensock){ //only called when client list is full
    //set the start pointer to next client
    //clear and free the old client
    typeof(client_list_start) client_list_start_temp = client_list_start;
    client_list_start = client_list_start->next;
    clearclient(client_list_start_temp);
    acceptclient(_listensock);
}

void deleteclientandsplicelist(struct client_list_t * clienttodelete){
    if(previous_client != clienttodelete){ //i set them equal before the loop runs through the headers
        previous_client->next = clienttodelete->next;
    }
    clearclient(clienttodelete);
}
enum fieldtype_t {CONTENT_LENGTH, HOST, TRANSFER_ENCODING, CONNECTION, USER_AGENT, ACCEPT_LANGUAGE, ACCEPT, ACCEPT_ENCODING, CONTENT_TYPE, CACHE_CONTROL, DATE, ETAG, EXPIRES, LAST_MODIFIED, SERVER, X_CACHE};
#define HEADER_FIELD_FORMAT_ERROR -1
#define MAX_DATE_LENGTH 40
char * extractfieldfromstring(const char* str, char* const strend, void * const outvalue, const enum fieldtype_t fieldtype){
    //returns the value in outvalue if not NULL and returns a pointer to the beginning of the field including the field name, if field is not found returns NULL, if field is incorrectly formatted returns -1
    char * field;
    switch (fieldtype) {
        case CONTENT_LENGTH: field = "content-length:"; break;
        case HOST: field = "host:"; break;
        case TRANSFER_ENCODING: field = "transfer-encoding:"; break;
        case CONNECTION: field = "connection:"; break;
        case USER_AGENT: field = "user-agent:"; break;
        case ACCEPT_LANGUAGE: field = "accept-language:"; break;
        case ACCEPT: field = "accept:"; break;
        case ACCEPT_ENCODING: field = "accept-encoding:"; break;
        case CONTENT_TYPE: field = "content-type:"; break;
        case CACHE_CONTROL: field = "cache-control:"; break;
        case DATE: field = "date:"; break;
        case ETAG: field = "etag:"; break;
        case EXPIRES: field = "expires:"; break;
        case LAST_MODIFIED: field = "last-modified:"; break;
        case SERVER: field = "server:"; break;
        case X_CACHE: field = "x-cache:"; break;
        default:
            EXIT("\nUnsupported field in extractfieldfromstring()");
    }
    char *returnstr;
    const int fieldlen = strlen(field);
    {
        char * tempstrend = strend - (fieldlen+1);
        while(str < tempstrend){
            int a;
            for(a = 0; a < fieldlen; a++){
                if(tolower(str[a]) != field[a]) {
                    str++;
                    goto continuewhile;
                }
            }
            returnstr = str;
            str += a;
            break;
            continuewhile:
        }
        if(str == tempstrend){
            return NULL;
        }
    }
    char *fieldend = strstr(str, "\r");
    if(!fieldend) return HEADER_FIELD_FORMAT_ERROR;
    enum returntype_t {LONG_INT, DOUBLE, TIME, STRING} returntype = STRING;
    switch (fieldtype) {
        case CONTENT_LENGTH: returntype = LONG_INT; break;
        case HOST: break;
        case TRANSFER_ENCODING: break;
        case CONNECTION: break;
        case USER_AGENT: break;
        case ACCEPT_LANGUAGE: break;
        case ACCEPT: break;
        case ACCEPT_ENCODING: break;
        case CONTENT_TYPE: break;
        case CACHE_CONTROL: break;
        case DATE: returntype = TIME; break;
        case ETAG: break;
        case EXPIRES: break;
        case LAST_MODIFIED: returntype = TIME; break;
        case SERVER: break;
        case X_CACHE: break;
        default:
            EXIT("\nUnsupported field in extractfieldfromstring()");
    }
    switch (returntype) {
        case LONG_INT: 
            while(!isdigit(str) && str < fieldend) str++;
            if(fieldend == str) return HEADER_FIELD_FORMAT_ERROR;
            if(!outvalue) *((long int*)outvalue) = strtol(str, NULL, 10);
            break;
        case DOUBLE:
            while(!isdigit(str) && str < fieldend) str++;
            if(fieldend == str) return HEADER_FIELD_FORMAT_ERROR;
            if(!outvalue) *((double*)outvalue) = strtod(str, NULL);
            break;
        case TIME: {
                while(!isprint(str) && str < fieldend) str++;
                if(fieldend == str) return HEADER_FIELD_FORMAT_ERROR;
                if(!fieldend || fieldend-str > MAX_DATE_LENGTH) return HEADER_FIELD_FORMAT_ERROR;
                char tempdate[fieldend - str];
                strncpy(tempdate, str, sizeof(tempdate));
                tempdate[sizeof(tempdate)-1] = '\0';
                if(!outvalue && !InternetTimeToSystemTime(tempdate,(SYSTEMTIME*)outvalue, 0))
                    return HEADER_FIELD_FORMAT_ERROR;
                break;
            }
        case STRING:
            while(!isprint(str) && str < fieldend) str++;
            *((char *)outvalue) = str;
            break;
        default: break;
    }
    return returnstr;
}

void processheader(struct client_list_t* client){
    int length = 0;
    char* fieldstart = extractfieldfromstring(requests[clientindex].request, requests[clientindex].body_start, &length, CONTENT_LENGTH);
    if(fieldstart){
        if(fieldstart == HEADER_FIELD_FORMAT_ERROR){
            senderrorresponsetoclient(client, BAD_REQUEST); 
            return;
        }else{
            requests[clientindex].http_header.content_length = length;
            requests[clientindex].handlerindex = FN_GETBODYCONTENTLENGTH;
            return;
        }
    }
    char * transferencoding;
    fieldstart = extractfieldfromstring(requests[clientindex].request, requests[clientindex].body_start, transferencoding, TRANSFER_ENCODING);
    if(fieldstart){
        if(fieldstart == HEADER_FIELD_FORMAT_ERROR){
            senderrorresponsetoclient(client, BAD_REQUEST);
            return;
        }else{
            if(requests[clientindex].request_end<transferencoding+strlen("chunked")-1 || strncmp("chunked", transferencoding, strlen("chunked"))){
                senderrorresponsetoclient(client, NOT_IMPLIMENTED);
                return;
            }
            requests[clientindex].handlerindex = FN_GETBODYCHUNKED;
            return;
        }
    }
    requests[clientindex].handlerindex = FN_GETBODYCONNECTION;
}

void getheader(struct client_list_t* client, int newbytes){//find when the header is complete
    if(!newbytes){
        senderrorresponsetoclient(client, BAD_REQUEST);
        return;
    }
    char * startscan = requests[clientindex].request_end - newbytes - 3;//go back to not split "\r\n\r\n" in half
    if(startscan-requests[clientindex].request < 0) startscan = requests[clientindex].request; //don't overflow
    char * bodystart = strstr(startscan, "\r\n\r\n");
    if(!bodystart) return;
    requests[clientindex].body_start = bodystart+4;
    processheader(client);
}

void getbodycontentlength(struct client_list_t* client, int newbytes){
    if(requests[clientindex].request_end - (requests[clientindex].body_start-1) < requests[clientindex].http_header.content_length){
        return;
    }
    reply(client);
}
void getbodychunked(struct client_list_t* client, int newbytes){
    sscanf(" %lx\r\n");
}
void getbodyconnection(struct client_list_t* client, int newbytes){}
void reply(struct client_list_t* client){}

int main(){
    //if a function is called with a socket, you must set clientindex to lowest open client
    //if a function is called with a client struct pointer you must set clientindex to that client
    WSADATA startup;
    if(WSAStartup(MAKEWORD(2, 2), &startup)) EXIT("WSAStartup()\n");
    printwsadata(&startup);
    SOCKET listensock = create_socket(0, "80");
    printf("\nListening on: ");
    printaddressofsocket(listensock);
    client_list = (struct client_list_t *)calloc(MAXCLIENTS+1, sizeof(struct client_list_t)); //set socket to 0 to clear, last space is always 0
    addresses = (struct sockaddr_storage *)calloc(MAXCLIENTS, sizeof(struct sockaddr_storage));
    requests = (struct http_request_t *)calloc(MAXCLIENTS, sizeof(struct http_request_t *));
    FD_ZERO(&listener_set_main);
    FD_SET(listensock, &listener_set_main);
    client_list_start = client_list, client_list_end = client_list;
    lowest_open_client = client_list;// when remove a client check to see if it's lower, when add a client, move up to next empty space, == maxclients means full
    numberofcurrentclients = 0;
    void (*handlers[])(struct client_list_t*, int) = {&getheader, &getbodycontentlength, &getbodychunked, &getbodyconnection, &reply};
    while(1){
        //check listening socket with select by copying its fdset
        //if new client available add it to the front the list and store it in lowest_open_client
        //if list is full, replace the first item on the list, set client_list_start to the next one, set the client_list_end client to point to new client and set client_list_end to point to new client
        //when removing a client due to lost connection or sent request, check if lower than lowest_open_client
        //when adding client, increment lowest_open_client to next open spot or until >= MAXCLIENTS
        if(!numberofcurrentclients){waitforatleastoneclient(listensock);}else{
            listener_set = listener_set_main; struct timeval timeout = {0, 70000};
            select(0, &listener_set, 0, 0, &timeout);
            if(FD_ISSET(listensock, &listener_set)){
                if(numberofcurrentclients > MAXCLIENTS){EXIT("\nnumberofcurrentclients out of bounds, how did that happen???"); //debug statements basically
                if(lowest_open_client-client_list == MAXCLIENTS-1 && numberofcurrentclients != MAXCLIENTS) EXIT("\nnumberofcurrentclients and lowest_opem_client disagree! UR screwed!");
                }else if(numberofcurrentclients == MAXCLIENTS){
                    clientindex = client_list_start-client_list;
                    replaceoldestclient(listensock);
                }else if(numberofcurrentclients > 0){
                    clientindex = lowest_open_client-client_list;
                    acceptclient(listensock);
                }
            }
        }
        struct client_list_t * currentclient;
        previous_client = currentclient = client_list_start; //if currentclient == previous_client we know not to link back or else we create an infinite loop
        do{ //must maintain previous_client
            clientindex = currentclient-client_list;
            if((clock() - requests[clientindex].timeout) / CLOCKS_PER_SEC > 5)
                deleteclientandsplicelist(currentclient);
            FD_ZERO(&monoclient_set);
            FD_SET(currentclient->sock, &monoclient_set);
            struct timeval timeout = {0, 0};
            select(0, &monoclient_set, 0, 0, &timeout);
            if(FD_ISSET(currentclient->sock, &monoclient_set)){
                int bufferremaining = requests[clientindex].buffersize - (requests[clientindex].request_end - requests[clientindex].request) - 1;
                if(bufferremaining == 0) doubleclientrequestbuffer(currentclient);
                int newbytes = recv(currentclient->sock, requests[clientindex].request, bufferremaining, 0);
                if(newbytes == SOCKET_ERROR) EXIT("recv() in main failed for some reason.");
                requests[clientindex].request_end += newbytes;
                (*(handlers+requests[clientindex].handlerindex))(currentclient, newbytes);
            }
            previous_client = currentclient;
        }while(currentclient = currentclient->next);//this is fine cause if we have 0 clients we just wait for accept();
    }
}
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

#define _GNU_SOURCE

#include <winsock2.h>
#include <ws2tcpip.h>
#include <wininet.h>
#include <shlwapi.h>
#include <sysinfoapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <setjmp.h>

#define EXIT(message) do { fprintf(stderr, message); exit(1); } while(0)

void printwsadata(WSADATA *a){
    union {
        WORD version;
        unsigned char bytes[2];
    } ver = {a->wVersion}, hiver = {a->wHighVersion};
    printf("Initialized Winsock Version %hhu.%hhu\nHighest Available: %hhu.%hhu\nDescription:\t%s\nSystem Status:\t%s", ver.bytes[0], ver.bytes[1], hiver.bytes[0], hiver.bytes[1], a->szDescription, a->szSystemStatus);
}

void print_escaped_string(const char *str, size_t length) {
    for (size_t i = 0; i < length; i++) {
        char c = str[i];
        if (isprint(c)) {
            putchar(c);
        } else {
            switch (c) {
                case '\n':
                    printf("\\n");
                    break;
                case '\r':
                    printf("\\r");
                    break;
                case '\t':
                    printf("\\t");
                    break;
                case '\0':
                    printf("\\0");
                    break;
                default:
                    printf("\\x%02X", c);
                    break;
            }
        }
    }
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
#define MAX_DATE_LENGTH INTERNET_RFC1123_BUFSIZE
struct http_header_data_t{
        char host[1024];
        char path[MAX_PATH];
        char useragent[4096];
        char contenttype[256];
        SYSTEMTIME time;
        char connection : 1; //1 is keepalive 0 is close
        char requesttype : 2; //enum {ERROR, GET, POST, HEAD}
};
struct http_request_t{
    struct http_header_data_t * http_header;
    int content_length;
    enum functionhandler handlerindex; //specifies a function to handle the client each time it is addressed
    unsigned long buffersize;
    char *request;
    char *request_end; //points at the last character of the stirng
    char *body_start;
    uint64_t timeout;
} *requests;
struct sockaddr_storage *addresses;
struct client_list_t;
struct client_list_t{
    SOCKET sock;
    struct client_list_t *next;
}*client_list, *client_list_start, *client_list_end, *lowest_open_client, *previous_client, *currentclient;
unsigned int numberofcurrentclients;
int clientindex; //clientindex is set by main() only
jmp_buf buf;
void deleteclientandsplicelist(struct client_list_t * clienttodelete);
void reply(struct client_list_t* client);

void printclientinformation(struct client_list_t * client){
    int cli = client-client_list;
    printf("\nClient %lld -> %lld sock=%d ", client-client_list, ((client->next-client_list) >= 0) ? (client->next-client_list) : -1, client->sock);printaddressofsocket(client->sock);
    printf( "\ncontent_length: \t%d"
            "\nhandler function index:\t%d"
            "\nbuffersize:\t\t%ld"
    ,requests[cli].content_length,
    requests[cli].handlerindex,
    requests[cli].buffersize);
    printf("\nrequest:");
    print_escaped_string(requests[cli].request, requests[cli].request_end-requests[cli].request+1);
    printf("\nbody_start: %s", requests[cli].body_start ? "FOUND" : "NULL");
    if(requests[cli].http_header){
        char datestring[MAX_DATE_LENGTH];
        InternetTimeFromSystemTimeA(&requests[cli].http_header->time, INTERNET_RFC1123_FORMAT, datestring, sizeof(datestring));
        printf( "\nhost: %s"
                "\npath: %s"
                "\nuser-agent: %s"
                "\ncontent-type: %s"
                "\nconnection: %s"
                "\nrequesttype: %hhd"
                "\ntime: %s",
                requests[cli].http_header->host,
                requests[cli].http_header->path,
                requests[cli].http_header->useragent,
                requests[cli].http_header->contenttype,
                requests[cli].http_header->connection ? "close" : "keep",
                requests[cli].http_header->requesttype,
                datestring);
    }
    putchar('\n');
}

//every function must maintain lowest_open_client before they exit
//and not modify any other client except the one addressed to them or lowest_open_client, otherwise the program will break
enum errortype_t {TOO_LONG = 414, NOT_ENOUGH_MEMORY = 507, BAD_REQUEST = 400, NOT_FOUND = 404, NOT_IMPLIMENTED = 501, REQUEST_TIMEOUT = 408};
void senderrorresponsetoclient(struct client_list_t * clienttoclose, enum errortype_t error){
    printf("\nSending error to client:");
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
            case REQUEST_TIMEOUT:
                status = "408 Request Timeout";
                description = "Request not fully recieved before timeout.";
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
        printf("\n%s\n", response);
        send(clienttoclose->sock, response, strlen(response), 0);
    }
    deleteclientandsplicelist(clienttoclose);
}

void initializehttprequestbuffer(struct client_list_t *structtoinit){
    requests[clientindex].buffersize = DEFAULTREQUESTBUFFERSIZE;
    requests[clientindex].request_end = requests[clientindex].request = (char *)calloc(requests[clientindex].buffersize, 1);
    if(!requests[clientindex].request) senderrorresponsetoclient(structtoinit, NOT_ENOUGH_MEMORY);
    requests[clientindex].request_end--;
    requests[clientindex].timeout = GetTickCount64();
    printclientinformation(structtoinit);
    /*printf( "\nInitialized slot %lld:\n"
            "buffersize =\t%ld\n"
            "request =\t%p\n"
            "request_end =\t%p\n"
            "timeout =\t%llu\n\n",
       (ptrdiff_t)(structtoinit-client_list),
       requests[clientindex].buffersize,
       requests[clientindex].request,
       requests[clientindex].request_end,
       requests[clientindex].timeout);*/
}
void resetclienthttprequestbuffer(struct client_list_t *structtoreset){
    requests[clientindex].buffersize = DEFAULTREQUESTBUFFERSIZE;
    free(requests[clientindex].request);
    requests[clientindex].request_end = requests[clientindex].request = (char *)calloc(requests[clientindex].buffersize, 1);
    requests[clientindex].request_end--;
    requests[clientindex].handlerindex = 0;
    requests[clientindex].body_start = 0;
    requests[clientindex].timeout = GetTickCount64() + 300000;
    if(requests[clientindex].http_header){
        memset(requests[clientindex].http_header, 0, sizeof(struct http_header_data_t));
        requests[clientindex].http_header = NULL;
    }
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
    printf("\n\n~~~~~~~Waiting for any connection...");
    if(lowest_open_client != client_list) EXIT("\nlowest_open_client != client_list   This should never happen");
    int temp = sizeof(struct sockaddr_storage);
    lowest_open_client->sock = accept(_listensock, (struct sockaddr *)&addresses[clientindex], &temp);
    printf(" connection found on socket:%d ", client_list->sock);printaddressofsocket(client_list->sock);
    initializehttprequestbuffer(lowest_open_client);
    lowest_open_client++;
    printf("\nlowest_open_client %d -> %d", (lowest_open_client-client_list)-1, lowest_open_client-client_list);
    printf("\nnumberofcurrentclients: %d -> %d", numberofcurrentclients, numberofcurrentclients+1);
    numberofcurrentclients++;
    client_list_start = client_list;
    client_list_end = client_list;
}
//expects _listensock to be ready and non-blocking place a client at the end of the list
void acceptclient(SOCKET _listensock){
    printf("\n\n~~~~~~~~Accepting new client. Loading into slot %lld\nSlot %lld now points to %lld", (ptrdiff_t)(lowest_open_client-client_list),(ptrdiff_t)(client_list_end-client_list),(ptrdiff_t)(lowest_open_client-client_list));
    client_list_end->next = lowest_open_client;
    client_list_end = lowest_open_client;
    int temp = sizeof(struct sockaddr_storage);
    lowest_open_client->sock = accept(_listensock, (struct sockaddr *)&addresses[clientindex], &temp);
    initializehttprequestbuffer(lowest_open_client);
    printf("\nlowest_open_client %d -> ", lowest_open_client-client_list);
    while((lowest_open_client)->sock) lowest_open_client++; //find empty client, the end must always be empty
    printf("%d", lowest_open_client-client_list);
    printf("\nnumberofcurrentclients: %d -> %d", numberofcurrentclients, numberofcurrentclients+1);
    numberofcurrentclients++;
}

void clearclient(struct client_list_t *clienttoclear){ //free() what's necessary and set all to 0, fix lowest open address and lowest open client, do not call clearclient directly
    printf("\n\n~~~Clearing cient\n"); printclientinformation(clienttoclear);
    if(!clienttoclear->sock || !addresses[clientindex].ss_family || !requests[clientindex].buffersize) {
        printf("\nclienttoclear index:%lld", (ptrdiff_t)(clienttoclear-client_list));
        EXIT("\nclearclient() found bad formatting, this is gonna be hard to debug");
    }
    if(clienttoclear < lowest_open_client){
        printf("\nlowest_open_client %d ->", lowest_open_client-client_list);
        lowest_open_client = clienttoclear;
        printf("%d", lowest_open_client-client_list);
    }//min 
    closesocket(clienttoclear->sock);
    free(requests[clientindex].request); //relieve request buffer
    memset(&addresses[clientindex], 0, sizeof(addresses[clientindex]));
    memset(&requests[clientindex], 0, sizeof(requests[clientindex]));
    memset(clienttoclear, 0, sizeof(struct client_list_t));
    if(requests[clientindex].http_header){
        memset(requests[clientindex].http_header, 0, sizeof(struct http_header_data_t));
    }
    printf("numberofcurrentclients: %d -> %d", numberofcurrentclients, numberofcurrentclients-1);
    numberofcurrentclients--;
    if(numberofcurrentclients < 0){EXIT("numberofcurrentclients < 0? in clearclient");}
}

void replaceoldestclient(SOCKET _listensock){ //only called when client list is full
    //set the start pointer to next client
    //clear and free the old client
    struct client_list_t * client_list_start_temp = client_list_start;
    client_list_start = client_list_start->next;
    clearclient(client_list_start_temp);
    acceptclient(_listensock);
}

void deleteclientandsplicelist(struct client_list_t * clienttodelete){
    previous_client->next = clienttodelete->next;
    if(clienttodelete == client_list_end){
        client_list_end = previous_client;
    }else if(clienttodelete == client_list_start){
        client_list_start = clienttodelete->next;
    }
    clearclient(clienttodelete);
    longjmp(buf, 1);
}

enum fieldtype_t {CONTENT_LENGTH, HOST, TRANSFER_ENCODING, CONNECTION, USER_AGENT, ACCEPT_LANGUAGE, ACCEPT, ACCEPT_ENCODING, CONTENT_TYPE, CACHE_CONTROL, DATE_FIELD, ETAG, EXPIRES, LAST_MODIFIED, SERVER, X_CACHE};
#define HEADER_FIELD_FORMAT_ERROR (const char *)-1
const char * extractfieldfromstring(const char str[], char* const strend, void * const outvalue, const enum fieldtype_t fieldtype){
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
        case DATE_FIELD: field = "date:"; break;
        case ETAG: field = "etag:"; break;
        case EXPIRES: field = "expires:"; break;
        case LAST_MODIFIED: field = "last-modified:"; break;
        case SERVER: field = "server:"; break;
        case X_CACHE: field = "x-cache:"; break;
        default:
            EXIT("\nUnsupported field in extractfieldfromstring()");
    }
    const char * returnstr;
    const int fieldlen = strlen(field);
    {
        char * tempstrend = strend - (fieldlen+1);
        while(str < tempstrend){
            int a;
            for(a = 0; a < fieldlen; a++){
                if(tolower(str[a]) != field[a]) {
                    str+= a > 0 ? a : 1;
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
        case DATE_FIELD: returntype = TIME; break;
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
            while(!isdigit(*str) && str < fieldend) str++;
            if(fieldend == str) return HEADER_FIELD_FORMAT_ERROR;
            if(!outvalue) *((long int*)outvalue) = strtol(str, NULL, 10);
            break;
        case DOUBLE:
            while(!isdigit(*str) && str < fieldend) str++;
            if(fieldend == str) return HEADER_FIELD_FORMAT_ERROR;
            if(!outvalue) *((double*)outvalue) = strtod(str, NULL);
            break;
        case TIME: {
                while(!isprint(*str) && str < fieldend) str++;
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
            while(!isprint(*str) && str < fieldend) str++;
            *((const char **)outvalue) = str;
            break;
        default: break;
    }
    return returnstr;
}

void getbodylengthtypefromheader(struct client_list_t* client){
    int length = 0;
    const char * fieldstart = extractfieldfromstring(requests[clientindex].request, requests[clientindex].body_start, &length, CONTENT_LENGTH);
    if(fieldstart){
        if(fieldstart == HEADER_FIELD_FORMAT_ERROR){
            senderrorresponsetoclient(client, BAD_REQUEST); 
            return;
        }else{
            requests[clientindex].content_length = length;
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
    getbodylengthtypefromheader(client);
}

void getbodycontentlength(struct client_list_t* client, int newbytes){
    if(!newbytes){
        senderrorresponsetoclient(client, BAD_REQUEST);
        return;
    }
    if(requests[clientindex].request_end - (requests[clientindex].body_start-1) < requests[clientindex].content_length){
        return;
    }
    reply(client);
}
void getbodychunked(struct client_list_t* client, int newbytes){
    if(!newbytes){
        senderrorresponsetoclient(client, BAD_REQUEST);
        return;
    }
    static int bodyindex[MAXCLIENTS] = {0}; //points to the place where the next chunked length should be as added to body_start
    int bytelength;
    while(requests[clientindex].body_start + bodyindex[clientindex] <= requests[clientindex].request_end){
        int chunklengthlinelength; //perhaps there is no newline after a 0 chunk?
        char * checkfornewline = strstr(requests[clientindex].body_start + bodyindex[clientindex], "\r\n");
        if(!checkfornewline) return;
        chunklengthlinelength = (requests[clientindex].body_start+bodyindex[clientindex]) - checkfornewline;
        chunklengthlinelength+=2;
        if(!sscanf(requests[clientindex].body_start + bodyindex[clientindex], " %x", &bytelength)){senderrorresponsetoclient(client, BAD_REQUEST); return;}
        if(!bytelength){
            requests[clientindex].request_end = requests[clientindex].body_start + (bodyindex[clientindex] - 1);
            bodyindex[clientindex] = 0;
            reply(client);
        }
        bytelength+=2;
        for(int i = bodyindex[clientindex]; (i + chunklengthlinelength) + requests[clientindex].body_start <= requests[clientindex].request_end; i++){
            requests[clientindex].body_start[i] = requests[clientindex].body_start[i+chunklengthlinelength];
        }
        requests[clientindex].request_end -= chunklengthlinelength;
        bodyindex[clientindex] += bytelength;
    }
}
void getbodyconnection(struct client_list_t* client, int newbytes){
    if(newbytes) return;
    if(connect(client->sock, (struct sockaddr*)&addresses[clientindex], sizeof(SOCKADDR_STORAGE))) return;
    reply(client);
}

int loadheaderdata(struct client_list_t * client){
    // Function returns a pointer to a static struct http_header_data_t.
    // The function first checks for the request method (POST, GET, HEAD) on the first line.
    // If a '\r' character is present, it null-terminates the first line and then resets the character after finishing parsing.
    // If no '\r' is found, it continues without modification. The allocated buffer is always null-terminated
    // because it is calloc'd and the buffer size is kept 1 less than the allocated size.
    // The function loads the path string that comes after the '/' character.
    // It does not consider anything for the http version
    // It has a bug where if the strings (POST, GET, HEAD) are in the path or somewhere they shouldn't be and NOT before there
    // it will interpret the request that way which could be bad.
    // It then iterates through the string from the beginning to the end minus the "\r\n\r\n" sequence,
    // which would have been found by getheader() (the only reason this function is called).
    // For each character, if it is ':', the function checks if the preceding characters match any of the fields
    // defined in the fieldstwings array, which contains pointers to each field and their lengths.
    // The comparison is done by scanning backwards up to the length of the field string minus 1,
    // while converting the characters from the buffer to lowercase using tolower().
    // or up to the last valid character which is kept track of, but cannot be before the beginning of the string
    // this handles overflow.
    // If a match is found, a switch statement is called based on the index of the matched field in the fieldstwings array.
    // The matched field pointer is set to 0 to mark it as processed, and the corresponding field is handled accordingly.
    // The function uses sscanf_s to extract and convert (if necessary) the argument after the ':' character.
    // sscanf_s disregards leading whitespace and stops at trailing whitespace due to the format specifiers,
    // except when parsing the date field.
    // When parsing the connection field, the string after "connection:" is copied and converted to lowercase using strlwr().
    // The function then checks if "close" or "keep-alive" is present in the string. It will match even if the keyword
    // is surrounded by other characters (e.g., "iwe7rbweclosecibueipuw" or "close 2938nefnei"), which is intended.
    // However, if the keyword appears after the first whitespace (e.g., "connection: wubrw9r8 close"), it will not match.

    //variable out needs to 0'd when a client that is pointing to it is no longer pointing to it
    //the only time this happens is when a client is cleared or reset so it's handled in those two functions that do that
    //if an error response is sent to a client it also clears the client
    //the functions that clear or reset a client only clear struct "out" when the client they are clearing point to something
    //they memset at that location
    static struct http_header_data_t out;
    requests[clientindex].http_header = &out;
    memset(&out, 0, sizeof(out));
    char * lastalnum = 0;
    char * mainstringpos = requests[clientindex].request;
    char * tempend = strchr(mainstringpos, '\r');
    if(tempend) *tempend = '\0';
    if(strstr(mainstringpos, "GET")){out.requesttype = 1;
    }else if(strstr(mainstringpos, "POST")){out.requesttype = 2;
    }else if(strstr(mainstringpos, "HEAD")){out.requesttype = 3;}else{senderrorresponsetoclient(client, BAD_REQUEST); return 1;}
    char *pathtemp = strchr(mainstringpos, '/');
    if(!pathtemp){return 1;}
    if(sscanf_s(pathtemp, "%s", out.path, sizeof(out.path))){senderrorresponsetoclient(client, TOO_LONG); return 1;}
    if(tempend) *tempend = '\0';
        //path and request type complete
    mainstringpos = requests[clientindex].request;
    struct string_t {const char * string; int len;} fieldstwings[] = {
        {"host:", 6},
        {"user-agent:", 12}, 
        {"content-type:", 14},
        {"connection:", 12},
        {"date:", 6}};
    char * lastvalidchar = 0;
    while(mainstringpos < requests[clientindex].body_start - 4){
        if(!lastvalidchar && (isalnum(*mainstringpos) || *mainstringpos == '-' || *mainstringpos == ':')){
            lastvalidchar = mainstringpos;
        }else if(lastvalidchar && !(isalnum(*mainstringpos) || *mainstringpos == '-' || *mainstringpos == ':')){
            lastvalidchar = 0;
        }
        if(*mainstringpos == ':'){
            for(int x = 0; x < sizeof(fieldstwings)/sizeof(struct string_t); x++){
                if(fieldstwings[x].string){
                for(int i = 0; mainstringpos-i > lastvalidchar && i < fieldstwings[x].len; i++){
                    if(tolower(*(mainstringpos-i)) != *(fieldstwings[x].string-i)){
                        goto nomatch;
                    }
                }
                struct string_t type;
                switch(x){
                    case 0:
                        type.string = out.host; type.len = sizeof(out.host);
                        goto getstring;
                    case 1:
                        type.string = out.useragent; type.len = sizeof(out.useragent);
                        goto getstring;
                    case 2:
                        type.string = out.contenttype; type.len = sizeof(out.contenttype);
                        getstring:
                        fieldstwings[x].string = 0;
                        if(sscanf_s(mainstringpos+1, "%s", type.string, type.len) == EOF){senderrorresponsetoclient(client, TOO_LONG); return 1;}
                        break;
                    case 3:
                        char connection[100];
                        memset(connection, 0, sizeof(connection));
                        if(sscanf_s(mainstringpos+1, "%s", connection, sizeof(connection))){senderrorresponsetoclient(client, BAD_REQUEST); return 1;}
                        strlwr(connection);
                        if(strstr(connection, "close")){out.connection = 0;}else
                        if(strstr(connection, "keep-alive")){out.connection = 1;}else{
                            senderrorresponsetoclient(client, BAD_REQUEST);
                        }
                        fieldstwings[x].string = 0;
                        break;
                    case 4:
                        char date[MAX_DATE_LENGTH];
                        memset(date, 0, sizeof(date));
                        if(sscanf_s(mainstringpos+1, "%40[^\t\n\v\f\r]", date, sizeof(date))){senderrorresponsetoclient(client, BAD_REQUEST); return 1;}
                        if(!InternetTimeToSystemTimeA(date, &out.time, 0)){senderrorresponsetoclient(client, 0); return 1;}
                        fieldstwings[x].string = 0;
                        break;
                }
                break;
                }
                nomatch:
            }
        }
    }
}

static size_t _bufremain = 0;
char * _buffer = 0;
void snprintfappendinit(char * buffer, size_t bufsize){
    _bufremain = bufsize;
    _buffer = buffer;
    return;
}
int snprintfappend(char * format, ...){
    va_list var;
    va_start(var, format);
    int written = vsnprintf(_buffer, _bufremain, format, var);
    va_end(var);
    if(written <= 0){return written;}
    _bufremain-=written;
    _buffer+=written;
    return written;
}
size_t snprintfgetbufremain(){return _bufremain;}
int snprintftrackexternalwrite(int numwrote){
    _bufremain -= numwrote;
    return _bufremain;
}

void reply(struct client_list_t* client){
    if(loadheaderdata(client)){senderrorresponsetoclient(client, BAD_REQUEST); return;}
    struct http_header_data_t * quick = requests[clientindex].http_header;
    if(!quick->host[0]){senderrorresponsetoclient(client, BAD_REQUEST); return;}
    if(!(quick->requesttype == 1 || quick->requesttype == 3)){senderrorresponsetoclient(client, NOT_IMPLIMENTED); return;}
    FILE * manifest = fopen("manifest.txt", "r");
    char path[MAX_PATH];
    while(EOF != fscanf_s(manifest, "%[^\r\n] ", path, sizeof(path))){
        if(StrStrIA(quick->path, path)) goto found;
    }
    senderrorresponsetoclient(client, NOT_FOUND);
    fclose(manifest);
    return;
    found:
    fclose(manifest);
    memcpy(path, "path.txt", 9); //DEBUG TEST MUST DELETE
    FILE * file = fopen(path, "rb");
    if(!file) senderrorresponsetoclient(client, NOT_FOUND);
    #define MAX_REPLY_SIZE 100000 //MUST BE LARGER THAN 1Kb
    char response[MAX_REPLY_SIZE];
    fseek(file, 0, SEEK_END);
    size_t contentlength = ftell(file) + 1;
    rewind(file);

    snprintfappendinit(response, sizeof(response));
    snprintfappend("HTTP/1.1 200 OK\r\n");
    snprintfappend("Content-Type: %s\r\n", getmimetypefromstringconst(path));
    snprintfappend("Content-Length: %llu\r\n", contentlength);
    if(quick->connection){
        snprintfappend("Connection: keep-alive\r\n");
    }else{
        snprintfappend("Connection: close\r\n");
    }
    SYSTEMTIME timevar;
    GetLocalTime(&timevar);
    char datestr[MAX_DATE_LENGTH];
    if(InternetTimeFromSystemTimeA(&timevar, INTERNET_RFC1123_FORMAT, datestr, sizeof(datestr)))
        snprintfappend("Date: %s\r\n", datestr);
    snprintfappend("\r\n");
    if(quick->requesttype == 1){
        if(snprintfgetbufremain() < contentlength){senderrorresponsetoclient(client, 0); return;}else{
            if(contentlength!=fread(&response[MAX_REPLY_SIZE-snprintfgetbufremain()], 1, contentlength, file)) printf("\n\nwhatwhatwhat\n\n");
            snprintftrackexternalwrite(contentlength);
        }
    }
    send(client->sock, response, MAX_REPLY_SIZE-snprintfgetbufremain(), 0);
    fclose(file);
    if(!quick->connection){
        deleteclientandsplicelist(client);
    }else{
        resetclienthttprequestbuffer(client);
    }
}

int main(){
    //if a function is called with a socket, you must set clientindex to lowest open client
    //if a function is called with a client struct pointer you must set clientindex to that client
    //if you think i'm stupid for using goto go fuck yourself
    WSADATA startup;
    FD_SET monoclient_set, listener_set, listener_set_main;
    struct client_list_t * currentclient;
    if(WSAStartup(MAKEWORD(2, 2), &startup)) EXIT("WSAStartup()\n");
    printwsadata(&startup);
    SOCKET listensock = create_socket(0, "80");
    printf("\nListening on: ");
    printaddressofsocket(listensock);
    client_list = (struct client_list_t *)calloc(MAXCLIENTS+1, sizeof(struct client_list_t)); //set socket to 0 to clear, last is always 0 so that when we search for open spot we stop there
    addresses = (struct sockaddr_storage *)calloc(MAXCLIENTS, sizeof(struct sockaddr_storage));
    requests = (struct http_request_t *)calloc(MAXCLIENTS, sizeof(struct http_request_t *));
    FD_ZERO(&listener_set_main);
    FD_SET(listensock, &listener_set_main);
    lowest_open_client = client_list_start = client_list_end = client_list;
    // when remove a client check to see if it's lower than lowest_open_client, if it is client_list_start make sure to maintain client_list_start by moving it to the next one
    // 
    // when add a client, move up to next empty space, == maxclients means full.
    // 
    numberofcurrentclients = 0;
    void (*handlers[])(struct client_list_t*, int) = {&getheader, &getbodycontentlength, &getbodychunked, &getbodyconnection};
    while(1){
        //check listening socket with select by copying its fdset
        //if new client available add it to the front the list and store it in lowest_open_client
        //if list is full, replace the first item on the list, set client_list_start to the next one, set the client_list_end client to point to new client and set client_list_end to point to new client
        //when removing a client due to lost connection or sent request, check if lower than lowest_open_client
        //when adding client, increment lowest_open_client to next open spot or until >= MAXCLIENTS
        if(!numberofcurrentclients){
            clientindex = lowest_open_client-client_list;
            waitforatleastoneclient(listensock);
        }else{
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
        previous_client = currentclient = client_list_start; //if currentclient == previous_client we know not to link back or else we create an infinite loop
        do{ //must maintain previous_client
            int returnval = 0;
            if(returnval = setjmp(buf)) {
                goto error;
                currentclient=previous_client;
            }
            clientindex = currentclient-client_list;
            {int64_t ticktemp = GetTickCount64();
            if((ticktemp - requests[clientindex].timeout) > 500 && (ticktemp - requests[clientindex].timeout < UINT64_MAX - 6000000)){
                deleteclientandsplicelist(currentclient);
            }}
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
            error:
            currentclient = currentclient->next;
        }while(currentclient);//this is fine cause if we have 0 clients we just wait for accept();
    }
}
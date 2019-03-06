#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>

#include "util/netio.h"

#define SERVER_PORT 5678 // modificat ca argument din linie de comanda

typedef struct _File {
    struct _File *nextFile;
    const char *name;
    const int size;
} File;

File *startFiles; // list of files 

void die(const char *fmt, ...) {
    va_list va;
    va_start(va,fmt);
    fprintf(stderr,"error: ");
    vfprintf(stderr,fmt,va);
    fputc('\n',stderr);
    va_end(va);
    exit(-1);
}

void init() {
    // se uita in folderul de fisiere (available/downloads/etc) 
    // si memoreaza in lista inlantuita de fisiere numele si size-ul fiecaruia
}

void process(int socketfd, struct sockaddr_in remote_addr, socklen_t rlen) {
    printf("Accepted connection from %u", remote_addr.sin_addr.s_addr);
    // infinite loop while socket is open
    // read line from socket, line that contains the command
    // split command by space
    // switch case on command[0] which is a keyword
    // case 'CEI_FA_ASTA': verifica daca are fisierul de la command[1] in lista de fisere disponibile
    // case 'DAMI': verifica daca are fisierul de la command[1] in lista, daca il are returneaza continutul
    //              intre command[2] si command[3], daca nu il are, inchide socket-ul
    // default: inchide socket
}

int main() {
    int sockfd, newsockfd, reuseaddr = 1;
    struct sockaddr_in server_addr, remote_addr;
    socklen_t rlen;

    init();

    if ((sockfd=socket(PF_INET, SOCK_STREAM, 0)) < 0)
    { 
        die("Failed to create socket: errno %d", errno);
    }
    if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(reuseaddr)) < 0) {
        die("Failed to set socket option: errno %d", errno);
    }
    set_addr(&server_addr, NULL, INADDR_ANY, SERVER_PORT);
    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    { 
        die("Failed to bind socket: errno %d", errno);
    }
    if (listen(sockfd, 5) < 0)
    { 
        die("Failed to listen to socket : errno %d", errno);
    }
    printf("Serverul ruleaza pe portul %d\n", SERVER_PORT);
    for (;;) {
        newsockfd = accept(sockfd, (struct sockaddr *)&remote_addr, &rlen);
        if (newsockfd < 0) 
        {
            die("Failed to accept incoming socket request");
        }
        if(fork() == 0 ){
            close(sockfd);
            process(newsockfd, remote_addr, rlen);
            exit(0);
        }
        close(newsockfd);
    }
    return 0;
}
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
#include <dirent.h>
#include <sys/stat.h>

#include "../util/netio.h"
#include "../util/io.h"

#define DEFAULT_PORT 5432
#define DEFAULT_FILES_FOLDER "./files"
#define SOCKET_BUFFER_SIZE 1024

typedef struct _ServedFile {
    struct _ServedFile *nextFile;
    char *path;
    unsigned long size;
} ServedFile;

char *filesDirectory;
ServedFile *files; // list of files 
ServedFile *last;

void die(const char *fmt, ...) {
    va_list va;
    va_start(va,fmt);
    fprintf(stderr,"error: ");
    vfprintf(stderr,fmt,va);
    fputc('\n',stderr);
    va_end(va);
    exit(-1);
}

void cleanUp() {
    ServedFile *q, *aux;
    q=files;
    while(q) {
        aux=q;
        q=q->nextFile;
        free(aux->path);
        free(aux);
    }
    free(filesDirectory);
}

void addServedFile(char *filePath, unsigned long size) {
    ServedFile *next = (ServedFile *)malloc(sizeof(ServedFile));
    next->nextFile = NULL;
    next->path=filePath;
    next->size=size;
    if (last) {
        last->nextFile=next;
        last=next;
    }
    else {
        last = next;
        files = last;
    }
}

void init(char *dirName) {
    DIR *filesDir;
    struct dirent *entry;
    struct stat fileStat;
    int filesDirectoryLength = strlen(filesDirectory);
    char *buffer = (char *) malloc((filesDirectoryLength + strlen(dirName) + 35) * sizeof(char));

    sprintf(buffer, "%s%s", filesDirectory, dirName);
    filesDir = opendir(buffer);
    if(!filesDir) {
        die("Failed to open directory: %s", buffer);
    }
    while((entry = readdir(filesDir)) != NULL) {
        if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        sprintf(buffer, "%s%s/%s", filesDirectory, dirName, entry->d_name);
        stat(buffer, &fileStat);
        buffer = buffer + filesDirectoryLength;
        if(S_ISDIR(fileStat.st_mode)) {
            init(buffer);
        }
        else if(S_ISREG(fileStat.st_mode)) {
            char *filePath = (char *) malloc((strlen(buffer) + 1) * sizeof(char));
            strcpy(filePath, buffer);
            addServedFile(filePath, fileStat.st_size);
        }
    }
    closedir(filesDir);
}

ServedFile *findByPath(const char *path) {
    char *p;
    if(!path) return NULL;
    
    ServedFile *q = files;
    while(q) {
        p=q->path;
        if(strcmp(q->path, path) == 0 || strcmp(++p, path) == 0) {
            break;
        }
        q = q->nextFile;
    }
    return q;
}

void process(int socketfd, struct sockaddr_in remote_addr, socklen_t rlen) {
    printf("Accepted connection from %s:%d\n", inet_ntoa(remote_addr.sin_addr), remote_addr.sin_port);
    ServedFile *q;
    FILE *f;
    char buffer[SOCKET_BUFFER_SIZE];
    char *command, *fileName, *valueFrom, *valueTo;
    unsigned long from, to, current, howMany;
    int n;
    for(;;) {
        n = readLine(socketfd, &buffer, SOCKET_BUFFER_SIZE);
        if (n > 0) {
            command = strtok(buffer, " \n\r\t");
            if(command && strcmp(command, "CEI_FA_ASTA") == 0) {
                fileName = strtok(NULL, " \n\r\t");
                q=findByPath(fileName);
                if(q) {
                    sprintf(buffer, "%s %lu\n", fileName, q->size);
                }
                else {
                    sprintf(buffer, "%d\n", -1);
                }
                write(socketfd, buffer, strlen(buffer));
            }
            else if(command && strcmp(command, "DAMI") == 0) {
                fileName = strtok(NULL, " \n\r\t");
                q=findByPath(fileName);
                if(q) { // File exists
                    valueFrom = strtok(NULL, " \n\r\t");
                    valueTo = strtok(NULL, " \n\r\t");
                    if (valueFrom && valueTo) { // Command format is valid
                        from = atol(valueFrom);
                        to = atol(valueTo);
                        if(from>=0 && to>0 && from<to && to <= q->size) { // Requested bytes are valid
                            sprintf(buffer, "%s%s", filesDirectory, q->path);     
                            f=fopen(buffer, "rb+");
                            if(!f) {
                                printf("Failed to open file for download: %s\n", buffer);
                                break;
                            }
                            fseek(f, from, SEEK_SET);
                            current=from;
                            while(current < to) {
                                howMany = to-current > SOCKET_BUFFER_SIZE ? SOCKET_BUFFER_SIZE : to-current;
                                howMany = fread(buffer, 1, howMany, f);
                                write(socketfd, buffer, howMany);
                                current += howMany;
                            }
                            fclose(f);
                            printf("Served '%s' [%lu, %lu) to  %s:%d\n", q->path, from, to, inet_ntoa(remote_addr.sin_addr), remote_addr.sin_port);
                        }
                        else {
                            sprintf(buffer, "%d\n", -3);
                            write(socketfd, buffer, strlen(buffer));
                        }
                    }
                    else {
                        sprintf(buffer, "%d\n", -2);
                        write(socketfd, buffer, strlen(buffer));
                    }
                }
                else {
                    sprintf(buffer, "%d\n", -1);
                    write(socketfd, buffer, strlen(buffer));
                }
            }
            else {
                printf("Invalid command '%s' error\n", command);
                break;
            }
        }
        else if(n < 0) {
            printf("Socket read error: errno %d\n", errno);
            break;
        }
        else {
            break;
        }
    }
    close(socketfd);
}

int main(int argc, char *argv[]) {
    int port, sockfd, newsockfd, reuseaddr = 1;
    struct sockaddr_in server_addr, remote_addr;
    socklen_t rlen;

    port = argc > 1 ? atoi(argv[1]) : DEFAULT_PORT;
    filesDirectory = argc > 2 ? argv[2] : DEFAULT_FILES_FOLDER;

    init("");

    if ((sockfd=socket(PF_INET, SOCK_STREAM, 0)) < 0)
    { 
        die("Failed to create socket: errno %d", errno);
    }
    if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(reuseaddr)) < 0) {
        die("Failed to set socket option: errno %d", errno);
    }
    set_addr(&server_addr, NULL, INADDR_ANY, port);
    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    { 
        die("Failed to bind socket: errno %d", errno);
    }
    if (listen(sockfd, 5) < 0)
    { 
        die("Failed to listen to socket : errno %d", errno);
    }
    printf("Serverul ruleaza pe portul %d\n", port);
    for (;;) {
        newsockfd = accept(sockfd, (struct sockaddr *)&remote_addr, &rlen);
        if (newsockfd < 0) {
            if(errno == EINTR) continue;
            die("Failed to accept incoming socket request: errno %d", errno);
        }
        if(fork() == 0){
            close(sockfd);
            process(newsockfd, remote_addr, rlen);
            exit(0);
        }
        // process(newsockfd, remote_addr, rlen);
        close(newsockfd);
    }

    cleanUp();
    return 0;
}
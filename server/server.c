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

#include "util/netio.h"

#define DEFAULT_PORT 5432
#define DEFAULT_FILES_FOLDER "./files"
#define SOCKET_BUFFER_SIZE 1024

typedef struct _ServedFile {
    struct _ServedFile *nextFile;
    const char *path;
    const char *name;
    unsigned long int size;
} ServedFile;

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

void addServedFile(char *filePath, char *fileName, unsigned long size) {
    ServedFile *next = (ServedFile *)malloc(sizeof(ServedFile));
    next->nextFile = NULL;
    next->path=filePath;
    next->name=fileName;
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

void init(const char *filesDirName) {
    DIR *filesDir;
    struct dirent *entry;
    struct stat fileStat;
    char *filePath, *fileName;

    filesDir = opendir(filesDirName);
    if(!filesDir) {
        die("Failed to open files directory: %s", filesDirName);
    }
    while((entry = readdir(filesDir)) != NULL) {
        filePath = (char *) malloc((strlen(filesDirName) + strlen(entry->d_name) + 2) * sizeof(char));
        fileName = (char *) malloc((strlen(entry->d_name) +1) * sizeof(char));
        sprintf(filePath, "%s/%s", filesDirName, entry->d_name);
        sprintf(fileName, "%s", entry->d_name);
        stat(filePath, &fileStat);
        if(S_ISREG(fileStat.st_mode)) {
            addServedFile(filePath, fileName, fileStat.st_size);
        }
    }
    closedir(filesDir);
}

int readLine(int fd, void *buffer, int n) {
    int totalRead = 0, chRead;
    char *buf;
    char ch;

    buf = buffer;
    for(;;) {
        chRead = read(fd, &ch, 1);
        if(chRead > 0) {
            if(totalRead < n-1) {
                if(ch == '\n') {
                    break;
                }
                totalRead++;
                *buf++ = ch;
            }
        } 
        else if (chRead == 0) {
            if(totalRead == 0) {
                return 0;
            }
            else {
                break;
            }
        }
        else {
            return -1;
        }
    }

    *buf = '\0';
    return totalRead;
}

ServedFile *findByName(const char *name) {
    if(!name) return NULL;

    ServedFile *q = files;
    while(q && strcmp(q->name, name) != 0) {
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
    long int from, to, current;
    int n, howMany;
    for(;;) {
        n = readLine(socketfd, &buffer, SOCKET_BUFFER_SIZE);
        if (n > 0) {
            command = strtok(buffer, " \n\r\t");
            if(command && strcmp(command, "CEI_FA_ASTA") == 0) {
                fileName = strtok(NULL, " \n\r\t");
                q=findByName(fileName);
                sprintf(buffer, "%s %lu\n", fileName, q ? q->size : -1);
                write(socketfd, buffer, strlen(buffer));
            }
            else if(command && strcmp(command, "DAMI") == 0) {
                fileName = strtok(NULL, " \n\r\t");
                q=findByName(fileName);
                if(q) { // File exists
                    valueFrom = strtok(NULL, " \n\r\t");
                    valueTo = strtok(NULL, " \n\r\t");
                    if (valueFrom && valueTo) { // Command format is valid
                        from = atol(valueFrom);
                        to = atol(valueTo);
                        if(from>=0 && to>0 && from<to && (unsigned)to <= q->size) { // Requested bytes are valid     
                            f=fopen(q->path, "rb");
                            if(!f) {
                                printf("Failed to open file for download: %s\n", q->path);
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
                            sprintf(buffer, "\n");
                            write(socketfd, buffer, strlen(buffer));
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
            printf("Socket read error\n");
            break;
        }
    }
}

int main(int argc, char *argv[]) {
    int port, sockfd, newsockfd, reuseaddr = 1;
    char *filesdir;
    struct sockaddr_in server_addr, remote_addr;
    socklen_t rlen;

    port = argc > 1 ? atoi(argv[1]) : DEFAULT_PORT;
    filesdir = argc > 2 ? argv[2] : DEFAULT_FILES_FOLDER;

    init(filesdir);

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
    return 0;
}
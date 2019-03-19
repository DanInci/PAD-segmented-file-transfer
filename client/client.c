#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <math.h>
#include <errno.h>
#include <sys/socket.h>

#include "../util/io.h"
#include "../util/netio.h"
#include "../util/progress.h"

    // parinte: citeasca fiecare linie din servers.config, linie ce reprezinta adresa de conectare la un server
    //          interogheaza serverele si vede cate din ele sunt disponibile si se pune intr-o lista circulara
    //          verifica daca diminesiunea fisierului e mai mare decat nr de segmente
    //          calculeaza nr de bytes pe segment in mod egal, in functie de dimensiunea fisierului si nr de segmente (sa fie primul intreg superior rezultatului impartirii)
    //          creaza un proces pentru fiecare segment si imparte request-urile catre serverele disponibile intr-o maniera round robin
    //          uneste toate fisierele temporare
    //          sterge fisierele temporare
    //          verifica diminesiune fisier rezultat
    // fiu:     creaza conexiunea catre server
    //          trimite comanda de request de fiser la server
    //          citeste informatia din socket si o pune intr-un buffer
    //          scrie continutul buffer-ului intr-un fisier partial (nume_fiser-nr_segment)
    //          inchide conexiunea

#define MAX_SERVERS 100
#define SERVERS_CONFIG "./servers.config"
#define SOCKET_BUFFER_SIZE 1024

typedef struct _Server {
    char *addr;
    int port;
} Server;

typedef struct _ReachedServer {
    const Server *info;
    int socketFd;
    unsigned long fileSize;
} ReachedServer;

int serversNo; // number of known servers;
Server **servers; // dynamic array of known servers from servers.config

int reachedServerNo; // number of reached servers;
ReachedServer **reachedServers; // dynamic array of reached servers that contain the file

/*
 * Clean ups -- e.g. memory deallocations
 */
void cleanUp() {
    int i = 0;
    for(i=0; i<serversNo; i++) {
        free(servers[i]->addr);
        free(servers[i]);
        if(i<reachedServerNo) {
            close(reachedServers[i]->socketFd);
            free(reachedServers[i]);
        }
    }
}

void die(const char *fmt, ...) {
    va_list va;
    va_start(va,fmt);
    fprintf(stderr,"error: ");
    vfprintf(stderr,fmt,va);
    fputc('\n',stderr);
    va_end(va);
    cleanUp();
    exit(-1);
}

void addServer(char *line) {
    if(!servers) {
        servers = (Server **) malloc(sizeof(Server *));
    }
    else {
        servers = (Server **) realloc(servers, (serversNo+1) * sizeof(Server *));
    }
    Server *s = (Server *) malloc(sizeof(Server));
    char *colon = strchr(line, ':');
    if(colon) {
        int addrLen = colon-line;
        s->addr = (char *) malloc((addrLen + 1) * sizeof(char));
        strncpy(s->addr, line, addrLen);
        s->addr[addrLen] = '\0';
        s->port = atoi(colon+1);       
    }
    else {
        s->addr = (char *) malloc((strlen(line) + 1) * sizeof(char));
        s->port = -1;
    }
    servers[serversNo] = s;
    serversNo++;
}

void addReachedServer(const Server *info, const int socketFd, const unsigned long fileSize) {
    if(!reachedServers) {
        reachedServers = (ReachedServer **) malloc(sizeof(ReachedServer *));
    }
    else {
        reachedServers = (ReachedServer **) realloc(reachedServers, (reachedServerNo+1) * sizeof(ReachedServer *));
    }

    ReachedServer *s = (ReachedServer *) malloc(sizeof(ReachedServer));
    s->info = info;
    s->socketFd = socketFd;
    s->fileSize = fileSize;

    reachedServers[reachedServerNo] = s;
    reachedServerNo++;
}

/*
 * Reads known server addresses from each line in servers.config
 */
void readServersConfig() {
    char line[50];
    char *end;
    FILE *file;
    file = fopen(SERVERS_CONFIG, "r");
    if(!file) {
        die("Failed to open %s\n", SERVERS_CONFIG);
    }
    while(fgets(line, sizeof(line), file) != NULL && serversNo < MAX_SERVERS) {
        if ((end = strchr(line, '\n'))) {
            *end = '\0';
        }
        addServer(line);
    }
    fclose(file);
}

int openNewSocket(char *addr, int port) {
    int sockFd;
    struct sockaddr_in localAddr, remoteAddr;
    if((sockFd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        die("Failed to create socket: errno %d", errno);
    }
    set_addr(&localAddr, NULL, INADDR_ANY, 0);
    if(bind(sockFd, (struct sockaddr *) &localAddr, sizeof(localAddr)) < 0) {
        die("Failed to bind socket: errno %d", errno);
    }
    set_addr(&remoteAddr, addr, 0, port);
    if(connect(sockFd, (struct sockaddr *) &remoteAddr, sizeof(remoteAddr)) < 0) {
        printf("Failed to connect to '%s:%d'\n", addr, port);
        return -1;
    }
    return sockFd;
}

/*
 * Creates a socket connection to the server with the given addr and asks for the file
 * Adds successful interogation responses to ReachedServers list
 */
void interogateServer(const Server *s, const char *fileName) {
    char buffer[SOCKET_BUFFER_SIZE];
    int sockFd = openNewSocket(s->addr, s->port);
    
    if(sockFd != -1) {
        sprintf(buffer, "CEI_FA_ASTA %s\n", fileName);
        write(sockFd, buffer, strlen(buffer));
        readLine(sockFd, buffer, SOCKET_BUFFER_SIZE);

        char *result = strtok(buffer, " \r\n\t\0");
        if(result && strcmp(result, "-1") == 0) {
            printf("File '%s' is missing from '%s:%d'\n",fileName, s->addr, s->port);
        }
        else if (result) {
            char *sizeStr = strtok(NULL, " \r\n\t\0");
            printf("File '%s' was found on '%s:%d' with size %s\n", fileName, s->addr, s->port, sizeStr);
            addReachedServer(s, sockFd, atol(sizeStr));
        }
    }
}

/*
 * Checks if there were any reached servers
 * Checks if reached servers found the same file (same file size)
 * Checks if file size is bigger than given segment number
 */
int validateReachedServers(int segmentsNo) {
    int i;
    unsigned long headSize=0;
    
    if(reachedServerNo == 0) return -1;

    headSize=reachedServers[0]->fileSize;
    
    for(i=1; i<reachedServerNo; i++) {
        if(headSize != reachedServers[i]->fileSize) {			    
	        return -2;
        }
    }
    
    if(headSize<segmentsNo) return -3;

    return 0;
}

/*
 * Something to find a proper distribution of bytes per segment
 */
unsigned long calculateBytesPerSegment(int segmentsNo) {
	return reachedServers[0]->fileSize/segmentsNo;
}

char *getPartialFileName(int segmentNo) {
    char *partialFileName = (char *) malloc(20 * sizeof(char));
    sprintf(partialFileName, "partial%d", segmentNo);
    return partialFileName;
}

/*
 * Uses the existing socket from ReachedServer structure or creates a new one
 * Requests the file segment and reads it into a buffer
 * Writes the contents of the buffer into a partial file
 * Closes socket connection
 */
void downloadSegment(ReachedServer *s, const char *fileName, const int currentSegmentNo, unsigned long segmentSize, const unsigned long la, const unsigned long lb) {
    FILE *g;
    char *pfName;
    char buffer[SOCKET_BUFFER_SIZE];
    int n;

    pfName = getPartialFileName(currentSegmentNo);
    g = fopen(pfName, "wb");
    if(!g) {
        printf("Failed to write partial file '%s'\n", pfName);
        free(pfName);
        exit(-1);
    }

    sprintf(buffer, "DAMI %s %lu %lu\n", fileName, la, lb);
    write(s->socketFd, buffer, strlen(buffer));
    while(segmentSize>0) {
        if((n = read(s->socketFd, &buffer, segmentSize < SOCKET_BUFFER_SIZE ? segmentSize : SOCKET_BUFFER_SIZE)) < 0) {
                printf("Failed to read bytes from '%s:%d'\n", s->info->addr, s->info->port);
                fclose(g);
                free(pfName);
                close(s->socketFd);
                exit(-1);
        }
        if(fwrite(buffer, 1, n, g) < n) {
            printf("Failed to write all the bytes received from '%s:%d' to partial file '%s'\n", s->info->addr, s->info->port, pfName);
            fclose(g);
            free(pfName);
            close(s->socketFd);
            exit(-1);
        }
	    segmentSize -= n;
    }
    close(s->socketFd);
    fclose(g);
    free(pfName);
}

/*
 * Merges the partial files into a single file
 * Deletes partial files afterwards
 */

void mergePartialFiles(char *fileName, int segmentsNo) {
    char buff[SOCKET_BUFFER_SIZE];
    char *fname;
    int i, n, ret;
    FILE *finalFile = fopen(fileName, "wb");
    if(!finalFile) {
        die("Failed to create final file '%s'", finalFile);
    }
    for(i=0;i<segmentsNo;i++) {	
        fname = getPartialFileName(i);
	    FILE *pf=fopen(fname, "rb");
        if(!pf) {
            die("Failed to open partial file %s", fname);
        }
        do {
            if((n = fread(buff, 1, SOCKET_BUFFER_SIZE, pf)) < 0) {
                die("Failed to read bytes from partial file");
            }
            if(fwrite(buff, 1, n, finalFile) < n) {
                die("Failed to write all the bytes from partial file '%s' to the final file", fname);
            }
        } while(n == SOCKET_BUFFER_SIZE);
        fclose(pf);
        ret=remove(fname);
	    if(ret < 0) {
      	    die("Failed to delete partial file %s", fname);
   	    }
        free(fname);
    }
    fclose(finalFile);
}

int compareFileSize(char *fileName, unsigned long size) {
    struct stat st;
    stat(fileName, &st);
    return st.st_size -  size;
}

void on_progress (progress_data_t *data) {
  progress_write(data->holder);
}

int main(int argv, char *argc[]) { // nume fisier, nr segmente
    ReachedServer *s;
    int  err, segmentsNo, i, secondPass=0;
    unsigned long bytesPerSegment, segmentSize, la, lb;
    char *fileName;
    progress_t *progress;

    if(argv < 3) {
        die("Usage: %s <file_name> <segments_no>", argc[0]);
    }

    fileName = argc[1];
    segmentsNo = atoi(argc[2]);
    
    readServersConfig();

    for(i=0; i<serversNo; i++) {
        interogateServer(servers[i], fileName);
    }

    if((err = validateReachedServers(segmentsNo)) < 0) {
        switch(err) { // switch case for different validation err codes
            case -1:
                die("There are no reached servers");
                break;
	        case -2:
                die("Different file sizes on servers");
                break;
	        case -3:
                die("File size is smaller that desired number of segments");
                break;
        }
    } 
    printf("%d out of %d servers were reached for file '%s'\n", reachedServerNo, serversNo, fileName);
    
    bytesPerSegment = calculateBytesPerSegment(segmentsNo);

    for (i=0; i<segmentsNo; i++) {
        s=reachedServers[i % reachedServerNo];
        if(secondPass == 1) {
            s->socketFd = openNewSocket(s->info->addr, s->info->port);
        } else if(i % reachedServerNo) {
            secondPass = 1;
        }
        la = bytesPerSegment*i;
        lb = i == segmentsNo-1 ? s->fileSize : bytesPerSegment*i + bytesPerSegment;
        segmentSize = i == segmentsNo-1 ? s->fileSize-la : bytesPerSegment;
        if(fork() == 0){
            downloadSegment(s, fileName, i, segmentSize, la, lb);
            exit(0);
        }
        // downloadSegment(s, fileName, i, bytesPerSegment, la, lb);
    }

    progress = progress_new(segmentsNo, 70);
    progress->fmt = "Downloading: [:bar] :percent :elapsed";
    progress_on(progress, PROGRESS_EVENT_PROGRESS, on_progress);

    progress_write(progress);
    for(i=0; i<segmentsNo; i++) {
        wait(NULL);
        progress_tick(progress, 1);
    }

    printf("\nMerging partial files...\n");
    mergePartialFiles(fileName, segmentsNo);
    
    if (compareFileSize(fileName, reachedServers[0]->fileSize) != 0) {
	    die("Failed to verify download!");
    }
    
    printf("Download successful!\n");
    cleanUp();
    return 0;
}
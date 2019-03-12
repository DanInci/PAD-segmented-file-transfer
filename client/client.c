#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <math.h>

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

#define MAX_ADDR_LENGTH 100

typedef struct _ReachedServer {
    struct _ReachedSever *next;
    const char *addr;
    const int socketFd;
    const int fileSize;
} ReachedServer;

int serversNo; // number of known servers;
int newFileSize;
char (*server_addrs)[MAX_ADDR_LENGTH]; // dynamic array of known servers from servers.config

ReachedServer *servers; // pointer to a server that contains the file

/*
 * Clean ups -- e.g. memory deallocations
 */
void cleanUp() {

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

/*
 * Reads known server addresses from each line in servers.config
 */
void readServersConfig() { 

}

/*
 * Creates a socket connection to the server with the given addr and asks for the file
 * Adds successfull interogation responses to ReachedServers list
 * Concurrency problems might occur
 */
void interogateServer(const char *addr, const char *fileName) {

}

/*
 * Checks if there were any reached servers
 * Checks if reached servers found the same file (same file size)
 * Checks if file size is bigger than given segment number
 */
int validateReachedServers(int segmentsNo) {
    return -1;
}

/*
 * Something to find a proper distribution of bytes per segment
 */
int calculateBytesPerSegment(int segmentsNo) {
	double segmentsSize=0;   
	segmentsSize=ceil((double)servers->fileSize/segmentsNo);
    	return segmentsSize;
}

/*
 * Uses the existing socket from ReachedServer structure or creates a new one
 * Requests the file segment and reads it into a buffer
 * Writes the contents of the buffer into a partial file
 * Closes socket connection
 */
void downloadSegment(ReachedServer *s, int currentSegmentNo, const char *fileName, int bytesPerSegment) {

}

/*
 * Merges the partial files into a single file
 * Deletes partial files afterwards
 */

void mergePartialFiles(char *fileName,int segmentsNo) {
    int i,n,k,ret;
    struct stat st;
    FILE *finalFile =fopen(fileName,"a");
    if(finalFile==NULL)
    {
        die("Shit");
    }
    char buff[4097], fname[100];
    for(i=0;i<segmentsNo;i++)
    {	
	char segNo=i+'0';
	strcpy(fname,fileName);
        strcat(fileName,segNo);
	FILE *pf=fopen(fname,"rb");
        if(pf==NULL)
        {
            die("Shit");
        }
        while((n=fread(buff,sizeof(char),4096,pf)))
        {
            k=fwrite(buff,sizeof(char),n,finalFile);
            if(!k)
            {
            die("Shit");
            }
        }
        fclose(pf);
        ret=remove(fname);
	if(ret == 0) 
	{
      	    die("File deleted successfully");
   	} 
	else 
	{
      	    die("Error: unable to delete the file");
   	}
    }
    stat(finalFile,&st);
    newFileSize=st.st_size;
    fclose(finalFile);
    return 0;
}

int main(int argv, char *argc[]) { // nume fisier, nr segmente
    int i, err, segmentsNo;
    char *fileName, *ptr;

    if(argv < 3) {
        die("Usage: %s <file_name> <segments_no>", argc[0]);
    }

    fileName = argc[1];
    segmentsNo = atoi(argc[2]);
    
    readServersConfig();

    for(i=0, ptr=server_addrs; i<serversNo; i++, ptr++) {
        if(fork() == 0){
            interogateServer(ptr, fileName);
            exit(0);
        }
    }

    if((err = validateReachedServers(segmentsNo)) < 0) {
        switch(err) { // switch case for different validation err codes
            case -1:
                die("Bla bla bla");
                break;
        }
    } 
    
    int bytesPerSegment = calculateBytesPerSegment(segmentsNo);

    ReachedServer *current;
    for(i=0;i<segmentsNo;i++) {
        if(current == NULL) {
            current = servers;
        }
        if(fork() == 0){
            downloadSegment(current, i, fileName, bytesPerSegment);
            exit(0);
        }
        current = current->next;
    }

    mergePartialFiles(fileName,segmentsNo);
    
    if(servers->fileSize!=newFileSize)
    {
	die("Download error!");
    }
    else
	die("Your file is downloaded!");
    
    cleanUp();
    return 0;
}
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>

int set_addr(struct sockaddr_in *addr, const char *name, u_int32_t inaddr, short sin_port) {
    struct hostent *h;

    memset((void *) addr, 0, sizeof(*addr));
    addr->sin_family = AF_INET;
    if(name != NULL) {
        h = gethostbyname(name);
        if(h == NULL) {
            return -1;
        }
        addr->sin_addr.s_addr = *(u_int32_t *) h->h_addr_list[0];
    } else {
        addr->sin_addr.s_addr = htonl(inaddr);        
    }
    if (sin_port != -1) addr->sin_port = htons(sin_port);
    return 0;
}
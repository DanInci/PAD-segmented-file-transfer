#include <stdio.h>
#include <unistd.h>

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
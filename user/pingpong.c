#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
    if (argc > 1) {
        fprintf(2, "Too many args\n");
        exit(1);
    }
    int f2c[2], c2f[2];
    pipe(f2c);
    pipe(c2f);
    int pid = fork();
    if(pid == 0){
        char buf[4];
        read(f2c[0], buf, 4);
        fprintf(1, "%d: received %s\n", getpid(), buf);
        write(c2f[1], "pong", 4);
        close(c2f[1]);
    }
    else{
        write(f2c[1], "ping", 4);
        close(f2c[1]);
        char buf[4];
        read(c2f[0], buf, 4);
        fprintf(1, "%d: received %s\n", getpid(), buf);
        
    }
    exit(0);
}
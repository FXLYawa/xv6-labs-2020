#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int is_prime(int n) {
    if (n < 2) {
        return 0;
    }
    for (int i = 2; i * i <= n; i++)
        if (n % i == 0) return 0;
    fprintf(1, "prime %d\n", n);
    return 1;
}

void prime(int pip){
    int t;
    if(read(pip, &t, sizeof(t))==0)return;
    is_prime(t);
    int pp[2];
    pipe(pp);
    int pid=fork();
    if(pid==0){
        close(pip);
        close(pp[1]);
        prime(pp[0]);
        close(pp[0]);
        exit(0);
    }
    else{
        close(pp[0]);
        while(read(pip, &t, sizeof(t))){
            write(pp[1], &t, sizeof(t));
        }
        close(pp[1]);
        close(pip);
        wait(0);
    }
}

int 
main(int argc, char *argv[]) {
    if (argc > 1) {
        fprintf(2, "Too many args\n");
        exit(1);
    }
    int p[2];
    pipe(p);
    int pid=fork();
    if(pid==0){
        close(p[1]);
        prime(p[0]);
        close(p[0]);
    }
    else{
        close(p[0]);
        for (int i = 2; i <= 35; i++) {
            write(p[1], &i, sizeof(i));
        }
        close(p[1]);
        wait(0);
    }
    exit(0);
}
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc,char *argv[]){
    if(argc < 2){
        fprintf(2,"Usage: sleep time\n");
        exit(1);
    }
    else if(argc>3){
        fprintf(2,"Too many args\n");
        exit(1);
    }
    sleep(atoi(argv[1]));
    exit(0);
}
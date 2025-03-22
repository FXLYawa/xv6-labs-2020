#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"


char * readline(){
    char *line = (char *)malloc(100 * sizeof(char));
    int i = 0;
    char c;
    while(read(0,&c,1)){
        if(c == '\n'||c=='\0'||c=='\r'||c==-1){
            line[i] = '\0';
            break;
        }
        line[i++] = c;
    }
    if(i <= 1){
        free(line);
        return 0;
    }
    return line;
}



int
main(int argc,char *argv[]){
    if(argc < 3){
        fprintf(2,"Too few args\n");
        exit(1);
    }
    else if(argc > 3){
        fprintf(2,"Too many args\n");
        exit(1);
    }
    char *line;
    char **args;
    args = (char **)malloc(3 * sizeof(char *));
    args[0] = (char *)malloc(100 * sizeof(char));
    args[1] = (char *)malloc(100 * sizeof(char));
    args[2] = (char *)malloc(100 * sizeof(char));
    strcpy(args[0],argv[1]);
    strcpy(args[1],argv[2]);
    while((line = readline())!=0){
        strcpy(args[2],line);
        free(line);
        if(fork() == 0){
            exec(args[0],args);
            fprintf(2,"xargs: exec %s failed\n",argv[1]);
            exit(1);
        }
        wait(0);
    }
    
    exit(0);
}
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<stdlib.h>
#include<string.h>
#include<sys/wait.h>
#include<fcntl.h>
#include<dirent.h>

#define LINE_SIZE 1024
#define TOK_SIZE 64
#define DELIMITER " \t\r\n\a"

typedef struct cmd{
    struct cmd * next;
    char ** args;
    char * in , * out;
    int background;
}cmd;

void shell_loop();
char * shell_read_line();
cmd * arg_to_cmd(char **);
void read_line_record(char*,int);
char ** shell_parsing(char*);
int shell_run(cmd*);
int pipe_operation(cmd*);
int operation(int , int ,cmd *);
void cmd_check(cmd *);

int shell_cd(char **);
int shell_help(char**);
int shell_exit(char **);
int shell_echo(char**);
int shell_record(char**);
int shell_replay(char **);
int shell_my_pid(char**);
char * record_buffer[16];
int record_head = 0, record_tail = 0;
char * builtin_str[] = {
    "cd" , "help" , "exit" , "echo" , "record" , "replay" , "mypid"
};
int (*builtin_function[])(char **) = {
    &shell_cd , &shell_help , &shell_exit , &shell_echo , &shell_record , &shell_replay, &shell_my_pid
};
int shell_cd(char **args){
    if(args[1] == NULL) {
        fprintf(stderr,"lack of argument \"cd\"\n");
    }
    else{
        if(chdir(args[1])!=0){
            perror("cd failed\n");
        }
    }
    return 1;
}
int shell_help(char **args){
    printf("this help function\n");
    return 1;
}
int shell_exit(char ** args){
    return 0;
}

int shell_echo(char ** args){
    int size=0;
    while(args[size]!=NULL)size++;
    int i =1,option = 0;
    if(strcmp(args[i],"-n") == 0){option = 1;i++;}
    for(;i<size;i++){
        printf("%s",args[i]);
        if(i!=size-1)printf(" ");
    }
    if(!option) printf("\n");
    return 1;
}

int shell_record(char ** args){
    int start = record_head;
    int index = start;
    int tmp = 1;
    if(record_head < record_tail) index = record_tail;
    do
    {
        printf("%2d: %s\n",tmp,record_buffer[start]);
        start++;
        tmp++;
        start%=16;
    } while (start != index);
    
    return 1;
}

int shell_replay(char ** args){
    char * str = args[1];
    int index;
    if(str[1]!='\0'){
        index = (str[0] - '0') * 10 + (str[1] - '0')-1;
    }
    else{
        index = str[0] - '0' -1;
    }
    index += record_head;
    index%= 16;
    char * line = malloc(sizeof(char)*BUFSIZ);
    strcpy(line , record_buffer[index]);
    char ** arg = shell_parsing(line);
    cmd * cm = arg_to_cmd(arg);
    shell_run(cm);
    free(line);
    free(arg);
    cmd * tmp = cm->next;
    while(cm!= NULL){
        free(cm);
        cm = tmp;
        if(tmp!=NULL) tmp = tmp->next;
    }
    return 1;
}
int shell_my_pid(char **args){
    if(strcmp(args[1], "-i") == 0){
        FILE * fp = fopen("/proc/self/stat","r");
        int pid;
        fscanf(fp,"%d",&pid);
        printf("%d\n",pid);
        return 1;
    }
    else{
        char target[100];
        sprintf(target,"/proc/%s/stat",args[2]);
//        printf("%s\n",target);
        FILE * fp = fopen(target , "r");
        if(fp == NULL){
            printf("this process dont exist\n");
            return 1;
        }
        else{
            int pid , ppid;
            char comm[1000] , state;
            fscanf(fp,"%d %s %c %d",&pid,comm,&state,&ppid);
            if(strcmp("-p",args[1])==0){
                printf("%d\n",ppid);
            }
            if(strcmp("-c",args[1])==0){
                if(args[2] == NULL){
                    printf("mypid -c: lack of arguments\n");
                }
                DIR * dir;
                dir = opendir("/proc/");
                struct dirent * diretmp;
                while ((diretmp = (struct dirent *)readdir(dir)) != NULL)
                {
                    int is_num = 1;
                    for(int i=0;i<strlen(diretmp->d_name)-1;i++){
                        char c = diretmp->d_name[i];
                        if(c<'0'||c>'9'){
                            is_num = 0;
                        }
                    }
                    if(!is_num){
                        continue;
                    }
                    char name[LINE_SIZE];
                    sprintf(name,"/proc/%s/stat",diretmp->d_name);
                    FILE * fp = fopen(name , "r");
                    char pid[100] , ppid[100] , comm[100] , c;
                    fscanf(fp,"%s %s %c %s",pid , comm ,&c , ppid);
                    if(strcmp(ppid , args[2]) == 0){
                        printf("%s\n", pid);
                    }
                    fclose(fp);
                }
                closedir(dir);
            }
            
        }
    }
    return 1;
}

int builtin_func_num(){
    return sizeof(builtin_str) / sizeof(char *);
}



void main(){
    record_buffer[record_head] = NULL;
    shell_loop();
    return;
}
void shell_loop(){
    char * line;
    char ** args;
    int status;
    do{
        printf(">>>$ ");
        line = shell_read_line();
        args = shell_parsing(line);
        cmd * cm = arg_to_cmd(args);
        if(cm->args[0] == NULL) continue;
        //cmd_check(cm);
        status = shell_run(cm);
        
        cmd * tmp = cm->next;
        while (cm != NULL)
        {
            free(cm);
            cm = tmp;
            if(tmp!=NULL)tmp = tmp->next;
        }
        free(line);
        free(args);
    }while(status);
    return;
}
char * shell_read_line(){
    int buf_size = LINE_SIZE;
    int position = 0;
    int c;
    char * buffer = malloc(sizeof(char) * buf_size);
    if(!buffer){
        fprintf(stderr,"first line allocation failed\n");
        exit(EXIT_FAILURE);
    }
    while (1)
    {
        c= getchar();
        if(c == EOF) exit(EXIT_SUCCESS);
        else if(c == '\n'){
            buffer[position] = '\0';
            if(position!=0) read_line_record(buffer,buf_size);
            return buffer;
        }
        else{
            buffer[position] = c;
        }
        position++;
        if(position >= buf_size){
            buf_size += LINE_SIZE;
            buffer = realloc(buffer,buf_size);
            if(!buffer){
                fprintf(stderr,"line allocation failed\n");
                exit(EXIT_FAILURE);
            }
        }
    }
}
void read_line_record(char * line,int size){
    if(strstr(line,"replay")!=NULL){
        return;
    }
    char * rec = malloc(sizeof(char)* size);
    strcpy(rec,line);
    if(record_tail == record_head && record_buffer[record_head] != NULL){
        free(record_buffer[record_head]);
        record_head++;
        record_head%=16;
    }
    record_buffer[record_tail] = rec;
    record_tail++;
    record_tail%=16;

    return;
}
char ** shell_parsing(char * line){
    int buf_size = TOK_SIZE;
    int position = 0;
    char ** tokens = malloc(sizeof(char*)*buf_size);
    if(!tokens){
        fprintf(stderr,"first token allocation failed\n");
        exit(EXIT_FAILURE);
    }
    char * token = strtok(line,DELIMITER);
    while(token != NULL){
        tokens[position++] = token;
        if(position >= buf_size){
            buf_size += TOK_SIZE;
            tokens = realloc(tokens,buf_size);
            if(!tokens){
                fprintf(stderr,"token allocation failed\n");
                exit(EXIT_FAILURE);
            }
        }
        token = strtok(NULL,DELIMITER);
    }
    tokens[position] = NULL;
    return tokens;
}
int shell_execute(char ** args){
    for(int i=0;i<builtin_func_num();i++){
        if(strcmp(args[0] , builtin_str[i]) == 0)
        return builtin_function[i](args);
    }
    if(execvp(args[0],args) == -1){
        perror("exec failed\n");
        exit(EXIT_FAILURE);
    }
}
int shell_run(cmd * cm){
    int fd, in  = dup(0) , out = dup(1);
    int status = -1;
    if(cm->next == NULL){
        if(cm->in != NULL){
            fd = open(cm->in,O_RDONLY);    
            dup2(fd,STDIN_FILENO);
            close(fd);
        }
        if(cm->out != NULL){
            fd = open(cm->out , O_RDWR | O_CREAT , 0644);
            dup2(fd,STDOUT_FILENO);
            close(fd);
        }
        for(int i =0;i< builtin_func_num();i++){
            if(strcmp(builtin_str[i] , cm->args[0]) == 0){
               status = builtin_function[i](cm->args);
            }
        }
        if(cm->in != NULL){
            dup2(in, STDIN_FILENO);
        }
        if(cm->out != NULL){
            dup2(out , STDOUT_FILENO);
        }
        close(in);
        close(out);
    }
    if(status == -1){
        return pipe_operation(cm);
    }
    return status;
}
cmd * arg_to_cmd(char ** args){
    char * token = args[0];
    char ** tmp = args;
    cmd * cm = (cmd *)malloc(sizeof(cmd));
    char ** processed_arg = malloc(sizeof(char*) * TOK_SIZE);
    if(!processed_arg){
        perror("buffer allocation failed\n");
        exit(EXIT_FAILURE);
    }
    int buffer_size = TOK_SIZE;
    int index = 0;
    cm->args = processed_arg;
    cm->in = NULL;
    cm->next = NULL;
    cm->out = NULL;
    cm->background = 0;
    cmd * cm_tmp = cm;
    for(int i=0;token != NULL;i++,token = args[i]){
        if(strcmp(token , "|") == 0){
            args[i-1] = NULL;
            cmd * new = (cmd *)malloc(sizeof(cmd));
            processed_arg = malloc(sizeof(char*) * TOK_SIZE);
            new->args = processed_arg;
            new->in = NULL;
            new->out = NULL;
            new->next = NULL;
            new->background = 0;
            cm_tmp->next = new;
            cm_tmp = new;
            index = 0;
            continue;
        }
        else if(strcmp(token, ">") == 0){
            i++;token = args[i];
            cm_tmp->out = token;
        }
        else if(strcmp(token, "<") == 0){
            i++;token = args[i];
            cm_tmp->in = token;
        }
        else if(strcmp(token , "&") == 0){
            cm_tmp->background = 1;
        }
        else{
            processed_arg[index++] = token;
            if(index >= buffer_size){
                buffer_size += TOK_SIZE;
                processed_arg = realloc(processed_arg,buffer_size);
                if(!processed_arg){
                    perror("reallocation failed\n");
                    exit(EXIT_FAILURE);
                }
            }
        }

    }
    return cm;
}
int pipe_operation(cmd * cm){
    int fd[2] , in=0 ,out=1;
    cmd * tmp = cm;
    while(tmp->next != NULL){
        pipe(fd);
        operation(in , fd[1], tmp);
        close(fd[1]);
        in = fd[0];
        tmp = tmp->next;
    }
    if(in != 0){
        operation(in , STDOUT_FILENO , tmp);
        return 1;
    }
    operation(STDIN_FILENO,STDOUT_FILENO,tmp);
    return 1;
}
int operation(int in , int out , cmd * cm){
    pid_t pid;
    pid = fork();
    if(pid == -1){
        perror("fork failed\n");
        exit(EXIT_FAILURE);
    }
    if(pid == 0){
        if( in != 0 ){
            dup2(in , 0);
            close(in);
        }
        else{
            if(cm->in != NULL){
                int d = open(cm->in , O_RDONLY);
                dup2(d , 0);
                close(d);
            }
        }
        if( out != 1){
            dup2(out , 1);
            close(out);
        }
        else{
            if(cm->out != NULL){
                int d = open(cm->out , O_RDWR| O_CREAT , 0644);
                dup2(d , 1);
                close(d);     
            }
        }
        if(shell_execute(cm->args) == -1){
            perror("execution failed\n");
            exit(EXIT_FAILURE);
        }
    }
    else{
        int status;
        if(cm->background){
            if(cm->next == NULL)
            printf("pid :%d\n",pid);
        }
        do{
            waitpid(pid , &status , WUNTRACED);
        }while(!WIFEXITED(status) && !WIFSIGNALED(status));
    }
    return 1;
}
void cmd_check(cmd * cm){
    while(cm!=NULL){
        printf("%s %s ",cm->in , cm->out);
        char * token;
        int i=0;
        do
        {
            token = cm->args[i];
            printf("%s ",token);
            i++;
        } while (token != NULL);
        printf("\n");
        cm = cm->next;
    }
    return;
}

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/wait.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>

#include "commands.h"
#include "built_in.h"

#define FILE_SERVER "/tmp/test_server"



pid_t proc_create(struct single_command,int index_of_commands,int* result);
void* server();

static struct built_in_command built_in_commands[] = {
  { "cd", do_cd, validate_cd_argv },
  { "pwd", do_pwd, validate_pwd_argv },
  { "fg", do_fg, validate_fg_argv }
};

static int is_built_in_command(const char* command_name)
{
  static const int n_built_in_commands = sizeof(built_in_commands) / sizeof(built_in_commands[0]);

  for (int i = 0; i < n_built_in_commands; ++i) {
    if (strcmp(command_name, built_in_commands[i].command_name) == 0) {
      return i;
    }
  }

  return -1; // Not found
}

/*
 * Description: Currently this function only handles single built_in commands. You should modify this structure to launch process and offer pipeline functionality.
 */
int evaluate_command(int n_commands, struct single_command (*commands)[512])
{
int result=0;
pid_t pid2;
int status;
int temp_in, temp_out;
pthread_t server_thread;

temp_in = dup(0);

int client_sock;
struct sockaddr_un server_addr;

  if (n_commands > 0) {
    struct single_command* com = (*commands);

    assert(com->argc != 0);
if(n_commands>1){
	pthread_create(&server_thread,NULL,server,NULL);
}
for(int i = 0;i<n_commands;i++){
if(!strcmp(com[i].argv[com[i].argc-1],"&")){
	com[i].argv[com[i].argc-1]=NULL;
	strcpy(background.bg_commands,"");
	for(int j=0;j<com[i].argc-1;j++){
		strcat(background.bg_commands,com[i].argv[j]);
		strcat(background.bg_commands," ");
	}
	background.bg_int++;//bg:0 -> not bg, bg:1 -> background
}
    int built_in_pos = is_built_in_command(com[i].argv[0]);
    if (built_in_pos != -1) {
      if (built_in_commands[built_in_pos].command_validate(com[i].argc, com[i].argv)) {
	if(i>0){
		dup2(temp_in,0);
	}
        if (built_in_commands[built_in_pos].command_do(com[i].argc, com[i].argv) != 0) {
          fprintf(stderr, "%s: Error occurs\n", com[i].argv[0]);
        }
      } else {
        fprintf(stderr, "%s: Invalid arguments\n", com[i].argv[0]);
        return -1;
      }
    } else if (strcmp(com[i].argv[0], "") == 0) {
      return 0;
    } else if (strcmp(com[i].argv[0], "exit") == 0) {
      return 1;
    } else {
	if(n_commands>1&&i==0){//IPC
		pid2=fork();
		if(!pid2){//child
			client_sock = socket(PF_FILE, SOCK_STREAM,0);
			if(client_sock == -1){
				fprintf(stderr,"client socket error\n");
				return -1;
			}
			memset(&server_addr,0,sizeof(server_addr));
			server_addr.sun_family = AF_UNIX;
			strcpy(server_addr.sun_path, FILE_SERVER);
	
			while(-1==connect(client_sock, (struct sockaddr*)&server_addr,sizeof(server_addr))){

				sleep(2);
			}
					
			temp_out = dup(1);
			dup2(client_sock,1);

			proc_create(com[i],i,&result);
			
			dup2(temp_out,1);
			exit(1);
		}
		else{//parent
			waitpid(pid2,&status,0);
		}
	}
	else{//one command
		proc_create(com[i],i,&result);
		if(i>0){
			dup2(temp_in,0);
		}
		if(result==-1)
			return -1;
	}


    }//else
  }//for
	if(n_commands>1){
		close(client_sock);
		pthread_join(server_thread,(void**)&status);
	}
}//if
  return 0;
}

void* bg_com()
{
	int status;
	waitpid(background.bg_pid,&status,0);
	background.bg_int=0;
	if(WIFEXITED(status)){
		printf("%d Done %s\n",background.bg_pid,background.bg_commands);
	}
	else if(WIFSIGNALED(status)){
		if(6==WTERMSIG(status)){
			printf("Exit %s\n",background.bg_commands);
		}
	}
	
	pthread_exit(0);
}

void* server()
{	
	int server_sock, client_sock;
	
	if(0==access(FILE_SERVER,F_OK))
		unlink(FILE_SERVER);

	server_sock = socket(PF_FILE, SOCK_STREAM, 0);
	struct sockaddr_un server_addr, client_addr;
	int client_addr_size=sizeof(client_addr);
	memset(&server_addr,0,sizeof(server_addr));
	server_addr.sun_family = AF_UNIX;
	strcpy(server_addr.sun_path, FILE_SERVER);

	if(-1== bind(server_sock,(struct sockaddr*)&server_addr,sizeof(server_addr))){
		fprintf(stderr,"server bind error\n");
	}
	
	while(-1==listen(server_sock,10)){
		fprintf(stderr,"server listen error\n");
	}

	client_sock = accept(server_sock,(struct sockaddr*)&client_addr,&client_addr_size);
	if(client_sock==-1){
		fprintf(stderr,"client accept error\n");
	}
	
	dup2(client_sock,0);
	
	close(server_sock);
	close(client_sock);
	pthread_exit(0);
}

void free_commands(int n_commands, struct single_command (*commands)[512])
{
  for (int i = 0; i < n_commands; ++i) {
    struct single_command *com = (*commands) + i;
    int argc = com->argc;
    char** argv = com->argv;

    for (int j = 0; j < argc; ++j) {
      free(argv[j]);
    }

    free(argv);
  }

  memset((*commands), 0, sizeof(struct single_command) * n_commands);
}
//return the pid of child process
pid_t proc_create(struct single_command exec_com,int index_command, int* result)
{
	pid_t pid1;
	pid1=fork();
	int status;
	if(pid1==0){//child
		execv(exec_com.argv[0],exec_com.argv);
		char* Env = getenv("PATH");
		char* saveptr=NULL;
		char exec_buf[4096];
		strcpy(exec_buf,Env);
		char* tok = strtok_r(exec_buf,":",&saveptr);
		
		while(tok != NULL){
			char* temp = (char*)malloc(strlen(tok));
			char* temp_m = (char*)malloc(strlen(exec_com.argv[0]));

			strcpy(temp_m,exec_com.argv[0]);
			strcpy(temp,tok);
			strcat(temp,"/");
			strcat(temp,exec_com.argv[0]);
			strcpy(exec_com.argv[0],temp);

			execv(exec_com.argv[0],exec_com.argv);

			strcpy(exec_com.argv[0],temp_m);
			
			tok = strtok_r(NULL,":",&saveptr);
		}
		fprintf(stderr,"%s : command not found\n",exec_com.argv[0]);
		*result = -1;
		abort();
//		exit(1);
	}
	
	else{//parent
		if(background.bg_int==1){//background
			background.bg_int++;
			printf("%u\n",pid1);
			background.bg_pid = pid1;
			pthread_attr_t attr;
			pthread_attr_init(&attr);
			pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_JOINABLE);
			pthread_create(&(background.bg_thread),NULL,bg_com,NULL);
		}
		else{
			waitpid(pid1,&status,0);
			return pid1;
		}
	}
}

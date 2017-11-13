#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/wait.h>
#include <unistd.h>

#include "commands.h"
#include "built_in.h"

pid_t proc_create(struct single_command,int index_of_commands,int* result);

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
  if (n_commands > 0) {
    struct single_command* com = (*commands);

    assert(com->argc != 0);
for(int i = 0;i<n_commands;i++){
    int built_in_pos = is_built_in_command(com[i].argv[0]);
    if (built_in_pos != -1) {
      if (built_in_commands[built_in_pos].command_validate(com[i].argc, com[i].argv)) {
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
	proc_create(com[i],i,&result);
	if(result==-1)
		return -1;
    }
  }//for
}//if
  return 0;
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
		exit(1);
	}
	
	else{//parent
		waitpid(pid1,&status,0);
		return pid1;
	}
}

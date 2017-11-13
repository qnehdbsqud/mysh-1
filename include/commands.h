#ifndef MYSH_COMMANDS_H_
#define MYSH_COMMANDS_H_
#include <sys/types.h>
#include <pthread.h>

struct single_command
{
  int argc;
  char** argv;
};

struct background_data{
	pid_t bg_pid;
	pthread_t bg_thread;
	char bg_commands[100];
	int bg_int;
} background;

int evaluate_command(int n_commands, struct single_command (*commands)[512]);

void free_commands(int n_commands, struct single_command (*commands)[512]);

#endif // MYSH_COMMANDS_H_

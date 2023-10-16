// SPDX-License-Identifier: BSD-3-Clause

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <fcntl.h>
#include <unistd.h>

#include "cmd.h"
#include "utils.h"

#define READ		0
#define WRITE		1

/**
 * Internal change-directory command.
 */
static bool shell_cd(word_t *dir)
{
	int r;
	char *home;

	if (dir != NULL) { // cd dir
		r = chdir(get_word(dir));
		if (r < 0)
			return EXIT_FAILURE;
	} else {
		home = getenv("HOME"); // get home directory
		if (home == NULL) // if home is not set
			return EXIT_FAILURE;
		r = chdir(home); // cd home
		if (r < 0)
			return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

/**
 * Internal exit/quit command.
 */
static int shell_exit(void)
{
	exit(EXIT_SUCCESS);
}

/**
 * Parse a simple command (internal, environment variable assignment,
 * external command).
 */
static int parse_simple(simple_command_t *s, int level, command_t *father)
{
	char *c = get_word(s->verb);
	int fd, r;

	if (strcmp(c, "true") == 0) // true returns success
		return EXIT_SUCCESS;
	if (strcmp(c, "false") == 0) // false returns failure
		return EXIT_FAILURE;

	if (strcmp(c, "cd") == 0) { // cd
		if (s->out != NULL) // create output redirection file
			close(open(get_word(s->out), O_WRONLY | O_CREAT | O_TRUNC, 0644));
		if (s->err != NULL) // create error redirection file
			close(open(get_word(s->err), O_WRONLY | O_CREAT | O_TRUNC, 0644));
		return shell_cd(s->params); // cd dir
	}

	if (strcmp(c, "exit") == 0 || strcmp(c, "quit") == 0) // exit/quit
		return shell_exit();

	// environment variable assignment
	if (s->verb != NULL &&
			s->verb->next_part != NULL &&
			s->verb->next_part->string != NULL &&
			strcmp(s->verb->next_part->string, "=") == 0) {
		const char *var = s->verb->string;
		char *value = get_word(s->verb->next_part->next_part);

		r = setenv(var, value, 1);
		if (r < 0)
			return EXIT_FAILURE;
		return EXIT_SUCCESS;
	}

	pid_t pid = fork(); // create child process

	if (pid == 0) { // child process
		int nr_args;
		char **argv = get_argv(s, &nr_args);

		if (s->in != NULL) { // input redirection
			fd = open(get_word(s->in), O_RDONLY);
			dup2(fd, STDIN_FILENO);
		}

		// output redirection
		if (s->out != NULL && s->err != NULL &&
				strcmp(get_word(s->out), get_word(s->err)) == 0) { // out == err
			fd = open(get_word(s->out), O_CREAT | O_TRUNC | O_WRONLY, 0666);
			dup2(fd, STDOUT_FILENO);
			dup2(fd, STDERR_FILENO);
		} else {
			int flags;

			if (s->out != NULL) { // out
				// set flags
				flags = O_CREAT | O_WRONLY;
				if (s->io_flags & IO_OUT_APPEND)
					flags |= O_APPEND;
				else
					flags |= O_TRUNC;

				fd = open(get_word(s->out), flags, 0666); // redirect output
				dup2(fd, STDOUT_FILENO);
			}

			if (s->err != NULL) { // err
				// set flags
				flags = O_CREAT | O_WRONLY;
				if (s->io_flags & IO_ERR_APPEND)
					flags |= O_APPEND;
				else if ((s->io_flags & IO_OUT_APPEND) == 0)
					flags |= O_TRUNC;

				fd = open(get_word(s->err), flags, 0666); // redirect error
				dup2(fd, STDERR_FILENO);
			}
		}

		r = execvp(argv[0], argv); // execute command
		if (r < 0) { // invalid command
			fprintf(stderr, "Execution failed for '%s'\n", argv[0]);
			exit(EXIT_FAILURE);
		}
	}

	waitpid(pid, &r, 0); // wait for child process to finish
	return WEXITSTATUS(r); // return exit status
}

/**
 * Process two commands in parallel, by creating two children.
 */
static bool run_in_parallel(command_t *cmd1, command_t *cmd2, int level,
		command_t *father)
{
	/* TODO: Execute cmd1 and cmd2 simultaneously. */
	int pid[2], r[2];

	pid[0] = fork(); // create first child process
	if (pid[0] == 0)
		exit(parse_command(cmd1, level, father));

	pid[1] = fork(); // create second child process
	if (pid[1] == 0)
		exit(parse_command(cmd2, level, father));

	// wait for both child processes to finish
	waitpid(pid[0], &r[0], 0);
	waitpid(pid[1], &r[1], 0);

	// return success if both child processes returned success
	if (WEXITSTATUS(r[0]) == EXIT_SUCCESS && WEXITSTATUS(r[1]) == EXIT_SUCCESS)
		return EXIT_SUCCESS;
	return EXIT_FAILURE;
}

/**
 * Run commands by creating an anonymous pipe (cmd1 | cmd2).
 */
static bool run_on_pipe(command_t *cmd1, command_t *cmd2, int level,
		command_t *father)
{
	int r, fd[2], pid[2];

	pipe(fd); // create pipe

	pid[0] = fork(); // create process left of pipe
	if (pid[0] == 0) {
		// redirect output to pipe
		close(fd[0]);
		dup2(fd[1], STDOUT_FILENO);
		r = parse_command(cmd1, level, father);
		close(fd[1]);
		exit(r);
	}

	pid[1] = fork(); // create process right of pipe
	if (pid[1] == 0) {
		// redirect input from pipe
		close(fd[1]);
		dup2(fd[0], STDIN_FILENO);
		r = parse_command(cmd2, level, father);
		close(fd[0]);
		exit(r);
	}

	// close pipe for parent process
	close(fd[0]);
	close(fd[1]);

	// wait for both child processes to finish
	waitpid(pid[0], &r, 0);
	waitpid(pid[1], &r, 0);

	return WEXITSTATUS(r);
}

/**
 * Parse and execute a command.
 */
int parse_command(command_t *c, int level, command_t *father)
{
	if (c->op == OP_NONE) // simple command
		return parse_simple(c->scmd, level, father);

	int r;

	switch (c->op) {
	case OP_SEQUENTIAL:
		parse_command(c->cmd1, level + 1, c);
		r = parse_command(c->cmd2, level + 1, c);
		break;

	case OP_PARALLEL:
		r = run_in_parallel(c->cmd1, c->cmd2, level + 1, c);
		break;

	case OP_CONDITIONAL_NZERO:
		r = parse_command(c->cmd1, level + 1, c);
		if (r == EXIT_FAILURE)
			r = parse_command(c->cmd2, level + 1, c);
		break;

	case OP_CONDITIONAL_ZERO:
		r = parse_command(c->cmd1, level + 1, c);
		if (r == EXIT_SUCCESS)
			r = parse_command(c->cmd2, level + 1, c);
		break;

	case OP_PIPE:
		r = run_on_pipe(c->cmd1, c->cmd2, level + 1, c);
		break;

	default:
		return SHELL_EXIT;
	}

	return r;
}

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/wait.h>

#include "tokenize.h"
#include "svec.h"

// method declarations
int do_pipe(svec* all);
int do_2pipes(svec* all);
int do_rredir(svec* all);
int do_lredir(svec* all);
void check_rv(int rv);
int do_semicolon(svec* all);
int do_and(svec* all);
int do_or(svec* all);
int do_bg(svec* all);
int do_cd(svec* all);
void split(svec* all, svec* left, svec* right, char* split);

// fork, execvp, wait, return status code 
int
run_program(svec* args)
{
	if (strcmp(args->data[0], "exit") == 0
	|| strcmp(args->data[0], "exit\n") == 0) {
		exit(0);
	}
	int cpid;
	if ((cpid = fork())) {
		int status;
		waitpid(cpid, &status, 0);
		return status;
	} else {
		execvp(args->data[0], args->data);
	}
}

// fork, execvp, wait, return status code
// - with the given file descriptors as input and output
int
run_program_wfds(svec* args, int in, int out)
{
	if (in != 0) {
		close(0); // close stdin
		dup(in);
	}
	if (out != 1) {
		close(1);
		dup(out);
	}
	if (strcmp(args->data[0], "exit") == 0
	|| strcmp(args->data[0], "exit\n") == 0) {
		exit(0);
	}
	int cpid;
	if ((cpid = fork())) {
		int status;
		waitpid(cpid, &status, 0);
		return status;
	} else {
		execvp(args->data[0], args->data);
	}
}


// executes the given command
int
execute(svec* arr)
{
	int hasPipe = 0; // 0 is false, 1 is true
	int hasLRedir = 0; // 0 is false, 1 is true
	int hasRRedir = 0; // 0 is false, 1 is true
	int hasSemicolon = 0; // 0 is false, 1 is true
	int hasAnd = 0;
	int hasOr = 0;
	int hasBg = 0;
	int hasCd = 0;
	
	for (int index = 0; index < arr->size; index += 1) {
		if (strcmp(arr->data[index], "|") == 0) {
			hasPipe += 1;
		} else if (strcmp(arr->data[index], "<") == 0) {
			hasLRedir = 1;
		} else if (strcmp(arr->data[index], ">") == 0) {
			hasRRedir = 1;
		} else if (strcmp(arr->data[index], ";") == 0) {
			hasSemicolon = 1;
		} else if (strcmp(arr->data[index], "&&") == 0) {
			hasAnd = 1;
		} else if (strcmp(arr->data[index], "||") == 0) {
			hasOr = 1;
		} else if (strcmp(arr->data[index], "&") == 0) {
			hasBg = 1;
		} else if (strcmp(arr->data[index], "cd") == 0) {
			hasCd = 1;
		}
	}
	
	if (hasPipe == 1) {
		do_pipe(arr);
	} else if (hasPipe > 1) {
		do_2pipes(arr);
	} else if (hasRRedir) {
		do_rredir(arr);
	} else if (hasLRedir) {
		do_lredir(arr);
	} else if (hasSemicolon) {
		do_semicolon(arr);
	} else if (hasAnd) {
		do_and(arr);
	} else if (hasOr) {
		do_or(arr);
	} else if (hasBg) {
		do_bg(arr);
	} else if (hasCd) {
		do_cd(arr);
	} else {
		run_program_wfds(arr, 0, 1);
	}
	return 0;
}
	
// check_rv: This code is from Nat's lecture notes: 08-processes/pipe/pipe1.c
void
check_rv(int rv)
{
	if (rv == -1) {
		perror("fail");
		exit(0);
	}
}

// splits all into left and right svecs at split
void split
(svec* all, svec* left, svec* right, char* split)
{
	int split_idx = 0;
	int idx = 0;
	while (split_idx == 0 && idx < all->size) {
		if (strcmp(all->data[idx], split) != 0) {
			svec_push_back(left, all->data[idx]);
			idx += 1;
		}
		else {
			split_idx = idx;
			idx += 1;
		}
	}
	while (idx < all->size) {
		svec_push_back(right, all->data[idx]);
		idx += 1;
	}
}

// performs the cd command that changes the current directory
int
do_cd(svec* all)
{
	char buffer[100];
	getcwd(buffer, 100);
	strcat(buffer, "/");
	strcat(buffer, all->data[1]);
	return chdir(buffer);
}

// performs the given command in the background
int
do_bg(svec* all)
{
	if (strcmp(all->data[0], "exit") == 0
	|| strcmp(all->data[0], "exit\n") == 0) {
		exit(0);
	}
	int cpid;
	if ((cpid = fork())) {
	} else {
		execvp(all->data[0], all->data);
	}
	return 0;
}

// performs the given && command
int
do_and(svec* all)
{
	char* a = "&&";
	svec* left = make_svec();
	svec* right = make_svec();
	split(all, left, right, a);
	int rv_l;
	rv_l = run_program_wfds(left, 0, 1);
	check_rv(rv_l);
	if (rv_l == 0) {
		run_program_wfds(right);
	}
	free_svec(left);
	free_svec(right);
	return 0;
}

// performs the given || command
int
do_or(svec* all)
{
	char* o = "||";
	svec* left = make_svec();
	svec* right = make_svec();
	split(all, left, right, o);
	int rv_l;
	rv_l = run_program(left);
	check_rv(rv_l);
	if (rv_l != 0) {
		run_program(right);
	}
	free_svec(left);
	free_svec(right);
	return 0;
}

// performs the given command with 2 |'s
int
do_2pipes(svec* all)
{
	char* p = "|";
	svec* left = make_svec();
	svec* right = make_svec();
	svec* rightRight = make_svec();
	svec* rightLeft = make_svec();
	split(all, left, right, p);
	split(right, rightLeft, rightRight, p);
	int rv, cpid, ccpid;
	// first pipe
	int pipe_fds[2];
	rv = pipe(pipe_fds);
	check_rv(rv);
	int p_read = pipe_fds[0];
	int p_write = pipe_fds[1];
	// second pipe
	int pipe2_fds[2];
	rv = pipe(pipe2_fds);
	check_rv(rv);
	int p2_read = pipe2_fds[0];
	int p2_write = pipe2_fds[1];
	
	if ((cpid = fork())) {
		check_rv(cpid);
		// parent
		int status1;
        waitpid(cpid, &status1, 0);
		close(p_write);
		if ((ccpid = fork())) {
			check_rv(ccpid);
			// parent
			int status;
			waitpid(cpid, &status, 0);
			close(p2_write);
			run_program_wfds(rightRight, p2_read, 1);
			close(p2_read);
		}
		else {
			// child
			close(p2_read);
			run_program_wfds(rightLeft, p_read, p2_write);
			close(p2_write);
			close(p_read);
		}
		run_program_wfds(right, p_read, 1);
		close(p_read);
	}
	else {
		// child
		close(p_read);
		run_program_wfds(left, 0, p_write);
		close(p_write);
	}
	free_svec(left);
	free_svec(right);
	free_svec(rightRight);
	free_svec(rightLeft);
	return 0;
}

// do_pipe: This code is from Nat's lecture notes: 08-processes/sort-pipe/sort-pipe.c
// 			and 09-fork-exec/io/cstd.c
// performs the given command with 1 |
int
do_pipe(svec* all)
{
	//printf("i am in do_pipe\n");
	char* p = "|";
	svec* left = make_svec();
	svec* right = make_svec();
	split(all, left, right, p);
	// make the pipes, set up variables
	int rv, cpid;
	int pipe_fds[2];
	rv = pipe(pipe_fds);
	check_rv(rv);
	int p_read = pipe_fds[0];
	int p_write = pipe_fds[1];
	
	if ((cpid = fork())) {
		check_rv(cpid);
		// parent
		int status1;
        waitpid(cpid, &status1, 0);
		close(p_write);
		run_program_wfds(right, p_read, 1);
		close(p_read);
	}
	else {
		// child
		close(p_read);
		run_program_wfds(left, 0, p_write);
		close(p_write);
	}
	free_svec(left);
	free_svec(right);
	return 0;
}

// performs the given redirect output command
int
do_rredir(svec* all)
{
	char* rdir = ">";
	svec* left = make_svec();
	svec* right = make_svec();
	split(all, left, right, rdir);
	
	char* output_file = right->data[0];
	// make the pipe
	int pipe_fds[2];
	int rv;
	rv = pipe(pipe_fds);
	check_rv(rv);
	int p_read = pipe_fds[0];
	int p_write = pipe_fds[1];
	
	int cpid;
	if ((cpid = fork())) {
		check_rv(cpid);
		// parent
		char temp[256];
		close(p_write);
		// read from the pipe
		int nn = read(p_read, temp, 255);
		temp[nn] = 0;
		// write the output of the left into file
		FILE* left_output = fopen(output_file, "w");
		fprintf(left_output, temp);
		fclose(left_output);
	}
	else {
		// child
		close(p_read);
		run_program_wfds(left, 0, p_write);
	}
	free_svec(left);
	free_svec(right);
	return 0;
}

// performs the given redirect input command
int
do_lredir(svec* all)
{
	char* ldir = "<";
	svec* left = make_svec();
	svec* right = make_svec();
	split(all, left, right, ldir);
	
	FILE* input_file = fopen(right->data[0], "r");
	run_program_wfds(left, fileno(input_file), 1);
	fclose(input_file);
	free_svec(right);
	free_svec(left);
	return 0;
}

// performs the given semicolon command
int
do_semicolon(svec* all)
{
	char* s = ";";
	svec* left = make_svec();
	svec* right = make_svec();
	split(all, left, right, s);
	execute(left);
	execute(right);
	free_svec(left);
	free_svec(right);
	return 0;
}


int
main(int argc, char* argv[])
{
	char cmd[256];
	char* ctrl_d = "";
	char* to_exit = "exit\n";
	FILE* script;
	svec* commands;
	if (argc > 1) {
		script = fopen(argv[1], "r");
		if (script == NULL) {
			exit(0);
		}
		ctrl_d = fgets(cmd, 256, script);
	}
	while (ctrl_d != NULL) {
		if (argc == 1) {
			printf("nush$ ");
			fflush(stdout);
			ctrl_d = fgets(cmd, 256, stdin);
			commands = tokenize(cmd);
			execute(commands);
			free_svec(commands);
		} else {
			// reading a script
			commands = tokenize(cmd);
			execute(commands);
			free_svec(commands);
			ctrl_d = fgets(cmd, 256, script);
		}
	}
	if (argc > 1) {
		fclose(script);
	}
    return 0;
}

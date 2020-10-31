//required imports
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <sys/wait.h>
#include <sys/resource.h>
//useful global definitions
#define inputBuffSize 1024
#define tokenBuffSize 64
#define tokenDelim " \t\n\a\r"

//initialize all functions to avoid errors
//main system function
int my_system(char* line);
//read/parse/execute input to program
char *get_a_line();
char **parse_input(char *input);
int execute_input(char **tokens);
//execute external command
int ext_cmd(char **tokens);
//functions for internal commands
int tshell_chdir(char **tokens);
int tshell_cd(char **tokens);
int tshell_history();
void tshell_limit(char **tokens);
int builtin_num();
//handle inputs that raise flags
void sig_handler(int sig);
void sig_ignore(int sig);
//fifo handler
int fifoFunc(char *tokens);
//contains all the internal functions
char *builtin_names[] = {"chdir", "cd", "history", "limit"};
int (*builtin_cmd[]) (char **) = {&tshell_chdir, &tshell_cd, &tshell_history, &tshell_limit};
//initialize useful global variables
char *input[100];
char *history[100];
char *fifopath;
int fifo = 0;
int currLine = 0;
int currCmd = 0;

//main function that calls system
void main(int argc, char* argv[]){
	//signal functions that will deal with ctrl + C/Z flags
	signal(SIGINT, sig_handler);
	signal(SIGTSTP, sig_ignore);
	//positioning variables for history and input
	currLine = 0;
	currCmd = 0;
	//if fifo
	if(argc > 1){
		fifopath = (char *) malloc(strlen(argv[1] + 1));
		strcpy(fifopath, argv[1]);
	}
	//call the my_system function if input is not NULL
	while (1){
		printf(">> ");
		input[currLine] = get_a_line();
		if(strlen(input[currLine]) > 1){
			my_system(input[currLine]);
			currLine++;
		}
		free(input[currLine]);
	}
}
//system function that proceeds to execute
int my_system(char* line){
	char **tokens;
	//first save history before executing cmds
	if (line != NULL){
		history[currCmd%100] = line;
		currCmd++;
		//if fifo go to func instead of regular parsing/executing
		if (fifo == 1){
			fifoFunc(line);
			fifo = 0;
		}
		//if not fifo, regular execution
		else{
			tokens = parse_input(line);
			execute_input(tokens);
		}
	}
	free(tokens);
}
//function that read input and returns it to be parsed
char *get_a_line(){
	int buffSize = inputBuffSize;
	char *input_buffer = malloc(sizeof(char)*buffSize);
	int ch;
	int p = 0;
	if (!input_buffer) {
		fprintf(stderr, "allocation error\n");
		exit(EXIT_FAILURE);
	}
	while (1){
		ch = getchar();
		//if end of text file terminate
		if (ch == EOF){
			exit(EXIT_FAILURE);
		}
		//this implies that fifo is detected
		if (ch == '|'){
    		fifo = 1;
		}
		//return the input to be parsed
		if (ch == '\n'){
			input_buffer[p] = '\0';
			return input_buffer;
		}
		//fill up the buffer
		else{
			input_buffer[p] = ch;
		}
		p++;
		//if we run out of buffer space, reallocate some memory
		if (p >= buffSize) {
			buffSize += inputBuffSize;
			input_buffer = realloc(input_buffer, buffSize);
			if (!input_buffer) {
				fprintf(stderr, "allocation error\n");
				exit(EXIT_FAILURE);
			}
		}
	}
}
//function that parses the input and prepares it for execution
char **parse_input(char *input){
	int buffSize = tokenBuffSize;
	char **tokens_buff = malloc(sizeof(char*)*buffSize);
	char *token;
	int p = 0;
	if (!tokens_buff) {
		fprintf(stderr, "allocation error\n");
		exit(EXIT_FAILURE);
	}
	//use tokenizer to parse the input
	token = strtok(input, tokenDelim);
	while (token != NULL) {
		tokens_buff[p] = token;
		p++;
		//if we run out of buffer space, reallocate some memory
		if (p >= buffSize) {
			buffSize += tokenBuffSize;
			tokens_buff = realloc(tokens_buff, sizeof(char*)*buffSize);
			if (!tokens_buff) {
				fprintf(stderr, "allocation error\n");
				exit(EXIT_FAILURE);
			}
		}
		token = strtok(NULL, tokenDelim);
	}
	tokens_buff[p] = NULL;
	//return the tokens for execution
	return tokens_buff;
}
//this function decided whether we will execute an internal or external cmd
int execute_input(char **tokens){
	//if empty return
	if (tokens[0] == NULL) {
		return 0;
	}
	int i;
	//if internal function call it from the array
	for (i = 0; i < builtin_num(); i++){
		if (strcmp(tokens[0], builtin_names[i]) == 0){
			return (*builtin_cmd[i])(tokens);
		}
	}
	//if not internal then execture external command
	return ext_cmd(tokens);
}
//this function is called by the previous one to execute external commands
int ext_cmd(char **tokens){
	//fork to create child process
	int wait;
	pid_t pid;
	pid = fork();
	//if pid child process then execute
	if (pid == 0){
		struct rlimit limit;
        getrlimit(RLIMIT_DATA, &limit);
        //set limit and check before executing
        if (errno == ENOMEM){
        	perror("error");
        }
		else if (execvp(tokens[0], tokens) == -1){
			perror("error");
		}
		exit(EXIT_FAILURE);
	}
	else if (pid < 0){
		perror("error");
	}
	else{
		do{//wait for child process to complete
			pid = waitpid(pid, &wait, WUNTRACED);
		}
		while (!WIFSIGNALED(wait) && !WIFEXITED(wait));
	}	
	return 1;
}
//return the length of the builtin functions array
int builtin_num(){
	return sizeof(builtin_names)/sizeof(char *);
}
//internal command for change dir
int tshell_chdir(char **tokens){
	if (tokens[1] == NULL){
		chdir(getenv("HOME"));
	}
	else{
		//system call
		if (chdir(tokens[1]) != 0){
			perror("error");
		}
	}
	return 1;
}
//replica of internal command above for cd
int tshell_cd(char **tokens){
	if (tokens[1] == NULL){
		chdir(getenv("HOME"));
	}
	else{
		//system call
		if (chdir(tokens[1]) != 0){
			perror("error");
		}
	}
	return 1;
}
//function that provides last 100 cmds excluding spaces
int tshell_history(){
	for(int i = 0; i <= 99; i++){
		if (history[i] != 0){
			//display the cmds
			printf("%d \t %s \n",i, history[i]);
		}
	}
	return 1;
}
//function that sets limit for allocation
void tshell_limit(char **tokens){
	char *p;
	struct rlimit prev;
	getrlimit(RLIMIT_DATA, &prev);
	struct rlimit next = {strtoimax(tokens[1], &p, 10), prev.rlim_max};
	if (setrlimit(RLIMIT_DATA, &next) == 0)
		printf("set the limit\n");
	else{
		fprintf(stderr, "error while setting\n");
	}
}
//function that handles when ctrl + C flag turns on
void sig_handler(int sig){
	printf("\nConfirm Termination? (y/n)\n");
	size_t buffSize = 16;
	char *reply_buffer = (char *)malloc(sizeof(char)*buffSize);
	int reply;
	while(1){
		reply = getline(&reply_buffer, &buffSize, stdin);
		//if reply is not NULL
		if(reply > 1){
			//if y then exit
			if(strcmp(reply_buffer, "y\n")==0){
				exit(EXIT_SUCCESS);
			}
			//if n then cancel
			else if(strcmp(reply_buffer, "n\n")==0){
				printf("Termination cancelled, press enter to resume\n");
				break;
			}
			//else ask for correct reply
			else{
				printf("Please reply with y or n\n");
			}
		}
	}
	fflush(stdout);
}
//function that handles when ctrl + Z flag turns on
void sig_ignore(int sig){
	//function prevents the flag from terminating shell and instead ignores it
	printf("\nIgnoring Ctrl + Z in this shell\n");
	printf("\nPress enter to resume\n");
	fflush(stdout);
}
//function that handles if we detect fifo
int fifoFunc(char *tokens){
	//prepare both cmds for the pipe
	int buffSize = tokenBuffSize;
	int fifo2 = 0;
	int cmd1_size = 0;
	int cmd2_size = 0;
	char **cmd1 = malloc(sizeof(char*)*buffSize);
	char **cmd2 = malloc(sizeof(char*)*buffSize);
	char *t = (char*) malloc(sizeof(char*)*buffSize);
	t = strtok(tokens, tokenDelim);
	while (t != NULL){
		//if fifo detected, sanity check
    	if(strcmp(t, "|") == 0){
    		fifo2 = 1;
    	}
    	//second cmd since we reached '|'
    	else if(fifo2 == 1){
    		cmd2[(cmd2_size)] = t;
    		cmd2_size++;
    	}
    	//first cmd since we did not reached '|'
    	else if(fifo2 == 0){
    		cmd1[cmd1_size] = t;
    		cmd1_size++;
    	}
    	t = strtok(NULL, tokenDelim);
    }
    //clear size of the cmds
	cmd1_size = 0;
	cmd2_size = 0;
	pid_t pid = fork();
	if (pid == 0){
		if (fifopath == "" &&  fifo == 1){
			perror("error");
			return 0;
		}
		//open first file des as write only
		int fileDes = open(fifopath , O_WRONLY);
		if (fileDes < 0){
  			perror("error");
  			return 0;
		}
		dup2(fileDes, STDOUT_FILENO);
		if(execvp(cmd1[0], cmd1) < 0){
			exit(EXIT_FAILURE);
		}
		else{ 
			exit(EXIT_SUCCESS);
		}
	}
	else{
		pid_t pid2 = fork();
		if(pid2 == 0){
			//close the file des and open the second one as read only
			close(STDIN_FILENO);
			int fileDes2 = open(fifopath, O_RDONLY);
			if (execvp(cmd2[0], cmd2) < 0){
				exit(EXIT_FAILURE);
			}
			else{
				exit (EXIT_SUCCESS);
			}
		}
		else{
			wait (NULL);
		}
	}
	return 1; 
}

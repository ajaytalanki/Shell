#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>

#define CMDLINE_MAX 512
#define MAX_PIPE_COMMANDS 4
#define DEFAULT_STATUS 0

struct command{
  char* cmd;
  char* argv[CMDLINE_MAX];
  char* filename;
  int numArgs;
  int outputRedirection;
  int appendingRedirection;
};

/* trims leading and trailing whitespace from string */
void trim(char* str){
  int index = 0;
  int pos = 0;
  while(str[index] == ' ' || str[index] == '\n' || str[index] == '\t'){
    index++;
  }
  while(str[pos + index] != '\0'){
    str[pos] = str[pos + index];
    pos++;
  }
  str[pos] = '\0';
  pos = 0;
  index = -1;
  while(str[pos] != '\0'){
    if(str[pos] != ' ' && str[pos] != '\t' && str[pos] != '\n'){
      index = pos;
    }
    pos++;
  }
  str[index + 1] = '\0';
}

/* if cmd has >, outputRedirection is turned on */
int checkOutputRedirection(const char* ptr){
  for(int i = 0; i < (int)strlen(ptr); i++){
    if(ptr[i] == '>'){
      return 1;
    }
  }
  return 0;
}

/* if cmd has >>, appendingOutputRedirection is turned on */
int checkAppendingRedirection(const char* ptr){
  for(int i = 0; i < ((int)strlen(ptr)) - 1; i++){
    if(ptr[i] == '>' && ptr[i+1] == '>'){
      return 1;
    }
  }
  return 0;
}

void parseCommand(struct command* cmdObj,  char* cmd){
  char* delim = " ";
  int counter = 0;
  char* firsttoken = (char*)malloc(strlen(cmd));
  strcpy(firsttoken, cmd);
  cmdObj->outputRedirection = 0;
  cmdObj->appendingRedirection = 0;
  cmdObj->filename = "";

  /* if output redirection, set redirection flags on and grab filename */
  if(checkOutputRedirection(cmd)){
    cmdObj->outputRedirection = 1;
    cmdObj->appendingRedirection = checkAppendingRedirection(cmd);
    char* delim2 = ">";
    firsttoken = strtok(cmd, delim2);
    cmdObj->filename = strtok(NULL, delim2);
    if(cmdObj->filename != NULL){
      trim(cmdObj->filename);
    }
  }

  /* populates cmdObj.argv array for execvp */
  char* ptr = strtok(firsttoken, delim);
  while(ptr != NULL){
    cmdObj->argv[counter] = ptr;
    counter++;
    ptr = strtok(NULL, delim);
  }
  cmdObj->numArgs = counter;
  cmdObj->cmd = cmdObj->argv[0];
  cmdObj->argv[counter] = NULL;
  /* no need to free firsttoken because strtok deallocates it for us */
}

/*counts number of pipes in the cmd input */
int countPipes(char* cmd){
  int counter = 0;
  for(int i =0; i < (int)strlen(cmd); i++){
    if(cmd[i] == '|'){
      counter++;
    }
  }
  return counter;
}

/* iterates through pipes and populates cmdObjects array with parsed commands */
int parsePipe(struct command* cmdObjects[], char* cmd){
  char* delim = "|";
  char* token = strtok_r(cmd, delim, &cmd);
  int i = 0;
  /*parses each command in the pipe and creates cmdObjects to put into array */
  while( token != NULL ) {
    struct command* cmdObj = (struct command*)malloc(sizeof(struct command));
    parseCommand(cmdObj, token);
    cmdObjects[i] = cmdObj;
    token = strtok_r(NULL, delim, &cmd);
    i++;
  }
  return i;
}

/* returns 1 if c is first non whitespace character in the string */
int isFirstCharacter(const char* str, char c){
  int i = 0;
  while(isspace(str[i])){
    i++;
  }
  if(str[i] == c){
    return 1;
  }
  return 0;
}

/* returns 1 if error, 0 otherwise */
int errorCheckCommand(struct command cmdObj, char* cmd){
  /* missing command before output redirection */
  if(checkOutputRedirection(cmd) == 1){
    if(isFirstCharacter(cmd, '>') == 1){
      fprintf(stderr, "Error: missing command\n");
      return 1;
    }
  }
  if(cmdObj.numArgs >= 17){
    fprintf(stderr, "Error: too many process arguments\n");
    return 1;
  }
  /* if there is output redirection and the parser doesn't detect a file */
  if((cmdObj.filename == NULL || !strcmp(cmdObj.filename, "")) &&
    cmdObj.outputRedirection == 1){
    fprintf(stderr, "Error: no output file\n");
    return 1;
  }
  /* if there's a '/' in the filename, file cannot be opened */
  if(cmdObj.outputRedirection == 1){
    for(int i = 0; i < (int)strlen(cmdObj.filename); i++){
      if(cmdObj.filename[i] == '/'){
        fprintf(stderr, "Error: cannot open output file\n");
        return 1;
      }
    }
  }
  return 0;
}

/*returns 1 if there is an error when parsing pipe commands, else return 0 */
int errorCheckPipes(struct command* cmdObjects[], char* cmd, int numCommands){
  /* output redirection is not last command of pipe */
  for(int i = 0; i < numCommands; i++){
    if(cmdObjects[i]->outputRedirection == 1 && i != numCommands - 1){
      fprintf(stderr, "Error: mislocated output redirection\n");
      return 1;
    }
  }
  /* checks for errors for each command in the pipes */
  for(int i = 0; i < numCommands; i++){
    if(errorCheckCommand(*(cmdObjects[i]), cmd) == 1){
      return 1;
    }
  }
  /* strips whitespace from cmd string */
  char strippedCmd[strlen(cmd)];
  int j = 0;
  for(int i = 0; i < (int)strlen(cmd); i++){
    if(!isspace(cmd[i])){
      strippedCmd[j] = cmd[i];
      j++;
    }
  }
  /*if strippedCmd has any missing commands between pipes*/
  for(int i = 0; i < j; i++){
    if(strippedCmd[i] == '|'){
      if(i == 0 || i == j - 1 || strippedCmd[i+1] == '|') {
        fprintf(stderr, "Error: missing command\n");
        return 1;
      }
    }
  }
  return 0;
}

void freeCmdObjects(struct command* cmdObjects[], int numCommands){
  for(int i = 0; i < numCommands; i++){
    free(cmdObjects[i]);
  }
}

void commandNotFound(){
  /*execvp didn't recognize the command */
  if(errno == ENOENT){
    fprintf(stderr, "Error: command not found\n");
  }
}

int executePipeCommand(int inputRead, int fd[2], struct command*
cmdObjects[], int i){
  pid_t pid = fork();
  if(pid == 0){
    if(inputRead != 0){
      dup2(inputRead, STDIN_FILENO);
      close(inputRead);
    }
    if(fd[1] != 0){
      dup2(fd[1], 1);
      close(fd[1]);
    }
    execvp(cmdObjects[i]->argv[0], cmdObjects[i]->argv);
    exit(1);
  }
  int status;
  waitpid(pid, &status, 0);
  return WEXITSTATUS(status);
}

int main(void){
  char cmd[CMDLINE_MAX];
  char originalCmd[CMDLINE_MAX];
  while (1) {
    char *nl;

    /* Print prompt */
    printf("sshell$ ");
    fflush(stdout);

    /* Get command line */
    fgets(cmd, CMDLINE_MAX, stdin);

    /* Print command line if stdin is not provided by terminal */
    if (!isatty(STDIN_FILENO)) {
      printf("%s", cmd);
      fflush(stdout);
    }

    /* Remove trailing newline from command line */
    nl = strchr(cmd, '\n');
    if (nl)
      *nl = '\0';

    struct command cmdObj;
    strcpy(originalCmd, cmd);
    int numPipes = countPipes(cmd);

    /* if there's no piping run regular parseCommand and errorCheckCommand */
    if(numPipes == 0){
      parseCommand(&cmdObj, cmd);
      /* if there is an error, prompt again by continuing to next iteration */
      if(errorCheckCommand(cmdObj, originalCmd) == 1){
        continue;
      }
    } else { /* there is piping, so run parsePipe and errorCheckPipe */
      struct command* cmdObjects[MAX_PIPE_COMMANDS];
      int numCommands = parsePipe(cmdObjects, cmd);
      if(errorCheckPipes(cmdObjects, originalCmd, numCommands) == 1){
        freeCmdObjects(cmdObjects, numCommands);
        continue;
      }
      int i;
      int fd[2];
      int inputRead = 0;
      // pid_t pid;
      int exitCodes[3] = {0,0,0};
      for(i = 0; i < numPipes; ++i){
        pipe(fd);
        exitCodes[i] = executePipeCommand(inputRead, fd, cmdObjects, i);
        close(fd[1]);
        inputRead = fd[0];
      }
      pid_t pid2 = fork();
      if(pid2 == 0){
        int fd1 = 0;
        if(cmdObjects[i]->outputRedirection == 1){
          if(cmdObjects[i]->appendingRedirection == 1){
            fd1 = open(cmdObjects[i]->filename, O_RDWR|O_CREAT|O_APPEND, 0644);
          } else {
            fd1 = open(cmdObjects[i]->filename, O_RDWR|O_CREAT|O_TRUNC, 0644);
          }
          dup2(fd1, STDOUT_FILENO);
          close(fd1);
        }
        dup2(inputRead, STDIN_FILENO);
        close(inputRead);
        execvp(cmdObjects[i]->argv[0], cmdObjects[i]->argv);
        commandNotFound(); // checks if execvp couldn't recognize command
        exit(1);
      } else{
        int status;
        waitpid(pid2 , &status, 0);
        close(inputRead);
        fprintf(stderr, "+ completed '%s' ", originalCmd);
        int j = 0;
        while(j < i){
          fprintf(stderr, "[%d]", exitCodes[j]);
          j++;
        }
        fprintf(stderr, "[%d]\n", WEXITSTATUS(status));
      }
      freeCmdObjects(cmdObjects, numCommands);
      continue;
    }

    /* Builtin commands */
    if (!strcmp(cmdObj.cmd, "exit")) {
      fprintf(stderr, "Bye...\n");
      fprintf(stderr, "+ completed '%s' [%d]\n", originalCmd, DEFAULT_STATUS);
      break;
    }

    if(!strcmp(cmdObj.cmd, "pwd")){
      if(getcwd(cmd, sizeof(cmd)) != NULL) {
        fprintf(stdout, "%s\n", cmd);
        fprintf(stderr, "+ completed '%s' [%d]\n", originalCmd, DEFAULT_STATUS);
        continue;
      }
    }

    if(!strcmp(cmdObj.cmd, "cd")) {
      /* if chdir == 0, cd is successful, else prompt error message */
      if(chdir(cmdObj.argv[1]) == 0) {
        fprintf(stderr, "+ completed '%s' [%d]\n", originalCmd, DEFAULT_STATUS);
      } else {
        fprintf(stderr, "Error: cannot cd into directory\n+ completed '%s' [%d]"
                        "\n", originalCmd, 1);
      }
      continue;
    }

    /* Regular commands */
    int fd;
    pid_t pid = fork();
    if(pid == 0){
      if(cmdObj.outputRedirection == 1){
        /* if appending redirection, append to file, else truncate file */
        if(cmdObj.appendingRedirection == 1){
          fd = open(cmdObj.filename, O_RDWR | O_CREAT | O_APPEND, 0644);
        } else {
          fd = open(cmdObj.filename, O_RDWR | O_CREAT | O_TRUNC, 0644);
        }
        /* redirect STDOUT to fd */
        dup2(fd, STDOUT_FILENO);
        close(fd);
      }
      execvp(cmdObj.argv[0],cmdObj.argv);
      commandNotFound(); // checks if execvp couldn't recognize command
      exit(1);
    } else if(pid > 0){
      /* if parent runs first, wait for child and obtain exit status */
      int status;
      waitpid(pid, &status, 0);
      fprintf(stderr,"+ completed '%s' [%d]\n",originalCmd,WEXITSTATUS(status));
    }
  }
  return EXIT_SUCCESS;
}
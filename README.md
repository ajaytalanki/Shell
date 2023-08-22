# SShell
## Summary

### No piping
First, sshell checks the command input for piping. If there are no pipes, there
is only one command object. A command object has the command, redirection
flags, number of arguments in the command, and an argv array. The command
input is passed into the *parseCommand* function which parses the command
input and sets the command object's data members. See *parseCommand*
implementation for reference. After parsing, the program runs error
checking for simple commands with no piping. See *errorCheckCommands* for
reference. If any error is found, the error is reported and starts a new prompt.

### Piping
If there is piping, an array of command objects is used to store the
commands for each pipe. The array is populated using the parsePipes function
described down below. The program then runs the *errorCheckPipes* function,
which is defined below. The piping logic is described in phase 5
implementation for reference.

Next, the shell checks to see if the commands were any of the builtin command
options. See phase 3 implementation for reference. If the input is not a
builtin command, then the program forks. Then the child checks for output
redirection. Reference phase 4 implementation.

## Error Checking implementation:
### *errorCheckCommands* implementation:
The logic for *errorCheckCommands* is as follows: If output redirection is
turned on, the program checks if there is a command
before the '>' symbol. If there is no command before the '>' symbol, a
"missing command error" is reported. The shell also checks if there is a 
file after the '>'. A "no output file" error is reported if none is found. The
sshell also checks that there are at max 16 process arguments per command,
by checking each *cmdObjects* numArgs member. Lastly, if output redirection
is detected, sshell checks to see if the file has '/'. If '/' is present in
the file sshell reports that it cannot open a file.

### *errorCheckPipes* implementation:
The logic for *errorCheckPipes* is as follows: If there is an output
redirection that is before the last command a "mislocated output
redirection" error is reported. Then the function iterates through the
command objects array and uses the *errorCheckCommand* function for each
command. Any error is reported. Lastly, to deal with missing commands for
pipes, we stripped the command of whitespace. If there are two pipe signs in
a row or if the pipe sign is at the beginning or ending character, then the
shell throws an error.

## Parsing implementation:
### *parseCommand* implementation:
This function accepts two parameters: the original cmd ('cmd') and struct 
command pointer ('cmdObj'). First the function checks if there is any output 
redirection symbol in *cmd*. If there is then it tokenizes, with the use of strtok, 
into *firstoken* and *filename*. *firstoken* contains the command that is then
tokenized by white space to extract the command and the arguments. All which
is initialized to members inside cmdObj. If there is no output redirection. then
*firstoken* which contains a copy of *cmd*, parses the command with white space.
The last element in the argv array is then set to null for the execvp call.

### *parsePipes* implementation:
This function accepts an array of struct command pointer objects ('cmdObjects')
and the original command ('cmd'). The function parses the array with the
piping symbol as its delimiter. Each token is extracted using strtok_r and
passed to parseCommand which parses each command individually and assigns it
to the *cmdObjects* array.

## Built-in Commands
In the case that the user inputs one of the three build in commands "pwd", "cd",
and "exit" the code manually executes these commands. If the input is pwd
then the code uses getcwd() function to get the current directory. If the input
is cd then the chdir() function is used to  change directory. Exit is implemented
by breaking the while loop.

## Output Redirection
The *parseCommand* method checks for the occurrence of any output redirection
sign. Then the function tokenizes into parts: command before the sign and
the filename after. The command before the sign (after being parsed like a
regular command) and the filename are initialized in the struct command
object. In the main function, upon execution of a command, we check if the
struct commands *isOutputRedirection* member is 1. If it is, then we proceed to
follow the output redirection logic. If output redirection or appending output 
redirection is turned on, we used open on the *cmdObject.filename* and stored 
the fd. Dup2 was used to change the STDOUT to fd and then closed fd. 

### Appending Output Redirection
A new member was added to the original command struct that checked if
appending redirection is true or not. In the execution in the main, if 
appending is true then the macros for opening the file are changed to append 
rather than truncate.

## Piping
### Generic:
To execute piping, we loop the number of pipes in the command and
in exception of the last command each command individually with the
help of the *executePipeCommand* function. For each iteration of the pipe we
first create the pipe by calling the pipe function. Then the
*executePipeCommand* accepts four parameters. The *inputRead* parameter
specifies which read pipe the stdin should point for each command. The parameter
'i' is used to specify the ith command in the *cmdObjects* array.

### Inside the executePipeCommand function
First this function creates a parent and child process. The child process
uses dup2 to change STDIN and STDOUT to point to the right directory. If
inputRead is zero (the first command), then there's no need to change the fd to 
zero. However, if it is anything else (not the first command) then the stdin 
needs to point to the read of the previous pipe. Similar is true for the fd[1] 
which should write to the next pipe. The functions returns exit status.

### Last command:
The last command is executed in a similar way that a command is executed
with no piping. After this execution, the program continues to the front of
the while loop.

## Limitations:
### Background Jobs:
The biggest limitation is that the program implements no background jobs. An
attempt was made to implement background jobs however the trials failed to
print out the completion messages in order. Attempts were made to use
waitpid with WNOHANG and various structs but none passed the cases.

## Sources

https://www.educative.io/answers/splitting-a-string-using-strtok-in-c

https://vineetreddy.wordpress.com/2017/05/17/pwd-vs-getcwd/

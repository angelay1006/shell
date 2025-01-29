```
 ____  _          _ _   ____
/ ___|| |__   ___| | | |___ \
\___ \| '_ \ / _ \ | |   __) |
 ___) | | | |  __/ | |  / __/
|____/|_| |_|\___|_|_| |_____|
```
# Program Structure
- **Overview**: This custom shell processes commands from the user, handles input/output
redirection, manages job control, and has basic shell functionality. Shell 2 extends
the code from [Shell 1](https://github.com/cs0330-fall2024/shell-1-angelay1006) and adds support for job management, both foreground and background.  
- **Flow**
  - First, the shell's job list and process group ID are initialized. 
  - We then enter a REPL loop where the prompt can be optionally displayed. The loop
    reads and processes user input where the command, arguments, and redirection are set up. 
    - `&` is used to execute jobs in the background
    - `fg` command is used to bring a background job to the foreground
    - `bg` command can continue a stopped job in the background
  - If the command is built-in (ie. `cd`, `ln`, `rm`, `exit`, `jobs`, `fg`, `bg`),
    it's executed directly without creating a new process. 
  - For external commands, a child process is created, input/output redirection are 
    handled, and job control is managed for foreground/background execution
  - If a background job is completed, it will be reaped before the next prompt. 
- **Error Handling**: There are many checks on user input, system calls, and operations on 
files. Informative error messages will be printed for failure in these cases. 

# Bugs
- There aren't any known bugs in this implementation
# Extra Features
- None have been added so far beyond command execution and job control
# How to Compile
- In this repository there is a `Makefile` provided — running `make` will compile 
  both versions, `33sh` (shell with prompt) and `33noprompt` (shell without prompt)
- To compile only the version with the prompt run `make 33sh`
- To compile only the version without the prompt run `make 33noprompt`
- To clean up build files run `make clean`
- To run with prompt, run `./33sh`, and to run without, `./33noprompt`
```
 ____  _          _ _  
/ ___|| |__   ___| | |
\___ \| '_ \ / _ \ | |  
 ___) | | | |  __/ | |  
|____/|_| |_|\___|_|_| 
```
## Overview
I wrote this custom shell for Brown University's _CS1330: Introduction to Computer Systems_; this version has been cleaned up and modified to run on any local machine, without the need for a course container. This project implements job control functionality and supports background job execution, foreground and background job management, signal forwarding, and job reaping.

From this project I reinforced my understanding of process control (using `fork()`, `execv()`, `waitpid()`). I also learned how to manage signals like `SIGINT`, `SIGTSTP`, `SIGCONT`, and `SIGCHLD`, as well as handling background, foreground and suspended jobs properly. Other than that, I learned how to forward signals and manage process groups, and use `open()`, `close()`, `dup2()`, and error checking for file I/O and redirection. 

**Note**: The files `jobs.c` and `jobs.h` were provided by _CS1330_ and contain support code for tracking jobs; my implementation focuses on handling job control, user signal management and handling processes. 

## Features
- Runs external commands, using `fork()` and `execv()`
- Built-in commands: Implements `cd`, `ln`, `rm`, `exit`
- Input and Output Redirection: Supports `<`, `>`, and `>>` for file I/O
- Error handling: This shell will gracefully handle malformed input and syscall errors
- Foreground and Background Jobs: Users can run commands in the background using &.
- Job Management: The shell tracks jobs and allows users to list them using the `jobs` command.
- Signal Handling: There is proper handling of `SIGINT`, `SIGTSTP`, and `SIGCONT`.
- Reaping Background Jobs: Ensures that terminated jobs are correctly removed from the job list.
- Built-in commands:
  
    | Command | Description |
    | --- | --- |
    | `cd <directory>` | Change the working directory. |
    | `ln <target> <link>` | Create a hard link |
    | `rm <file>`| Removes a file |
    | `jobs`| Lists all background and stopped jobs |
    | `fg %<jid>`| Resume a background job in the foreground |
    | `bg %<jid>` | Resume a stopped job in the background |
    | `exit` | Clean up and exit the shell |

## Code Structure
- `sh.c` is the core shell implementation (user input handling, job control, execution, signal handling)
- `jobs.c` is course-provided support code for tracking jobs
- `jobs.h` is the header file for job related functions, also provided by Brown University
-  Compilation instructions are included in the `Makefile` which I wrote

## Installation & Compilation
The prerequisites are that you have a Linux/macOS environment with `gcc`, and some basic familiarity with terminal commands. For compilation, clone the repository and compile using `make` which will generate an executable named `myshell`. Then, you can start the shell by running `./myshell`. 

## References
- Brown University's [CS1330](https://cs0330-fall2024.github.io/)


   
   
    

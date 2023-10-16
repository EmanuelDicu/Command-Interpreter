# Command-Interpreter

## Overview
This command interpreter is a simple Unix-like shell implementation written in C. It provides basic shell functionalities such as executing commands, handling simple internal commands like "cd" and environment variable assignments, and supporting sequential, parallel, and conditional execution of commands.

## Project Structure
The project is organized into the following files:

### `cmd.c` - Core Command Execution Logic

The `cmd.c` file is a critical component of the project, responsible for the core logic of parsing and executing shell commands. It provides the essential functionalities required for interacting with the shell.

#### Includes and Defines

- `cmd.c` includes standard C libraries like `stdio.h`, `stdlib.h`, and `string.h` for handling I/O, memory management, and string operations.
- System-specific headers such as `sys/types.h`, `sys/stat.h`, and `sys/wait.h` are included for system calls and process management.
- Constants like `READ` and `WRITE` are defined to represent file descriptor values for reading and writing.

#### Internal Functions

- `shell_cd`: Handles changing the current working directory (cd command).
- `shell_exit`: Manages the process of exiting the Mini-Shell (exit/quit command).
- `parse_simple`: Parses and executes simple commands, which can be internal (e.g., `cd`) or external programs.
- `run_in_parallel`: Executes two commands in parallel by creating separate child processes for each.
- `run_on_pipe`: Manages commands involving pipes (cmd1 | cmd2) by creating a pipe between two child processes.
- `parse_command`: The central function responsible for parsing and executing commands, including sequential, parallel, conditional, and piped commands.

#### External Command Execution

- The file utilizes the `get_word` and `get_argv` functions defined in `utils.c` to handle word concatenation and argument list creation for external command execution.

#### Conditional Execution Logic

- The `parse_command` function performs conditional execution logic, branching based on the type of command (e.g., sequential, parallel, conditional, or piped commands).

#### File I/O and Process Management

- `cmd.c` contains code for file input/output and process management, creating child processes, setting up input and output redirection, and executing external commands using system calls like `fork()`, `dup2()`, and `execvp()`.

#### Error Handling

- The code in `cmd.c` includes error handling, such as checking return values from system calls and reporting execution failures.

In summary, `cmd.c` is a crucial file within the Mini-Shell project, responsible for interpreting and executing various types of shell commands. It plays a pivotal role in providing core shell functionality, making it an integral part of the Unix-like shell implementation.

### `utils.c` - Utility Functions

The `utils.c` file contains utility functions used by the interpreter. It provides helper functions for string operations, memory management, and word concatenation.

#### Internal Functions

- `get_word`: Concatenates characters to form a word, handling whitespace and special characters.
- `get_argv`: Creates an argument list from a word list.
- `free_argv`: Frees the memory allocated for an argument list.

### `main.c` - Main Function

The `main.c` file contains the main function for the shell. It is responsible for initializing the shell and calling the `parse_command` function defined in `cmd.c` to parse and execute commands.

## How to Build
The project includes a `Makefile` for building the project. To build the project, follow these steps:

## Supported Commands
The interpreter supports the following commands:

- Basic shell commands, e.g., running external programs.
- Internal commands: cd, exit, quit, and environment variable assignments.
- Sequential execution using semicolons (;).
- Parallel execution using ampersands (&).
- Conditional execution based on the exit status of commands (&& and ||).
- Piping commands using the pipe symbol (|).

License
This project is licensed under the BSD 3-Clause License.

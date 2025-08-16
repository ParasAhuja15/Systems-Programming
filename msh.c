//  MSH main file
// Write your msh source code here


//#include "parser.h"
#include <stddef.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>


#define MAX_COMMANDS 8
#define MAX_LINE_LEN 256
#define MAX_ARGS 8



// files in case of redirection
char filev[3][64];


//to store the execvp second parameter
static char *argv_execvp[MAX_ARGS + 1];  // +1 for NULL terminator
void siginthandler(int param) {
    const char msg[] = "****  Exiting MSH **** \n";
    write(STDERR_FILENO, msg, sizeof(msg) - 1); // OK in a handler
    _exit(0);
}



// Add this handler function near the top of your file
void sigchld_handler(int param) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}


/**
 * Get the command with its parameters for execvp
 * Execute this instruction before run an execvp to obtain the complete command
 * @param argvv
 * @param num_command
 * @return
 */
void getCompleteCommand(char*** argvv, int num_command) {
    for (int j = 0; j < MAX_ARGS + 1; j++) argv_execvp[j] = NULL; // reset all

    int i = 0;
    for (; i < MAX_ARGS && argvv[num_command][i] != NULL; i++)
        argv_execvp[i] = argvv[num_command][i];

    argv_execvp[i] = NULL; // ensure terminator
}



/**
 * Parses a command line into argvv, filev, and in_background.
 * This is a simple parser for demonstration.
 */
int read_command(char ****argvv, char filev[3][64], int *in_background) {
    static char line[MAX_LINE_LEN];
    char *token, *saveptr1, *saveptr2;
    int command_counter = 0;


    // Reset filev and in_background
    strcpy(filev[0], "0");
    strcpy(filev[1], "0");
    strcpy(filev[2], "0");
    *in_background = 0;


    // Read line from stdin
    if (!fgets(line, sizeof(line), stdin)) return 0;
    line[strcspn(line, "\n")] = 0; // Remove newline


    // Count number of commands (split by '|')
    char *line_copy = strdup(line);
    token = strtok_r(line_copy, "|", &saveptr1);
    while (token) {
        command_counter++;
        token = strtok_r(NULL, "|", &saveptr1);
    }
    free(line_copy);


    if (command_counter == 0) return 0;


    // Allocate argvv
    *argvv = malloc(sizeof(char**) * command_counter);
    if (!*argvv) {
        perror("malloc failed");
        exit(1);
    }


    // Parse each command
    int cmd_idx = 0;
    token = strtok_r(line, "|", &saveptr1);
    while (token && cmd_idx < command_counter) {
        (*argvv)[cmd_idx] = malloc(sizeof(char*) * (MAX_ARGS + 1));
        if (!(*argvv)[cmd_idx]) {
            perror("malloc failed");
            exit(1);
        }
        int argc = 0;
        char *arg = strtok_r(token, " ", &saveptr2);
        while (arg && argc < MAX_ARGS) {
            if (strcmp(arg, "<") == 0) {
                arg = strtok_r(NULL, " ", &saveptr2);
                if (arg) strcpy(filev[0], arg);
            } else if (strcmp(arg, ">") == 0) {
                arg = strtok_r(NULL, " ", &saveptr2);
                if (arg) strcpy(filev[1], arg);
            } else if (strcmp(arg, "2>") == 0) {
                arg = strtok_r(NULL, " ", &saveptr2);
                if (arg) strcpy(filev[2], arg);
            } else if (strcmp(arg, "&") == 0) {
                *in_background = 1;
            } else {
                (*argvv)[cmd_idx][argc] = strdup(arg);
                if ((*argvv)[cmd_idx][argc] == NULL) {
                    perror("strdup failed");
                    exit(1);
                }
                argc++;
            }
            arg = strtok_r(NULL, " ", &saveptr2);
        }
        (*argvv)[cmd_idx][argc] = NULL;
        cmd_idx++;
        token = strtok_r(NULL, "|", &saveptr1);
    }
    return command_counter;
}


/**
 * Correction mode: parses a given command line string.
 */
int read_command_correction(char ****argvv, char filev[3][64], int *in_background, char *cmd_line) {
    char *token;
    int argc = 0;


    // Reset filev and in_background
    strcpy(filev[0], "0");
    strcpy(filev[1], "0");
    strcpy(filev[2], "0");
    *in_background = 0;


    // Allocate argvv
    *argvv = malloc(sizeof(char**) * 1);
    if (!*argvv) {
        perror("malloc failed");
        exit(1);
    }
    (*argvv)[0] = malloc(sizeof(char*) * 8);
    if (!(*argvv)[0]) {
        perror("malloc failed");
        exit(1);
    }


    token = strtok(cmd_line, " ");
    while (token && argc < 7) {
        if (strcmp(token, "<") == 0) {
            token = strtok(NULL, " ");
            if (token) strcpy(filev[0], token);
        } else if (strcmp(token, ">") == 0) {
            token = strtok(NULL, " ");
            if (token) strcpy(filev[1], token);
        } else if (strcmp(token, "2>") == 0) {
            token = strtok(NULL, " ");
            if (token) strcpy(filev[2], token);
        } else if (strcmp(token, "&") == 0) {
            *in_background = 1;
        } else {
            (*argvv)[0][argc++] = strdup(token);
        }
        token = strtok(NULL, " ");
    }
    (*argvv)[0][argc] = NULL;
    return 1; // One command parsed
}


/**
 * Main sheell  Loop  
 */
int main(int argc, char* argv[])
{
    signal(SIGINT, siginthandler); // Install once
    signal(SIGCHLD, sigchld_handler);
    //This variable is created for the function mycalc. It should be declared here to avoid the reset of the value
    int env_sum = 0;
    
    /**** Do not delete this code.****/
    int end = 0; 
    int executed_cmd_lines = -1;
    char *cmd_line = NULL;
    char *cmd_lines[10];


    if (!isatty(STDIN_FILENO)) {
        cmd_line = (char*)malloc(100);
        while (scanf(" %[^\n]", cmd_line) != EOF){
            if(strlen(cmd_line) <= 0) return 0;
            cmd_lines[end] = (char*)malloc(strlen(cmd_line)+1);
            strcpy(cmd_lines[end], cmd_line);
            end++;
            fflush(stdout);
        }
}


    /*********************************/


    char ***argvv = NULL;
    int num_commands = 0; // Initialize to 0



    while (1){
        // Free argvv from previous iteration
        if (argvv) {
            for (int i = 0; i < num_commands; ++i) {
                if (argvv[i]) {
                    for (int j = 0; argvv[i][j] != NULL; ++j) {
                        free(argvv[i][j]);
                    }
                    free(argvv[i]);
                }
            }
            free(argvv);
            argvv = NULL;
        }


        int status = 0;
        int command_counter = 0;
        int in_background = 0;
        //signal(SIGINT, siginthandler);


        // Prompt 
        
        write(STDERR_FILENO, "MSH>>", strlen("MSH>>"));
        // Get command
        //********** DO NOT MODIFY THIS PART. IT DISTINGUISH BETWEEN NORMAL/CORRECTION MODE***************
        executed_cmd_lines++;
        if( end != 0 && executed_cmd_lines < end) {
            command_counter = read_command_correction(&argvv, filev, &in_background, cmd_lines[executed_cmd_lines]);
        }
        else if( end != 0 && executed_cmd_lines == end) {
            return 0;
        }
        else {
            command_counter = read_command(&argvv, filev, &in_background); //NORMAL MODE                                                                                                                                               
        }
        num_commands = command_counter; // IMPORTANT: Update num_commands for the next loop's cleanup
        //************************************************************************************************



        /************************ STUDENTS CODE ********************************/
       if (command_counter > 0) {
            if (command_counter > MAX_COMMANDS){
                printf("Error: Maximum number of commands is %d \n", MAX_COMMANDS);
                continue; // <-- Add this line
            }
            else {
                // Print command
                //print_command(argvv, filev, in_background);
                //This function is used to iterate through all the commands enterd by the user. It takes values between 0 and command_counter-1.
                int ncommand = 0;
                
                 //This variable changes its value when a syntax error is detected.
                int error_syntax=0;
                //Here we use the function getCompleteCOmmand to fill up the arg_excevp for the first command.
                getCompleteCommand(argvv,ncommand);
                
                // BUILT-IN CHECK MOVED: The logic for mycalc/mycp is now inside the fork/exec block
                // to allow them to work in pipelines. We only check for them here if it's a single
                // command that doesn't need forking (for simplicity, we will fork them all).


                if(command_counter == 1 && (strcmp(argv_execvp[0],"mycalc")==0 || strcmp(argv_execvp[0],"mycp")==0)) {
                    // This block is now only for single, non-piped built-in commands.
                    // The main logic is moved into the child process block.
                }


                if (command_counter == 1){
                 //For a unique command
                //We should have an inmediate child of the minishell.
                    int pid = fork();
                  //Check there's no errors creating the child
                  if (pid < 0){
                    perror("An error has occurred when creating the child\n");
                    return -1;
                  }
                  if (pid == 0){//For the child
                  // Restore default signal handling for child
                  signal(SIGINT, SIG_DFL);


                  //Now we must check all the files redirections.
                      //For the input
                      if(strcmp(filev[0],"0") != 0){
                        int ifid = open(filev[0], O_RDONLY, 0666);
                        if (ifid < 0) { perror("Error opening input file"); exit(1); }
                        if (dup2(ifid, STDIN_FILENO) < 0) { perror("dup2 input"); exit(1); }
                        close(ifid);
                      }
                      //For the output
                      if(strcmp(filev[1],"0") != 0){
                        int ofid = open(filev[1], O_CREAT | O_WRONLY | O_TRUNC, 0666);
                        if (ofid < 0) { perror("Error opening output file"); exit(1); }
                        if (dup2(ofid, STDOUT_FILENO) < 0) { perror("dup2 output"); exit(1); }
                        close(ofid);
                      }
                      //For the standard error
                      if(strcmp(filev[2],"0") != 0){
                        int efid = open(filev[2], O_CREAT | O_WRONLY | O_TRUNC, 0666);
                        if (efid < 0) { perror("Error opening error file"); exit(1); }
                        if (dup2(efid, STDERR_FILENO) < 0) { perror("dup2 error"); exit(1); }
                        close(efid);
                      } 


                  // UNIFIED BUILT-IN AND EXEC LOGIC
                  if(strcmp(argv_execvp[0],"mycalc")==0) {
                      // --- PASTE mycalc LOGIC HERE ---
                      if (argv_execvp[1] && argv_execvp[2] && argv_execvp[3]) {
                          char *endptr1, *endptr2;
                          int operand1 = strtol(argv_execvp[1], &endptr1, 10);
                          int operand2 = strtol(argv_execvp[3], &endptr2, 10);

                          if (*endptr1 == '\0' && *endptr2 == '\0') {
                              if (strcmp(argv_execvp[2], "add") == 0) {
                                  // Perform addition, print result
                                  printf("[OK] %d + %d = %d\n", operand1, operand2, operand1 + operand2);
                                  exit(0); // Success
                              } else if (strcmp(argv_execvp[2], "mod") == 0) {
                                  if (operand2 == 0) {
                                      write(STDERR_FILENO, "[ERROR] Division by zero\n",
      strlen("[ERROR] Division by zero\n"));

                                      exit(1); // Failure
                                  }
                                  // Perform modulo, print result
                                  printf("[OK] %d %% %d = %d; Quotient %d\n", operand1, operand2, operand1 % operand2, operand1 / operand2);
                                  exit(0); // Success
                              }
                          }
                      }
                      write(STDERR_FILENO, "[ERROR] Syntax error in mycalc\n",
      strlen("[ERROR] Syntax error in mycalc\n"));

                      exit(1); // Failure: bad syntax
                      // --- END mycalc LOGIC ---
                  } else if (strcmp(argv_execvp[0],"mycp")==0) {
                      // ... PASTE mycp LOGIC HERE ...
                      // Ensure it also uses exit(0) for success and exit(1) for failure.
                      if (argv_execvp[1] && argv_execvp[2]) {
                          int src = open(argv_execvp[1], O_RDONLY);
                          if (src < 0) { perror("open src"); exit(1); }
                          int dst = open(argv_execvp[2], O_WRONLY | O_CREAT | O_TRUNC, 0666);
                          if (dst < 0) { perror("open dst"); close(src); exit(1); }
                          char buf[1024];
                          ssize_t n;
                          while ((n = read(src, buf, sizeof(buf))) > 0) {
                              if (write(dst, buf, n) != n) { perror("write"); close(src); close(dst); exit(1); }
                          }
                          close(src); close(dst);
                          exit(0);
                      }
                      write(STDERR_FILENO, "[ERROR] Syntax error in mycp\n",
      strlen("[ERROR] Syntax error in mycp\n"));

                      exit(1);
                  }


                  //To the function execvp we give the name of the program as first argument and next the whole vector of arguments for the program execution 
                    execvp(argv_execvp[0],argv_execvp);
                    // If execvp returns, an error occurred.
                    perror("There was an error executing the command execvp");
                    exit(1); // Use exit() to terminate the child process
                }else{ //When pid>0 --> for the parent child
                            //We divide it into two cases. When it's running in background and when it's not.
                            if (in_background == 0){
                                //Case 0: Not in background. Here we must wait until the child process it's finished before going on.
                             //This would be captured in the child's exit function and it's recieved by the function wait in the parent.
                                    if (waitpid(pid, &status, 0) < 0){ // FIX: Wait for the specific child PID
                                        perror("Error in waiting syscall.");
                                        return -1;
                                    }
                            }else{
                                //Case 1: We're running in background. Wait isn't needed.
                                //We print the pid of the child's process.
                                printf("[%d]\n", pid);
                            }
                    }
            }else { // command_counter > 1
    int pipes[MAX_COMMANDS-1][2];
    int pids[MAX_COMMANDS];
    int i;


    // Create pipes
    for (i = 0; i < command_counter - 1; ++i) {
        if (pipe(pipes[i]) < 0) {
            perror("pipe");
            exit(1);
        }
    }


    for (i = 0; i < command_counter; ++i) {
        getCompleteCommand(argvv, i);
        int pid = fork();
        if (pid < 0) {
            perror("fork");
            exit(1);
        }
        if (pid == 0) { // Child
            // Restore default signal handling for child
            signal(SIGINT, SIG_DFL);


            // Input redirection for first command or from previous pipe
            if (i == 0) {
                if (strcmp(filev[0], "0") != 0) {
                    int ifid = open(filev[0], O_RDONLY, 0666);
                    if (ifid < 0) { perror("open input"); exit(1); }
                    dup2(ifid, STDIN_FILENO);
                    close(ifid);
                }
            } else {
                if (dup2(pipes[i-1][0], STDIN_FILENO) < 0) { perror("dup2"); exit(1); }
                close(pipes[i-1][0]);
            }


            // Output redirection for last command or to next pipe
            if (i == command_counter - 1) {
                if (strcmp(filev[1], "0") != 0) {
                    int ofid = open(filev[1], O_WRONLY | O_CREAT | O_TRUNC, 0666);
                    if (ofid < 0) { perror("open output"); exit(1); }
                    dup2(ofid, STDOUT_FILENO);
                    close(ofid);
                }
                 // Apply error redirection ONLY to the last command
                if (strcmp(filev[2], "0") != 0) {
                    int efid = open(filev[2], O_WRONLY | O_CREAT | O_TRUNC, 0666);
                    if (efid < 0) { perror("open error"); exit(1); }
                    dup2(efid, STDERR_FILENO);
                    close(efid);
                }
            } else {
                if (dup2(pipes[i][1], STDOUT_FILENO) < 0) { perror("dup2"); exit(1); }
                close(pipes[i][1]);
            }


            // Close all pipe fds in child
            for (int j = 0; j < command_counter - 1; ++j) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            
            // UNIFIED BUILT-IN AND EXEC LOGIC FOR PIPELINES
            if(strcmp(argv_execvp[0],"mycalc")==0) {
                // --- PASTE mycalc LOGIC HERE ---
                if (argv_execvp[1] && argv_execvp[2] && argv_execvp[3]) {
                    char *endptr1, *endptr2;
                    int operand1 = strtol(argv_execvp[1], &endptr1, 10);
                    int operand2 = strtol(argv_execvp[3], &endptr2, 10);

                    if (*endptr1 == '\0' && *endptr2 == '\0') {
                        if (strcmp(argv_execvp[2], "add") == 0) {
                            // Perform addition, print result
                            printf("[OK] %d + %d = %d\n", operand1, operand2, operand1 + operand2);
                            exit(0); // Success
                        } else if (strcmp(argv_execvp[2], "mod") == 0) {
                            if (operand2 == 0) {
                               write(STDERR_FILENO, "[ERROR] Division by zero\n", strlen("[ERROR] Division by zero\n"));

                                exit(1); // Failure
                            }
                            // Perform modulo, print result
                            printf("[OK] %d %% %d = %d; Quotient %d\n", operand1, operand2, operand1 % operand2, operand1 / operand2);
                            exit(0); // Success
                        }
                    }
                    write(STDERR_FILENO, "[ERROR] Syntax error in mycalc\n", 31);
                    exit(1); // Failure: bad syntax
                    // --- END mycalc LOGIC ---
                } else if (strcmp(argv_execvp[0],"mycp")==0) {
                    // ... PASTE mycp LOGIC HERE ...
                    if (argv_execvp[1] && argv_execvp[2]) {
                        int src = open(argv_execvp[1], O_RDONLY);
                        if (src < 0) { perror("open src"); exit(1); }
                        int dst = open(argv_execvp[2], O_WRONLY | O_CREAT | O_TRUNC, 0666);
                        if (dst < 0) { perror("open dst"); close(src); exit(1); }
                        char buf[1024];
                        ssize_t n;
                        while ((n = read(src, buf, sizeof(buf))) > 0) {
                            if (write(dst, buf, n) != n) { perror("write"); close(src); close(dst); exit(1); }
                        }
                        close(src); close(dst);
                        exit(0);
                    }
                    write(STDERR_FILENO, "[ERROR] Syntax error in mycp\n", 29);
                    exit(1);
                }


            execvp(argv_execvp[0], argv_execvp);
            perror("execvp");
            exit(1);
        } else {
            pids[i] = pid;
        }
    }


    // Close all pipe fds in parent
    for (i = 0; i < command_counter - 1; ++i) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }


    // Wait for all children
    if (in_background == 0) {
        for (i = 0; i < command_counter; ++i) {
            waitpid(pids[i], NULL, 0);
        }
    } else {
        for (i = 0; i < command_counter; ++i) {
            printf("[%d]\n", pids[i]);
        }
    }
}
        }
    }
}

void cleanup(char ***argvv, int command_counter, char **cmd_lines, int end) {
    if (argvv) {
        for (int i = 0; i < command_counter; ++i) {
            if (argvv[i]) {
                for (int j = 0; argvv[i][j] != NULL; ++j) {
                    free(argvv[i][j]);
                }
                free(argvv[i]);
            }
        }
        free(argvv);
    }
    for (int i = 0; i < end; ++i) {
        free(cmd_lines[i]);
    }
}
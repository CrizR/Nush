//Created by Chris Risley on 9/27/17.
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include "svec.h"

//Function declarations
void parse_token(char* token, svec* tokens);
svec* read_input(const char *del, char* input);
int execute_command(int argc, char * argv[], int bg);
int launch_reg(int argc, char * argv[], int bg);
void run(int argc, char * argv[]);
int launch_pipe(int c_one, int c_two, char * command_one[], char * command_two[]);
void check_rv(int rb);
int del_split(int run_state, svec* tokens);
int func_cd(int argc, char * argv[]);
int func_help(int argc, char * argv[]);
int func_exit(int argc, char * argv[]);
int func_pwd(int argc, char * argv[]);
int isNum(char* ints);
int launch_redirect(int redirect, int c_one, int c_two, char * command_one[], char * command_two[]);


//Builtins
char *p_str[] = {
    "cd",
    "help",
    "exit",
    "pwd"
};
int (*p_func[]) (int argc, char * argv[]) = {
    &func_cd,
    &func_help,
    &func_exit,
    &func_pwd
};


//Entry point
int main(int argc, const char * argv[]) {
    if (argc == 1){
        //If Nush was started without any arguments run normally
        run(0, NULL);
    } else {

        //If Nush has arguments, read from the script and execute accordingly
        FILE * input_script = fopen(argv[1], "r");
        char input[256];
        while(fgets(input, 256, input_script)){
            svec* tokens = read_input(" ", input);
            char * nargv[tokens->size];
            for (int i = 0; i < tokens->size; i++){
                nargv[i] = svec_get(tokens, i);
            }
            int run_state = 1;
            run_state = del_split(run_state, tokens);
            if (run_state == 0){
             return EXIT_SUCCESS;
            }
        }
    }
  
    return EXIT_SUCCESS;
}

//Splits the tokens by delimiters to make a list of commands that are then executed
int del_split(int run_state, svec *tokens) {
    char * nargv[tokens->size + 1];
    for (int i = 0; i < tokens->size; i++){
        nargv[i] = svec_get(tokens, i);
    }
    
    int command_count = 0, k = 0, j = 0, bg = 0, piping = 0, and = 0, or = 0, exit_value = 0, redirect = 0;
    
    //count the number of commands in the token list
    for(int i = 0; i < tokens->size; i++){
        if ((strcmp(nargv[i], ";") == 0)
            ||(strcmp(nargv[i], "||") == 0)
            ||(strcmp(nargv[i], "&&") == 0)
            ||(strcmp(nargv[i], "|") == 0)
            ||(strcmp(nargv[i], "<") == 0)
            ||(strcmp(nargv[i], ">") == 0)){
            command_count++;
        }
    }
    
    char * commands[command_count + 1][tokens->size];
    // Maximum number of commands is 100
    // Had to change command_count + 1 to 100 in the initialization
    // of command_size to avoid a clang check warning that claimed
    // that command_size was uninitalized even though I
    // Iterated through each element and set the value to zero
    int command_size[100] = {0};
    
    //split each token into seperate commands
    for (int i = 0; i < tokens->size; i++){
        char * token = nargv[i];
        int right_size = tokens->size >= i + 1;
        if (strcmp(token, "&") == 0){
            bg = 1;
        }else if (strcmp(token, "||") == 0 && right_size){
            or = 1;
            j++;
            k=0;
        }else if (strcmp(token, "&&") == 0 && right_size){
            and = 1;
            j++;
            k=0;
        }else if (strcmp(token, "<") == 0 && right_size){
            redirect = 1;
            j++;
            k=0;
        }else if (strcmp(token, ">") == 0 && right_size){
            redirect = 2;
            j++;
            k=0;
        }else if (strcmp(token, "|") == 0 && right_size){
            piping = 1;
            j++;
            k=0;
        }else if (strcmp(token, ";") == 0 && right_size){
            j++;
            k=0;
        } else {
            command_size[j] += 1;
            commands[j][k] = token;
            k++;
        }
    }
    //if piping launch the pipe function
    if (piping == 1){
          if (commands[0] != NULL && commands[1] != NULL){
        run_state = launch_pipe(command_size[0],command_size[1], commands[0], commands[1]);
          }
    //if redirecting input or output launch the redirect function
    } else if (redirect > 0){
         if (commands[0] != NULL && commands[1] != NULL){
        run_state = launch_redirect(redirect, command_size[0],command_size[1], commands[0], commands[1]);
         }
    //Otherwise handly it normally and parse input for booleans like true and false
    } else {
        for (int i = 0; i <= command_count; i++){
          if (commands[i] != NULL){
            int next = 0;
            int size = command_size[i];
            commands[i][size] = 0;
            if (and == 1){
                if (commands[i][0] != NULL && strcmp(commands[i][0], "true") == 0){
                    next = 1;
                }
                if (commands[i][0] != NULL && strcmp(commands[i][0], "false") == 0){
                    break;
                }
                if (next == 0){
                    run_state = execute_command(size, commands[i], bg);
                    if (run_state == 0){
                      exit_value = 1;
                    }
                }
            } else if (or == 1){
                if (commands[i][0] != NULL && strcmp(commands[i][0], "true") == 0){
                    break;
                }
                if (commands[i][0] != NULL && strcmp(commands[i][0], "false") == 0){
                    next = 1;
                }
                if (next == 0){
                    run_state = execute_command(size, commands[i], bg);
                       if (run_state == 1){
                        exit_value = 1;
                       }
                  }
            } else {
                    run_state = execute_command(size, commands[i], bg);
            }
          }
        }
    }
    if (exit_value == 1){
        exit(EXIT_SUCCESS);
    } else {
    return run_state;
    }
}

//Runs the program until input is null
void run(int argc, char* argv[]){
    if (argv != NULL){
        execute_command(argc, argv, 0);
    }
    char input[100];
    printf("nush$ ");
    fgets(input, 100, stdin);
    const char del[]= {" "};
    int run_state = 1;
    do{
        //read input
        svec * tokens = read_input(del, input);
        //split and execute input
        run_state = del_split(run_state, tokens);
        if (run_state == 0){
            exit(EXIT_SUCCESS);
        }
        free_svec(tokens);
        printf("nush$ ");
    }while(fgets(input, 100, stdin) != NULL);
}


//Read the users input character by character
svec* read_input(const char *del, char *input) {
    //Rid of trailing new line
    input[strcspn(input, "\n")] = 0;
    svec* tokens = make_svec();
    char* token = strtok(input, del);
    while (token) {
        parse_token(token, tokens);
        token = strtok(NULL, " ");
    }
    return tokens;
}

// Seperate each token by the |<>&; delimiters and add them to the svec struct
void parse_token(char* token, svec* tokens){
    char res[100] = "";
    char *del = "|<>&;";
    int k = 0;
    while (token[k] != '\0'){
        int j = 0;
        while (j != 6){
            if (token[k] == del[j]){
                if (token[k+1] == '&' || token[k+1] == '|'){
                    char str[3] = "\0";
                    str[0] = token[k];
                    str[1] = token[k+1];
                    k+=1;
                    token[k] = 0;
                    svec_push_back(tokens, res);
                    svec_push_back(tokens, str);
                    strcpy(res, "");
                    
                } else if (token[k] == del[j]){
                    token[k] = 0;
                    char str[2] = "\0";
                    str[0] = del[j];
                    svec_push_back(tokens, res);
                    svec_push_back(tokens, str);
                    strcpy(res, "");
                }
            }
            j++;
        }
        if (token[k] != ' ' && token[k] != '\t' && token[k] != '\n'){
            char str[2] = "\0";
            str[0] = token[k];
            strcat(res, str);
        }
        k++;
    }
    svec_push_back(tokens, res);
}


//Executes the commands by first checking if the command is a builtin
//If it's not then launching the regular shell function
int execute_command(int argc, char * argv[], int bg){
    int num_of_commands = sizeof(p_str) / sizeof(char *);
    if (argc == 0){
        return 1;
    }
    for (int i = 0; i < num_of_commands; i++){
        if (strcmp(argv[0], p_str[i]) == 0){
            return (*p_func[i])(argc,argv);
        }
    }
    return launch_reg(argc, argv, bg);
}

//For redirecting input and output.
//Ex. sort < test.txt
//Ex. sort test.txt > sortedtest.txt
int launch_redirect(int redirect, int c_one, int c_two, char * command_one[], char * command_two[]){

    command_one[c_one] = 0;
    command_two[c_two] = 0;

    pid_t fork1 = fork();
    if (fork1 == 0){
        if (fork1 < 0){
            perror("-nush:");
        }
        //Child
        if (redirect == 1 && command_two[0] != NULL){ // <
            //Open file
            int input = open(command_two[0], O_RDONLY, 0777);
            //Put file into stdin
            dup2(input, 0);
            //Close file
            close(input);
            //Execute command on stdin
            if (execvp(command_one[0], command_one) == -1){
                perror("-nush:");
                exit(EXIT_FAILURE);
            }
            
        } else if (redirect == 2 && command_one[0] != NULL){ // >
            //Open output file
            int output_file = open(command_two[0], O_CREAT | O_TRUNC | O_RDWR, 0777);
            //Set output file into stdout
            dup2(output_file, 1);
            //Close file
            close(output_file);
            //Execute command to stdout
            if (execvp(command_one[0], command_one) == -1){
                perror("-nush:");
                exit(EXIT_FAILURE);
            }
        }
      
    }
    //Parent then waits for child.
    waitpid(fork1, NULL, WUNTRACED);
    return 1;
}


//Taken and Modified From Lecture Notes
//Forks the program twice and pipes the output of the first command into the input of the
//second
int launch_pipe(int c_one, int c_two, char * command_one[], char * command_two[]){
    long rv;
    int pipe_fds[2];
    rv = pipe(pipe_fds);
    assert(rv != -1);
    
     command_one[c_one] = 0;
     command_two[c_two] = 0;
    
    pid_t fork1 = fork();
    if (fork1 == 0 && command_one[0] != NULL){
        //In Child
        if (fork1 < 0){
               perror("-nush:");
        }
        //Child
        //put pipe 1 into stdout
        dup2(pipe_fds[1], STDOUT_FILENO);
        //close the other pipe
        close(pipe_fds[0]);
        //execute command
        if (execvp(command_one[0], command_one) == -1) {
             perror("-nush:");
           exit(EXIT_FAILURE);
        }
    }
    
    pid_t fork2 = fork();
    if (fork2 == 0 && command_two[0] != NULL){
        //In Child
        if (fork2 < 0){
            perror("-nush:");
        }
        dup2(pipe_fds[0], STDIN_FILENO);
        close(pipe_fds[1]);
       
        if (execvp(command_two[0], command_two) == -1) {
           perror("-nush:");
            exit(EXIT_FAILURE);
        }
    }
        //Close pipes and have parent wait
         close(pipe_fds[0]);
         close(pipe_fds[1]);
         waitpid(fork1, NULL, WUNTRACED);
         waitpid(fork2, NULL, WUNTRACED);
  
    return 1;
}

//Partly from lecture notes but has been modified significantly.
//Executes the commands as defined in the argv. If command is to be put
//in the background then executes the command and has the parent continue without
//waiting
int launch_reg(int argc, char * argv[], int bg){
    int cpid = fork();
    char *cmd = argv[0];
    if (cpid == 0){
        //Child
        argv[argc] = 0;
        for (int ii = 0; ii < strlen(cmd); ++ii) {
            if (cmd[ii] == ' ') {
                cmd[ii] = 0;
                break;
            }
        }
        
        if (bg == 1){
            fclose(stdin); // close child's stdin
            fopen("/dev/null", "r");
        }
        
        if (execvp(cmd, argv) == -1) {
              perror("-nush:");
            exit(EXIT_FAILURE);
        }
        
    } else {
        if (bg == 0){
            //If background process don't wait
            waitpid(cpid, NULL, 0);
        }
    }
    return 1;
}

//Exit the program
int func_exit(int argc, char * argv[]){
    return 0;
}

//Return the current directory
int func_pwd(int argc, char * argv[]){
     char path[1024];
    if (getcwd(path, sizeof(path)) != NULL){
        printf("%s", path);
        printf("\r\n");
    } else {
         perror("-nush:");
        exit(EXIT_FAILURE);
    }
    return 1;
}

//Determine if the given characters are ints
int isNum(char* ints){
    for(int i = 0; i < sizeof(ints) / sizeof(char *); i++){
        if (!isdigit(ints[i])){
            return 0;
        }
    }
    return 1;
}

//Learn more about this unix shell recreation
int func_help(int argc, char * argv[]){
    char * ops = "|<>;||&&sleep";
    for(int i = 0; i < 70; i++){
        printf("%s", "-");
    }
    printf("\r\n");
    printf("Welcome to Chris Risley's Unix Shell called Nush\n");
    printf("Nush& supports the following commands and operators:\r\n");
    
    printf("%s", "Operators:");
    for(int i = 0; i < sizeof(ops) / sizeof(char); i++){
        printf("%c ", ops[i]);
    }
    printf("\r\n");
    
    printf("%s", "Built Ins:");
    for(int i = 0; i < sizeof(p_str) / sizeof(char *); i++){
        printf("%s() ", p_str[i]);
    }
    printf("\r\n");
    
    for(int i = 0; i < 70; i++){
        printf("%s", "-");
    }
    printf("\r\n");
    
    return 1;
}

//Changes the current directory
int func_cd(int argc, char * argv[]){
    if (argc < 2){
        chdir("/Users/");
    } else {
        if (chdir(argv[1]) != 0) {
            perror("-nush:");
        }
    }
    return 1;
}




#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include <pwd.h>
#include <sys/types.h>
#include "linenoise.h"

#define MAX_LENGTH 1024
#define clear_terminal() printf("\033[H\033[J")
#define TERMINAL_TITLE "Eggshell Terminal"

static char* PATH = "";
static char* PROMPT = "eggsh> ";
static char CWD[MAX_LENGTH] = "";
static char* USER = "";
static char* HOME = "";
static char SHELL[MAX_LENGTH] = "";
static char TERMINAL[MAX_LENGTH] = "";
//static int EXTICODE = 1;

void eggsh_start();
void welcome_message();
void change_directory(char* path);
void print_header();

int main(int argc, char **argv, char **env) {

    clear_terminal();
    linenoiseClearScreen();

    char* input;

    eggsh_start();
    welcome_message();

    //just testing changing of directory
    //change_directory("/home/keithgciantar/Desktop");

    //add while to keep getting input
    //make the prompt it's own function in a while loop

    input = linenoise(PROMPT);

    printf("%s is your input...\n\n", input);

    return 0;
}

void eggsh_start(){
    print_header();

    static char* buffer;
    size_t  buffer_size = (size_t)sysconf(_SC_GETPW_R_SIZE_MAX);

    if(buffer_size == -1){
        buffer_size = 16*MAX_LENGTH;
    }

    buffer = malloc(buffer_size);

    struct passwd pwd;
    struct passwd *user_details;
    int out = getpwuid_r(getuid(), &pwd, buffer, buffer_size, &user_details);
    if(user_details != NULL) {
        if(out == 0) {
            USER = user_details->pw_name;
            HOME = user_details->pw_dir;
        }
    }

    free(buffer);

    //get the HOME environmental variable
    if(getenv("HOME") != NULL && HOME != "") {
        HOME = getenv("HOME");
    }

    //get the USER environmental variable
    if(getenv("USER") != NULL && USER != "") {
        USER = getenv("USER");
    }

    //get the working directory where the shell launched from
    if(getcwd(SHELL, MAX_LENGTH) == NULL) {
        perror("Cannot get shell binary directory.\n");
    }

    //get the name of current terminal
    if(ttyname_r(STDIN_FILENO, TERMINAL, MAX_LENGTH) != 0) {
        perror("Cannot get terminal name.\n");
    }

    //get the current working directory
    if(getcwd(CWD, MAX_LENGTH) == NULL) {
        perror("Cannot get current working directory.\n");
    }

    //get the search path for external commands
    if(getenv("PATH") != NULL){
        PATH = getenv("PATH");
    }
}

void welcome_message(){
    printf("Welcome %s!\n", USER);
    printf("Terminal: %s\n", TERMINAL);
    printf("Working Directory: %s\n", CWD);
    printf("Shell Path: %s\n", SHELL);
    printf("Search Path: %s\n\n", PATH);
}

void change_directory(char* path){
    chdir(path);

    if(getcwd(CWD, MAX_LENGTH) == NULL) {
        perror("Cannot get current working directory");
    }

    printf("Directory changed to: %s\n\n", CWD);
}

void print_header(){
    struct winsize terminal_size;
    ioctl(STDIN_FILENO, TIOCGWINSZ, &terminal_size);

    for(int i = 0; i < (terminal_size.ws_col - strlen(TERMINAL_TITLE))/2; i++){
        printf("-");
    }

    printf("%s", TERMINAL_TITLE);

    for(int i = 0; i < (terminal_size.ws_col - strlen(TERMINAL_TITLE))/2; i++){
        printf("-");
    }

    printf("\n");
}
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include <pwd.h>
#include <sys/types.h>
#include "linenoise.h"

#define MAX_LENGTH 1024
#define EDITABLE_SHELL_VARS 7

#define clear_terminal() printf("\033[H\033[J")
#define TERMINAL_TITLE "Eggshell Terminal"

char *PATH = "";
char *PROMPT = "eggsh> ";
char CWD[MAX_LENGTH] = "";
char *USER = "";
char *HOME = "";
char SHELL[MAX_LENGTH] = "";
char TERMINAL[MAX_LENGTH] = "";
int EXTICODE = 0;

static char SHELL_VAR_NAMES[EDITABLE_SHELL_VARS][MAX_LENGTH];

static char USER_VAR_NAMES[MAX_LENGTH][MAX_LENGTH];
static char USER_VAR_VALUES[MAX_LENGTH][MAX_LENGTH];

static int VAR_COUNT = 0;

void eggsh_init();
void welcome_message();

void change_directory(char *path);
void print_header();
void get_input();

int check_for_char_in_string(const char input[], int input_length, char check_char);
int check_shell_variable_names(const char var_name[]);
int check_user_variable_names(const char var_name[]);

void set_variable(const char input[], int input_length, int equals_position);

char *get_variable_value(const char var_name[]);

void clear_string(char input[], int input_length);

void print_shell_variables();
void print_user_variables();

void auto_complete(const char *buffer, linenoiseCompletions *lc);
//char* auto_complete_hints(const char *buffer, int *colour, int *bold);

int main(int argc, char **argv, char **env) {

    //clear any data in the terminal before starting
    clear_terminal();
    linenoiseClearScreen();

    //setup tab autocomplete with the callback auto_complete
    //the callback will run any time the user presses tab
    linenoiseSetCompletionCallback(auto_complete);

    //linenoiseSetHintsCallback(auto_complete_hints);

    eggsh_init();

    welcome_message();

    get_input();

    return 0;
}

void auto_complete(const char *buffer, linenoiseCompletions *lc) {
    if (buffer[0] == 'e' || buffer[0] == 'E') {
        linenoiseAddCompletion(lc, "exit");
    } else if (buffer[0] == 'a' || buffer[0] == 'A') {
        linenoiseAddCompletion(lc, "all");
    }
}

//this is a very useful addition to the program but I think it might be a little intrusive at the moment
/*char* auto_complete_hints(const char *buffer, int *colour, int *bold) {
    if(strcasecmp(buffer, "e") == 0) {
        *colour = 36;
        *bold = 0;
        return "xit";
    }
    return NULL;
}*/

//initialise shell variables
void eggsh_init() {

    linenoiseHistorySetMaxLen(10);

    //create a buffer to be used by getpwuid_r
    static char *buffer;
    size_t buffer_size = (size_t) sysconf(_SC_GETPW_R_SIZE_MAX);

    if (buffer_size == -1) {
        buffer_size = 16 * MAX_LENGTH;
    }

    buffer = malloc(buffer_size);

    //get the HOME and USER shell variables by using getpwuid_r
    struct passwd pwd;
    struct passwd *user_details;
    int out = getpwuid_r(getuid(), &pwd, buffer, buffer_size, &user_details);
    if (user_details != NULL) {
        if (out == 0) {
            USER = user_details->pw_name;
            HOME = user_details->pw_dir;
        }
    }

    free(buffer);

    //get the HOME environmental variable
    if (getenv("HOME") != NULL && HOME != "") {
        HOME = getenv("HOME");
    }

    //get the USER environmental variable
    if (getenv("USER") != NULL && USER != "") {
        USER = getenv("USER");
    }

    //get the working directory where the shell launched from
    if (getcwd(SHELL, MAX_LENGTH) == NULL) {
        perror("Cannot get shell binary directory.\n");
    }

    //get the name of current terminal
    if (ttyname_r(STDIN_FILENO, TERMINAL, MAX_LENGTH) != 0) {
        perror("Cannot get terminal name.\n");
    }

    //get the current working directory
    if (getcwd(CWD, MAX_LENGTH) == NULL) {
        perror("Cannot get current working directory.\n");
    }

    //get the search path for external commands
    if (getenv("PATH") != NULL) {
        PATH = getenv("PATH");
    }

    strcpy(SHELL_VAR_NAMES[0], "PATH");
    strcpy(SHELL_VAR_NAMES[1], "PROMPT");
    strcpy(SHELL_VAR_NAMES[2], "CWD");
    strcpy(SHELL_VAR_NAMES[3], "USER");
    strcpy(SHELL_VAR_NAMES[4], "HOME");
    strcpy(SHELL_VAR_NAMES[5], "SHELL");
    strcpy(SHELL_VAR_NAMES[6], "TERMINAL");
}

//print header and welcome message
void welcome_message() {
    print_header();
    printf("Welcome %s!\n", USER);
}

void change_directory(char *path) {
    chdir(path);

    if (getcwd(CWD, MAX_LENGTH) == NULL) {
        perror("Cannot get current working directory");
    }

    printf("Directory changed to: %s\n\n", CWD);
}

void print_header() {
    struct winsize terminal_size;
    ioctl(STDIN_FILENO, TIOCGWINSZ, &terminal_size);

    for (int i = 0; i < (terminal_size.ws_col - strlen(TERMINAL_TITLE)) / 2; i++) {
        printf("-");
    }

    printf("%s", TERMINAL_TITLE);

    for (int i = 0; i < (terminal_size.ws_col - strlen(TERMINAL_TITLE)) / 2; i++) {
        printf("-");
    }

    printf("\n");
}

//indefinitely read from the input until 'exit' is entered
void get_input() {
    char *input;
    int input_length = 0;
    while ((input = linenoise(PROMPT)) != NULL) {
        if (strcasecmp(input, "exit") == 0) {
            printf("%s\n", input);
            return;
        } else if (strcasecmp(input, "all") == 0) {
            print_shell_variables();
        }

        linenoiseHistoryAdd(input);
        //printf("%s\n", input);

        input_length = (int) strlen(input);

        //checking for var=value
        int equals_position = check_for_char_in_string(input, input_length, '=');

        if (equals_position != -1 && equals_position != 0) {
            set_variable(input, input_length, equals_position);
        }

        if (VAR_COUNT != 0) {
            //print_user_variables();
        }

        linenoiseFree(input);
        input = "";
    }
}

//function to check if the input contains an a certain character
//returns a -1 if no character is found, else returns the position of the character
int check_for_char_in_string(const char input[], int input_length, char check_char) {
    int char_position = -1;

    for (int i = 0; i < input_length; i++) {
        if (input[i] == check_char) {
            char_position = i;
            break;
        }
    }

    return char_position;
}

//check the editable shell variables to see if a variable exists
//if the variable exists return the position of that variable, else return -1
int check_shell_variable_names(const char var_name[]) {
    int var_position = -1;

    for (int i = 0; i < EDITABLE_SHELL_VARS; i++) {
        if (strcasecmp(var_name, SHELL_VAR_NAMES[i]) == 0) {
            var_position = i;
            break;
        }
    }

    return var_position;
}

//check the array of user added variables to see if a variable exists
//if the variable exists return the position of that variable, else return -1
int check_user_variable_names(const char var_name[]) {
    int var_position = -1;

    for (int i = 0; i < VAR_COUNT; i++) {
        if (strcasecmp(var_name, USER_VAR_NAMES[i]) == 0) {
            var_position = i;
            break;
        }
    }

    return var_position;
}

//adding a user created variables if var=value is entered
void set_variable(const char input[], int input_length, int equals_position) {
    char temp_var_name[MAX_LENGTH] = "";
    strncpy(temp_var_name, input, (size_t) equals_position);

    char temp_var_value[MAX_LENGTH] = "";
    strncpy(temp_var_value, &input[equals_position + 1], (size_t) (input_length - (equals_position + 1)));

    int dollar_position = check_for_char_in_string(input, input_length, '$');
    if (dollar_position != -1) {
        char var_after_dollar[MAX_LENGTH] = "";
        strncpy(var_after_dollar, &input[dollar_position + 1], (size_t) (input_length - (dollar_position + 1)));

        strcpy(temp_var_value, "");
        strcpy(temp_var_value, get_variable_value(var_after_dollar));

        if (strcmp(temp_var_value, "VARIABLE NOT FOUND") == 0) {
            strncpy(temp_var_value, &input[equals_position + 1], (size_t) (input_length - (equals_position + 1)));
        }
    }

    //checking whether a variable already exists as a shell variable
    int var_position = check_shell_variable_names(temp_var_name);
    if (var_position != -1) { //if the variable already exists as a shell variable, update the the value
        char *temp_value_pointer = &temp_var_value[0];

        if (strcmp(SHELL_VAR_NAMES[var_position], "PATH") == 0) {
            clear_string(USER_VAR_VALUES[var_position], (int) strlen(PATH));
            PATH = temp_value_pointer;
        } else if (strcmp(SHELL_VAR_NAMES[var_position], "PROMPT") == 0) {
            clear_string(USER_VAR_VALUES[var_position], (int) strlen(PROMPT));
            PROMPT = temp_value_pointer;
        } else if (strcmp(SHELL_VAR_NAMES[var_position], "CWD") == 0) {
            clear_string(USER_VAR_VALUES[var_position], (int) strlen(CWD));
            strcpy(CWD, temp_var_value);
        } else if (strcmp(SHELL_VAR_NAMES[var_position], "USER") == 0) {
            clear_string(USER_VAR_VALUES[var_position], (int) strlen(USER));
            USER = temp_value_pointer;
        } else if (strcmp(SHELL_VAR_NAMES[var_position], "HOME") == 0) {
            clear_string(USER_VAR_VALUES[var_position], (int) strlen(HOME));
            HOME = temp_value_pointer;
        } else if (strcmp(SHELL_VAR_NAMES[var_position], "SHELL") == 0) {
            clear_string(USER_VAR_VALUES[var_position], (int) strlen(SHELL));
            strcpy(SHELL, temp_var_value);
        } else if (strcmp(SHELL_VAR_NAMES[var_position], "TERMINAL") == 0) {
            clear_string(USER_VAR_VALUES[var_position], (int) strlen(TERMINAL));
            strcpy(TERMINAL, temp_var_value);
        }
    } else {
        var_position = check_user_variable_names(temp_var_name);

        if (var_position != -1) { //if the variable already exists as a user added variable, update the the value
            clear_string(USER_VAR_VALUES[var_position], MAX_LENGTH);
            strncpy(USER_VAR_VALUES[var_position], temp_var_value, strlen(temp_var_value));
        } else { //if the variable does not exist, add a new variable
            strncpy(USER_VAR_NAMES[VAR_COUNT], temp_var_name, strlen(temp_var_name));
            strncpy(USER_VAR_VALUES[VAR_COUNT], temp_var_value, strlen(temp_var_value));
            VAR_COUNT++;
        }
    }
}

char *get_variable_value(const char var_name[]) {
    int var_position = -1;
    char var_value[MAX_LENGTH] = "";

    for (int i = 0; i < EDITABLE_SHELL_VARS; i++) {
        if (strcmp(SHELL_VAR_NAMES[i], var_name) == 0) {
            var_position = i;
            break;
        }
    }

    if (var_position != -1) {
        if (strcmp(SHELL_VAR_NAMES[var_position], "PATH") == 0) {
            strcpy(var_value, PATH);
        } else if (strcmp(SHELL_VAR_NAMES[var_position], "PROMPT") == 0) {
            strcpy(var_value, PROMPT);
        } else if (strcmp(SHELL_VAR_NAMES[var_position], "CWD") == 0) {
            strcpy(var_value, CWD);
        } else if (strcmp(SHELL_VAR_NAMES[var_position], "USER") == 0) {
            strcpy(var_value, USER);
        } else if (strcmp(SHELL_VAR_NAMES[var_position], "HOME") == 0) {
            strcpy(var_value, HOME);
        } else if (strcmp(SHELL_VAR_NAMES[var_position], "SHELL") == 0) {
            strcpy(var_value, SHELL);
        } else if (strcmp(SHELL_VAR_NAMES[var_position], "TERMINAL") == 0) {
            strcpy(var_value, TERMINAL);
        }
    } else {
        for (int i = 0; i < VAR_COUNT; i++) {
            if (strcmp(USER_VAR_NAMES[i], var_name) == 0) {
                var_position = i;
                break;
            }
        }

        if (var_position != -1) {
            strcpy(var_value, USER_VAR_VALUES[var_position]);
        } else {
            strcpy(var_value, "VARIABLE NOT FOUND");
        }
    }

    char *output_value = &var_value[0];

    return output_value;
}

void clear_string(char input[], int input_length) {
    for (int i = 0; i < input_length; i++) {
        input[i] = '\0';
    }
}

//print all the shell variables
void print_shell_variables() {
    //PATH
    //PROMPT
    //CWD
    //USER
    //HOME
    //SHELL
    //TERMINAL
    printf("PATH=%s\n", PATH);
    printf("PROMPT=%s\n", PROMPT);
    printf("CWD=%s\n", CWD);
    printf("USER=%s\n", USER);
    printf("HOME=%s\n", HOME);
    printf("SHELL=%s\n", SHELL);
    printf("TERMINAL=%s\n", TERMINAL);
    print_user_variables();
}

//print all the user variables
void print_user_variables() {
    for (int i = 0; i < VAR_COUNT; i++) {
        printf("%s=%s \n", USER_VAR_NAMES[i], USER_VAR_VALUES[i]);
    }
}
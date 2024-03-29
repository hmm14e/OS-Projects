#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include "shell.h"
#include "builtins.h"
#include "utils.h"


#define SH_LINE_BUFFSIZE 255
#define SH_TOKEN_BUFFSIZE 255
#define  SH_PATH_BUFFSIZE 255

const char* SH_TOKEN_DELIMS = " \t\n\r";
const char *SH_SPECIAL_CHARS = "|<>&";

void _print_args(char** args)
{
    for (char *s = *args; s != NULL; s=*++args)
        printf("%s ", s);
    printf("\n");
}


char *sh_read_line()
{
    char *buffer = calloc(SH_LINE_BUFFSIZE, sizeof(char));
    size_t ix = 0;
    char c;
    while(1) {
        c = getchar();
        if (c == '\n') {
            buffer[ix] = '\0';
            return buffer;
        }
        else
            buffer[ix] = c;
        ix++;
    }
}


/* for each char, replace all instances of it with " <char> " */
char *sh_add_whitespace(char *line, const char *chars)
{
    char old[2], new[4];
    old[1] = '\0';
    /* tmp will hold intermediary stages */
    char *ret, *tmp = calloc(512, sizeof(char));
    strcpy(tmp, line);
    for (int i = 0; i < strlen(chars); i++) {
        old[0] = chars[i];
        sprintf(new, " %s ", old);
        ret = str_replace(tmp, old, new);
        if (!ret)
            continue;
        /* free the old, and set the new */
        free(tmp);
        tmp = ret;
    }
    return ret;
}


char **sh_parse_line(char *line)
{
    char **tokens = str_split(line, SH_TOKEN_DELIMS);
    if (!tokens)
        exit(-1);
    return tokens;
}


/* return true if there are no parsing errors */
/* this function should only be called after `sh_add_whitespace`, assuring that '&', '|', '<', '>' are all alone */
bool _is_well_formed(char **args)
{
    /* empty command is considered invalid */
    if (!args[0])
        return false;

    for (int i = 0; args[i] != NULL; i++) {
        /* can't have '&' anywhere but beginning and end */
        if (strcmp(args[i], "&") == 0)
            if (i != 0 && args[i + 1] != NULL){
                fprintf(stdout, "sh: parsing error near &\n");
                return false;
            }
        /*  any '>', '<', '|' must have a "command" before AND after it */
        if (strcmp(args[i], "<") == 0 || strcmp(args[i], ">") == 0 || strcmp(args[i], "|") == 0) {
            /* if before of after are empty, clearly there is no command */
            if (i == 0 || !args[i - 1] || !args[i + 1]){
                fprintf(stdout, "sh: parsing error near %s\n", args[i]);
                return false;
            }
            /* now check to see that the args before or after it are "commands", e.g. not '<', '>', '|' or '&' */
            if (strchr(SH_SPECIAL_CHARS, args[i - 1][0]) || strchr(SH_SPECIAL_CHARS, args[i + 1][0])) {
                fprintf(stdout, "sh: parsing error near %s\n", args[i]);
                return false;
            }
        }
    }
    return true;
}


bool _contains_env_variable(char* tok)
{
    if (strlen(tok) <= 1)
        return false;
    /* env variable is of form $[A-z][A-z0-9]* */
    return tok && tok[0] == '$' && isalpha(tok[1]);
}


/* returns length of environment variable, $[A-z][A-z0-9]* */
int _get_env_var_len(char *tok)
{
    for (int i = 0; i < strlen(tok); i++) {
        if (tok[i] != '$' && !isalnum(tok[i])) {
            return i;
        }
    }
    return strlen(tok);
}


bool _is_path_variable(char* tok)
{
    return tok && (strchr(tok, '/') || tok[0] == '/' || tok[0] == '.' || tok[0] == '~');
}


/**
 * expands all environment variables found in the args
 * assumes that environment variables only start at the beginning of a token
 */
char **sh_expand_env_vars(char** args)
{
    if (!args) return NULL;
    /* create copy instead of modifying in place */
    char **expanded_args = strstr_copy(args);
    /* check each token for a leading env variable, replace if found */
    for(int i = 0; expanded_args[i] != NULL; i++){
        char *arg = expanded_args[i];
        if (_contains_env_variable(arg)) {
            /* extract the env_var from the arg */
            int env_var_len = _get_env_var_len(arg); /* including $ */
            char *env_var = calloc(env_var_len + 1, sizeof(char));
            strncpy(env_var, arg, env_var_len);

            /* lookup the value, and replace the variable with the actual value */
            char *env_var_val = getenv(env_var + 1); /* +1 excludes $ */
            char *expanded;
            if (env_var_val)
                expanded = str_replace(arg, env_var, env_var_val);
            else {
                fprintf(stderr, "sh: %s not found\n", env_var);
                _free2d(expanded_args);
                return NULL;
            }
            /* update to expanded arg */
            free(expanded_args[i]);
            expanded_args[i] = expanded;
        }
    }
    return expanded_args;
}


/**
 * we define a command as an argument that is the first tokenm directly after a pipe '|', or after `etime` or `io`
 * return 0: arg, 1: cd 2: built-in command, 3: external command
 */
int _is_command(char **args, int i)
{
    if (i != 0 && strcmp(args[i - 1], "|") != 0 &&
        strcmp(args[i - 1], "etime") != 0 && strcmp(args[i - 1], "io") != 0)
        return 0;
    else if (strcmp(args[i], "cd") == 0)
        return 1;
    else if (is_builtin_cmd(args[i]))
        return 2;
    else
        return 3;
}


/**
 * resolve paths by resolving '.'s, '..'s and '~'s,
 * returns NULL on failure to expand
 */
char *_resolve_path(char* path) {
    /* if ~, first prepend $HOME to the path, then proceed */
    char *new_path;
    if (path[0] == '~')
        new_path = str_combine(getenv("HOME"), path + 1);
    else
        /* hacky way to copy string */
        new_path = str_combine(path, "");

    /* defer to realpath() to resolve '.'s and '..'s */
    char *realpath_buffer = NULL;
    char *ret = realpath(new_path, realpath_buffer);
    free(new_path);
    if (!ret){
        free(realpath_buffer);
        fprintf(stdout, "sh: no such file or directory: %s\n", path);
        return NULL;
    }
    return ret;
}


int _is_regular_file(const char *path)
{
    struct stat path_stat;
    stat(path, &path_stat);
    return S_ISREG(path_stat.st_mode);
}


/* searches $PATH for the first matching path and returns the full path*/
char *_match_path(char *executable)
{
    /* create copy of getenv("PATH") becayse str_split modifies it */
    char **dirs = str_split(getenv("PATH"), ":");
    char *ret = NULL;
    for (int i = 0; dirs[i] != NULL; i++){
        /* append '/' to the dir */
        char *dir_slash = str_combine(dirs[i], "/");
        char *filepath = str_combine(dir_slash, executable);
        free(dir_slash);
        /* X_OK checks for execute permission */
        if (_is_regular_file(filepath) && access(filepath, X_OK) != -1){
            ret = filepath;
            break;
        }
        free(filepath);
    }
    if (!ret)
        fprintf(stdout, "sh: command not found: %s\n", executable);
    _free2d(dirs);
    return ret;
}

char *_expand_external_command(char *arg)
{
    char *expanded_path = NULL;
    if (strchr(arg, '/'))
        /* expand relative path */
        expanded_path = _resolve_path(arg);
    else
        /* search the $PATH */
        expanded_path = _match_path(arg);
    return expanded_path;
}


/**
 * expand paths when the command is an external command
 * Searches $PATH when the command is NOT builtin and there are no '/s'
 * e.g. python prog.py -> /usr/bin/python prog.py
 */
char** sh_expand_paths(char** args)
{
    if (!args) return NULL;

    /* create copy instead of modifying in place */
    char **expanded_args = strstr_copy(args);

    int arg_type;
    for (int i = 0; expanded_args[i] != NULL; i++){
        char *arg = expanded_args[i];
        if ((arg_type = _is_command(expanded_args, i)) > 0) {
            if (arg_type == 1){
                /* cd, if arg appears after, must expand it */
            }
            else if (arg_type == 3) {
                /* expand external commands */
                /* built-ins `etime` and `io` also expect a external command as their first arg */
                char *expanded_path = _expand_external_command(arg);
                /* error out on broken path  */
                if (!expanded_path) {
                    _free2d(expanded_args);
                    return NULL;
                }
                /* successful expansion */
                free(expanded_args[i]);
                expanded_args[i] = expanded_path;
            }
        }
        /* cd's first args (if present) needs to be expanded */
        else if (i > 0 && strcmp(args[i - 1], "cd") == 0) {
            if (strchr(arg, '/') || strchr(arg, '~') || strchr(arg, '.')){
                char *expanded_path = _resolve_path(arg);
                if (!expanded_path) {
                    _free2d(expanded_args);
                    return NULL;
                }
                free(expanded_args[i]);
                expanded_args[i] = expanded_path;
            }
        }
    }
    return expanded_args;
}


void sh_prompt()
{
    fflush(stdout);
    printf("%s@%s :: %s => ", getenv("USER"), getenv("MACHINE"), getenv("PWD"));
    fflush(stdout);
}


size_t eq_current_size(CommandGroup** bg_cmd_grp_queue)
{
    size_t cur_size = 0;
    while (bg_cmd_grp_queue[cur_size] != NULL){
        cur_size++;
    }
    return cur_size;
}


/* remove the cmd_grp at position i, and shift every cmd_grp after it left */
void eq_remove_and_free(CommandGroup** bg_cmd_grp_queue, int i)
{
    size_t cur_size = eq_current_size(bg_cmd_grp_queue);
    if (i >= cur_size) {
        fprintf(stderr, "invalid eq_remove at pos: %i with cur_size: %zu\n", i, cur_size);
        return;
    }
    if (i < 0 || !bg_cmd_grp_queue[i])
        return;
    command_group_free(bg_cmd_grp_queue[i]);
    bg_cmd_grp_queue[i] = NULL;
    /* shift left */
    for (int j = i + 1; j < cur_size; j++) {
        bg_cmd_grp_queue[j - 1] = bg_cmd_grp_queue[j];
        bg_cmd_grp_queue[j] = NULL; /* mark end */
    }
}


void eq_append(CommandGroup** bg_cmd_grp_queue, CommandGroup* cmd_grp)
{
    size_t cur_size = eq_current_size(bg_cmd_grp_queue);
    if (cur_size > 255) {
        fprintf(stderr, "command group full!\n");
        return;
    }
    /* print the position in the execution queue  and the pids*/
    printf("[%zu] ", cur_size + 1);
    for (int i = 0; i < cmd_grp->num_unreaped_pids; i++)
        printf("%i ", cmd_grp->unreaped_pids[i]);
    printf("\n");

    bg_cmd_grp_queue[cur_size] = cmd_grp;
}


/* loop throught the entire queue, removing the pid from */
void eq_remove_pid(CommandGroup** bg_cmd_grp_queue, pid_t pid)
{
    size_t qsize = eq_current_size(bg_cmd_grp_queue);
    CommandGroup *cmd_grp;
    for (int i = 0; i < qsize; i++) {
        cmd_grp = bg_cmd_grp_queue[i];
        /* only removes if cmd_grp owns the pid */
        command_group_reap_pid(cmd_grp, pid);
        if (cmd_grp->num_unreaped_pids == 0) {
            printf("[%d]+ ", i + 1);
            command_group_print(cmd_grp);
            printf("\n");
            /* shift the entire queue due to removal, and free the cmd_grp */
            eq_remove_and_free(bg_cmd_grp_queue, i);
        }
    }
}


void sh_reap_zombies(CommandGroup** bg_cmd_grp_queue)
{
    int status;
    pid_t pid;
    while((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        eq_remove_pid(bg_cmd_grp_queue, pid);
    }
}


void sh_loop()
{
    char *line, *whitespaced_line;
    char **args, **exp_env_args, **exp_path_args;
    CommandGroup **bg_cmd_grp_queue = calloc(256, sizeof(CommandGroup*));

    pid_t pid = getpid();
    do {
        /* */
        sh_reap_zombies(bg_cmd_grp_queue);
        sh_prompt();
        line = sh_read_line();

        whitespaced_line = sh_add_whitespace(line, SH_SPECIAL_CHARS);

        args = sh_parse_line(whitespaced_line);

        if (!_is_well_formed(args)) {
            free(line); free(whitespaced_line); _free2d(args);
            continue;
        }

        /* expand env variables */
        exp_env_args = sh_expand_env_vars(args);
        if (!exp_env_args) {
            free(line); free(whitespaced_line); _free2d(args);
            continue;
        }

        /* expand commands to absolute paths */
        exp_path_args = sh_expand_paths(exp_env_args);
        if (!exp_path_args){
            free(line); free(whitespaced_line); _free2d(args); _free2d(exp_env_args);
            continue;
        }

        /* create command group and execute */
        CommandGroup * cmd_grp = command_group_from_args(exp_path_args);
        /* actual execution */
        command_group_execute(cmd_grp);
        /* background cmd_grp's get free'd when all their child pids are reaped */
        if (cmd_grp->background){

            eq_append(bg_cmd_grp_queue, cmd_grp);
        }
        else
            command_group_free(cmd_grp);
        /* cleanup */
        free(line); free(whitespaced_line); _free2d(args);
        _free2d(exp_env_args); _free2d(exp_path_args);

    } while(1);
    free(bg_cmd_grp_queue);
}



int main(int argc, char **argv)
{
    sh_loop();
    return 0;
}







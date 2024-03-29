#include "command.h"
/**
 * Functions responsible for controlling the event loop of the shell, involving prompting, parsing, and executing
 */


/**
 ************************************************************************************
 ********************************* Helper Functions *********************************
 ************************************************************************************
 */


/**
 * _print_args - prints each element in 2d array
 */
void _print_args(char** args);




/**
 * _is_well_formed - returns whether a command is of valid form, i.e. no parsing errors
 * @args: the parsed array of args
 * NOTE: Parsing errors include redirects with no corresponding file, pipes with no corresponding commands,
 *       and ampersands in the middle of a command, e.g. "| ls", "ls >", "ls & |"
 */
bool _is_well_formed(char **args);


/**
 * _is_evn_variable - returns whether a token is referencing an environment variable
 * NOTE: this function only deals with environment variables at the start of a token
 * e.g. ls $HOME or ls $PWD/<somedir>
 */
bool _contains_env_variable(char* tok);


/**
 * _get_env_var_len - return the length of the env var including the '$'
 */
int _get_env_var_len(char* tok);


/**
 * _is_path_variable - returns whether a token is a path to some file
 */
bool _is_path_variable(char* tok);


/**
 * _is_builtin_cmd - returns whether a command is a builtin
 * echo, etime, exit, io
 */
bool _is_builtin_cmd(char *tok);


/**
 * _expand_external_command - resolves relative path or searches $PATH to get absolute path for external commands
 * e.g. "python" -> "/usr/bin/python"
 */
char *_expand_external_command(char *arg);


/**
 * _is_command - returns whether the `i`th arg in `args` is a command
 * @args: array of tokenized strings
 * @i: the argument to check
 * @returns: 0-arg, 1-cd, 2-built-in command, 3-external command
 */
int _is_command(char **args, int i);




/**
 * _resolve_path - resolves all '.'s, '..s', and leading '~' in a path
 * @path: path to be resolved
 * @return: the resolved path, NULL if failed allocation or not valid path
 */
char *_resolve_path(char *path);


/**
 * _is_regular_file - returns whether file is regular file, rather than directory file
 */
int _is_regular_file(const char *path);

/**
 * _match_path - finds first matching directory in the $PATH that contains `executable`
 * @executable - the name of the executable file
 * @return: the absolute path to the executable, NULL if no match or no execute permissions on match(s)
 */
char *_match_path(char *executable);


/**
 ************************************************************************************
 ********************************* Shell Functions **********************************
 ************************************************************************************
 */


/**
 * sh_read_line - read shell user input from stdin
 * @return: char * to beginning of line
 * Assumption: The user input will be no more than 255 characters
 */
char *sh_read_line();


/**
 * sh_add_whitespace - adds whitespace around chars, so it can later be tokenized
 * @line: command line input
 * @char: the chars to add whitespace around, (<, >, |, &) usually
 * @return: copy of the line with added whitespace
 * e.g. 'ls -al|grep me>outfile <infile' --> 'ls -al | grep me > outfile  <infile'
 */
char *sh_add_whitespace(char *line, const char *chars);


/**
 * sh_parse_line - parse a line into tokens for future execution
 * @line: Line to be parsed
 * @return: array of tokens
 */
char **sh_parse_line(char *line);


/**
 * sh_expand_env_vars - expand environment variables in args
 * @args: array of char* denoting the arguments
 * @returns: copy of args with environment variables expanded
 */
char **sh_expand_env_vars(char** args);


/**
 * sh_expand_paths - expands the paths of commands to their absolute path
 * @args: array of char* denoting the arguments
 * @returns: copy of args with all the paths expanded to absolute paths
 */
char **sh_expand_paths(char** args);


/**
 * eq_current_size - returns the number of CommandGroups in the execution queue
 */
size_t eq_current_size(CommandGroup** bg_cmd_grp_queue);


/**
 * eq_remove_and_free - remove the cmd_grp at position i, and shift every cmd_grp after it left
 */
void eq_remove_and_free(CommandGroup** bg_cmd_grp_queue, int i);


/**
 * eq_append - append CommandGroup to the queue
 */
void eq_append(CommandGroup** bg_cmd_grp_queue, CommandGroup* cmd_grp);


/**
 * eq_remove_pid - find the CommandGroup in the queue that owns the pid and removes it from there
 * NOTE: this funciton will free the CommandGroup if it no longer owns any pids
 */
void eq_remove_pid(CommandGroup** bg_cmd_grp_queue, pid_t pid);


/**
 * sh_reap_zombies - get all terminated background child process pids, reap them, and print out indicating they finished
 * @bg_cmd_grp_queue: the queue of all current cmd_grps who still have unreaped pids
 */
void sh_reap_zombies(CommandGroup** bg_cmd_grp_queue);


/**
 * sh_prompt - prompt user with '$USER@$MACHINE :: $PWD =>'
 */
void sh_prompt();


/**
 * sh_loop - loop grabbing commands from the user and executing them
 */
void sh_loop();






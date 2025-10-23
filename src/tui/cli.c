/*
 * Copyright (C) 2025 Sergio Gómez Del Real
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "cli.h"

#include "client.h"

struct cli_window wcli;

#define CLI_MAX_INPUT_SIZE 256

list_t *cmd_list;

static int str_command_get_argc_argv(const char *str, char ***argv) {
	// how many arguments?
	int num_args = 1;
	for (size_t i = 0; i < strlen(str); i++) {
		if (isblank(str[i]))
			num_args++;
		if (str[i] == '\0')
			break;
	}

	*argv = malloc(num_args * sizeof(char *));
	if (!*argv) {
		perror("malloc()");
		num_args = 0;
		goto out;
	}
	char *last_ptr = str;
	for (size_t i = 0, j = 0; i < strlen(str)+1; i++) {
		if (isblank(str[i]) || str[i] == '\0') {
			size_t size = (&str[i] - last_ptr);
			char *ptr = malloc(size+1);
			if (!ptr) {
				perror("malloc()");
				num_args = 0;
				goto out;
			}
			strncpy(ptr, last_ptr, size);
			ptr[size] = '\0';
			(*argv)[j++] = ptr;
			last_ptr = &str[i+1];
		}
	}

out:
	return num_args;
}

// caller owns the allocation
static struct cmd *new_cmd(const char *str) {
	struct cmd *c = malloc(sizeof(*c));
	if (!c) {
		perror("malloc()");
		goto out;
	}

	int argc;
	char **argv;
	argc = str_command_get_argc_argv(str, &argv);
	c->argc = argc;
	c->argv = argv;
out:
	return c;
}

static char str[256] = {};
static int str_len = 0;

void cli_redraw() {
	if (wcli.current_pos_x == (wcli.max_x-1)) {
		wcli.current_pos_x = 1;
		wcli.current_pos_y++;;
	}
	if (wcli.current_pos_y == (wcli.max_y-1)) {
		wcli.current_pos_y -= 1;
		scroll(wcli.win);
	}
	if (!str_len) {
		mvwaddstr(wcli.win, wcli.current_pos_y, 1, "> ");
	}
	wmove(wcli.win, wcli.current_pos_y, wcli.current_pos_x);
	wborder(wcli.win, ACS_CKBOARD, ACS_CKBOARD, ACS_CKBOARD, ACS_CKBOARD, ACS_CKBOARD, ACS_CKBOARD, ACS_CKBOARD, ACS_CKBOARD);
	wrefresh(wcli.win);
}

static void cli_parse(int ch, struct cmd **cmd_out) {
	*cmd_out = NULL;

	if (str_len < 255 && (isalnum(ch) || isblank(ch))) {
		str[str_len++] = ch;
		wcli.current_pos_x++;
	}
	else if (ch == '\n') {
		struct cmd *c = {};
		if (str_len) {
			c = new_cmd(str);
			list_add(cmd_list, (uintptr_t)c); // the list now owns cmd
			str_len = 0;
			memset(str, 0, sizeof(str));
			*cmd_out = c; // immutable borrow
		}
		wcli.current_pos_y++;
		wcli.current_pos_x = strlen("> ")+1;;
	}
	else if (ch == '\b') {
		if (str_len > 0) {
			str[--str_len] = ' ';
		}
	}
	cli_redraw();
}

static uint16_t str_to_addr(const char *str) {
	uint16_t addr = 0;
	for (size_t i = 0; i < strlen(str); i++) {
		if (str[i] >= '0' && str[i] <= '9') {
			addr |= (str[i]-0x30) << ((strlen(str)-1-i) * 4);
		}
		else {
			addr |= (str[i]-0x57) << ((strlen(str)-1-i) * 4);
		}
	}

	return addr;
}

static void handle_breakpoint(const struct cmd *cmd) {
	uint16_t addr = 0;
	char *str = cmd->argv[1];
	if (str[0] == '0' && str[1] == 'x')
		str += 2;

	addr = str_to_addr(str);
	client_set_breakpoint(addr);
}

static void handle_until(const struct cmd *cmd) {
	uint16_t addr = 0;
	char *str = cmd->argv[1];
	if (str[0] == '0' && str[1] == 'x')
		str += 2;

	addr = str_to_addr(str);
	client_control_flow_until(addr);
}

static void handle_delete(const struct cmd *cmd) {
	uint16_t addr = 0;
	char *str = cmd->argv[1];
	if (str) {
		if (str[0] == '0' && str[1] == 'x')
			str += 2;
		addr = str_to_addr(str);
	}

	client_unset_breakpoint(addr);
}

static void handle_continue(const struct cmd *cmd) {
	client_control_flow_continue();
}

static bool parse_cmd(const struct cmd *command) {
	if (!strcmp(command->argv[0], "break") || !strcmp(command->argv[0], "b")) {
		handle_breakpoint(command);
	}
	else if (!strcmp(command->argv[0], "until")) {
		handle_until(command);
	}
	else if (!strcmp(command->argv[0], "c") || !strcmp(command->argv[0], "cont") ||
			!strcmp(command->argv[0], "continue")) {
		handle_continue(command);
	}
	else if (!strcmp(command->argv[0], "delete") || !strcmp(command->argv[0], "d")) {
		handle_delete(command);
	}
	else {
		return false;
	}
	return true;
}

void cli_window_handle_input(int ch) {
	struct cmd *curr_cmd;

	cli_parse(ch, &curr_cmd);
	if (!curr_cmd) {
		return;
	}
	curr_cmd->valid = parse_cmd(curr_cmd);
}

WINDOW *cli_init() {
	cmd_list = create_list();

	wcli.win = newwin(LINES/3, COLS/2, LINES-(LINES/3), 0);
	wcli.current_pos_y = 1;
	wcli.current_pos_x = sizeof("> ");;
	getmaxyx(wcli.win, wcli.max_y, wcli.max_x);
	scrollok(wcli.win, TRUE);

	return wcli.win;
}

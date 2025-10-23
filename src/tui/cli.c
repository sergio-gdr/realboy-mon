/* RealBoy Debugger.
 * Copyright (C) 2025 Sergio Gómez Del Real <sgdr>
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

struct cli_window wcli;

#define CLI_MAX_INPUT_SIZE 256

list_t *cmd_list;

static int str_command_get_argc_argv(char *str, char ***argv) {
	// how many arguments?
	int num_args = 1;
	for (size_t i = 0; i < strlen(str); i++) {
		if (isblank(str[i]))
			num_args++;
		if (str[i] == '\0')
			break;
	}

	*argv = malloc(num_args * sizeof(char *));
	char *last_ptr = str;
	for (size_t i = 0, j = 0; i < strlen(str)+1; i++) {
		if (isblank(str[i]) || str[i] == '\0') {
			size_t size = (&str[i] - last_ptr);
			char *ptr = malloc(size+1);
			strncpy(ptr, last_ptr, size);
			ptr[size] = '\0';
			(*argv)[j++] = ptr;
			last_ptr = &str[i+1];
		}
	}

	return num_args;
}

// caller owns the allocation
static struct cmd *new_cmd(char *str) {
	struct cmd *c = malloc(sizeof(*c));
	int argc;
	char **argv;
	argc = str_command_get_argc_argv(str, &argv);

	c->argc = argc;
	c->argv = argv;

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

void cli_parse(int ch, struct cmd **cmd_out) {
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

WINDOW *cli_init() {
	cmd_list = create_list();
	wcli.current_pos_y = 1;
	wcli.current_pos_x = sizeof("> ");;
	wcli.win = newwin(LINES/3, COLS/2, LINES-(LINES/3), 0);
	scrollok(wcli.win, TRUE);
	getmaxyx(wcli.win, wcli.max_y, wcli.max_x);
	return wcli.win;
}

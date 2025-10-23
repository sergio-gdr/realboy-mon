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

#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <ncurses.h>
#include <libemu.h>

#include "cli.h"
#include "client.h"

#include "disasm.h"

struct wsrc_instr {
	struct instruction *instr;
	bool is_highlighted;
	bool is_current;
};

struct source_window {
	WINDOW *win;
	int max_y, max_x;
	int current_pos_y;
	uint32_t longest_str_size;

	struct instruction current_highlight;
	struct wsrc_instr current_instr;
	list_t *instrs;
};

typedef struct {
	struct source_window src_window;
	WINDOW *cli_window;
	WINDOW *reg_window;
	WINDOW *focus_window;
	WINDOW *help_window;
	WINDOW *misc_window;
} tui_t;
tui_t tui;

static void handle_control_flow_until(uint32_t addr) {

}

static void handle_control_flow_break(uint32_t addr) {

}

static char *reg_to_str(uint32_t reg) {
	char *paytostr = malloc(15);
	if (paytostr) {
		if (reg < 0x10)
			sprintf(paytostr, "0x000%x", reg);
		else if (reg < 0x100)
			sprintf(paytostr, "0x00%x", reg);
		else if (reg < 0x1000)
			sprintf(paytostr, "0x0%x", reg);
		else
			sprintf(paytostr, "0x%x", reg);
	}
	return paytostr;
}

static char *handle_get_instr_at_addr(uint32_t *instr, uint32_t size) {
	return disasm(instr, size);
}

struct dispatch_table disp = {
	.handle_control_flow_break = handle_control_flow_break,
	.handle_control_flow_until = handle_control_flow_until,
	.handle_get_instr_at_addr = handle_get_instr_at_addr,
};

static void change_focus() {
	if (tui.focus_window == tui.src_window.win) {
		wborder(tui.focus_window, 0, 0, 0, 0, 0, 0, 0, 0);
		wrefresh(tui.focus_window);
		tui.focus_window = tui.cli_window;
		wborder(tui.focus_window, ACS_CKBOARD, ACS_CKBOARD, ACS_CKBOARD, ACS_CKBOARD, ACS_CKBOARD, ACS_CKBOARD, ACS_CKBOARD, ACS_CKBOARD);
		wrefresh(tui.focus_window);
		cli_redraw();
		curs_set(1);
		echo();
	}
	else if (tui.focus_window == tui.cli_window) {
		wborder(tui.focus_window, 0, 0, 0, 0, 0, 0, 0, 0);
		wrefresh(tui.focus_window);
		tui.focus_window = tui.reg_window;
		wborder(tui.focus_window, ACS_CKBOARD, ACS_CKBOARD, ACS_CKBOARD, ACS_CKBOARD, ACS_CKBOARD, ACS_CKBOARD, ACS_CKBOARD, ACS_CKBOARD);
		wrefresh(tui.focus_window);
		curs_set(0);
		noecho();
	}
	else {
		wborder(tui.focus_window, 0, 0, 0, 0, 0, 0, 0, 0);
		wrefresh(tui.focus_window);
		tui.focus_window = tui.src_window.win;
		wborder(tui.focus_window, ACS_CKBOARD, ACS_CKBOARD, ACS_CKBOARD, ACS_CKBOARD, ACS_CKBOARD, ACS_CKBOARD, ACS_CKBOARD, ACS_CKBOARD);
		wrefresh(tui.focus_window);
	}
}

static inline void refresh_all() {
	touchwin(tui.reg_window);
	touchwin(tui.cli_window);
	touchwin(tui.src_window.win);
	touchwin(tui.help_window);
	wrefresh(tui.reg_window);
	wrefresh(tui.cli_window);
	wrefresh(tui.src_window.win);
	wrefresh(tui.help_window);
}

static void sigint_handler(int _) {
	if (client_is_server_executing()) {
		client_stop_server();
		return;
	}

	static bool press_twice;
	if (tui.focus_window == tui.cli_window) {
		return;
	}

	if (press_twice) {
		endwin();
		exit(0);
	}

	const char *exit_monitor_str = "send ctrl+c again to exit monitor";
	wborder(tui.misc_window, 0, 0, 0, 0, 0, 0, 0, 0);
	int max_x, max_y;
   	getmaxyx(tui.misc_window, max_y, max_x);
	mvwaddstr(tui.misc_window, max_y/2, max_x/2 - strlen(exit_monitor_str)/2, exit_monitor_str);
	wrefresh(tui.misc_window);

	press_twice = true;
	sleep(3);
	press_twice = false;
	wclear(tui.misc_window);
	wrefresh(tui.misc_window);

	refresh_all();
}

static bool is_current_instr_in_instrs() {
	uintptr_t uiptr_instr;
	list_for_each(tui.src_window.instrs, uiptr_instr) {
		struct wsrc_instr *in = (struct wsrc_instr *)uiptr_instr;
		if (in->instr->addr == tui.src_window.current_instr.instr->addr) {
			return true;
		}
	}
	return false;
}

static void redraw_reg_window() {
	enum cpu_reg reg_enum = CPU_REG_AF;
	const char *cpu_regs[] = { "AF: ", "BC: ", "DE: ", "HL: ", "SP: ", "PC: " };

	for (size_t i = 0; i < sizeof(cpu_regs)/sizeof(char *); i++) {
		char *cpu_reg_str = reg_to_str(client_get_cpu_reg(reg_enum++)); // we own
		char buf[sizeof(cpu_regs)+6] = {};
		strcpy(buf, cpu_regs[i]);
		strcpy(buf + strlen(cpu_regs[i]), cpu_reg_str);

		mvwaddstr(tui.reg_window, i+1, 1, buf);
		wrefresh(tui.reg_window);
		free(cpu_reg_str);
	}

	char *ppu_reg_str = reg_to_str(client_get_ppu_reg(PPU_REG_LY)); // we own
	const char *s = "ly: ";
	char buf[sizeof(cpu_regs)+6] = {};
	strcpy(buf, s);
	strcpy(buf+strlen(s), ppu_reg_str);

	mvwaddstr(tui.reg_window, (sizeof(cpu_regs)/sizeof(*cpu_regs))+1, 1, buf);
	wrefresh(tui.reg_window);
	free(ppu_reg_str);
}

static list_t *get_instrs(uint16_t start_addr) {
	list_t *instr_list = create_list(); // we own
	if (!instr_list)
		goto err1;

	struct wsrc_instr *wsrc_instr;
	int curr_addr = start_addr;
	int num_instrs = tui.src_window.max_y-2;
	for (int i = 0; i < num_instrs; i++) {
		wsrc_instr = calloc(1, sizeof(*wsrc_instr));
		if (!wsrc_instr) {
			perror("calloc()");
			goto err2;
		}
		wsrc_instr->instr = client_get_instruction(curr_addr);
		if (!wsrc_instr->instr) {
			goto err2;
		}
		wsrc_instr->is_highlighted = false;
		list_add(instr_list, (uintptr_t)wsrc_instr);
		curr_addr += wsrc_instr->instr->len;
	}

	return instr_list;
err2:
	{
	uintptr_t instr;
	list_for_each(instr_list, instr) {
		free(((struct wsrc_instr *)instr)->instr);
	}
	list_free(instr_list);
	}
err1:
	return NULL;
}

static uint32_t get_pc() {
	return client_get_cpu_reg(CPU_REG_PC);
}

static struct instruction *get_current_instr() {
	uint16_t pc = get_pc();
	return client_get_instruction(pc);
}

static void wsrc_redraw(uint32_t addr) {
	struct source_window *wsrc = &tui.src_window;

	free(wsrc->instrs);
	wsrc->instrs = get_instrs(addr); // we own this list

	wclear(wsrc->win);
	if (wsrc->win == tui.focus_window)
		wborder(wsrc->win, ACS_CKBOARD, ACS_CKBOARD, ACS_CKBOARD, ACS_CKBOARD, ACS_CKBOARD, ACS_CKBOARD, ACS_CKBOARD, ACS_CKBOARD);
	else
		wborder(wsrc->win, 0, 0, 0, 0, 0, 0, 0, 0);
	wrefresh(wsrc->win);

	wsrc->longest_str_size = 0;

	// fill the source window with instructions
	uintptr_t uiptr_instr;
	int i = 0;
	list_for_each(wsrc->instrs, uiptr_instr) {
		struct wsrc_instr *in = (struct wsrc_instr *)uiptr_instr;
		mvwaddstr(wsrc->win, i+1, wsrc->max_x/2, reg_to_str(in->instr->addr));
		mvwaddstr(wsrc->win, i+1, (wsrc->max_x/2)+7, in->instr->str);
		i++;
	}
	wrefresh(wsrc->win);
}

static void wsrc_highlight_instr(uint32_t addr) {
	struct source_window *wsrc = &tui.src_window;
	uintptr_t uiptr_instr;
	int i = 0;
	list_for_each(wsrc->instrs, uiptr_instr) {
		struct wsrc_instr *in = (struct wsrc_instr *)uiptr_instr;
		if (in->is_highlighted) {
			wattroff(wsrc->win, A_REVERSE);
			mvwaddstr(wsrc->win, i+1, wsrc->max_x/2, reg_to_str(in->instr->addr));
			mvwaddstr(wsrc->win, i+1, (wsrc->max_x/2)+7, in->instr->str);
			in->is_highlighted = false;
		}
		if (in->instr->addr == addr) {
			wattron(wsrc->win, A_REVERSE);
			mvwaddstr(wsrc->win, i+1, wsrc->max_x/2, reg_to_str(in->instr->addr));
			mvwaddstr(wsrc->win, i+1, (wsrc->max_x/2)+7, in->instr->str);
			wattroff(wsrc->win, A_REVERSE);
			wsrc->current_highlight = *in->instr;
			in->is_highlighted = true;
		}
		i++;
	}
}

static void wsrc_move(bool up_down) {
	struct source_window *wsrc = &tui.src_window;

	if ((wsrc->current_pos_y == 1 && up_down == 0) ||
			(wsrc->current_pos_y == wsrc->max_y-2 && up_down == 1)) {
		// XXX this algorithm for getting the previous instructions is wrong.
		// it is harder than it seems, since any byte may contain either an instruction or
		// a memory address.
		if (up_down == 0) {
			struct instruction *first_instr = ((struct wsrc_instr*)wsrc->instrs->items[0])->instr;
			uint32_t target_addr;
			if (first_instr->addr == 0)
				return;
			if (first_instr->addr == 1) {
				target_addr = 0;
			}
			if (first_instr->addr == 2) {
				struct instruction *instr = client_get_instruction(first_instr->addr-2);
				if (instr->len == 2)
					target_addr = 0;
				else
					target_addr = 1;
				free(instr);
			}
			else {
				struct instruction *instr = client_get_instruction(first_instr->addr-3);
				if (instr->len == 3)
					target_addr = first_instr->addr-3;
				else {
					free(instr);
					instr = client_get_instruction(first_instr->addr-2);
					if (instr->len == 2)
						target_addr = first_instr->addr-2;
					else
						target_addr = first_instr->addr-1;
				}
				free(instr);
			}
			wsrc_redraw(target_addr);
			wsrc_highlight_instr(target_addr);
		}
		else {
			struct instruction *first_instr = ((struct wsrc_instr*)wsrc->instrs->items[0])->instr;
			wsrc_redraw(first_instr->addr+first_instr->len);
			wsrc_highlight_instr(((struct
				wsrc_instr*)wsrc->instrs->items[wsrc->current_pos_y-1])->instr->addr);
		}
		return;
	}

	wsrc->current_pos_y = up_down == 0 ? wsrc->current_pos_y-1 : wsrc->current_pos_y+1;
	wsrc->current_highlight = *((struct wsrc_instr*)wsrc->instrs->items[wsrc->current_pos_y-1])->instr;
	wsrc_highlight_instr(((struct wsrc_instr
		*)wsrc->instrs->items[wsrc->current_pos_y-1])->instr->addr);
}

static void init_wins() {
	// source window
	wborder(tui.src_window.win, ACS_CKBOARD, ACS_CKBOARD, ACS_CKBOARD, ACS_CKBOARD, ACS_CKBOARD, ACS_CKBOARD, ACS_CKBOARD, ACS_CKBOARD);
	wrefresh(tui.src_window.win);
	tui.src_window.current_pos_y = 1;
	keypad(tui.src_window.win, true);

	// register window
	wborder(tui.reg_window, 0, 0, 0, 0, 0, 0, 0, 0);
	wrefresh(tui.reg_window);

	// command-line interface window
	wborder(tui.cli_window, 0, 0, 0, 0, 0, 0, 0, 0);
	mvwaddstr(tui.cli_window, 1, 1, "> ");
	wrefresh(tui.cli_window);

	// help window
	wborder(tui.help_window, 0, 0, 0, 0, 0, 0, 0, 0);
	mvwaddstr(tui.help_window, 1, 1, "SHORTCUTS");
	mvwaddstr(tui.help_window, 2, 2, "Ctrl+C (twice): close debugger");
	mvwaddstr(tui.help_window, 3, 2, "TAB: change window focus");
	mvwaddstr(tui.help_window, 4, 2, "j/k: vim-style up and down");
	wrefresh(tui.help_window);
}

static void wsrc_set_curr_instr(uint32_t addr) {
	struct source_window *wsrc = &tui.src_window;

	free(tui.src_window.current_instr.instr);
	wsrc->current_instr.instr = client_get_instruction(addr);

	if (!is_current_instr_in_instrs())
		wsrc_redraw(wsrc->current_instr.instr->addr);

	uintptr_t uiptr_instr;
	int i = 0;
	list_for_each(tui.src_window.instrs, uiptr_instr) {
		struct wsrc_instr *in = (struct wsrc_instr *)uiptr_instr;
		if (in->is_current) {
			in->is_current = false;
			mvwaddch(wsrc->win, i+1, (wsrc->max_x/2)-2, ' ');
		}
		if (in->instr->addr == addr) {
			in->is_current = true;
			mvwaddch(wsrc->win, i+1, (wsrc->max_x/2)-2, ACS_DIAMOND);
			wsrc->current_pos_y = i+1;
		}
		i++;
	}
}

static void halt_and_wait() {
	const char *stop_server_str = "emulator is executing. ctrl+c to stop execution.";
	wborder(tui.misc_window, 0, 0, 0, 0, 0, 0, 0, 0);
	int max_x, max_y;
	getmaxyx(tui.misc_window, max_y, max_x);
	mvwaddstr(tui.misc_window, max_y/2, max_x/2 - strlen(stop_server_str)/2, stop_server_str);
	wrefresh(tui.misc_window);

	client_recv_msg_and_dispatch(true);

	wclear(tui.misc_window);
	wrefresh(tui.misc_window);
	refresh_all();

	wsrc_set_curr_instr(get_pc());
	wsrc_highlight_instr(tui.src_window.current_instr.instr->addr);
	redraw_reg_window();
}

static void do_control_flow_next() {
	struct source_window *wsrc = &tui.src_window;

	client_control_flow_next();
	wsrc_set_curr_instr(get_pc());
	wsrc_highlight_instr(wsrc->current_instr.instr->addr);
	redraw_reg_window();
}

static void wsrc_handle_input(int input_char) {
	struct source_window *wsrc = &tui.src_window;

	if (input_char == 'j' || input_char == 'k' ||
			input_char == KEY_PPAGE || input_char == KEY_NPAGE) {
		int num_moves = input_char == 'j' || input_char == 'k' ? 1 : 6;
		while (num_moves--)
			wsrc_move(input_char == 'j' || input_char == KEY_NPAGE ? 1 : 0);
	}
	else if (input_char == '\n') {
		// either target the highlighted instruction if the user selected one in the TUI, or
		// else just target the next instruction.
		if (wsrc->current_highlight.addr != wsrc->current_instr.instr->addr) {
			client_control_flow_until(wsrc->current_highlight.addr);
		}
		else {
			do_control_flow_next();
		}
	}
}

static void interpret_input(int input_char) {
	if (tui.focus_window == tui.cli_window) {
		cli_window_handle_input(input_char);
	}
	else if (tui.focus_window == tui.src_window.win) {
		wsrc_handle_input(input_char);
	}
}

int tui_run() {
	noecho();
	curs_set(0);

	// we handle SIGINT so that the user can ctrl+c to quit
	struct sigaction sig = { .sa_handler = sigint_handler, .sa_flags = SA_NODEFER };
	sigaction(SIGINT, &sig, NULL);

	// send a MONITOR_STOP message to server
	client_stop_server();

	tui.src_window.current_instr.instr = get_current_instr();
	tui.src_window.current_highlight = *tui.src_window.current_instr.instr;

	wsrc_redraw(0);
	wsrc_set_curr_instr(get_pc());
	wsrc_highlight_instr(tui.src_window.current_instr.instr->addr);
	redraw_reg_window();

	while (1) {
		if (client_is_server_executing()) {
			halt_and_wait();
		}

		// we parse on a char-by-char basis
		int input_char = wgetch(tui.focus_window);

		// TAB changes the focused window
		if (input_char == '\t') {
			change_focus();
			continue;
		}
		else {
			interpret_input(input_char);
		}
	}

	endwin();
	return(0);
}

struct dispatch_table *tui_init() {
	initscr();

	if (!(tui.cli_window = cli_init())) {
		goto err;
	}

	if (!(tui.src_window.win = newwin((LINES/3)*2+(LINES%3), COLS/2, 0, 0))) {
		goto err;
	}
	getmaxyx(tui.src_window.win, tui.src_window.max_y, tui.src_window.max_x);

	if (!(tui.reg_window = newwin((LINES/3)*2+(LINES%3), COLS/2, 0, COLS/2))) {
		perror("newwin()");
		goto err;
	}
	if (!(tui.help_window = newwin(LINES/3, COLS/2, LINES-(LINES/3), COLS/2))) {
		perror("newwin()");
		goto err;
	}

	// this window is used as a popup to display messages
	if (!(tui.misc_window = newwin(5, COLS/3, LINES/3, COLS/3))) {
		perror("newwin()");
		goto err;
	}

	init_wins();

	// initially the focus is on the src window
	tui.focus_window = tui.src_window.win;

	return &disp;
err:
	return NULL;
}

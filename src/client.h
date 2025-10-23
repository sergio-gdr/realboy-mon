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

#ifndef CLIENT_H
#define CLIENT_H

#include <libemu.h>

struct instruction {
	uint16_t addr;
	uint32_t len;
	char *str;
};

struct dispatch_table {
	void (*handle_control_flow_until)(uint32_t addr);
	void (*handle_control_flow_break)(uint32_t addr);
	char *(*handle_print_addr)(uint32_t addr);
	char *(*handle_get_cpu_reg)(uint32_t cpu_reg);
	char *(*handle_get_ppu_reg)(uint32_t ppu_reg);
	char *(*handle_get_instr_at_addr)(uint32_t *instr, uint32_t size);
};
int client_init(const struct dispatch_table *disp);

uint32_t client_get_cpu_reg(enum cpu_reg reg);
uint32_t client_get_ppu_reg(enum ppu_reg reg);
struct instruction *client_get_instruction(uint32_t addr);

void client_control_flow_until(uint32_t addr);
void client_control_flow_continue();
void client_control_flow_next();

void client_recv_msg_and_dispatch(bool wait);
void client_set_breakpoint(uint32_t addr);
void client_unset_breakpoint(uint32_t addr);
void client_stop_server();
void client_resume_server();
bool client_is_server_executing();

#endif

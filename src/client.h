/* RealBoy Debugger.
 * Copyright (C) 2013-2025 Sergio Gómez Del Real <sgdr>
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
	size_t len;
	char *str;
};

int client_init();
char *client_get_cpu_reg(enum cpu_reg reg);
char *client_get_ppu_reg(enum ppu_reg reg);
uint32_t client_get_pc();
struct instruction *client_get_instr_at_addr(uint16_t addr);

void client_control_flow_until(uint16_t addr);
void client_control_flow_continue();

void client_advance_instr();
void client_set_breakpoint(uint16_t addr);
void client_unset_breakpoint(uint16_t addr);

#endif

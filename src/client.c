/*
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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <libemu.h>

#include "client.h"

#include "disasm.h"

static char *handle_get_ppu_reg(const struct msg *reply) {
	uint8_t reg = reply->payload;
	char *paytostr = malloc(20);
	if (paytostr) {
		if (reg < 0x10)
			sprintf(paytostr, "0x0%hx", (uint8_t)reg);
		else
			sprintf(paytostr, "0x%hx", (uint8_t)reg);
	}

	return paytostr;
}

static char *handle_get_cpu_reg(const struct msg *reply) {
	uint32_t addr = reply->payload;
	char *paytostr = malloc(15);
	if (paytostr) {
		if (addr < 0x10)
			sprintf(paytostr, "0x000%x", addr);
		else if (addr < 0x100)
			sprintf(paytostr, "0x00%x", addr);
		else if (addr < 0x1000)
			sprintf(paytostr, "0x0%x", addr);
		else
			sprintf(paytostr, "0x%x", addr);
	}
	return paytostr;
}

static char *handle_print_addr(const struct msg *reply) {
	uint32_t payload = reply->payload;
	char *paytostr = malloc(5);
	if (payload)
		snprintf(paytostr, 4, "0x%hx", payload);
	return paytostr;
}

static char *handle_get_instr_at_addr(const struct msg *reply) {
	uint32_t *instr = reply->hdr.size == sizeof(uint32_t) ? (uint32_t *)&reply->payload : (uint32_t *)reply->payload;
	size_t size = reply->hdr.size/sizeof(uint32_t);
	return disasm(instr, size);
}

static char *dispatch(const struct msg *reply) {
	switch (reply->hdr.type) {
		case TYPE_INSPECT:
			switch (reply->hdr.subtype.inspect) {
				case INSPECT_PRINT_ADDR:
					return handle_print_addr(reply);
				case INSPECT_GET_CPU_REG:
					return handle_get_cpu_reg(reply);
				case INSPECT_GET_PPU_REG:
					return handle_get_ppu_reg(reply);
				case INSPECT_GET_INSTR_AT_ADDR:
					return handle_get_instr_at_addr(reply);
				default:
					return NULL;
			}
			break;
		default:
			return NULL;
	}
}

int client_init() {
	return emu_init_monitor(false, NULL);
}

static int send_req_and_recv_reply(const struct msg *req, struct msg *reply) {
	int ret;
	if ((ret = ipc_send_msg(req)) != -1) {
		ret = ipc_recv_msg(reply);
	}
	return ret;
}

struct instruction *client_get_instr_at_addr(uint16_t addr) {
	struct msg req = (struct msg){
		.hdr.type = TYPE_INSPECT,
		.hdr.subtype.inspect = INSPECT_GET_INSTR_AT_ADDR,
		.hdr.size = 4,
		.payload = addr
	};
	struct msg reply = (struct msg){};
	send_req_and_recv_reply(&req, &reply);
	char *str_reply = dispatch(&reply);
	if (!str_reply) {
		return NULL;
	}
	char *disasm = malloc(strlen(str_reply)+20);
	sprintf(disasm, "0x%x: %s", addr, str_reply);

	req = (struct msg){
		.hdr.type = TYPE_INSPECT,
		.hdr.subtype.inspect = INSPECT_GET_OP_LEN,
		.hdr.size = 4,
		.payload = reply.hdr.size == sizeof(uint32_t) ? reply.payload : *(uint32_t *)reply.payload
	};
	reply = (struct msg){};
	send_req_and_recv_reply(&req, &reply);

	if (reply.hdr.size > sizeof(uint32_t))
		free((void *)reply.payload);

	// caller owns
	struct instruction *instr = malloc(sizeof(*instr));
	instr->addr = addr;
	instr->len = reply.payload;
	instr->str = disasm;
	return instr;
}

uint32_t client_get_pc() {
	struct msg req = (struct msg){
		.hdr.type = TYPE_INSPECT,
		.hdr.subtype.inspect = INSPECT_GET_CPU_REG,
		.hdr.size = 4,
		.payload = CPU_REG_PC
	};
	struct msg reply = {};
	send_req_and_recv_reply(&req, &reply);

	return reply.payload;
}

char *client_get_ppu_reg(enum ppu_reg reg) {
	struct msg req = (struct msg){
		.hdr.type = TYPE_INSPECT,
		.hdr.subtype.inspect = INSPECT_GET_PPU_REG,
		.hdr.size = 4,
		.payload = reg
	};
	struct msg reply = {};
	send_req_and_recv_reply(&req, &reply);
	char *reg_str = dispatch(&reply);

	return reg_str;
}

char *client_get_cpu_reg(enum cpu_reg reg) {
	struct msg req = (struct msg){
		.hdr.type = TYPE_INSPECT,
		.hdr.subtype.inspect = INSPECT_GET_CPU_REG,
		.hdr.size = 4,
		.payload = reg
	};
	struct msg reply = {};
	send_req_and_recv_reply(&req, &reply);
	char *reg_str = dispatch(&reply);

	return reg_str;
}

void client_control_flow_until(uint16_t addr) {
	struct msg req = {
		.hdr.type = TYPE_CONTROL_FLOW,
		.hdr.subtype = CONTROL_FLOW_UNTIL,
		.hdr.size = 4,
		.payload = addr
	};
	struct msg reply;
	send_req_and_recv_reply(&req, &reply);
}

void client_advance_instr() {
	struct msg req = (struct msg){
		.hdr.type = TYPE_CONTROL_FLOW,
		.hdr.subtype.control_flow = CONTROL_FLOW_NEXT,
		.hdr.size = 0,
		.payload = 0
	};
	struct msg reply = {};
	send_req_and_recv_reply(&req, &reply);
}

void client_set_breakpoint(uint16_t addr) {
	struct msg req = (struct msg){
		.hdr.type = TYPE_CONTROL_FLOW,
		.hdr.subtype.control_flow = CONTROL_FLOW_BREAK,
		.hdr.size = 4,
		.payload = addr
	};
	struct msg reply = {};
	send_req_and_recv_reply(&req, &reply);
}

void client_unset_breakpoint(uint16_t addr) {
	struct msg req = (struct msg){
		.hdr.type = TYPE_CONTROL_FLOW,
		.hdr.subtype.control_flow = CONTROL_FLOW_DELETE,
		.hdr.size = 4,
		.payload = addr
	};
	struct msg reply = {};
	send_req_and_recv_reply(&req, &reply);
}

void client_control_flow_continue() {
	struct msg req = (struct msg){
		.hdr.type = TYPE_CONTROL_FLOW,
		.hdr.subtype.control_flow = CONTROL_FLOW_CONTINUE,
		.hdr.size = 0,
		.payload = 0
	};
	struct msg reply = {};
	send_req_and_recv_reply(&req, &reply);
}

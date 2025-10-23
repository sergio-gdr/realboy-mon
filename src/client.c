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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <libemu.h>

#include "client.h"

bool server_is_executing;

struct dispatch_table dispatch_table;

static int send_req_and_recv_reply(const struct msg *req, struct msg *reply) {
	int ret;
	if ((ret = emu_send_msg(req)) != -1) {
		if (reply)
			ret = emu_recv_msg(reply, true);
	}
	return ret;
}

static int send_req(const struct msg *req) {
	return send_req_and_recv_reply(req, NULL);
}

struct instruction *client_get_instruction(uint32_t addr) {
	struct msg req = (struct msg){
		.hdr.type = TYPE_INSPECT,
		.hdr.subtype.inspect = INSPECT_GET_INSTR_AT_ADDR,
		.hdr.size = 4,
		.payload = &addr
	};
	struct msg reply = (struct msg){};
	send_req_and_recv_reply(&req, &reply);
	char *disasm =
		dispatch_table.handle_get_instr_at_addr(reply.payload, reply.hdr.size);
	uint32_t op = *(uint32_t *)reply.payload;
	free(reply.payload);

	req = (struct msg){
		.hdr.type = TYPE_INSPECT,
		.hdr.subtype.inspect = INSPECT_GET_OP_LEN,
		.hdr.size = 4,
		.payload = &op
	};
	reply = (struct msg){};
	send_req_and_recv_reply(&req, &reply);

	// caller owns
	struct instruction *instr = malloc(sizeof(*instr));
	instr->addr = addr;
	instr->len = *(uint32_t*)reply.payload;
	instr->str = disasm;
	free((void *)reply.payload);
	return instr;
}

uint32_t client_get_ppu_reg(enum ppu_reg reg) {
	struct msg req = (struct msg){
		.hdr.type = TYPE_INSPECT,
		.hdr.subtype.inspect = INSPECT_GET_PPU_REG,
		.hdr.size = 4,
		.payload = &reg
	};
	struct msg reply = {};
	send_req_and_recv_reply(&req, &reply);
	uint32_t ppu_reg = *(uint32_t*)reply.payload;
	free(reply.payload);
	return ppu_reg;
}

uint32_t client_get_cpu_reg(enum cpu_reg reg) {
	struct msg req = (struct msg){
		.hdr.type = TYPE_INSPECT,
		.hdr.subtype.inspect = INSPECT_GET_CPU_REG,
		.hdr.size = 4,
		.payload = &reg
	};
	struct msg reply = {};
	send_req_and_recv_reply(&req, &reply);
	uint32_t cpu_reg = *(uint32_t*)reply.payload;
	free(reply.payload);
	return cpu_reg;
}

void client_control_flow_until(uint32_t addr) {
	struct msg req = {
		.hdr.type = TYPE_CONTROL_FLOW,
		.hdr.subtype = CONTROL_FLOW_UNTIL,
		.hdr.size = 4,
		.payload = &addr
	};
	send_req(&req);
	server_is_executing = true;
}

void client_recv_msg_and_dispatch(bool wait) {
	struct msg msg;
	emu_recv_msg(&msg, wait);

	switch (msg.hdr.type) {
		case TYPE_CONTROL_FLOW:
			switch (msg.hdr.subtype.control_flow) {
				case CONTROL_FLOW_UNTIL:
					dispatch_table.handle_control_flow_until(*(uint32_t*)msg.payload);
					server_is_executing = false;
					break;
				case CONTROL_FLOW_BREAK:
					dispatch_table.handle_control_flow_break(*(uint32_t*)msg.payload);
					server_is_executing = false;
					break;
				default:
					fprintf(stderr, "client_recv_msg_and_dispatch SUBTYPE\n");
			}
			break;
		default:
			fprintf(stderr, "client_recv_msg_and_dispatch TYPE\n");
	}
}

void client_control_flow_next() {
	struct msg req = (struct msg){
		.hdr.type = TYPE_CONTROL_FLOW,
		.hdr.subtype.control_flow = CONTROL_FLOW_NEXT,
		.hdr.size = 0,
		.payload = 0
	};
	send_req(&req);
}

void client_set_breakpoint(uint32_t addr) {
	struct msg req = (struct msg){
		.hdr.type = TYPE_CONTROL_FLOW,
		.hdr.subtype.control_flow = CONTROL_FLOW_BREAK,
		.hdr.size = 4,
		.payload = &addr
	};
	send_req(&req);
}

void client_unset_breakpoint(uint32_t addr) {
	struct msg req = (struct msg){
		.hdr.type = TYPE_CONTROL_FLOW,
		.hdr.subtype.control_flow = CONTROL_FLOW_DELETE,
		.hdr.size = addr ? 4 : 0,
		.payload = addr ? &addr : 0
	};
	send_req(&req);
}

void client_control_flow_continue() {
	struct msg req = (struct msg){
		.hdr.type = TYPE_CONTROL_FLOW,
		.hdr.subtype.control_flow = CONTROL_FLOW_CONTINUE,
		.hdr.size = 0,
		.payload = 0
	};
	send_req(&req);
	server_is_executing = true;
}

bool client_is_server_executing() {
	return server_is_executing;
}

void client_resume_server() {
	struct msg req = (struct msg){
		.hdr.type = TYPE_MONITOR,
		.hdr.subtype.monitor = MONITOR_RESUME,
		.hdr.size = 0,
		.payload = 0
	};
	send_req(&req);
	server_is_executing = true;
}

void client_stop_server() {
	struct msg req = (struct msg){
		.hdr.type = TYPE_MONITOR,
		.hdr.subtype.monitor = MONITOR_STOP,
		.hdr.size = 0,
		.payload = 0
	};
	send_req(&req);
	server_is_executing = false;
}

int client_init(const struct dispatch_table *disp) {
	dispatch_table = *disp;
	return emu_init(false);
}

/* (C) 2012 by Harald Welte <laforge@gnumonks.org>
 *
 * All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _UART_CMD_H
#define _UART_CMD_H

#include <stdint.h>
#include <stdarg.h>
#include <linuxlist.h>

enum cmd_op {
	CMD_OP_GET	= (1 << 0),
	CMD_OP_SET	= (1 << 1),
	CMD_OP_EXEC	= (1 << 2),
};

enum pstate {
	ST_IN_CMD,
	ST_IN_ARG,
};

struct strbuf {
	uint8_t idx;
	char buf[32];
};

struct cmd_state {
	struct strbuf cmd;
	struct strbuf arg;
	enum pstate state;
	void (*out)(const char *format, va_list ap);
};

int uart_cmd_out(struct cmd_state *cs, char *format, ...);
int uart_cmd_char(struct cmd_state *cs, uint8_t ch);
int uart_cmd_reset(struct cmd_state *cs);

struct cmd {
	const char *cmd;
	uint32_t ops;
	int (*cb)(struct cmd_state *cs, enum cmd_op op, const char *cmd,
		  int argc, char **argv);
	const char *help;
	/* put list at the end for simpler initialization */
	struct llist_head list;
};

void uart_cmd_register(struct cmd *c);
void uart_cmd_unregister(struct cmd *c);
void uart_cmds_register(struct cmd *c, unsigned int num);


#endif

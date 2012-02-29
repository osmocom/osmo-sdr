
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

#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <stdarg.h>

#include <uart_cmd.h>

void strbuf_reset(struct strbuf *sb)
{
	sb->idx = 0;
	memset(sb->buf, 0, sizeof(sb->buf));
}

void strbuf_append(struct strbuf *sb, char ch)
{
	if (sb->idx < sizeof(sb->buf)-1)
		sb->buf[sb->idx++] = ch;
}

static LLIST_HEAD(cmd_list);

void uart_cmd_register(struct cmd *c)
{
	llist_add_tail(&c->list, &cmd_list);
}

void uart_cmd_unregister(struct cmd *c)
{
	llist_del(&c->list);
}

void uart_cmds_register(struct cmd *c, unsigned int num)
{
	int i;

	for (i = 0; i < num; i++)
		uart_cmd_register(&c[i]);
}

#define CMD_MAX_ARGS	10

static int handle_cb(struct cmd_state *cs, int op, char *cmd, char *arg)
{
	struct cmd *c;
	int rc;
	char *argv[CMD_MAX_ARGS];
	int argc = 0;

	if (arg) {
		char *tok;
		/* tokenize the argument portion into individual arguments */
		for (tok = strtok(arg, ","); tok; tok = strtok(NULL, ",")) {
			if (argc >= CMD_MAX_ARGS)
				break;
			argv[argc++] = tok;
		}
	}

	llist_for_each_entry(c, &cmd_list, list) {
		if (!strcmp(c->cmd, cmd)) {
			if (!(c->ops & op)) {
				uart_cmd_out(cs, "Command `%s' doesn't "
					     "support this operation\n\r", c->cmd);
				return -EINVAL;
			}

			rc = c->cb(cs, op, cmd, argc, argv);
			if (rc < 0)
				uart_cmd_out(cs, "Error executing command\n\r");
			return rc;
		}
	}

	uart_cmd_out(cs, "Unknown command `%s'\n\r", cmd);
	return -EINVAL;
}

static void print_list(struct cmd_state *cs)
{
	struct cmd *c;

	uart_cmd_out(cs, "Supported commands:\r\n");

	llist_for_each_entry(c, &cmd_list, list){
		const char *help = "";

		if (c->help)
			help = c->help;

		uart_cmd_out(cs, "%s  --  %s\n\r", c->cmd, c->help);
	}
}

int uart_cmd_char(struct cmd_state *cs, uint8_t ch)
{
	int rc;

	uart_cmd_out(cs, "%c", ch);

	switch (cs->state) {
	case ST_IN_CMD:
		switch (ch) {
		case '=':
			cs->state = ST_IN_ARG;
			break;
		case '?':
			uart_cmd_out(cs, "\n\r");
			if (cs->cmd.idx == 0)
				print_list(cs);
			else
				rc = handle_cb(cs, CMD_OP_GET, cs->cmd.buf, NULL);
			uart_cmd_reset(cs);
			break;
		case '!':
			uart_cmd_out(cs, "\n\r");
			rc = handle_cb(cs, CMD_OP_EXEC, cs->cmd.buf, NULL);
			uart_cmd_reset(cs);
			break;
		case ' ':
		case '\t':
			/* ignore any whitespace */
			break;
		case '\n':
		case '\r':
			/* new line always resets buffer */
			uart_cmd_reset(cs);
			break;
		default:
			strbuf_append(&cs->cmd, ch);
			break;
		}
		break;
	case ST_IN_ARG:
		switch (ch) {
		case '\r':
			uart_cmd_out(cs, "\n");
			/* fall through */
		case '\n':
			rc = handle_cb(cs, CMD_OP_SET, cs->cmd.buf, cs->arg.buf);
			uart_cmd_reset(cs);
			break;
		case ' ':
		case '\t':
			/* ignore any whitespace */
			break;
		default:
			strbuf_append(&cs->arg, ch);
			break;
		}
	}

	if (ch == '\r')
		return 1;

	return 0;
}

int uart_cmd_out(struct cmd_state *cs, char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	cs->out(format, ap);
	va_end(ap);
}

int uart_cmd_reset(struct cmd_state *cs)
{
	strbuf_reset(&cs->cmd);
	strbuf_reset(&cs->arg);
	cs->state = ST_IN_CMD;

	uart_cmd_out(cs, "\r\n > ");

	return 0;
}

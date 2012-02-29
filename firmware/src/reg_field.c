/* (C) 2011-2012 by Harald Welte <laforge@gnumonks.org>
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


#include <stdint.h>
#include <errno.h>
#include <string.h>

#include <uart_cmd.h>
#include <reg_field.h>

uint32_t reg_field_read(struct reg_field_ops *ops, struct reg_field *field)
{
	uint32_t rc = ops->read_cb(ops->data, field->reg);

	return (rc >> field->shift) & ((1 << field->width)-1);
}

int reg_field_write(struct reg_field_ops *ops, struct reg_field *field, uint32_t val)
{
	uint32_t old = ops->read_cb(ops->data, field->reg);
	uint32_t mask, newreg;

	mask = ((1 << field->width)-1) << field->shift;
	newreg = (old & ~mask) | ((val << field->shift) & mask);

	return ops->write_cb(ops->data, field->reg, newreg);
}

int reg_field_cmd(struct cmd_state *cs, enum cmd_op op,
		  const char *cmd, int argc, char **argv,
		  struct reg_field_ops *ops)
{
	uint32_t tmp;
	int i;

	for (i = 0; i < ops->num_fields; i++) {
		if (strcmp(ops->field_names[i], cmd))
			continue;

		switch (op) {
		case CMD_OP_SET:
			if (argc < 1)
				return -EINVAL;

			reg_field_write(ops, &ops->fields[i], atoi(argv[0]));
			return 0;
			break;
		case CMD_OP_GET:
			tmp = reg_field_read(ops, &ops->fields[i]);
			uart_cmd_out(cs, "%s:%u\n\r", cmd, tmp);
			return 0;
			break;
		default:
			return -EINVAL;
			break;
		}
	}
	return -EINVAL;
}

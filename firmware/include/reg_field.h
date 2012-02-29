#ifndef _REG_FIELD_H
#define _REG_FIELD_H

#include <uart_cmd.h>

/* structure describing a field in a register */
struct reg_field {
	uint8_t reg;
	uint8_t shift;
	uint8_t width;
};

struct reg_field_ops {
	const struct reg_field *fields;
	const char **field_names;
	uint32_t num_fields;
	void *data;
	int (*write_cb)(void *data, uint32_t reg, uint32_t val);
	uint32_t (*read_cb)(void *data, uint32_t reg);
};

uint32_t reg_field_read(struct reg_field_ops *ops, struct reg_field *field);
int reg_field_write(struct reg_field_ops *ops, struct reg_field *field, uint32_t val);
int reg_field_cmd(struct cmd_state *cs, enum cmd_op op,
		  const char *cmd, int argc, char **argv,
		  struct reg_field_ops *ops);


#endif

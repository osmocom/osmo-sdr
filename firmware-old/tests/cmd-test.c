
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>

#include <uart_cmd.h>

static void my_out(const char *format, va_list ap)
{
	vprintf(format, ap);
}

static int my_cb(struct cmd_state *cs, enum cmd_op op, const char *cmd,
		 int argc, char **argv)
{
	int i;

	printf("my_cb(%u,%s,%u,[", op, cmd, argc);
	for (i = 0; i < argc; i++)
		printf("%s,", argv[i]);
	printf("])\n");

	return 0;
}

static struct cmd cmds[] = {
	{ "foo", CMD_OP_SET|CMD_OP_GET|CMD_OP_EXEC, my_cb,
	  "the foo command" },
	{ "bar", CMD_OP_GET, my_cb,
	  "the gettable bar command" },
	{ "baz", CMD_OP_SET, my_cb,
	  "the settable baz command" },
};

int main(int argc, char **argv)
{
	struct cmd_state cs;

	cs.out = my_out;
	uart_cmd_reset(&cs);
	uart_cmds_register(cmds, sizeof(cmds)/sizeof(cmds[0]));

	while (1) {
		uint8_t ch;
		int rc;

	       	rc = read(0, &ch, 1);
		if (rc < 0)
			exit(1);

		uart_cmd_char(&cs, ch);
	}
}

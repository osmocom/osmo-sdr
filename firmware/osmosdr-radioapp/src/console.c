/* (C) 2011-2012 by Christian Daniel <cd@maintech.de>
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

#include <stddef.h>
#include "crt/stdio.h"
#include "crt/string.h"
#include "driver/dbgio.h"
#include "driver/sys.h"
#include "console.h"

#define MAX_NUM_PARAMS 16

typedef enum {
	ISGroup,
	ISElement,
	ISAssign,
	ISParam,
	ISDone,
} InputState;

typedef struct {
	const Group* root;
	char line[80];
	int lineSize;
	InputState inputState;
	const Group* group;
	int groupLen;
	const Element* element;
	int elementLen;

	int dfuCount;
} Console;

static Console g_console;

static void appendGroup(int c)
{
	char line[sizeof(g_console.line) + 2];
	int lineSize = g_console.lineSize;
	int cnt;
	const Group* lastMatchedGroup = NULL;

	memcpy(line, g_console.line, g_console.lineSize);
	line[lineSize++] = c;

	cnt = 0;
	for(const Group* g = g_console.root; g->name != NULL; g++) {
		if(strncmp(g->name, line, lineSize) == 0) {
			lastMatchedGroup = g;
			cnt++;
		}
	}
	if(cnt == 0)
		return;
	dbgio_putChar(c);

	while(True) {
		c = lastMatchedGroup->name[lineSize];
		if(c == '\0')
			break;
		cnt = 0;
		for(const Group* g = g_console.root; g->name != NULL; g++) {
			if(strncmp(g->name, line, lineSize) == 0) {
				lastMatchedGroup = g;
				if(g->name[lineSize] != c) {
					cnt = 1;
					break;
				}
			}
		}
		if(cnt == 0) {
			line[lineSize++] = c;
			dbgio_putChar(c);
		} else {
			break;
		}
	}

	g_console.lineSize = lineSize;
	memcpy(g_console.line, line, g_console.lineSize);
	if(c == '\0') {
		g_console.line[g_console.lineSize++] = '.';
		g_console.inputState = ISElement;
		g_console.group = lastMatchedGroup;
		g_console.groupLen = g_console.lineSize;
		dbgio_putChar('.');
	}
}

static void appendElement(int c)
{
	char line[sizeof(g_console.line) + 2];
	int lineSize = g_console.lineSize;
	int cnt;
	const Element* lastMatchedElement = NULL;

	memcpy(line, g_console.line, g_console.lineSize);
	line[lineSize++] = c;

	cnt = 0;
	for(const Element* e = g_console.group->elements; e->name != NULL; e++) {
		if(strncmp(e->name, line + g_console.groupLen, lineSize - g_console.groupLen) == 0) {
			lastMatchedElement = e;
			cnt++;
		}
	}
	if(cnt == 0)
		return;
	dbgio_putChar(c);

	while(True) {
		c = lastMatchedElement->name[lineSize - g_console.groupLen];
		if(c == '\0')
			break;
		cnt = 0;
		for(const Element* e = g_console.group->elements; e->name != NULL; e++) {
			if(strncmp(e->name, line + g_console.groupLen, lineSize - g_console.groupLen) == 0) {
				lastMatchedElement = e;
				if(e->name[lineSize - g_console.groupLen] != c) {
					cnt = 1;
					break;
				}
			}
		}
		if(cnt == 0) {
			line[lineSize++] = c;
			dbgio_putChar(c);
		} else {
			break;
		}
	}

	g_console.lineSize = lineSize;
	memcpy(g_console.line, line, g_console.lineSize);
	if(c == '\0') {
		g_console.element = lastMatchedElement;
		g_console.elementLen = g_console.lineSize;

		if(g_console.element->parameters == NULL)
			g_console.inputState = ISDone;
		else g_console.inputState = ISAssign;
	}
}

static void appendAssign(int c)
{
	if(c != '=')
		return;

	g_console.line[g_console.lineSize++] = c;
	g_console.elementLen = g_console.lineSize;
	g_console.inputState = ISParam;
	dbgio_putChar(c);
}

static int parseParams(const char* params[])
{
	int num = 0;
	int remain = g_console.lineSize - g_console.elementLen;
	int ofs = g_console.elementLen;

	params[num++] = g_console.line + ofs;

	while(remain > 0) {
		if(g_console.line[ofs] == ',')
			params[num++] = g_console.line + ofs;
		ofs++;
		remain--;
	}

	return num;
}

static int splitParams(const char* params[])
{
	int num = 0;
	int remain = g_console.lineSize - g_console.elementLen;
	int ofs = g_console.elementLen;

	params[num++] = g_console.line + ofs;

	while(remain > 0) {
		if(g_console.line[ofs] == ',') {
			g_console.line[ofs] = '\0';
			params[num] = g_console.line + ofs;
			params[num]++;
			num++;
		}
		ofs++;
		remain--;
	}
	g_console.line[ofs] = '\0';

	return num;
}

static void appendParam(int c)
{
	const char* params[MAX_NUM_PARAMS];
	int numParams = parseParams(params);
	const Parameter* parameter = &g_console.element->parameters[numParams - 1];

	if(c == ',') {
		if(parameter[1].type != PTEnd) {
			if(numParams >= MAX_NUM_PARAMS)
				return;
			g_console.line[g_console.lineSize++] = c;
			dbgio_putChar(c);
			return;
		} else {
			return;
		}
	} else if(c == '-') {
		if(parameter->type == PTSigned) {
			if((g_console.line[g_console.lineSize - 1] == ',') ||  (g_console.line[g_console.lineSize - 1] == '=')) {
				g_console.line[g_console.lineSize++] = c;
				dbgio_putChar(c);
				return;
			}
		}
	} else if((c >= '0') && (c <= '9')) {
		if((parameter->type == PTSigned) || (parameter->type == PTUnsigned)) {
			g_console.line[g_console.lineSize++] = c;
			dbgio_putChar(c);
			return;
		}
	}
}

static void append(int c)
{
	switch(g_console.inputState) {
		case ISGroup:
			appendGroup(c);
			break;

		case ISElement:
			appendElement(c);
			break;

		case ISAssign:
			appendAssign(c);
			break;

		case ISParam:
			appendParam(c);
			break;

		case ISDone:
			break;
	}
}

static void backspace(void)
{
	if(g_console.lineSize > 0) {
		g_console.lineSize--;
		puts("\b \b");
	}
	switch(g_console.inputState) {
		case ISGroup:
			break;

		case ISElement:
			if(g_console.groupLen == g_console.lineSize + 1) {
				g_console.lineSize--;
				puts("\b \b");
				g_console.inputState = ISGroup;
				g_console.group = NULL;
			}
			break;

		case ISAssign:
			g_console.inputState = ISElement;
			break;

		case ISParam:
			if(g_console.elementLen == g_console.lineSize + 1)
				g_console.inputState = ISAssign;
			break;

		case ISDone:
			g_console.inputState = ISElement;
			g_console.element = NULL;
			break;
	}
}

static void execute(void)
{
	if((g_console.inputState == ISGroup) || (g_console.inputState == ISElement)) {
		puts("Error: Invalid command. Press '?' to get a list of all available commands.\n");
		return;
	}

	if(g_console.element->cmd == NULL) {
		puts("Error: Command not yet implemented.\n");
		return;
	}

	if(g_console.inputState == ISParam) {
		const char* argv[MAX_NUM_PARAMS];
		int argc = splitParams(argv);
		int needed = 0;
		for(const Parameter* p = g_console.element->parameters; p->type != PTEnd; p++, needed++) ;
		if(argc < needed) {
			puts("Error: Missing parameters. Press '?' after the = to get a list of parameters.\n");
			return;
		}
		g_console.element->cmd(argc, argv);
		return;
	}
	if((g_console.inputState == ISAssign) || (g_console.inputState == ISDone)) {
		g_console.element->cmd(0, NULL);
	}
}

static void printInputLine(void)
{
	g_console.line[g_console.lineSize] = '\0';
	printf("> %s", g_console.line);
}

static void printHelp(void)
{
	static const char* paramTypes[PTEnd] = {
		"signed number    ",
		"unsigned number  "
	};

	puts("?\n");
	switch(g_console.inputState) {
		case ISGroup:
			for(const Group* g = g_console.root; g->name != NULL; g++) {
				printf("%s commands:\n", g->desc);
				for(const Element* e = g->elements; e->name != NULL; e++) {
					int len = strlen(g->name) + strlen(e->name) + 3;
					printf("  %s.%s", g->name, e->name);
					for(; len < 20; len++)
						puts(" ");
					puts(e->desc);
					puts("\n");
				}
			}
			break;

		case ISElement:
			printf("%s commands:\n", g_console.group->desc);
			for(const Element* e = g_console.group->elements; e->name != NULL; e++) {
				int len = strlen(g_console.group->name) + strlen(e->name) + 3;
				printf("  %s.%s", g_console.group->name, e->name);
				for(; len < 20; len++)
					puts(" ");
				puts(e->desc);
				puts("\n");
			}
			break;

		case ISAssign:
			puts("Type '=' to assign a new value or query the current value by pressing enter.\n");
			break;

		case ISParam: {
			const char* params[MAX_NUM_PARAMS];
			int numParams = parseParams(params);
			int pos = 1;
			printf("%s.%s parameters:\n", g_console.group->name, g_console.element->name);
			for(const Parameter* p = g_console.element->parameters; p->type != PTEnd; p++, pos++)
				printf("%c %s %s\n", pos == numParams ? '*' : ' ', paramTypes[p->type], p->desc);
			break;
		}

		case ISDone:
			puts("Press enter to execute.\n");
			break;
	}
}

void console_configure(const Group console[])
{
	g_console.root = console;
	g_console.lineSize = 0;
	g_console.dfuCount = 0;

	puts("OsmoSDR ready - press '?' for help anytime.\n");
	printInputLine();
}

void console_task(void)
{
	int c;

	while((c = dbgio_getChar()) >= 0) {
		// short-cut to DFU mode -> "+++"
		if(c == '+') {
			g_console.dfuCount++;
			if(g_console.dfuCount >= 3) {
				dputs("+++\n\nShortcut sequence detected - switching to DFU mode.\n");
				sys_reset(True);
			} else {
				continue;
			}
		} else {
			g_console.dfuCount = 0;
		}

		if((c == '?') || (c == '\t')) {
			printHelp();
			printInputLine();
		} else if((c == 8) || (c == 127)) { // backspace
			backspace();
		} else if((c == '\n') || (c == '\r')) {
			puts("\n");
			if(g_console.lineSize > 0) {
				g_console.line[g_console.lineSize] = '\0';
				execute();
			}
			g_console.lineSize = 0;
			g_console.inputState = ISGroup;
			printInputLine();
		} else {
			if(g_console.lineSize < sizeof(g_console.line) - 2)
				append(c);
		}
	}
}

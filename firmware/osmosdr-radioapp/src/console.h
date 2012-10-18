#ifndef INCLUDE_CONSOLE_H
#define INCLUDE_CONSOLE_H

typedef enum {
	PTSigned,
	PTUnsigned,
	PTEnd,
} ParameterType;

typedef struct {
	ParameterType type;
	const char* desc;
} Parameter;

typedef struct {
	const char* name;
	const char* desc;
	const Parameter* parameters;
	void (*cmd)(int argc, const char* argv[]);
} Element;

typedef struct {
	const char* name;
	const char* desc;
	const Element* elements;
} Group;

#define CONSOLE_CMD_BEGIN(name) static const Parameter name[] = {
#define CONSOLE_CMD_PARAM(type, desc) { type, desc }
#define CONSOLE_CMD_END() , { PTEnd, NULL } };

#define EXTERN_CONSOLE_GROUP(name) extern const Element name[];
#define CONSOLE_GROUP_BEGIN(name) const Element name[] = {
#define CONSOLE_GROUP_CMD(name, desc, params, cmd) { name, desc, params, cmd }
#define CONSOLE_GROUP_END() , { NULL, NULL } };

#define CONSOLE_BEGIN(name) static const Group name[] = {
#define CONSOLE_GROUP(name, desc, group) { name, desc, group }
#define CONSOLE_END() , { NULL, NULL} };

void console_configure(const Group root[]);
void console_task(void);

#endif // INCLUDE_CONSOLE_H

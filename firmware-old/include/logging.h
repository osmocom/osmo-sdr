#ifndef _LOGGING_H
#define _LOGGING_H

#define DTUN	1
#define DVCO	2

/*! \brief different log levels */
#define LOGL_DEBUG	1	/*!< \brief debugging information */
#define LOGL_INFO	3
#define LOGL_NOTICE	5	/*!< \brief abnormal/unexpected condition */
#define LOGL_ERROR	7	/*!< \brief error condition, requires user action */
#define LOGL_FATAL	8	/*!< \brief fatal, program aborted */

#define LOGP(ss, level, fmt, args...) \
	logp2(ss, level, __FILE__, __LINE__, 0, fmt, ##args)

#define LOGPC(ss, level, fmt, args...) \
	logp2(ss, level, __FILE__, __LINE__, 1, fmt, ##args)

void logp2(int subsys, unsigned int level, char *file,
	   int line, int cont, const char *format, ...)
				__attribute__ ((format (printf, 6, 7)));

#endif

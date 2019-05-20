#ifndef DEBUG_H_INCLUDED
#define DEBUG_H_INCLUDED
#define _BSD_SOURCE
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <errno.h>
#include <error.h>
#include <math.h>
// FONT MACRO
#define CL_FT_BLACK             "\x1b[30m"
#define CL_FT_BLUE              "\x1b[34m"
#define CL_FT_BROWN             "\x1b[33m"
#define CL_FT_GRAY              "\x1b[37m"
#define CL_FT_GREEN             "\x1b[32m"
#define CL_FT_MAGENTA			"\x1b[36m"
#define CL_FT_PURPLE			"\x1b[35m"
#define CL_FT_RED               "\x1b[31m"

#define CL_BG_BLACK             "\x1b[40m"
#define CL_BG_BLUE              "\x1b[44m"
#define CL_BG_BROWN             "\x1b[43m"
#define CL_BG_GRAY				"\x1b[47m"
#define CL_BG_GREEN             "\x1b[42m"
#define CL_BG_MAGENTA			"\x1b[46m"
#define CL_BG_PURPLE			"\x1b[45m"
#define CL_BG_RED               "\x1b[41m"

#define TY_FT_BOLD				"\x1b[1m"
//#define TY_FT_BOLD			"\x1b[5m"
#define TY_FT_DEFAULT			"\x1b[0m"
#define TY_FT_HBOLD				"\x1b[2m"
#define TY_FT_REVERSE			"\x1b[7m"
#define TY_FT_UNDERLINE			"\x1b[4m"

#define SET_FONT(X) printf(X)
#define printf_color(color, ...) \
	{ \
		SET_FONT(color); \
		printf(__VA_ARGS__); \
		SET_FONT(TY_FT_DEFAULT); \
	}



// LOG MACRO
#if defined(DEBUG)
    #define LOG(format, ...) \
    do { \
        char __tb[101]; \
        time_t __t = time(NULL); \
        struct tm* tmp = localtime(&__t); \
        strftime(__tb, 100, "%d.%m.%Y %H:%M:%S ", tmp); \
        const char* __filename__ = __FILE__; \
        char* __iterator__ = (char*)__filename__ + strlen(__filename__) - 1; \
        int __count__slash__ = 0; \
		while (__iterator__ > __filename__ && __count__slash__ < 2) { \
			if (*__iterator__ == '/') \
				__count__slash__ ++; \
			if (__count__slash__ != 2) \
				__iterator__--; \
		} \
		char buffer[10000] = {}; \
		sprintf(buffer, "%s[%s:%d]:%d " , __tb, __iterator__, __LINE__, getpid()); \
        sprintf(buffer + strlen(buffer), format, ##__VA_ARGS__); \
        fprintf(stderr, "%s\n", buffer); \
        fflush(stderr); \
    } while (0)
	#define LOG_VAR(VAR, format) \
	do { \
		LOG(#VAR " = " format, VAR); \
	} while (0)
#else
    #define LOG(format, ...) 
	#define LOG_VAR(VAR, format) 
#endif

// PERROR MACRO
#define PERROR(format, ...) \
	do { \
		char __tb[101]; \
		time_t __t = time(NULL); \
		struct tm* tmp = localtime(&__t); \
		strftime(__tb, 100, "%d.%m.%Y %H:%M:%S ", tmp); \
		const char* __filename__ = __FILE__; \
		char* __iterator__ = (char*)__filename__ + strlen(__filename__) - 1; \
		int __count__slash__ = 0; \
		while (__iterator__ > __filename__ && __count__slash__ < 2) { \
			if (*__iterator__ == '/') \
				__count__slash__ ++; \
			if (__count__slash__ != 2) \
				__iterator__--; \
		} \
		char buffer[10000] = {}; \
		sprintf(buffer, "%s[%s:%d]:%d " , __tb, __iterator__, __LINE__, getpid()); \
		sprintf(buffer + strlen(buffer), CL_FT_RED); \
		sprintf(buffer + strlen(buffer), TY_FT_BOLD); \
		sprintf(buffer + strlen(buffer), "ERROR! "); \
		sprintf(buffer + strlen(buffer), TY_FT_DEFAULT); \
		sprintf(buffer + strlen(buffer), format, ##__VA_ARGS__); \
		if (errno != 0) \
			sprintf(buffer + strlen(buffer), " (%s)\n", strerror(errno)); \
		else \
			sprintf(buffer + strlen(buffer), "\n"); \
        fprintf(stderr, "%s", buffer); \
		fflush(stderr); \
	} while (0)

#if defined(DEBUG)
#define TIMER_START(IDENTIFIER) \
	struct timeval timeval_start_##IDENTIFIER; \
	int timeval_start_line_##IDENTIFIER; \
	struct timeval timeval_stop_##IDENTIFIER; \
	int timeval_stop_line_##IDENTIFIER; \
	struct timeval timeval_diff_##IDENTIFIER; \
	time_t t_##IDENTIFIER; \
	char __tb_##IDENTIFIER[101]; \
	struct tm* tmp_##IDENTIFIER; \
	const char* __filename___##IDENTIFIER ; \
	char* __iterator___##IDENTIFIER; \
	int __count__slash___##IDENTIFIER; \
	char buffer__##IDENTIFIER[10000]; \
	\
	\
	if (gettimeofday(&timeval_start_##IDENTIFIER, NULL) == EXIT_FAILURE) { \
		PERROR("Start point failed with identifier=\"%s\"", #IDENTIFIER); \
	} \
	timeval_start_line_##IDENTIFIER = __LINE__;



#define TIMER_DUMP(IDENTIFIER) \
	if (gettimeofday(&timeval_stop_##IDENTIFIER, NULL) == EXIT_FAILURE) { \
       PERROR("Stop point failed with identifier=\"%s\"", #IDENTIFIER); \
	} \
	timeval_stop_line_##IDENTIFIER = __LINE__; \
	timersub(&timeval_stop_##IDENTIFIER, &timeval_start_##IDENTIFIER, &timeval_diff_##IDENTIFIER); \
    \
    \
	t_##IDENTIFIER = time(NULL); \
	tmp_##IDENTIFIER = localtime(&t_##IDENTIFIER); \
    strftime(__tb_##IDENTIFIER, 100, "%d.%m.%Y %H:%M:%S ", tmp_##IDENTIFIER); \
    __filename___##IDENTIFIER = __FILE__; \
    __iterator___##IDENTIFIER = (char*)__filename___##IDENTIFIER + strlen(__filename___##IDENTIFIER) - 1; \
    __count__slash___##IDENTIFIER = 0; \
	while (__iterator___##IDENTIFIER > __filename___##IDENTIFIER && __count__slash___##IDENTIFIER < 2) { \
		if (*__iterator___##IDENTIFIER == '/') \
			__count__slash___##IDENTIFIER ++; \
		if (__count__slash___##IDENTIFIER != 2) \
			__iterator___##IDENTIFIER--; \
	} \
	sprintf(buffer__##IDENTIFIER, "%s [%s:%d:%d] ", __tb_##IDENTIFIER, __iterator___##IDENTIFIER, timeval_start_line_##IDENTIFIER, timeval_stop_line_##IDENTIFIER); \
    sprintf(buffer__##IDENTIFIER + strlen(buffer__##IDENTIFIER), "%s.time = %ld.%03ld sec\n", #IDENTIFIER, timeval_diff_##IDENTIFIER.tv_sec, (timeval_diff_##IDENTIFIER.tv_usec) / 1000); \
    fprintf(stderr, buffer__##IDENTIFIER); \
    fflush(stderr); \

#define TIMER_CLEAR(IDENTIFIER) \
	if (gettimeofday(&timeval_start_##IDENTIFIER, NULL) == EXIT_FAILURE) { \
		PERROR("Start point failed with identifier=\"%s\"", #IDENTIFIER); \
	} \
	timeval_start_line_##IDENTIFIER = __LINE__;



#else
	#define TIMER_START(IDENTIFIER) ;
	#define TIMER_CLEAR(IDENTIFIER) ;
	#define TIMER_DUMP(IDENTIFIER)  ;
#endif


#endif // DEBUG_H_INCLUDED

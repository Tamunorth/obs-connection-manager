#include "session-log.h"
#include "cm-types.h"
#include <obs-module.h>
#include <util/platform.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static FILE *log_file = NULL;
static char log_path[1024] = {0};

void cm_log_open(void)
{
	if (log_file) {
		cm_log_close();
	}

	char *base = obs_module_config_path("");
	if (!base) {
		blog(LOG_WARNING, CM_LOG_PREFIX
		     "log: cannot resolve module config dir");
		return;
	}
	os_mkdirs(base);

	time_t now = time(NULL);
	struct tm tm_buf;
#ifdef _WIN32
	localtime_s(&tm_buf, &now);
#else
	localtime_r(&now, &tm_buf);
#endif
	char ts[32];
	strftime(ts, sizeof(ts), "%Y%m%d-%H%M%S", &tm_buf);

	snprintf(log_path, sizeof(log_path), "%ssessions%c%s.log", base,
		 (int)'/', ts);

	/* Ensure sessions dir exists */
	char sessions_dir[1024];
	snprintf(sessions_dir, sizeof(sessions_dir), "%ssessions", base);
	os_mkdirs(sessions_dir);

	bfree(base);

	log_file = fopen(log_path, "a");
	if (!log_file) {
		blog(LOG_WARNING, CM_LOG_PREFIX
		     "log: failed to open %s", log_path);
		return;
	}
	fprintf(log_file, "# Connection Manager session log\n");
	fprintf(log_file, "# started %s\n", ts);
	fflush(log_file);
	blog(LOG_INFO, CM_LOG_PREFIX "session-log opened path=%s", log_path);
}

void cm_log_close(void)
{
	if (log_file) {
		fclose(log_file);
		log_file = NULL;
		blog(LOG_INFO, CM_LOG_PREFIX "session-log closed");
	}
}

void cm_log_event(const char *fmt, ...)
{
	if (!log_file)
		return;

	time_t now = time(NULL);
	struct tm tm_buf;
#ifdef _WIN32
	localtime_s(&tm_buf, &now);
#else
	localtime_r(&now, &tm_buf);
#endif
	char ts[32];
	strftime(ts, sizeof(ts), "%H:%M:%S", &tm_buf);

	fprintf(log_file, "%s ", ts);

	va_list args;
	va_start(args, fmt);
	vfprintf(log_file, fmt, args);
	va_end(args);

	fprintf(log_file, "\n");
	fflush(log_file);
}

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>

char VERSION_STRING[] = "this is: ";

const char LEVEL_FATAL[] = "FATL";
const char LEVEL_ERROR[] = "INFO";
const char LEVEL_WARN[] = "WARN";
const char LEVEL_INFO[] = "INFO";
const char LEVEL_DEBUG[] = "DBUG";

#define LLOG_FATAL (1)
#define LLOG_ERROR (2)
#define LLOG_WARN  (3)
#define LLOG_INFO  (4)
#define LLOG_DEBUG (5)

#define LOG_FILENAME "/tmp/boomer.log"
FILE *pFile;
uint64_t previous_flush_timestamp;
uint32_t log_start_pos;
#define PREVIOUS_LOG_FILENAME "/run/shm/previous_boomer.log"
#define LOG_FILE_MAX_BYTES 6000000
#define LOG_FLUSH_INTERVAL_MILLIS 1E7

uint64_t counter(){ //was static, removed
  struct timespec now;
  clock_gettime( CLOCK_MONOTONIC_RAW, &now );
  return (uint64_t)now.tv_sec * UINT64_C(1000000000) + (uint64_t)now.tv_nsec;
}

void logging_init(void){
	pFile=fopen(LOG_FILENAME, "a+");
	if(pFile==NULL)
	{
   	perror("Error opening /run/shm log file\n");
		exit(1);
	}
   log_start_pos = ftell(pFile);
	time_t t = time(NULL);
	struct tm tm = *localtime(&t);
	fprintf(pFile, "==== Log opened at: %d-%02d-%02d %02d:%02d:%02d\n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
	previous_flush_timestamp = counter();
}


void log_main(int level, char * filename, int line_num, char * format, ...) 
{
   const char * level_ptr = LEVEL_DEBUG;
   if (level == LLOG_FATAL) level_ptr = LEVEL_FATAL;
   else if (level == LLOG_ERROR) level_ptr = LEVEL_ERROR;
   else if (level == LLOG_WARN) level_ptr = LEVEL_WARN;
   else if (level == LLOG_INFO) level_ptr = LEVEL_INFO;

   char info_string[96]; 
   struct timespec curTime; struct tm* info; 
   clock_gettime(CLOCK_REALTIME, &curTime); 
   info = localtime(&curTime.tv_sec); 
   sprintf(info_string, "%d-%d-%dT%d:%d:%d.%d_%s_%s-L%03d", 1900 + info->tm_year, info->tm_mon, info->tm_mday, \
      info->tm_hour, info->tm_min, info->tm_sec, (int) (curTime.tv_nsec/1000000), level_ptr, filename, line_num); 
   
   char log_string[128]; 
   va_list aptr; 
   va_start(aptr, format); 
   vsprintf(log_string, format, aptr); 
   va_end(aptr);

   if (ftell(pFile) > LOG_FILE_MAX_BYTES){
		fclose(pFile);
		remove(PREVIOUS_LOG_FILENAME);
		if (rename(LOG_FILENAME, PREVIOUS_LOG_FILENAME) != 0) printf("Error: boomer_log rename failed");
		pFile=fopen(LOG_FILENAME, "w");
		if(pFile==NULL)
		{
			perror("Error opening /run/shm log file\n");
		}
      log_start_pos = ftell(pFile);
		time_t t = time(NULL);
		struct tm tm = *localtime(&t);
		fprintf(pFile, "==== Log rolled at: %d-%02d-%02d %02d:%02d:%02d\n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
	}
	fprintf(pFile, "%s>%s\n", info_string, log_string);
	// fflush( pFile );

	if((counter() - previous_flush_timestamp) > LOG_FLUSH_INTERVAL_MILLIS){
		// time_t t = time(NULL);
		// struct tm tm = *localtime(&t);
		// printf("==== flushing at: %02d:%02d:%02d\n", tm.tm_hour, tm.tm_min, tm.tm_sec);
		fflush( pFile );
		previous_flush_timestamp = counter();
	}
 }

void save_debug(char*path){
   fflush(pFile);
   uint32_t log_end_pos = ftell(pFile);
   uint32_t log_size = log_end_pos - log_start_pos;
   //printf("log_start: %d log_end: %d; size: %d\n", log_start_pos, log_end_pos, log_size);
   char buffer[1024*1024];
   size_t elements_rw;
   #define ELEMENTS_TO_READ 1
   if (log_size < sizeof(buffer)) {
      int rc = fseek(pFile, log_start_pos-1, SEEK_SET);
      if (rc != 0) 
      {
         perror("debug_log_save: seek to log_start failed");
         return;
      }
      elements_rw = fread(buffer, log_size, ELEMENTS_TO_READ, pFile);
      if (elements_rw != ELEMENTS_TO_READ) {
         printf("%s read of %d bytes failed.", LOG_FILENAME, log_size);
      } else
      {
         FILE *f = fopen(path,"w");
         elements_rw = fwrite(buffer, log_size, ELEMENTS_TO_READ, f);
         if (elements_rw != ELEMENTS_TO_READ) {
            printf("%s write of %d bytes failed.", path, log_size);
         }
         fclose(f);
      }
      // restore append position
      rc = fseek(pFile, 0, SEEK_END);
      if (rc != 0) 
      {
         perror("debug_log_save: seek to end failed");
      }
    } else {
      printf("log_size of %d larger than buffer.\n", log_size);
   }
}
// the following goes in logging.h 
#define LOG_FATAL(...)  log_main(LLOG_FATAL, __BASE_FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...)  log_main(LLOG_ERROR, __BASE_FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARNING(...) log_main(LLOG_WARN, __BASE_FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(...)   log_main(LLOG_INFO, __BASE_FILE__, __LINE__, __VA_ARGS__)
#define LOG_DEBUG(...)  log_main(LLOG_DEBUG, __BASE_FILE__, __LINE__, __VA_ARGS__)

int main(int argc, const char *argv[]) {
   logging_init();
	LOG_DEBUG("%s %s %d", "Test of Debug: ", VERSION_STRING, 11);
	LOG_INFO("%s %s %d", "Test of Info: ", VERSION_STRING, 12);
	LOG_WARNING("%s %s %d", "Test of Warn: ", VERSION_STRING, 13);
	LOG_ERROR("%s %s %d", "Test of Err: ", VERSION_STRING, 14);
	LOG_FATAL("%s %s %d", "Test of Fatal: ", VERSION_STRING, 15);
   save_debug("/tmp/log.log");
}
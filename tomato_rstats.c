/*
	rstats modified for standalone use.
	
	Some code copied from tomatousb project
	Copyright (C) 2006-2009 Jonathan Zarate

	Will read rstats backup file to JSON output.
*/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdint.h>
#include <inttypes.h>

#define K 1024
#define M (1024 * 1024)
#define G (1024 * 1024 * 1024)

#define MAX_NDAILY 62
#define MAX_NMONTHLY 25

#define MAX_COUNTER	2

#define DAILY		0
#define MONTHLY		1

#define ID_V0 0x30305352
#define ID_V1 0x31305352
#define CURRENT_ID ID_V1

typedef struct {
	uint32_t xtime;
	uint64_t counter[MAX_COUNTER];
} data_t;

typedef struct {
	uint32_t id;

	data_t daily[MAX_NDAILY];
	int dailyp;

	data_t monthly[MAX_NMONTHLY];
	int monthlyp;
} history_t;

typedef struct {
	uint32_t id;

	data_t daily[62];
	int dailyp;

	data_t monthly[12];
	int monthlyp;
} history_v0_t;

history_t history;

const char uncomp_fn[] = "/var/tmp/rstats-uncomp";

int f_read(const char *path, void *buffer, int max)
{
	int f;
	int n;

	if ((f = open(path, O_RDONLY)) < 0) return -1;
	n = read(f, buffer, max);
	close(f);
	return n;
}

int decomp(const char *fname, void *buffer, int size, int max)
{
	char s[256];
	int n;

	unlink(uncomp_fn);

	n = 0;
	sprintf(s, "gzip -dc %s > %s", fname, uncomp_fn);
	if (system(s) == 0)
	{
		n = f_read(uncomp_fn, buffer, size * max);
		if (n <= 0) n = 0;
			else n = n / size;
	}
	else {
		printf("%s: %s != 0\n", __FUNCTION__, s);
	}
	unlink(uncomp_fn);
	memset((char *)buffer + (size * n), 0, (max - n) * size);
	return n;
}

static void clear_history(void)
{
	memset(&history, 0, sizeof(history));
	history.id = CURRENT_ID;
}

int load_history(const char *fname)
{
	history_t hist;

	if ((decomp(fname, &hist, sizeof(hist), 1) != 1) || (hist.id != CURRENT_ID)) {
		history_v0_t v0;

		if ((decomp(fname, &v0, sizeof(v0), 1) != 1) || (v0.id != ID_V0)) {
			return 0;
		}
		else {
			// temp conversion
			clear_history();

			// V0 -> V1
			history.id = CURRENT_ID;
			memcpy(history.daily, v0.daily, sizeof(history.daily));
			history.dailyp = v0.dailyp;
			memcpy(history.monthly, v0.monthly, sizeof(v0.monthly));	// v0 is just shorter
			history.monthlyp = v0.monthlyp;
		}
	}
	else {
		memcpy(&history, &hist, sizeof(history));
	}
	return 1;
}

int* get_time(int32_t xtime)
{
	int *i = malloc(sizeof(int) * 3);

	i[0] = ((xtime >> 16) & 0xFF) + 1900;
	i[1] = ((xtime >> 8) & 0xFF) + 1;
	i[2] = xtime & 0xFF;

	return i;
}

void print_counters(data_t data, int div)
{
	int* time = get_time(data.xtime);
	
	printf("{\"date\":");
	printf("\"%d/%d/%d\",", time[1], time[2], time[0]);
	printf("\"download\":");
	printf("\"%"PRIu64"\",", data.counter[0] / div);
	printf("\"upload\":");
	printf("\"%"PRIu64"\"", data.counter[1] / div);
	printf("}");
}

void print_json()
{
	int i, prnt = 0;

	printf("{\"daily\":[");
	for(i = 0; i < MAX_NDAILY; i++)
	{
		data_t data = history.daily[i];

		if(data.xtime == 0) {
			prnt = 0; continue;
		}
		else {
			if(prnt) {
				printf(",");
			}
		}

		print_counters(data, M);
		prnt = 1;
	}
	printf("],");

	printf("\"monthly\":[");
	for(i = 0; i < MAX_NMONTHLY; i++)
	{
		data_t data = history.monthly[i];
		
		if(data.xtime == 0) { 
			prnt = 0; continue;
		}
		else {
			if(prnt) {
				printf(",");
			}
		}

		print_counters(data, G);
		prnt = 1;
	}
	printf("]}");
}

int main(int argc, char *argv[])
{
	if (argc > 1)
	{
		load_history(argv[1]);
		print_json();
	}
	else {
		printf("rstats Copyright (C) 2006-2009 Jonathan Zarate\n");
		printf("usage: <fname>\n");
	}
	return 0;
}

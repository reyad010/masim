#include <argp.h>
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "masim.h"
#include "config.h"


#define LEN_ARRAY(x) (sizeof(x) / sizeof(*x))

void pr_regions(struct mregion *regions, size_t nr_regions)
{
	struct mregion *region;
	int i;

	printf("memory regions\n");
	for (i = 0; i < nr_regions; i++) {
		region = &regions[i];
		printf("\t%s: %zu bytes\n", region->name, region->sz);
	}
	printf("\n");
}


void pr_phases(struct phase *phases, int nr_phases)
{
	struct phase *phase;
	struct access *pattern;
	int i, j;

	for (i = 0; i < nr_phases; i++) {
		phase = &phases[i];
		printf("Phase %d for %u ms\n", i, phase->time_ms);
		for (j = 0; j < phase->nr_patterns; j++) {
			pattern = &phase->patterns[j];
			printf("\tPattern %d\n", j);
			printf("\t\t%s access region %s with stride %zu\n",
					pattern->random_access ?
					"randomly" : "sequentially",
					pattern->mregion->name,
					pattern->stride);
		}
	}
}

extern struct mregion mregions[];
extern struct phase phases[];

struct access_pattern {
	struct mregion *regions;
	ssize_t nr_regions;
	struct phase *phases;
	ssize_t nr_phases;
};

struct access_pattern apattern;

size_t len_line(char *str, size_t lim_seek)
{
	size_t i;

	for (i = 0; i < lim_seek; i++) {
		if (str[i] == '\n')
			break;
	}

	if (i == lim_seek)
		return -1;

	return i;
}

/* Read exactly required bytes from a file */
void readall(int file, char *buf, ssize_t sz)
{
	ssize_t sz_read;
	ssize_t sz_total_read = 0;
	while (1) {
		sz_read = read(file, buf + sz_total_read, sz - sz_total_read);
		if (sz_read == -1)
			err(1, "read() failed!");
		sz_total_read += sz_read;
		if (sz_total_read == sz)
			return;
	}
}

char *rm_comments(char *orig, ssize_t origsz)
{
	char *read_cursor;
	char *write_cursor;
	size_t len;
	size_t offset;
	char *result;

	result = (char *)malloc(origsz);
	read_cursor = orig;
	write_cursor = result;
	offset = 0;
	while (1) {
		read_cursor = orig + offset;
		len = len_line(read_cursor, origsz - offset);
		if (len == -1)
			break;
		if (read_cursor[0] == '#')
			goto nextline;
		strncpy(write_cursor, read_cursor, len + 1);
		write_cursor += len + 1;
nextline:
		offset += len + 1;
	}

	return result;
}

struct access_pattern *read_config(char *cfgpath)
{
	struct stat sb;
	char *cfgstr;
	int f;
	struct access_pattern *ret = NULL;
	char *content;

	f = open(cfgpath, O_RDONLY);
	if (f == -1)
		err(1, "open(\"%s\") failed", cfgpath);
	if (fstat(f, &sb))
		err(1, "fstat() for config file (%s) failed", cfgpath);
	cfgstr = (char *)malloc(sb.st_size * sizeof(char));
	readall(f, cfgstr, sb.st_size);

	content = rm_comments(cfgstr, sb.st_size);
	free(cfgstr);
	printf("Content of config: %s\n", content);

	close(f);

	return ret;
}

static struct argp_option options[] = {
	{
		.name = "config",
		.key = 'c',
		.arg = "<config file>",
		.flags = 0,
		.doc = "path to memory access configuration file",
		.group = 0,
	},
	{
		.name = "pr_config",
		.key = 'p',
		.arg = 0,
		.flags = 0,
		.doc = "flag for configuration content print",
		.group = 0,
	},
	{}
};

char *config_file = "config";
int do_print_config;

error_t parse_option(int key, char *arg, struct argp_state *state)
{
	switch(key) {
	case 'c':
		config_file = (char *)malloc((strlen(arg) + 1 ) * sizeof(char));
		strcpy(config_file, arg);
		break;
	case 'p':
		do_print_config = 1;
		break;
	default:
		return ARGP_ERR_UNKNOWN;
	}

	return 0;
}

int main(int argc, char *argv[])
{
	struct argp argp = {
		.options = options,
		.parser = parse_option,
		.args_doc = "",
		.doc = "Simulate given memory access pattern",
	};
	argp_parse(&argp, argc, argv, ARGP_IN_ORDER, NULL, NULL);

	read_config(config_file);

	apattern.regions = mregions;
	apattern.nr_regions = LEN_ARRAY(mregions);
	apattern.phases = phases;
	apattern.nr_phases = LEN_ARRAY(phases);
	if (do_print_config) {
		pr_regions(apattern.regions, LEN_ARRAY(mregions));
		pr_phases(phases, LEN_ARRAY(phases));
	}

	return 0;
}

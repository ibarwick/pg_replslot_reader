/*
 * pg_replslot_reader
 *
 * Utility to display information about replication slots stored in
 * the PostgreSQL data directory, regardless of whether the server
 * is running or not.
 *
 * It is designed to be independent of PostgreSQL version, provided the
 * version is at least 9.4 (when replication slots were introduced).
 *
 * Replication slot state is kept as on-disk data, not in the system
 * catalogue, due to the requirement to make them available on standbys
 * for cascaded standby setups.
 *
 * See: src/backend/replication/slot.c
 *
 * Portions Copyright (c) 2012-2016, PostgreSQL Global Development Group
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "pg_replslot_reader.h"

static void ScanReplSlotDirs(const char *datadir);
static void ReadReplSlotDir(const char *replslot_dir);

static void ValidatePgVersion(const char *datadir);
static void do_help(void);
static void do_usage(void);



typedef struct ReplslotInfo
{
	bool slotfile_parsed;
	char error[MAXLEN];
	char name[MAXLEN];
	ReplicationSlotType type;
	uint32 version;
	uint32 length;
	uint32 db_oid;
	ReplicationSlotPersistency persistency;
	struct ReplslotInfo *next;
} ReplslotInfo;



const char *progname;
char datadir[MAXPGPATH] = "";
ReplslotInfo *replslot_info_start = NULL;
ReplslotInfo *replslot_info_last = NULL;


int
main(int argc, char **argv)
{
	static struct option long_options[] =
	{
		{"help", no_argument, NULL, 1},
		{"pgdata", required_argument, NULL, 'D'},
		{"version", no_argument, NULL, 'V'},
	};

	int			optindex;
	int			c;

	progname = argv[0];

	/* Prevent getopt_long() from printing an error message */
	opterr = 0;

	while ((c = getopt_long(argc, argv, "?VD:", long_options,
							&optindex)) != -1)
	{
		switch (c)
		{
			case '?':
				/* Actual help option given */
				if (strcmp(argv[optind - 1], "-?") == 0)
				{
					do_help();
					exit(0);
				}
				/* unknown option reported by getopt */
				else
					goto unknown_option;
				break;
			case 1:
				do_help();
				exit(0);
			case 'V':
				printf("%s %s (PostgreSQL %s)\n", progname, RR_VERSION, PG_VERSION);
				exit(0);
			case 'D':
				strncpy(datadir, optarg, MAXPGPATH);
				break;
			default:
			unknown_option:
				do_usage();
				exit(1);
		}
	}

	if (datadir[0] == '\0')
	{
		puts("Please provide the PostgreSQL data directory location with -D/--pgdata");
		exit(1);
	}

	ValidatePgVersion(datadir);
	ScanReplSlotDirs(datadir);

	exit(0);
}



/*
 * See also eponymous function in src/backend/utils/init/miscinit.c
 */
static void
ValidatePgVersion(const char *datadir)
{
	FILE	   *fd;
	int			ret;

	long		file_major,
				file_minor,
				version_num;

	char		path[MAXPGPATH];
	struct stat sb;

	printf("Checking directory %s...\n", datadir);

	snprintf(path, MAXPGPATH, "%s/PG_VERSION", datadir);
	if (stat(path, &sb) != 0)
	{
		printf("%s is not a PostgreSQL directory\n", datadir);
		exit(1);
	}

	fd = fopen(path, "r");
	if (fd == NULL)
	{
		printf("Unable to read PG_VERSION file in %s\n", datadir);
		fclose(fd);
		exit(1);
	}

	ret = fscanf(fd, "%ld.%ld", &file_major, &file_minor);
	fclose(fd);

	if (ret != 2)
	{
		printf("PG_VERSION file in %s does not contain a valid version number\n", datadir);
		exit(1);
	}

	version_num = (file_major * 10000) + (file_minor * 100);

	if (version_num < MIN_SUPPORTED_VERSION_NUM)
	{
		printf("This data directory is for PostgreSQL %ld.%ld; %s supports %s or later\n",
			   file_major,
			   file_minor,
			   progname,
			   MIN_SUPPORTED_VERSION
			);
		exit(0);
	}
}


static void
ScanReplSlotDirs(const char *datadir)
{
	char slotdir_path[MAXPGPATH];
	DIR			  *slotdir;
	struct dirent *slotdir_ent;
	int			   slotdir_cnt = 0;

	snprintf(slotdir_path, MAXPGPATH,
			 "%s/pg_replslot",
			 datadir);

	slotdir = opendir(slotdir_path);

	if (slotdir == NULL)
	{
		printf("Unable to open directory '%s'\n", datadir);
		exit(1);
	}


	while ((slotdir_ent = readdir(slotdir)) != NULL) {
		struct stat statbuf;
		char slotdir_ent_path[MAXPGPATH];

		if(strcmp(slotdir_ent->d_name, ".") == 0 || strcmp(slotdir_ent->d_name, "..") == 0)
			continue;

		snprintf(slotdir_ent_path, MAXPGPATH,
				 "%s/%s",
				 slotdir_path,
				 slotdir_ent->d_name);


		if (stat(slotdir_ent_path, &statbuf) == 0 && !S_ISDIR(statbuf.st_mode))
			continue;

		ReadReplSlotDir(slotdir_ent_path);

		slotdir_cnt++;
	}

	closedir(slotdir);

	if (slotdir_cnt == 0)
	{
		puts("No replication slots found");
		exit(0);
	}

	printf("%i replication slot(s) found\n\n", slotdir_cnt);

	if (replslot_info_start != NULL)
	{
		ReplslotInfo *ptr = replslot_info_start;

		do
		{
			if (ptr->slotfile_parsed == false)
			{
				printf("Unable to parse slot \"%s\":\n%s\n", ptr->name, ptr->error);
			}
			else
			{
				int i;

				printf("%s\n", ptr->name);

				for (i = 0; i < strlen(ptr->name); i++)
				{
					putchar('-');
				}
				puts("");

				printf("  Type: ");
				if (ptr->type == RS_PHYSICAL )
					puts("physical");
				else
					printf("logical; DB oid: %u", ptr->db_oid);

				printf("  Persistency: %s\n",ptr-> persistency == RS_PERSISTENT ? "persistent" : "empheral");
				printf("  Version: %u\n", ptr->version);
				printf("  Length: %u\n", ptr->length);

			}
			ptr = ptr->next;
		} while (ptr != NULL);
	}

	puts("");
}


/*
 * parts copied from RestoreSlotFromDisk()
 */

static void
ReadReplSlotDir(const char *replslot_dir)
{
	ReplicationSlotOnDisk cp;
	FILE *fd;
	char		path[MAXPGPATH];
	int			readBytes;

	ReplslotInfo *replslot_info_current;

	snprintf(path, MAXPGPATH,
			 "%s/state",
			 replslot_dir);

	replslot_info_current = pg_malloc0(sizeof(ReplslotInfo));

	if (replslot_info_start == NULL)
	{
		replslot_info_start = replslot_info_current;
		replslot_info_last = replslot_info_current;
	}
	else
	{
		replslot_info_last->next = replslot_info_current;
	}


	replslot_info_last = replslot_info_current;
	replslot_info_last->next = NULL;

	replslot_info_last->slotfile_parsed = true;
	*replslot_info_last->error = '\0';
	*replslot_info_last->name = '\0';
	replslot_info_last->version = 0;
	replslot_info_last->length = 0;
	replslot_info_last->db_oid = InvalidOid;
	replslot_info_last->persistency = RS_PERSISTENT;
	replslot_info_last->next = NULL;

	fd = fopen(path, "rb");
	if (fd == NULL)
	{
		snprintf(
			replslot_info_last->error, MAXLEN,
			"Unable to open replication slot file %s:\n%s\n",
			   path,
			   strerror(errno)
			);
		replslot_info_last->slotfile_parsed = false;

		fclose(fd);
		return;
	}

	/* read part of statefile that's guaranteed to be version independent */
	readBytes = fread(&cp, 1, ReplicationSlotOnDiskConstantSize, fd);
	if (readBytes != ReplicationSlotOnDiskConstantSize)
	{
		fclose(fd);

		snprintf(
			replslot_info_last->error, MAXLEN,
			"could not read file \"%s\", read %d of %u",
			path, readBytes,
			(uint32) ReplicationSlotOnDiskConstantSize);
		return;
	}

	/* verify magic */
	if (cp.magic != SLOT_MAGIC)
	{
		fclose(fd);

		snprintf(
			replslot_info_last->error, MAXLEN,
			"replication slot file \"%s\" has wrong magic number: %u instead of %u",
			path, cp.magic, SLOT_MAGIC);
		return;
	}

	/* verify version */
	if (cp.version < MIN_SLOT_VERSION || cp.version > MAX_SLOT_VERSION)
	{
		fclose(fd);

		snprintf(
			replslot_info_last->error, MAXLEN,
			"replication slot file \"%s\" has unsupported version %u",
			path, cp.version);
		return;
	}

	/* boundary check on length */
	if (cp.length != ReplicationSlotOnDiskV2Size)
	{
		fclose(fd);

		snprintf(
			replslot_info_last->error, MAXLEN,
			"replication slot file \"%s\" has corrupted length %u",
			path, cp.length);
		return;
	}

	/* Now that we know the size, read the entire file */
	readBytes = fread((char *) &cp + ReplicationSlotOnDiskConstantSize,
					  1,
					  cp.length,
					  fd);
	if (readBytes != cp.length)
	{
		fclose(fd);
		snprintf(
			replslot_info_last->error, MAXLEN,
			"could not read file \"%s\", read %d of %u",
			path, readBytes, cp.length);
		return;
	}


	strncpy(
		replslot_info_last->name,
		cp.slotdata.name.data,
		MAXLEN);

	replslot_info_last->version = cp.version;
	replslot_info_last->length =  cp.length;

	if (cp.slotdata.database == InvalidOid)
	{
		replslot_info_last->type = RS_PHYSICAL;
	}
	else
	{
		replslot_info_last->type = RS_LOGICAL;
		replslot_info_last->db_oid = (uint32)cp.slotdata.database;
	}

	replslot_info_last->persistency = cp.slotdata.persistency;

	fclose(fd);
}


static void
do_usage(void)
{
	printf(_("%s: replication slot reader\n"), progname);
	printf(_("Try \"%s --help\" for more information.\n"), progname);
}

static void
do_help(void)
{
	printf(_("%s: replication slot reader\n"), progname);
	printf(	 "\n");
	printf(_("General options:\n"));
	printf(_("	-?, --help							show this help, then exit\n"));
	printf(_("	-V, --version						output version information, then exit\n"));
	printf(	 "\n");
	printf(_("General configuration options:\n"));
	printf(_("	-D, --pgdata=DIR					PostgreSQL data directory to examine\n"));
	printf(	 "\n");
}

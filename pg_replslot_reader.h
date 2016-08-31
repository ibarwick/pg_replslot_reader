/*
 * pg_replslot_reader.h
 *
 * Portions Copyright (c) 2012-2016, PostgreSQL Global Development Group
 *
 */

#ifndef PG_REPLSLOT_READER_H
#define PG_REPLSLOT_READER_H

#include <getopt_long.h>

#include "postgres.h"
#include "fmgr.h"

#include "libpq-fe.h"
#include "postgres_fe.h"

#include "access/xlog.h"

#define RR_VERSION "0.1"
/* replication slots available from PostgreSQL 9.4 */
#define MIN_SUPPORTED_VERSION		"9.4"
#define MIN_SUPPORTED_VERSION_NUM	90400

#define MAXLEN 1024

typedef enum ReplicationSlotType
{
	RS_PHYSICAL,
	RS_LOGICAL
} ReplicationSlotType;


/* ------------------------------------------------------------------------
 *  Below copied from:
 *   - src/backend/replication/slot.c
 *   - src/include/replication/slot.h
 * ------------------------------------------------------------------------
 */



/* size of version independent data */
#define ReplicationSlotOnDiskConstantSize \
	offsetof(ReplicationSlotOnDisk, slotdata)
/* size of the part of the slot not covered by the checksum */
#define SnapBuildOnDiskNotChecksummedSize \
	offsetof(ReplicationSlotOnDisk, version)
/* size of the part covered by the checksum */
#define SnapBuildOnDiskChecksummedSize \
	sizeof(ReplicationSlotOnDisk) - SnapBuildOnDiskNotChecksummedSize
/* size of the slot data that is version dependent */
#define ReplicationSlotOnDiskV2Size \
	sizeof(ReplicationSlotOnDisk) - ReplicationSlotOnDiskConstantSize

#define SLOT_MAGIC		0x1051CA1		/* format identifier */

/*
 * As of 2016-04, there is only one format version (number 2) in existence
 * in all PostgreSQL release versions and in the development code since slots
 * were introduced in 9.4; format version 1 was deprecated with 9.4rc1
 * (commit ec5896aed3c01da24c1f335f138817e9890d68b6).
 */
#define MIN_SLOT_VERSION	2		/* earliest slot format version we know about */
#define MAX_SLOT_VERSION	2		/* latest slot format version we know about */


/*
 * Behaviour of replication slots, upon release or crash.
 *
 * Slots marked as PERSISTENT are crashsafe and will not be dropped when
 * released. Slots marked as EPHEMERAL will be dropped when released or after
 * restarts.
 *
 * EPHEMERAL slots can be made PERSISTENT by calling ReplicationSlotPersist().
 */
typedef enum ReplicationSlotPersistency
{
	RS_PERSISTENT,
	RS_EPHEMERAL
} ReplicationSlotPersistency;

/*
 * On-Disk data of a replication slot, preserved across restarts.
 */
typedef struct ReplicationSlotPersistentData
{
	/* The slot's identifier */
	NameData	name;

	/* database the slot is active on */
	Oid			database;

	/*
	 * The slot's behaviour when being dropped (or restored after a crash).
	 */
	ReplicationSlotPersistency persistency;

	/*
	 * xmin horizon for data
	 *
	 * NB: This may represent a value that hasn't been written to disk yet;
	 * see notes for effective_xmin, below.
	 */
	TransactionId xmin;

	/*
	 * xmin horizon for catalog tuples
	 *
	 * NB: This may represent a value that hasn't been written to disk yet;
	 * see notes for effective_xmin, below.
	 */
	TransactionId catalog_xmin;

	/* oldest LSN that might be required by this replication slot */
	XLogRecPtr	restart_lsn;

	/* oldest LSN that the client has acked receipt for */
	XLogRecPtr	confirmed_flush;

	/* plugin name */
	NameData	plugin;
} ReplicationSlotPersistentData;


/*
 * Replication slot on-disk data structure.
 */
typedef struct ReplicationSlotOnDisk
{
	/* first part of this struct needs to be version independent */

	/* data not covered by checksum */
	uint32		magic;
	pg_crc32c	checksum;

	/* data covered by checksum */
	uint32		version;
	uint32		length;

	/*
	 * The actual data in the slot that follows can differ based on the above
	 * 'version'.
	 */

	ReplicationSlotPersistentData slotdata;
} ReplicationSlotOnDisk;


#endif   /* PG_REPLSLOT_READER_H */

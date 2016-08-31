pg_replslot_reader
==================

Utility to display information about replication slots stored in
the PostgreSQL data directory, regardless of whether the server
is running or not.

It is designed to be independent of PostgreSQL version, provided the
version is at least 9.4 (when replication slots were introduced).

Replication slot state is kept as on-disk data, not in the system
catalogue, due to the requirement to make them available on standbys
for cascaded standby setups.

This utility serves little practical purpose.

Installation
------------

Simply:

    USE_PGXS=1 make install

which will place the `pg_replslot_reader` binary in the PostgreSQL
directory determined by whatever `pg_config` is found in the path.

Usage
-----

Provide the data directory to examine with `-D/--pgdata`.

Example:

    pg_replslot_reader -D ~/devel/postgres/data/head
    Checking directory /home/barwick/devel/postgres/data/head...
    1 replication slot(s) found

    foo
    ---
      Type: physical
      Persistency: persistent
      Version: 2
      Length: 160


Copyright
---------

`pg_replslot_reader` is licensed under the same terms as the PostgreSQL
project itself.
/*
 * Copyright (c) 2022 3mdeb <contact@3mdeb.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>

#include <sys/types.h>
#include <sys/efi.h>
#include <sys/efiio.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <err.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>
#include <uuid.h>

#define TABLE_MAX_LEN 30

static void efi_table_print_esrt(void *data);
static void usage(void);
static void fail(const char msg[]);

struct efi_table_op {
	char name[TABLE_MAX_LEN];
	void (*parse) (void *);
	struct uuid uuid;
};

static const struct efi_table_op efi_table_ops[] = {
	{ .name = "esrt",
	  .parse = efi_table_print_esrt,
	  .uuid = EFI_TABLE_ESRT },
};

int
main(int argc, char **argv)
{
	struct efi_get_table_ioc table = {
		.ptr = NULL,
		.buf_len = 0,
		.table_len = 0
	};
	int efi_fd, ch, rc = 1, efi_idx = -1;
	bool got_table = false;
	bool table_set = false;
	bool uuid_set = false;
	struct option longopts[] = {
		{ "uuid",  required_argument, NULL, 'u' },
		{ "table", required_argument, NULL, 't' },
		{ NULL,    0,                 NULL,  0  }
	};

	while ((ch = getopt_long(argc, argv, "u:t:", longopts, NULL)) != -1) {
		switch (ch) {
		case 'u':
		{
			char *uuid_str = optarg;
			struct uuid uuid;
			uint32_t status;
			size_t n;

			uuid_set = 1;

			uuid_from_string(uuid_str, &uuid, &status);
			if (status != uuid_s_ok)
				fail("invalid UUID");

			for (n = 0; n < nitems(efi_table_ops); n++) {
				if (!memcmp(&uuid, &efi_table_ops[n].uuid,
						sizeof(uuid))) {
					efi_idx = n;
					got_table = true;
					break;
				}
			}
			break;
		}
		case 't':
		{
			char *table_name = optarg;
			size_t n;

			table_set = true;

			for (n = 0; n < nitems(efi_table_ops); n++) {
				if (!strcmp(table_name,
						efi_table_ops[n].name)) {
					efi_idx = n;
					got_table = true;
					break;
				}
			}

			if (!got_table)
				fail("unsupported efi table");
			break;
		}
		default:
			usage();
		}
	}

	if (!table_set && !uuid_set)
		fail("table is not set");

	if (!got_table)
		fail("unsupported table");

	efi_fd = open("/dev/efi", O_RDWR);
	if (efi_fd < 0)
		fail("/dev/efi");

	table.uuid = efi_table_ops[efi_idx].uuid;
	if (ioctl(efi_fd, EFIIOC_GET_TABLE, &table) == -1)
		fail(NULL);

	table.ptr = malloc(table.table_len);
	table.buf_len = table.table_len;

	if (ioctl(efi_fd, EFIIOC_GET_TABLE, &table) == -1)
		fail(NULL);
	close(efi_fd);

	efi_table_ops[efi_idx].parse(table.ptr);
	return (rc);
}

static void
efi_table_print_esrt(void *data)
{
	struct efi_esrt_table *esrt = data;
	struct efi_esrt_entry_v1 *esre = (void *)esrt->entries;

	printf("ESRT FwResourceCount = %d\n", esrt->fw_resource_count);

	uint32_t i;
	for (i = 0; i < esrt->fw_resource_count; i++) {
		char *uuid = NULL;
		uuid_to_string(&esre[i].fw_class, &uuid, NULL);

		printf("ESRT[%d]:\n", i);
		printf("  FwClass: %s\n", uuid);
		printf("  FwType: 0x%08x\n", esre[i].fw_type);
		printf("  FwVersion: 0x%08x\n", esre[i].fw_version);
		printf("  LowestSupportedFwVersion: 0x%08x\n",
			   esre[i].lowest_supported_fw_version);
		printf("  CapsuleFlags: 0x%08x\n",
			   esre[i].capsule_flags);
		printf("  LastAttemptVersion: 0x%08x\n",
			   esre[i].last_attempt_version);
		printf("  LastAttemptStatus: 0x%08x\n",
			   esre[i].last_attempt_status);

		free(uuid);
	}
}

static void
usage(void)
{
	printf("usage: efitable [-d uuid | -t name]\n");
	exit(1);
}

static void
fail(const char msg[])
{
	if (msg != NULL)
		fprintf(stderr, "%s\n", msg);
	exit(1);
}

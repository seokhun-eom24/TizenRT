/****************************************************************************
 *
 * Copyright 2019 Samsung Electronics All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific
 * language governing permissions and limitations under the License.
 *
 ****************************************************************************/

/// @file memory_fragmentation_test.c

/// @brief Heap stress test for memory fragmentation measurement.
///        Allocates and frees memory blocks with realistic size distribution
///        to simulate real-world memory usage patterns.

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <tinyara/config.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#ifdef CONFIG_DEBUG_MM_HEAPINFO
#include <tinyara/mm/mm.h>
#include <tinyara/fs/ioctl.h>
#include <tinyara/mminfo.h>
#endif

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define MEMFRAG_TRACK_BYTES 131072	/* 128KB for 5000+ entries */

#define MEMFRAG_WEIGHT_SMALL 85
#define MEMFRAG_WEIGHT_MEDIUM 12
#define MEMFRAG_WEIGHT_LARGE 3

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct memfrag_alloc_entry {
	void *ptr;
	size_t size;
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

static struct memfrag_alloc_entry *g_entries;
static int g_capacity;
static int g_active_count;
static size_t g_active_bytes;
static uint32_t g_prng_state = 1;

static const size_t g_sizes_small[] = {
	36, 36, 36, 36, 36, 36, 36, 36,	/* 36 is dominant (308k occurrences) */
	24, 24, 16, 16, 40, 40,
	8, 33, 48, 47, 56, 12, 13,
	23, 20, 26, 27, 28, 30, 32, 52, 64
};

static const size_t g_sizes_medium[] = {
	120, 120, 108, 108,	/* 120: 1019, 108: 816 occurrences */
	128, 129, 256, 316,
	512, 612, 1018, 1024
};

static const size_t g_sizes_large[] = {
	1796, 1936, 1952, 1968, 2048,
	4096, 4096, 4688, 5120,
	8192, 10240, 16384, 32768,
	64000, 98795, 131072, 262144,
	307202, 319296, 512000
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static uint32_t pseudo_random(void)
{
	g_prng_state = g_prng_state * 1103515245u + 12345u;
	return g_prng_state;
}

static void memfrag_set_seed(uint32_t seed)
{
	if (seed == 0) {
		seed = 1;
	}
	g_prng_state = seed;
}

static size_t memfrag_select_size(void)
{
	uint32_t r = pseudo_random() % 100;
	if (r < MEMFRAG_WEIGHT_SMALL) {
		return g_sizes_small[pseudo_random() % ARRAY_SIZE(g_sizes_small)];
	}
	if (r < (MEMFRAG_WEIGHT_SMALL + MEMFRAG_WEIGHT_MEDIUM)) {
		return g_sizes_medium[pseudo_random() % ARRAY_SIZE(g_sizes_medium)];
	}
	return g_sizes_large[pseudo_random() % ARRAY_SIZE(g_sizes_large)];
}

static void memfrag_cleanup_allocations(void)
{
	int i;

	if (!g_entries) {
		return;
	}

	/* Compact approach: active entries are always at indices 0..g_active_count-1 */
	for (i = 0; i < g_active_count; i++) {
		free(g_entries[i].ptr);
		g_entries[i].ptr = NULL;
		g_entries[i].size = 0;
	}

	g_active_count = 0;
	g_active_bytes = 0;
}

static int memfrag_init_table(uint32_t seed, bool set_seed)
{
	int capacity;

	if (g_entries) {
		memfrag_cleanup_allocations();
		free(g_entries);
		g_entries = NULL;
		g_capacity = 0;
	}

	g_entries = (struct memfrag_alloc_entry *)malloc(MEMFRAG_TRACK_BYTES);
	if (!g_entries) {
		printf("init: failed to allocate tracking table (%d bytes)\n", MEMFRAG_TRACK_BYTES);
		return -1;
	}

	memset(g_entries, 0, MEMFRAG_TRACK_BYTES);
	capacity = (int)(MEMFRAG_TRACK_BYTES / sizeof(struct memfrag_alloc_entry));
	if (capacity <= 0) {
		free(g_entries);
		g_entries = NULL;
		printf("init: tracking table too small\n");
		return -1;
	}

	g_capacity = capacity;
	g_active_count = 0;
	g_active_bytes = 0;

	if (set_seed) {
		memfrag_set_seed(seed);
	}

	printf("init: table_bytes=%d capacity=%d seed=%u\n",
		MEMFRAG_TRACK_BYTES, g_capacity, (unsigned int)g_prng_state);
	return 0;
}

/* Compact approach: no need for find_free_slot or find_nth_active
 * - malloc: append at g_active_count (O(1))
 * - free: swap with last entry (O(1))
 */

static void memfrag_log_status(const char *tag)
{
	printf("%s: active=%d bytes=%zu capacity=%d\n",
		tag, g_active_count, g_active_bytes, g_capacity);
}

static void memfrag_dump_heapinfo(void)
{
#ifdef CONFIG_DEBUG_MM_HEAPINFO
	int fd;
	heapinfo_option_t option;

	fd = open(MMINFO_DRVPATH, O_RDONLY);
	if (fd < 0) {
		printf("heapinfo: open %s failed\n", MMINFO_DRVPATH);
		return;
	}

	memset(&option, 0, sizeof(option));
	option.heap_type = HEAPINFO_HEAP_TYPE_BINARY;
	strncpy(option.app_name, CONFIG_APP1_BIN_NAME, BIN_NAME_MAX - 1);
	option.app_name[BIN_NAME_MAX - 1] = '\0';

	if (ioctl(fd, MMINFOIOC_PARSE, (unsigned long)&option) < 0) {
		printf("heapinfo: ioctl parse failed\n");
	}

#ifndef CONFIG_DEBUG_CHECK_FRAGMENTATION
	printf("heapinfo: fragmentation detail requires CONFIG_DEBUG_CHECK_FRAGMENTATION\n");
#endif

	close(fd);
#else
	printf("heapinfo: not available (CONFIG_DEBUG_MM_HEAPINFO disabled)\n");
#endif
}

static bool memfrag_parse_size(const char *text, size_t *value)
{
	char *endptr;
	unsigned long parsed;

	if (!text || !value) {
		return false;
	}

	parsed = strtoul(text, &endptr, 10);
	if (endptr == text || *endptr != '\0') {
		return false;
	}

	*value = (size_t)parsed;
	return true;
}

static bool memfrag_parse_ratio(const char *text, int *numer, int *denom)
{
	char *slash;
	char *endptr;
	long n, d;

	if (!text || !numer || !denom) {
		return false;
	}

	slash = strchr(text, '/');
	if (!slash) {
		return false;
	}

	n = strtol(text, &endptr, 10);
	if (endptr != slash || n <= 0) {
		return false;
	}

	d = strtol(slash + 1, &endptr, 10);
	if (*endptr != '\0' || d <= 0 || n > d) {
		return false;
	}

	*numer = (int)n;
	*denom = (int)d;
	return true;
}

static void memfrag_print_usage(const char *prog)
{
	printf("Usage:\n");
	printf("  %s init [seed]\n", prog);
	printf("  %s seed <seed>\n", prog);
	printf("  %s malloc <total_bytes>\n", prog);
	printf("  %s free <count>\n", prog);
	printf("  %s loop <bytes> <ratio> <count>\n", prog);
	printf("  %s info\n", prog);
	printf("  %s help\n", prog);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

#ifdef CONFIG_BUILD_KERNEL
int main(int argc, FAR char *argv[])
#else
int memfrag_main(int argc, char *argv[])
#endif
{
	const char *cmd;
	size_t value = 0;
	size_t remaining;
	size_t allocated_bytes = 0;
	int alloc_count = 0;
	int free_count = 0;
	int slot;
	size_t chunk;
	const char *fail_reason = NULL;

	if (argc < 2) {
		memfrag_print_usage(argv[0]);
		return -1;
	}

	cmd = argv[1];
	if (strcmp(cmd, "help") == 0) {
		memfrag_print_usage(argv[0]);
		return 0;
	}

	if (strcmp(cmd, "init") == 0) {
		int ret;

		if (argc >= 3) {
			if (!memfrag_parse_size(argv[2], &value)) {
				printf("init: invalid seed\n");
				return -1;
			}
			ret = memfrag_init_table((uint32_t)value, true);
		} else {
			ret = memfrag_init_table(0, false);
		}
		if (ret < 0) {
			return ret;
		}
		memfrag_log_status("init");
		return 0;
	}

	if (strcmp(cmd, "seed") == 0) {
		if (argc < 3 || !memfrag_parse_size(argv[2], &value)) {
			printf("seed: invalid seed\n");
			return -1;
		}
		memfrag_set_seed((uint32_t)value);
		printf("seed: %u\n", (unsigned int)g_prng_state);
		memfrag_log_status("seed");
		return 0;
	}

	if (strcmp(cmd, "info") == 0) {
		memfrag_log_status("info");
		memfrag_dump_heapinfo();
		return 0;
	}

	if (!g_entries) {
		printf("error: run 'memfrag init' first\n");
		return -1;
	}

	if (strcmp(cmd, "malloc") == 0) {
		if (argc < 3 || !memfrag_parse_size(argv[2], &value) || value == 0) {
			printf("malloc: invalid size\n");
			return -1;
		}

		remaining = value;
		while (remaining > 0) {
			/* Compact: just use g_active_count as slot (O(1)) */
			if (g_active_count >= g_capacity) {
				fail_reason = "table full";
				break;
			}

			chunk = memfrag_select_size();
			if (chunk > remaining) {
				chunk = remaining;
			}

			slot = g_active_count;
			g_entries[slot].ptr = malloc(chunk);
			if (!g_entries[slot].ptr) {
				fail_reason = "malloc failed";
				break;
			}

			g_entries[slot].size = chunk;
			g_active_count++;
			g_active_bytes += chunk;

			alloc_count++;
			allocated_bytes += chunk;
			remaining -= chunk;
		}

		printf("malloc: requested=%zu bytes, allocated=%zu bytes in %d blocks\n",
			value, allocated_bytes, alloc_count);
		if (fail_reason) {
			printf("malloc: stopped (%s)\n", fail_reason);
		}
		memfrag_log_status("malloc");
		return 0;
	}

	if (strcmp(cmd, "free") == 0) {
		if (argc < 3 || !memfrag_parse_size(argv[2], &value) || value == 0) {
			printf("free: invalid count\n");
			return -1;
		}

		while (free_count < (int)value && g_active_count > 0) {
			/* Compact: random index in [0, g_active_count-1], then swap with last (O(1)) */
			int target = (int)(pseudo_random() % (uint32_t)g_active_count);
			int last = g_active_count - 1;

			free(g_entries[target].ptr);
			g_active_bytes -= g_entries[target].size;

			/* Swap with last entry to keep array compact */
			if (target != last) {
				g_entries[target].ptr = g_entries[last].ptr;
				g_entries[target].size = g_entries[last].size;
			}
			g_entries[last].ptr = NULL;
			g_entries[last].size = 0;

			g_active_count--;
			free_count++;
		}

		printf("free: requested=%zu blocks, freed=%d blocks\n",
			value, free_count);
		if (g_active_count == 0 && free_count < (int)value) {
			printf("free: stopped (no active allocations)\n");
		}
		memfrag_log_status("free");
		return 0;
	}

	if (strcmp(cmd, "loop") == 0) {
		size_t loop_bytes;
		int numer, denom;
		size_t loop_count;
		size_t iter;
		int free_target;
		int i;

		if (argc < 5) {
			printf("loop: usage: loop <bytes> <ratio> <count>\n");
			return -1;
		}

		if (!memfrag_parse_size(argv[2], &loop_bytes) || loop_bytes == 0) {
			printf("loop: invalid bytes\n");
			return -1;
		}

		if (!memfrag_parse_ratio(argv[3], &numer, &denom)) {
			printf("loop: invalid ratio (use format like 1/2)\n");
			return -1;
		}

		if (!memfrag_parse_size(argv[4], &loop_count) || loop_count == 0) {
			printf("loop: invalid count\n");
			return -1;
		}

		printf("loop: bytes=%zu ratio=%d/%d count=%zu\n",
			loop_bytes, numer, denom, loop_count);

		for (iter = 1; iter <= loop_count; iter++) {
			/* malloc phase */
			remaining = loop_bytes;
			alloc_count = 0;
			allocated_bytes = 0;
			fail_reason = NULL;

			while (remaining > 0) {
				if (g_active_count >= g_capacity) {
					fail_reason = "table full";
					break;
				}

				chunk = memfrag_select_size();
				if (chunk > remaining) {
					chunk = remaining;
				}

				slot = g_active_count;
				g_entries[slot].ptr = malloc(chunk);
				if (!g_entries[slot].ptr) {
					fail_reason = "malloc failed";
					break;
				}

				g_entries[slot].size = chunk;
				g_active_count++;
				g_active_bytes += chunk;

				alloc_count++;
				allocated_bytes += chunk;
				remaining -= chunk;
			}

			/* free phase: free (active_count * numer / denom) blocks */
			free_target = (g_active_count * numer) / denom;
			free_count = 0;

			for (i = 0; i < free_target && g_active_count > 0; i++) {
				int target = (int)(pseudo_random() % (uint32_t)g_active_count);
				int last = g_active_count - 1;

				free(g_entries[target].ptr);
				g_active_bytes -= g_entries[target].size;

				if (target != last) {
					g_entries[target].ptr = g_entries[last].ptr;
					g_entries[target].size = g_entries[last].size;
				}
				g_entries[last].ptr = NULL;
				g_entries[last].size = 0;

				g_active_count--;
				free_count++;
			}

			printf("loop[%zu]: malloc %zu bytes (%d blocks), free %d/%d (%d blocks)\n",
				iter, allocated_bytes, alloc_count, numer, denom, free_count);
			if (fail_reason) {
				printf("loop[%zu]: malloc stopped (%s)\n", iter, fail_reason);
			}
			memfrag_log_status("loop");
		}

		memfrag_dump_heapinfo();
		return 0;
	}

	printf("unknown command: %s\n", cmd);
	memfrag_print_usage(argv[0]);
	return -1;
}

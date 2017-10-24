/*
 *  DA probe
 *
 * Copyright (c) 2000 - 2014 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact:
 *
 * Cherepanov Vitaliy <v.cherepanov@samsung.com>
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation; either version 2.1 of the License, or (at your option)
 * any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * Contributors:
 * - S-Core Co., Ltd
 *
 */

#include <unistd.h>
#include "dahelper.h"
#include "real_functions.h"

struct data_list_t {
	union {
		void *next;
		void *first;
	};
	union {
		void *data;
		void *tail;
	};
	union {
		uint32_t hash;
		uint32_t count;
	};
};

struct map_t {
	uint32_t hash;
	int is_instrument;
	void *addr;
	void *endaddr;
	char permissions[20];
	long long offset;
	char device[256];
	long long inode;
	char filename[1024]; /* TODO dynamic allocation */
};

typedef int cmp_by_hash_and_name_t(void *a, void *b);

/* variables */

static pthread_mutex_t maps_lock = PTHREAD_MUTEX_INITIALIZER;

char **map_inst_list = NULL;
char *map_inst_list_set = NULL;
uint32_t map_inst_count = 0;

static struct data_list_t sections_list = {
	{NULL}, /* head element */
	{NULL}, /* tail for head list el */
	{0},    /* count */
};

static uint32_t addr_hash_table_el_count_total = 0;
static uint32_t addr_hash_table_el_count_buzy = 0;
static struct map_t **addr_hash_table = NULL;

static uint32_t name_hash_table_el_count_total = 0;
static uint32_t name_hash_table_el_count_buzy = 0;
static struct map_t **name_hash_table = NULL;

#define MAX_READERS_COUNT 8
static sem_t readers_mutex;

/*
 *
 * Functions
 *
 */

static uint32_t calc_string_hash(char *str)
{
	uint32_t res = 0;
	while (*str != '\0') {
		res += *str;
		res <<= 1;
		str++;
	}
	return res;
}

static struct data_list_t *get_first_el(struct data_list_t *list)
{
	return (struct data_list_t *)list->first;
}

static struct data_list_t *get_next_el(struct data_list_t *el)
{
	return (struct data_list_t *)el->next;
}

static void print_map(struct map_t *map)
{
	PRINTMSG("mapp-> %p-%p %s %llx %s %llx %s",
		  map->addr,
		  map->endaddr,
		  map->permissions,
		  map->offset,
		  map->device,
		  map->inode,
		  map->filename);
}

static int read_mapping_line(FILE *mapfile, struct map_t *m)
{
	char ch1, ch2;
	int ret = 0;

	if (m == NULL) {
		PRINTERR("map_t param is NULL\n");
		return 0;
	}

	ret = fscanf(mapfile, "%p-%p %s %llx %s %llx%c%c",
		     &m->addr,
		     &m->endaddr,
		     (char *)m->permissions,
		     &m->offset,
		     (char *)m->device,
		     &m->inode,
		     &ch1, &ch2);

	m->is_instrument = 0;
	if (ret > 0 && ret != EOF) {
		int len = 0;

		if (ch2 != '\n') {
			ret = (fgets((char *)m->filename, sizeof(m->filename), mapfile) != NULL);
			if (ret) {
				/* remove leading white spaces */
				if (m->filename[0] == ' ') {
					char *p = m->filename;
					while (*p == ' ')
						p++;
					len = strlen(p);
					memmove(m->filename, p, len);
				} else
					len = strlen(m->filename);
				if (len > 0)
					len--;
			}
		}

		m->filename[len] = '\0';

		return 1;
	} else
		return 0;

	return (ret != 0 && ret != EOF);
}

static void print_list()
{
	struct data_list_t *p = NULL;
	struct map_t *m = NULL;
	uint32_t i = 0;

	PRINTMSG("=====================================");
	for (p = get_first_el(&sections_list); p != NULL && i < sections_list.count; p = p->next) {
		m = (struct map_t *)p->data;
		PRINTMSG("mapp[%03d]-> %p-%p %s %llx %s %llx %s",
			 ++i,
			 m->addr,
			 m->endaddr,
			 m->permissions,
			 m->offset,
			 m->device,
			 m->inode,
			 m->filename);
		usleep(500);
	}

}

static struct data_list_t *new_data(void)
{
	struct data_list_t *el = (*real_malloc)(sizeof(*el));
	el->next = NULL;
	el->hash = 0;
	el->data = NULL;
	return el;
}

static int data_list_append(struct data_list_t *head, struct data_list_t *el)
{
	if (head->first == NULL) {
		// empty list
		head->first = el;
		head->tail = el;
	} else {
		((struct data_list_t *)head->tail)->next = el;
		head->tail = el;
	}
	head->count++;
	return 0;
}

static int add_to_map_list(struct map_t **m)
{
	struct data_list_t *el = new_data();
	el->data = (void *)(*m);
	el->hash = (*m)->hash;
	data_list_append(&sections_list, el);
	*m = NULL;

	return 0;
}

static void print_list_sorted(struct map_t **list)
{
	struct map_t *m = NULL;
	uint32_t i = 0;

	PRINTMSG("=====================================");
	for (i = 0; i < sections_list.count; i++) {
		m = list[i];
		if (m->is_instrument)
		PRINTMSG("mapp[%03d]-> %016X %d <%s>",
			 i + 1,
			 m->hash,
			 m->is_instrument,
			 m->filename);
		usleep(500);
	}
}

static int realloc_array(struct map_t ***p, uint32_t *count_total,
			 uint32_t *count_buzy, cmp_by_hash_and_name_t cmp_func)
{
	uint32_t i, j;
	struct map_t **list;
	struct data_list_t *el;

	if (*p == NULL) {
		/* first init */
		if (sections_list.count != 0) {
			*p = (*real_malloc)(sizeof(**p) * sections_list.count);
			*count_total = sections_list.count;
		}
	} else {
		/* not first init */
		if (sections_list.count > *count_total) {
			/* not enaught space in hash */
			*p = realloc(*p, sizeof(**p) * sections_list.count);
			*count_total = sections_list.count;
		}
	}
	*count_buzy = sections_list.count;

	/* fill table */
	list = *p;
	el = get_first_el(&sections_list);

	for (i = 0; i < sections_list.count; i++) {
		list[i] = (struct map_t *)el->data;
		el = get_next_el(el);
	}

	/* sort */
	uint32_t max = *count_buzy;
	struct map_t *tmp;
	if (cmp_func) {
		for (i = 0; i < max; i++)
			for (j = i + 1; j < *count_buzy; j++) {
				if (cmp_func(list[i], list[j]) > 0) {
					tmp = list[i];
					list[i] = list[j];
					list[j] = tmp;
				}
			}
		return 0;
	}

	return 0;
}

static int create_addr_hash_table()
{
	realloc_array(&addr_hash_table, &addr_hash_table_el_count_total,
		      &addr_hash_table_el_count_buzy, NULL);

	return 0;
}

static int cmp_by_hash_and_name(void *a, void *b)
{
	if (((struct map_t *)a)->hash != ((struct map_t *)b)->hash) {
		/* hash value is major priority */
		if (((struct map_t *)a)->hash > ((struct map_t *)b)->hash)
			return 1;
		else /* if i(a->hash < b->hash) */
			return -1;
	} else {
		/* hash equel */
		/* retun filename cmp */
		return strcmp(((struct map_t *)a)->filename,
			      ((struct map_t *)b)->filename);
	}

	/* never happens */
	return 0;
}

static int create_name_hash_table()
{
	realloc_array(&name_hash_table, &name_hash_table_el_count_total,
		      &name_hash_table_el_count_buzy, cmp_by_hash_and_name);

	return 0;
}

static struct map_t **get_map_by_filename(char *filename, int *count)
{
	uint32_t hash = calc_string_hash(filename);
	struct map_t **res = NULL;
	uint32_t left, right, cur;

	*count = 0;

	if (name_hash_table_el_count_buzy == 0 ||
	    name_hash_table[0]->hash > hash ||
	    name_hash_table[name_hash_table_el_count_buzy - 1]->hash < hash) {
		goto find_exit;
	}

	left = 0;
	right = name_hash_table_el_count_buzy - 1;
	cur = (left + right) >> 1;
	while (left < right) {
		if (hash < name_hash_table[cur]->hash) {
			right = cur - 1;
			cur = (left + right) >> 1;
			continue;
		}
		if (hash > name_hash_table[cur]->hash) {
			left = cur + 1;
			cur = (left + right) >> 1;
			continue;
		}
		break;
	}

	/* resolve collisions */
	if (name_hash_table[cur]->hash == hash) {

		/* get first with same hash */
		while (1) {
			if (cur == 0)
				/* top of list */
				break;
			if (name_hash_table[cur - 1]->hash != hash)
				/* previous element have different hash */
				break;
			/* previous element have the same hash */
			cur--;
		}

		/* get first with same hash and filename */
		while (1) {
			if (cur > name_hash_table_el_count_buzy - 1)
				/* top of list */
				break;
			if (name_hash_table[cur]->hash != hash)
				/* previous element have different hash */
				break;
			if (strncmp(name_hash_table[cur]->filename, filename, strlen(filename)) == 0)
				break;
			/* previous element have the same hash */
			cur++;
		}

		/* calculate sections count */
		while (cur <= (name_hash_table_el_count_buzy - 1) &&
		       name_hash_table[cur]->hash == hash &&
		       strncmp(name_hash_table[cur]->filename, filename, strlen(filename)) == 0) {
			if (res == NULL)
				res = &name_hash_table[cur];
			cur++;
			*count = *count + 1;
		}
	}

find_exit:
	return res;
}

static int update_is_instrument_lib_attr_nolock()
{
	uint32_t i;
	struct map_t **map;

	/* clear is_instrument fields */
	for (i = 0; i < name_hash_table_el_count_buzy; i++)
		name_hash_table[i]->is_instrument = 0;

	/* set is_instrument fields */
	for (i = 0; i < map_inst_count; i++) {
		int count = 0;
		map = get_map_by_filename(map_inst_list[i], &count);
		for (;count > 0; count--) {
			PRINTMSG("set 1!!! = %s [%p:%p]", (*map)->filename,
				  (*map)->addr, (*map)->endaddr);
			(*map)->is_instrument = 1;
			map++;
		}
	}

	return 0;
	print_list_sorted(name_hash_table);
}

static struct map_t *get_map_by_addr(void *addr)
{
	struct map_t *d = NULL;
	struct map_t *res = NULL;
	uint32_t left, right, cur;

	if (addr_hash_table_el_count_buzy == 0)
		goto find_exit;

	if (addr < addr_hash_table[0]->addr)
		goto find_exit;

	if (addr > addr_hash_table[addr_hash_table_el_count_buzy - 1]->endaddr)
		goto find_exit;

	left = 0;
	right = addr_hash_table_el_count_buzy - 1;
	cur = 0;
	while (left < right) {
		cur = (left + right) >> 1;

		d = (struct map_t *)addr_hash_table[cur];
		if (addr < d->addr) {
			right = cur - 1;
			continue;
		}
		if (addr > d->endaddr) {
			left = cur + 1;
			continue;
		}

		/* success */
		return (addr_hash_table[cur]);
	}

	if (left == right) {
		d = (struct map_t *)addr_hash_table[left];
		if ((d->addr <= addr) && (addr <= d->endaddr))
			res = addr_hash_table[left];
	}

find_exit:
	return res;
}

static void maps_reader_lock_all()
{
	int i;
	for (i = 0; i < MAX_READERS_COUNT; i++)
		sem_wait(&readers_mutex);
}

static void maps_reader_unlock_all()
{
	int i;
	for (i = 0; i < MAX_READERS_COUNT; i++)
		sem_post(&readers_mutex);
}

static inline void maps_reader_lock()
{
	sem_wait(&readers_mutex);	/* down semaphore */
}

static inline void maps_reader_unlock()
{
	sem_post(&readers_mutex);	/* up semaphore */
}

static inline void maps_writer_lock()
{
	pthread_mutex_lock(&maps_lock);
}

static inline void maps_writer_unlock()
{
	pthread_mutex_unlock(&maps_lock);
}

/* TODO refactor this function to alloc map_inst_list_set and copy it frm src*/
// WARNING! this function use maps_set and set it to NULL
// so first param must be malloced and do not free maps_set after call
int set_map_inst_list(char **maps_set, uint32_t maps_count)
{
	int res = 0;
	uint32_t i = 0;
	char *p;

	maps_writer_lock();
	maps_reader_lock_all();

	if (map_inst_list != NULL) {
		free(map_inst_list);
		map_inst_list = NULL;
	}

	if (map_inst_list_set != NULL) {
		free(map_inst_list_set);
		map_inst_list_set = NULL;
	}

	map_inst_list = real_malloc(sizeof(*map_inst_list) * maps_count);
	if (maps_count != 0 && map_inst_list == NULL) {
		PRINTERR("Cannot allocate data for map_inst_list\n");
		res = -1;
		goto unlock_exit;
	}

	if (maps_set != NULL && *maps_set != NULL && map_inst_list != NULL) {
		map_inst_list_set = *maps_set;
		map_inst_count = maps_count;

		/* add library mapping names */
		p = map_inst_list_set + sizeof(maps_count);
		for (i = 0; i < maps_count; i++) {
			map_inst_list[i] = p;
			p += strlen(p) + 1;
			PRINTMSG("-------> %s", map_inst_list[i]);
		}
	} else {
		map_inst_count = 0;
	}

	res = update_is_instrument_lib_attr_nolock();

	if (maps_set != NULL)
		*maps_set = NULL;

unlock_exit:
	maps_reader_unlock_all();
	maps_writer_unlock();

	return res;
}

int maps_make()
{
	int res = 0;
	maps_writer_lock();
	maps_reader_lock_all();
	FILE *f = fopen("/proc/self/maps", "r");
	struct map_t *map = NULL;

	/* read to exists locations */
	struct data_list_t *cur = get_first_el(&sections_list);
	sections_list.count = 0;
	while (cur != NULL) {
		map = (struct map_t *)cur->data;
		if (!read_mapping_line(f, map))
			break;
		if (map->permissions[2] == 'x') {
			map->hash = calc_string_hash(map->filename);
			cur->hash = map->hash;
			sections_list.count++;
			cur = get_next_el(cur);
		}
	}

	/* add  locations */
	map = (*real_malloc)(sizeof(*map));
	if (map == NULL) {
		PRINTERR("Can not alloc data for map\n");
		res = -1;
		goto unlock_exit;
	}
	while (read_mapping_line(f, map)) {
		if (map->permissions[2] == 'x') {
			map->hash = calc_string_hash(map->filename);
			add_to_map_list(&map);
			if (map == NULL)
				map = (*real_malloc)(sizeof(*map));
		}
	}

	if (map != NULL)
		free(map);

	create_addr_hash_table();
	create_name_hash_table();

	update_is_instrument_lib_attr_nolock();

unlock_exit:
	fclose(f);

	maps_reader_unlock_all();
	maps_writer_unlock();

	return res;
}

int maps_print_maps_by_addr(const void *addr)
{
	int res = -1;
	struct map_t *map = NULL;

	maps_reader_lock();
	map = get_map_by_addr((void *)addr);
	if (map != NULL)
		print_map(map);
	maps_reader_unlock();

	return res;
}

int maps_is_instrument_section_by_addr_no_remap(const void *addr)
{
	int res = -1;
	struct map_t *map;

	maps_reader_lock();
	map = get_map_by_addr((void *)addr);
	if (map != NULL)
		res = map->is_instrument;
	maps_reader_unlock();

	return res;

}

/* interface function */
int maps_is_instrument_section_by_addr(const void *addr)
{
	int ret = maps_is_instrument_section_by_addr_no_remap(addr);

	if (ret == -1) {
		PRINTMSG("========> hz addr %p. remap", addr);
		if (maps_make() != 0) {
			PRINTERR("maps make failed\n");
			ret = -1;
			goto exit;
		}
		ret = maps_is_instrument_section_by_addr_no_remap(addr);
		if (ret == -1) {
			print_list();
			PRINTERR("!!!!unknown addr %p", addr);
			get_map_by_addr((void *)addr);
			sleep(2);
			exit(0);
			ret = 0;
		}
	}

exit:
	return ret;
}

/* must be called ones */
int maps_init()
{
	int res = 0;

	probeBlockStart();
	res = sem_init(&readers_mutex, 0, MAX_READERS_COUNT);
	set_map_inst_list(NULL, 0);
	probeBlockEnd();

	return res;
}

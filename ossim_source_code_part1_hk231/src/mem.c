#include "mem.h"
#include "stdlib.h"
#include "string.h"
#include <pthread.h>
#include <stdio.h>

static BYTE _ram[RAM_SIZE];

static struct
{
	uint32_t proc; // ID of process currently uses this page
	int index;	   // Index of the page in the list of pages allocated
	// to the process.
	int next; // The walkpage in the list. -1 if it is the last
			  // page.
} _mem_stat[NUM_PAGES];

static pthread_mutex_t mem_lock;

void init_mem(void)
{
	memset(_mem_stat, 0, sizeof(*_mem_stat) * NUM_PAGES);
	memset(_ram, 0, sizeof(BYTE) * RAM_SIZE);
	pthread_mutex_init(&mem_lock, NULL);
}

/* get offset of the virtual address */
static addr_t get_offset(addr_t addr)
{
	return addr & ~((~0U) << OFFSET_LEN);
}

/* get the first layer index */
static addr_t get_first_lv(addr_t addr)
{
	return addr >> (OFFSET_LEN + PAGE_LEN);
}

/* get the second layer index */
static addr_t get_second_lv(addr_t addr)
{
	return (addr >> OFFSET_LEN) - (get_first_lv(addr) << PAGE_LEN);
}

/* Search for page table table from the a segment table */
static struct trans_table_t *get_page_table(addr_t index, // Segment level index
											struct page_table_t *page_table)
{ // first level table

	/*
	 * TODO: Given the Segment index [index], you must go through each
	 * row of the segment table [page_table] and check if the v_index
	 * field of the row is equal to the index
	 *
	 * */

	int i;
	for (i = 0; i < page_table->size; i++)
	{
		// Enter your code here
		if (page_table->table[i].v_index == index)
		{
			return page_table->table[i].next_lv;
		}
	}
	return NULL;
}

/* Translate virtual address to physical address. If [virtual_addr] is valid,
 * return 1 and write its physical counterpart to [physical_addr].
 * Otherwise, return 0 */
static int translate(addr_t virtual_addr,	// Given virtual address
					 addr_t *physical_addr, // Physical address to be returned
					 struct pcb_t *proc)
{ // Process uses given virtual address

	/* Offset of the virtual address */
	addr_t offset = get_offset(virtual_addr);
	/* The first layer index */
	addr_t first_lv = get_first_lv(virtual_addr);
	/* The second layer index */
	addr_t second_lv = get_second_lv(virtual_addr);

	/* Search in the first level */
	struct trans_table_t *page_table = NULL;
	page_table = get_page_table(first_lv, proc->page_table);
	if (page_table == NULL)
	{
		return 0;
	}

	int i;
	for (i = 0; i < page_table->size; i++)
	{
		if (page_table->table[i].v_index == second_lv)
		{
			/* TODO: Concatenate the offset of the virtual addess
			 * to [p_index] field of page_table->table[i] to
			 * produce the correct physical address and save it to
			 * [*physical_addr]  */
			*physical_addr = page_table->table[i].p_index << OFFSET_LEN | offset;
			return 1;
		}
	}
	return 0;
}

addr_t alloc_mem(uint32_t size, struct pcb_t *proc)
{
	pthread_mutex_lock(&mem_lock);
	addr_t ret_mem = 0;
	/* TODO: Allocate [size] byte in the memory for the
	 * process [proc] and save the address of the first
	 * byte in the allocated memory region to [ret_mem].
	 * */

	uint32_t num_pages = ((size % PAGE_SIZE) == 0)
							 ? size / PAGE_SIZE
							 : size / PAGE_SIZE + 1; // Number of pages we will use
	int mem_avail = 0;								 // We could allocate new memory region or not?
	/* First we must check if the amount of free memory in
	 * virtual address space and physical address space is
	 * large enough to represent the amount of required
	 * memory. If so, set 1 to [mem_avail].
	 * Hint: check [proc] bit in each page of _mem_stat
	 * to know whether this page has been used by a process.
	 * For virtual memory space, check bp (break pointer).
	 * */

	uint32_t avai_pages = 0; // Number of available pages in _mem_stat
	for (int i = 0; i < NUM_PAGES; i++)
	{
		if (_mem_stat[i].proc == 0)
		{
			avai_pages++;
		}
	}
	uint32_t cond1 = num_pages * PAGE_SIZE + proc->bp;	// expectable size
	uint32_t cond2 = NUM_PAGES * PAGE_SIZE + PAGE_SIZE; // total size
	if (avai_pages >= num_pages && cond1 < cond2)
		mem_avail = 1;
	else
		ret_mem = 0;
	if (mem_avail)
	{
		/* We could allocate new memory region to the process */
		ret_mem = proc->bp;
		proc->bp += num_pages * PAGE_SIZE;
		/* Update status of physical pages which will be allocated
		 * to [proc] in _mem_stat. Tasks to do:
		 *  - Update [proc], [index], and [next] field
		 *  - Add entries to segment table page tables of [proc]
		 *    to ensure accesses to allocated memory slot is
		 *    valid. */
		uint32_t count = 0;
		for (int i = 0, start_rg = 0, save_pre_p = 0; i < NUM_PAGES; i++)
		{
			if (_mem_stat[i].proc == 0)
			{
				_mem_stat[i].proc = proc->pid;
				_mem_stat[i].index = start_rg;
				if (start_rg != 0)
					_mem_stat[save_pre_p].next = i;
				/*------------------------*/
				addr_t physical_addr = i << OFFSET_LEN;
				addr_t first_lv = get_first_lv(ret_mem + start_rg * PAGE_SIZE);
				addr_t second_lv = get_second_lv(ret_mem + start_rg * PAGE_SIZE);
				// addr_t offset = get_offset(ret_mem + start_rg * PAGE_SIZE);
				int check = 0;
				/*Allocating the 2nd level*/
				for (int n = 0; n < proc->page_table->size; n++)
				{
					if (proc->page_table->table[n].v_index == first_lv)
					{
						proc->page_table->table[n].next_lv->table[proc->page_table->table[n].next_lv->size].v_index = second_lv;
						proc->page_table->table[n].next_lv->table[proc->page_table->table[n].next_lv->size].p_index = physical_addr >> OFFSET_LEN;
						proc->page_table->table[n].next_lv->size++;
						check = 1;
						break;
					}
				}
				/*Initialize*/
				if (check == 0)
				{
					int n = proc->page_table->size;
					proc->page_table->size++;
					proc->page_table->table[n].next_lv = (struct trans_table_t *)malloc(sizeof(struct trans_table_t));
					proc->page_table->table[n].next_lv->size = 0;
					proc->page_table->table[n].next_lv->size++;
					proc->page_table->table[n].v_index = first_lv;
					proc->page_table->table[n].next_lv->table[0].v_index = second_lv;
					proc->page_table->table[n].next_lv->table[0].p_index = physical_addr >> OFFSET_LEN;
				}
				save_pre_p = i;
				start_rg++;
				count++;
				if (count == num_pages)
				{
					_mem_stat[save_pre_p].next = -1;
					break;
				}
			}
		}
	}
	pthread_mutex_unlock(&mem_lock);
	return ret_mem;
}

int free_mem(addr_t address, struct pcb_t *proc)
{

	/*TODO: Release memory region allocated by [proc]. The first byte of
	 * this region is indicated by [address]. Task to do:
	 *    - Set flag [proc] of physical page use by the memory block
	 *      back to zero to indicate that it is free.
	 *    - Remove unused entries in segment table and page tables of
	 *      the process [proc].
	 *    - Remember to use lock to protect the memory from other
	 *      processes.  */
	pthread_mutex_lock(&mem_lock);
	addr_t physical_addr;
	if (translate(address, &physical_addr, proc))
	{
		int walk;
		int i = 0;
		int start_rg = 0; // Start index of the region needing to delete
		// Find the index of physical_addr in _mem_stat
		while (i < NUM_PAGES)
		{
			if (physical_addr == i << OFFSET_LEN)
			{
				break;
			}
			i++;
		}
		// Delete the region
		walk = i;
		while (walk != -1)
		{
			_mem_stat[walk].proc = 0;
			walk = _mem_stat[walk].next; // Walking to the next index in _mem_stat to continue to delete
			/*Delete the page table and trans table based on first_lv and second_lv*/
			addr_t first_lv = get_first_lv(address + start_rg * PAGE_SIZE);
			addr_t second_lv = get_second_lv(address + start_rg * PAGE_SIZE);
			for (int n = 0; n < proc->page_table->size; n++)
			{
				if (proc->page_table->table[n].v_index == first_lv) // outer table
				{
					for (int m = 0; m < proc->page_table->table[n].next_lv->size; m++)
					{
						if (proc->page_table->table[n].next_lv->table[m].v_index == second_lv) // inner table
						{
							int k;
							for (k = m; k < proc->page_table->table[n].next_lv->size - 1; k++)
							{
								proc->page_table->table[n].next_lv->table[k].v_index = proc->page_table->table[n].next_lv->table[k + 1].v_index;
								proc->page_table->table[n].next_lv->table[k].p_index = proc->page_table->table[n].next_lv->table[k + 1].p_index;
							}
							proc->page_table->table[n].next_lv->table[k].v_index = 0;
							proc->page_table->table[n].next_lv->table[k].p_index = 0;
							proc->page_table->table[n].next_lv->size--;
							break;
						}
					}
					if (proc->page_table->table[n].next_lv->size == 0) // Invalid trans_table
					{
						free(proc->page_table->table[n].next_lv);
						int m = 0;
						for (m = n; m < proc->page_table->size - 1; m++)
						{
							proc->page_table->table[m].v_index = proc->page_table->table[m + 1].v_index;
							*proc->page_table->table[m].next_lv = *proc->page_table->table[m + 1].next_lv;
						}
						proc->page_table->table[m].v_index = 0;
						proc->page_table->table[m].next_lv = NULL;
						proc->page_table->size--;
					}
					break;
				}
			}
			start_rg++; // Walking to the next index in _mem_stat to continue to delete
		}
	}
	pthread_mutex_unlock(&mem_lock);
	return 0;
}

int read_mem(addr_t address, struct pcb_t *proc, BYTE *data)
{
	addr_t physical_addr;
	if (translate(address, &physical_addr, proc))
	{
		*data = _ram[physical_addr];
		return 0;
	}
	else
	{
		return 1;
	}
}

int write_mem(addr_t address, struct pcb_t *proc, BYTE data)
{
	addr_t physical_addr;
	if (translate(address, &physical_addr, proc))
	{
		_ram[physical_addr] = data;
		// dump();
		return 0;
	}
	else
	{
		return 1;
	}
}

void dump(void)
{
	int i;
	for (i = 0; i < NUM_PAGES; i++)
	{
		if (_mem_stat[i].proc != 0)
		{
			printf("%03d: ", i);
			printf("%05x-%05x - PID: %02d (idx %03d, nxt: %03d)\n",
				   i << OFFSET_LEN,
				   ((i + 1) << OFFSET_LEN) - 1,
				   _mem_stat[i].proc,
				   _mem_stat[i].index,
				   _mem_stat[i].next);
			int start_rg;
			for (start_rg = i << OFFSET_LEN;
				 start_rg < ((i + 1) << OFFSET_LEN) - 1;
				 start_rg++)
			{

				if (_ram[start_rg] != 0)
				{
					printf("\t%05x: %02x\n", start_rg, _ram[start_rg]);
				}
			}
		}
	}
}

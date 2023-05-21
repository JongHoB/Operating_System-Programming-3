/**********************************************************************
 * Copyright (c) 2020-2023
 *  Sang-Hoon Kim <sanghoonkim@ajou.ac.kr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTIABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 **********************************************************************/

#include <stdio.h>
#include <stdlib.h>

#include "types.h"
#include "list_head.h"
#include "vm.h"

/**
 * Ready queue of the system
 */
extern struct list_head processes;

/**
 * Currently running process
 */
extern struct process *current;

/**
 * Page Table Base Register that MMU will walk through for address translation
 */
extern struct pagetable *ptbr;

/**
 * TLB of the system.
 */
extern struct tlb_entry tlb[1UL << (PTES_PER_PAGE_SHIFT * 2)];


/**
 * The number of mappings for each page frame. Can be used to determine how
 * many processes are using the page frames.
 */
extern unsigned int mapcounts[];


/**
 * lookup_tlb(@vpn, @rw, @pfn)
 *
 * DESCRIPTION
 *   Translate @vpn of the current process through TLB. DO NOT make your own
 *   data structure for TLB, but should use the defined @tlb data structure
 *   to translate. If the requested VPN exists in the TLB and it has the same
 *   rw flag, return true with @pfn is set to its PFN. Otherwise, return false.
 *   The framework calls this function when needed, so do not call
 *   this function manually.
 *
 * RETURN
 *   Return true if the translation is cached in the TLB.
 *   Return false otherwise
 */
bool lookup_tlb(unsigned int vpn, unsigned int rw, unsigned int *pfn)
{
	return false;
}


/**
 * insert_tlb(@vpn, @rw, @pfn)
 *
 * DESCRIPTION
 *   Insert the mapping from @vpn to @pfn for @rw into the TLB. The framework will
 *   call this function when required, so no need to call this function manually.
 *   Note that if there exists an entry for @vpn already, just update it accordingly
 *   rather than removing it or creating a new entry.
 *   Also, in the current simulator, TLB is big enough to cache all the entries of
 *   the current page table, so don't worry about TLB entry eviction. ;-)
 */
void insert_tlb(unsigned int vpn, unsigned int rw, unsigned int pfn)
{
}


/**
 * alloc_page(@vpn, @rw)
 *
 * DESCRIPTION
 *   Allocate a page frame that is not allocated to any process, and map it
 *   to @vpn. When the system has multiple free pages, this function should
 *   allocate the page frame with the **smallest pfn**.
 *   You may construct the page table of the @current process. When the page
 *   is allocated with ACCESS_WRITE flag, the page may be later accessed for writes.
 *   However, the pages populated with ACCESS_READ should not be accessible with
 *   ACCESS_WRITE accesses.
 *
 * RETURN
 *   Return allocated page frame number.
 *   Return -1 if all page frames are allocated.
 */
unsigned int alloc_page(unsigned int vpn, unsigned int rw)
{
	int outer_pte_index=vpn/NR_PTES_PER_PAGE;
	int pte_index=vpn%NR_PTES_PER_PAGE;
	struct pte_directory *cur_outer_pte=current->pagetable.outer_ptes[outer_pte_index];
	int pfn;


	for(int i=0;i<NR_PAGEFRAMES;i++){
		if(!mapcounts[i]){
			pfn=i;
			if(cur_outer_pte==NULL){
				cur_outer_pte=(struct pte_directory *)malloc(sizeof(struct pte_directory));
			}
			struct pte*cur_pte = &(cur_outer_pte->ptes[pte_index]);
			cur_pte->valid=true;
			cur_pte->rw=rw;
			cur_pte->private=rw;
			cur_pte->pfn=pfn;
			mapcounts[i]++;
			return pfn;
		}
	}


	return -1;
}


/**
 * free_page(@vpn)
 *
 * DESCRIPTION
 *   Deallocate the page from the current processor. Make sure that the fields
 *   for the corresponding PTE (valid, rw, pfn) is set @false or 0.
 *   Also, consider the case when a page is shared by two processes,
 *   and one process is about to free the page. Also, think about TLB as well ;-)
 */
void free_page(unsigned int vpn)
{
	int outer_pte_index=vpn/NR_PTES_PER_PAGE;
	int pte_index=vpn%NR_PTES_PER_PAGE;
	struct pte_directory *cur_outer_pte=current->pagetable.outer_ptes[outer_pte_index];
	struct pte*cur_pte = &(cur_outer_pte->ptes[pte_index]);
	if(!cur_outer_pte||!cur_pte){
		return;
	}
	int pfn=cur_pte->pfn;
	cur_pte->valid=false;
	cur_pte->rw=0;
	cur_pte->pfn=0;
	cur_pte->private=0;
	mapcounts[pfn]--;
	///////////////////////////////
	/*Also, think about TLB as well ;-)*/

	return;


}


/**
 * handle_page_fault()
 *
 * DESCRIPTION
 *   Handle the page fault for accessing @vpn for @rw. This function is called
 *   by the framework when the __translate() for @vpn fails. This implies;
 *   0. page directory is invalid
 *   1. pte is invalid
 *   2. pte is not writable but @rw is for write
 *   This function should identify the situation, and do the copy-on-write if
 *   necessary.
 *
 * RETURN
 *   @true on successful fault handling
 *   @false otherwise
 */
bool handle_page_fault(unsigned int vpn, unsigned int rw)
{
	int outer_pte_index=vpn/NR_PTES_PER_PAGE;
	int pte_index=vpn%NR_PTES_PER_PAGE;
	struct pte_directory *cur_outer_pte=current->pagetable.outer_ptes[outer_pte_index];
	if(!cur_outer_pte)
	{
		return false;
	}
	struct pte*cur_pte = &(cur_outer_pte->ptes[pte_index]);
	if(!cur_pte){
		return false;
	}
	if(cur_pte->valid&&rw==ACCESS_WRITE&&cur_pte->private==ACCESS_WRITE){
			for(int i=0;i<NR_PAGEFRAMES;i++){
				if(!mapcounts[i]){
					int pfn=i;
					mapcounts[cur_pte->pfn]--;
					cur_pte->pfn=pfn;
					cur_pte->rw=rw;
					mapcounts[pfn]++;
					return true;
				}
			}
	}
	return false;
}


/**
 * switch_process()
 *
 * DESCRIPTION
 *   If there is a process with @pid in @processes, switch to the process.
 *   The @current process at the moment should be put into the @processes
 *   list, and @current should be replaced to the requested process.
 *   Make sure that the next process is unlinked from the @processes, and
 *   @ptbr is set properly.
 *
 *   If there is no process with @pid in the @processes list, fork a process
 *   from the @current. This implies the forked child process should have
 *   the identical page table entry 'values' to its parent's (i.e., @current)
 *   page table. 
 *   To implement the copy-on-write feature, you should manipulate the writable
 *   bit in PTE and mapcounts for shared pages. You may use pte->private for 
 *   storing some useful information :-)
 */
void switch_process(unsigned int pid)
{
	struct process * proc=NULL;
	list_for_each_entry(proc,&processes,list){
		if(proc->pid==pid){
			list_add_tail(&current->list,&processes);
			current=proc;
			ptbr=&current->pagetable;
			break;
		}
	}
	if(current->pid==pid){
		list_del(&current->list);
		return;
	}
	struct process *child=(struct process *)malloc(sizeof(struct process));
	child->pid=pid;
	for(int i=0;i<NR_PTES_PER_PAGE;i++)
	{
		if(current->pagetable.outer_ptes[i]){
			child->pagetable.outer_ptes[i]=(struct pte_directory *)malloc(sizeof(struct pte_directory));
			for(int j=0;j<NR_PTES_PER_PAGE;j++){
				if(current->pagetable.outer_ptes[i]->ptes[j].rw==ACCESS_WRITE)
				{
					current->pagetable.outer_ptes[i]->ptes[j].rw=ACCESS_READ;
				}
				child->pagetable.outer_ptes[i]->ptes[j]=current->pagetable.outer_ptes[i]->ptes[j];
				if(current->pagetable.outer_ptes[i]->ptes[j].valid){
					mapcounts[current->pagetable.outer_ptes[i]->ptes[j].pfn]++;	
				}
		}
		}
	}
	list_add_tail(&current->list,&processes);
	current=child;
	ptbr=&current->pagetable;
	return;
}

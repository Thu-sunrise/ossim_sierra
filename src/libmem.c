/*
 * Copyright (C) 2025 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* Sierra release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

// #ifdef MM_PAGING
/*
 * System Library
 * Memory Module Library libmem.c
 */

#include "string.h"
#include "mm.h"
#include "syscall.h"
#include "libmem.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

static pthread_mutex_t mmvm_lock = PTHREAD_MUTEX_INITIALIZER;

/*enlist_vm_freerg_list - add new rg to freerg_list
 *@mm: memory region
 *@rg_elmt: new region
 *
 */
int enlist_vm_freerg_list(struct mm_struct *mm, struct vm_rg_struct *rg_elmt)
{
  struct vm_rg_struct *rg_node = mm->mmap->vm_freerg_list;

  if (rg_elmt->rg_start >= rg_elmt->rg_end)
    return -1;

  if (rg_node != NULL)
    rg_elmt->rg_next = rg_node;

  /* Enlist the new region */
  mm->mmap->vm_freerg_list = rg_elmt;

  return 0;
}

/*get_symrg_byid - get mem region by region ID
 *@mm: memory region
 *@rgid: region ID act as symbol index of variable
 *
 */
struct vm_rg_struct *get_symrg_byid(struct mm_struct *mm, int rgid)
{
  if (rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ)
    return NULL;

  return &mm->symrgtbl[rgid];
}

/*__alloc - allocate a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *@alloc_addr: address of allocated memory region
 *
 */
int __alloc(struct pcb_t *caller, int vmaid, int rgid, int size, int *alloc_addr)
{
  /*Allocate at the toproof */
  pthread_mutex_lock(&mmvm_lock);
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
  struct vm_rg_struct rgnode;
  /* TODO: commit the vmaid */

  if (get_free_vmrg_area(caller, vmaid, size, &rgnode) == 0)
  {
    caller->mm->symrgtbl[rgid].rg_start = rgnode.rg_start;
    caller->mm->symrgtbl[rgid].rg_end = rgnode.rg_end;
    *alloc_addr = rgnode.rg_start;

    pthread_mutex_unlock(&mmvm_lock);
    return 0;
  }

  /* TODO get_free_vmrg_area FAILED handle the region management (Fig.6)*/
  /* TODO retrive current vma if needed, current comment out due to compiler redundant warning*/
  /*Attempt to increate limit to get space */

  /* TODO retrive old_sbrk if needed, current comment out due to compiler redundant warning*/
  // int old_sbrk = cur_vma->sbrk;
  int old_vm_end = cur_vma->vm_end;
  //printf("old sbrk: %d\n", old_sbrk);

  /* TODO INCREASE THE LIMIT as inovking systemcall
   * sys_memap with SYSMEM_INC_OP
   */
  struct sc_regs regs;
  regs.a1 = SYSMEM_INC_OP;
  regs.a2 = vmaid;
  regs.a3 = size;
  regs.orig_ax = 17;
  /* SYSCALL 17 sys_memmap */
  if (syscall(caller, 17, &regs) != 0)
  {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }

  cur_vma->sbrk = old_vm_end + size;
  /* TODO: commit the limit increment */
  caller->mm->symrgtbl[rgid].rg_start = old_vm_end;
  caller->mm->symrgtbl[rgid].rg_end = old_vm_end + size;

  *alloc_addr = old_vm_end;

  // trả vùng còn dư sau khi cấp phát vào danh sách freerg_lst
  // struct vm_area_struct *remain_rg = get_vma_by_num(caller->mm, vmaid);
  if (old_vm_end + size < cur_vma->vm_end)
  {
    struct vm_rg_struct *rg_free = malloc(sizeof(struct vm_rg_struct));
    rg_free->rg_start = cur_vma->sbrk;
    rg_free->rg_end = cur_vma->vm_end;
    enlist_vm_freerg_list(caller->mm, rg_free);
  }
  //printf("sbrk: %ld\n", cur_vma->sbrk);
  pthread_mutex_unlock(&mmvm_lock);
  return 0;
}

/*__free - remove a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
int __free(struct pcb_t *caller, int vmaid, int rgid)
{
  pthread_mutex_lock(&mmvm_lock);

  if (rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ)
  {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }

  struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  if (currg == NULL || cur_vma == NULL)
  {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }

  // Thêm vùng vào vm_freerg_list
  struct vm_rg_struct *rgnode = malloc(sizeof(struct vm_rg_struct));
  rgnode->rg_start = currg->rg_start;
  rgnode->rg_end = currg->rg_end;
  //printf("rgnode start %ld end %ld\n", rgnode->rg_start, rgnode->rg_end);
  enlist_vm_freerg_list(caller->mm, rgnode);

  // Giải phóng frame và xóa ánh xạ
  int start_pgn = currg->rg_start / PAGING_PAGESZ;
  int end_pgn = (currg->rg_end - 1) / PAGING_PAGESZ;
  //printf("start_pgn %d end_pgn %d\n", start_pgn, end_pgn);

  int rg_using_pg_count[PAGING_MAX_PGN] = {0};

  for (int i = 0; i < PAGING_MAX_SYMTBL_SZ; i++)
  {
    struct vm_rg_struct rg = caller->mm->symrgtbl[i];
    if (rg.rg_start <= 0 && rg.rg_end <= 0)
      continue;

    int start_pgn = rg.rg_start / PAGING_PAGESZ;
    int end_pgn = (rg.rg_end - 1) / PAGING_PAGESZ;
    for (int pg = start_pgn; pg <= end_pgn; pg++)
    {
      rg_using_pg_count[pg] += 1;
    }
  }

  // kiểm tra xem có còn range nào sử dụng page đó không, nếu không thì put free frame và unset bit present
  for (int i = start_pgn; i <= end_pgn; i++)
  {
    uint32_t pte = caller->mm->pgd[i];
    if (!PAGING_PAGE_PRESENT(pte))
      continue;
    int fpn = PAGING_FPN(pte);
    if (rg_using_pg_count[i] <= 1)
    {
      // Không còn range nào dùng page này nữa sau khi free
      clear_frame(caller->mram, fpn);
      MEMPHY_put_freefp(caller->mram, fpn);
      pte_unset_flags(&caller->mm->pgd[i]);
    }
  }

  // Sort the vm_freerg_list by rg_start (this part is unchanged)
  struct vm_rg_struct *curr = caller->mm->mmap->vm_freerg_list;
  struct vm_rg_struct *sorted_list = NULL;

  while (curr != NULL)
  {
    struct vm_rg_struct *next = curr->rg_next;
    curr->rg_next = NULL;

    if (sorted_list == NULL)
    {
      sorted_list = curr;
    }
    else
    {
      struct vm_rg_struct *s = sorted_list;
      struct vm_rg_struct *s_prev = NULL;
      while (s != NULL && s->rg_start < curr->rg_start)
      {
        s_prev = s;
        s = s->rg_next;
      }
      if (s_prev == NULL)
      {
        curr->rg_next = sorted_list;
        sorted_list = curr;
      }
      else
      {
        s_prev->rg_next = curr;
        curr->rg_next = s;
      }
    }
    curr = next;
  }

  caller->mm->mmap->vm_freerg_list = sorted_list;

  // Merge adjacent regions within the same page
  curr = caller->mm->mmap->vm_freerg_list;

  while (curr != NULL && curr->rg_next != NULL)
  {
    struct vm_rg_struct *next = curr->rg_next;
    // Check if regions are adjacent and in the same page
    if (curr->rg_end >= next->rg_start &&
        curr->rg_end / PAGING_PAGESZ == (next->rg_start - 1) / PAGING_PAGESZ)
    {
      if (curr->rg_end < next->rg_end)
      {
        curr->rg_end = next->rg_end;
      }
      curr->rg_next = next->rg_next;
      free(next);
    }
    else
    {
      curr = curr->rg_next;
    }
  }

  // Split regions at page boundaries
  struct vm_rg_struct *new_list = NULL;
  curr = caller->mm->mmap->vm_freerg_list;

  while (curr != NULL)
  {
    struct vm_rg_struct *next = curr->rg_next;
    curr->rg_next = NULL;

    // Calculate the start and end pages of the current region
    uint32_t start_page = curr->rg_start / PAGING_PAGESZ;
    uint32_t end_page = (curr->rg_end - 1) / PAGING_PAGESZ;

    if (curr->rg_start == curr->rg_end)
    {
      // Special case: empty region (start == end)
      if (new_list == NULL)
      {
        new_list = curr;
      }
      else
      {
        struct vm_rg_struct *tail = new_list;
        while (tail->rg_next != NULL)
          tail = tail->rg_next;
        tail->rg_next = curr;
      }
      curr = next;
      continue;
    }

    // Split the region across page boundaries
    uint32_t current_start = curr->rg_start;
    for (uint32_t page = start_page; page <= end_page; page++)
    {
      struct vm_rg_struct *new_region = (struct vm_rg_struct *)malloc(sizeof(struct vm_rg_struct));
      new_region->rg_start = current_start;
      new_region->rg_next = NULL;

      // Calculate the end of this page
      uint32_t page_end = (page + 1) * PAGING_PAGESZ;
      if (page == end_page)
      {
        new_region->rg_end = curr->rg_end;
      }
      else
      {
        new_region->rg_end = page_end;
      }

      // Add the new region to the list
      if (new_list == NULL)
      {
        new_list = new_region;
      }
      else
      {
        struct vm_rg_struct *tail = new_list;
        while (tail->rg_next != NULL)
          tail = tail->rg_next;
        tail->rg_next = new_region;
      }

      current_start = new_region->rg_end;
    }

    free(curr); // Free the original region
    curr = next;
  }

  // Update the vm_freerg_list with the new split list
  caller->mm->mmap->vm_freerg_list = new_list;

  // Xóa vùng trong symrgtbl
  currg->rg_start = -1;
  currg->rg_end = -1;
  caller->regs[rgid] = 0;

  pthread_mutex_unlock(&mmvm_lock);
  return 0;
}

void clear_frame(struct memphy_struct *mram, int fpn) {
  int offset = fpn * PAGING_PAGESZ;
  if (offset >= mram->maxsz) return; // kiểm tra tránh out-of-bounds

  memset(mram->storage + offset, 0, PAGING_PAGESZ);
}

/*liballoc - PAGING-based allocate a region memory
 *@proc:  Process executing the instruction
 *@size: allocated size
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */
int liballoc(struct pcb_t *proc, uint32_t size, uint32_t reg_index)
{
  /* TODO Implement allocation on vm area 0 */
  int addr;

  int val = __alloc(proc, 0, reg_index, size, &addr);
  if (val == -1)
  {
    return -1;
  }
  proc->regs[reg_index] = addr;
#ifdef IODUMP
  // MEMPHY_dump(proc->mram);
#ifdef OUTPUT_FOLDER
  FILE *output_file = proc->file;
  fprintf(output_file, "\033[1;42m===== PHYSICAL MEMORY AFTER ALLOCATION =====\033[0m\n");
  fprintf(output_file, "\033[1;32mPID=%d - Region=%d - Address=%08x - Size=%d byte\033[0m\n", proc->pid, reg_index, addr, size);
#endif

  printf("\033[1;42m============= PHYSICAL MEMORY AFTER ALLOCATION ============\033[0m\n");
  printf("\033[1;32mPID=%d - Region=%d - Address=%08x - Size=%d byte\033[0m\n", proc->pid, reg_index, addr, size);
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); // print max TBL
#endif
#endif

  return val;
}

/*libfree - PAGING-based free a region memory
 *@proc: Process executing the instruction
 *@size: allocated size
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */

int libfree(struct pcb_t *proc, uint32_t reg_index)
{
  /* TODO Implement free region */
  /* By default using vmaid = 0 */
  int val = __free(proc, 0, reg_index);
  if (val == -1)
  {
    return -1;
  }
#ifdef IODUMP

#ifdef OUTPUT_FOLDER
  FILE *output_file = proc->file;
  fprintf("\033[1;42m===== PHYSICAL MEMORY AFTER DEALLOCATION =====\033[0m\n");
  fprintf(output_file, "\033[1;32mPID=%d - Region=%d033[0m\n", proc->pid, reg_index);
#endif

  printf("\033[1;42m============ PHYSICAL MEMORY AFTER DEALLOCATION ============\033[0m\n");
  printf("\033[1;32mPID=%d - Region=%d\033[0m\n", proc->pid, reg_index);
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); // print max TBL
#endif
#endif
  return val;
}

/*pg_getpage - get the page in ram
 *@mm: memory region
 *@pagenum: PGN
 *@framenum: return FPN
 *@caller: caller
 *
 */

int pg_getpage(struct mm_struct *mm, int pgn, int *fpn, struct pcb_t *caller)
{
  uint32_t pte = mm->pgd[pgn];

  if (!PAGING_PAGE_PRESENT(pte))
  { /* Page is not online, make it actively living */
    int vicpgn, swpfpn;
    int vicfpn;
    uint32_t vicpte;

    int tgtfpn = PAGING_SWP(pte); // the target frame storing our variable

    /* TODO: Play with your paging theory here */
    /* Find victim page */
    if (find_victim_page(caller->mm, &vicpgn) == -1)
    {
      return -1;
    }

    /* Get free frame in MEMSWP */
    if (MEMPHY_get_freefp(caller->active_mswp, &swpfpn) == -1)
    {
      return -1;
    }

    vicpte = mm->pgd[vicpgn];
    vicfpn = PAGING_FPN(vicpte);

    /* Do swap frame from MEMRAM to MEMSWP and vice versa*/
    /* TODO copy victim frame to swap
     * SWP(vicfpn <--> swpfpn)
     * SYSCALL 17 sys_memmap
     * with operation SYSMEM_SWP_OP
     */
    struct sc_regs regs;
    regs.a1 = SYSMEM_SWP_OP;
    regs.a2 = vicfpn;
    regs.a3 = swpfpn;
    regs.orig_ax = 17;
    /* SYSCALL 17 sys_memmap */
    if (syscall(caller, 17, &regs) != 0)
    {
      return -1;
    }

    /* Copy target frame from swap to mem */
    /* TODO copy target frame form swap to mem
     * SWP(tgtfpn <--> vicfpn)
     * SYSCALL 17 sys_memmap
     * with operation SYSMEM_SWP_OP
     */
    regs.a1 = SYSMEM_SWP_OP;
    regs.a2 = tgtfpn;
    regs.a3 = vicfpn;
    regs.orig_ax = 17;
    /* SYSCALL 17 sys_memmap */
    if (syscall(caller, 17, &regs) != 0)
    {
      return -1;
    }

    /* Update page table */
    pte_set_swap(&mm->pgd[vicpgn], 0, swpfpn);

    /* Update its online status of the target page */
    pte_set_fpn(&mm->pgd[pgn], vicfpn);

    enlist_pgn_node(&caller->mm->fifo_pgn, pgn);
  }

  *fpn = PAGING_FPN(mm->pgd[pgn]);

  return 0;
}

/*pg_getval - read value at given offset
 *@mm: memory region
 *@addr: virtual address to acess
 *@value: value
 *
 */
int pg_getval(struct mm_struct *mm, int addr, BYTE *data, struct pcb_t *caller)
{
  int pgn = PAGING_PGN(addr);
  int off = PAGING_OFFST(addr);
  int fpn;

  /* Get the page to MEMRAM, swap from MEMSWAP if needed */
  if (pg_getpage(mm, pgn, &fpn, caller) != 0)
    return -1; /* invalid page access */

  /* TODO
   *  MEMPHY_read(caller->mram, phyaddr, data);
   *  MEMPHY READ
   *  SYSCALL 17 sys_memmap with SYSMEM_IO_READ
   */
  int phyaddr = (fpn * PAGING_PAGESZ) + off;
  struct sc_regs regs;
  regs.a1 = SYSMEM_IO_READ;
  regs.a2 = phyaddr; // read one byte
  // regs.a3 = ;
  regs.orig_ax = 17;

  // MEMPHY_read(caller->mram, phyaddr, data);
  /* SYSCALL 17 sys_memmap */
  if (syscall(caller, 17, &regs) != 0)
  {
    return -1;
  }
  // Update data
  *data = (BYTE)regs.a3;

  return 0;
}

/*pg_setval - write value to given offset
 *@mm: memory region
 *@addr: virtual address to acess
 *@value: value
 *
 */
int pg_setval(struct mm_struct *mm, int addr, BYTE value, struct pcb_t *caller)
{
  int pgn = PAGING_PGN(addr);
  int off = PAGING_OFFST(addr);
  if (pgn < 0 || pgn >= PAGING_MAX_PGN)
  {
    return -1;
  }

  if (off >= PAGING_PAGESZ)
  {
    return -1;
  }
  int fpn;

  /* Get the page to MEMRAM, swap from MEMSWAP if needed */
  if (pg_getpage(mm, pgn, &fpn, caller) != 0)
  {
    return -1; /* invalid page access */
  }
  /* TODO
   *  MEMPHY_write(caller->mram, phyaddr, value);
   *  MEMPHY WRITE
   *  SYSCALL 17 sys_memmap with SYSMEM_IO_WRITE
   */
  int phyaddr = (fpn * PAGING_PAGESZ) + off;
  struct sc_regs regs;
  regs.a1 = SYSMEM_IO_WRITE;
  regs.a2 = phyaddr;
  regs.a3 = value;
  regs.orig_ax = 17;

  /* SYSCALL 17 sys_memmap */
  // MEMPHY_write(caller->mram, phyaddr, value);
  if (syscall(caller, 17, &regs) != 0)
  {
    return -1;
  }

  return 0;
}

/*__read - read value in region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@offset: offset to acess in memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
int __read(struct pcb_t *caller, int vmaid, int rgid, int offset, BYTE *data)
{
  pthread_mutex_lock(&mmvm_lock);
  struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  if (currg == NULL || cur_vma == NULL)
  {
    pthread_mutex_lock(&mmvm_lock);
    return -1;
  }

  // Kiểm tra rgid đã được cấp phát
  if (currg->rg_start >= currg->rg_end)
  {
    pthread_mutex_unlock(&mmvm_lock);
    printf("\033[1;42m============== PHYSICAL MEMORY AFTER READING ==============\033[0m\n");
    printf("\033[1;32mError: Region %d not allocated\033[0m\n", rgid);
    printf("\033[1;42m===========================================================\033[0m\n");
    return -1;
  }

  pg_getval(caller->mm, currg->rg_start + offset, data, caller);
  pthread_mutex_unlock(&mmvm_lock);
  return 0;
}

/*libread - PAGING-based read a region memory */
int libread(struct pcb_t *proc, uint32_t source, uint32_t offset, uint32_t *destination)
{

  // Kiểm tra destination_index trong giới hạn regs[0..9]
  if (*destination >= 10)
  {
    printf("\033[1;42m============== PHYSICAL MEMORY AFTER READING ==============\033[0m\n");
    printf("\033[1;32mError: Register index %u out of bounds (0-9)\033[0m\n", *destination);
    printf("\033[1;42m===========================================================\033[0m\n");
    return -1;
  }

  BYTE data;
  int val = __read(proc, 0, source, offset, &data);
  if (val == -1) {
    return -1;
  }

  *destination = (uint32_t)data;
#ifdef IODUMP

#ifdef OUTPUT_FOLDER
  FILE *output_file = proc->file;
  fprintf(output_file, "\033[1;42m===== PHYSICAL MEMORY AFTER READING =====\033[0m\n");
  fprintf(output_file, "\033[1;32mread region=%d offset=%d value=%d\033[0m\n", source, offset, data);
#endif

  printf("\033[1;42m============== PHYSICAL MEMORY AFTER READING ==============\033[0m\n");
  printf("\033[1;32mread region=%d offset=%d value=%d\033[0m\n", source, offset, data);
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); // print max TBL
#endif
  MEMPHY_dump(proc->mram);
#endif

  return val;
}

/*__write - write a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@offset: offset to acess in memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
int __write(struct pcb_t *caller, int vmaid, int rgid, int offset, BYTE value)
{
  pthread_mutex_lock(&mmvm_lock);
  struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  if (currg == NULL || cur_vma == NULL)
  { /* Invalid memory identify */
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }

  // Kiểm tra rgid đã được cấp phát
  if (currg->rg_start >= currg->rg_end)
  {
    pthread_mutex_unlock(&mmvm_lock);
    printf("\033[1;42m============== PHYSICAL MEMORY AFTER WRITING ==============\033[0m\n");
    printf("\033[1;32mError: Region %d not allocated\033[0m\n", rgid);
    printf("\033[1;42m===========================================================\033[0m\n");
    return -1;
  }

  // Calculate the absolute virtual address
  uint32_t abs_addr = currg->rg_start + offset;

  // Check if the address is within the region's bounds
  if (abs_addr >= currg->rg_end)
  { // Note: rg_end is exclusive, so >= is correct
    pthread_mutex_unlock(&mmvm_lock);
    printf("\033[1;42m============== PHYSICAL MEMORY AFTER WRITING ==============\033[0m\n");
    printf("\033[1;32mError: Write address %u out of bounds for region %d [%lu, %lu)\033[0m\n",
           abs_addr, rgid, currg->rg_start, currg->rg_end);
    printf("\033[1;42m===========================================================\033[0m\n");
    return -1;
  }

  pg_setval(caller->mm, currg->rg_start + offset, value, caller);
  pthread_mutex_unlock(&mmvm_lock);
  return 0;
}

/*libwrite - PAGING-based write a region memory */
int libwrite(
    struct pcb_t *proc,   // Process executing the instruction
    BYTE data,            // Data to be wrttien into memory
    uint32_t destination, // Index of destination register
    uint32_t offset)
{
  int val = __write(proc, 0, destination, offset, data);
  if (val == -1) {
    return -1;
  }
#ifdef IODUMP

#ifdef OUTPUT_FOLDER
  FILE *output_file = proc->file;
  fprintf("\033[1;42m===== PHYSICAL MEMORY AFTER WRITING =====\033[0m\n");
  fprintf(output_file, "write region=%d offset=%d value=%d\n", destination, offset, data);
#endif
  printf("\033[1;42m============== PHYSICAL MEMORY AFTER WRITING ==============\033[0m\n");
  printf("\033[1;32mwrite region=%d offset=%d value=%d\033[0m\n", destination, offset, data);
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); // print max TBL
#endif
  MEMPHY_dump(proc->mram);
#endif

  return val;
}

/*free_pcb_memphy - collect all memphy of pcb
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@incpgnum: number of page
 */
int free_pcb_memph(struct pcb_t *caller)
{
  pthread_mutex_lock(&mmvm_lock);
  int pagenum, fpn;
  uint32_t pte;

  for (pagenum = 0; pagenum < PAGING_MAX_PGN; pagenum++)
  {
    pte = caller->mm->pgd[pagenum];

    if (!PAGING_PAGE_PRESENT(pte))
    {
      fpn = PAGING_PTE_FPN(pte);
      MEMPHY_put_freefp(caller->mram, fpn);
    }
    else
    {
      fpn = PAGING_PTE_SWP(pte);
      MEMPHY_put_freefp(caller->active_mswp, fpn);
    }
  }
  pthread_mutex_unlock(&mmvm_lock);
  return 0;
}

/*find_victim_page - find victim page
 *@caller: caller
 *@pgn: return page number
 *
 */
int find_victim_page(struct mm_struct *mm, int *retpgn)
{
  struct pgn_t *pg = mm->fifo_pgn;
  struct pgn_t *prev = NULL;

  /* TODO: Implement the theorical mechanism to find the victim page */
  if (!pg) {
      return -1; // Danh sách rỗng
  }

  // Nếu chỉ có một node trong danh sách
  if (pg->pg_next == NULL) {
      *retpgn = pg->pgn;
      mm->fifo_pgn = NULL;
      free(pg);
      return 0;
  }

  // Duyệt đến node cuối cùng
  while (pg->pg_next != NULL) {
      prev = pg;
      pg = pg->pg_next;
  }

  *retpgn = pg->pgn;
  prev->pg_next = NULL;
  free(pg);

  return 0;
}

/*get_free_vmrg_area - get a free vm region
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@size: allocated size
 *
 */
int get_free_vmrg_area(struct pcb_t *caller, int vmaid, int size, struct vm_rg_struct *newrg)
{
  // Lấy vùng nhớ ảo (VM Area) tương ứng với vmaid
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
  if (cur_vma == NULL || cur_vma->vm_freerg_list == NULL)
  {
    return -1; // Không tìm thấy vùng nhớ ảo hoặc danh sách trống
  }
  int fpn;
  struct vm_rg_struct *rgit = cur_vma->vm_freerg_list;
  struct vm_rg_struct *prev = NULL;

  // Khởi tạo giá trị mặc định cho newrg
  newrg->rg_start = -1;
  newrg->rg_end = -1;

  // Duyệt qua danh sách các vùng nhớ trống
  while (rgit != NULL)
  {
    int rg_size = rgit->rg_end - rgit->rg_start;

    // Kiểm tra nếu vùng nhớ đủ lớn
    if (rg_size >= size)
    {
      // Cập nhật vùng nhớ được cấp phát
      newrg->rg_start = rgit->rg_start;
      newrg->rg_end = rgit->rg_start + size;

      // Nếu vùng nhớ trống còn dư, cắt bớt phần đầu
      if (rg_size > size)
      {
        rgit->rg_start += size;
      }
      else
      {
        // Nếu vùng nhớ vừa khít, loại bỏ khỏi danh sách
        if (prev == NULL)
        {
          cur_vma->vm_freerg_list = rgit->rg_next;
        }
        else
        {
          prev->rg_next = rgit->rg_next;
        }
        free(rgit);
      }

      int pgn = PAGING_PGN(newrg->rg_start);
      uint32_t pte = caller->mm->pgd[pgn];
      //printf("present: %d\n", PAGING_PAGE_PRESENT(pte));
      // Nếu trang chưa được ánh xạ
      if (!PAGING_PAGE_PRESENT(pte))
      {
        MEMPHY_get_freefp(caller->mram, &fpn); // lấy frame đầu tiên trong danh sách free
        //printf("fpn: %d\n", fpn);

        pte_set_fpn(&caller->mm->pgd[pgn], fpn); // ánh xạ trang tới frame
        //printf("fpn after set: %d\n", fpn);
      }
      return 0; // Tìm thấy vùng nhớ phù hợp
    }

    // Tiếp tục duyệt danh sách
    prev = rgit;
    rgit = rgit->rg_next;
  }

  return -1;
}
// #endif

/**
 * Erwan Dupard - CS443 - Virtual Memory Manager
 */

# include <string.h>
# include <stdlib.h>
# include <stdint.h>
# include <stdio.h>

# define BACKING_STORE          ("./BACKING_STORE.bin")
# define DEFAULT_NUMBER_VALUE   ( -1 )
# define TLB_SIZE               ( 16 )
# define PHYSICAL_MEMORY_SIZE   ( 0xFFFF )
# define PAGE_TABLE_SIZE        ( 0xFF )
# define RETURN_FAILURE         ( 1 )
# define RETURN_SUCCESS         ( 0 )

/* TLB entry definition (Linked List) */
typedef struct                  s_tlb_entry
{
  int8_t                        page_number;
  int8_t                        frame_number;
  struct s_tlb_entry            *next;
}                               t_tlb_entry;

typedef struct                  s_mmu
{
  int8_t                        page_table[PAGE_TABLE_SIZE];
  int8_t                        physical_memory[PHYSICAL_MEMORY_SIZE];
  struct s_tlb_entry            *tlb;
  FILE                          *bs;
  unsigned long long            page_fault;
  unsigned long long            tlb_hits;
  unsigned long long            addresses_count;
}                               t_mmu;

static inline uint8_t           get_offset(uint16_t address)
{
  return address & 0x00FF;
}

static inline uint8_t           get_page_number(uint16_t address)
{
  return (address & 0xFF00) >> 8;
}

static size_t                   tlb_size(struct s_tlb_entry **tlb)
{
  size_t                        size = 0;
  t_tlb_entry                   *it = *tlb; 

  while (it)
  {
    ++size;
    it = it->next;
  }
  return size;
}

static char                     free_tlb(struct s_tlb_entry **tlb)
{
  t_tlb_entry                   *it = *tlb;
  t_tlb_entry                   *tmp;

  while (it)
  {
    tmp = it;
    it = it->next;
    free(tmp);
  }
  return RETURN_SUCCESS;
}

static char                     find_in_tlb(t_mmu *mmu, uint8_t page_number, uint8_t *frame_number)
{
  t_tlb_entry                   *it = mmu->tlb;

  while (it)
  {
    if (it->page_number == page_number)
    {
      *frame_number = it->frame_number;
      ++mmu->tlb_hits;
      return RETURN_SUCCESS;
    }
    it = it->next;
  }
  return RETURN_FAILURE;
}

static char                     add_tlb_entry(struct s_tlb_entry **tlb, int8_t page_number, int8_t frame_number)
{
  t_tlb_entry                   *e;
  t_tlb_entry                   *it = *tlb;

  if (tlb_size(tlb) >= TLB_SIZE)
  {
    printf("[^] TLB table is full !\n");
    return RETURN_FAILURE;
  }
  if ((e = malloc(sizeof(*e))) == NULL)
  {
    fprintf(stderr, "[-] malloc() failure\n");
    return RETURN_FAILURE;
  }
  e->page_number = page_number;
  e->frame_number = frame_number;
  e->next = NULL;
  if (*tlb == NULL)
  {
    *tlb = e;
    return RETURN_SUCCESS;
  }
  while(it && it->next)
    it = it->next;
  it->next = e;
  return RETURN_SUCCESS;
}

/**
 * Read the given filename and return an allocated
 * uint32_t array of addresses.
 */
static uint16_t                 *read_address_file(char const *filename, unsigned long long *line_count)
{
  FILE                          *fp = NULL;
  char                          *line = NULL;
  uint16_t                      *addresses = NULL;
  size_t                        len = 0;
  unsigned long long            lines = 0;
  ssize_t                       read;

  /* First allocation of the addresses array */
  if ((addresses = malloc(sizeof(*addresses))) == NULL)
  {
    fprintf(stderr, "[-] malloc() failure.. \n");
    return NULL;
  }
  /* Opening addresses file */
  if ((fp = fopen(filename, "r")) == NULL)
  {
    fprintf(stderr, "[-] Failed to open file '%s'\n", filename);
    return NULL;
  }
  /* Reading file line by line */
  while ((read = getline(&line, &len, fp)) != -1) {
    /* If a line has been read */
    if (line) 
    {
      /* Convert the line and store it into the array */
      addresses[lines++] = atoi(line);
      /* Reallocate the array while we have new lines */
      if ((addresses = realloc(addresses, sizeof(*addresses) * (lines + 1))) == NULL)
      {
        fprintf(stderr, "[-] realloc() failure.. \n");
        return NULL;
      }
    }
  }
  *line_count = lines;
  fclose(fp);
  if (line)
    free(line);
  return addresses;
}

static char                     first_fit(t_mmu *mmu, uint8_t *data, uint8_t frame_number)
{
  for (unsigned i = 0 ; i < PAGE_TABLE_SIZE; ++i)
    if (mmu->page_table[i] == DEFAULT_NUMBER_VALUE)
    {
      /* Add value into page table */
      mmu->page_table[i] = frame_number;
      /* Also update TLB */
      add_tlb_entry(&mmu->tlb, mmu->page_table[i], frame_number);
      /* Copy block into local physical memory */
      memcpy(&mmu->physical_memory[(frame_number * 0xFF + 1)], data, 0xFF + 1);
      /* Returns because we don't want to fill all the page table */
      return RETURN_SUCCESS;
    }
  return RETURN_SUCCESS;
}

static char                     page_fault(t_mmu *mmu, uint8_t page_number)
{
  uint16_t                      offset = page_number * 256;
  uint8_t                       data[0xFF + 2];

  if (fseek(mmu->bs, offset, SEEK_SET) == -1)
    return RETURN_FAILURE;
  fread(data, 1, 0xFF + 1, mmu->bs);
  first_fit(mmu, data, page_number);
  return RETURN_SUCCESS;
}

static char                     init_mmu(t_mmu *mmu)
{
  /* Pre opening the backing store to gain speed */
  if ((mmu->bs = fopen(BACKING_STORE, "rb")) == NULL)
  {
    fprintf(stderr, "[-] Failed to open Backing Store\n");
    return RETURN_FAILURE;
  }
  /* Fill the arrays with default values */
  memset(&mmu->physical_memory, 0, sizeof(mmu->physical_memory));
  memset(&mmu->page_table, DEFAULT_NUMBER_VALUE, sizeof(mmu->page_table));
  memset(&mmu->tlb, DEFAULT_NUMBER_VALUE, sizeof(mmu->tlb));
  mmu->page_fault = 0;
  mmu->tlb_hits = 0;
  mmu->tlb = NULL;
  return RETURN_SUCCESS;
}

static char                     process_address(t_mmu *mmu, uint16_t address)
{
  uint8_t                       offset = get_offset(address);
  uint8_t                       page_number = get_page_number(address);
  uint16_t                      physical_addr;
  int                           frame_number = -1;
  uint8_t                       tlb_frame_number = 0;

  if (find_in_tlb(mmu, page_number, &tlb_frame_number) == RETURN_SUCCESS)
    frame_number = tlb_frame_number;
  else
    frame_number = mmu->page_table[page_number];
  if (frame_number == DEFAULT_NUMBER_VALUE)
  {
    /* The page could not be found in the Page table  *** PAGE FAULT ***  */
    ++mmu->page_fault;
    page_fault(mmu, page_number);
    return RETURN_SUCCESS;
  }
  /* Retrieving physical addr as a 16 bits integer */
  physical_addr = (frame_number + offset) * 256;
  printf("| Logical address:  %08x    |\n", (uint16_t)address);
  printf("| Physical address: %08x    |\n", physical_addr);
  printf("| Value:            %02x          |\n", mmu->physical_memory[physical_addr]);
  printf("+ - - - - - - - - - - - - - - - +\n");
  return RETURN_SUCCESS;
}

int                             main(int argc, char **argv)
{
  t_mmu                         mmu; /* MMU on the stack */
  uint16_t                      *addresses = NULL;

  if (argc < 2)
  {
    fprintf(stderr, "[^] USAGE: ./vmmgr <adresses_file.txt>\n");
    return RETURN_FAILURE;
  }
  /* Init the main structure */
  if (init_mmu(&mmu) == RETURN_FAILURE)
    return RETURN_FAILURE;
  /* Retrieving addresses file */
  if ((addresses = read_address_file(argv[1], &mmu.addresses_count)) == NULL)
    return RETURN_FAILURE;
  /* At least one address to process */
  if (mmu.addresses_count == 0)
  {
    fprintf(stderr, "[-] You need at least one address to process .. \n");
    return RETURN_FAILURE;
  }
  printf("[^] Processing %llu addresses\n", mmu.addresses_count);
  /* Iterating over addresses and process them */
  for (unsigned i = 0 ; i < mmu.addresses_count ; ++i)
    (void)process_address(&mmu, addresses[i]); /* Don't care about the return value */
  /* Display Page fault and TLB hit percentage */
  printf("# Page fault : %llu - %f%%\n", mmu.page_fault, (((float)mmu.page_fault * 100.0) / (float)mmu.addresses_count));
  printf("# TLB hits   : %llu - %f%%\n", mmu.tlb_hits, (((float)mmu.tlb_hits * 100.0) / (float)mmu.addresses_count));
  /* The allocated uint16_t array have to be free */
  free(addresses);
  /* Free the TLB */
  free_tlb(&mmu.tlb);
  /* We need to close the backing store  */
  fclose(mmu.bs);
  return RETURN_SUCCESS;
}

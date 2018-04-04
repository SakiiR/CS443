/**
 * Erwan Dupard - CS443 - Virtual Memory Manager
 */

# include <string.h>
# include <stdlib.h>
# include <stdint.h>
# include <stdio.h>

# define BACKING_STORE           ("./BACKING_STORE.bin")

# define DEFAULT_NUMBER_VALUE    ( -1 )

# define TLB_SIZE                ( 16 )

# define PHYSICAL_MEMORY_SIZE    ( 0xFFFF )
# define PAGE_TABLE_SIZE         ( 0xFF )

# define RETURN_FAILURE          ( 1 )
# define RETURN_SUCCESS          ( 0 )

typedef struct                  s_tlb_entry
{
  int8_t                        page_number;
  int8_t                        frame_number;
}                               t_tlb_entry;

typedef struct                  s_mmu
{
  int8_t                        page_table[PAGE_TABLE_SIZE];
  int8_t                        physical_memory[PHYSICAL_MEMORY_SIZE];
  struct s_tlb_entry            tlb[TLB_SIZE];
  FILE                          *bs;
}                               t_mmu;


static inline uint8_t           get_offset(uint16_t address)
{
  return address & 0x00FF;
}

static inline uint8_t           get_page_number(uint16_t address)
{
  return (address & 0xFF00) >> 8;
}

/**
 * Read the given filename and return an allocated
 * uint32_t array of addresses.
 */
static uint16_t                 *read_address_file(char const *filename, unsigned int *line_count)
{
  FILE                          *fp = NULL;
  char                          *line = NULL;
  uint16_t                      *addresses = NULL;
  size_t                        len = 0;
  size_t                        lines = 0;
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
      mmu->page_table[i] = frame_number;
      /* Also update TLB */
      memcpy(mmu->physical_memory, data, 0xFF + 1);
      return RETURN_SUCCESS;
    }
  return RETURN_SUCCESS;
}

static char                     page_fault(t_mmu *mmu, uint8_t page_number)
{
  uint16_t                      offset = page_number * 256;
  uint8_t                       data[0xFF + 1];

  fseek(mmu->bs, offset, SEEK_SET);
  fread(data, 0xFF + 1, 1, mmu->bs);
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
  return RETURN_SUCCESS;
}

static char                     process_address(t_mmu *mmu, uint16_t address)
{
  uint8_t                       offset = get_offset(address);
  uint8_t                       page_number = get_page_number(address);
  uint16_t                      physical_addr;
  int                           frame_number = mmu->page_table[page_number];

  if (frame_number == DEFAULT_NUMBER_VALUE)
  {
    /* The page could not be found in the Page table  *** PAGE FAULT ***  */
    page_fault(mmu, page_number);
    return RETURN_FAILURE;
  }
  physical_addr = (frame_number + offset) * 256;
  printf("| Logical address:  %08x    |\n", (uint16_t)address);
  printf("| Physical address: %08x    |\n", physical_addr);
  printf("| Value:            %08x    |\n", mmu->physical_memory[physical_addr]);
  printf("+ - - - - - - - - - - - - - - - +\n");
  return RETURN_SUCCESS;
}

int                             main(int argc, char **argv)
{
  t_mmu                         mmu; /* MMU on the stack */
  uint16_t                      *addresses = NULL;
  unsigned int                  addresses_count = 0;

  if (argc < 2)
  {
    fprintf(stderr, "[^] USAGE: ./vmmgr <adresses_file.txt>\n");
    return RETURN_FAILURE;
  }
  if (init_mmu(&mmu) == RETURN_FAILURE)
    return RETURN_FAILURE;
  /* Retrieving addresses file */
  if ((addresses = read_address_file(argv[1], &addresses_count)) == NULL)
    return RETURN_FAILURE;
  /* Iterating over addresses and process them */
  for (unsigned i = 0 ; i < addresses_count ; ++i)
    (void)process_address(&mmu, addresses[i]); /* Don't care about the return value */
  /* The allocated uint16_t array have to be free */
  free(addresses);
  /* We need to close the backing store  */
  fclose(mmu.bs);
  return RETURN_SUCCESS;
}

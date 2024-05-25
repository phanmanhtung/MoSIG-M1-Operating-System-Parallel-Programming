#include "mem_alloc.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <stdint.h>

#include "mem_alloc_types.h"
#include "my_mmap.h"

/* pointer to the beginning of the memory region to manage */
void *heap_start;

/* Pointer to the first free block in the heap */
mb_free_t *first_free;

#define ULONG(x)((long unsigned int)(x))

#if defined(FIRST_FIT)

void *memory_alloc(size_t size)
{
    // Check for invalid size
    if (size == 0) {
        return NULL; // Cannot allocate zero bytes
    }

    // Traverse the free block list to find a block that fits
    mb_free_t *current = first_free, *prev = NULL;

    while (current != NULL) {
        if (current->size >= size + sizeof(mb_allocated_t)) {
            if (current->size - size - sizeof(mb_allocated_t) <= sizeof(mb_free_t)) {
                if (prev != NULL) prev->next = current->next;
                    else first_free = current->next;
                
                mb_allocated_t *allocated_block = (mb_allocated_t *)current;
                allocated_block->size = current->size - sizeof(mb_allocated_t);

                print_alloc_info((void *)(allocated_block + 1), size);
                return (void *)(allocated_block + 1); // Return a pointer immediately after metadata
            }
            // Calculate the address of the new free block after the allocated block
            mb_free_t *new_free_block = (mb_free_t *)((char *)current + size + sizeof(mb_allocated_t));
            new_free_block->size = current->size - sizeof(mb_allocated_t) - size;
            new_free_block->next = current->next;

            // Found a block that can accommodate the requested size and metadata
            mb_allocated_t *allocated_block = (mb_allocated_t *)current;
            allocated_block->size = size;

            // Update the linked list of free blocks
            if (prev != NULL) prev->next = new_free_block;
                else first_free = new_free_block;

            // Call print_alloc_info to print allocation information
            print_alloc_info((void *)(allocated_block + 1), size);

            return (void *)(allocated_block + 1); // Return a pointer immediately after metadata
        }
        prev = current;
        current = current->next;
    }

    // No suitable block found, print an error message and return NULL
    print_alloc_error(size);
    return NULL;
}

#elif defined(BEST_FIT)

void *memory_alloc(size_t size)
{
    // Check for invalid size
    if (size == 0) {
        return NULL; // Cannot allocate zero bytes
    }

    // Initialize pointers for traversal
    mb_free_t *current = first_free, *prev = NULL;
    
    // Initialize pointers to track free memory blocks
    mb_free_t *best = NULL, *best_prev = NULL;

    while (current != NULL) {
        if (current->size >= size + sizeof(mb_allocated_t)) {
            // Found a suitable block
            if (best == NULL || best->size > current->size) {
                best = current;
                best_prev = prev;
            }
        }
        // Move to the next block
        prev = current;
        current = current->next;
    }

    if (best != NULL) {
        // Allocate memory from the best block
        mb_allocated_t *allocated_block = (mb_allocated_t *)best;
        mb_free_t *new_free_block, *current_next = best->next;
        int new_free_block_size = 0;

        if (best->size - size - sizeof(mb_allocated_t) <= sizeof(mb_free_t)) {
            // Not enough space for a new free block
            allocated_block->size = best->size - sizeof(mb_allocated_t);
        } else {
            // Create a new free block after the allocated block
            new_free_block_size = best->size - sizeof(mb_allocated_t) - size;
            allocated_block->size = size;
        }

        // Calculate the address of the new free block after the allocated block
        if (new_free_block_size != 0) {
            new_free_block = (mb_free_t *)((char *)best + size + sizeof(mb_allocated_t));
            new_free_block->size = new_free_block_size;
            new_free_block->next = current_next;
        } else {
            new_free_block = current_next;
        }

        // Update the head node of the linked list of free blocks
        if (best_prev != NULL) {
            best_prev->next = new_free_block;
        } else {
            first_free = new_free_block;
        }

        // Call print_alloc_info to print allocation information
        print_alloc_info((void *)(allocated_block + 1), size);

        return (void *)(allocated_block + 1); // Return a pointer immediately after metadata
    }

    // No suitable block found, print an error message and return NULL
    print_alloc_error(size);
    return NULL;
}

#elif defined(NEXT_FIT)

// Define a global pointer to track the next fit position
static mb_free_t *next_fit_ptr = NULL;

void *memory_alloc(size_t size)
{
    // Check for invalid size
    if (size == 0) {
        return NULL; // Cannot allocate zero bytes
    }

    // Check for the first allocation
    if (next_fit_ptr == NULL) {
        next_fit_ptr = first_free;
    }

    // Start the search for a block to fit from the next_fit_ptr position
    mb_free_t *current = next_fit_ptr, *prev = NULL;

    while (current != NULL) {
        if (current->size >= size + sizeof(mb_allocated_t)) {
            if (current->size - size - sizeof(mb_allocated_t) <= sizeof(mb_free_t)) {
                if (prev != NULL) prev->next = current->next;
                else first_free = current->next;

                mb_allocated_t *allocated_block = (mb_allocated_t *)current;
                allocated_block->size = current->size - sizeof(mb_allocated_t);

                // Update next_fit_ptr to point to the next free block
                next_fit_ptr = current->next;

                print_alloc_info((void *)(allocated_block + 1), size);
                return (void *)(allocated_block + 1); // Return a pointer immediately after metadata
            }
            // Calculate the address of the new free block after the allocated block
            mb_free_t *new_free_block = (mb_free_t *)((char *)current + size + sizeof(mb_allocated_t));
            new_free_block->size = current->size - sizeof(mb_allocated_t) - size;
            new_free_block->next = current->next;

            // Found a block that can accommodate the requested size and metadata
            mb_allocated_t *allocated_block = (mb_allocated_t *)current;

            // Update the linked list of free blocks
            if (prev != NULL) prev->next = new_free_block;
            else first_free = new_free_block;

            // Update next_fit_ptr to point to the next free block
            next_fit_ptr = new_free_block;

            // Call print_alloc_info to print allocation information
            print_alloc_info((void *)(allocated_block + 1), size);

            return (void *)(allocated_block + 1); // Return a pointer immediately after metadata
        }
        prev = current;
        current = current->next;
    }

    // No suitable block found, print an error message and return NULL
    print_alloc_error(size);
    return NULL;
}

#endif

void run_at_exit(void)
{
    fprintf(stderr,"YEAH B-)\n");
    
    /* TODO: insert your code here */
}

void memory_init(void)
{
    /* register the function that will be called when the programs exits */
    atexit(run_at_exit);

    /* TODO: insert your code here */
    /* Use my_mmap to allocate the memory region (MEM_POOL_SIZE bytes) */
    heap_start = my_mmap(MEM_POOL_SIZE);

    first_free = (mb_free_t *)heap_start;
    first_free->size = MEM_POOL_SIZE;
    first_free->next = NULL;
}

void memory_free(void *p) {
    /* TODO: insert your code here */
    if (p == NULL) {
        return; // Ignore freeing NULL pointers
    }
    print_free_info(p);

    // The metadata of the block to free is immediately before the allocated block
    mb_allocated_t *p_metadata = (mb_allocated_t *)p - 1;
    size_t free_block_size = p_metadata->size + sizeof(mb_allocated_t);

    // Traverse the free list to find the correct position (don't want insert to the head of free list)
    mb_free_t *current = first_free, *prev = NULL;
    mb_free_t *new_free_block = (mb_free_t *)p_metadata;

    // Find the correct position to insert the newly freed block based on address
    while (current != NULL && current < new_free_block) {
        prev = current;
        current = current->next;
    }
    new_free_block->next = current;
    new_free_block->size = free_block_size;
    if (prev != NULL) prev->next = new_free_block;
        else first_free = new_free_block;

    // Merge contiguous free blocks if necessary
    if (prev != NULL && (char *)prev + prev->size == (char *)new_free_block) { // Inspect the prev block
        prev->size += new_free_block->size; // absorb the size
        prev->next = new_free_block->next;
        new_free_block = prev;

        #if defined(NEXT_FIT)
        next_fit_ptr = NULL; // If the prev block is combined, the next_fit_ptr will be an invalid address
        #endif
    }

    if (current != NULL && (char *)new_free_block + new_free_block->size == (char *)current) { // Inspect the after block
        new_free_block->size += current->size;
        new_free_block->next = current->next;
    }
    
}

size_t memory_get_allocated_block_size(void *addr)
{

    /* TODO: insert your code here */

    return 0;
}

int is_allocated(void *addr)
{
    mb_free_t *free_block = (mb_free_t *)heap_start;
    while (free_block != NULL) {
        // Check if the address falls within a free block
        if (addr >= (void *)(free_block + 1) && addr < (void *)((char *)free_block + free_block->size)) {
            return 0; // Not allocated (it's within a free block)
        }
        free_block = free_block->next;
    }

    // If the address is not within any free block, consider it allocated
    return 1;
}

void print_mem_state(void)
{
    printf("Memory State:\n");

    char *mem_ptr = (char *)heap_start;
    char *end_ptr = (char *)heap_start + MEM_POOL_SIZE;

    while (mem_ptr < end_ptr) {
        if (is_allocated(mem_ptr)) {
            // Allocated block, print 'X' to represent an allocated byte
            printf("X");
            mem_ptr += sizeof(mb_allocated_t) + memory_get_allocated_block_size(mem_ptr);
        } else {
            // Free block, print '.' to represent a free byte
            printf(".");
            mem_ptr += sizeof(mb_free_t) + ((mb_free_t *)mem_ptr)->size;
        }
    }

    printf("\n");
}

void print_info(void) {
    fprintf(stderr, "Memory : [%lu %lu] (%lu bytes)\n", (long unsigned int) heap_start, (long unsigned int) ((char*)heap_start+MEM_POOL_SIZE), (long unsigned int) (MEM_POOL_SIZE));
}

void print_free_info(void *addr){
    if(addr){
        fprintf(stderr, "FREE  at : %lu \n", ULONG((char*)addr - (char*)heap_start));
    }
    else{
        fprintf(stderr, "FREE  at : %lu \n", ULONG(0));
    }
    
}

void print_alloc_info(void *addr, int size){
  if(addr){
    fprintf(stderr, "ALLOC at : %lu (%d byte(s))\n", 
	    ULONG((char*)addr - (char*)heap_start), size);
  }
  else{
    fprintf(stderr, "Warning, system is out of memory\n"); 
  }
}

void print_alloc_error(int size) 
{
    fprintf(stderr, "ALLOC error : can't allocate %d bytes\n", size);
}


#ifdef MAIN
int main(int argc, char **argv) {
    memory_init();
    print_info();

    // memory_alloc(20);
    // void * a = memory_alloc(20);
    // memory_alloc(10);
    // void * b = memory_alloc(15);
    // memory_alloc(20);
    // memory_free(a);
    // memory_free(b);
    // memory_alloc(10);
    // memory_alloc(20);
    // print_mem_state();

    void * a = memory_alloc(10);
    void * b = memory_alloc(10);
    memory_free(a);
    memory_free(b);
    memory_alloc(10);

    return EXIT_SUCCESS;
}

#endif 

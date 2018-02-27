/**
 * Made by Erwan Dupard (CSUSM Student)
 * 2018 - 02 - 06   10:48 PM
 */
#include <limits.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

/**
 * Some Macro used for the project
 */
#define IN_INTERVAL(x)  (x >= 1 && x < INT_MAX && (3*x+1 >= 0 && 3*x+1 <= INT_MAX))
#define RETURN_SUCCESS  (0)
#define RETURN_FAILURE  (1)
#define SYSCALL_ERROR   (-1)
#define ODD(x)          (x % 2 == 1)

/**
 * Function generation the sequence with the printf() stdlib call
 */
int             generate_sequence(unsigned int base)
{
    printf("%d", base);
    if (base <= 1)
        return RETURN_SUCCESS;
    else
        printf(", ");
    if (ODD(base))
        base = 3 * base + 1;
    else
        base = base / 2;
    return generate_sequence(base);
}

/**
 * Main - Entry Point
 */
int                 main(int argc, char **argv)
{
    int             pid = 0;
    int             base = 0;

    if (argc < 2)
    {
        fprintf(stderr, "[-] USAGE: %s <start_number>\n", argv[0]);
        return RETURN_FAILURE;
    }
    /** Convert const char * -> int  */
    /** Check if base is > 0  */
    base = atoi(argv[1]);
    if (!(IN_INTERVAL(base)))
    {
        printf("[-] Please, enter a valid number (>= 1)\n");
        return RETURN_FAILURE;
    }
    /** Process creation */
    if ((pid = fork()) == 0)
    { /** Here, we are in the child process, so generate the sequence */
        printf("[+] Process created .. pid=%d\n", getpid());
        return generate_sequence((unsigned int)base);
    } 
    else if (pid > 0)
    { /** Here, we are in the parent process, so wait for the son process to complete */
        wait(NULL);
        fflush(stdout);  /* Make sure to flush after all the call to printf (in case printf didn't do it by itself */
        printf("\n[+] Finished job !\n");
    }
    else
    { /** Error at the fork() system call */
        fprintf(stderr, "[-] Failed to create the process\n");
        perror("fork");
        return RETURN_FAILURE;
    }
    return RETURN_SUCCESS;
}

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define RETURN_SUCCESS (0)
#define RETURN_ERROR (1)
#define SYSCALL_ERROR (-1)
#define MAX_BYTE_READ (4096)
#define OUTFILE_PERMS (S_IRWXU | S_IRGRP | S_IROTH)
#define OUTFILE_MODE (O_CREAT | O_WRONLY | O_TRUNC)

/**
 * Function used to close two file descriptors infd and outfd
 */
void        close_files(int infd, int outfd)
{
    close(infd);
    close(outfd);
}

/**
 * Function used to open infile and outfile then store the file descriptors
 * into infd and outfd
 *
 * In case of error, a message will be displayed and the function will returns RETURN_ERROR
 */
int         open_files(const char *infile, const char *outfile, int *infd, int *outfd)
{
    /**
     * Opening the input file (read only)
     */
    if ((*infd = open(infile, O_RDONLY)) == SYSCALL_ERROR) 
    {
        fprintf(stderr, "[-] Failed to open input file: %s\n", infile);
        perror("open");
        return RETURN_ERROR;
    }

    /**
     * Opening the output file
     *  - Create it if it is not exists (grant all permission for the current user "-rwx------")
     *  - Trunc the file if it exists
     */
    if ((*outfd = open(outfile, OUTFILE_MODE, OUTFILE_PERMS)) == SYSCALL_ERROR)
    {
        fprintf(stderr, "[-] Failed to open output file: %s\n", outfile);
        perror("open");
        return RETURN_ERROR;
    }
    return RETURN_SUCCESS; 
}

/**
 * Read from the intfd then write the readed content to outfd
 */
int copy_files(int infd, int outfd)
{
    char    buffer[MAX_BYTE_READ];
    int     byte_read;

    /**
     * Reading the input file with the read() syscall, then write the content to 
     * the output file with the write() syscall.
     * Placing a \0 at the end of the buffer each time we call read to avoid memory corruption 
     * (only if the buffer would be used for an other purpose like (strlen, printf, strcpy))
     */
    while((byte_read = read(infd, buffer, MAX_BYTE_READ)) > 0)
    {
        buffer[byte_read] = 0;
        if (write(outfd, buffer, byte_read) == SYSCALL_ERROR)
        {
            fprintf(stderr, "[-] Failed to write()\n");
            perror("write");
            return RETURN_ERROR;
        }
    }

    /**
     * If the read() system call failed, we should know it
     */
    if (byte_read == SYSCALL_ERROR)
    {
        fprintf(stderr, "[-] An error occured while reading data\n");
        perror("read");
        close_files(infd, outfd);
        return RETURN_ERROR;
    }
    return RETURN_SUCCESS;
}

/**
 * Main function, entry point
 */
int         main(int argc, char **argv)
{
    char    *infile, *outfile;
    int     infd, outfd;

    if (argc < 3) 
    {
        fprintf(stderr, "[+] Usage: %s <input file> <output file>\n", argv[0]);
        return RETURN_ERROR;
    }
    // infile and outfile are easier to read
    infile = argv[1];
    outfile = argv[2];
    if (open_files(infile, outfile, &infd, &outfd) == RETURN_ERROR)
        return RETURN_ERROR;
    if (copy_files(infd, outfd) == RETURN_ERROR)
        return RETURN_ERROR;
    printf("[+] Copied all data\n");
    // closing file descriptors
    close_files(infd, outfd);
    return RETURN_SUCCESS;
}


// [sakiir@Sakiir-PC Prog1]$ gcc filecpy.c -o filecpy
// [sakiir@Sakiir-PC Prog1]$ ./filecpy 
// [+] Usage: ./filecpy <input file> <output file>
// [sakiir@Sakiir-PC Prog1]$ strace ./filecpy /etc/passwd /tmp/output.txt
// execve("./filecpy", ["./filecpy", "/etc/passwd", "/tmp/output.txt"], 0x7fff04a91430 /* 38 vars */) = 0
// brk(NULL)                               = 0x56396e09a000
// access("/etc/ld.so.preload", R_OK)      = -1 ENOENT (No such file or directory)
// openat(AT_FDCWD, "/etc/ld.so.cache", O_RDONLY|O_CLOEXEC) = 3
// fstat(3, {st_mode=S_IFREG|0644, st_size=221866, ...}) = 0
// mmap(NULL, 221866, PROT_READ, MAP_PRIVATE, 3, 0) = 0x7f8ac04c1000
// close(3)                                = 0
// openat(AT_FDCWD, "/usr/lib/libc.so.6", O_RDONLY|O_CLOEXEC) = 3
// read(3, "\177ELF\2\1\1\3\0\0\0\0\0\0\0\0\3\0>\0\1\0\0\0`\20\2\0\0\0\0\0"..., 832) = 832
// fstat(3, {st_mode=S_IFREG|0755, st_size=2065784, ...}) = 0
// mmap(NULL, 8192, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0) = 0x7f8ac04bf000
// mmap(NULL, 3893488, PROT_READ|PROT_EXEC, MAP_PRIVATE|MAP_DENYWRITE, 3, 0) = 0x7f8abff1d000
// mprotect(0x7f8ac00cb000, 2093056, PROT_NONE) = 0
// mmap(0x7f8ac02ca000, 24576, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_FIXED|MAP_DENYWRITE, 3, 0x1ad000) = 0x7f8ac02ca000
// mmap(0x7f8ac02d0000, 14576, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_FIXED|MAP_ANONYMOUS, -1, 0) = 0x7f8ac02d0000
// close(3)                                = 0
// arch_prctl(ARCH_SET_FS, 0x7f8ac04c04c0) = 0
// mprotect(0x7f8ac02ca000, 16384, PROT_READ) = 0
// mprotect(0x56396d038000, 4096, PROT_READ) = 0
// mprotect(0x7f8ac04f8000, 4096, PROT_READ) = 0
// munmap(0x7f8ac04c1000, 221866)          = 0
// openat(AT_FDCWD, "/etc/passwd", O_RDONLY) = 3
// openat(AT_FDCWD, "/tmp/output.txt", O_WRONLY|O_CREAT|O_TRUNC, 0744) = 4
// read(3, "root:x:0:0:root:/root:/bin/bash\n"..., 4096) = 1532
// write(4, "root:x:0:0:root:/root:/bin/bash\n"..., 1532) = 1532
// read(3, "", 4096)                       = 0
// fstat(1, {st_mode=S_IFCHR|0620, st_rdev=makedev(136, 0), ...}) = 0
// brk(NULL)                               = 0x56396e09a000
// brk(0x56396e0bb000)                     = 0x56396e0bb000
// write(1, "[+] Copied all data\n", 20[+] Copied all data
// )   = 20
// close(3)                                = 0
// close(4)                                = 0
// exit_group(0)                           = ?
// +++ exited with 0 +++
// [sakiir@Sakiir-PC Prog1]$

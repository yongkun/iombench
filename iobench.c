/*
 *   iobench.c
 *
 *   Copyright 2014 Yongkun Wang
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 *
 *
 *   A microbenchmark for storage device such as hard disk and flash SSD.
 *
 *   Written by Yongkun Wang (yongkun@gmail.com)
 */

#define _GNU_SOURCE
#define _LARGEFILE64_SOURCE

#ifdef __APPLE__
#define lseek64 lseek
#define open64 open
#endif

#define SECTOR_SIZE 512
#define ASCII_PRINTABLE_LOW 32
#define ASCII_PRINTABLE_HIGH 126
#define MAX_FILE_NAME_LENGTH 1024

#define GET_OUTPUT(fd) ((fd) != NULL ? (fd) : stdout)

#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <getopt.h>
#include <pthread.h>
#include <libgen.h>
#include <limits.h>

void usage()
{
    fprintf(stderr,
        "\n"
        "NAME\n"
        "       iobench - microbenchmark for storage devices/systems\n"
        "\n"
        "SYNOPSIS\n"
        "       iobench  [ -d time ] [ -f filename ] [ -n count ] [ -H ]\n"
        "                [ -p size ] [ -o filename ] [ -P ] [ -r percent ]\n"
        "                [ -R time ] [ -s addr ] [ -S addr ] [ -t count ]\n"
        "                [ -w percent ]\n"
        "       iobench  -h\n"
        "\n"
        "DESCRIPTION\n"
        "       iobench is a microbenchmark for storage systems or devices\n"
        "       such as hard disk and flash SSD. It provides the following\n"
        "       features:\n"
        "           - Synchronous IO by O_SYNC and O_DIRECT to try to bypass\n"
        "             OS or file system buffer.\n"
        "           - Multi-threading to simulate multiple outstanding IOs.\n"
        "           - Various IO sequences including sequential/random\n"
        "             reads/writes, and mixed IOs with any R/W ratio.\n"
        "       It is recommended to tune your devices/systems for prefetching\n"
        "       or write-back cache with some tools like hdparm.\n"
        "\n"
        "OPTIONS\n"
        "   -d <time>       Duration of test of each thread in seconds. \n"
        "                   The longer the better, especially for SSDs.\n"
        "\n"
        "   -f <filename>   Filename for test. Can be device file like /dev/sda."
        "\n"
        "                   This is a recommended way to test new drives.\n"
        "                   Make sure you have correct permissions.\n"
        "                   WARNING! All data include partition table will be\n"
        "                   overwritten! Cannot be recovered!\n"
        "\n"
        "   -n <count>      Send <number> of requests per thread. Program will\n"
        "                   terminate by -n or -d which happens first.\n"
        "\n"
        "   -H              Human friendly output.\n"
        "\n"
        "   -o <filename>   Output file to append. Default output to console.\n"
        "\n"
        "   -p <size>       Page size in sector (512B) for IO. This number\n"
        "                   is multiplied by 512. For example, -p 8 uses 4096\n"
        "                   as page size.\n"
        "\n"
        "   -P              Output the execution details of each request.\n"
        "\n"
        "   -r              Use random addresses. Random IO.\n"
        "\n"
        "   -R <time>       Rampup interval in seconds between threads.\n"
        "\n"
        "   -s <addr>       Initial file offset. Automatically aligned by 512.\n"
        "                   Default to 0.\n"
        "\n"
        "   -S <addr>       End offset. Automatically aligned by 512. Note\n"
        "                   that lseek64 is not defined on OSX, so the largest\n"
        "                   offset on Mac may be 2G even you specify larger\n"
        "                   value.\n"
        "                   If this value is larger than file length, reads\n"
        "                   or writes may happen on the hole of the file. Run\n"
        "                   'man lseek' to see the details. A safe way is to \n"
        "                   generate the file in advance using this tool or\n"
        "                   dd, then set the end offset smaller than the file\n"
        "                   length.\n"
        "                   For sequential read/write, file offset will be\n"
        "                   reset to initial address (-s) when file offset\n"
        "                   reaches this value.\n"
        "                   Within the valid address space, set this value the\n"
        "                   larger the better, which ensures large disk head\n"
        "                   movement in random access for hard disk, and might\n"
        "                   trigger more data movements and block erasures\n"
        "                   inside SSDs (it is highly depends on the\n"
        "                   implementation of FTL).\n"
        "                   Set this value to the capacity of disk when testing\n"
        "                   with device file.\n"
        "\n"
        "   -t <count>      Number of threads.\n"
        "\n"
        "   -w <percent>    Percent of write requests. 0-100. Default 50.\n"
        "\n");
}

/* for options */
int duration = 10;
char filename[MAX_FILE_NAME_LENGTH] = "testfile.tmp";
int request_count = 100;
char human_readable[2];
char output_filename[MAX_FILE_NAME_LENGTH];
FILE *output_file;
int page_size = 4096;
int print_detail = 0;
int random_addr = 0;
int rampup_interval = 0; /* in us (microsecond) */
off_t start_addr = 0;
off_t seek_span = 16*1024*1024;
int thread_count = 1;
int write_percent = 50;

void close_file()
{
    if (output_file != NULL)
        fclose(output_file);
}

void print_option_values()
{
    fprintf(GET_OUTPUT(output_file), "configuration: %s"
           "request_count %d , filename %s , %s"
           "page_size %d , write_percent %d , random_addr %d , duration %d , %s"
           "start_addr %lld , seek_span %lld , thread_count %d , %s"
           "rampup_interval %d , print_detail %d , output_filename %s\n",
           human_readable, 
           request_count, filename, human_readable,
           page_size, write_percent, random_addr, duration, human_readable,
           (long long int)start_addr, (long long int)seek_span, thread_count,
            human_readable,
           rampup_interval, print_detail, output_filename);
}

/* for stats */
time_t sum_time = 0;
long total_count = 0;
time_t w_sum_time = 0;
long w_total_count = 0;
time_t r_sum_time = 0;
long r_total_count = 0;

/* pthread init */
pthread_mutex_t mutex_mix_sum = PTHREAD_MUTEX_INITIALIZER;

inline off_t align_address(off_t addr)
{
    if (addr < SECTOR_SIZE)
        addr = 0;
    else
        addr = addr - addr % SECTOR_SIZE;
    
    return addr;
}

/*
 * check whether the directories exist, make directories when directories do not
 * exist and "create" is 1. Return 0 on successful completion, otherwise return
 * -1.
 */
int check_path(const char *filename, int create)
{
    struct stat s;
    int err_stat = stat(filename, &s);
    if (err_stat != -1 && S_ISBLK(s.st_mode))
        return 0;
    if ( err_stat == -1 && errno != ENOENT) {
        perror("check_path:blkfile_check:stat");
        return errno;
    }
    
    char *file = strdup(filename);
    char *dir = dirname(file);
    if (dir == NULL) {
        fprintf(stderr, "failed to get directory for %s.", filename);
        return -1;
    }
    if (strcmp(dir, ".") == 0 || strcmp(dir, "/") == 0 ||
        strcmp(dir, "./") == 0) {
        return 0;
    }

    err_stat = stat(dir, &s);
    if (err_stat != -1 && S_ISDIR(s.st_mode))
        return 0;
    if ( err_stat == -1 && errno != ENOENT) {
        perror("check_path:dir_check:stat");
        return errno;
    }
    
    if (create > 0) {
        
#if defined(__linux__) || defined(__APPLE__)
    
        char command[MAX_FILE_NAME_LENGTH];
        snprintf(command, MAX_FILE_NAME_LENGTH - 1, "mkdir -p -m 755 %s", dir);
        if(system(command) != -1)
            return 0;
#endif
        
        return -1;
    }
    return 0;
}

void get_options(int argc, char **argv)
{
    int duration_enabled = 0;
    int req_count_enabled = 0;
    int opt;
    while ((opt = getopt(argc, argv, "d:f:hHn:o:Pp:rR:s:S:t:w:")) != -1) {
        switch (opt) {
        case 'd':
            duration = atoi(optarg);
            if (duration <= 0) {
                printf("incorrect value %s for -d <duration>.\n", optarg);
                exit(-1);
            }
            duration_enabled = 1;
            break;
        case 'f':
            strncpy(filename, optarg, MAX_FILE_NAME_LENGTH - 1);
            if (check_path(filename, 1) != 0) {
                printf("Failed to validate/create path for %s\n", filename);
                exit(-2);
            }
            break;
        case 'n':
            request_count = atoi(optarg);
            if (request_count <= 0) {
                printf("incorrect value %s for -n <count>.\n", optarg);
                exit(-3);
            }
            req_count_enabled = 1;
            break;
        case 'h':
            usage();
            exit(0);
            break;
        case 'H':
            strcpy(human_readable, "\n");
            break;
        case 'o':
            strncpy(output_filename, optarg, MAX_FILE_NAME_LENGTH - 1);
            if (check_path(output_filename, 1) != 0) {
                printf("Failed to validate/create path for %s\n",
                       output_filename);
                exit(-4);
            }
            output_file = fopen(output_filename, "a+");
            if (output_file == NULL) {
                perror("Error open output file.\n");
                exit(errno);
            }
            break;
        case 'p':
            page_size= atoi(optarg);
            if (page_size < 1 ||  page_size > 2048) {
                printf("incorrect value %s for -p <size>.\n", optarg);
                exit(-5);
            }
            page_size *= SECTOR_SIZE;
            break;
        case 'P':
            print_detail = 1;
            break;
        case 'r':
            random_addr = 1;
            break;
        case 'R':
            rampup_interval = atoi(optarg);
            if (rampup_interval < 1) {
                printf("incorrect value %s for -R <time>.\n", optarg);
                exit(-6);
            }
            break;
        case 's':
            start_addr = atoll(optarg);
            if (start_addr < 0) {
                printf("incorrect value %s for -s <addr>.\n", optarg);
                exit(-7);
            }
            align_address(start_addr);
            break;
        case 'S':
            seek_span = atoll(optarg);
            if (seek_span <= 0) {
                printf("incorrect value %s for -S <addr>.\n", optarg);
                exit(-8);
            }
            align_address(seek_span);
            break;
        case 't':
            thread_count = atoi(optarg);
            if (thread_count < 1) {
                printf("incorrect value %s for -t <count>.\n", optarg);
                exit(-9);
            }
            break;
        case 'w':
            write_percent = atoi(optarg);
            if (write_percent > 100 || write_percent < 0) {
                printf("incorrect value %s for -w <percent>, "
                       "should be [0, 100].\n", optarg);
                exit(-10);
            }
            break;
        case ':':
            printf("%s takes an argument which is missing.\n", optarg);
            break;
        case '?':
            usage();
            exit(0);
            break;
        }
    }
    if (optind < argc) {
        printf("There are invalid options.\n");
        usage();
        exit(-11);
    }
    
    /* disable another stop timer if only one timer specified */
    if (duration_enabled && !req_count_enabled)
        request_count = INT_MAX;
    if (!duration_enabled && req_count_enabled)
        duration = INT_MAX;
}

inline off_t reposition_offset(int fd, long req_count)
{    
    off_t offset = 0;
    off_t cursor = 0;
    
    if (random_addr > 0) {
        
        /* get random address */
        struct timeval tv_rand;
        if (gettimeofday(&tv_rand, NULL) < 0) {
            perror("reposition_offset:gettimeofday()");
            exit(errno);
        }
        srand((unsigned int)(tv_rand.tv_usec * tv_rand.tv_sec * seek_span));
        cursor = (double)rand() / RAND_MAX * seek_span;
        cursor = align_address(cursor);
        
        /* reposition the offset of open file.
         * For hard disk, lseek dosen't move the disk arm to specified address,
         * the overhead of lseek is usually very small, should be less than 2us.
         * the overhead of disk arm moving is included in read/write function.
         */
        offset = lseek64(fd, cursor, SEEK_SET);
        if (offset == -1) {
            fprintf(stderr, "The end offset %lld specified by -S may be larger "
                    "than the file length.\n", (long long int)seek_span);
            perror("reposition_offset:lseek:");
            exit(errno);
        }
    } else {
        
        /* rewind to file start when advancing beyond the file size. */
        if ((off_t)((req_count + 1) * page_size) > seek_span) {
            offset = lseek64(fd, start_addr, SEEK_SET);
            if (offset == -1) {
                perror("lseek(): init start_addr error.\n");
                exit(errno);
            }
        } else {
            /* do nothing, file offset automatically advances sequentially. */
            offset = req_count * page_size;      
        }
    }
    return offset;
}

inline int should_write()
{    
    int is_write = 1;
    if (write_percent == 0) {
        is_write = 0;
    } else if (write_percent == 100) {
        is_write = 1;
    } else {
        
        /* flip the coin to decide read or write */
        struct timeval tv_rand;
        if (gettimeofday(&tv_rand, NULL) < 0) {
            perror("should_write:gettimeofday()");
            exit(errno);
        }
        srand((unsigned int)(tv_rand.tv_usec * tv_rand.tv_sec));
        int perc = (double)rand() / RAND_MAX * 100;
        if (perc < write_percent) {
            is_write = 1;
        } else {
            is_write = 0;
        }        
    }
    return is_write;
}

void *do_io()
{    
    int iData = 33;
    char *buffer;
    ssize_t retval = 0;
    struct timeval tv_before;
    struct timeval tv_after;
    off_t offset;
    
    long local_count = 0;
    long local_sum_time = 0;
    long time_elapsed = 0;
    long r_count = 0;
    long w_count = 0;
    long r_time = 0;
    long w_time = 0;
        
    int flags = O_CREAT | O_RDWR | O_SYNC;
#ifdef __linux__
    flags |= O_DIRECT;
    flags |= O_LARGEFILE;
#endif
    
    int fd_test = open(filename, flags, 0666);
    if (fd_test < 0) {
        fprintf(stderr, "error to open file %s, please check permission.\n",
                filename);
        perror("do_io:open()");
        exit(errno);
    }
    
    if (posix_memalign((void **)&buffer, SECTOR_SIZE, page_size)) {
        perror("do_io:posix_memalign()");
        exit(errno);
    }
    
    struct timeval tv_start;    
    if (gettimeofday(&tv_start, NULL) < 0) {
        perror("do_io:gettimeofday()");
        exit(errno);
    }
    
    time_t start_time, stop_time;
    start_time = time(NULL);
    stop_time = start_time + duration;
    
    long i=0;
    while (time(NULL) < stop_time && i < request_count) {
        
        offset = reposition_offset(fd_test, i);
        
        int is_write = should_write();
        
        /* prepare human readable data for write */
        if (is_write > 0) {
            iData++;
            if ( iData > 126 ) iData = 33;
            memset(buffer, iData, page_size);
        }
        
        time_elapsed = 0;
        
        if (gettimeofday(&tv_before, NULL) < 0){
            perror("do_io:gettimeofday()");
            exit(errno);
        }
        
        if (is_write > 0) {
            retval = write(fd_test, buffer, page_size);
        } else {
            retval = read(fd_test, buffer, page_size);
        }
        
        if (retval < 0) {
            perror(is_write > 0 ? "write() error.\n" : "read() error.\n");
            exit(errno);
        }
        if (gettimeofday(&tv_after, NULL) < 0){
            perror("do_io:gettimeofday()");
            exit(errno);
        }
        
        time_elapsed=(tv_after.tv_sec - tv_before.tv_sec) * 1000000 +
                        tv_after.tv_usec - tv_before.tv_usec;
        
        if (print_detail > 0)
            fprintf(GET_OUTPUT(output_file), "io,%ld,%s,%d,%lld,%ld\n",
                    tv_before.tv_sec, is_write == 1 ? "w": "r",
                    page_size, (long long int)offset, time_elapsed);
        
        local_count++;
        local_sum_time += time_elapsed;
        if (is_write > 0) {
            w_count++;
            w_time += time_elapsed;
        } else {
            r_count++;
            r_time += time_elapsed;
        }
        
        i++;
    }
    
    if (thread_count > 1) {
        fprintf(GET_OUTPUT(output_file), "thread: page_size %d , "
               "[ read_count %ld , read_time(us) %ld , avg_latency(us) %ld ], "
               "[ write_count %ld , write_time(us) %ld , avg_latency(us) %ld ], "
               "[ total_count %ld , time(us) %ld , avg_latency(us) %ld ]\n",
               page_size,
               r_count, r_time, r_count == 0 ? 0 : r_time / r_count,
               w_count, w_time, w_count == 0 ? 0 : w_time / w_count,
               local_count, local_sum_time,
               local_sum_time == 0 ? 0 : local_sum_time / local_count);
    }
    
    pthread_mutex_lock(&mutex_mix_sum);
    sum_time += local_sum_time;
    total_count += local_count;
    w_sum_time += w_time;
    w_total_count += w_count;
    r_sum_time += r_time;
    r_total_count += r_count;
    pthread_mutex_unlock(&mutex_mix_sum);
    
    close(fd_test);
    free(buffer);
    return NULL;
}

void start_io_threads()
{
    pthread_t *g_tid = NULL;
    g_tid = (pthread_t *)malloc(sizeof(pthread_t *) * thread_count);
    int i=0;
    for (i = 0; i < thread_count; i++) {
        g_tid[i] = (pthread_t)malloc(sizeof(pthread_t));
        
        int ret = pthread_create(&g_tid[i], NULL, do_io, NULL);
        if (ret != 0) {
            perror("error creating threads.\n");
            if (ret == EAGAIN) {
                perror("not enough system resources.\n");
            }
            exit(errno);
        }
        
        if (rampup_interval > 0) {
            struct timespec ts, rem;            
            ts.tv_sec = (time_t) (rampup_interval / 1000000);
            ts.tv_nsec = (long) (rampup_interval % 1000000) * 1000;
            while (nanosleep(&ts, &rem) == -1) {
                if (errno == EINTR) {
                    memcpy(&ts, &rem, sizeof(struct timespec));
                } else {
                    printf("sleep time invalid %ld s %ld ns",
                                        ts.tv_sec, ts.tv_nsec);
                    break;
                }
            }
        }        
    }    
    for (i = 0; i< thread_count; i++) {
        if (pthread_join(g_tid[i], NULL) != 0)
            perror("thread wait error.\n");
    }
}

int main(int argc, char **argv)
{
    atexit(close_file);
    
    get_options(argc, argv);
    print_option_values();
    
    start_io_threads();
    
    fprintf(GET_OUTPUT(output_file), "summary: page_size %d , %s"
           "[ read_count %ld , read_time(us) %ld , avg_latency(us) %ld ], %s"
           "[ write_count %ld , write_time(us) %ld , avg_latency(us) %ld ], %s"
           "[ total_count %ld , time(us) %ld , avg_latency(us) %ld ]\n",
           page_size, human_readable, 
           r_total_count, r_sum_time,
           r_total_count == 0 ? 0 : r_sum_time / r_total_count, human_readable, 
           w_total_count, w_sum_time,
           w_total_count == 0 ? 0 : w_sum_time / w_total_count, human_readable, 
           total_count, sum_time,
           sum_time == 0 ? 0 : sum_time / total_count);
    
    return 0;
}


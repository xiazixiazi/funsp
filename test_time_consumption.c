#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <fcntl.h>
#include <time.h>
#include <stdarg.h>
#include <string.h>
#include <getopt.h>

const char *type_list[] = {
    "clang_O0",
    "clang_O1",
    "clang_O2",
    "clang_O3",
    "clang_O0_split_mean",
    "clang_O1_split_mean",
    "clang_O2_split_mean",
    "clang_O3_split_mean",
    "obfuscator-llvm-O0-sub-fla-bcf"
};
const int type_count = sizeof(type_list) / sizeof(type_list[0]);

char *program_list[][5] = {
    {"echo", "hello world", NULL},
    {"ls", "/", NULL},
    {"cat", "LICENSE", NULL},
    {"stat", "LICENSE", NULL},
    {"wc", "LICENSE", NULL},
    {"date", NULL},
    {"uptime", NULL},
    {"df", "-h", NULL},
    {"du", "-sh", NULL},
    {"sha1sum", "LICENSE", NULL},
    {"sha224sum", "LICENSE", NULL},
    {"sha256sum", "LICENSE", NULL},
    {"sha384sum", "LICENSE", NULL},
    {"sha512sum", "LICENSE", NULL},
    {"base32", "LICENSE", NULL},
    {"base64", "LICENSE", NULL},
};
const int program_count = sizeof(program_list) / sizeof(program_list[0]);

int debug = 0;

void logging(const char *level, const char *format, va_list args) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_buf[20];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);
    
    fprintf(stderr, "%s - %s - ", time_buf, level);
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
}

void log_info(const char *format, ...) {
    va_list args;
    va_start(args, format);
    logging("INFO", format, args);
    va_end(args);
}

void log_debug(const char *format, ...) {
    if (!debug) return;
    va_list args;
    va_start(args, format);
    logging("DEBUG", format, args);
    va_end(args);
}

int main(int argc, char *argv[]) {
    int times = 10000;
    int opt;
    char *envp[1] = {NULL};
    
    while ((opt = getopt(argc, argv, "t:d")) != -1) {
        switch (opt) {
            case 't':
                times = atoi(optarg);
                break;
            case 'd':
                debug = 1;
                break;
            default:
                fprintf(stderr, "Usage: %s [-t times] [-d]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    log_info("Start");
    struct timeval start, end;
    gettimeofday(&start, NULL);

    FILE *fp = fopen("output/test_time_consumption_result.csv", "w");
    if (!fp) {
        perror("fopen failed");
        exit(EXIT_FAILURE);
    }

    // Write CSV header
    fprintf(fp, "program,times");
    for (int i = 0; i < type_count; i++)
        fprintf(fp, ",%s", type_list[i]);
    fprintf(fp, "\n");
    fflush(fp);

    for (int p = 0; p < program_count; p++) {
        const char *program = program_list[p][0];
        char line[4096] = {0};
        snprintf(line, sizeof(line), "%s,%d", program, times);

        for (int t = 0; t < type_count; t++) {
            const char *type = type_list[t];
            char path[1024];
            snprintf(path, sizeof(path), "./datasets/coreutils-8.30/%s/%s", type, program);

            struct timeval test_start, test_end;
            gettimeofday(&test_start, NULL);

            pid_t ppid = vfork();
            if (ppid == 0) { // Child process
                int null_fd = open("/dev/null", O_RDWR);
                if (null_fd == -1) {
                    perror("open failed");
                    exit(EXIT_FAILURE);
                }
                dup2(null_fd, STDIN_FILENO);
                dup2(null_fd, STDOUT_FILENO);
                dup2(null_fd, STDERR_FILENO);
                close(null_fd);

                for (int i = 0; i < times; i++) {
                    pid_t pid = vfork();
                    if (pid == 0) { // Grandchild process
                        execve(path, program_list[p], envp);
                        fprintf(stderr, "%s: %m\n", path);
                        exit(EXIT_FAILURE);
                    } else if (pid > 0) {
                        waitpid(pid, NULL, 0);
                    } else {
                        perror("fork failed");
                        exit(EXIT_FAILURE);
                    }
                }
                exit(EXIT_SUCCESS);
            } else if (ppid > 0) {
                waitpid(ppid, NULL, 0);
                gettimeofday(&test_end, NULL);
                double elapsed = (test_end.tv_sec - test_start.tv_sec) +
                               (test_end.tv_usec - test_start.tv_usec) / 1000000.0;
                char temp[64];
                snprintf(temp, sizeof(temp), ",%.6f", elapsed);
                strcat(line, temp);
            } else {
                perror("fork failed");
                exit(EXIT_FAILURE);
            }
        }

        fprintf(fp, "%s\n", line);
        fflush(fp);
        log_debug("Written data for %s", program);
    }

    fclose(fp);
    gettimeofday(&end, NULL);
    double total_time = (end.tv_sec - start.tv_sec) +
                      (end.tv_usec - start.tv_usec) / 1000000.0;
    log_info("[*] Time Cost: %.6f seconds", total_time);

    return 0;
}

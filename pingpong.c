#define _GNU_SOURCE
#include <errno.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <stdint.h>

#ifndef LOOPS
#define LOOPS 100000
#endif

#ifndef USE_AFFINITY
#define USE_AFFINITY 1
#endif

#ifndef PARENT_CPU
#define PARENT_CPU 0
#endif

#ifndef CHILD_CPU
#define CHILD_CPU 0
#endif

// In the ideal ping-pong, each round-trip has exactly two context switches.
#ifndef CS_PER_RTRIP
#define CS_PER_RTRIP 2
#endif

static void die(const char *msg) { perror(msg); exit(EXIT_FAILURE); }

static uint64_t ns_now(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) die("clock_gettime");
    return (uint64_t)ts.tv_sec * 1000000000ull + ts.tv_nsec;
}

static void pin_to_cpu(int cpu) {
#if USE_AFFINITY
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    if (sched_setaffinity(0, sizeof(set), &set) != 0) die("sched_setaffinity");
#else
    (void)cpu;
#endif
}

int main(void) {
    int p2c[2], c2p[2];
    if (pipe(p2c) < 0) die("pipe p2c");
    if (pipe(c2p) < 0) die("pipe c2p");

    pin_to_cpu(PARENT_CPU);
    pid_t pid = fork();
    if (pid < 0) die("fork");

    if (pid == 0) {
        // child
        pin_to_cpu(CHILD_CPU);
        close(p2c[1]); close(c2p[0]);

        int hey;
        // warm-up a few exchanges to stabilize scheduling/caches
        for (int i = 0; i < 1024; i++) {
            if (read(p2c[0], &hey, sizeof(hey)) != sizeof(hey)) die("child warmup read");
            hey++;
            if (write(c2p[1], &hey, sizeof(hey)) != sizeof(hey)) die("child warmup write");
        }

        for (int i = 0; i < LOOPS; i++) {
            if (read(p2c[0], &hey, sizeof(hey)) != sizeof(hey)) die("child read");
            hey++;
            if (write(c2p[1], &hey, sizeof(hey)) != sizeof(hey)) die("child write");
        }
        _exit(0);

    } else {
        // parent
        close(p2c[0]); close(c2p[1]);

        int hey = 0;

        // warm-up
        for (int i = 0; i < 1024; i++) {
            if (write(p2c[1], &hey, sizeof(hey)) != sizeof(hey)) die("parent warmup write");
            if (read(c2p[0], &hey, sizeof(hey)) != sizeof(hey)) die("parent warmup read");
        }

        uint64_t t0 = ns_now();

        for (int i = 0; i < LOOPS; i++) {
            if (write(p2c[1], &hey, sizeof(hey)) != sizeof(hey)) die("parent write");
            if (read(c2p[0], &hey, sizeof(hey)) != sizeof(hey)) die("parent read");
        }

        uint64_t t1 = ns_now();
        wait(NULL);

        double total_ns = (double)(t1 - t0);
        double ns_per_rtrip = total_ns / LOOPS;
        double ns_per_cs = ns_per_rtrip / CS_PER_RTRIP;

        printf("# %d pipe round-trips (parent->child->parent)\n", LOOPS);
#if USE_AFFINITY
        printf("# Affinity: parent->CPU%d, child->CPU%d\n", PARENT_CPU, CHILD_CPU);
#else
        printf("# No Affinity\n");
#endif
        printf("# Assumed %d context switches per round-trip\n", CS_PER_RTRIP);
        printf("total time: %.6f (s)\n", total_ns / 1e9);
        printf("per round-trip: %.2f (ns)\n", ns_per_rtrip);
        printf("per context switch (approx): %.2f (ns)\n", ns_per_cs);
        printf("round-trips per second: %.0f\n", 1e9 / ns_per_rtrip);
    }
    return 0;
}

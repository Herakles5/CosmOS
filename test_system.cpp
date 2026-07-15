#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>

long long get_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000LL + tv.tv_usec / 1000;
}

int main() {
    signal(SIGCHLD, SIG_IGN);
    long long t1 = get_ms();
    system("echo test > /dev/null");
    long long t2 = get_ms();
    printf("system() took %lld ms\n", t2 - t1);
    return 0;
}

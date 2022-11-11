#define NUM_PROCESSES 3
#define NUM_PIPES 6

int main(void) {
    int pfd[10][2];
    int i;

    for (i = 0; i < NUM_PIPES; i++) {
        if (pipe(pfd[i]) == -1) {
        }
    }

    return 0;
}

/**
 *
 *
 * Cigarette smokers problem
 *
 * Agent process
 *
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>


/**
 *
 * message types
 *
 */

#define AGENT 0

// mtype paper request 1
// mtype paper response 4
#define PAPER 1

// mtype tobacco request 2
// mtype tobacco response 5
#define TOBACCO 2

// mtype matches request 3
// mtype matches response 6
#define MATCHES 3

// mtype smoker received response
#define SUCCESS 7


/**
 *
 * message buffer
 *
 */

struct msgbuf {
    long mtype;
    char mtext[100];
};


/**
 *
 * message queue helper function
 *
 */

void _send_message(int *msqid, long mtype, char *mtext) {
    // send message to queue helper function
    struct msgbuf buf;
    int mlen = strlen(mtext) + 1;
    int flag = 0; // block when queue is full

    memcpy(buf.mtext, mtext, mlen);
    buf.mtype = mtype;

    if (msgsnd(*msqid, (struct msgbuf *)&buf, mlen, flag) == -1) {
        perror("msgsnd");
    }

    printf("[msg -->] %s\n", mtext);
}

void _receive_message(int *msqid, long mtype) {
    // receive message from queue helper function
    struct msgbuf buf;
    int mlen = sizeof(buf) - sizeof(long);
    int flag = 0; // block when queue is empty

    if (msgrcv(*msqid, (struct msgbuf *)&buf, mlen, mtype, flag) == -1) {
        perror("msgrcv");
    }

    printf("[msg <--] %s\n", buf.mtext);
}

char *ingredient_to_str(int mtype) {
    char *ingredient;

    switch (mtype) {
        case PAPER:
            ingredient = "paper";
            break;
        case TOBACCO:
            ingredient = "tobacco";
            break;
        case MATCHES:
            ingredient = "matches";
            break;
    }

    return ingredient;
}


int reload_table() {
    // agent  takes two random ingredients
    // and puts them on the table for smokers
    // function returns the only ingredient that is not on the table
    // smoker who has that one item will be chosen from the queue
    int missing_ingredient = rand() % 3 + 1;

    printf("Agent is putting new items on the table...\n");

    switch (missing_ingredient) {
        case PAPER:
            printf("Agent put tobacco and matches on the table\n");
            break;
        case TOBACCO:
            printf("Agent put paper and matches on the table\n");
            break;
        case MATCHES:
            printf("Agent put paper and tobacco on the table\n");
            break;
    }

    printf("Missing ingredient is %s\n", ingredient_to_str(missing_ingredient));

    // return the only ingredient that is not on the table
    return missing_ingredient;
}

void accept_smoker_request(int *msqid, int mtype) {
    // accept smoker request from the queue
    // accept the first smoker whose request matches the missing ingredient
    // reads message from the queue
    printf("Accepting smoker request for %s\n", ingredient_to_str(mtype));
    _receive_message(msqid, (long)mtype);
    printf("Request for %s received from smoker successfully\n", ingredient_to_str(mtype));
}

void supply_smoker(int *msqid, int mtype) {
    // send available items to smoker
    // sends response to the queue
    char *mtext;

    printf("Agent is supplying smoker with ");

    switch (mtype) {
        case PAPER:
            printf("tobacco and matches\n");
            mtext = "Tobacco and matches for you!";
            break;
        case TOBACCO:
            printf("paper and matches\n");
            mtext = "Paper and matches for you!";
            break;
        case MATCHES:
            printf("paper and tobacco\n");
            mtext = "Paper and tobacco for you!";
            break;
    }

    _send_message(msqid, (long)mtype+3, mtext);

    printf("Agent sent items to smoker\n");
}

void acknowledge_end(int *msqid) {
    // reads message from the queue
    long mtype = SUCCESS;

    printf("Waiting for smoker to receive items...\n");
    _receive_message(msqid, mtype);
    printf("Smoker is served successfully. All done!\n\n\n");
}

void send_request(int *msqid, int mtype) {
    // sends message to the queueu
    char *mtext = ingredient_to_str(mtype);
    printf("I have %s. Waiting for agent...\n", ingredient_to_str(mtype));
    _send_message(msqid, mtype, mtext);
    sleep(2);
}

void receive_ingredient(int *msqid, int mtype) {
    // reads message from the queue
    printf("Waiting for agent to give me %s...\n", ingredient_to_str(mtype));
    _receive_message(msqid, (long)mtype+3);
    printf("Received %s from agent\n", ingredient_to_str(mtype));
    printf("Smoking the cigarette\n");
    sleep(5);
}

void send_end(int *msqid) {
    // sends message to the queue
    long mtype = SUCCESS;
    char *mtext = "Cigarette smoked successfully";

    printf("Sending end message to smoker...\n");
    _send_message(msqid, mtype, mtext);
    printf("Agent is notified successfully. All done!\n\n\n");
    sleep(2);
}

void run_agent(int *msqid) {
    int smoker_ingredient;

    while (1) {
        smoker_ingredient = reload_table();
        accept_smoker_request(msqid, smoker_ingredient);
        sleep(2);
        supply_smoker(msqid, smoker_ingredient);
        sleep(2);
        acknowledge_end(msqid);
        sleep(5);
    }
}

void run_consumer(int *msqid, int missing_ingredient) {
    while (1) {
        send_request(msqid, missing_ingredient);
        receive_ingredient(msqid, missing_ingredient);
        send_end(msqid);
    }
}

int main(int argc, char *argv[]) {
    int type;
    int msqid;
    int msqkey;

    if (argc != 2) {
        perror("Missing type!\n");
        exit(1);
    }
    type = atoi(argv[1]);

    srand((unsigned int)time(NULL));

    // message queue
    msqkey = getuid();
    printf("msqkey: %d\n", msqkey);

    if ((msqid = msgget(msqkey, 0600 | IPC_CREAT)) == -1) {
        perror("msgget");
        exit(1);
    }
    printf("msqid: %d\n", msqid);

    switch (type) {
        case AGENT:
            run_agent(&msqid);
        case PAPER:
            run_consumer(&msqid, PAPER);
        case TOBACCO:
            run_consumer(&msqid, TOBACCO);
        case MATCHES:
            run_consumer(&msqid, MATCHES);
    }

    return 0;
}

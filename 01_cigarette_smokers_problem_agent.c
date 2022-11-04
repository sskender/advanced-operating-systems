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

// mtype paper request 0
// mtype paper response 3
#define PAPER 0

// mtype tobacco request 1
// mtype tobacco response 4
#define TOBACCO 1

// mtype matches request 2
// mtype matches response 5
#define MATCHES 2

// mtype smoker received response
#define SUCCESS 42


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
    int flag = 0; // block when queue is empty

    if (msgrcv(*msqid, (struct msgbuf *)&buf, sizeof(buf)-sizeof(long), mtype, flag) == -1) {
        perror("msgrcv");
        exit(1);
    }

    printf("[msg <--] %s\n", buf.mtext);
}


int reload_table() {
    // agent  takes two random items
    // and puts them on the table for smoker processes
    // function returns the only item that is not on the table

    // TODO remove
    /* return 1; */
    // TODO 
    // smoker who needs that item will be chosen from the queue
    // missing item type matches the smoker request as mtype in buffer
    int missing_item = rand() % 3;

    printf("Agent is putting new items on the table ...\n");

    switch (missing_item) {
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

    // return the only item not on the table
    return missing_item;
}

void accept_smoker_request(int *msqid, long mtype) {
    // accept smoker request from the queue
    // accept the first smoker whose request matches
    // currently available items on the table
    // reads message from the queue
    
    // TODO wrapper for string
    printf("Accepting smoker request for %ld\n", mtype);

    _receive_message(msqid, mtype);

    printf("Message received from smoker successfully\n");
}

void supply_smoker(int *msqid, long mtype) {
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

    _send_message(msqid, mtype+3, mtext);

    printf("Agent sent items to smoker\n");
}

void acknowledge_smoker_response(int *msqid) {
    // reads message from the queue
    int mtype = SUCCESS;

    printf("Waiting for smoker to receive items\n");
    _receive_message(msqid, mtype);
    printf("Smoker is served successfully\n");
}

int main(void) {
    int msqid;
    int msqkey;

    int missing_item;

    srand((unsigned int)time(NULL));

    // create a new message queue
    msqkey = getuid();
    printf("msqkey: %d\n", msqkey);

    if ((msqid = msgget(msqkey, 0600 | IPC_CREAT)) == -1) {
        perror("msgget");
        exit(1);
    }

    printf("msqid: %d\n", msqid);

    // TODO here
    while (1) {
        missing_item = reload_table();
        printf("missing item is %d\n", missing_item);

        accept_smoker_request(&msqid, missing_item);
        supply_smoker(&msqid, missing_item);
        acknowledge_smoker_response(&msqid);
    }

    return 0;
}

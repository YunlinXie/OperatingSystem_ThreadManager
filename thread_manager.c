// author: Yunlin Xie

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>

/**************************************************************************************************/
/**************************************Linked List Part********************************************/
// This is linked list to store commands
struct Node {
    char* input;
    struct Node *next;
};

// Create a new node
void CreateNode(struct Node *thisNode, char input[20]) {
    thisNode->input = (char*)malloc(sizeof(char) * 20);
    if(thisNode->input == NULL){
        printf("Error! memory not allocated.\n");
        exit(1);
    }
    strcpy(thisNode->input, input);
    thisNode->next = NULL;
    return;
}

// Deallocate the linked list
void DeallocateList(struct Node *headNode) {
    struct Node *ptr = headNode;
    while(headNode != NULL) {
        headNode = headNode->next ;
        free(ptr);
        ptr = headNode;
    }
}


/**************************************************************************************************/
/**************************************************************************************************/
void* thread_runner(void*);

struct THREADDATA_STRUCT {
    pthread_t creator;
};

//thread mutex lock for access to the log index
pthread_mutex_t tlockLogIndex = PTHREAD_MUTEX_INITIALIZER;

//thread mutex lock for access to the THREADDATA allocation
pthread_mutex_t tlockThreadAllocate = PTHREAD_MUTEX_INITIALIZER;

//thread mutex lock for access to the THREADDATA deallocation
pthread_mutex_t tlockThreadDeallocate = PTHREAD_MUTEX_INITIALIZER;

//thread mutex lock for updating to the first node
pthread_mutex_t tlockUpdateFirst = PTHREAD_MUTEX_INITIALIZER;

//thread mutex lock for updating the flag
pthread_mutex_t tlockUpdateFlag = PTHREAD_MUTEX_INITIALIZER;

// two thread id
pthread_t tid1, tid2;

typedef struct THREADDATA_STRUCT THREADDATA;
THREADDATA* p=NULL;

//variable for indexing of messages by the logging function
int logindex = 0;

// A flag to indicate if the reading of input is complete, thread2 knows when to stop
bool is_reading_complete = false;

// First node and previous first node for thread2 to check
struct Node *dummyFirst;
struct Node *dummyPreviousFirst;

// buffer for reading input
char buf[20];

/**************************************************************************************************/
/******************************************helper functions****************************************/
int getLogindex() {
    pthread_mutex_lock(&tlockLogIndex);
    logindex++;
    pthread_mutex_unlock(&tlockLogIndex);
    return logindex;
}

void updateIsReadingComplete() {
    pthread_mutex_lock(&tlockUpdateFlag);
    if (is_reading_complete == false) {
        is_reading_complete = true;
    } else {
        is_reading_complete = false;
    }
    pthread_mutex_unlock(&tlockUpdateFlag);
}

void printLogMsg(time_t now, int logid, pthread_t threadid, pid_t pid, char msg[100]) {
    int year, month, day, hour, minute, second;
    struct tm *local = localtime(&now);
    year = local->tm_year + 1900;
    month = local->tm_mon + 1;
    day = local->tm_mday;
    hour = local->tm_hour;
    minute = local->tm_min;
    second = local->tm_sec;
    if (hour < 12) { // before noon
        printf("Logindex %d, thread %u, PID %d, %02d/%02d/%d  %02d:%02d:%02d am: %s\n", logid, (unsigned int)threadid, pid, day, month, year, hour, minute, second, msg);
    } else { // after noon
        printf("Logindex %d, thread %u, PID %d, %02d/%02d/%d  %02d:%02d:%02d am: %s\n", logid, (unsigned int)threadid, pid, day, month, year, hour-12, minute, second, msg);
    }
}

/**************************************************************************************************/
/***************************************thread runner**********************************************/
void* thread_runner(void* x) {
    pthread_t me;
    me = pthread_self();

    // variables to store time components
    time_t now;

    pthread_mutex_lock(&tlockThreadAllocate);
    if (p == NULL) {
        p = (THREADDATA*) malloc(sizeof(THREADDATA));
        p->creator = me;
        time(&now);
        printLogMsg(now, getLogindex(), me, getpid(), "ALLOCATE THREADDATA");
    }
    pthread_mutex_unlock(&tlockThreadAllocate);

    struct Node *lastNode = (struct Node*)malloc(sizeof(struct Node));
    CreateNode(lastNode, "invalid last node");
    lastNode->next = NULL;
    dummyFirst = lastNode;
    dummyPreviousFirst = lastNode;

    while (true) {
        if (p!=NULL && p->creator==me) {
            fgets(buf, 20, stdin);
        }

        // Case 1: invalid input
        // when entering a empty line exit
        if (buf[0]=='\n') {
            if (is_reading_complete) {
                updateIsReadingComplete();
            }
            if (p!=NULL && p->creator!=me) { // different thread than the one store in p
                // deallocate p
                pthread_mutex_lock(&tlockThreadDeallocate);
                free(p);
                time(&now);
                printLogMsg(now, getLogindex(), me, getpid(), "DEALLOCATE THREADDATA");
                pthread_mutex_unlock(&tlockThreadDeallocate);

                // deallocate the linked list
                pthread_mutex_lock(&tlockThreadDeallocate);
                DeallocateList(dummyFirst);
                time(&now);
                printLogMsg(now, getLogindex(), me, getpid(), "DEALLOCATE LINKED LIST");
                pthread_mutex_unlock(&tlockThreadDeallocate);
            }
            // exiting message
            time(&now);
            printLogMsg(now, getLogindex(), me, getpid(), "EXITING THREAD RUNNER FUNCTION");
            return NULL;
        }
        // case 2: valid input
        // different tasks for different threads
        if (p!=NULL && p->creator==me) { // same thread as the one store in p
            pthread_mutex_unlock(&tlockUpdateFirst);
            struct Node *newNode = (struct Node*)malloc(sizeof(struct Node));
            CreateNode(newNode, buf);
            newNode->next = dummyFirst;
            dummyFirst = newNode;
            memset(buf, 0, 20);
            time(&now);
            printLogMsg(now, getLogindex(), me, getpid(), "UPDATED FIRST NODE");
            pthread_mutex_unlock(&tlockUpdateFirst);
            if (!is_reading_complete) {
                updateIsReadingComplete();
            }
        }
        while (is_reading_complete) { // print first node after checking
            sleep(2);
            if (p!=NULL && p->creator!=me) {
                if (dummyPreviousFirst != dummyFirst) {
                    time(&now);
                    char input[100];
                    strcpy(input, "FIRST NODE IN THE LIST IS: ");
                    strcat(input, dummyFirst->input);
                    printLogMsg(now, getLogindex(), me, getpid(), input);
                    dummyPreviousFirst = dummyFirst;
                }
            } else {
                break;
            }
        }
    }

}


/**************************************************************************************************/
/**********************************************MAIN************************************************/
int main() {
    printf("create first thread\n");
    pthread_create(&tid1, NULL, thread_runner, NULL);

    printf("create second thread\n");
    pthread_create(&tid2, NULL, thread_runner, NULL);

    printf("wait for first thread to exit\n");
    pthread_join(tid1, NULL);
    printf("first thread exited\n");

    printf("wait for second thread to exit\n");
    pthread_join(tid2, NULL);
    printf("second thread exited\n");

    return 0;
}












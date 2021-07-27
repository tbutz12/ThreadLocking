#include <sys/mman.h>
#include <linux/unistd.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

//shared memory struct
struct shared_mem {
        int visitorsGivenTours;
        int visitorsGivenToursTotal;
        int visitorsInMuseum;
        int guidesInMuseum;
        int visitorsOutside;
        int guidesOutside;
        int numTickets;
};
//semaphores
long sem_mutex_v = -1;
long sem_mutex_g = -1;
long sem_mutex_v_wait = -1;
long sem_mutex_g_wait = -1;
//arguments passed
int numVisitors = 0;
int numGuides = 0;
int pv = 0;
int dv = 0;
int pg = 0;
int dg = 0;
int sv = 0;
int sg = 0;
bool museumIsOpen = false;
int dtime = 0;
struct timeval start;
struct shared_mem *shared = NULL;
//semaphore functions
long create(int value, char name[32], char key[32]) {
        return syscall(__NR_cs1550_create, value, name, key);
}

long open(char name[32], char key[32]) {
        return syscall(__NR_cs1550_open, name, key);
}

long down(long sem_id) {
        return syscall(__NR_cs1550_down, sem_id);
}

long up(long sem_id) {
        return syscall(__NR_cs1550_up, sem_id);
}

long close(long sem_id) {
        return syscall(__NR_cs1550_close, sem_id);
}
//returns seconds passed
int getTimeOfDay(){
        struct timeval end;
        gettimeofday(&end, NULL);
        time_t secs_used = (end.tv_sec - start.tv_sec);
        return (int)secs_used;
}
//visitor arrival
void visitorArrives(int visitorID){
        dtime = getTimeOfDay();
        fprintf(stderr, "Visitor %d arrives at time %d.\n", visitorID, dtime);
        if(shared->numTickets == 0) {
                return;
        }
        shared->visitorsOutside -= 1;
        //check to see if max visitors are in museum, if so, wait
        if(shared->visitorsInMuseum == 10 && visitorID < shared->numTickets) {
                down(sem_mutex_v_wait);
        }
        //tell guide visitor arrived
        up(sem_mutex_v);
        return;

}
//visitor tours the museum
void tourMuseum(int visitorID){
        dtime = getTimeOfDay();
        shared->visitorsInMuseum += 1;
        fprintf(stderr, "Visitor %d tours the museum at time %d.\n", visitorID, dtime);
        shared->visitorsGivenTours += 1;
        //visitor tours for 2 seconds
        sleep(2);
        //tell guide visitor is done touring
        up(sem_mutex_v_wait);
        return;
}
//visitor is leaving the museum
void visitorLeaves(int visitorID){
        dtime = getTimeOfDay();
        fprintf(stderr, "Visitor %d leaves the museum at time %d.\n", visitorID, dtime);
        shared->visitorsInMuseum -= 1;
        if(shared->visitorsInMuseum == 0 || shared->visitorsInMuseum == shared->numTickets - 10) {
                // if there are no visitors left in the museum or there are no more tickets, continue
                up(sem_mutex_g_wait);
        }
        //tell guide the visitor has left
        up(sem_mutex_v_wait);
        exit(0);
        return;
}
//tour guide has arrived to museum
void tourGuideArrives(int guideID){
        dtime = getTimeOfDay();
        shared->guidesOutside += 1;
        fprintf(stderr, "Tour guide %d arrives at time %d.\n", guideID, dtime);
        shared->numTickets = 10 * shared->guidesInMuseum;
        //tell visitor guide is here
        up(sem_mutex_g);
        return;
}
//tour guide opens the museum
void openMuseum(int guideID){
        museumIsOpen = true;
        dtime = getTimeOfDay();
        shared->guidesInMuseum++;
        fprintf(stderr, "Tour guide %d opens the museum for tours at time %d.\n", guideID, dtime);
        //tell visitor the museum is now open
        up(sem_mutex_g);
        return;
}
//tour guide is leaving museum
void tourGuideLeaves(int guideID){
        dtime = getTimeOfDay();
        fprintf(stderr, "Tour guide %d leaves the museum at time %d.\n", guideID, dtime);
        shared->guidesInMuseum--;
        //if there are no more guides in the museum, close the museum
        if(shared->guidesInMuseum == 0) {
                museumIsOpen = false;
        }
        //tell visitor the guide has now left
        up(sem_mutex_g_wait);
        exit(0);
}
//visitor process
void visitorGeneratorProcess() {
        int x = 0;
        int y = 0;
        srand(sv);
        //loop through all visitors
        for (x; x < numVisitors; x++) {
                //fork process
                int pid = fork();
                if (pid == 0) {
                        //NOW IN CHILD PROCESS
                        visitorArrives(x);
                        if(shared->guidesOutside == 0) {
                                down(sem_mutex_g);
                        }

                        if(x < shared->numTickets) {
                                tourMuseum(x);
                        }

                        visitorLeaves(x);
                }
                else {
                        //NOW IN PARENT PROCESS
                        //Check probability of follower, if value is less than random value sleep for user inputted seconds
                        int value = rand() % 100 + 1;
                        if (value > pv) {
                                sleep(dv);
                        }
                }
        }
        //wait for all child processes to terminate
        for(y; y < numVisitors; y++) {
                wait(NULL);
        }
}

int main(int argc, char *argv[]){
        //process id for forking
        int pid = 0;
        gettimeofday(&start, NULL);
        numVisitors = atoi(argv[2]);
        //read user input
        pv = atoi(argv[6]);
        dv = atoi(argv[8]);
        sv = atoi(argv[10]);
        numGuides = atoi(argv[4]);
        pg = atoi(argv[12]);
        dg = atoi(argv[14]);
        sg = atoi(argv[16]);
        //random keys
        char key1[32] = "CS1550 is fun!";
        char key2[32] = "CS1550 is fun!2";
        char key3[32] = "CS1550 is fun!3";
        char key4[32] = "CS1550 is fun!4";
        char key5[32] = "CS1550 is fun!4";
        //initialize mutexes
        sem_mutex_v = create(0, "sem1", key1);
        sem_mutex_g = create(0, "sem2", key2);
        sem_mutex_v_wait = create(0, "sem3", key3);
        sem_mutex_g_wait = create(0, "sem4", key4);
        //allocate shared memory space for structure
        shared = (struct shared_mem*)mmap(NULL, sizeof(struct shared_mem), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);
        //initialize struct values
        shared->visitorsGivenTours = 0;
        shared->visitorsInMuseum = 0;
        shared->guidesInMuseum = 0;
        shared->visitorsOutside = numVisitors;
        shared->guidesOutside = numGuides;
        shared->numTickets = numGuides * 10;
        //integers for looping
        int x = 0;
        int y = 0;
        int i = 0;
        int z = 0;
        //forking process
        pid = fork();
        if(pid == 0) {
                //visitor process
                //NOW IN CHILD PROCESS
                visitorGeneratorProcess();
                exit(0);
        }
        else {
                //guide process
                //NOW IN PARENT PROCESS
                srand(sg);
                //loop through all tour guides
                for(x; x < numGuides; x++) {
                        //fork process again for tour guides
                        pid = fork();
                        if(pid == 0) {
                                //NOW IN CHILD PROCESS
                                tourGuideArrives(x);
                                //if museum is not open and there are no visitors outside, wait
                                if(!museumIsOpen && shared->visitorsOutside == 0) {
                                        down(sem_mutex_v);
                                        openMuseum(x);
                                        //shared->guidesInMuseum++;
                                }
                                //if there are less than 2 tour guides, open the museum
                                else if(shared->guidesInMuseum < 2) {
                                        openMuseum(x);
                                        //shared->guidesInMuseum++;
                                }
                                //if there are 2 tour guides in the museum, wait for guide to leave then open museum again
                                else if(shared->guidesInMuseum == 2) {
                                        down(sem_mutex_g_wait);
                                        openMuseum(x);
                                        //shared->guidesInMuseum++;
                                }
                                //if there are more than allowed visitors per guide, then loop through and wait until a spot opens, then have tour guide leave
                                if(shared->visitorsInMuseum >= 10) {
                                        for(z = 0; z < 10; z++) {
                                                down(sem_mutex_v_wait);
                                        }
                                        tourGuideLeaves(x);
                                }
                                //if there are not more than the allowed visitors, loop through again to make sure there are no left over visitors touring in the museum
                                else{
                                        for(i = 0; i < shared->visitorsInMuseum; i++) {
                                                down(sem_mutex_v_wait);
                                        }
                                }
                                //if all visitors havent finished touring
                                if(shared->visitorsGivenTours != 10) {
                                        down(sem_mutex_g_wait);
                                }
                                tourGuideLeaves(x);
                        }
                        //Check probability of follower, if value is less than random value sleep for user inputted seconds
                        else{
                                int value = rand() % 100 + 1;
                                if(value > pg) {
                                        sleep(dg);
                                }
                        }
                }
                //loop through and wait for all child processes to be terminated
                for(y; y < numGuides; y++) {
                        wait(NULL);
                }
        }
        //wait for first forked child process to terminate
        wait(NULL);
        //close all semaphores
        close(sem_mutex_v);
        close(sem_mutex_v_wait);
        close(sem_mutex_g);
        close(sem_mutex_g_wait);
        exit(0);
}

//gcc -g -m32 -o museumsim -I /u/OSLab/tmb132/Project1/linux-2.6.23.1/include/ museumsim.c
//./museumsim -m 10 -k 1 -pv 100 -dv 1 -sv 1 -pg 100 -dg 1 -sg 2 &> output.txt
// scp tmb132@thoth.cs.pitt.edu:/u/OSLab/tmb132/Project1/linux-2.6.23.1/include/museumsim .

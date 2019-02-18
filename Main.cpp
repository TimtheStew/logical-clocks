#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

/* Our main, spins up a bunch of proccesses to fight over      */
/* resources, waits to let them fight and log, then kills      */
/* them all off. Passes down it's arguments                    */
int main (int argc, char * argv[]){
  
  if (argc != 3)
  {
    perror("missing arguments - [numProcs][debug]");
    exit(-1);
  }

  /* grab our ID and the number of processes from our args     */
  int numProcs = atoi(argv[1]);
  int debug = atoi(argv[2]);
  int i;
  pid_t childs[200];

  /* loop to fork and execv all the child processes            */
  for (i =1 ; i <= numProcs; i++){

    pid_t pid = fork();
    if (pid < 0)
    {
      perror("fork() failed");
      exit(-1);
    }
    /* Child process                                           */
    if (pid == 0)
    {
      /* Set up the arguments for the Process, the first is    */
      /* the ID, then numprocs and the debug bool.             */
      char id[10];
      sprintf(id, "%d", i);
      char *args[5];
      args[0] = "procMain";
      args[1] = id;
      args[2] = argv[1];
      args[3] = argv[2];
      args[4] = NULL;
      /* execv the process, passing it the args we made        */
      execv("./ProcMain", args);
    } 
    /* Parent process                                          */
    /* add the child pid to our childs array for killing later */
    if (pid > 0)
    {
      childs[i] = pid;
    }
  }
  /* Only the parent will make it down here                   */
  /* Sleep for 2 minutes to let our children squabble          */
  sleep(60);

  /* KILL THEM ALL                                             */
  for (i = 1; i < numProcs; i++){
    kill(childs[i], SIGTERM);
  }

  return 0;
}
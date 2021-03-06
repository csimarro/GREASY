#include "basicengine.h"
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

BasicEngine::BasicEngine ( const string& filename) : AbstractSchedulerEngine(filename){
  
  engineType="basic";
  gethostname(hostname, sizeof(hostname));
  
}

void BasicEngine::run() {
  
  log->record(GreasyLog::devel, "BasicEngine::run", "Entering...");
  
  runScheduler();

  log->record(GreasyLog::devel, "BasicEngine::run", "Exiting...");

}

void BasicEngine::allocate(GreasyTask* task) {
  
  int worker;
  
  log->record(GreasyLog::devel, "BasicEngine::allocate", "Entering...");
  
  log->record(GreasyLog::info,  "Allocating task " + toString(task->getTaskId()));
  
  worker = freeWorkers.front();
  freeWorkers.pop();
  taskAssignation[worker] = task->getTaskId();
  
  log->record(GreasyLog::debug, "BasicEngine::allocate", "Sending task " + toString(task->getTaskId()) + " to worker " + toString(worker));
  
  pid_t pid = fork();
  
  if (pid == 0) {
   //Child process will exec command...
   // We use system instead of exec because of greater compatibility with command to be executed
   exit(system(task->getCommand().c_str()));  
  } else if (pid > 0) {
    // parent
    pidToWorker[pid] = worker;
    task->setTaskState(GreasyTask::running);
    workerTimers[worker].reset();
    workerTimers[worker].start();
    
  } else {
   //error 
   log->record(GreasyLog::error,  "Could not execute a new process");
   task->setTaskState(GreasyTask::failed);
   task->setReturnCode(-1);
   freeWorkers.push(worker);
  }
  
  log->record(GreasyLog::devel, "BasicEngine::allocate", "Exiting...");
  
}

void BasicEngine::waitForAnyWorker() {
  
  int retcode = -1;
  int worker;
  pid_t pid;
  int status;
  int maxRetries=0;
  GreasyTask* task = NULL;
  
  log->record(GreasyLog::devel, "BasicEngine::waitForAnyWorker", "Entering...");
  
  if (config->keyExists("MaxRetries")) fromString(maxRetries, config->getValue("MaxRetries"));
  
  log->record(GreasyLog::debug,  "Waiting for any task to complete...");
  pid = wait(&status);

  // Get the return code
  if (WIFEXITED(status)) retcode = WEXITSTATUS(status);

  worker = pidToWorker[pid];
    
  // Push worker to the free workers queue again
  freeWorkers.push(worker);
  workerTimers[worker].stop();
  
  // Update task info
  task = taskMap[taskAssignation[worker]];
  task->setReturnCode(retcode);
  task->setElapsedTime(workerTimers[worker].secsElapsed());
  task->setHostname(string(hostname));

  taskEpilogue(task);
  
  log->record(GreasyLog::devel, "BasicEngine::waitForAnyWorker", "Exiting...");
  
}

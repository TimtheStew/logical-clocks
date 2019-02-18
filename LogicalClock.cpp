#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <iostream>
#include <queue>
#include "LogicalClock.h"


/* LogicalClock constructor                         */
LogicalClock::LogicalClock(int procId, int debugBool)
{
  this -> debugIsOn = debugBool;
  this -> procId = procId;
  this -> lTime = procId;
}

/* just returns the time, plain and simple          */
int LogicalClock::getTime()
{
  return this -> lTime;
}

/* This function updates the internal clock, as well */
/* as sets the time for the event properly, allowing */
/* it to be logged if need be. It then returns the   */
/* current lTime                                     */
int LogicalClock::updateTime(Event oe)
{ 
  Event e = oe;
  /* Recieves are special in that we check to see if */
  /* the sender's lTime is greater than ours         */
  if (e.type == EvType::RECIEVE)
  {
    if (e.lTime > this -> lTime)
    {
      /* If it's greater set ours to it              */
      this -> lTime = e.lTime;
    }
  }

  /* then we just increment and set the event's time */
  /* to be our new time                              */
  this -> lTime++;
  e.lTime = this -> lTime;

  /* Print if we should be printing                  */
  if (debugIsOn) 
  {
    printTime(e);
  }

  return this -> lTime;
}
/* Used locally to print debug info if debug is on   */
int LogicalClock::printTime(Event e)
{
  /* get the physical local time                     */
  struct timeval tv;
  struct timezone tz;
  struct tm *tm;
  gettimeofday(&tv, &tz);
  tm=localtime(&tv.tv_sec);
  sprintf (timeBuf, "%02d:%02d:%02d:%03d",tm->tm_hour, 
    tm->tm_min, tm->tm_sec, (tv.tv_usec/1000) );
  std::cout << timeBuf << " proc: " << procId << " event: " << EvNames[e.type] << " " << EvSubNames[e.subType] << " at lTime: " << e.lTime << "\n";
  if (e.type == EvType::LOCAL && e.subType == EvSubtype::ACQUIRE){
    std::cout << "proc: " << procId << " HAS THE RESOURCE! ------------------------ \n";
  }
  if (e.type == EvType::LOCAL && e.subType == EvSubtype::RELEASE){
    std::cout << "proc: " << procId << " RELEASED THE RESOURCE! ------------------- \n";
  }
  return 0;
}
/*
int main(int argc, char *argv[]){
  LogicalClock *lClock = new LogicalClock(1, 1);
  Event e;
  e.type = EvType::SEND;
  e.subType = EvSubtype::RELEASE;
  e.lTime = 1;
  std::priority_queue<Event> req_q;
  req_q.push(e);
  Event e2;
  e2.type = EvType::LOCAL;
  e2.subType = EvSubtype::RELEASE;
  e2.lTime = 2;
  Event e3;
  e3.type = EvType::RECIEVE;
  e3.subType = EvSubtype::RELEASE;
  e3.lTime = 3;
  req_q.push(e3);
  std::cout << lClock->EvNames[req_q.top().type] << "\n";
}
*/
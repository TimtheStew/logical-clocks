/* localtime example */
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <iostream>

int main ()
{
  char timeBuf  [256]; 
  struct timeval tv;
  struct timezone tz;
  struct tm *tm;
  gettimeofday(&tv, &tz);
  tm=localtime(&tv.tv_sec);
  sprintf (timeBuf, "%02d:%02d:%02d:%03d",tm->tm_hour, tm->tm_min, tm->tm_sec, (tv.tv_usec/1000) );
  std::cout << timeBuf;
}
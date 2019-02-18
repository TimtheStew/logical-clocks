#ifndef LOGICALCLOCK_H
#define LOGICALCLOCK_H 

/* we assign initial values to these enum's so that we can     */
/* more easily print their string values in printTime()        */
/* general type of event                                       */
enum EvType { SEND=0, RECIEVE, LOCAL };
/* event subtype                                               */
enum EvSubtype { RELEASE=0, ACKNOWLEDGE, DUMMY,
                CONNECT, ACQUIRE, REQUEST };

/* for representing events                                     */
struct Event
{
  EvType type;
  EvSubtype subType;
  /* this is the time of the event, if the event is            */
  /* a recieve, then it's the time it was sent by the sender   */
  int lTime;
  int procId;

  /* here we override the < operator, effectively replacing it */
  /* with a >, so that our priority queue sorts them 'backward'*/
  bool operator<(const Event &e) const
  {
    return lTime > e.lTime;
  }
};


class LogicalClock
{
  private:
  int procId;
  int lTime;
  int debugIsOn;
  int printTime(Event e);
  char timeBuf [256]; 

  public:
  LogicalClock(int procId, int debugBool);
  int getTime();
  int updateTime(Event e);
  char const *EvNames[10] = 
  {
    "SEND",
    "RECIEVE",
    "LOCAL"
  };
  char const *EvSubNames[10] = 
  {
    "RELEASE",
    "ACKNOWLEDGE",
    "DUMMY",
    "CONNECT",
    "ACQUIRE",
    "REQUEST"
  };
};
#endif
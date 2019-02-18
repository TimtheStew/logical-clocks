# Logical Clock Mutual Exclusion

An implementation of Lamports Logical Clocks, used to keep a resource pool
live and safe. 

# Log

## 5/03/2018 - 7:39am
- Things are looking up. Now each process gets the resource once, then 
they all hang and crash, but the important part is they never think they
have it when they don't?

## 5/03/2018 - 6:00am
- Getting desperate now, decided to switch back to seperate input and output
sockets. Will only poll for input, and honestly that makes the error handling
so much easier. I thought I had read the relevant man pages several times 
over, but I learned there's always something more >.>

## 4/30/2018 - 2:26pm
- Looks like I'm going to need an extension. My recv's keep failing, even
though my select sets the POLLIN bits of revents, and my connects() have
been verified via setsockopt() after poll() sets the POLLOUT bits of revents.


## 4/29/2018 - 9:58pm
- I knew something about what I was planning didn't quite make sense, but I 
only just realized that if I have every process connect to every other, 
they'd each have two connections. Really, they each only need to connect to
all the processes ahead of themselves (ID-wise), so the first proc will 
call connect() to all the other, and the nth's will just fill in it's first
n-1 fds[] spots as the connects from lower ID procs come in. We're gonna include
the proc ID in every message anyway, so the i in fds[i] can be diff. from 
the proc ID it represents. Until everyone has connected we can ignore all the 
non listening sockets with our poll calls. Then once any process can talk to 
all the others it can begin normal operation. (which requires broadcasting, 
the reason we need it.)

## 4/27/2018 - 5:14pm
- Started implementing the Logical Clocks. The way I've got it working is
when you want to update the time, you make an event corresponding the 
type of event you want (if it's recieve, the lTime of the event should 
be the lTime it was sent according to the sending proc, for non-recieve 
events it can be whatever) then when you pass that event to the function
updateTime(Event e) it will ensure the time gets set properly (for things
such as logging) before it returns, and also call the printing if debug is 
on. 

- The main thing left to do with the sockets is connect to each of the other 
processes, which I'm still kind of tryna wrap my head around, but I think I 
got it. 

- Have to stop working on this now though, have some capstone documents to 
update before midnight

## 4/27/2018 - 12:45pm
- Just taking these notes here cause I've already got the file open
- Things that are going to be on the final - 7th @10:30am TTL 401b
  - READ THE RAFT PAPER READ THE RAFT PAPER READ THE RAFT PAPER
  - One sheet of 8.5 x 11 double sided cheat sheet
  - Multicasting & timeouts to detect failure
  - CAP (consistency, Availability, Partitioning)
  - Distributed Commit Protocols
    - 2PC 2PL - why, how, recovery, tolerance
  - Data Centric Consistency Models (def)
    - Strict, Sequential, Causal, Eventual
  - Distributed Consensus 
    - Paxos
    - RAFT (several questions)
      - explain logs
    - SAFETY, LIVENESS
  - Clocks
    - Physical Clocks
      - Clock drift, skew
      - Berkeley, Cristians Algo.
  - Logical Clocks
    - Lamport & Vector Clocks
  - Distributed Mutual Exclusion (def)
    - Token Ring
    - Centralized Lock server from HW #1
    - Shared Priority Queue (Ricart / Agrowala extension)

## 4/26/2018 - 7:46pm
- Spent a couple hours reading about poll(), send() and recv() found an IBM 
example of poll() that really helped clear things up. Adapting it to use to
connect my processes.

- I'm probably going to need each process to wait a second or something after 
creating it's listening socket, to ensure they've each made them by the time
they start trying to connect() to each other. 

- I want my main loop to look like:
    ```
    while(not end_server)
    {
    if (imholdingresource)
    {   
        create local event
        updateTime(local event)
        wait 2-3 seconds
        create send-release event
        updateTime(send-release event)
        send releases (with getTime())
    }    
    else // im not holding it
    {
      if (I have a current outstanding request)
      { 
        poll()
        if (it's the listening socket)
        {
          hook em up in fds
          continue
        }
        Event e = createEvent(msg i just got)
        if (the event is a request)
        {
          add it to the queue
          updateTime(e)
          send an acknowledgement
        }
        if (the event is a release)
        {
          updateTime(e)
          check to make sure the sender is at the front of our Q
          pop our Q
        }
        if (I'm at front of Q and my request has been recv'd by all)
        {
          imholdingresource = TRUE
        }
      }
      else // I'm not waiting on a request
      { 
        create a send-request event
        updateTime(send-request event)
        sendRequest
      }
    }
    }
    ```

    - In terms of Events, there are 3 main kinds, sends, recieves, and 
    locals. There are 2 kinds of sends (release and acknowledge) and 
    3 kinds of recieves (release, acknowledge, and connect). Local 
    events consist of I guess anything I want, probably just the holding
    onto the resource thing. Each event has an associated time.

## 4/16/2018 - 1:35pm

- Found some C/C++ socket boilerplate, rings bells from Operating Systems
Going to try to turn it into my own little message passing library.
    - Need to implement a broadcast? Probably don't want to artifically 
    shove all the traffic through one place though. Looks like what I want
    to do is something with Select(). Then each client can connect to all
    the others, and broadcast that way. It seems like with select you have
    loop over the list and check to see which one you've been sent stuff 
    on, but at this scale I don't think that would be too big of a deal.
    - So I'll start each client with an ID (from 1 to NUM_CLIENTS) and
    tell them each what NUM_CLIENTS is, and they should pick the same 
    high port number, like 8000, and grab them sequentially from there, 
    then connect to each other and manage using select();
    - I guess really each client should have, in addition to an instance
    of LamportClock, an instance of something that abstracts the socket
    stuff to a send(CLIENT_ID, message) or broadcast(message) level.
    - so that a resource request could look like:
        ```
        request(Resource res){
            lampTime_t lTime = clock.getTime();
            broadcast(MSG_REQ, res, lTime);
            clock.updateTime(E);
            // wait for each acknowldgement response? 
        }
        ```
    - Ran into trouble there with waiting for requests. Maybe I'll make 
    a Request Struct with spots to mark the acknowldgements from each
    other Client? And then whenever I want to request I could make a new
    one and pass it around through the necesarry calls, and then just 
    update it as I recieve the acknowldgements (or other messages with
    higher clock times)
    - Though looking at the description, maybe I want a class for Event, 
    and some subclasses for each kind, one of which could be Request? 

- Figure they always tell me to, so I should start by writing a tester.
The tester should:
    - Create a pool of client threads
    - Check to make sure they can all communicate
    - Have them all wait semi-random intervals and then request the
    resource, and then when they get it, wait some time and then release.
    - Either do this enough they definitely will, or ensure their requests
    overlap.
    - Make sure not more than one client is holding the resource at any
    given time.
    - At the end, printTime for each Client, and then analyze them to check
    for efficiency, using local clocks we could measure average wait and 
    things in real time units.
    - since all clients are local, we can print local actual times along
    the debug value about waiting for or holding each resource, and then
    check for overlaps there. 

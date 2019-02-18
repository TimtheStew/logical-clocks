/* Adapted from an example found at: 
 * https://www.ibm.com/support/knowledgecenter/en/ssw_ibm_i_71/rzab6/poll.htm
 * ( I also left in all their explanatory comments )
 * ( and added my own in their style, I like it )
 * 
 * Timothy Stewart - CIS 4307
 * 
 * This server acts as the main loop of each of process. It starts knowing 
 * how many processes there are going to be, and which one it is (ID), which
 * also determines its port (BASE_PORT + ID). It creates a reusable socket to
 * listen on at that port, waits a couple seconds so that each of it's siblings 
 * will have done the same, then trys to connect() to each of them, then 
 * it commences normal operation, which consists of holding the resource for 
 * a couple seconds, or if it doesnt have it, requesting it. While also 
 * responding to all other messages sent to it by it's siblings and 
 * updating it's lamport clock appropriately.
 * 
 * */


#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <queue>
#include <arpa/inet.h>
#include <iostream>
#include <sstream>
#include <fcntl.h>
#include <ctime>
#include <cstdlib>
#include "LogicalClock.h"

#define BASE_PORT    12000
#define MAX_PROC        10

#define TRUE             1
#define FALSE            0

int main (int argc, char *argv[])
{ 
  int    len, rc, on = 1;
  int    listen_sd = -1, new_sd = -1;
  int    desc_ready, end_server = FALSE, compress_array = FALSE;
  int    close_conn;
  char   buffer[16];
  struct sockaddr_in6   addr;
  int    timeout;
  struct pollfd fds[MAX_PROC+1];
  int    outs[MAX_PROC+1];
  int    curr_req_list[MAX_PROC+1];
  int    nfds, current_size = 0, i, j, k, l, m;
  int    not_done = TRUE, have_resource = FALSE, not_all_connected = TRUE;
  int    num_accepted = 0, req_active = FALSE, req_good = FALSE;
  int    port, t, st;
  struct Event e, curr_req;

  if (argc < 4)
  {
    perror("missing arguments - [ID][numProcs][debug]");
    exit(-1);
  }
  /* grab our ID and the number of processes from our args     */
  int id = atoi(argv[1]);
  int numProcs = atoi(argv[2]);
  int debug = atoi(argv[3]);

  if (numProcs >= MAX_PROC)
  {
    perror("max proc num 199!");
    exit(-1);
  }

  nfds = numProcs + 1;
  port = BASE_PORT + id;

  /* initialize our Lamport Clock and Request Priority Queue   */
  LogicalClock *lClock = new LogicalClock(id, debug);
  std::priority_queue<Event> req_q;


  // set them all to 1 initially
  memset(curr_req_list, 0, sizeof(curr_req));


  /* Create an AF_INET6 stream socket to receive incoming      */
  /* connections on                                            */
  listen_sd = socket(AF_INET6, SOCK_STREAM, 0);
  if (listen_sd < 0)
  {
    perror("socket() failed");
    exit(-1);
  }
  printf("proc %d just listen()'d\n", id);

  /* Allow socket descriptor to be reuseable                   */
  rc = setsockopt(listen_sd, SOL_SOCKET,  SO_REUSEADDR,
                  (char *)&on, sizeof(on));
  if (rc < 0)
  {
    perror("setsockopt() failed");
    close(listen_sd);
    exit(-1);
  }

  /* Set socket to be nonblocking. All of the sockets for      */
  /* the incoming connections will also be nonblocking since   */
  /* they will inherit that state from the listening socket.   */
  rc = fcntl(listen_sd, F_SETFL, fcntl(listen_sd, F_GETFL, 0) | O_NONBLOCK);
  if (rc < 0)
  {
    perror("ioctl() failed");
    close(listen_sd);
    exit(-1);
  }

  /* Bind the socket                                           */
  memset(&addr, 0, sizeof(addr));
  addr.sin6_family      = AF_INET6;
  memcpy(&addr.sin6_addr, &in6addr_any, sizeof(in6addr_any));
  addr.sin6_port        = htons(BASE_PORT + id );
  rc = bind(listen_sd,
            (struct sockaddr *)&addr, sizeof(addr));
  if (rc < 0)
  {
    perror("bind() failed");
    close(listen_sd);
    exit(-1);
  }

  /* Initialize the pollfd and outs structure                  */
  memset(fds, 0 , sizeof(fds));
  memset(outs, 0, sizeof(outs));

  /* Set the listen back log                                   */
  rc = listen(listen_sd, numProcs+1);
  if (rc < 0)
  {
    perror("listen() failed");
    close(listen_sd);
    exit(-1);
  }

  /* Sleep, to ensure each of our sibling has also reached     */
  /* this point, and avoid having non-blocking connect()s fail */
  /* because that's a total pain in the ass                    */
  sleep(3);

  /* Set up the initial listening socket                       */
  fds[0].fd = listen_sd;
  fds[0].events = POLLIN;

  /* connect() to everyone                                     */
  for( k = 1; k <= numProcs; k++ ){
      if (k != id)
      {
      /* make a socket                                         */
      new_sd = socket(AF_INET6, SOCK_STREAM, 0);
      if (new_sd < 0)
      {
        perror("socket() failed");
        end_server = true;
      }
      /* make a socket non-blocking                            */
      /*
      rc = fcntl(new_sd, F_SETFL, fcntl(new_sd, F_GETFL, 0) | O_NONBLOCK);
      if (rc < 0)
      {
        perror("ioctl() failed");
        close(new_sd);
        end_server = true;
      }
      */
      /* The adress to connect to                              */
      struct sockaddr_in6 address;
      memset(&address, 0, sizeof(address));
      address.sin6_family = AF_INET6;
      address.sin6_port = htons(BASE_PORT+k);
      rc = inet_pton(AF_INET6, "::1", &address.sin6_addr);
      if (rc < 0)
      {
        perror("inet_pton() failed");
        end_server = TRUE;
      }

      printf("proc %d about to connect() to %d\n", id, k);
      rc = connect(new_sd, (struct sockaddr *)&address, sizeof(address));
      if (rc == -1)
      {
        if (errno != EINPROGRESS){
          printf("proc %d connect() error rc = %d\n", id, rc);
          perror("connect() failed");
          end_server = TRUE;
        }
      }
      outs[k] = new_sd;
    }
  }

  /* Initialize the timeout to 3 minutes. If no                */
  /* activity after 3 minutes this program will end.           */
  /* timeout value is based on milliseconds.                   */
  timeout = (30 * 1000);
  srand(time(0) + id);
  /* Loop waiting for incoming connects or for incoming data   */
  /* on any of the connected sockets.                          */
  do
  { 
    int randomval = rand() % 4;
    for (m = 0; m < randomval; m ++)
    {
      e.lTime = lClock->getTime();
      e.type = EvType::LOCAL;
      e.subType = EvSubtype::DUMMY;
      lClock->updateTime(e);
    }
    /* If we have the resource, we make a local release event, */
    /* then a send release event, then broadcast that event to */
    /* all other processes, otherwise we poll() and respond    */
    if (have_resource)
    {
      int randomval = rand() % 4;
      for (m = 0; m < randomval; m ++)
      {
        e.lTime = lClock->getTime();
        e.type = EvType::LOCAL;
        e.subType = EvSubtype::DUMMY;
        lClock->updateTime(e);
      }
      e.lTime = lClock->getTime();
      e.type = EvType::LOCAL;
      e.subType = EvSubtype::RELEASE;
      lClock->updateTime(e);
      sleep(2);
      e.lTime = lClock->getTime();
      e.type = EvType::SEND;
      e.subType = EvSubtype::RELEASE;
      lClock->updateTime(e);
      for (m = 1; m < numProcs; m++)
      {
        if (m != id)
        {
          /* Broadcast the release                             */
          memset(buffer, 0, sizeof(buffer));
          snprintf(buffer, sizeof(buffer), "%d %d %d", 
                    id, lClock->getTime(), EvSubtype::RELEASE);
          rc = send(outs[m], buffer, sizeof(buffer), 0);
          if (rc < 0)
          {
            printf("proc %d send RELEASE to fds[%d] failed\n", id, m);
            perror("send() RELEASE failed");
            end_server = TRUE;
          }
        }
      }
      /* We don't have the resource, or an active request      */
      have_resource = FALSE;
      req_active = FALSE;
    }
    else if ((num_accepted == numProcs - 1) && not_done)
    {
      printf("proc %d about to do their first request\n", id);
      e.lTime = lClock->getTime();
      e.type = EvType::SEND;
      e.subType = EvSubtype::REQUEST;
      e.procId = id;
      lClock->updateTime(e);
      memset(curr_req_list, 0, sizeof(curr_req_list));
      req_active = TRUE;
      curr_req = e;
      req_q.push(curr_req);
      curr_req_list[id] = TRUE;
      memset(buffer, 0, sizeof(buffer));
      snprintf(buffer, sizeof(buffer), "%d %d %d", 
                id, lClock->getTime(), EvSubtype::REQUEST);
      for (m = 1; m < numProcs; m++)
      {
        if (m != id)
        {
          rc = send(outs[m], buffer, sizeof(buffer), 0);
          if (rc < 0)
          {
            printf("proc %d failed to send REQUEST to proc %d \n", id, m);
            perror("send() RELEASE failed");
            end_server = TRUE;
          }
        }
      }
      not_done = FALSE;
    } 
    /* poll(), loop, and respond accordingly                   */
    else 
    { 
      if (num_accepted < numProcs - 1){
        printf("proc %d not all accepted\n", id);
      } else {
        printf("proc %d all accepted\n", id);
      }

      /* Call poll() and wait 30 seconds for it to complete.   */
      printf("proc %d waiting on poll()...\n", id);
      rc = poll(fds, nfds, timeout);
      printf("proc %d got past poll() \n", id);

      /* Check to see if the poll call failed.                   */
      if (rc < 0)
      {
        printf("proc %d poll() failed\n", id);
        perror("  poll() failed");
        break;
      }

      /* Check to see if the 3 minute time out expired.          */
      if (rc == 0)
      {
        printf("  poll() timed out.  End program.\n");
        break;
      }

      /* One or more descriptors are readable.  Need to          */
      /* determine which ones they are.                          */
      current_size = nfds;
      for (i = 0; i < current_size; i++)
      {
        /* Loop through to find the descriptors that returned    */
        /* POLLIN and determine whether it's the listening       */
        /* or the active connection.                             */
        if(fds[i].revents == 0)
          continue;

        /* If revents is not POLLIN, it's an unexpected result,  */
        /* log and end the server.                               */
        /*
        if(fds[i].revents != POLLIN )
        {
          printf("  Error! revents = %d\n", fds[i].revents);
          end_server = TRUE;
          break;
        }
        */

        if (fds[i].fd == listen_sd)
        {
          /* Listening descriptor is readable.                   */
          printf("proc %d Listening socket is readable\n", id);

          /* Accept all incoming connections that are            */
          /* queued up on the listening socket before we         */
          /* loop back and call poll again.                      */
          do
          {
            /* Accept each incoming connection. If               */
            /* accept fails with EWOULDBLOCK, then we            */
            /* have accepted all of them. Any other              */
            /* failure on accept will cause us to end the        */
            /* server.                                           */
            new_sd = accept(listen_sd, NULL, NULL);
            if (new_sd < 0)
            {
              if (errno != EWOULDBLOCK)
              {
                perror("  accept() failed");
                end_server = TRUE;
              }
              break;
            }

            /* Add the new incoming connection to the            */
            /* pollfd structure                                  */
            printf("proc %d New incoming connection at fds[%d]- %d\n", id, num_accepted+1, new_sd);
            fds[num_accepted+1].fd = new_sd;
            fds[num_accepted+1].events = POLLIN;
            num_accepted++;

            /* Loop back up and accept another incoming          */
            /* connection                                        */
          } while (new_sd != -1);
        }

        /* This is not the listening socket, therefore an        */
        /* existing connection must be readable                  */
        else
        {
          if (req_active)
          {
            printf("proc %d fds[%d] is readable\n", id, i);
            close_conn = FALSE;
            /* Receive all incoming data on this socket            */
            /* before we loop back and call poll again.            */
            do
            {
              /* Receive data on this connection until the         */
              /* recv fails with EWOULDBLOCK. If any other         */
              /* failure occurs, we will close the                 */
              /* connection.                                       */
              if (fds[i].revents & POLLOUT)
              {
                printf("proc %d got a pollout\n", id);
                socklen_t len = sizeof(errno);
                getsockopt(fds[i].fd, SOL_SOCKET, SO_ERROR, &errno, &len);
                if (errno == 0)
                {
                  printf("proc %d passed getSockOpt test \n", id);
                }
                else
                {
                  end_server = TRUE;
                } 
                break;
              }

              
              printf("proc %d about to call recv() on fds[%d]\n", id, i);
              rc = recv(fds[i].fd, buffer, sizeof(buffer), 0);
              if (rc < 0)
              {
                //if (errno != EWOULDBLOCK && errno != EAGAIN)
                if (errno != EWOULDBLOCK)
                {
                  printf("proc %d recv failed() errno: %d\n", id, errno);
                  perror("  recv() failed");
                  close_conn = TRUE;
                }
                break;
              }
              /* Check to see if the connection has been           */
              /* closed by the client                              */
              if (rc == 0)
              {
                printf("  Connection closed\n");
                close_conn = TRUE;
                break;
              }
              if (rc != -1)
              {
                /* Data was received                                 */
                len = rc;
                printf("proc %d - %d bytes received\n", id, len);

                std::string message(buffer);
                std::istringstream iss(message);
                std::string word;
                e.type = EvType::RECIEVE;

                iss >> word;
                std::stringstream ss(word);
                ss >> e.procId;
                iss >> word;
                std::stringstream ss2(word);
                ss2 >> e.lTime;
                iss >> word;
                std::stringstream ss3(word);
                ss3 >> st;
                e.subType = static_cast<EvSubtype>(st);

                memset(buffer, 0, sizeof(buffer));

                if (e.lTime > curr_req.lTime)
                {
                  curr_req_list[e.procId] = TRUE;
                }
                
                lClock->updateTime(e);
                if (e.subType == EvSubtype::REQUEST)
                {
                  Event new_req = e;
                  //new_req.lTime = lClock->getTime();
                  req_q.push(new_req);
                  snprintf(buffer, sizeof(buffer), "%d %d %d", 
                    id, lClock->getTime(), EvSubtype::ACKNOWLEDGE);
                  rc = send(outs[e.procId], buffer, sizeof(buffer), 0);
                  if (rc < 0)
                  {
                    printf("proc %d failed to send ACKNOWLEDGE to proc %d \n", id, e.procId);
                    perror("send() ACKNOWLEDGE failed");
                    end_server = TRUE;
                    break;
                  }
                }
                else if (e.subType == EvSubtype::RELEASE)
                {
                  req_q.pop();
                }
                
                req_good = TRUE;
                for (m = 1; m <= numProcs; m++)
                {
                  if(! curr_req_list[m])
                  {
                    req_good = FALSE;
                  }
                }
                if (req_good && (req_q.top().procId == id))
                {
                  req_q.pop();
                  e.lTime = lClock->getTime();
                  e.type = EvType::LOCAL;
                  e.subType = EvSubtype::ACQUIRE;
                  lClock->updateTime(e);
                  have_resource = TRUE;
                  req_active = FALSE;
                }
              }
              int randomval = rand() % 4;
              for (m = 0; m < randomval; m ++)
              {
                e.lTime = lClock->getTime();
                e.type = EvType::LOCAL;
                e.subType = EvSubtype::DUMMY;
                lClock->updateTime(e);
              }
            } while(TRUE);

            /* If the close_conn flag was turned on, we need       */
            /* to clean up this active connection. This            */
            /* clean up process includes removing the              */
            /* descriptor.                                         */
            if (close_conn)
            {
              close(fds[i].fd);
              fds[i].fd = -1;
              compress_array = TRUE;
            }
          } 
          else if (num_accepted == numProcs - 1)
          {
            e.lTime = lClock->getTime();
            e.type = EvType::SEND;
            e.subType = EvSubtype::REQUEST;
            lClock->updateTime(e);
            memset(curr_req_list, 0, sizeof(curr_req_list));
            req_active = TRUE;
            curr_req = e;
            curr_req.lTime = lClock->getTime();
            curr_req_list[id] = TRUE;
            memset(buffer, 0, sizeof(buffer));
            snprintf(buffer, sizeof(buffer), "%d %d %d", 
                      id, lClock->getTime(), EvSubtype::REQUEST);
            for (m = 1; m < numProcs; m++)
            {
              if (m != id)
              {
                rc = send(outs[m], buffer, sizeof(buffer), 0);
                if (rc < 0)
                {
                  printf("proc %d failed to send REQUEST to proc %d \n", id, m);
                  perror("send() RELEASE failed");
                  end_server = TRUE;
                }
              }
            }
          }
        }  /* End of existing connection is readable             */
      } /* End of loop through pollable descriptors              */

      /* If the compress_array flag was turned on, we need       */
      /* to squeeze together the array and decrement the number  */
      /* of file descriptors. We do not need to move back the    */
      /* events and revents fields because the events will always*/
      /* be POLLIN in this case, and revents is output.          */
      if (compress_array)
      {
        compress_array = FALSE;
        for (i = 0; i < nfds; i++)
        {
          if (fds[i].fd == -1)
          {
            for(j = i; j < nfds; j++)
            {
              fds[j].fd = fds[j+1].fd;
            }
            i--;
            nfds--;
          }
        }
      }
    }
  } while (end_server == FALSE); /* End of serving running.    */

  /* Clean up all of the sockets that are open                 */
  for (i = 0; i < nfds; i++)
  {
    if(fds[i].fd >= 0)
      close(fds[i].fd);
      fds[i].fd = -1;
  }
  for (i = 0; i < MAX_PROC+1 ; i++)
  {
    if(outs[i] >= 0)
      close(outs[i]);
      outs[i] = -1;
  }
  return 0;
}



//// yudi program

#include "goldchase.h"
#include "Map.h"
#include "Screen.h"
#include <fstream>
#include <iostream>
#include <stdlib.h>
#include <string>
#include <sys/mman.h>
#include <time.h>
#include <vector>
#include <sys/stat.h>
#include <fcntl.h>
#include <semaphore.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <mqueue.h>

using namespace std;

struct GameBoard {
  int columns;
  int rows;
  pid_t pid[5];
  unsigned char player;
  char board[0];

};
int cleanupcounter =0;
Map *mapPointer;
string mq_name[5];
/*Below are the helper functions*/
void sendMessage(GameBoard*, int);
void broadcast(GameBoard*, int);
void handle_interrupt(int);
void clean_up(int);
void read_message(int);
void winingbroadcast(GameBoard*, int);
void killsignal(GameBoard*);

mqd_t readqueue_fd; //message queue file descriptor

int counter = 0;

//bool movePlayer(GameBoard*, unsigned char, int& ,int,char,Map&, bool);
sem_t *my_sem_ptr;


int main()
{

  mq_name[0]="/yudi_player1_mq";
  mq_name[1]="/yudi_player2_mq";
  mq_name[2]="/yudi_player3_mq";
  mq_name[3]="/yudi_player4_mq";
  mq_name[4]="/yudi_player5_mq";
////////This is used for sync
    struct sigaction my_signal;
    my_signal.sa_handler=handle_interrupt;
    sigemptyset(&my_signal.sa_mask);
    my_signal.sa_flags=0;
    my_signal.sa_restorer=NULL;
    sigaction(SIGUSR1, &my_signal, NULL);
///////This is used for clean up
    struct sigaction exit_handler;
    exit_handler.sa_handler=clean_up;
    sigemptyset(&exit_handler.sa_mask);
    exit_handler.sa_flags=0;
    sigaction(SIGINT, &exit_handler, NULL);
    sigaction(SIGHUP, &exit_handler, NULL);
    sigaction(SIGTERM, &exit_handler, NULL);

///////This is used for readqueue implementation
  struct sigaction action_to_take;
  action_to_take.sa_handler=read_message;
  sigemptyset(&action_to_take.sa_mask);
  action_to_take.sa_flags=0;
  sigaction(SIGUSR2, &action_to_take, NULL);

  struct mq_attr mq_attributes;
  mq_attributes.mq_flags=0;
  mq_attributes.mq_maxmsg=10;
  mq_attributes.mq_msgsize=120;


  bool GoldFound = false;
  unsigned char currentPlayer;

  int playerPos;

  int fd;
  GameBoard *gb;
  my_sem_ptr= sem_open(
      "/TAGgoldchase",
      O_CREAT|O_EXCL|O_RDWR,
      S_IROTH| S_IWOTH| S_IRGRP| S_IWGRP| S_IRUSR| S_IWUSR,
      5
      );
  if(my_sem_ptr==SEM_FAILED)
  {
    if(errno!=EEXIST)
    {
      perror("Error in creating semaphore");
      exit(1);
    }
  }
  if(my_sem_ptr!=SEM_FAILED)  ////1st player
  {

  srand(time(NULL));
  fstream myfile;
  myfile.open("mymap.txt");
	string mapOrig="";
	string numOfGold;
	string line;
	int rows=0, columns=0;
  sem_wait(my_sem_ptr);
	if(myfile.is_open())
	{
		getline(myfile,numOfGold);

		while(getline(myfile,line))
		{
			mapOrig= mapOrig+line;
			rows++;
			if(columns == 0)
			{
				columns = line.length();
			}
		}
	}
	myfile.close();
	int numOfGold_num = atoi(numOfGold.c_str());
	int mapSize = mapOrig.length();

	const char *theMine= mapOrig.c_str();


	// create shared memory
	fd = shm_open("/yudi_shm",O_RDWR | O_CREAT|O_EXCL, S_IRUSR| S_IWUSR);
	ftruncate(fd,columns*rows);
	gb = (GameBoard*)mmap(NULL,columns*rows+sizeof(GameBoard), PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
	gb->rows = rows;
	gb->columns = columns;

  for (int z = 0 ; z<=4 ; z++)
  {
    gb->pid[z] = 0;
  }
  //Assigning first player
	 currentPlayer = G_PLR0;

  	const char* ptr=theMine;
  	char* mp=gb->board;

  	//Convert the ASCII bytes iinto bit
 	while(*ptr!='\0')
  	{
   		if(*ptr==' ')      *mp=0;
    	else if(*ptr=='*') *mp=G_WALL;
    		++ptr;
    		++mp;
  	}


		int pos=0;

    // Assign Gold AND fools GOLD
		for(int i=0;i<numOfGold_num-1;i++)
		{
			pos = rand()%mapSize;
			while(gb->board[pos]!=0)
			{
				pos = rand()%mapSize;
			}
			gb->board[pos] = G_FOOL;
		}

		while(1)
		{
			pos = rand()%mapSize;
      if(gb->board[pos] == 0)
      {
        gb->board[pos] = G_GOLD;
        break;
      }

		}

	// aSSIGN 1ST PLAYER

			playerPos = rand()%mapSize;
			while(gb->board[playerPos]!=0)
			{
				playerPos = rand()%mapSize;
			}
			gb->board[playerPos] = currentPlayer;
      gb->pid[0] = getpid();

      // readqueue implementation

      if((readqueue_fd=mq_open(mq_name[0].c_str(), O_RDONLY|O_CREAT|O_EXCL|O_NONBLOCK,
          S_IRUSR|S_IWUSR, &mq_attributes))==-1)
          {
            perror("mq_open");
            exit(1);
          }

    struct sigevent mq_notification_event;
    mq_notification_event.sigev_notify=SIGEV_SIGNAL;
    mq_notification_event.sigev_signo=SIGUSR2;
    mq_notify(readqueue_fd, &mq_notification_event);
    sem_post(my_sem_ptr);
}

else
{

  my_sem_ptr= sem_open("/TAGgoldchase",O_EXCL|O_RDWR,S_IROTH| S_IWOTH| S_IRGRP| S_IWGRP| S_IRUSR| S_IWUSR,1);
  if(my_sem_ptr==SEM_FAILED)
  {
    perror("something wrong ");
  }
  sem_wait(my_sem_ptr);
  fd = shm_open("/yudi_shm",O_RDWR |O_EXCL, S_IRUSR| S_IWUSR);
  if(fd==-1)
  {
    perror("Shared Memory ");

  }

  int second_player_rows;
  int second_player_cols;
  read(fd, &second_player_rows, sizeof(int));
  read(fd, &second_player_cols, sizeof(int));
  gb = (GameBoard*)mmap(NULL,second_player_rows*second_player_cols+sizeof(GameBoard), PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);

  sem_post(my_sem_ptr);

  if(!(gb->player & G_PLR0))
  {
    currentPlayer = G_PLR0;
    gb->pid[0] = getpid();
    if((readqueue_fd=mq_open(mq_name[0].c_str(), O_RDONLY|O_CREAT|O_EXCL|O_NONBLOCK,
        S_IRUSR|S_IWUSR, &mq_attributes))==-1)
        {
          perror("mq_open");
          exit(1);
        }

  struct sigevent mq_notification_event;
  mq_notification_event.sigev_notify=SIGEV_SIGNAL;
  mq_notification_event.sigev_signo=SIGUSR2;
  mq_notify(readqueue_fd, &mq_notification_event);
  }
  else if(!(gb->player & G_PLR1))
  {
    currentPlayer = G_PLR1;
    gb->pid[1] = getpid();
    if((readqueue_fd=mq_open(mq_name[1].c_str(), O_RDONLY|O_CREAT|O_EXCL|O_NONBLOCK,
        S_IRUSR|S_IWUSR, &mq_attributes))==-1)
        {
          perror("mq_open");
          exit(1);
        }

  struct sigevent mq_notification_event;
  mq_notification_event.sigev_notify=SIGEV_SIGNAL;
  mq_notification_event.sigev_signo=SIGUSR2;
  mq_notify(readqueue_fd, &mq_notification_event);
  }
  else if(!(gb->player & G_PLR2))
  {
    currentPlayer = G_PLR2;
    gb->pid[2] = getpid();
    if((readqueue_fd=mq_open(mq_name[2].c_str(), O_RDONLY|O_CREAT|O_EXCL|O_NONBLOCK,
        S_IRUSR|S_IWUSR, &mq_attributes))==-1)
        {
          perror("mq_open");
          exit(1);
        }

  struct sigevent mq_notification_event;
  mq_notification_event.sigev_notify=SIGEV_SIGNAL;
  mq_notification_event.sigev_signo=SIGUSR2;
  mq_notify(readqueue_fd, &mq_notification_event);
  }
  else if(!(gb->player & G_PLR3))
  {
    currentPlayer = G_PLR3;
    gb->pid[3] = getpid();
    if((readqueue_fd=mq_open(mq_name[3].c_str(), O_RDONLY|O_CREAT|O_EXCL|O_NONBLOCK,
        S_IRUSR|S_IWUSR, &mq_attributes))==-1)
        {
          perror("mq_open");
          exit(1);
        }

  struct sigevent mq_notification_event;
  mq_notification_event.sigev_notify=SIGEV_SIGNAL;
  mq_notification_event.sigev_signo=SIGUSR2;
  mq_notify(readqueue_fd, &mq_notification_event);
  }
  else if(!(gb->player & G_PLR4))
  {
    currentPlayer = G_PLR4;
    gb->pid[4] = getpid();
    if((readqueue_fd=mq_open(mq_name[4].c_str(), O_RDONLY|O_CREAT|O_EXCL|O_NONBLOCK,
        S_IRUSR|S_IWUSR, &mq_attributes))==-1)
        {
          perror("mq_open");
          exit(1);
        }

  struct sigevent mq_notification_event;
  mq_notification_event.sigev_notify=SIGEV_SIGNAL;
  mq_notification_event.sigev_signo=SIGUSR2;
  mq_notify(readqueue_fd, &mq_notification_event);
  }
  else
  {
    cout << "Sorry! Game is Full" << endl;
    return 0;
  }


  playerPos = rand()%second_player_rows*second_player_cols;
  while(gb->board[playerPos]!=0)
  {
    playerPos = rand()%second_player_rows*second_player_cols;
  }


  sem_wait(my_sem_ptr);
  gb->board[playerPos] = currentPlayer;
  sem_post(my_sem_ptr);

}/// end of else
sem_wait(my_sem_ptr);


gb->player |= currentPlayer;
Map goldMine((const unsigned char*)gb->board,gb->rows,gb->columns);
mapPointer = &goldMine;
killsignal(gb);


sem_post(my_sem_ptr);
goldMine.postNotice("This is a notice");

int a = 0;
bool flag= FALSE;

while(true && cleanupcounter == 0)
{
  a=goldMine.getKey();
  int curIndex = playerPos;
  int curCol = playerPos%gb->columns;
  int curRow = playerPos/gb->columns;
  int prevEltCol=-1,prevEltRow=-1,nextEltCol=-1,nextEltRow=-1,downEltCol=-1,downEltRow=-1,upEltCol=-1,upEltRow=-1;
  int upEltPos=-1,downEltPos=-1;

    if( a == 'h') //Left
    {
      if(counter == 1)
      {

        if(((playerPos+1)%gb->columns) == 1)
        {
          winingbroadcast(gb,currentPlayer);
          goldMine.postNotice("You have Won");
          sem_wait(my_sem_ptr);
          gb->board[playerPos] &= ~currentPlayer;
          //killsignal(gb);
          gb->player &= ~currentPlayer;
          killsignal(gb);
          sem_post(my_sem_ptr);
          break;
        }
      }

        if(gb->board[playerPos-1] != G_WALL && ((playerPos+1)%gb->columns >=0 ))
        {
          sem_wait(my_sem_ptr);
          gb->board[playerPos] &= ~currentPlayer;
          playerPos = playerPos-1;
          gb->board[playerPos] |=currentPlayer;
          sem_post(my_sem_ptr);

          goldMine.drawMap();
          killsignal(gb);
        }
      }

      else if(a == 'l') //Right
      {
        if(counter == 1)
        {
          if((playerPos +1) % gb->columns == 0)
          {
            winingbroadcast(gb,currentPlayer);
            goldMine.postNotice("You have Won");
            sem_wait(my_sem_ptr);
            gb->board[playerPos] &= ~currentPlayer;
          //  killsignal(gb);
            gb->player &= ~currentPlayer;
            killsignal(gb);
            sem_post(my_sem_ptr);
            break;

          }
        }
        if(gb->board[playerPos+1] != G_WALL && ((playerPos+1)%gb->columns <(gb->columns) ))
    		{
          sem_wait(my_sem_ptr);
          gb->board[playerPos] &= ~currentPlayer;
          playerPos = playerPos+1;
          gb->board[playerPos]|=currentPlayer;
          sem_post(my_sem_ptr);
    		}
        goldMine.drawMap();
        killsignal(gb);
      }
      else if( a == 'k') //Up
      {
        if(counter == 1)
        {
        if(playerPos-gb->columns <= 0)
         {
           winingbroadcast(gb,currentPlayer);
           goldMine.postNotice("You have Won");
           sem_wait(my_sem_ptr);
           gb->board[playerPos] &= ~currentPlayer;
           gb->player &= ~currentPlayer;
           killsignal(gb);
           sem_post(my_sem_ptr);
           break;
         }
       }


        if(playerPos-gb->columns > 0)
        {

          if(gb->board[playerPos-gb->columns] != G_WALL)
          {
            sem_wait(my_sem_ptr);
            gb->board[playerPos] &= ~currentPlayer;
            playerPos = playerPos-gb->columns;
            gb->board[playerPos] |= currentPlayer;
            sem_post(my_sem_ptr);
          }
        }
        goldMine.drawMap();
        killsignal(gb);
      }
      else if( a == 'j') // Down
      {
        if(counter == 1)
        {
          if((playerPos + gb->columns >= gb->rows * gb->columns))
          {

            winingbroadcast(gb,currentPlayer);
            goldMine.postNotice("You have Won");
            sem_wait(my_sem_ptr);
            gb->board[playerPos] &= ~currentPlayer;
            gb->player &= ~currentPlayer;
            killsignal(gb);
            sem_post(my_sem_ptr);

            break;
          }
        }

        if(gb->board[playerPos+gb->columns] != G_WALL)
        {
          if((playerPos+gb->columns < gb->rows * gb->columns))// playerPos/gbp->columns))
          {
            sem_wait(my_sem_ptr);
            gb->board[playerPos] &= ~currentPlayer;
            playerPos = playerPos+gb->columns;
            gb->board[playerPos]|=currentPlayer;
            sem_post(my_sem_ptr);
          }
        goldMine.drawMap();
        killsignal(gb);
      }
    }
    else if (a =='b')
    {
      broadcast(gb,currentPlayer);

  }// end of broadcast

    else if (a == 'm')
    {
      sendMessage(gb, currentPlayer);

    } // end of 'm'

    else if( a == 'Q') //Quit
    {
      goldMine.postNotice ("Exiting");
      sem_wait(my_sem_ptr);
      gb->board[playerPos] &= ~ currentPlayer;
      gb->player &= ~ currentPlayer;
      killsignal(gb);

      sem_post(my_sem_ptr);
      break;
    }

  if(gb->board[playerPos] & G_GOLD) //Finding real gold
   {
     counter++;
     sem_wait(my_sem_ptr);
     gb->board[playerPos] &= ~G_GOLD;
     goldMine.postNotice("You have found REAL GOLD");
     sem_post(my_sem_ptr);
   }

   if(gb->board[playerPos] & G_FOOL) // finding Fools Gold
  {
    sem_wait(my_sem_ptr);
    gb->board[playerPos] &= ~G_FOOL;
    sem_post(my_sem_ptr);
    goldMine.postNotice("OOPS!!!! FOOL'S GOLD");
  }

}
sem_wait(my_sem_ptr);

switch(currentPlayer)
{
  case G_PLR0:
    gb->pid[0] = 0;
    mq_close(readqueue_fd);
    mq_unlink(mq_name[0].c_str());
    break;

  case G_PLR1:
    gb->pid[1] = 0;
    mq_close(readqueue_fd);
    mq_unlink(mq_name[1].c_str());
    break;

  case G_PLR2:
    gb->pid[2] = 0;
    mq_close(readqueue_fd);
    mq_unlink(mq_name[2].c_str());
    break;

  case G_PLR3:
    gb->pid[3] = 0;
    mq_close(readqueue_fd);
    mq_unlink(mq_name[3].c_str());
    break;

  case G_PLR4:
    gb->pid[4] = 0;
    mq_close(readqueue_fd);
    mq_unlink(mq_name[4].c_str());
    break;
}
gb->board[playerPos] &= ~currentPlayer;
gb->player &= ~currentPlayer;
killsignal(gb);


sem_post(my_sem_ptr);

if(gb->pid[0] == 0 && gb->pid[1] == 0 && gb->pid[2] == 0 && gb->pid[3] == 0 && gb->pid[4] == 0)
{
  sem_close(my_sem_ptr);
  sem_unlink("/TAGgoldchase");
  shm_unlink("/yudi_shm");
}

return 0;
}  /// end of main


//this signal is used for sync
void handle_interrupt(int)
{
  mapPointer->drawMap();
}

//This is used for reading message implementation
void read_message(int)
{
  struct sigevent mq_notification_event;
  mq_notification_event.sigev_notify=SIGEV_SIGNAL;
  mq_notification_event.sigev_signo=SIGUSR2;
  mq_notify(readqueue_fd, &mq_notification_event);

  int err;
  char msg[121];
  memset(msg, 0, 121);
  while((err=mq_receive(readqueue_fd, msg, 120, NULL))!=-1)
  {
    mapPointer->postNotice(msg);
    memset(msg, 0, 121);
  }

  if(errno!=EAGAIN)
  {
    perror("mq_receive");
    exit(1);
  }
}

//This is to send Kill Signal
void killsignal(GameBoard* gb)
{
  for(int i =0 ; i<=4 ; i++)
  {
    if(gb->pid[i]!=0)
    kill(gb->pid[i], SIGUSR1);
  }
}

//This is used when CTRL+C is pressed
void clean_up(int currentPlayer)
{
  cleanupcounter++;
}

//This is used for broadcast messaging
void broadcast(GameBoard* gb, int currentPlayer)
{
  string msg= mapPointer->getMessage();

  switch(currentPlayer)
  {
    case G_PLR0:
      msg = "Player 1 says: " +msg;
      break;
    case G_PLR1:
      msg = "Player 2 says: " +msg;
      break;
    case G_PLR2:
      msg = "Player 3 says: " +msg;
      break;
    case G_PLR3:
      msg = "Player 4 says: " +msg;
      break;
    case G_PLR4:
      msg = "Player 5 says: " +msg;
      break;
  }

  char message_text[121];
  memset(message_text, 0, 121);
  strncpy(message_text, msg.c_str(), 120);
  for(int i = 0 ; i<=4; i++)
  {
    if(gb->pid[i]!=0 && gb->pid[i]!=getpid())
    {
    mqd_t writequeue_fd; //message queue file descriptor
    if((writequeue_fd=mq_open(mq_name[i].c_str(), O_WRONLY|O_NONBLOCK))==-1)
    {
      perror("mq_open");
      exit(1);
    }

    if(mq_send(writequeue_fd, message_text, strlen(message_text), 0)==-1)
    {
      perror("mq_send");
      exit(1);
    }
    mq_close(writequeue_fd);
  }
}//end for-loop
}
void winingbroadcast(GameBoard* gb, int currentPlayer)
{
  string msg;
  switch(currentPlayer)
  {
    case G_PLR0:
      msg = "Player 1 won";
      break;
    case G_PLR1:
      msg = "Player 2 won " ;
      break;
    case G_PLR2:
      msg = "Player 3 won" ;
      break;
    case G_PLR3:
      msg = "Player 4 won ";
      break;
    case G_PLR4:
      msg = "Player 5 won" ;
      break;
  }

  char message_text[121];
  memset(message_text, 0, 121);
  strncpy(message_text, msg.c_str(), 120);
  for(int i = 0 ; i<=4; i++)
  {
    if(gb->pid[i]!=0 && gb->pid[i]!=getpid())
    {
    mqd_t writequeue_fd;
    if((writequeue_fd=mq_open(mq_name[i].c_str(), O_WRONLY|O_NONBLOCK))==-1)
    {
      perror("mq_open");
      exit(1);
    }

    if(mq_send(writequeue_fd, message_text, strlen(message_text), 0)==-1)
    {
      perror("mq_send");
      exit(1);
    }
    mq_close(writequeue_fd);
  }
}//end for-loop

}
//This is used for sending message
void sendMessage(GameBoard* gb, int currentPlayer)
{
  unsigned char mask = 0;

  for (int i = 0; i <=4 ; i++)
  {
    if(gb->pid[i]!= 0 && gb->pid[i] != getpid())
    {
      switch(i)
      {
        case 0:
         mask |= G_PLR0;
         break;

         case 1:
         mask |= G_PLR1;
         break;

         case 2:
         mask |= G_PLR2;
         break;

         case 3:
         mask |= G_PLR3;
         break;

         case 4:
         mask |= G_PLR4;
         break;

      }
    }
  }


      unsigned char answer = mapPointer->getPlayer(mask);

      string playername;
      string msg = mapPointer->getMessage();
      switch(currentPlayer)
      {
        case G_PLR0:
          msg = "Player 1 says: " +msg;
          break;
        case G_PLR1:
          msg = "Player 2 says: " +msg;
          break;
        case G_PLR2:
          msg = "Player 3 says: " +msg;
          break;
        case G_PLR3:
          msg = "Player 4 says: " +msg;
          break;
        case G_PLR4:
          msg = "Player 5 says: " +msg;
          break;
      }

      char message_text[121];
      memset(message_text, 0, 121);
      strncpy(message_text, msg.c_str(), 120);
      if(answer == G_PLR0)
      {
        mqd_t writequeue_fd;
        if((writequeue_fd=mq_open(mq_name[0].c_str(), O_WRONLY|O_NONBLOCK))==-1)
        {
          perror("mq_open");
          exit(1);
        }

        if(mq_send(writequeue_fd, message_text, strlen(message_text), 0)==-1)
        {
          perror("mq_send");
          exit(1);
        }
        mq_close(writequeue_fd);
      }

      else if(answer == G_PLR1)
      {

        mqd_t writequeue_fd;
        if((writequeue_fd=mq_open(mq_name[1].c_str(), O_WRONLY|O_NONBLOCK))==-1)
        {
          perror("mq_open");
          exit(1);
        }

        if(mq_send(writequeue_fd, message_text, strlen(message_text), 0)==-1)
        {
          perror("mq_send");
          exit(1);
        }
        mq_close(writequeue_fd);
      }

      else if(answer == G_PLR2)
      {

        mqd_t writequeue_fd; //message queue file descriptor
        if((writequeue_fd=mq_open(mq_name[2].c_str(), O_WRONLY|O_NONBLOCK))==-1)
        {
          perror("mq_open");
          exit(1);
        }

        if(mq_send(writequeue_fd, message_text, strlen(message_text), 0)==-1)
        {
          perror("mq_send");
          exit(1);
        }
        mq_close(writequeue_fd);
      }

      else if(answer == G_PLR3)
      {

        mqd_t writequeue_fd;
        if((writequeue_fd=mq_open(mq_name[3].c_str(), O_WRONLY|O_NONBLOCK))==-1)
        {
          perror("mq_open");
          exit(1);
        }

        if(mq_send(writequeue_fd, message_text, strlen(message_text), 0)==-1)
        {
          perror("mq_send");
          exit(1);
        }
        mq_close(writequeue_fd);
      }

      else if(answer == G_PLR4)
      {

        mqd_t writequeue_fd;
        if((writequeue_fd=mq_open(mq_name[4].c_str(), O_WRONLY|O_NONBLOCK))==-1)
        {
          perror("mq_open");
          exit(1);
        }
        if(mq_send(writequeue_fd, message_text, strlen(message_text), 0)==-1)
        {
          perror("mq_send");
          exit(1);
        }
        mq_close(writequeue_fd);
      }
}


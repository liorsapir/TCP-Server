//Lior Sapir
//I.D. 304916562
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <errno.h>
#include <dirent.h>
#include <time.h>
#include "threadpool.h"


#define RFC1123FMT "%a, %d %b %Y %H:%M:%S GMT"
#define MAX_PORT 65535
#define MAX_POOL 200
#define MAX_LEN_REQ 4000
#define MAX_LISTEN 5 // for accept
#define BUFFER  127 // for time
#define BAD_REQUEST 400
#define FORBIDDEN 403
#define NOT_FOUND 404
#define INTERNAL_SERV_ERROR 500
#define NOT_SUPPORTED 501
#define FOUND 302
#define FOUND_OK 200
#define FLAG_CASE 500
#define DIR_FLAG 222 // for table
////---private methoods---////
int checkIfNumber(char *arg);// check parameters from user
char** readTokens(char *arg,int *sizeTokens, int nbytes);// parse the request
void freeTokens(char** tokens);//free the memory if error in malloc
int validProtocol(char **arg,int sizeTokens);// check validition of protocol request
int validMethood(char *arg);// check validition of methood request
DIR* validPath(DIR *dir, char *path);// check validition of path request
int buildRespond(char *buff,char* respond ,char** tokens, int sizeTokens,int cliFd);//building general responed
int writeResponedClient(char* respond, int cliFd);//write to client
void createTime(char* buff);// create time to char*, this time.
int createErrorRespone(char* respond ,char** tokens, int sizeTokens);// handle the error responed
void createErrorPathRes(char* respond,int isFound, char* path);// after valid usage build error responed
int buildTableRes(char* respond, char* path, struct stat *statfile, int cliFd);// create the dir table
int buildingBody(char *resp, char* up, char* down);// in case of errors building general error body
int tableRes(char* respond, char* path, struct stat *statfile, DIR *dir, struct dirent *pdir);// the table function
char *get_mime_type(char *name);// check what file type
int buildHeader(char* respond, int flag, char* path, int size);// building general header
int responFile(char* respond, char* path, struct stat statfile, int cliFd);// write the file to the client
int workDispatch(void* arg);// handle the thread work
int checkPermittions(char* path);//check permition for all directory

int main(int argc, char *argv[])
{
  if(argc != 4)
  {
    fprintf(stderr,"\nquan: argument not legal\n"); // in case of wrong input
    fprintf(stderr, "Usage: server <port> <pool-size>\n");
    return EXIT_FAILURE;
  }
  //server <port> <pool-size> <max-number-of-request>
  if(checkIfNumber(argv[1]) || checkIfNumber(argv[2]) || checkIfNumber(argv[3]))
  {
    fprintf(stderr,"\nnumbers: argument not legal\n"); // in case of wrong input
    fprintf(stderr, "Usage: server <port> <pool-size>\n");
    return EXIT_FAILURE;
  }

  if(atoi(argv[1]) > MAX_PORT || atoi(argv[1]) <= 0)
  {
    fprintf(stderr,"\nport: argument not legal\n"); // in case of wrong input
    fprintf(stderr, "Usage: server <port> <pool-size>\n");
    return EXIT_FAILURE;
  }
  ///////////////////////////////////////////////////////////////////////
  ////////-------end check argument from user--------------//////////////
  ///////////////////////////////////////////////////////////////////////

  int listenPort = atoi(argv[1]);// turning to numbers
  int poolSize = atoi(argv[2]);
  int numRequest = atoi(argv[3]);
  if(poolSize > MAX_POOL || poolSize < 0)
  {
    fprintf(stderr,"\npoolSize: argument not legal\n"); // in case of wrong input
    fprintf(stderr, "Usage: server <port> <pool-size>\n");
    return EXIT_FAILURE;
  }
  ///////////////////////////////////////////////////////////////////////
  ////////-------create the required threadpool------------//////////////
  ///////////////////////////////////////////////////////////////////////
  threadpool *pool;
  pool = NULL;
  pool = create_threadpool(poolSize);

  ///////////////////////////////////////////////////////////////////////
  ////////--------create a socket--------------------------//////////////
  ///////////////////////////////////////////////////////////////////////
  //socket - bind - listen
  int srvFd; // for server
  int cliFd; // for client
  int cliSize = 0;
  struct sockaddr_in srv;
  struct sockaddr_in cli;
  cliSize = sizeof(cli);
  if((srvFd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
  {
    perror("\nsocket fail\n");
    return EXIT_FAILURE;
  }
  ///////////////////////////////////////////////////////////////////////
  ////////--------------bind to port-----------------------//////////////
  ///////////////////////////////////////////////////////////////////////
  srv.sin_family = AF_INET;
  srv.sin_port = htons(listenPort);
  srv.sin_addr.s_addr = htonl(INADDR_ANY);

  if(bind(srvFd, (struct sockaddr*)&srv, sizeof(srv)) < 0 )
  {
    perror("\nbind fail\n");
    return EXIT_FAILURE;
  }

  ///////////////////////////////////////////////////////////////////////
  ////////--------------listen to connection---------------//////////////
  ///////////////////////////////////////////////////////////////////////

  if(listen(srvFd, MAX_LISTEN) < 0 )
  {
    perror("\nlisten fail\n");
    return EXIT_FAILURE;
  }

  ///////////////////////////////////////////////////////////////////////
  ////////-------------accept the connection---------------//////////////
  ///////////////////////////////////////////////////////////////////////
 // add for request later and add  loop
 int i;
 for (i = 0; i < numRequest; i++)
 {
   cliFd = accept(srvFd, NULL, NULL);//////////////////////////////lasim lev latikon haze!!!!!
   if(cliFd < 0)
   {
     perror("\naccept fail\n");
     return EXIT_FAILURE;
   }

   dispatch(pool, &workDispatch, (void*)(intptr_t)cliFd);
  ///////////////////////////////////////////////////////////////////////
  ////////-------------read from client--------------------//////////////
  ///////////////////////////////////////////////////////////////////////
  }
  destroy_threadpool(pool);
  close(srvFd);
  return EXIT_SUCCESS;
}
///////////////////////////////////////////////////////////////////////
////////-------------private methoods code---------------//////////////
///////////////////////////////////////////////////////////////////////
int checkIfNumber(char *arg)
{
  int i;
  for (i = 0; i < strlen(arg); i++)
  {
    if(!isdigit(arg[i]))
      return 1;
  }
  return 0;
}

DIR* validPath(DIR *dir, char *path) // check directory existence
{
  dir = opendir(path);
  return dir;
}

char** readTokens(char *arg, int *sizeTokens, int nbytes)//method to read the input to arrays
{
	if(arg == NULL)
    return NULL;
	char words[MAX_LEN_REQ],temp[MAX_LEN_REQ],nisoi[MAX_LEN_REQ];
  memset(words, '\0',sizeof(words));//initialze
  memset(temp, '\0',sizeof(temp));
  memset(nisoi, '\0',sizeof(nisoi));
	int i=0;
	int wordsNum = 0;
	char* word = NULL;
  char* wordnissoi = NULL;
	char** tokens = NULL;
	int wordlen=0;
  for(i= 0 ; i < nbytes; i++)//initialze
    temp[i] = arg[i];
  for(i= 0 ; i < nbytes; i++)
    words[i] = arg[i];
	word=strtok(temp,"\r\n");
  wordnissoi = strtok(word," \r\n");
	while(wordnissoi != NULL)//count words in arg
	{
		wordsNum++;
		wordnissoi=strtok(NULL," \r\n");
	}
  if(wordsNum == 0)
    return NULL;

  *sizeTokens = wordsNum; // set the quantity of words

	tokens = (char**)calloc(wordsNum+1, sizeof(char*));// +1 for end word
	if(tokens == NULL)//valid create
	{
		perror("EROR: cant malloc first token");
		return NULL;
	}

	for(i= 0 ; i<=wordsNum ; i++)
		tokens[i] = NULL;// initial the tokens

  word=strtok(words,"\r\n");
  wordnissoi = strtok(word," \r\n");
	for(i= 0 ; i<wordsNum ; i++)
  {
		wordlen = strlen(wordnissoi);	//every eort to each array
		tokens[i] = (char*)calloc(wordlen+1, sizeof(char));
		if(tokens[i] == NULL)
		{
			freeTokens(tokens);
			fprintf(stderr,"EROR: cant malloc token[%d] token",i);
			return NULL;
		}
		strcpy(tokens[i],wordnissoi);
		tokens[i][wordlen]='\0';//input to array of array
		wordnissoi=strtok(NULL," ");
	}
	return tokens;
}

void freeTokens(char** tokens)//free the memory of tokens
{
	if(tokens == NULL)
		return;
	int i=0;
	while(tokens[i] != NULL)
	{
		free(tokens[i]);
    tokens[i] = NULL;
		i++;
	}
	free(tokens);
}

int buildRespond(char *buff,char* respond ,char** tokens, int sizeTokens, int cliFd)
{
  ///////////////////////////////////////////////////////////////////////
  ////////--------create a responed to client--------------//////////////
  ///////////////////////////////////////////////////////////////////////
  ////  ---- in case of error ----////
  if(createErrorRespone(respond, tokens, sizeTokens) == EXIT_FAILURE)
    return EXIT_FAILURE;

  char path[MAX_LEN_REQ];
  memset(path, '\0', sizeof(path));
  path[0] = '.';// to start from root directory
  strcat(path, tokens[1]);
  struct stat statfile;
  if((stat(path,&statfile)) == -1)// no really file
  {
    createErrorPathRes(respond,NOT_FOUND,path);
    return EXIT_FAILURE;
  }
  else if(S_ISDIR(statfile.st_mode))// check if directory
  {
    if(checkPermittions(path) == EXIT_FAILURE)// check for all permitions
    {
      createErrorPathRes(respond,FORBIDDEN,path);
      return EXIT_FAILURE;
    }
    if(statfile.st_mode & S_IROTH)
    {
      if(path[strlen(path)-1] != '/')// in case of missing/in end of path
      {
          createErrorPathRes(respond,FOUND,path);
          return EXIT_FAILURE;
      }
      else
      {
          // serching for index
          char forIndex[strlen(path)+10];
          memset(forIndex, '\0', sizeof(forIndex));
          strcpy(forIndex,path);
          strcat(forIndex, "index.html"); // if index.htm
          struct stat statfileInd;
          if((stat(forIndex,&statfileInd)) >=0)
          {
            if(statfileInd.st_mode & S_IROTH)
            {
              //send index;
              responFile(respond, path, statfileInd,cliFd);
              return FLAG_CASE;
            }
            else
            {
              //check ppermotion on index
              createErrorPathRes(respond,FORBIDDEN,path);
              return EXIT_FAILURE;
            }
          }
          else
          {
            //in case of building dir table
            if(statfile.st_mode & S_IROTH)
            {
              buildTableRes(respond,path,&statfile,cliFd);
              return DIR_FLAG;
            }
            else
            {
              createErrorPathRes(respond,FORBIDDEN,path);
              return EXIT_FAILURE;
            }
          }
        }
      }
      else
      {
        createErrorPathRes(respond,FORBIDDEN,path);
        return EXIT_FAILURE;
      }
  }
  else  //----------in case of file
  {
    if(S_ISREG(statfile.st_mode))
    {
      if(checkPermittions(path) == EXIT_FAILURE)
      {
        createErrorPathRes(respond,FORBIDDEN,path);
        return EXIT_FAILURE;
      }
      if(statfile.st_mode & S_IROTH)
      {
        if(responFile(respond, path, statfile, cliFd) == FLAG_CASE)
          return FLAG_CASE;
      }
      else
      {
        createErrorPathRes(respond,FORBIDDEN,path);
        return EXIT_FAILURE;
      }
    }
  }
}
///////////////////////////////////////////////////////////////////////
////////--------erros to write to client-----------------//////////////
///////////////////////////////////////////////////////////////////////
void createErrorPathRes(char* respond,int isFound, char* path)
{
  int size = 0;
  char temp[MAX_LEN_REQ];
  memset(temp, '\0', sizeof(temp));
  if(isFound == NOT_FOUND)
    size = buildingBody(temp,"404 Not Found","File not found.");
  else if(isFound == FOUND)
  {
        size = buildingBody(temp,"302 Found","Directories must end with a slash.");
  }
  else if(isFound == FORBIDDEN)
    size = buildingBody(temp,"403 Forbidden","Access denied.");
  else if(isFound == INTERNAL_SERV_ERROR)
    size = buildingBody(temp,"500 Internal Server Error","Some server side error.");
  char temp1[MAX_LEN_REQ];
  memset(temp1, '\0', sizeof(temp1));
  buildHeader(temp1, isFound, path, size);
  strcat(respond, temp1);/// the header
  strcat(respond, temp);/// the budy
}
///////////////////////////////////////////////////////////////////////
////////--------create a dir table-----------------------//////////////
///////////////////////////////////////////////////////////////////////
int buildTableRes(char* respond, char* path, struct stat *statfile, int cliFd)
{
  DIR *dir;
  struct dirent *pdir;
  dir = validPath(dir,path);
  if(dir == NULL)
  {
    createErrorPathRes(respond, INTERNAL_SERV_ERROR ,path);
    return EXIT_FAILURE;
  }

  int size = 0;
  //////////////////////-----------------count the files in dir---////
  int sizeOfFiles = 0;
  while((pdir = readdir(dir)) != NULL)
    sizeOfFiles++;
  rewinddir(dir);
  sizeOfFiles = (sizeOfFiles * INTERNAL_SERV_ERROR) + MAX_LEN_REQ;
  char temp[sizeOfFiles];
  memset(temp, '\0', sizeof(temp));
  size = tableRes(temp, path, statfile, dir, pdir);
  closedir(dir);
  //// -------- alloc a good size for table---------///////
  char respondTemp[sizeOfFiles + MAX_LEN_REQ];
  memset(respondTemp, '\0', sizeOfFiles + MAX_LEN_REQ);
  buildHeader(respondTemp, FOUND_OK, path, size);
  strcat(respondTemp,temp);
  int nbytes = 0;
  ////---- write the table-----/////
  if((nbytes = write(cliFd, respondTemp, sizeof(respondTemp))) < 0 )
  {
    perror("EROR3: Cant write Strem\n");
    return EXIT_FAILURE;
  }
  close(cliFd);
  return DIR_FLAG;
}
///////////////////////////////////////////////////////////////////////
////////------------the html table building--------------//////////////
///////////////////////////////////////////////////////////////////////
int tableRes(char* respond, char* path, struct stat *statfile, DIR *dir, struct dirent *pdir)
{
  int size = 0;
  strcat(respond,"<HTML>\r\n<HEAD><TITLE>Index of ");
  strcat(respond,path);
  strcat(respond,"</TITLE></HEAD>\r\n\n<BODY>\r\n<H4>Index of ");
  strcat(respond,path);
  strcat(respond,"</H4>\r\n<table CELLSPACING=8>\r\n");
  strcat(respond,"<tr><th>Name</th><th>Last Modified</th><th>Size</th></tr>\r\n");

  while((pdir = readdir(dir)) != NULL)
  {
    struct stat thisfile;
    strcat(respond,"<tr><td><A HREF=\"");
    strcat(respond,(pdir->d_name));
    strcat(respond,"\"> ");
    strcat(respond,pdir->d_name);
    strcat(respond,"</A></td><td>");
    char forPath[MAX_LEN_REQ];
    memset(forPath, '\0', sizeof(forPath));
    strcat(forPath,path);
    strcat(forPath,pdir->d_name);
    if((stat(forPath,&thisfile)) >=0)
    {
      strcat(respond,ctime(&thisfile.st_mtime));
      strcat(respond,"</td><td>");
      if(S_ISREG(thisfile.st_mode))
      {
        char forSize[FLAG_CASE];
        memset(forSize, '\0', sizeof(forSize));
        sprintf(forSize, "%d", (int)thisfile.st_size);
        strcat(respond,forSize);
      }
      strcat(respond,"</td></tr>\n");
    }
  }
  strcat(respond,"</table>\n<HR>\n<ADDRESS>webserver/1.0</ADDRESS>\n");
  strcat(respond,"</BODY></HTML>\n");
  size = strlen(respond);
  return size;
}
///////////////////////////////////////////////////////////////////////
////////--------build the header of responed-------------//////////////
///////////////////////////////////////////////////////////////////////
int buildHeader(char* respond, int flag, char* path, int size)
{
  printf("hereee2\n");
  struct stat statfilenew;
  memset(respond, '\0', sizeof(respond));
  strcat(respond,"HTTP/1.0 ");
  if( flag == FOUND_OK)
    strcat(respond,"200 OK\r\n");
  else if(flag == BAD_REQUEST)
    strcat(respond,"400 Bad Request\r\n");
  else if(flag == NOT_FOUND)
    strcat(respond,"404 Not Found\r\n");
  else if(flag == FOUND)
  {
    strcat(respond,"302 Found\r\n");
  }
  else if(flag == FORBIDDEN)
    strcat(respond,"403 Forbidden\r\n");
  else if(flag == INTERNAL_SERV_ERROR)
    strcat(respond,"500 Internal Server Error\r\n");
  else
    strcat(respond,"501 Not supported\r\n");
  strcat(respond,"Server: webserver/1.0\r\n");
  strcat(respond,"Date: ");
  char timebuf[BUFFER];
  memset(timebuf, '\0', sizeof(timebuf));
  createTime(timebuf);
  strcat(respond,timebuf);
  strcat(respond,"\r\n");
  if(flag == FOUND)
  {
    strcat(respond,"Location: ");
    (path)++;
    strcat(respond,path);
    strcat(respond,"/\r\n");
    (path)++;
  }

  char *localtype = get_mime_type(path);
  if(localtype != NULL)
  {
    strcat(respond,"Content-Type: ");
    strcat(respond,localtype);
    strcat(respond,"\r\n");
  }
  if(localtype == NULL)
  {
    if(flag == BAD_REQUEST || flag == NOT_FOUND || flag == FOUND|| flag == FORBIDDEN)
      strcat(respond,"Content-Type: text/html\r\n");
    else if(flag == INTERNAL_SERV_ERROR || flag == NOT_SUPPORTED)
      strcat(respond,"Content-Type: text/html\r\n");
  }

  char temp1[MAX_LEN_REQ];
  memset(temp1, '\0', sizeof(temp1));
  if(flag == FOUND)
    strcat(respond,"Content-Length: 123\r\n");
  else if(flag == BAD_REQUEST)
    strcat(respond,"Content-Length: 113\r\n");
  else if(flag == NOT_FOUND)
    strcat(respond,"Content-Length: 112\r\n");
  else if(flag == FORBIDDEN)
    strcat(respond,"Content-Length: 111\r\n");
  else if(flag == INTERNAL_SERV_ERROR)
    strcat(respond,"Content-Length: 144\r\n");
  else if(flag == NOT_SUPPORTED)
    strcat(respond,"Content-Length: 129\r\n");
  else
  {
    strcat(respond,"Content-Length: ");
    sprintf(temp1, "%d", size);
    strcat(respond,temp1);
    strcat(respond,"\r\n");
  }
  if(flag == FOUND_OK)
  {
    strcat(respond,"Last-Modified: ");
    if((stat(path,&statfilenew)) >= 0)
    {
        strcat(respond,ctime(&statfilenew.st_mtime));
    }
    else
      return EXIT_FAILURE;
  }
  strcat(respond, "Connection: close\r\n\r\n");
  return strlen(respond);
}

///////////////////////////////////////////////////////////////////////
////////--------building body of rresponed---------------//////////////
///////////////////////////////////////////////////////////////////////
int buildingBody(char *resp, char* up, char* down)
{
  strcat(resp,"\r\n<HTML><HEAD><TITLE>");
  strcat(resp,up);
  strcat(resp,"</TITLE></HEAD>\n");
  strcat(resp,"<BODY><H4>");
  strcat(resp,up);
  strcat(resp,"</H4>\n");
  strcat(resp,down);
  strcat(resp,"\n</BODY></HTML>\n");
  return strlen(resp)+strlen(up)+strlen(down);
}
///////////////////////////////////////////////////////////////////////
////////--------send the file to client------------------//////////////
///////////////////////////////////////////////////////////////////////
int responFile(char* respond, char* path, struct stat statfile, int cliFd )
{
  if(S_ISREG(statfile.st_mode) &&(statfile.st_mode & S_IROTH) )
  {
        int foRead,forWrite;
        char buff[MAX_LEN_REQ];
        int newfd = open(path, O_RDONLY);
        if (newfd == -1)
        {
          createErrorPathRes(respond, INTERNAL_SERV_ERROR, path);
          return EXIT_FAILURE;
        }
        char resFile[MAX_LEN_REQ];
        memset(resFile, 0, MAX_LEN_REQ);
        int size = 0;
        size = buildHeader(resFile, FOUND_OK, path, statfile.st_size);
        if((forWrite = write(cliFd, resFile, size)) < 0)
        {
          perror("EROR1: Cant write Strem\n");
          return EXIT_FAILURE;
        }
        bzero(resFile,MAX_LEN_REQ);
        int ret_in,ret_out;
        //// write all the file to client
        while((ret_in = read(newfd, resFile, MAX_LEN_REQ)) > 0)
        {
          int temp = write(cliFd, resFile, ret_in);
          if(temp < 0 )
          {
            perror("EROR: Cant read buff\n");
            return EXIT_FAILURE;
          }
        }
        if(ret_in < 0)///---in case of error
        {
          perror("EROR: Cant read buff\n");
          return EXIT_FAILURE;
        }
        close(newfd);
        close(cliFd);
        return FLAG_CASE;
  }
  else
  {
    createErrorPathRes(respond, FORBIDDEN, path);
    return EXIT_FAILURE;
  }
}

///////////////////////////////////////////////////////////////////////
////////------the handle function to responed to client--//////////////
///////////////////////////////////////////////////////////////////////
int workDispatch(void* arg)
{
  int cliFd = (intptr_t)arg;
  int nbytes;
  int place = 0;
  char buff[MAX_LEN_REQ];
  memset(buff,0,MAX_LEN_REQ);
  char buffe[MAX_LEN_REQ];
  memset(buffe,0,MAX_LEN_REQ);
  nbytes = read(cliFd, buff, sizeof(buff));
  if(nbytes < 0)
  {
    perror("\nERROR READ1: read fail\n");
    return EXIT_FAILURE;
  }
  printf("request is:\n%s\n",buff);
  int flagRespond = 0;
  int sizeTokens = 0;
  char **tokens = readTokens(buff,&sizeTokens,nbytes);
  if(tokens == NULL)
  {
    perror("\nproblem with read tokens\n");
    return EXIT_FAILURE;
  }
  ///////////////////////////////////////////////////////////////////////
  ////////-------------building the responed---------------//////////////
  ///////////////////////////////////////////////////////////////////////
  char respond[MAX_LEN_REQ];
  memset(respond, '\0', MAX_LEN_REQ);
  int jok;
  jok = buildRespond(buff, respond, tokens, sizeTokens,cliFd);

  ///////////////////////////////////////////////////////////////////////
  ////////-------------writing the responed----------------//////////////
  ///////////////////////////////////////////////////////////////////////

  freeTokens(tokens);
  if(jok != FLAG_CASE && jok != DIR_FLAG)
  {
    if((nbytes = write(cliFd, respond, sizeof(respond))) < 0 )
    {
      perror("EROR3: Cant write Strem\n");
      return EXIT_FAILURE;
    }
    close(cliFd);
  }
}
void createTime(char* timebuf)
{
  time_t now;
  now = time(NULL);
  strftime(timebuf, BUFFER, RFC1123FMT, gmtime(&now));
}
///////////////////////////////////////////////////////////////////////
////////--------create a error responed -----------------//////////////
///////////////////////////////////////////////////////////////////////
int createErrorRespone(char* respond ,char** tokens, int sizeTokens)
{
  int size = 0;
  char lolo[MAX_LEN_REQ];
  memset(lolo, '\0', sizeof(lolo));
  ///------ no size fit or protocol or path is OK-----////
  if(validProtocol(tokens,sizeTokens) == EXIT_FAILURE)
  {
        char temp[MAX_LEN_REQ];
        memset(temp, '\0', sizeof(temp));
        size = buildingBody(temp,"400 Bad Request","Bad Request.");
        buildHeader(lolo, BAD_REQUEST, tokens[1], size);
        strcat(respond,lolo);
        strcat(respond,temp);
        return EXIT_FAILURE;
  }
  /// in case of problem with methood
  if(validMethood(tokens[0]) == EXIT_FAILURE)
  {
      char temp[MAX_LEN_REQ];
      memset(temp, '\0', sizeof(temp));
      size = buildingBody(temp,"501 Not supported","Method is not supported.");
      buildHeader(lolo, NOT_SUPPORTED, tokens[1], size);
      strcat(respond,lolo);
      strcat(respond,temp);
      return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
///////////////////////////////////////////////////////////////////////
////////--------checks functions of requests-------------//////////////
///////////////////////////////////////////////////////////////////////
int validProtocol(char **arg,int sizeTokens)
{
  if(sizeTokens != 3)
  {
    fprintf(stderr,"\nrequest problem - problem arguments\n");
    return EXIT_FAILURE;
  }
  if(arg[1] == NULL || arg[1][0] != '/')
    return EXIT_FAILURE;
  char *protoc = arg[2];
  if(strcmp(protoc,"HTTP/1.0") != 0 && strcmp(protoc,"HTTP/1.1") != 0)
  {
    fprintf(stderr,"\nrequest problem - no HTTP protocol\n");
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
int validMethood(char *arg)
{
  if(strcmp(arg,"GET") != 0)
  {
    fprintf(stderr,"\nrequest problem - no GET methood\n");
    return EXIT_FAILURE;
  }
  return 0;
}
char *get_mime_type(char *name)
{
  char *ext = strrchr(name, '.');
  if (!ext) return NULL;
  if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0) return "text/html";
  if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
  if (strcmp(ext, ".gif") == 0) return "image/gif";
  if (strcmp(ext, ".png") == 0) return "image/png";
  if (strcmp(ext, ".css") == 0) return "text/css";
  if (strcmp(ext, ".au") == 0) return "audio/basic";
  if (strcmp(ext, ".wav") == 0) return "audio/wav";
  if (strcmp(ext, ".avi") == 0) return "video/x-msvideo";
  if (strcmp(ext, ".mpeg") == 0 || strcmp(ext, ".mpg") == 0) return "video/mpeg";
  if (strcmp(ext, ".mp3") == 0) return "audio/mpeg";
  return NULL;
}

int checkPermittions(char* path)
{
  char temp[MAX_LEN_REQ];
  memset(temp, '\0', sizeof(temp));
  strcpy(temp, path);
  char temp1[MAX_LEN_REQ];
  memset(temp1, '\0', sizeof(temp1));
  struct stat statop;
  int i;
  for(i = 2; i < strlen(temp); i++)
  {
    if( temp[i] == '/')
    {
      temp[i] = '\0';
      strcpy(temp1, temp);
      if(stat(temp1, &statop)>=0)
      {
        if(!(statop.st_mode & S_IROTH))
          return EXIT_FAILURE;
      }
      temp[i] = '/';
      memset(temp1, '\0', sizeof(temp1));
    }
  }
  return EXIT_SUCCESS;
}

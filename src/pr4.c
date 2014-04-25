/* CMPSC 473, Project 4, starter kit
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "partitioner.h"

/*--------------------------------------------------------------------------------*/

int debug = 0;	// extra output; 1 = on, 0 = off

/*--------------------------------------------------------------------------------*/

/* The input file (stdin) represents a sequence of file-system commands,
 * which all look like     cmd filename filesize
 *
 * command	action
 * -------	------
 *  root	initialize root directory
 *  print	print current working directory and all descendants
 *  chdir	change current working directory
 *                (.. refers to parent directory, as in Unix)
 *  mkdir	sub-directory create (mk = make)
 *  rmdir	              delete (rm = delete)
 *  mvdir	              rename (mv = move)
 *  mkfil	file create
 *  rmfil	     delete
 *  mvfil	     rename
 *  szfil	     resize (sz = size)
 *  exit        quit the program immediately
 */

/* The size argument is usually ignored.
 * The return value is 0 (success) or -1 (failure).
 */
int do_root (char *name, char *size);
int do_print(char *name, char *size);
int do_chdir(char *name, char *size);
int do_mkdir(char *name, char *size);
int do_rmdir(char *name, char *size);
int do_mvdir(char *name, char *size);
int do_mkfil(char *name, char *size);
int do_rmfil(char *name, char *size);
int do_mvfil(char *name, char *size);
int do_szfil(char *name, char *size);
int do_exit (char *name, char *size);

struct action {
  char *cmd;					// pointer to string
  int (*action)(char *name, char *size);	// pointer to function
} table[] = {
    { "root" , do_root  },
    { "print", do_print },
    { "chdir", do_chdir },
    { "mkdir", do_mkdir },
    { "rmdir", do_rmdir },
    { "mvdir", do_mvdir },
    { "mkfil", do_mkfil },
    { "rmfil", do_rmfil },
    { "mvfil", do_mvfil },
    { "szfil", do_szfil },
    { "exit" , do_exit  },
    { NULL, NULL }	// end marker, do not remove
};

typedef struct fileHeader
{
	bool isDirectory;
	block_id parent;
	unsigned int size;
	block_id contents;
  	char name[128];
  	block_id currentID;
} fileHeader;

fileHeader *currentDirectory;

/*--------------------------------------------------------------------------------*/

void parse(char *buf, int *argc, char *argv[]);

#define LINESIZE 128

/*--------------------------------------------------------------------------------*/

void testPartition() {
  // TESTING, REMOVE LATER
  initialize("./partition.data", 2000000);
  
  srand(time(NULL));

  const uint64_t num = 1973;

  block_id blk[num];

  for(int i = 0; i < num; i++) {
    blk[i] = allocate_block((rand() % 500) + 1);
  }

  for(int i = 0; i < num * 4; i++) {
    block_id one = rand() % num;
    block_id two = rand() % num;

    block_id temp = one;
    one = two;
    two = temp;
  }

  for(int i = 0; i < num/2; i++) {
    blk[i] = resize_block(blk[i], (rand() % 1024) + 1);
  }

  printInfo(stderr);

  for(int i = 0; i < num; i++) {
    free_block(blk[i]);
  }

  printInfo(stderr);


  // END OF TESTING
}

int main(int argc, char *argv[])
{

  testPartition();
  
  char in[LINESIZE];
  char *cmd, *fnm, *fsz;
  char dummy[] = "";

  int n;
  char *a[LINESIZE];

  while (fgets(in, LINESIZE, stdin) != NULL)
    {
      // commands are all like "cmd filename filesize\n" with whitespace between

      // parse in
      parse(in, &n, a);

      cmd = (n > 0) ? a[0] : dummy;
      fnm = (n > 1) ? a[1] : dummy;
      fsz = (n > 2) ? a[2] : dummy;
      if (debug) printf(":%s:%s:%s:\n", cmd, fnm, fsz);

      if (n == 0) continue;	// blank line

      int found = 0;
      for (struct action *ptr = table; ptr->cmd != NULL; ptr++)
        {
	  if (strcmp(ptr->cmd, cmd) == 0)
	    {
	      found = 1;
	      int ret = (ptr->action)(fnm, fsz);
	      if (ret == -1)
	        { printf("  %s %s %s: failed\n", cmd, fnm, fsz); }
	      break;
	    }
	}
      if (!found)
        { printf("command not found: %s\n", cmd); }
    }

  return 0;
}

/*--------------------------------------------------------------------------------*/

int do_root(char *name, char *size)
{
  if (debug) printf("%s\n", __func__);

  uint64_t numOfBytes = strtoull(size, NULL, 0);
  
  if(name == NULL)
  {
  	initialize("./partition.data", 16384);
  	numOfBytes = 16384
  }
  else
  {
  	initialize(name, numOfBytes);
  }
  
  block_id blk = allocate_block(numOfBytes + sizeof(fileHeader));
  
  fileHeader fh;
  fh.isDirectory = true;
  fh.parent = NULL;
  fh.size = numOfBytes;
  fh.contents = blk + sizeof(fileHeader);
  fh.name = name;
  
  int buf = malloc(numOfBytes);
  
  save_block(fh.contents, buf, fh.size);
  
  //Don't need that buffer anymore, we saved to disk
  free(buf);
  
  saveRootID(blk);
  
  //set root directory to be current directory
  currentDirectory = fh;

  return -1;
}

int do_print(char *name, char *size)
{
  if (debug) printf("%s\n", __func__);
  return -1;
}

int do_chdir(char *name, char *size)
{
  if (debug) printf("%s\n", __func__);
  /*
  uint64_t numOfBytes = strtoull(size, NULL, 0);
  
  //move up one level
  if(name == '..')
  {
  	//find the parent of the currentDirectory
  	block_id parentDirectory = currentDirectory.parent;
  	load_block(parentDirectory, currentDirectory, numOfBytes);
  	currentDirectory = parentDirectory;
  	
  }
  //move down one level to specified directory
  else
  {
  	//block_id childDirectory = currentDirectory.contents
  	//currentDirectory = childDirectory;
  }
  */
  return -1;
}

int do_mkdir(char *name, char *size)
{
  if (debug) printf("%s\n", __func__);
  
  uint64_t numOfBytes = strtoull(size, NULL, 0);
  
  block_id blk = allocate_block(numOfBytes + sizeof(fileHeader));
  
  fileHeader fh;
  fh.isDirectory = true;
  fh.parent = currentDirectory;
  fh.size = numOfBytes;
  fh.contents = blk + sizeof(fileHeader);
  fh.name = name;
  
  int buf = malloc(numOfBytes);
  
  save_block(fh.contents, buf, fh.size);
  
  //Don't need that buffer anymore, we saved to disk
  free(buf);
  
  //notify parent of block id
  //currentDirectory.contents
  
  currentDirectory = fh;

  return -1;
}

int do_rmdir(char *name, char *size)
{
  if (debug) printf("%s\n", __func__);
  
  /*
  // check if it's the root, if root don't remove
  //if (getRootID() ==)
  
  // need to deallocate the data that is pointed to
  fprintf(stderr, "%s: %s: removing directory...\n", __func__, name);
  // free_block();
  */
  
  return -1;
}

int do_mvdir(char *name, char *size)
{
  if (debug) printf("%s\n", __func__);
  return -1;
}

int do_mkfil(char *name, char *size)
{
  if (debug) printf("%s\n", __func__);
  
  uint64_t numOfBytes = strtoull(size, NULL, 0);
  
  block_id blk = allocate_block(numOfBytes + sizeof(fileHeader));
  
  fileHeader fh;
  fh.isDirectory = false;
  fh.parent = currentDirectory.currentID;
  fh.size = numOfBytes;
  fh.contents = blk + sizeof(fileHeader);
  fh.name = name;
  
  int buf = malloc(numOfBytes);
  
  save_block(fh.contents, buf, fh.size);
  
  //Don't need that buffer anymore, we saved to disk
  free(buf);

  return -1;
}

int do_rmfil(char *name, char *size)
{
  if (debug) printf("%s\n", __func__);
  
  /*
  // need to check if whatever is given is actually a file
  
  // need to deallocate the data that is pointed to
  fprintf(stderr, "%s: %s: removing file...\n", __func__, name);
  // free_block();
  
  */
  
  return -1;
}

int do_mvfil(char *name, char *size)
{
  if (debug) printf("%s\n", __func__);
  return -1;
}

int do_szfil(char *name, char *size)
{
  if (debug) printf("%s\n", __func__);
  
  uint64_t numOfBytes = strtoull(size, NULL, 0);
  
  //need to find file!!!
  
  //file already exists
  if()
  {
  	resize_block(currentDirectory.currentID, numOfBytes);
  }
  //file doesn't exist therefore can't resize
  else
  {
  	fprintf(stderr, "File does not exist! :'(");
  }
  
  return -1;
}

int do_exit(char *name, char *size)
{
  if (debug) printf("%s\n", __func__);
  exit(0);
  return 0;
}

/*--------------------------------------------------------------------------------*/

// parse a command line, where buf came from fgets()

// Note - the trailing '\n' in buf is whitespace, and we need it as a delimiter.

void parse(char *buf, int *argc, char *argv[])
{
  char *delim;          // points to first space delimiter
  int count = 0;        // number of args

  char whsp[] = " \t\n\v\f\r";          // whitespace characters

  while (1)                             // build the argv list
    {
      buf += strspn(buf, whsp);         // skip leading whitespace
      delim = strpbrk(buf, whsp);       // next whitespace char or NULL
      if (delim == NULL)                // end of line
        { break; }
      argv[count++] = buf;              // start argv[i]
      *delim = '\0';                    // terminate argv[i]
      buf = delim + 1;                  // start argv[i+1]?
    }
  argv[count] = NULL;

  *argc = count;

  return;
}

/*--------------------------------------------------------------------------------*/


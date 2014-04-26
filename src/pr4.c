/* CMPSC 473, Project 4, starter kit
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#include "partitioner.h"

/*--------------------------------------------------------------------------------*/

int debug = 0;	// extra output; 1 = on, 0 = off

/*--------------------------------------------------------------------------------*/

/* The input file (stdin) represents a sequence of file-system commands,
* which all look like cmd filename filesize
*
* command action
* ------- ------
* root initialize root directory
* print print current working directory and all descendants
* chdir change current working directory
* (.. refers to parent directory, as in Unix)
* mkdir sub-directory create (mk = make)
* rmdir delete (rm = delete)
* mvdir rename (mv = move)
* mkfil file create
* rmfil delete
* mvfil rename
* szfil resize (sz = size)
* exit quit the program immediately
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
  char *cmd;	// pointer to string
  int (*action)(char *name, char *size);	// pointer to function
} table[] = {
    { "root" , do_root },
    { "print", do_print },
    { "chdir", do_chdir },
    { "mkdir", do_mkdir },
    { "rmdir", do_rmdir },
    { "mvdir", do_mvdir },
    { "mkfil", do_mkfil },
    { "rmfil", do_rmfil },
    { "mvfil", do_mvfil },
    { "szfil", do_szfil },
    { "exit" , do_exit },
    { NULL, NULL }	// end marker, do not remove
};

#define MAX_FILENAME 128

typedef struct fileHeader
{
  bool isDirectory;
  block_id parent;
  block_id currentID;
  block_id contents;
  block_size_t size;
  char name[MAX_FILENAME+1];   // max filename size is 128 characters
} fileHeader;

fileHeader *currentDir;

/*--------------------------------------------------------------------------------*/

void parse(char *buf, int *argc, char *argv[]);

#define LINESIZE 128

/*--------------------------------------------------------------------------------*/

void testPartition() {
  // TESTING, REMOVE LATER
  initialize("./test_partition.data", 2000000);
  
  srand(time(NULL));

  const uint64_t num = 1973;

  block_id blk[num];

  for(unsigned int i = 0; i < num; i++) {
    blk[i] = allocate_block((rand() % 500) + 1);
  }

  for(unsigned int i = 0; i < num * 4; i++) {
    block_id one = rand() % num;
    block_id two = rand() % num;

    block_id temp = one;
    one = two;
    two = temp;
  }

  for(unsigned int i = 0; i < num/2; i++) {
    blk[i] = resize_block(blk[i], (rand() % 1024) + 1);
  }

  printInfo(stderr);

  for(unsigned int i = 0; i < num; i++) {
    free_block(blk[i]);
  }

  printInfo(stderr);


  // END OF TESTING
}

int main(int argc, char *argv[])
{  
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
{ printf(" %s %s %s: failed\n", cmd, fnm, fsz); }
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

  initialize("./partition.data", 64 * 1024 * 1024); // 64MB default partition size.

  currentDir = calloc(1, sizeof(fileHeader));

  // name left as empty string
  currentDir->isDirectory = true;
  currentDir->parent = 0;
  currentDir->size = 128 * sizeof(block_id);
  currentDir->currentID = allocate_block(currentDir->size + sizeof(fileHeader));
  currentDir->contents = currentDir->currentID + sizeof(fileHeader);

  void *initialContents = calloc(128, sizeof(block_id));

  save_block(currentDir->currentID, currentDir, sizeof(fileHeader));
  save_block(currentDir->contents, initialContents, currentDir->size);

  saveRootID(currentDir->currentID);

  free(initialContents);

  return 0;
}

char* path = NULL;
unsigned int pathLen = 0;

void printAll(fileHeader *dir) {
  if(path == NULL) {
    path = calloc(5000, sizeof(char));
    pathLen = 5000;
    path[0] = '.';
    path[1] = '/';
  }

  if(strlen(path) + strlen(dir->name) + 1 >= pathLen-1) {
    path = realloc(path, pathLen + 500);
    pathLen += 500;
  }

  int oldEnd = strlen(path);

  strcat(path, dir->name);

  printf("%s:\n", path);

  fileHeader *fh = malloc(sizeof(fileHeader));

  block_id *child_id = malloc(dir->size);
  load_block(dir->contents, child_id, dir->size);

  // first we print the files in this directory.
  bool atLeastOneFile = false;
  for(unsigned int i = 0; i < dir->size / sizeof(block_id); i++) {
    if(child_id[i] == 0) {
      continue;
    }

    load_block(child_id[i], fh, sizeof(fileHeader));

    if(fh->isDirectory) {
      // we'll print you later bro
      continue;
    }

    printf("  %s, %llu bytes\n", fh->name, fh->size);

    atLeastOneFile = true;

  }

  if(!atLeastOneFile) {
    printf("  <no files>\n");
  }

  printf("\n");

  if(dir->name[0] != '\0') {
    strcat(path, "/");
  }

  for(unsigned int i = 0; i < dir->size / sizeof(block_id); i++) {
    if(child_id[i] == 0) {
      continue;
    }

    load_block(child_id[i], fh, sizeof(fileHeader));

    if(fh->isDirectory == false) {
      // already printed!
      continue;
    }

    printAll(fh);

  }

  // pop off this part of the path from string.

  path[oldEnd] = '\0';  
  free(child_id);
  free(fh);

}

int do_print(char *name, char *size)
{
  if (debug) printf("%s\n", __func__);
  printInfo(stdout);
  printf("\n\n\t* Current Directory Information *\n\n");
  printAll(currentDir);
  return 0;
}

int do_chdir(char *name, char *size)
{
  if (debug) printf("%s\n", __func__);

  if(strcmp(name, "") == 0) {
    printf("specify a directory\n");
    return -1;
  }
  
  //move up one level
  if(strcmp(name, "..") == 0) {

    if(currentDir->parent != 0) {
      // actually move up a directory.
      load_block(currentDir->parent, currentDir, sizeof(fileHeader));
    } else {
      printf("already at root\n");
      return -1;
    }

    return 0;
  }

  // otherwise, search for the requested directory.
  block_id *info = malloc(currentDir->size);
  load_block(currentDir->contents, info, currentDir->size);

  fileHeader *temp = malloc(sizeof(fileHeader));

  for(unsigned int i = 0; i < currentDir->size / sizeof(block_id); i++) {
    if(info[i] == 0) {
      continue;
    }

    load_block(info[i], temp, sizeof(fileHeader));

    if(!(temp->isDirectory)) {
      continue;
    }

    if(strcmp(temp->name, name) == 0) {
      // we found it, change directory.
      free(currentDir);
      free(info);
      currentDir = temp;
      return 0;
    }

  }

  free(info);
  free(temp);

  // couldn't find the directory, command failed.
  printf("directory doesn't exist.\n");

  return -1;
}

int do_mkdir(char *name, char *size)
{
  if (debug) printf("%s\n", __func__);

  if(strcmp(name, "") == 0) {
    printf("specify a directory name\n");
    return -1;
  }

  if(strlen(name) > MAX_FILENAME) {
    printf("filename too long, max is 128 characters");
    return -1;
  }

  if(strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
    printf("invalid dir name: %s\n", name);
    return -1;
  }

  // search this directory for a open slot while also checking to see if
  // there's a name conflict.

  block_id *info = malloc(currentDir->size);
  load_block(currentDir->contents, info, currentDir->size);

  fileHeader *temp = malloc(sizeof(fileHeader));
  int openSlot = -1;
  unsigned int i = 0;
  for(; i < currentDir->size / sizeof(block_id); i++) {
    if(info[i] == 0) {
      if(openSlot == -1) {
        openSlot = i;
      }
      continue;
    }

    load_block(info[i], temp, sizeof(fileHeader));

    if(temp->isDirectory == false) {
      continue;
    }

    if(strcmp(temp->name, name) == 0) {
      printf("directory already exists\n");
      return -1;
    }

  }

  // at this point, there at least isn't a name conflict

  if(openSlot == -1) {
    // shit, the directory is full.

    block_id old_id = currentDir->currentID;

    currentDir->size = currentDir->size * 2;
    currentDir->currentID = resize_block(currentDir->currentID, currentDir->size + sizeof(fileHeader));
    currentDir->contents = currentDir->currentID + sizeof(fileHeader);

    info = realloc(info, currentDir->size);
    load_block(currentDir->contents, info, currentDir->size);

    // we gotta zero out the new entries, resize does not guarentee that they're zero.
    // we continue where we left off.
    for(; i < currentDir->size / sizeof(block_id); i++) {
      info[i] = 0;
      if(info[i] == 0 && openSlot == -1) {
        // trip first time.
        openSlot = i;
      }
    }

    // now there's an open slot, but if the id changed, we need to update parent and children
    // before continuing.
    if(old_id != currentDir->currentID) {
      if(currentDir->parent != 0) {
        load_block(currentDir->currentID, temp, sizeof(fileHeader));

        block_id *parContent = malloc(temp->size);
        load_block(temp->contents, parContent, temp->size);
        bool updateOccured = false;

        for(unsigned int i = 0; i < temp->size / sizeof(block_id) && !updateOccured; i++) {
          if(parContent[i] == old_id) {
            parContent[i] = currentDir->currentID;
            save_block(temp->contents, parContent, temp->size);
            updateOccured = true;
          }
        }

        if(!updateOccured) {
          printf("directory structure is corrupt.\n");
          return -1;
        }

        free(parContent);

      } else {
        // updating the parent is just saving this as the new rootID
        saveRootID(currentDir->currentID);
      }

      //childn' are gettin adopted
      for(unsigned int k = 0; k < currentDir->size / sizeof(block_id); k++) {
        if(info[k] == 0) {
          continue;
        }

        load_block(info[k], temp, sizeof(fileHeader));

        if(temp->parent != old_id) {
          printf("directory structure is corrupt.\n");
          return -1;
        }

        temp->parent = currentDir->currentID;

        save_block(info[k], temp, sizeof(fileHeader));
      }
    }
  }

  fileHeader *newDir = calloc(1, sizeof(fileHeader));

  newDir->isDirectory = true;
  newDir->parent = currentDir->currentID;
  newDir->size = 128 * sizeof(block_id);
  newDir->currentID = allocate_block(newDir->size + sizeof(fileHeader));
  newDir->contents = newDir->currentID + sizeof(fileHeader);
  strncpy(newDir->name, name, MAX_FILENAME);

  // save info and this header.

  info[openSlot] = newDir->currentID;
  save_block(currentDir->contents, info, currentDir->size);

  void *initialContents = calloc(128, sizeof(block_id));

  save_block(newDir->currentID, newDir, sizeof(fileHeader));
  save_block(newDir->contents, initialContents, newDir->size);

  free(info);
  free(temp);

  return 0;
}

void deleteDir(fileHeader *fh) {

  if(!fh->isDirectory) {
    printf("internal error: trying to deleteDir on a file!\n");
    exit(1);
  }

  fileHeader *subHead = malloc(sizeof(fileHeader));
  block_id *child = malloc(fh->size);

  load_block(fh->contents, child, fh->size);

  for(unsigned int i = 0; i < fh->size / sizeof(block_id); i++) {
    if(child[i] == 0) {
      continue;
    }

    load_block(child[i], subHead, sizeof(fileHeader));

    if(subHead->isDirectory) {

      deleteDir(subHead);

    } else {

      fileHeader *save = currentDir;
      currentDir = fh;
      do_rmfil(subHead->name, NULL);
      currentDir = save;

    }
  }

  // now to delete self.
  free_block(fh->currentID);
  free(subHead);
  free(child);

}

int do_rmdir(char *name, char *size)
{
  if (debug) printf("%s\n", __func__);

  if(strcmp(name, "") == 0) {
    printf("specify a directory\n");
    return -1;
  }

  if(strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
    printf("cannot delete %s", name);
    return -1;
  }
  
  // find dir
  block_id *info = malloc(currentDir->size);
  load_block(currentDir->contents, info, currentDir->size);

  fileHeader *temp = malloc(sizeof(fileHeader));
  bool didDelete = false;
  unsigned int i = 0;
  for(; i < currentDir->size / sizeof(block_id); i++) {
    if(info[i] == 0) {
      continue;
    }

    load_block(info[i], temp, sizeof(fileHeader));

    if(temp->isDirectory == false) {
      continue;
    }

    if(strcmp(temp->name, name) == 0) {
      deleteDir(temp);
      didDelete = true;
      break;
    }

  }

  //zero out id
  if(didDelete) {
    info[i] = 0;
    save_block(currentDir->contents, info, currentDir->size);
  } else {
    printf("directory doesn't exist\n");
    return -1;
  }

  free(info);
  free(temp);

  return 0;
}

int renameObject(char* name, char* newName, bool isADir) {
  if(strcmp(name, "") == 0 || strcmp(newName, "") == 0) {
    printf("must specify name and new name\n");
    return -1;
  }

  if(strlen(name) > MAX_FILENAME) {
    printf("new name too long, max is 128 characters");
    return -1;
  }

  if(strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
    printf("invalid name: %s\n", name);
    return -1;
  }

  if(strcmp(name, newName) == 0) {
    return 0; //nothin to do!
  }
  
  block_id *subDir = malloc(currentDir->size);
  load_block(currentDir->contents, subDir, currentDir->size);

  fileHeader *fh = malloc(sizeof(fileHeader));

  unsigned int i = 0;
  for(; i < currentDir->size / sizeof(block_id); i++) {
    if(subDir[i] == 0) {
      continue;
    }

    load_block(subDir[i], fh, sizeof(fileHeader));

    if(fh->isDirectory != isADir) {
      continue;
    }

    if(strcmp(name, fh->name) == 0) {
      break;
    }
  }

  if(i == currentDir->size / sizeof(block_id)) {
    printf("%s %s does not exist!", (isADir ? "directory" : "file"), name);
    return -1;
  }


  strncpy(fh->name, newName, MAX_FILENAME);

  save_block(subDir[i], fh, sizeof(fileHeader));

  free(fh);
  free(subDir);

  return 0;
}

int do_mvdir(char *name, char *newName)
{
  if (debug) printf("%s\n", __func__);



  return renameObject(name, newName, true);



}

int do_mvfil(char *name, char *newName)
{
  if (debug) printf("%s\n", __func__);



  return renameObject(name, newName, false);  



}

int do_mkfil(char *name, char *size)
{
  if (debug) printf("%s\n", __func__);

  if(strcmp(name, "") == 0) {
    printf("specify a file name\n");
    return -1;
  }

  if(strlen(name) > MAX_FILENAME) {
    printf("filename too long, max is 128 characters");
    return -1;
  }

  if(strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
    printf("invalid file name: %s\n", name);
    return -1;
  }

  int requestedSize = atoi(size);

  if(requestedSize < 0) {
    printf("can't allocate negative sized file\n");
    return -1;
  }

  // search this directory for a open slot while also checking to see if
  // there's a name conflict.

  block_id *info = malloc(currentDir->size);
  load_block(currentDir->contents, info, currentDir->size);

  fileHeader *temp = malloc(sizeof(fileHeader));
  int openSlot = -1;
  unsigned int i = 0;
  for(; i < currentDir->size / sizeof(block_id); i++) {
    if(info[i] == 0) {
      if(openSlot == -1) {
        openSlot = i;
      }
      continue;
    }

    load_block(info[i], temp, sizeof(fileHeader));

    if(temp->isDirectory) {
      continue;
    }

    if(strcmp(temp->name, name) == 0) {
      printf("file already exists\n");
      return -1;
    }

  }

  // at this point, there at least isn't a name conflict

  if(openSlot == -1) {
    // shit, the directory is full.

    block_id old_id = currentDir->currentID;

    currentDir->size = currentDir->size * 2;
    currentDir->currentID = resize_block(currentDir->currentID, currentDir->size + sizeof(fileHeader));
    currentDir->contents = currentDir->currentID + sizeof(fileHeader);

    info = realloc(info, currentDir->size);
    load_block(currentDir->contents, info, currentDir->size);

    // we gotta zero out the new entries, resize does not guarentee that they're zero.
    // we continue where we left off.
    for(; i < currentDir->size / sizeof(block_id); i++) {
      info[i] = 0;
      if(info[i] == 0 && openSlot == -1) {
        // trip first time.
        openSlot = i;
      }
    }

    // now there's an open slot, but if the id changed, we need to update parent and children
    // before continuing.
    if(old_id != currentDir->currentID) {
      if(currentDir->parent != 0) {
        load_block(currentDir->currentID, temp, sizeof(fileHeader));

        block_id *parContent = malloc(temp->size);
        load_block(temp->contents, parContent, temp->size);
        bool updateOccured = false;

        for(unsigned int i = 0; i < temp->size / sizeof(block_id) && !updateOccured; i++) {
          if(parContent[i] == old_id) {
            parContent[i] = currentDir->currentID;
            save_block(temp->contents, parContent, temp->size);
            updateOccured = true;
          }
        }

        if(!updateOccured) {
          printf("directory structure is corrupt.\n");
          return -1;
        }

        free(parContent);

      } else {
        // updating the parent is just saving this as the new rootID
        saveRootID(currentDir->currentID);
      }

      //childn' are gettin adopted
      for(unsigned int k = 0; k < currentDir->size / sizeof(block_id); k++) {
        if(info[k] == 0) {
          continue;
        }

        load_block(info[k], temp, sizeof(fileHeader));

        if(temp->parent != old_id) {
          printf("directory structure is corrupt.\n");
          return -1;
        }

        temp->parent = currentDir->currentID;

        save_block(info[k], temp, sizeof(fileHeader));
      }
    }
  }

  fileHeader *newDir = calloc(1, sizeof(fileHeader));

  newDir->isDirectory = false;
  newDir->parent = currentDir->currentID;
  newDir->size = requestedSize;
  newDir->currentID = allocate_block(newDir->size + sizeof(fileHeader));
  newDir->contents = newDir->currentID + sizeof(fileHeader);
  strncpy(newDir->name, name, MAX_FILENAME);

  // save info and this header.

  info[openSlot] = newDir->currentID;
  save_block(currentDir->contents, info, currentDir->size);

  save_block(newDir->currentID, newDir, sizeof(fileHeader));

  free(info);
  free(temp);

  return 0;
}
int do_rmfil(char *name, char *size)
{
  if (debug) printf("%s\n", __func__);

  if(strcmp(name, "") == 0) {
    printf("specify a file\n");
    return -1;
  }

  if(strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
    printf("cannot delete %s", name);
    return -1;
  }
  
  // find file
  block_id *info = malloc(currentDir->size);
  load_block(currentDir->contents, info, currentDir->size);

  fileHeader *temp = malloc(sizeof(fileHeader));
  bool didDelete = false;
  unsigned int i = 0;
  for(; i < currentDir->size / sizeof(block_id); i++) {
    if(info[i] == 0) {
      continue;
    }

    load_block(info[i], temp, sizeof(fileHeader));

    if(temp->isDirectory) {
      continue;
    }

    if(strcmp(temp->name, name) == 0) {
      free_block(temp->currentID);
      didDelete = true;
      break;
    }

  }

  //zero out id
  if(didDelete) {
    info[i] = 0;
    save_block(currentDir->contents, info, currentDir->size);
  } else {
    printf("file doesn't exist\n");
    return -1;
  }

  free(info);
  free(temp);

  return 0;
}

int do_szfil(char *name, char *size)
{
  if (debug) printf("%s\n", __func__);
  
  if(strcmp(name, "") == 0) {
    printf("specify a file\n");
    return -1;
  }

  if(strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
    printf("cannot resize %s", name);
    return -1;
  }

  int requestedSize = atoi(size);

  if(size < 0) {
    printf("negative sizes don't make sense.\n");
    return -1;
  }
  
  // find file
  block_id *info = malloc(currentDir->size);
  load_block(currentDir->contents, info, currentDir->size);

  fileHeader *temp = malloc(sizeof(fileHeader));
  bool didResize = false;
  unsigned int i = 0;
  for(; i < currentDir->size / sizeof(block_id); i++) {
    if(info[i] == 0) {
      continue;
    }

    load_block(info[i], temp, sizeof(fileHeader));

    if(temp->isDirectory) {
      continue;
    }

    if(strcmp(temp->name, name) == 0) {
      if((unsigned int)requestedSize < temp->size) {
        printf("warning: truncating file.\n");
      }
      temp->size = requestedSize;
      temp->currentID = resize_block(temp->currentID, temp->size + sizeof(fileHeader));
      didResize = true;
      break;
    }

  }

  //update id
  if(didResize) {
    info[i] = temp->currentID;
    save_block(info[i], temp, sizeof(fileHeader));
    save_block(currentDir->contents, info, currentDir->size);
  } else {
    printf("file doesn't exist\n");
    return -1;
  }

  free(info);
  free(temp);

  return 0;
}

int do_exit(char *name, char *size)
{
  if (debug) printf("%s\n", __func__);
  
  exit(0);
}

/*--------------------------------------------------------------------------------*/

// parse a command line, where buf came from fgets()

// Note - the trailing '\n' in buf is whitespace, and we need it as a delimiter.

void parse(char *buf, int *argc, char *argv[])
{
  char *delim; // points to first space delimiter
  int count = 0; // number of args

  char whsp[] = " \t\n\v\f\r"; // whitespace characters

  while (1) // build the argv list
    {
      buf += strspn(buf, whsp); // skip leading whitespace
      delim = strpbrk(buf, whsp); // next whitespace char or NULL
      if (delim == NULL) // end of line
        { break; }
      argv[count++] = buf; // start argv[i]
      *delim = '\0'; // terminate argv[i]
      buf = delim + 1; // start argv[i+1]?
    }
  argv[count] = NULL;

  *argc = count;

  return;
}

/*--------------------------------------------------------------------------------*/

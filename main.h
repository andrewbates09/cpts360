/*
 * Author: Andrew M Bates (abates09) & Joshua Clark (joshua.a.clark)
 * Assign: Final Project
 * Due   : April 25, 2013
 * 
 */

#ifndef __MAIN_H__
#define __MAIN_H__


// * -- INCLUDES -- * //
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/ext2_fs.h>     // linux/ext2_fs || ext2fs/ext2_fs
#include <libgen.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

//#include "util.c"

//#include "level1.c"
//#include "level2.c"
//#include "level3.c"


// * -- DEFINITIONS -- * //

typedef int bool;                   // added readability for fools
enum { false, true };

typedef struct ext2_group_desc  GD;
typedef struct ext2_super_block SUPER;
typedef struct ext2_inode       INODE;
typedef struct ext2_dir_entry_2 DIR;

#define BLOCK_SIZE          1024
#define BITS_PER_BLOCK      (8*BLOCK_SIZE)
#define INODES_PER_BLOCK    (BLOCK_SIZE/sizeof(INODE))

// Block number of EXT2 FS on FD
#define SUPERBLOCK      1
#define GDBLOCK         2
#define BBITMAP         3
#define IBITMAP         4
#define INODEBLOCK      5
#define ROOT_INODE      2

// Default file modes
#define FIFO_MODE       0010644
#define DIR_MODE        0040777
#define BLOCK_MODE      0060644
#define FILE_MODE       0100644
#define SYM_MODE        0120644
#define SUPER_MAGIC     0xEF53
#define SUPER_USER      0

// Proc status
#define FREE            0
#define BUSY            1
#define KILLED          2

// Table sizes
#define NMINODES        50
#define NMOUNT          10
#define NPROC           10
#define NFD             10
#define NOFT            50

// Open File Table
typedef struct Oft{
    int     mode;
    int     refCount;
    struct Minode *inodeptr;
    long    offset;
} OFT;

// PROC structure
typedef struct Proc{
    int     uid;
    int     pid;
    int     gid;
    int     ppid;
    struct Proc *parent;
    int     status;

    struct Minode *cwd;
    OFT     *fd[NFD];
} PROC;
      
// In-memory inodes structure
typedef struct Minode{		
    INODE   INODE;              // disk inode
    unsigned short  dev;                // mount table
    unsigned long   ino;
    unsigned short  refCount;
    unsigned short  dirty;
    unsigned short  mounted;
    struct Mount *mountptr;
    char name[128];          // name string of file
} MINODE;

// Mount Table structure
typedef struct Mount{
    int     ninodes;
    int     nblocks;
    int     fd; 
    bool    busy;   
    struct  Minode *mounted_inode;
    char    name[256]; 
    char    mount_name[64];
} MOUNT;


// * -- GLOBAL VARIABLES -- * //
OFT OFTABLE[NOFT];
MINODE MINODES[NMINODES];
PROC PROCS[NPROC];
MOUNT MOUNTS[NMOUNT];

GD    *gp;
SUPER *sp;
INODE *ip;
DIR   *dp;

bool mainloop = true;


// * - UNTIL - * //

void get_block(int dev, int blk, char *buf);

void put_block(int dev, int blk, char *buf);

char **token_path(char *pathname);

unsigned long getino(int *dev, char *pathname);

unsigned long search(const MINODE *mip, const char *name);

MINODE *iget(int dev, unsigned long ino);

void iput(MINODE *mip);

int findmyname(MINODE *parent, unsigned long myino, char *myname, int buflen);

int findino(MINODE *mip, unsigned long *myino, unsigned long *parentino);

int mountDevice(char *pathname, char *name);

bool getBit(const char *buffer, int index);

void setBit(char *buffer, int index, bool val);

int ialloc(int dev);

void ifree(int dev, int index);

int balloc(int dev);

void bfree(int dev, int index);

void createInode(int dev, int ino, int mode, int uid, int gid);

void createDirEntry(MINODE *parent, int ino, int type, char *name);

int removeDirEntry(MINODE *parent, const char* name);

// * - LEVEL 1 - * //

void init();

void mount_root(char *pathname);

int do_cd(char *pathname);

int do_ls(char *pathname);

int do_mkdir(char *pathname);

int do_rmdir(char *pathname);

int do_touch(char *pathname);
    
int do_chmod(char *pathname);

int do_chown(char *pathname);
 
int do_chgrp(char *pathname);

int do_stat(char *pathname);

int do_pwd(char *pathname);

int do_creat(char *pathname);

int do_link(char *pathname);

int do_unlink(char *pathname);

int do_symlink(char *pathname);

void quit();


// * - LEVEL 2 - * //



// * - LEVEL 3 - * //



// * - MAIN - * //

int do_menu(char *pathname);

int do_exit(char* pathname);

int iscommand(char *input);



// * - FUNCTION LIST - * //

char *command[] = {"mkdir", "rmdir", "cd", "chmod", "chown", "ls", "pwd", "creat", "link", "unlink", "symlink", "menu", "exit", "stat", "chmod", "touch", "chown", "chgrp", "0"};
int (*function[]) (char*) = {do_mkdir, do_rmdir, do_cd, do_chmod, do_chown, do_ls, do_pwd, do_creat, do_link, do_unlink, do_symlink, do_menu, do_exit, do_stat, do_chmod, do_touch, do_chown, do_chgrp};

#endif // __FPROJ_H__

/*
 * Author: Andrew M Bates (abates09) & Joshua Clark (joshua.a.clark)
 * Assign: Final Project
 * Due   : April 25, 2013
 * 
 */

/*
 * $ gcc -std=c99 -Wall -o "main" main.c"
 * $ clang main.c
 * 
 */
 
#include "main.h"

//--------------------------------------------------------------------------
// UTIL

// Reads block from disk
void get_block(int dev, int blk, char *buf)
{
    long offset = BLOCK_SIZE * blk;

    lseek(MOUNTS[dev].fd, offset, SEEK_SET);
    read(MOUNTS[dev].fd, buf, BLOCK_SIZE);
}

// Writes block to disk
void put_block(int dev, int blk, char *buf)
{
    long offset = BLOCK_SIZE * blk;

    lseek(MOUNTS[dev].fd, offset, SEEK_SET);
    write(MOUNTS[dev].fd, buf, BLOCK_SIZE);
}

// Split path into an array of directory names
// pass in: char testpath[] = "/dir1/dir2/dir3/base"; ~ abates
// is seg faulting on "a" or "/a" - works fine with others
char **token_path(char *pathname)
{
    char    **patharray = NULL;
    char    *tmppath = NULL;    
    int     npaths = 0;
    
    tmppath = strtok(pathname, "/");
    
    while(tmppath)
    {
		patharray = realloc (patharray, sizeof(char*) * ++npaths);
        
        if(tmppath == NULL)
        {
            break;
        }
        patharray[npaths - 1] = tmppath;
        tmppath = strtok(NULL, "/");
    }

    // realloc one extra element for the last NULL
    patharray = realloc (patharray, sizeof(char*) * (npaths + 1));
    patharray[npaths] = 0;
	
    return patharray;         // free(patharray);
}


// both of these functions are in clib, they destroy pathname ~ abates
// char *dirname(char *pathname)
// char *basename(char *pathname)


// Converts a pathname into its dev and inode
// returns inode number
unsigned long getino(int *dev, char *pathname)
{
	MINODE *minode = PROCS[0].cwd; 	// Change if doing mount

	if(pathname == NULL)
	{
		*dev = minode->dev;
		return minode->ino;
	}
	if(pathname[0] == '/')
		minode = &MINODES[0];

	minode->refCount++;
	char **paths = token_path(pathname);

	int i = 0;
	while(paths[i] != 0)
	{
		if((minode->INODE.i_mode & 0x4000) != 0x4000 && paths[i + 1] != 0) // In case one of the paths is not a dir
		{
			iput(minode);
			return -1;
		}

		int ino = search(minode, paths[i]);
		
		if(ino == -1) 	// In case search fails to find entry
		{
			iput(minode);
			return -1;
		}

		iput(minode);
		minode = iget(0, ino); 	// Fix this Should use div
		++i;
	}

	int ino = minode->ino;
	iput(minode);
	return ino;
}


// Searches DIR data block for name
unsigned long search(const MINODE *mip, const char *name)
{
    char buf[BLOCK_SIZE];
    for(int i = 0; i < 12; ++i)
    {
        if(mip->INODE.i_block[i] != 0)
        {
            get_block(mip->dev, mip->INODE.i_block[i], buf);
            DIR *dir = (DIR *)&buf;
            int pos = 0;
            while(pos < BLOCK_SIZE)
            {
                char dirname[dir->name_len + 1];
                strncpy(dirname, dir->name, dir->name_len);
                dirname[dir->name_len] = '\0';

				if(!strcmp(dirname, name))
                    return dir->inode;

                // move rec_len bytes
                char *loc = (char *)dir;
                loc += dir->rec_len;
				pos += dir->rec_len;
                dir = (DIR *)loc;
            }
        }
    }
	return -1;
}

// Creates MINODE from dev and ino
MINODE *iget(int dev, unsigned long ino)
{
	for(int i = 0; i < NMINODES; ++i)
	{
		if(MINODES[i].ino == ino && MINODES[i].dev == dev && MINODES[i].refCount > 0)
		{
			MINODES[i].refCount++;
			return &MINODES[i];
		}
	}
	int index = 0;
	while(index < NMINODES)
	{
		if(MINODES[index].refCount == 0)
			break;
		++index;
	}

	// Get inode from disk
	int block = (ino - 1) / INODES_PER_BLOCK;
	int ind = (ino - 1) % INODES_PER_BLOCK;

	char buffer[BLOCK_SIZE];
	get_block(dev, block + INODEBLOCK, buffer);

	INODE *inodes = (INODE *)buffer;
	memcpy((char *)&(MINODES[index].INODE), (char *)&(inodes[ind]), 128);
	
	// populate MINODE structure
	MINODES[index].dev = dev;
	MINODES[index].ino = ino;
	MINODES[index].refCount = 1;
	MINODES[index].dirty = 0;

	return &MINODES[index];
}

// Releases and MINODE
// Decreases refCount until refCount == 0 then writes to disk
void iput(MINODE *mip)
{
    mip->refCount--;
    if(mip->refCount == 0 && mip->dirty) 	// Remember to set to dirty flag
    {
        int inode = mip->ino - 1;
        int block = inode / 8; 	// Change for real drive
        int ino = inode % 8;
        
        char buf[BLOCK_SIZE];
        get_block(mip->dev, block + INODEBLOCK, buf);
        char *loc = buf + ino * 128; 	// Change for real drive
        memcpy(loc, &(mip->INODE), 128);
        put_block(mip->dev, block + INODEBLOCK, buf);
    }
}

// Finds the name string of myino in parent's datablock
int findmyname(MINODE *parent, unsigned long myino, char *myname, int buflen)
{
	char buf[BLOCK_SIZE];
    for(int i = 0; i < 12; ++i)
    {
        if(parent->INODE.i_block[i] != 0)
        {
            get_block(parent->dev, parent->INODE.i_block[i], buf);
            DIR *dir = (DIR *)&buf;
            int pos = 0;
            while(pos < BLOCK_SIZE)
            {
                char dirname[dir->name_len + 1];
                strncpy(dirname, dir->name, dir->name_len);
                dirname[dir->name_len] = '\0';

				if(dir->inode == myino)
                {
					strncpy(myname, dirname, buflen - 1);
					myname[buflen - 1] = '\0';
					return 0;
				}

                // move rec_len bytes
                char *loc = (char *)dir;
                loc += dir->rec_len;
				pos += dir->rec_len;
                dir = (DIR *)loc;
            }
        }
    }
	return -1;
}

// Gets ino of parent 
int findino(MINODE *mip, unsigned long *myino, unsigned long *parentino)
{
    *myino = search(mip, ".");
    *parentino = search(mip, "..");
    return *myino;
}

int mountDevice(char *pathname, char *name)
{
    int fd = open(pathname, O_RDWR);
    if(fd < 0)
    {
	   return -1;	
    }

    for(int i = 0; i < NMOUNT; ++i)
    {
        if(MOUNTS[i].fd == 0)
        {
            MOUNTS[i].fd = fd;
            MOUNTS[i].busy = false;
            strcpy(MOUNTS[i].name, pathname);
            strcpy(MOUNTS[i].mount_name, name);

			// Get super block
            char *buf = malloc(BLOCK_SIZE);
            get_block(i, SUPERBLOCK, buf);

            sp = (SUPER *)buf;  // Change if supporting mount in level3

			if(sp->s_magic != 0xEF53)
			{
				return -1;
			}

            MOUNTS[i].ninodes = sp->s_inodes_count;
            MOUNTS[i].nblocks = sp->s_blocks_count;

            MOUNTS[i].mounted_inode = iget(i, ROOT_INODE);  // .Minode (MOUNT has no member named Minode) ~ abates

			// Get group descriptor block
			buf = malloc(BLOCK_SIZE);
			get_block(i, GDBLOCK, buf);

			gp = (GD *)buf; 	// Change if supporting mount in level3

			if(!strcmp(name, "/"))
			{
				iget(i, 2);
				strcpy(MINODES[0].name, "/");
			}
			break;
        }
    }
    return 0;
}

// Gets the value of a bit in a block
bool getBit(const char *buffer, int index)
{
	int byte = index / 8;
	int bit = index % 8;

	if(buffer[byte] & (1 << bit))
		return true;
	else
		return false;
}

// Sets the value of a bit in a block
void setBit(char *buffer, int index, bool val)
{
	int byte = index / 8;
	int bit = index % 8;

	if(val)
		buffer[byte] |= ( 1 << bit);
	else
		buffer[byte] &= ~(1 << bit);
}


// Allocates a free inode number
int ialloc(int dev)
{
	char buffer[BLOCK_SIZE];
	get_block(dev, IBITMAP, buffer);

	for(int i = 0; i < MOUNTS[dev].ninodes; ++i)
	{
		if(!getBit(buffer, i))
		{
			setBit(buffer, i, true);
			put_block(dev, IBITMAP, buffer);
			return i + 1;
		}
	}
	return -1;
}

// Frees an allocated inode	
void ifree(int dev, int index)
{
	char buffer[BLOCK_SIZE];
	get_block(dev, IBITMAP, buffer);
	setBit(buffer, index - 1, false); 
}

// Allocates a free data block
int balloc(int dev)
{
	char buffer[BLOCK_SIZE];
	get_block(dev, BBITMAP, buffer);

	for(int i = 0; i < MOUNTS[dev].nblocks; ++i)
	{
		if(!getBit(buffer, i))
		{
			setBit(buffer, i, true);
			put_block(dev, BBITMAP, buffer);
			return i;
		}
	}
	return -1;
}

// Frees an allocated data block	
void bfree(int dev, int index)
{
	char buffer[BLOCK_SIZE];
	get_block(dev, BBITMAP, buffer);
	setBit(buffer, index, false); 
}

// Creates a new empty inode
void createInode(int dev, int ino, int mode, int uid, int gid)
{
	MINODE *mip = iget(dev, ino);

	mip->INODE.i_mode = mode;
	mip->INODE.i_uid = uid;
	mip->INODE.i_gid = gid;
	mip->INODE.i_size = 0;
	mip->INODE.i_links_count = 0;

	mip->INODE.i_atime = mip->INODE.i_ctime = mip->INODE.i_mtime = time(0);

	mip->INODE.i_blocks = 0;
	mip->dirty = true;

	for(int i = 0; i < 15; ++i)
	{
		mip->INODE.i_block[i] = 0;
	}
}

// Adds a dir entry to an existing directory
void createDirEntry(MINODE *parent, int ino, int type, char *name)
{
	int newideal = 4 * ((8 + strlen(name) + 3) / 4);
	char buffer[BLOCK_SIZE];
	for(int i = 0; i < 12; ++i)
	{
		if(parent->INODE.i_block[i] != 0)
        {
            get_block(parent->dev, parent->INODE.i_block[i], buffer);
            DIR *dir = (DIR *)&buffer;
            int pos = 0;
            while(pos < BLOCK_SIZE)
            {
                int curideal = 4 * ((8 + dir->name_len + 3) / 4); // Must be multiple of 4.

				if(dir->rec_len > (curideal + newideal))
				{
					dir->rec_len = curideal;

					// Move to end of current entry
					char *loc = (char *)dir;
                	loc += dir->rec_len;
					pos += dir->rec_len;
                	dir = (DIR *)loc;

					// Create new dir entry
					dir->inode = ino;
					dir->rec_len = BLOCK_SIZE - pos;
					dir->name_len = strlen(name);
					dir->file_type = type;
					strncpy(dir->name, name, strlen(name));

					put_block(parent->dev, parent->INODE.i_block[i], buffer);
					return;
				}	

                // move rec_len bytes
                char *loc = (char *)dir;
                loc += dir->rec_len;
				pos += dir->rec_len;
                dir = (DIR *)loc;
            }
        }
		else
		{
			int block = balloc(parent->dev);
			parent->INODE.i_size += BLOCK_SIZE;
			parent->INODE.i_blocks += BLOCK_SIZE / 512;
			parent->INODE.i_block[i] = block;
			parent->dirty = true;

			get_block(parent->dev, block, buffer);
			DIR *dir = (DIR *)&buffer;

			// Create new dir entry
			dir->inode = ino;
			dir->rec_len = BLOCK_SIZE;
			dir->name_len = strlen(name);
			dir->file_type = type;
			strncpy(dir->name, name, strlen(name));

			put_block(parent->dev, parent->INODE.i_block[i], buffer);

			return;
		}
	}
}


// Remove Dir Entry
// pass in parent and dir entry ino to remove
int removeDirEntry(MINODE *parent, const char* name)
{
    char buf[BLOCK_SIZE];
    for(int i = 0; i < 12; ++i)
    {
        if(parent->INODE.i_block[i] != 0)
        {
            get_block(parent->dev, parent->INODE.i_block[i], buf);
            DIR *dir = (DIR *)&buf;
            DIR *prevdir;
            int pos = 0;
            char *loc = (char *)dir;
            
            while(pos < BLOCK_SIZE)
            {
                char dirname[dir->name_len + 1];
                strncpy(dirname, dir->name, dir->name_len);
                dirname[dir->name_len] = '\0';

				if(!strcmp(dirname, name))  // found dir
                {
                    if (pos + dir->rec_len == BLOCK_SIZE)   //last entry
                    {
						if (pos == 0)   // only entry in block
                        {
                            int tmp = parent->INODE.i_block[i];
                            parent->INODE.i_block[i] = 0;
                            bfree(parent->dev, tmp);
							parent->dirty = true;
                            return 0;
                        }

						prevdir->rec_len += dir->rec_len;
						put_block(parent->dev, parent->INODE.i_block[i], buf);
                        return 0;
                    }
                    
                    prevdir = dir;  // set tail
                    int posb = pos;
                    
                    // move head rec_len bytes
                    loc = (char *)dir;
                    loc += dir->rec_len;
                    posb += dir->rec_len;
                    dir = (DIR *)loc;
                   
                    int remlen = prevdir->rec_len; // leftover space
                    
                    while(posb < BLOCK_SIZE)
                    {
                        prevdir->rec_len = dir->rec_len;    // assign
                        
                        // copy head to tail
                        prevdir->inode = dir->inode;
                        prevdir->file_type = dir->file_type;
                        prevdir->name_len = dir->name_len;
                        memcpy(prevdir->name, dir->name, dir->name_len);
                        
                        // give rest of space to prevdir
                        if (posb + dir->rec_len == BLOCK_SIZE)
                        {
                            prevdir->rec_len += remlen;
							put_block(parent->dev, parent->INODE.i_block[i], buf);
                            return 0;
                        }
                        
                        // move head rec_len bytes
                        loc = (char *)dir;
                        loc += dir->rec_len;
                        posb += dir->rec_len;
                        dir = (DIR *)loc;
                        
                        // move tail rec_len bytes
                        loc = (char *)prevdir;
                        loc += prevdir->rec_len;
                        posb += prevdir->rec_len;
                        prevdir = (DIR *)loc;
                    }
                }
                prevdir = dir;
                
                // move rec_len bytes
                loc = (char *)dir;
                loc += dir->rec_len;
				pos += dir->rec_len;
                dir = (DIR *)loc;
                
            }
        }
    }
	return -1;
}


//--------------------------------------------------------------------------
// LEVEL 1

void init()
{
	PROCS[0].uid = 0;
	PROCS[0].pid = 0;
	PROCS[0].gid = 0;
	PROCS[0].cwd = &MINODES[0];

	PROCS[1].uid = 1;
	PROCS[1].pid = 1;
	PROCS[1].gid = 1;
	PROCS[1].cwd = &MINODES[0];

}

void mount_root(char *pathname)
{
	printf("Mounting root directory.\n");
    mountDevice(pathname, "/");
}

int do_cd(char *pathname)
{
	int ino = getino(0, pathname);
	if(ino == -1)
	{
		printf("Path does not exist\n");
		return -1;
	}

	MINODE *mp = iget(0, ino);
	if((mp->INODE.i_mode & 0x4000) != 0x4000 )
	{
		printf("Path is not a directory.\n");
		return -1;
	}

	iput(PROCS[0].cwd);
	PROCS[0].cwd = mp;
    return 0;
}

int do_ls(char *pathname)
{
	MINODE *mip = PROCS[0].cwd;
	if(pathname)
	{
		int ino = getino(0, pathname);

		if(ino == -1)
		{
			printf("Could not find directory.\n");
			return -1;
		}
		mip = iget(0, ino);
	}

	// First 12 data blocks
	for(int i = 0; i < 12; ++i)
	{
		if(mip->INODE.i_block[i] == 0)
		{
			return 0;
		}

		char buffer[BLOCK_SIZE];
		get_block(0, mip->INODE.i_block[i], buffer);
		DIR *dir = (DIR *)buffer;

		int pos = 0;
		while(pos < BLOCK_SIZE)
		{
			char dirname[dir->name_len + 1];
			strncpy(dirname, dir->name, dir->name_len);
			dirname[dir->name_len] = '\0';

			MINODE *curmip = iget(mip->dev, dir->inode);

			printf( (curmip->INODE.i_mode & 0x4000) ? "D" : "");
			printf( (curmip->INODE.i_mode & 0x8000) == 0x8000 ? "R" : "");
			printf( (curmip->INODE.i_mode & 0xA000) == 0xA000 ? "S" : "");

            char *badctime = ctime(&curmip->INODE.i_mtime);
            badctime[24] = '\0';

    		printf( (curmip->INODE.i_mode & 0x0100) ? " r" : " -");
    		printf( (curmip->INODE.i_mode & 0x0080) ? "w" : "-");
    		printf( (curmip->INODE.i_mode & 0x0040) ? "x" : "-");
    		printf( (curmip->INODE.i_mode & 0x0020) ? "r" : "-");
    		printf( (curmip->INODE.i_mode & 0x0010) ? "w" : "-");
    		printf( (curmip->INODE.i_mode & 0x0008) ? "x" : "-");
    		printf( (curmip->INODE.i_mode & 0x0004) ? "r" : "-");
    		printf( (curmip->INODE.i_mode & 0x0002) ? "w" : "-");
    		printf( (curmip->INODE.i_mode & 0x0001) ? "x" : "-");
            
			printf("\t%d\t%d\t%d\t%s\t%s:%d\n", curmip->INODE.i_uid,
									 curmip->INODE.i_gid,
									 curmip->INODE.i_size,
                                     badctime,
									 //ctime(&curmip->INODE.i_mtime),
									 dirname, dir->inode);
			iput(curmip);

			// move rec_len bytes
			char *loc = (char *)dir;
			loc += dir->rec_len;
			pos += dir->rec_len;
			dir = (DIR *)loc;
		}
	}
	return 0;
}

int do_mkdir(char *pathname)
{
	char path[strlen(pathname) + 1];
	strcpy(path, pathname);
	char *base = basename(pathname);
	char *dir = NULL;
	if(strcmp(base, path))
	{
		dir = dirname(path);
	}

	int dev;
	int parentino = getino(&dev, dir);
	if(parentino == -1)
	{
		printf("Could not find parent.\n");
		return -1;
	}

	MINODE *parent = iget(dev, parentino);
	if((parent->INODE.i_mode & 0x4000) != 0x4000 )
	{
		printf("Parent is not a directory.\n");
		return -1;
	}

	if(search(parent, base) != -1)
	{
		printf("%s already exists.\n", base);
		return -1;
	}

	int ino = ialloc(dev);
	createDirEntry(parent, ino, EXT2_FT_DIR, base);
	createInode(dev, ino, DIR_MODE, PROCS[0].uid, PROCS[0].gid);
	MINODE *mip = iget(dev, ino);
	mip->INODE.i_links_count++;

	createDirEntry(mip, ino, EXT2_FT_DIR, ".");
	createDirEntry(mip, parent->ino, EXT2_FT_DIR, "..");

 	parent->INODE.i_atime = parent->INODE.i_mtime = time(0);

	iput(parent);
	iput(mip);
    
    return 0;
}


// remove directory if empty
int do_rmdir(char *pathname)
{
    // 
    char bpath[strlen(pathname) + 1];
	strcpy(bpath, pathname);
	char *base = basename(bpath);
	char *path = NULL;
    
	if(strcmp(base, bpath))
	{
		path = dirname(bpath);
    }
    
    int dev;
    int parentino = getino(&dev, path);    // get ino for parent
	if(parentino == -1)
	{
		printf("Could not find parent.\n");
		return -1;
	}
    MINODE *pip = iget(dev, parentino);
    
    int ino = search(pip, base);    // get ino  for dir to remove
    MINODE *mip = iget(dev, ino);
    
    
    //- lvl 3 check ownership goes here -//
    
    if ((mip->INODE.i_mode & 0x4000) != 0x4000)    // check dir
    {
        printf("Not a directory. Try again.\n");
        iput(mip);
        iput(pip);
        return -1;
    }
    
    if (mip->refCount > 1)   // check busy
    {
        printf("Directory is busy.\n");
        iput(mip);
        iput(pip);
        return -1;
    }

    // check dir is not empty
    char buf[BLOCK_SIZE];
    for(int i = 0; i < 12; ++i)
    {   
        printf("mip->INODE.i_block[i] = %d\n", mip->INODE.i_block[i]);
        if(mip->INODE.i_block[i] != 0)
        {
            get_block(mip->dev, mip->INODE.i_block[i], buf);
            DIR *dir = (DIR *)&buf;
            int pos = 0;
            while(pos < BLOCK_SIZE)
            {
                char dname[dir->name_len + 1];
                strncpy(dname, dir->name, dir->name_len);
                dname[dir->name_len] = '\0';
				if (strcmp(dname, ".") && strcmp(dname, ".."))    // &&
                {
                    printf("Unabe to remove dir.\n");
                    iput(mip);
                    iput(pip);
                    return -1;
                }
                // move rec_len bytes
                char *loc = (char *)dir;
                loc += dir->rec_len;
				pos += dir->rec_len;
                dir = (DIR *)loc;
            }
        }
    }
    
    // remove entry
    int rem = removeDirEntry(pip, base);
    if (rem == -1)
    {
        printf("Unabe to remove dir.\n");
        iput(mip);
        iput(pip);
        return -1;
    }

    // Deallocate its block and inode
    for (int i = 0; i < 12; ++i)
    {
        if (mip->INODE.i_block[i] != 0)
        {
            int tmpparent = pip->INODE.i_block[i];
            mip->INODE.i_block[i] = 0;
            bfree(pip->dev, tmpparent);
        }
    }
    
    ifree(mip->dev, mip->ino);
    mip->refCount = 0;  // iput(mip);
    
    // touch pip's atime, mtime fields;
    pip->INODE.i_atime = pip->INODE.i_mtime = time(0);
    
    // mark pip dirty;
    pip->dirty = true;
    iput(pip);

    return 0;
}


// modify the INODE's i_atime and i_mtime fields.
int do_touch(char *pathname)
{
	int dev;
	int ino = getino(&dev, pathname);
	if(ino == -1)
	{
		printf("Path does not exist.\n");
		return -1;
	}

	MINODE *mip = iget(dev, ino);

	mip->INODE.i_atime = mip->INODE.i_mtime = time(0);
	mip->dirty = true;

	iput(mip);

    return 0;
} 


// modify INODE.i_mode's permission bits
// You may specify newMode in Octal form, e.g. 0644
// or you may use +x, -r, +w, etc fancier formats.
// chmod
int do_chmod(char *pathname) // (int newMode, char *pathname)
{
    char *mod = strtok(pathname, " ");
	if(mod == NULL && *mod > '0')
	{
		printf("Please include both a mod and pathname.\n");
		return -1;
	}

	char *path = strtok(NULL, "");
	if(path == NULL)
	{
		printf("Please include both a mod and pathname.\n");
		return -1;
	}

	int dev;
	int ino = getino(&dev, path);
	if(ino == -1)
	{
		printf("Path does not exist.\n");
		return -1;
	}

	MINODE *mip = iget(dev, ino);

	int m = strtol(mod, 0, 8);
	for(int i = 0; i < 9; ++i)
	{
		if(m & (1 << i))
			mip->INODE.i_mode |= ( 1 << i);
		else
			mip->INODE.i_mode &= ~(1 << i); 
	}

	mip->INODE.i_atime = mip->INODE.i_mtime = time(0);
	mip->dirty = true;

    return 0;

}


// change INODE.i_uid to newOwner
// chown
int do_chown(char *pathname) //(int newOwner, char *pathname)
{
	char *usr = strtok(pathname, " ");
	if(usr == NULL && *usr > '0')
	{
		printf("Please include both a user and pathname.\n");
		return -1;
	}

	char *path = strtok(NULL, "");
	if(path == NULL)
	{
		printf("Please include both a user and pathname.\n");
		return -1;
	}

	int dev;
	int ino = getino(&dev, path);
	if(ino == -1)
	{
		printf("Path does not exist.\n");
		return -1;
	}

	MINODE *mip = iget(dev, ino);
	mip->INODE.i_uid = *usr - '0';

	mip->INODE.i_atime = mip->INODE.i_mtime = time(0);

	mip->dirty = true;

	iput(mip);

    return 0;
}
 

// change INODE.i_gid to newGroup
int do_chgrp(char *pathname)    //(int newGroup, char *pathname)
{
	char *grp = strtok(pathname, " ");
	if(grp == NULL && *grp > '0')
	{
		printf("Please include both a group and pathname.\n");
		return -1;
	}

	char *path = strtok(NULL, "");
	if(path == NULL)
	{
		printf("Please include both a group and pathname.\n");
		return -1;
	}

	int dev;
	int ino = getino(&dev, path);
	if(ino == -1)
	{
		printf("Path does not exist.\n");
		return -1;
	}

	MINODE *mip = iget(dev, ino);
	mip->INODE.i_gid = *grp - '0';

	mip->INODE.i_atime = mip->INODE.i_mtime = time(0);
	mip->dirty = true;

	iput(mip);


    return 0;
}

int do_stat(char *pathname) // (char *pathname, struct stat *stPtr)
{
	char path[strlen(pathname) + 1];
	strcpy(path, pathname);

	MINODE *mip;
    
    if (pathname == NULL)
    {
        printf("Please pass in a filename\n");
        return -1;
    }
	int ino = getino(0, pathname);
	if(ino == -1)
	{
		printf("Could not find file\n");
		return -1;
	}
	mip = iget(0, ino);
    
	char *base = basename(path);
	printf("  File: '%s'", base);     // dirname
		
	printf("\n  Size: %d", mip->INODE.i_size);     // %lld", (long long) curmip->INODE.i_size);
	printf("\tBlocks: %d", mip->INODE.i_blocks / 2);       // %lld", (long long) sb.st_blocks);
	printf("\tIO Block: %d", BLOCK_SIZE);     // %ld", (long) sb.st_blksize);
	printf("\tType: ");
		printf( (mip->INODE.i_mode & 0x4000) ? "Dir " : "");
		printf( (mip->INODE.i_mode & 0x8000) == 0x8000 ? "Reg " : "");
		printf( (mip->INODE.i_mode & 0xA000) == 0xA000 ? "Sym " : "");
	
	printf("\n  Dev: %d", mip->dev);        //
	printf("\tIno: %lu", mip->ino);            // %ld", (long) dir->ino);
	printf("\tLinks: %d", mip->INODE.i_links_count);          // %ld", (long) sb.st_nlink);

	printf("\n  Permisions: ");
		printf( (mip->INODE.i_mode & 0x0100) ? "r" : "-");
		printf( (mip->INODE.i_mode & 0x0080) ? "w" : "-");
		printf( (mip->INODE.i_mode & 0x0040) ? "x" : "-");
		printf( (mip->INODE.i_mode & 0x0020) ? "r" : "-");
		printf( (mip->INODE.i_mode & 0x0010) ? "w" : "-");
		printf( (mip->INODE.i_mode & 0x0008) ? "x" : "-");
		printf( (mip->INODE.i_mode & 0x0004) ? "r" : "-");
		printf( (mip->INODE.i_mode & 0x0002) ? "w" : "-");
		printf( (mip->INODE.i_mode & 0x0001) ? "x" : "-");
	printf("\tUid: %d", mip->INODE.i_uid);          // %ld", (long) curmip->INODE.i_uid);
	printf("\tGid: %d", mip->INODE.i_gid);          // %ld", (long) curmip->INODE.i_gid);
	
	printf("\n  Access: %s", ctime(&mip->INODE.i_atime));  
	printf("  Modify: %s", ctime(&mip->INODE.i_mtime));
	printf("  Change: %s", ctime(&mip->INODE.i_ctime));

	iput(mip);

    return 0;
}

// Prints current working directory. Mounting deviced not supported TODO missing iput?
int do_pwd(char *pathname)
{
    char **pwdarray = NULL;
	int nparen = 0;
	char namebuf[1024];
    char *tmppath = namebuf;
    
    // find first
	MINODE *curmip = PROCS[0].cwd;
	curmip->refCount++;
    
    // do rest once fist initialized
	while(curmip->ino != 2 || curmip->dev != 0) // handles base case
	{
		pwdarray = realloc(pwdarray, sizeof(char*) * ++nparen); // make more space 
        
		// do everything that josh just did in his version of pwd to check for stuff
		// search up
		// get next parent or current directory
		int parentino = search(curmip, "..");
		MINODE *parent = iget(0, parentino); // Change if supporting more devices
		
		int err = findmyname(parent, curmip->ino, tmppath, 128);
		if(err == -1)
		{
			printf("I no know what happen.\n");
			return -1;
		}

		pwdarray[nparen - 1] = tmppath;
		tmppath += strlen(tmppath) + 1;

		iput(curmip);
		curmip = parent;
	}

	pwdarray = realloc (pwdarray, sizeof(char*) * (nparen + 1));
	pwdarray[nparen] = 0;
	
    // do the printing
	int i = 0;
	printf("/");	// basecase (at root)
	while(pwdarray[i] != 0)	
	{
		printf("%s/", pwdarray[nparen - i - 1]); // print the array in reverse
		++i;
	}
	printf("\n");

    return 0;
}

// creat
int do_creat(char *pathname)
{
    char path[strlen(pathname) + 1];
	strcpy(path, pathname);
	char *base = basename(pathname);
	char *dir = NULL;
	if(strcmp(base, path))
	{
		dir = dirname(path);
	}

	int dev;
	int parentino = getino(&dev, dir);
	if(parentino == -1)
	{
		printf("Could not find parent.\n");
		return -1;
	}

	MINODE *parent = iget(dev, parentino);
	if((parent->INODE.i_mode & 0x4000) != 0x4000 )
	{
		printf("Parent is not a directory.\n");
		return -1;
	}

	int ino = ialloc(dev);
	createDirEntry(parent, ino, EXT2_FT_REG_FILE, base);
	createInode(dev, ino, FILE_MODE, PROCS[0].uid, PROCS[0].gid);
	MINODE *mip = iget(dev, ino);
	mip->INODE.i_links_count++;

	iput(parent);
	iput(mip);
    
    return 0;
}

// link
int do_link(char *pathname)
{
	char *patha = strtok(pathname, " ");
	char *pathb = strtok(NULL, " ");

	int dev;
	int ino = getino(&dev, patha);
	if(ino == -1)
	{
		printf("Invalid source path.\n");
		return -1;
	}
    MINODE *mipsrc = iget(dev, ino);

	if((mipsrc->INODE.i_mode & 0x8000) != 0x8000)
	{
		iput(mipsrc);
		printf("Source is not a regular file.\n");
		return -1;
	}
	
	char path[strlen(pathb) + 1];
	strcpy(path, pathb);
	char *base = basename(pathb);
	char *dir = NULL;
	if(strcmp(base, path))
	{
		dir = dirname(path);
	}

	ino = getino(&dev, dir);
	if(ino == -1)
	{
		iput(mipsrc);
		printf("Invalid target path.\n");
		return -1;
	}
    MINODE *mipparent = iget(dev, ino);
	if((mipparent->INODE.i_mode & 0x4000) != 0x4000)
	{
		iput(mipsrc);
		iput(mipparent);
		printf("Target path is not a directory.\n");
		return -1;
	}

	createDirEntry(mipparent, mipsrc->ino, EXT2_FT_REG_FILE, base);
	mipsrc->INODE.i_links_count++;
	mipsrc->dirty = true;

	iput(mipsrc);
	iput(mipparent);
	
	return 0;
}

// unlink
int do_unlink(char *pathname)
{
	char path[strlen(pathname) + 1];
	strcpy(path, pathname);
	char *base = basename(pathname);
	char *dir = NULL;

	if(strcmp(base, path))
	{
		dir = dirname(path);
	}

	int dev;
	int parentino = getino(&dev, dir);
	if(parentino == -1)
	{
		printf("Could not find parent.\n");
		return -1;
	}

	MINODE *pip = iget(dev, parentino);
	if((pip->INODE.i_mode & 0x4000) != 0x4000)
	{
		iput(pip);
		printf("Parent is not a directory.\n");
		return -1;
	}

	// remove direntry
	int rem = removeDirEntry(pip, base);
	iput(pip);
    if (rem == -1)
    {
        printf("Unabe to remove link.\n");
        return -1;
    }
    return 0;
}

int do_symlink(char *pathname)
{
	char *patha = strtok(pathname, " ");
	char *pathb = strtok(NULL, " ");

	char tmpa[strlen(patha) + 1];
	strcpy(tmpa, patha);

	int dev;
	int ino = getino(&dev, patha);
	if(ino == -1)
	{
		printf("Invalid source path.\n");
		return -1;
	}
    MINODE *mipsrc = iget(dev, ino);

	if((mipsrc->INODE.i_mode & 0x8000) != 0x8000 && (mipsrc->INODE.i_mode & 0x4000) != 0x4000 )
	{
		iput(mipsrc);
		printf("Source is not a regular file or a directory.\n");
		return -1;
	}
	
	char path[strlen(pathb) + 1];
	strcpy(path, pathb);
	char *base = basename(pathb);
	char *dir = NULL;
	if(strcmp(base, path))
	{
		dir = dirname(path);
	}

	ino = getino(&dev, dir);
	if(ino == -1)
	{
		iput(mipsrc);
		printf("Invalid target path\n");
		return -1;
	}
    MINODE *mipparent = iget(dev, ino);
	if((mipparent->INODE.i_mode & 0x4000) != 0x4000)
	{
		iput(mipsrc);
		iput(mipparent);
		printf("Target path is not a directory.\n");
		return -1;
	}

	ino = ialloc(dev);
	createDirEntry(mipparent, ino, EXT2_FT_SYMLINK, base);
	createInode(dev, ino, SYM_MODE, PROCS[0].uid, PROCS[0].gid);
	MINODE *mip = iget(dev, ino);
	mip->INODE.i_links_count++;
	strcpy((char *)mip->INODE.i_block, tmpa);

	iput(mip);
	iput(mipsrc);
	iput(mipparent);
	
    return 0;
}


void quit()
{
	for(int i = 0; i < NMINODES; ++i)
	{
		if(MINODES[i].refCount > 0 && MINODES[i].dirty)
		{
			MINODES[i].refCount = 1;
			iput(&MINODES[i]);
		}
	}
}

//--------------------------------------------------------------------------
// LEVEL 2


//--------------------------------------------------------------------------
// LEVEL 3


//--------------------------------------------------------------------------
// MAIN

// print clean menu
int do_menu(char *pathname)
{
    if (pathname == NULL)
    {
        int i = 0;
        printf("/**************************** MENU ****************************/\n");
        while (strcmp(command[i],"0") != 0)
        {
            printf("\t%s ", command[i]);
            if((i+1) % 6 == 0)
            {
                printf("\n");
            }
            ++i;
        }
        printf("\n/**************************************************************/\n\n");
        return 0;
    }
    else
    {
        printf("Your doing it wrong.\n");
        return -1;
    }
}

// set main loop to bail
int do_exit(char *pathname)
{
	mainloop = false;
    return 0;
}


// check if typed in is a valid command
// call command from array of functions
int iscommand(char *input)
{
	int len = strlen(input);
	input[len - 1] = '\0';

    char* cmd = strtok(input, " ");
	char* arg = strtok(NULL, "");

    for (int i = 0; i < 18; ++i)    // change to while strcmp() != 0
    {
        if (!strcmp(cmd,command[i]))
        {
            return (*function[i])(arg);
        }
    }
    printf("Invalid command, please try again.\n");
    return -1;
}


int main(int argc, const char *argv[])
{
    char input[256];
    bool loop = true;

    if(argc == 1)
    {
        printf("What device should be mounted as root directory:\n");
        fgets(input, 256, stdin);
        printf("\nmanual input = %s\n", input);
        /* I found out that there's an extra "/" in 'input' when passed
         * into main. Also, this needs to be directed out of the program
         * and onto the main hd to locate the file
         */
    }
    else
    {
        strcpy(input, argv[1]);
        printf("\ninput = %s\n", input);
    }
    
	// Initialize and Mount root directory
    init();
	mount_root(input);
    do_menu(NULL);

    while(mainloop)
    {
        printf("$ ");
        fgets(input, 256, stdin);
        int command = iscommand(input);
    }
	quit();
    return EXIT_SUCCESS;
}

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "vsfs.h"

// Global Variables
char disk_name[128];   // name of virtual disk file
int  disk_size;        // size in bytes - a power of 2
int  disk_fd;          // disk file handle
int  disk_blockcount;  // block count on disk


/* 
   Reads block blocknum into buffer buf.
   You will not modify the getblock() function. 
   Returns -1 if error. Should not happen.
*/
int getblock (int blocknum, void *buf)
{      
	int offset, n; 
	
	if (blocknum >= disk_blockcount) 
		return (-1); //error

	offset = lseek (disk_fd, blocknum * BLOCKSIZE, SEEK_SET); 
	if (offset == -1) {
		printf ("lseek error\n"); 
		exit(0); 

	}

	n = read (disk_fd, buf, BLOCKSIZE); 
	if (n != BLOCKSIZE) 
		return (-1); 

	return (0); 
}


/*  
    Puts buffer buf into block blocknum.  
    You will not modify the putblock() function
    Returns -1 if error. Should not happen. 
*/
int putblock (int blocknum, void *buf)
{
	int offset, n;
	
	if (blocknum >= disk_blockcount) 
		return (-1); //error

	offset = lseek (disk_fd, blocknum * BLOCKSIZE, SEEK_SET);
	if (offset == -1) {
		printf ("lseek error\n"); 
		exit (1); 
	}
	
	n = write (disk_fd, buf, BLOCKSIZE); 
	if (n != BLOCKSIZE) 
		return (-1); 

	return (0); 
}

/* 
   IMPLEMENT THE FUNCTIONS BELOW - You can implement additional 
   internal functions. 
 */

//Represents one entry in the fat table
typedef int fat_entry_t;

//Represents one entry in the directory
typedef struct DirectoryEntry{
	char filename[MAXFILENAMESIZE];
	int filesize;
	
	int first_block;
	int last_block;
} directory_entry_t;

//Represents entry in the open file table
typedef struct FileEntry
{
	directory_entry_t* entry;
	int current_block_index;
	int current_inblock_index;
	
} file_entry_t;

//Control blocks holds varius data for the file system
//Some data used here is just for simplicity 
typedef struct ControlBlock
{
	int empty_block;
	
} control_block_t;



directory_entry_t* directory; 
fat_entry_t* fat;
file_entry_t* openFileTable;
control_block_t control_block;
int fat_length;
int directory_length;

#define CONTROL_BLOCK_LOCATION 0
#define DIRECTORY_LOCATION 1
#define FAT_LOCATION ((directory_length*sizeof(directory_entry_t)-1)/BLOCKSIZE + 2)

int write_blocks(int block_index,void* item,int size)
{
	unsigned char* data = (unsigned char*) item;
	
	int blockCount = size/BLOCKSIZE;
	int remaining  = size%BLOCKSIZE;
	
	int i;
	
	for (i = 0 ; i < blockCount; i++)
	{
		putblock(i + block_index,(void*)(data + i * BLOCKSIZE));
	}

	
	if (remaining != 0)
	{
		unsigned char buffer[BLOCKSIZE] = {0};
	
		for (i = 0 ; i < remaining ; i++)
			buffer[i] = data[i+blockCount*BLOCKSIZE];
		
		putblock(block_index+blockCount,(void*)buffer);
	}
	
	return 0;
}

int read_blocks(int block_index,void* item,int size)
{
	unsigned char* data = (unsigned char*) item;
	
	int blockCount = size/BLOCKSIZE;
	int remaining  = size%BLOCKSIZE;
	
	int i;
	
	for (i = 0 ; i < blockCount; i++)
	{
		getblock(i + block_index,(void*)(data + i * BLOCKSIZE));
	}

	
	if (remaining != 0)
	{
		unsigned char buffer[BLOCKSIZE] = {0};
		
		getblock(block_index+blockCount,(void*)buffer);
		
		for (i = 0 ; i < remaining ; i++)
			data[i+blockCount*BLOCKSIZE] = buffer[i];
	}
	
	return 0;
}


int writeFAT()
{
	write_blocks(FAT_LOCATION,fat,fat_length*sizeof(fat_entry_t));
	
	return 0;
}

int writeDirectory()
{
	write_blocks(DIRECTORY_LOCATION,directory,directory_length*sizeof(directory_entry_t));
	
	return 0;
}

int writeControlBlock()
{
	write_blocks(CONTROL_BLOCK_LOCATION,&control_block,sizeof(control_block_t));
	
	return 0;
}

int readControlBlock()
{	
	read_blocks(CONTROL_BLOCK_LOCATION,&control_block,sizeof(control_block_t));
	return 0;
}

int readDirectory()
{
	read_blocks(DIRECTORY_LOCATION,directory,directory_length*sizeof(directory_entry_t));

	return 0;
}

int readFAT()
{
	read_blocks(FAT_LOCATION,fat,fat_length*sizeof(fat_entry_t));
	return 0;
}



/* format disk of size dsize */
int vsfs_format(char *vdisk, int dsize)
{
	strcpy (disk_name, vdisk); 
	disk_size = dsize;  
	disk_blockcount = disk_size / BLOCKSIZE; 

	disk_fd = open (disk_name, O_RDWR); 
	if (disk_fd == -1) {
		printf ("disk open error %s\n", vdisk); 
		exit(1); 
	}
	
	// perform your format operations here. 
	printf ("formatting disk=%s, size=%d\n", vdisk, disk_size); 
	
	//Find length of the fat and directory tables
	fat_length =  disk_blockcount;
	directory_length = MAXFILECOUNT;
	
	//Create and write control_block
	control_block.empty_block = FAT_LOCATION + (fat_length - 1)/BLOCKSIZE + 1;
	writeControlBlock();
	
	//Create Directory
	directory = malloc(directory_length*sizeof(directory_entry_t));
	
	int i;
	for (i = 0 ; i < directory_length ; i++)
	{
		directory[i].filesize = -1;
		directory[i].first_block = -1;
		directory[i].last_block = -1;
	}
	
	if (writeDirectory() == -1)
		return -1;
	
	free(directory);
	directory = NULL;
	
	//Create FAT structure
	fat = malloc(fat_length*sizeof(fat_entry_t));
	
	for (i = 0 ; i < control_block.empty_block - 1; i++)
		fat[i] = -1;
	for (i = 0 ; i < fat_length ; i++)
		fat[i] = i+1;
	
	fat[fat_length - 1] = -1;
	
	if (writeFAT() == -1)
		return -1;
	
	free(fat);
	fat = NULL;
	
	fsync (disk_fd); 
	close (disk_fd); 

	return (0); 
}


/* 
   Mount disk and its file system. This is not the same mount
   operation we use for real file systems:  in that the new filesystem
   is attached to a mount point in the default file system. Here we do
   not do that. We just prepare the file system in the disk to be used
   by the application. For example, we get FAT into memory, initialize
   an open file table, get superblock into into memory, etc.
*/
int vsfs_mount (char *vdisk)
{
	struct stat finfo; 

	strcpy (disk_name, vdisk);
	disk_fd = open (disk_name, O_RDWR); 
	if (disk_fd == -1) {
		printf ("vsfs_mount: disk open error %s\n", disk_name); 
		exit(1); 
	}
	
	fstat (disk_fd, &finfo); 

	printf ("vsfs_mount: mounting %s, size=%d\n", disk_name, 
		(int) finfo.st_size);  
	disk_size = (int) finfo.st_size; 
	disk_blockcount = disk_size / BLOCKSIZE; 

	// perform your mount operations here
	
	//Calculate length of the fat and directory tables
	fat_length =  disk_blockcount;
	directory_length = MAXFILECOUNT;
	
	//Move directory and fat tables into memory
	directory = malloc(directory_length*sizeof(directory_entry_t));
	fat = malloc(fat_length*sizeof(fat_entry_t));
	readControlBlock();
	readFAT();
	readDirectory();
	
	//Init open file table
	openFileTable = malloc(sizeof(file_entry_t) * MAXOPENFILES);
	
	int i = 0;
	for (i = 0 ; i < MAXOPENFILES ; i++)
	{
		openFileTable[i].entry = NULL;
	}
	
  	return (0); 
}


int vsfs_umount()
{
	// perform your unmount operations here

	// write your code
	writeFAT();
	writeDirectory();
	
	free(fat);
	free(directory);
	free(openFileTable);
	
	fat = NULL;
	directory = NULL;
	openFileTable = NULL;
	
	fsync (disk_fd); 
	close (disk_fd); 
	return (0); 
}


/* create a file with name filename */
int vsfs_create(char *filename)
{
	//Find empty file in directory
	int i;
	
	//If file already exists return -1
	for (i = 0 ; i < directory_length ;i++)
	{
		if (strcmp(directory[i].filename,filename) == 0)
			return -1;
	}
	
	//Find first empty spot in the directory
	for (i = 0 ; i < directory_length && directory[i].filesize != -1;i++);
	
	//If cannot find return -1 -> file count > MAXFILECOUNT
	if (i == directory_length)
		return -1;
	
	//Create directory entry for the file
	strcpy(directory[i].filename,filename);
	directory[i].filesize = 0;
	directory[i].first_block = -1;
	directory[i].last_block = -1;
	
	writeDirectory(); //Save directory into disk (file)

	return (0); 
}


/* open file filename */
int vsfs_open(char *filename)
{
	//int index = -1; 
	int i;
	
	//Find file in the directory
	for (i = 0 ; i < directory_length ;i++)
	{
		if (strcmp(directory[i].filename,filename) == 0)
			break;
	}
	
	//If file DNE -> return -1
	if (i == directory_length)
		return -1;
	
	//Find a spot in the open file table
	int j = 0;
	for (j = 0 ; j < MAXOPENFILES && openFileTable[j].entry != NULL ; j++);
	
	//If cannot find empty spot return -1. => maxopenfiles exceeded
	if (j == MAXOPENFILES)
		return -1;
	
	//Put data to the open file table
	openFileTable[j].entry = &directory[i];
	openFileTable[j].current_block_index = directory[i].first_block;
	openFileTable[j].current_inblock_index = 0;
	
	return (j); 
}

/* close file filename */
int vsfs_close(int fd)
{
	if (fd > MAXOPENFILES || fd < 0 || openFileTable[fd].entry == NULL)
		return -1;
	
	//if entry in the open file table null == file is not opened
	openFileTable[fd].entry = NULL;

	return (0); 
}

int vsfs_delete(char *filename)
{
	int i;
	
	//Find file in directory
	for (i = 0 ; i < directory_length ;i++)
	{
		if (strcmp(directory[i].filename,filename) == 0)
			break;
	}
	
	//If file DNE return -1
	if (i == directory_length)
		return -1;
	
	
	//If file is open, it cannot be deleted. :|
	int k;
	for (k = 0 ; k < MAXOPENFILES ; k++)
	{
		if (openFileTable[k].entry == &directory[i])
			return -1;
	}
	
	//Filesize == -1 -> no file
	directory[i].filesize = -1;
	
	if (directory[i].first_block == -1)
	{
		writeDirectory();
		return 0;
	}
	
	//Find the first and last block of the file
	int first = directory[i].first_block;
	int last = directory[i].last_block;
	
	//Insert (Push) blocks of the file into empty block list
	fat[last] = control_block.empty_block;
	control_block.empty_block = first;
	
	//Save fat and directory into disk
	writeDirectory();
	writeFAT();
	return (0);
}

void memory_copy(const unsigned char* src ,int src_from, unsigned char* dest, int dest_from, int length)
{
	int i;
	for (i = 0 ; i < length ; i++)
		dest[dest_from+i] = src[i+src_from];
}

int vsfs_read(int fd, void *buf, int n)
{
	unsigned char rb[BLOCKSIZE];
	unsigned char* buffer = (unsigned char*)buf;
	int bytes_read = 0; 
	
	if (fd > MAXOPENFILES || fd < 0 || openFileTable[fd].entry == NULL)
		return -1;
	
	if (n <= 0)
		return 0;
	
	if (openFileTable[fd].entry->first_block == -1)
		return 0;
	
	if (openFileTable[fd].current_block_index == -1)
		return 0;
	
	if (openFileTable[fd].current_block_index == openFileTable[fd].entry->last_block &&
			openFileTable[fd].current_inblock_index >= openFileTable[fd].entry->filesize % BLOCKSIZE)
		return 0;
	
	while (n > bytes_read)
	{
		if (openFileTable[fd].current_inblock_index == BLOCKSIZE)
		{
			openFileTable[fd].current_block_index = fat[openFileTable[fd].current_block_index];
			openFileTable[fd].current_inblock_index = 0;
		}
		
		if (openFileTable[fd].current_inblock_index == -1)
		{
			openFileTable[fd].current_block_index = -1;
			return 0;
		}
		getblock(openFileTable[fd].current_block_index,rb);
		int index = openFileTable[fd].current_inblock_index;
		
		int length = (BLOCKSIZE - index < n - bytes_read)?BLOCKSIZE - index:n - bytes_read;
		
		if (openFileTable[fd].current_block_index == openFileTable[fd].entry->last_block)
			length = (length < openFileTable[fd].entry->filesize % BLOCKSIZE)?(length):(openFileTable[fd].entry->filesize % BLOCKSIZE);
		
		memory_copy(rb,index,buffer,bytes_read,length);
		openFileTable[fd].current_inblock_index += length;
		bytes_read += length;
	}
	
	return (bytes_read);
}

int expand(int fd)
{
	int block = control_block.empty_block;
	
	if (block == -1)
		return -1;
	
	control_block.empty_block = fat[block];
	
	if (openFileTable[fd].entry->last_block == -1)
	{
		openFileTable[fd].entry->last_block = block;
		openFileTable[fd].entry->first_block = block;
		openFileTable[fd].current_block_index = block;
		openFileTable[fd].current_inblock_index = 0;
		openFileTable[fd].entry->filesize = 0;
		fat[block] = -1;
	}
	else
	{
		fat[openFileTable[fd].entry->last_block] = block;
		openFileTable[fd].entry->last_block = block;
		fat[block] = -1;
	}
	return 0;
}

int vsfs_write(int fd, void *buf, int n)
{
	unsigned char wb[BLOCKSIZE] = {0};
	unsigned char* buffer = (unsigned char*)buf;
	
	if (fd > MAXOPENFILES || fd < 0 || openFileTable[fd].entry == NULL)
		return -1;
	
	int bytes_written = 0;
	
	if (n <= 0)
		return 0;
	if (openFileTable[fd].entry->filesize == -1)
		openFileTable[fd].entry->filesize = 0;
	
	while (n>bytes_written)
	{
		if (openFileTable[fd].entry->filesize % BLOCKSIZE == 0)
		{
			if (expand(fd) == -1)
				return -1;
		}
		else
		{
			getblock(openFileTable[fd].entry->last_block,wb);
		}
		
		int block = openFileTable[fd].entry->last_block;
		
		int index= openFileTable[fd].entry->filesize % BLOCKSIZE;
		int length = (BLOCKSIZE - index< (n - bytes_written))?(BLOCKSIZE - index):(n-bytes_written);
		
		memory_copy(buffer,bytes_written,wb,index,length);
		
		putblock(block,wb);
		
		bytes_written += length;
		
		openFileTable[fd].entry->filesize +=length;
	}
	
	writeDirectory();
	writeFAT();
	
	return bytes_written;
} 

int vsfs_truncate(int fd, int size)
{
	if (fd > MAXOPENFILES || fd < 0 || openFileTable[fd].entry == NULL)
		return -1;
	
	
	if (openFileTable[fd].entry->filesize < size)
	{
		size = openFileTable[fd].entry->filesize;
	}
	int temp = size;
		
	int cur_block = openFileTable[fd].entry->first_block;
	
	if (cur_block == -1)
	{
		return 0;
	}
	
	if (size == 0)
	{
		fat[openFileTable[fd].entry->last_block] = control_block.empty_block;
		control_block.empty_block = openFileTable[fd].entry->first_block;
		
		openFileTable[fd].entry->first_block = -1;
		openFileTable[fd].entry->last_block = -1;
	}
	else
	{
		int prev_block = -1;
		
		while (cur_block != -1 && size > 0)
		{
			size -= BLOCKSIZE;
			
			prev_block = cur_block;
			cur_block = fat[cur_block];
		}
		
		fat[openFileTable[fd].entry->last_block] = control_block.empty_block;
		control_block.empty_block = cur_block;
		
		openFileTable[fd].entry->last_block = prev_block;
		fat[prev_block] = -1;
	}
	
	
	openFileTable[fd].entry->filesize = temp;
	vsfs_seek(fd,temp);
	
	writeFAT();
	writeDirectory();
	return (0); 
} 


int vsfs_seek(int fd, int offset)
{
	if (fd > MAXOPENFILES || fd < 0 || openFileTable[fd].entry == NULL)
		return -1;
	
	if (openFileTable[fd].entry->filesize < offset)
		offset = openFileTable[fd].entry->filesize;

	
	int cur_block = openFileTable[fd].entry->first_block;
	
	if (cur_block == -1)
		return 0;
	
	while (offset>BLOCKSIZE)
	{
		cur_block = fat[cur_block];
		offset -= BLOCKSIZE;
	}
	
	int cur_index = 0;
	if (offset != 0)
		cur_index += offset;
	
	if (cur_index > BLOCKSIZE)
	{
		cur_block = fat[cur_block];
		cur_index -= BLOCKSIZE;
	}
	
	openFileTable[fd].current_block_index = cur_block;
	openFileTable[fd].current_inblock_index = cur_index;
	
	
	return offset;
} 

int vsfs_filesize (int fd)
{
	if (fd > MAXOPENFILES || fd < 0 || openFileTable[fd].entry == NULL)
		return -1;
	
	return openFileTable[fd].entry->filesize;
}


void vsfs_print_dir ()
{

	// write your code
	directory_entry_t* cur = directory;
	
	int i = 0;
	for (i = 0 ; i < directory_length ; i++)
	{
		if (cur[i].filesize != -1)
			printf("%s : %d bytes\n",cur[i].filename,cur[i].filesize );
	}
		
}


void vsfs_print_fat ()
{
	directory_entry_t* cur = directory;
	
	int i = 0;
	for (i = 0 ; i < directory_length ; i++)
	{
		if (cur[i].filesize != -1)
		{
			int cur_block = cur[i].first_block;
			printf("%s ",cur[i].filename);
			
			while (cur_block != -1)
			{
				printf("%d ",cur_block);
				cur_block = fat[cur_block];
			}
			printf("\n");
		}
	}
}



#ifndef BVFS_H
#define BVFS_H

/* CMSC 432 - Homework 7
 * Assignment Name: bvfs - the BV File System
 * Due: Thursday, November 21st @ 11:59 p.m.
 */


/*
 * [Requirements / Limitations]
 *   Partition/Block info
 *     - Block Size: 512 bytes
 *     - Partition Size: 8,388,608 bytes (16,384 blocks)
 *
 *   Directory Structure:
 *     - All files exist in a single root directory
 *     - No subdirectories -- just names files
 *
 *   File Limitations
 *     - File Size: Maximum of 65,536 bytes (128 blocks)
 *     - File Names: Maximum of 32 characters including the null-byte
 *     - 256 file maximum -- Do not support more
 *
 *   Additional Notes
 *     - Create the partition file (on disk) when bv_init is called if the file
 *       doesn't already exist.
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>

/* To get local time.
   time_t mytime;
   mytime = time(NULL);
   printf(ctime(&mytime));
*/

/* BEGIN STRUCT STUFF */

struct INode {
  char *fileName;
  time_t time_stamp;
  int num_bytes;
  //short num_blocks;
  short *blocks;
};

struct SuperBlockInfo {
  short offsets[255];
  short next;
};

/* END STRUCT STUFF */



//Globals for hard set limitations
int FILE_AMOUNT = 256;

// BYTES
int FILENAME_SIZE = 32;
int BLOCK_SIZE = 512;
int PARTITION_SIZE = 8388608;
int FILE_SIZE = 65536;

struct SuperBlockInfo super_blocks[64];
struct INode INode_array[256];

// Prototypes
int bv_init(const char *fs_fileName);
int bv_destroy();
int bv_open(const char *fileName, int mode);
int bv_close(int bvfs_FD);
int bv_write(int bvfs_FD, const void *buf, size_t count);
int bv_read(int bvfs_FD, void *buf, size_t count);
int bv_unlink(const char* fileName);
void bv_ls();



/*
 * int bv_init(const char *fs_fileName);
 *
 * Initializes the bvfs file system based on the provided file. This file will
 * contain the entire stored file system. Invocation of this function will do
 * one of two things:
 *
 *   1) If the file (fs_fileName) exists, the function will initialize in-memory
 *   data structures to help manage the file system methods that may be invoked.
 *
 *   2) If the file (fs_fileName) does not exist, the function will create that
 *   file as the representation of a new file system and initialize in-memory
 *   data structures to help manage the file system methods that may be invoked.
 *
 * Input Parameters
 *   fs_fileName: A c-string representing the file on disk that stores the bvfs
 *   file system data.
 *
 * Return Value
 *   int:  0 if the initialization succeeded.
 *        -1 if the initialization failed (eg. file not found, access denied,
 *           etc.). Also, print a meaningful error to stderr prior to returning.
 */
int bv_init(const char *fs_fileName) {

  int fsFD = open(fs_fileName, O_CREAT | O_RDWR | O_EXCL, 0644);


  if(fsFD < 0){
    if(errno == EEXIST){
      // TODO: The files already exists so initialize all the necessary datastructures.

      // Make the file our partition size.
      lseek(fsFD, PARTITION_SIZE-3, SEEK_SET);
      write(fsFD, "0", sizeof("0"));

      // Seek back to the beginning of the file to begin writing our metadata.
      lseek(fsFD, 0, SEEK_SET);

      // Create super block linked list

      write(fsFD, (void*)INode_array, sizeof(INode_array)/sizeof(INode_array[0]));
      return 0;
    } else if(errno == EACCES) {
      // We got a permission denied error. TODO print meaningful error message.
      return -1;
    }
  } else {
    // TODO: The file does not exist so create the file to represent the File System.
    // Good news is that opeinging with the O_CREAT tag means that it is already created when we get here.
    // Then initialize all the necessary datastructures. 

    // Make the file our partition size.
    lseek(fsFD, PARTITION_SIZE-3, SEEK_SET);
    write(fsFD, "0", sizeof("0"));

    // Seek back to the beginning of the file to begin writing our metadata.
    lseek(fsFD, 0, SEEK_SET);
  
    // Create the Super Block which points to empty blocks.
    short offset = 257;
    short tracker;
    short block = 0;
    int loc = 0;

    printf("Size of tracker: %lu\n", sizeof(tracker));

    for(short x = 0; x < 255; x++){
      tracker = offset+x;
      write(fsFD, (void *)(&tracker), sizeof(tracker));
      printf("Writing: %d\n", tracker);
      super_blocks[loc].offsets[x] = offset+x;

      // Last 2 bytes before block end goes to next superblock
      if(x == 254){
        tracker = offset+x+1;
        write(fsFD, (void *)(&tracker), sizeof(tracker));
        super_blocks[loc].next = offset+x+1;
      }

    }
/*
    loc++;
    block = 512;

    // Write the INode array to file
    write(fsFD, INode_array, sizeof(INode_array));

    // Outer loop goes through all necessary superblocks
    for(short i = block; i < 16384; i += 257){ 
      
      // Inner loop to assign each empty block to a superblock
      for(short x = 1; x < 256; x++){
        write(fsFD, block+x, 2);
        printf("Writing: %d\n", block+x);
        super_blocks[loc].offsets[x] = block+x;

        // Last 2 bytes before block end goes to next superblock
        if(x == 255){
          write(fsFD, block+x+1, 2);
          super_blocks[loc].next = block+x+1;
        }

      }

      // Writing at position 0 here so we need to jump by 129+255 to our next superblock
      block += 257;
      loc++;
      lseek(fsFD, block*BLOCK_SIZE, SEEK_SET);
    }
  */  
    return 0;
  }
}





/*
 * int bv_destroy();
 *
 * This is your opportunity to free any dynamically allocated resources and
 * perhaps to write any remaining changes to disk that are necessary to finalize
 * the bvfs file before exiting.
 *
 * Return Value
 *   int:  0 if the clean-up process succeeded.
 *        -1 if the clean-up process failed (eg. bv_init was not previously,
 *           called etc.). Also, print a meaningful error to stderr prior to
 *           returning.
 */
int bv_destroy() {
}







// Available Modes for bvfs (see bv_open below)
int BV_RDONLY = 0;
int BV_WCONCAT = 1;
int BV_WTRUNC = 2;

/*
 * int bv_open(const char *fileName, int mode);
 *
 * This function is intended to open a file in either read or write mode. The
 * above modes identify the method of access to utilize. If the file does not
 * exist, you will create it. The function should return a bvfs file descriptor
 * for the opened file which may be later used with bv_(close/write/read).
 *
 * Input Parameters
 *   fileName: A c-string representing the name of the file you wish to fetch
 *             (or create) in the bvfs file system.
 *   mode: The access mode to use for accessing the file
 *           - BV_RDONLY: Read only mode
 *           - BV_WCONCAT: Write only mode, appending to the end of the file
 *           - BV_WTRUNC: Write only mode, replacing the file and writing anew
 *
 * Return Value
 *   int: >=0 Greater-than or equal-to zero value representing the bvfs file
 *           descriptor on success.
 *        -1 if some kind of failure occurred. Also, print a meaningful error to
 *           stderr prior to returning.
 */
int bv_open(const char *fileName, int mode) {
}






/*
 * int bv_close(int bvfs_FD);
 *
 * This function is intended to close a file that was previously opened via a
 * call to bv_open. This will allow you to perform any finalizing writes needed
 * to the bvfs file system.
 *
 * Input Parameters
 *   fileName: A c-string representing the name of the file you wish to fetch
 *             (or create) in the bvfs file system.
 *
 * Return Value
 *   int:  0 if open succeeded.
 *        -1 if some kind of failure occurred (eg. the file was not previously
 *           opened via bv_open). Also, print a meaningful error to stderr
 *           prior to returning.
 */
int bv_close(int bvfs_FD) {
}







/*
 * int bv_write(int bvfs_FD, const void *buf, size_t count);
 *
 * This function will write count bytes from buf into a location corresponding
 * to the cursor of the file represented by bvfs_FD.
 *
 * Input Parameters
 *   bvfs_FD: The identifier for the file to write to.
 *   buf: The buffer containing the data we wish to write to the file.
 *   count: The number of bytes we intend to write from the buffer to the file.
 *
 * Return Value
 *   int: >=0 Value representing the number of bytes written to the file.
 *        -1 if some kind of failure occurred (eg. the file is not currently
 *           opened via bv_open). Also, print a meaningful error to stderr
 *           prior to returning.
 */
int bv_write(int bvfs_FD, const void *buf, size_t count) {
}






/*
 * int bv_read(int bvfs_FD, void *buf, size_t count);
 *
 * This function will read count bytes from the location corresponding to the
 * cursor of the file (represented by bvfs_FD) to buf.
 *
 * Input Parameters
 *   bvfs_FD: The identifier for the file to read from.
 *   buf: The buffer that we will write the data to.
 *   count: The number of bytes we intend to write to the buffer from the file.
 *
 * Return Value
 *   int: >=0 Value representing the number of bytes written to buf.
 *        -1 if some kind of failure occurred (eg. the file is not currently
 *           opened via bv_open). Also, print a meaningful error to stderr
 *           prior to returning.
 */
int bv_read(int bvfs_FD, void *buf, size_t count) {
}







/*
 * int bv_unlink(const char* fileName);
 *
 * This function is intended to delete a file that has been allocated within
 * the bvfs file system.
 *
 * Input Parameters
 *   fileName: A c-string representing the name of the file you wish to delete
 *             from the bvfs file system.
 *
 * Return Value
 *   int:  0 if the delete succeeded.
 *        -1 if some kind of failure occurred (eg. the file does not exist).
 *           Also, print a meaningful error to stderr prior to returning.
 */
int bv_unlink(const char* fileName) {
}







/*
 * void bv_ls();
 *
 * This function will list the contests of the single-directory file system.
 * First, you must print out a header that declares how many files live within
 * the file system. See the example below in which we print "2 Files" up top.
 * Then display the following information for each file listed:
 *   1) the file size in bytes
 *   2) the number of blocks occupied within bvfs
 *   3) the time and date of last modification (derived from unix timestamp)
 *   4) the name of the file.
 * An example of such output appears below:
 *    | 2 Files
 *    | bytes:  276, blocks: 1, Tue Nov 14 09:01:32 2017, bvfs.h
 *    | bytes: 1998, blocks: 4, Tue Nov 14 10:32:02 2017, notes.txt
 *
 * Hint: #include <time.h>
 * Hint: time_t now = time(NULL); // gets the current unix timestamp (32 bits)
 * Hint: printf("%s\n", ctime(&now));
 *
 * Input Parameters
 *   None
 *
 * Return Value
 *   void
 */
void bv_ls() {
}

#endif

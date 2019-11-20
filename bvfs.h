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
#include <string.h>
#include <math.h>

/* BEGIN STRUCT STUFF */

struct INode {
  char file_name[32];
  time_t time_stamp;
  int num_bytes;
  short file_blocks[128];
  short padding[106];
};

struct SuperBlockInfo {
  short offsets[255];
  short next;
  short is_empty;
};

struct Cursor {
  const char *file_name;
  short file_block;
  int pos;
  int block;
  int mode;
};

/* END STRUCT STUFF */



//Globals for hard set limitations
int FILE_AMOUNT = 256;

// BYTES
int FILENAME_SIZE = 32;
int BLOCK_SIZE = 512;
int PARTITION_SIZE = 8388608;
int FILE_SIZE = 65536;

int INITIALIZED = 0;
int fsFD;
struct SuperBlockInfo super_block;
short current_super_block;
struct INode INode_array[256];
const char *current_open_file = (const char *)'\0';
struct Cursor cursors[256];

// Blank Stuff
static const struct INode blank_INode = {{0}, 0, 0, {0}, {0}};
static const struct Cursor blank_cursor = {0, 0, 0, 0};
static const struct SuperBlockInfo blank_super_block = {{0}, 0, 0};

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

  fsFD = open(fs_fileName, O_CREAT | O_RDWR | O_EXCL, 0644);

  printf("%lu\n", sizeof(blank_INode));

  if(fsFD < 0){
    if(errno == EEXIST){

      close(fsFD);

      fsFD = open(fs_fileName, O_RDWR | O_EXCL, 0644);

      // Read in the current super block that we are using.
      read(fsFD, (void*)(&current_super_block), 2);

      // Go to the first super block and read it into memory.
      lseek(fsFD, (current_super_block-1)*512, SEEK_SET);

      short num;
      for(int j = 0; j < 256; j ++){
        // Read in the int at that location
        read(fsFD, (void *)(&num), sizeof(num));

        // Push that int into the specified position in the specific location in the super_blocks array.
        super_block.offsets[j] = num;
      }

      // The last number in a super block is a reference to the next super block. Read it and go there.
      read(fsFD, (void *)(&num), sizeof(num));
      super_block.next = num;
      super_block.is_empty = 0;
        
      // Seek back to start of INodes and read in information to populate our array.

      lseek(fsFD, 512, SEEK_SET);

      // Start reading in and filling up our INode array.
      read(fsFD, &INode_array, sizeof(INode_array));

      // File system was initialized correctly.
      INITIALIZED = 1;

      return 0;
    } else if(errno == EACCES) {
      fprintf(stderr, "Unable to access file due to file permissions.");
      return -1;
    }
  } else {
    // Good news is that opeinging with the O_CREAT tag means that it is already created when we get here.
    // Then initialize all the necessary datastructures. 

    // Make the file our partition size.
    lseek(fsFD, PARTITION_SIZE-3, SEEK_SET);
    write(fsFD, "0", sizeof("0"));

    // Seek back to the beginning of the file to begin writing our metadata.
    lseek(fsFD, 0, SEEK_SET);

    // Create the Super Block which points to the first super block.
    short block = 257;
    current_super_block = 257;
    write(fsFD, (short *)(&block), 2);
  
    // Make our INode array with blank INode structs.
    for(int i =0; i < sizeof(INode_array)/sizeof(INode_array[0]); i++){
      INode_array[i] = blank_INode;
    }

    // Make our Cursor array with blank Cursor structs.
    for(int i =0; i < sizeof(cursors)/sizeof(cursors); i++){
      cursors[i] = blank_cursor;
    }


    // Some information to help us with the blocks.
    short tracker;
    int loc = 0;

    lseek(fsFD, (block-1)*BLOCK_SIZE, SEEK_SET);

    // Outer loop goes through all necessary superblocks
    for(short i = block; i < 16384; i += 256){ 
      
      // Inner loop to assign each empty block to a superblock
      for(short x = 1; x < 256; x++){
        tracker = block+x;
        write(fsFD, (void *)(&tracker), 2);
        // DEBUG OUTPUT
        //printf("Writing: %d\n", tracker);
        if(loc == 0) { super_block.offsets[x] = tracker; }

        // Last 2 bytes before block end goes to next superblock
        if(x == 255){
          tracker = block+x+1;
          write(fsFD, (void *)(&tracker), 2);
          if(loc == 0) {
            super_block.next = tracker;
            super_block.is_empty = 0;
          }
        }
      }

      // Writing at position 0 here so we need to jump by 129+255 to our next superblock
      block += 256;
      loc = 1;
      lseek(fsFD, (block-1)*BLOCK_SIZE, SEEK_SET);
    }

    // Initialize our first super block to use here.
    lseek(fsFD, (current_super_block-1)*512, SEEK_SET);

    short num;
    for(int j = 0; j < 256; j ++){
      // Read in the int at that location
      read(fsFD, (void *)(&num), sizeof(num));

      // Push that int into the specified position in the specific location in the super_blocks array.
      super_block.offsets[j] = num;
      //printf("%d\n", super_block.offsets[j]);
    }

    // File system was initialized correctly.
    INITIALIZED = 1;

    return 0;
  }

  fprintf(stderr, "Don't know how you got here. But you should be here.");
  return -1;
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
  /* We aren't mallocing any information so there is no need to free that up.
   We will also update all of our super blocks and INodes for every call to
   operation functions so there will be no need to write anything else to disk. */

  //Simply check to see if the filesystem was initialized correctly or not.

  if(INITIALIZED){
    // We need to write down the current super block to file.
    lseek(fsFD, 0, SEEK_SET);
    write(fsFD, &current_super_block, sizeof(current_super_block));

    // We also need to write down all the data that has changed in our currently used super block.
    lseek(fsFD, (current_super_block-1)*512, SEEK_SET);

    // Write down all the offsets
    for(int i = 0; i < sizeof(super_block.offsets)/sizeof(super_block.offsets[0]); i++){
      write(fsFD, &super_block.offsets[i], 2);
    }

    // Now write down the pointer to the next super block.
    write(fsFD, &super_block.next, 2);

    // Finally, write down all of our INode information to the file.
    lseek(fsFD, 512, SEEK_SET);
    write(fsFD, &INode_array, sizeof(INode_array));

    // Close the file.
    close(fsFD);
    printf("Thank you for using bv_fs.");
    return 0;
  }

  fprintf(stderr, "File system was never initialized with a call to bv_init().");
  return -1;

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
  /* Search through our INodes for the fileName in there.
     If we find it then we can "open" the file and write bytes to it where it lives
     otherwise we need to allocate a new INode with file information to use. */
  
  for(int i = 0; i < sizeof(INode_array)/sizeof(INode_array[0]); i++){

    // Check to see if fileNames match.
    if(strcmp(INode_array[i].file_name, fileName) == 0){
      // We've found the file which lives at this INode. We need to return a file descriptor to write to it.
      // Maybe we should just return the byte that we need to seek to as the file descriptor. That would make it
      // Easier to deal with.

      // CONCAT MODE / READ MODE
      if(mode == 1 || mode == 0){
        // We need to find the next block that can be written to.
        int bytes = INode_array[i].num_bytes;
        printf("%d\n", bytes);

        int loc = 0;
        for(int j = 1; j < sizeof(INode_array[i].file_blocks)/sizeof(INode_array[i].file_blocks[0]); j++){
          if(INode_array[i].file_blocks[j] == 0){
            break;
          }
          bytes -= 512;
          loc++;
        }

        // Create a cursor to go along with this file.
        for(int y = 0; y < sizeof(cursors)/sizeof(cursors[0]); y++){
          if(strcmp(cursors[y].file_name, fileName) == 0){
            cursors[y].file_name = fileName;
            cursors[y].block = INode_array[i].file_blocks[loc];

            if(mode == BV_WCONCAT){
              cursors[y].pos = (((INode_array[i].file_blocks[loc]-1)*512) + bytes);
              cursors[y].file_block = i;
            } else {
              cursors[y].pos = (((INode_array[i].file_blocks[0]-1)*512));
              cursors[y].file_block = 0;
            }

            cursors[y].mode = mode;
            break;
          }
        }
        
        // Set the file for current open file.
        current_open_file = fileName;

        return (((INode_array[i].file_blocks[loc]-1)*512) + bytes);
      }

      // TRUNCATE MODE
      if(mode == 2){
        // Give back all of the blocks that are now empty.
        short save = INode_array[i].file_blocks[0];

        for(int x = 1; x < sizeof(INode_array[i].file_blocks)/sizeof(INode_array[i].file_blocks[0]); x++){

          if(INode_array[i].file_blocks[x] != 0){
            int flag = 0;
            for(int y = 0; y < sizeof(super_block.offsets)/sizeof(super_block.offsets[0]); y++){
              if(super_block.offsets[y] == 0){
                flag = 1;
                super_block.is_empty = 0;
                super_block.offsets[y] = INode_array[i].file_blocks[x];
                break;
              }
            }
            // If there was no empty spots in the super block that means we need to make a new one.
            if(flag == 0){
              // We need to write back our super block to memory.
              lseek(fsFD, (current_super_block-1)*512, SEEK_SET);

              for(int z = 0; z < sizeof(super_block.offsets)/sizeof(super_block.offsets[0]); z++){
                write(fsFD, &super_block.offsets[z], 2);
              }

              super_block = blank_super_block;


              super_block.next = current_super_block;
              super_block.is_empty = 1;
              current_super_block = INode_array[i].file_blocks[x];
            }
          } else {
            break;
          }
        }

        INode_array[i] = blank_INode;
        INode_array[i].file_blocks[0] = save;
        time_t mytime = time(NULL);
        INode_array[i].time_stamp = mytime;
        strcpy(INode_array[i].file_name, fileName);

        // Create a cursor to go along with this file.
        for(int y = 0; y < sizeof(cursors)/sizeof(cursors[0]); y++){
          if(strcmp(cursors[y].file_name, fileName) == 0){
            cursors[y].file_name = fileName;
            cursors[y].block = save;
            cursors[y].pos = (save-1)*512;
            cursors[y].mode = mode;
            cursors[y].file_block = 0;
            break;
          }
        }

        // Set the file for current open file.
        current_open_file = fileName;

        return (save-1)*512;

      }
    }
  }

  // At this point we've exhausted our INode_array and the file does not exist in our filesystem already
  // so lets create it and put it at the lowest INode.

  if(mode == 0){
    fprintf(stderr, "File opened in BV_RDONLY mode. No file created.");
    return -1;
  }

  for(int i = 0; i < sizeof(INode_array)/sizeof(INode_array[0]); i++){
    if(INode_array[i].time_stamp == 0){
      // We've found the closest empty INode so lets put a file in there.
      // To do this we need to find the lowest empty block that we can write to.
      // That means loop through our super blocks till we found one!

      // Go through our offset array in each super block.
      for(int x = 0; x < sizeof(super_block.offsets)/sizeof(super_block.offsets[0]); x++){
      //printf("%d\n", super_block.offsets[x]);

        // This means that there is empty space at this offset!
        if(super_block.is_empty == 1){
          // We can just use this super block.
          //check to make sure fileName is not greater than 32 bytes.
          if(sizeof(*(fileName))>31){
            fprintf(stderr,"File name is too many characters");
            return -1;
          }

          strcpy(INode_array[i].file_name, fileName);
          INode_array[i].file_blocks[0] = current_super_block;
          current_super_block = super_block.next;

          // Seek to the next super block and read it into memory.
          lseek(fsFD, (current_super_block-1)*512, SEEK_SET);

          short num;
          for(int j = 0; j < 256; j ++){
            // Read in the int at that location
            read(fsFD, (void *)(&num), sizeof(num));

            // Push that int into the specified position in the specific location in the super_blocks array.
            super_block.offsets[j] = num;
          }

          // The last number in a super block is a reference to the next super block.
          read(fsFD, (void *)(&num), sizeof(num));
          super_block.next = num;
          super_block.is_empty = 0;

          // Get the time_stamp
          time_t mytime = time(NULL);
          INode_array[i].time_stamp = mytime;

          // Now make sure the super block there is NULL so nothing else can write to it.
          int fd = super_block.offsets[x];
          super_block.offsets[x] = 0;

          // Create a cursor to go along with this file.
          for(int y = 0; y < sizeof(cursors)/sizeof(cursors[0]); y++){
            if(cursors[y].file_name == NULL){
              cursors[y].file_name = fileName;
              cursors[y].block = fd;
              cursors[y].pos = (fd-1)*512;
              cursors[y].mode = mode;
              break;
            }
          }

          // Set the file for current open file.
          current_open_file = fileName;

          return (fd-1)*512;
         
        }


        else if(super_block.offsets[x] != 0){
          //check to make sure fileName is not greater than 32 bytes.
          if(sizeof(*(fileName))>31){
            fprintf(stderr,"File name is too many characters");
            return -1;
          }

          strcpy(INode_array[i].file_name, fileName);
          INode_array[i].file_blocks[0] = super_block.offsets[x];

          // Get the time_stamp
          time_t mytime = time(NULL);
          INode_array[i].time_stamp = mytime;

          // Now make sure the super block there is NULL so nothing else can write to it.
          int fd = super_block.offsets[x];
          super_block.offsets[x] = 0;

          // Check to see if the block is empty.
          if(fd == super_block.next-1){
            super_block.is_empty = 1;
          }

          // Create a cursor to go along with this file.
          for(int y = 0; y < sizeof(cursors)/sizeof(cursors[0]); y++){
            if(cursors[y].file_name == NULL){
              cursors[y].file_name = fileName;
              cursors[y].block = fd;
              cursors[y].pos = (fd-1)*512;
              cursors[y].mode = mode;
              break;
            }
          }

          // Set the file for current open file.
          current_open_file = fileName;

          return (fd-1)*512;
        }
      }
      // The reason we return -1 here is because we've exhausted our super blocks so there are
      // No free blocks left to write to.

      fprintf(stderr, "File System full. There are no free blocks to write files to.");
      return -1;

    }
  }

  // If we get here all INodes have been taken so we can't add more files.
  
  fprintf(stderr, "File System full. There are too many files in the system.");
  return -1;
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

    if(current_open_file == (const char *) '\0'){
      fprintf(stderr, "File not perviously openend via bv_open");
      return -1;
    }

    for(int i = 0; i < sizeof(cursors)/sizeof(cursors[0]); i++){
      if(current_open_file == cursors[i].file_name){
        current_open_file = (const char *)'\0';
        return 0;
      }
    }

    fprintf(stderr, "File not previously opened via bv_open");
    return -1;
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
  int writtenBytes = 0;
  int check = count;

  //printf("Made it to write.\n");


  printf("%s\n", current_open_file);
  for(int i = 0; i < sizeof(INode_array)/sizeof(INode_array[0]); i++){
    // Check to see if fileNames match.
    //printf("%s\n", INode_array[i].file_name);
    if(strcmp(INode_array[i].file_name, current_open_file) == 0){
      printf("Found our file.\n");
      
      //case for if there is more space in our file compared to
      //how much we want to read into buffer

      // Check to see where the cursor is at and if it is in read mode
      for(int y = 0; y < sizeof(cursors)/sizeof(cursors[0]); y++){
        if(strcmp(cursors[y].file_name, current_open_file) == 0){
          if(cursors[y].mode == BV_RDONLY){
            fprintf(stderr, "File is open in read mode.");
            return -1;
          }
          
          // Seek to where the cursor is.
          lseek(fsFD, cursors[y].pos, SEEK_SET);

          //printf("%d", cursors[y].pos);

          // Begin to write
          // Make sure to check if you are going over the block size (or amount you have left
          // in the block) and if we have enough room.

          // In this case we have enough room to write without overflowing to a new block.
          if(((cursors[y].block)*512 - cursors[y].pos) >= check){
            write(fsFD, buf, check);
            writtenBytes += check;
            INode_array[i].num_bytes += writtenBytes;
            return writtenBytes;
          } else {
            // Lets do the first time for the "rest" of the block even if it is the start.
            // That way we know by the time we get to the while loop we are working with full block sizes.


            int location = 0;
            // Ask backman if this is gonna work. Is buf going to keep our position to let us continue writing?
            // Or do we need a notion of a cursor for that?

            location += ((cursors[y].block-1)*512 - cursors[y].pos);

            if(location == 0){
              write(fsFD, buf+location, 512);
              writtenBytes += 512;
              check -= 512;
            } else {
              write(fsFD, buf+location, location);
              writtenBytes += location;
              check -= location;
            }

            while(check >= 0){
              //printf("%d\n", check);
              int new_block = 0;
              // Aquire a new block.

              // Loop through all of our offsets to find the next empty block.
              for(int x = 0; x < sizeof(super_block.offsets)/sizeof(super_block.offsets[0]); x++){

                // Our current super block is empty so lets just use this one.
                if(super_block.is_empty == 1){
                  new_block = current_super_block;
                  current_super_block = super_block.next;

                  // Seek to the next super block and read it into memory.
                  lseek(fsFD, (current_super_block-1)*512, SEEK_SET);

                  short num;
                  for(int j = 0; j < 256; j ++){
                    // Read in the int at that location
                    read(fsFD, (void *)(&num), sizeof(num));

                    // Push that int into the specified position in the specific location in the super_blocks array.
                    super_block.offsets[j] = num;
                  }

                  // The last number in a super block is a reference to the next super block.
                  read(fsFD, (void *)(&num), sizeof(num));
                  super_block.next = num;
                  super_block.is_empty = 0;

                  for(int z = 0; z < sizeof(INode_array[i].file_blocks)/sizeof(INode_array[i].file_blocks[0]); z++){
                    if(INode_array[i].file_blocks[z] == 0){
                      INode_array[i].file_blocks[z] = new_block;
                      break;
                    }
                  }
                  break;
                } 

                // We found empty space in this super block to use.
                else if(super_block.offsets[x] != 0){
                  new_block = super_block.offsets[x];
                  super_block.offsets[x] = 0;

                  int too_much = 1;
                  for(int z = 0; z < sizeof(INode_array[i].file_blocks)/sizeof(INode_array[i].file_blocks[0]); z++){
                    if(INode_array[i].file_blocks[z] == 0){
                      INode_array[i].file_blocks[z] = new_block;
                      too_much = 0;
                      break;
                    }
                  }
                  if(too_much == 1){
                    fprintf(stderr, "File size too big.");
                    return -1;
                  }
                  break;
                }
              }

              // Seek to the begining of that new block.
              lseek(fsFD, (new_block-1)*512, SEEK_SET);

              // Write to it.
              if(check > 512){
                write(fsFD, buf+location, 512);
                writtenBytes += 512;
                location += 512;
              } else {
                write(fsFD, buf+location, check);
                writtenBytes += check;
                location += check;
              }

              check -= 512;
              printf("%d\n", check);
              
              if(check <= 0){
                INode_array[i].num_bytes += writtenBytes;
                return writtenBytes;
              }
            }
          }
        }
      }
    }
  }

  printf("%d\n", writtenBytes);
  return writtenBytes;
}






/*
 * int bv_read(int bvfs_FD, void *buf, size_t count);
 *
 * This function will read count bytes from the location corresponding to the
 * cursor of the file (represented by bvfs_FD) to buf.
 *
 * Input Parameters`
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
  //we can check if it is open (return appropriate error)
  int writtenBytes=0;
  int check = count;
  //finding which file to read
  for(int i = 0; i < sizeof(INode_array)/sizeof(INode_array[0]); i++){
    // Check to see if fileNames match.
    if(strcmp(INode_array[i].file_name, current_open_file) == 0){
      //case for if there is more space in our file compared to
      //how much we want to read into buffer

      // Check to see where the cursor is at and if it is in read mode
      for(int y = 0; y < sizeof(cursors)/sizeof(cursors[0]); y++){
        if(strcmp(cursors[y].file_name, current_open_file) == 0){
          if(cursors[y].mode != BV_RDONLY){
            fprintf(stderr, "File is not open in read mode.");
            return -1;
          }

          printf("Nice.\n");

          //seek to where cursor is
          //would our cursor be at the start of a block? 
          lseek(fsFD, cursors[y].pos, SEEK_SET);
          //if our count is asking for more bytes than we have in file
          int maxBytes = INode_array[i].num_bytes;
          int cursor_offset = cursors[y].pos - (INode_array[i].file_blocks[cursors[y].file_block]-1)*512;
          //printf("%d\n", cursors[y].pos);
          //printf("%d\n", (INode_array[i].file_blocks[cursors[y].file_block]-1)*512);

          //printf("%d\n", cursor_offset);
          //printf("%d\n", maxBytes);
          if(maxBytes - cursor_offset == 0){
            fprintf(stderr, "Nothing left to read.");
            return -1;
          }

          if(check>maxBytes){
            int loc = cursors[y].file_block;
            int flag = 0;
            while(check>BLOCK_SIZE && writtenBytes<maxBytes){
              //do we need to check here if we try to read in more bytes than
              //we actually have?  
              //second time seek
              if(flag==1){
                lseek(fsFD, (INode_array[i].file_blocks[loc]-1)*512, SEEK_SET);
              }
              //first time read
              if(flag==0){
                read(fsFD, buf, (((cursors[y].block+1)*512)-cursors[y].pos));
                writtenBytes+=(((cursors[y].block+1)*512)-cursors[y].pos);
                check-=(((cursors[y].block+1)*512)-cursors[y].pos);
                loc++;
              }
              else{
                read(fsFD, buf, 512);
                check-=512;
                loc++;
                writtenBytes+=512;
              }
              flag=1;
            }
            if(flag == 0){
              //last time through, filling up the rest of count
              lseek(fsFD, (INode_array[i].file_blocks[loc]-1)*512, SEEK_SET);
              read(fsFD, buf, check);
              cursors[y].pos = ((INode_array[i].file_blocks[loc]-1)*512) + check;
              writtenBytes += check;
              printf("%d\n", writtenBytes);

              return writtenBytes;

            } else {
              //last time through, filling up the rest of count
              //if there is still room for bytes to write
              lseek(fsFD, (INode_array[i].file_blocks[loc]-1)*512 + cursor_offset, SEEK_SET);
              read(fsFD, buf, (maxBytes-writtenBytes));
              cursors[y].pos = ((INode_array[i].file_blocks[loc]-1)*512) + (maxBytes-writtenBytes);
              writtenBytes+=check;
              printf("%d\n", writtenBytes);

              return writtenBytes;
            }
          }
          //the case for if we are asking for less bytes than we have in file 
          //to be read to the buffer
          //this should be fine? 
          else{
            int loc = cursors[y].file_block;
            int flag = 0;
            while(check>BLOCK_SIZE && writtenBytes<maxBytes){
              //if you have less bytes than what is inbetween cursor position
              //and start of block
              if(flag == 0){
                if(maxBytes < ((cursors[y].block+1)*512)-cursors[y].pos){
                  read(fsFD, buf, maxBytes);
                  cursors[y].pos += maxBytes;
                  return maxBytes;
                }
              }
              if(flag == 1){ 
                lseek(fsFD, (INode_array[i].file_blocks[loc]-1)*512, SEEK_SET);
                read(fsFD, buf, 512);
                check-=512;
                loc++;
                writtenBytes+=512;
              }
              flag=1;
            }
            if(flag == 0){
              //last time through, filling up the rest of count
              lseek(fsFD, (INode_array[i].file_blocks[loc]-1)*512 + cursor_offset, SEEK_SET);
              read(fsFD, buf, check);
              cursors[y].pos = ((INode_array[i].file_blocks[loc]-1)*512) + check;
              writtenBytes += check;
              printf("%d\n", writtenBytes);

              return writtenBytes;

            } else {
              //last time through, filling up the rest of count
              lseek(fsFD, (INode_array[i].file_blocks[loc]-1)*512, SEEK_SET);
              read(fsFD, buf, (maxBytes-writtenBytes));
              cursors[y].pos = ((INode_array[i].file_blocks[loc]-1)*512) + (maxBytes-writtenBytes);
              writtenBytes += (maxBytes-writtenBytes);
              printf("%d\n", writtenBytes);

              return writtenBytes;
            }
          }
        }
      }
    }
  }

  return writtenBytes;
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

  for(int i = 0; i < sizeof(INode_array)/sizeof(INode_array[0]); i++){

    if(INode_array[i].file_name == fileName){
      for(int x = 0; x < sizeof(INode_array[i].file_blocks)/sizeof(INode_array[i].file_blocks[0]); x++){

        if(INode_array[i].file_blocks[x] != 0){
          int flag = 0;
          for(int y = 0; y < sizeof(super_block.offsets)/sizeof(super_block.offsets[0]); y++){
            if(super_block.offsets[y] == 0){
              flag = 1;
              super_block.is_empty = 0;
              super_block.offsets[y] = INode_array[i].file_blocks[x];
              break;
            }
          }
          // If there was no empty spots in the super block that means we need to make a new one.
          if(flag == 0){
            // We need to write back our super block to memory.
            lseek(fsFD, (current_super_block-1)*512, SEEK_SET);

            for(int z = 0; z < sizeof(super_block.offsets)/sizeof(super_block.offsets[0]); z++){
              write(fsFD, &super_block.offsets[z], 2);
            }

            //memcpy(&super_block, &blank_super_block, sizeof(struct SuperBlockInfo));
            super_block = blank_super_block;

            super_block.next = current_super_block;
            super_block.is_empty = 1;
            current_super_block = INode_array[i].file_blocks[x];
          }
        }
      }
      
      INode_array[i] = blank_INode;

      // Delete the cursor that goes along with this file.
      for(int y = 0; y < sizeof(cursors)/sizeof(cursors[0]); y++){
        if(cursors[y].file_name == fileName){
          cursors[y] = blank_cursor;
          break;
        }
      }

      return 0;
    }
  }

  fprintf(stderr, "File does not exist.");
  return -1;
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
  
  int count = 0;

  // First for loop to find out how many files we have stored in the file system.
  for(int i = 0; i < sizeof(INode_array)/sizeof(INode_array[0]); i++){
    // There is a file stored in this INode so we need to track it for the first print.
    if(strcmp(INode_array[i].file_name, "\0") != 0){
      count++;
    }
  }

  // Print out the number of files we found
  printf(" %d Files\n", count);

  // Second for loop to print out all of the information for each file.
  for(int i = 0; i < sizeof(INode_array)/sizeof(INode_array[0]); i++){
    // There is a file stored in this INode so we need to print out its info.
    if(strcmp(INode_array[i].file_name, "\0") != 0){
      printf(" bytes: %d, blocks: %f, %.24s, %s\n", INode_array[i].num_bytes, ceil(INode_array[i].num_bytes/512), ctime(&INode_array[i].time_stamp), INode_array[i].file_name);
    }
  }

}

#endif

/* lifpack.c -- pack a LIF disk */
/* 2014, J. Siebold, and placed under the GPL */

#include<stdio.h>
#include<unistd.h>
#include<fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include"lif_block.h"
#include "lif_create_entry.h"
#include"lif_dir_utils.h"
#include "lif_const.h"
#ifndef _WIN32
#define O_BINARY 0
#endif


#define MAXBLOCKS 64000
#define DEBUG 0
#define debug_print(fmt, ...) \
   do { if (DEBUG) fprintf(stderr, fmt, __VA_ARGS__); } while (0)


void usage(void)
  {
    fprintf(stderr, "Usage:lifpack lif-image-filename \n");
    fprintf(stderr,"\n");
    exit(1);
  }

int main(int argc, char **argv)
  {
    /* System variables */
    int option; /* Command line option character */
    int physical_flag; /*  Option to use a physical device */
    int lif_device; /* Descriptor of input device */
    int i,j;
    
    /* LIF disk values */
    unsigned int dir_start; /* first block of the directory */
    unsigned int dir_length; /* length of directory in blocks */
    unsigned int reccount; /* record count of new file */
    unsigned int blockmap[MAXBLOCKS]; /* Block mapping orig/packed file */
    unsigned int allocmap[MAXBLOCKS]; /* Block mapping orig/packed file */
    unsigned char * blocks[MAXBLOCKS]; /* File blocks */


    /* Directory search values */
    int dir_end; /* Set at end of directory */
    int dir_entry; /* Directory entry within current block */
    int dir_block; /* Current block offset from start of directory */
    int new_entry; /* entry number of file in directory */
    int start_block; /* first block of file */
    int num_blocks; /* number of blocks in file */
    int new_block_count; /* block numer in new file */
    unsigned int file_type; /* file type word */
    unsigned char dir_data[SECTOR_SIZE]; /* Current directory block data */
 
    /* Initialize block lists */
    for(i=0;i<MAXBLOCKS;i++) {
       blockmap[i]= -1;
       blocks[i]=(unsigned char *) NULL;
       allocmap[i]=0;
    }

    /* Process command line options */
    optind=1;
    physical_flag=0;
    while ((option=getopt(argc,argv,"p?"))!=-1)
      {
        switch(option)
          {
            case 'p' : physical_flag=1;
                       break;
            case '?' : usage();
                       break;
          }
      }

    /* Are the right number of names specified ? */
    if( optind != argc-1  )
      {
        /* No, give an error */
        usage();
      }

    /* Open lif input device */
    if((lif_device=lif_open(argv[optind],O_RDWR | O_BINARY,0,physical_flag))==-1)
      {
        fprintf(stderr,"Error opening %s\n",argv[optind]);
        exit(1);
      }

    /* Now read block 0 to find where the directory is */
    reccount=0;
    blocks[reccount]=malloc(sizeof (unsigned char) * SECTOR_SIZE);
    blockmap[reccount]=reccount;
    lif_read_block(lif_device,reccount,blocks[reccount]);

    /* Make sure it's a LIF disk */
    if(get_lif_int(blocks[reccount],2)!=0x8000)
      {
        fprintf(stderr,"This is not a LIF disk!\n");
        exit(1);
      }

    /* Find the directory */
    dir_start=get_lif_int(blocks[0]+8,4);
    dir_length=get_lif_int(blocks[0]+16,4);

    /*  block 1 */
    reccount++;
    blocks[reccount]=malloc(sizeof (unsigned char) * SECTOR_SIZE);
    blockmap[reccount]=reccount;
    lif_read_block(lif_device,reccount,blocks[reccount]);

    /* clear directory */
    for(reccount=dir_start;reccount<dir_start+dir_length;reccount++) {
       blocks[reccount]=malloc(sizeof (unsigned char) * SECTOR_SIZE);
       blockmap[reccount]=reccount;
       for(j=0;j<SECTOR_SIZE;j++) *(blocks[reccount]+j)=0xff;
    }

    /* Scan the directory, buffer in directory entries  */
    dir_end=0;
    new_entry=0;
    new_block_count=dir_start+dir_length;
    reccount=dir_start;
    for(dir_block=0; dir_block<dir_length; dir_block++)
      {
        lif_read_block(lif_device,dir_block+dir_start,dir_data);
        for(dir_entry=0; dir_entry<8; dir_entry++)
          {
            file_type=get_lif_int(dir_data+(dir_entry<<5)+10,2);
            if(file_type==0) { continue; } /* Skip deleted files */ 
            if(file_type==0xFFFF)
              {
                /* End of directory */
                dir_end=1;
                break;
              }
            /* file found */
            start_block=get_lif_int(dir_data+(dir_entry<<5)+12,4);
            num_blocks=get_lif_int(dir_data+(dir_entry<<5)+16,4);
            debug_print("Entry %d\n",new_entry);
            debug_print("Start block %d\n",start_block);
            debug_print("Num blocks %d\n",num_blocks);

            /* write new directory entry */
            for(i=0;i<ENTRY_SIZE;i++)
               *(blocks[reccount]+(new_entry<<5)+i)= dir_data[(dir_entry<<5)+i];
            put_lif_int(blocks[reccount]+(new_entry<<5)+12,4,new_block_count);
            new_entry++;
            if(new_entry==8) {
               new_entry=0;
               reccount++;
            }

            /* Do block mapping */
            for(i=0;i<num_blocks;i++) {
                blockmap[new_block_count]= start_block;
                debug_print("block %d mapped to %d\n",start_block,new_block_count);
                allocmap[start_block]=1;
                debug_print("Old block %d allocated\n",start_block);
                new_block_count++;
                start_block++; 
            }
          }
        if(dir_end ) { break; }; /* Quit at end or if file found */
      }

     /* read in all file blocks */
     for(i=0;i<MAXBLOCKS;i++) {
       if(allocmap[i]==1) {
          blocks[i]= malloc(BLOCK_SIZE*sizeof(unsigned char));
          lif_read_block(lif_device,i,blocks[i]);
          debug_print("Old block %d read\n",i);
       }
     }

     /* truncate lif file */
     lif_truncate(lif_device);
     /* write all blocks of packed file */
     for(i=0;i<MAXBLOCKS;i++) {
        j=blockmap[i];
        if(j == -1) break;
        debug_print("write new block %d using old block %d\n",i,j);
        lif_write_block(lif_device,i,blocks[j]);
        free(blocks[j]);
     }
    lif_close(lif_device);
    exit(0);      
  }
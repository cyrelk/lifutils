/* rom41lif.c -- a filter to create uncompressed sdata rom files */
/* 2017 Joachim Siebold and placed under the GPL */

/*  reads a HP-41 rom file and creates a HP-41 data file (uncompressed) */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include "config.h"
#include "lif_create_entry.h"
#include "lif_dir_utils.h"
#include "lif_const.h"


#define RECORD_SIZE 4 
#define PAGE_SIZE 4096*4
#define LIF_ROM41_SIZE  ((PAGE_SIZE / 4) * 5)

#define TRUE 1
#define DEBUG 0
#define debug_print(fmt, ...) \
            do { if (DEBUG) fprintf(stderr, fmt, __VA_ARGS__); } while (0)

void usage(void)
{
   fprintf(stderr,
   "Usage: rom41lif LIF-filename \n");
   fprintf(stderr,"      (Input comes from standard input)\n");
   fprintf(stderr,"      (Output goes to standard output)(\n");
   fprintf(stderr,"\n");
   fprintf(stderr,"      the LIF-filename must not exceed 10 characters");
   fprintf(stderr,"\n");
   exit(1);
}


int read_rom(unsigned char *memory)
/* read rom into memory, scramble data */
  {
    int byte_counter,j,s,l,t,checksum;
    unsigned int lo,hi;
    size_t bytes_read;
    unsigned short rom_data0[RECORD_SIZE]; /* Input data words */
    unsigned short rom_data1[RECORD_SIZE]; /* Input data words, read ahead */

    byte_counter=0; /* Start loading at start of memory */
    s=0;
    /* read in the file data, 1 record at a time */
    bytes_read=fread(rom_data1,sizeof(short),RECORD_SIZE,stdin);
    while(bytes_read== RECORD_SIZE) {
          memcpy(rom_data0,rom_data1,sizeof(short)* RECORD_SIZE);
          /* determine checksum */
          for(j=0;j< RECORD_SIZE;j++) {
              l=s;
              t= ((rom_data0[j] & 0xFF) <<8) | ((rom_data0[j] & 0xFF00) >> 8);
              s+= t;
              if (s>= 1024) {
                  s= s & 0x3FF;
                  s+=1;
              }
          }

       
          bytes_read=fread(rom_data1,sizeof(short),RECORD_SIZE,stdin);

          /* end of file, then store computed checksum */
          if(bytes_read != RECORD_SIZE) {
               checksum= (~l & 0x3FF)+1;
               rom_data0[RECORD_SIZE-1]= ((checksum &0xFF) <<8) |
                   ((checksum &0xFF00) >> 8);
          }


          /* copy data */
          for(j=0; j< RECORD_SIZE;j++) {
             lo= (rom_data0[j] & 0xFF00) >> 8;
             hi= rom_data0[j] & 0x00FF;
             if(byte_counter > PAGE_SIZE) exit(1);
             memory[byte_counter]= hi;
             byte_counter++;
             if(byte_counter > PAGE_SIZE) exit(1);
             memory[byte_counter]= lo;
             byte_counter++;
          }
             
    }
    return(byte_counter); /* number of bytes read */
  }

int main(int argc, char**argv)
  {
    unsigned char dir_entry[ENTRY_SIZE]; /* New directory entry */
    char lif_filename[NAME_LEN+1];
    int length; /* length of code */
    int num_reg; /* number of hpregs in data file */
    unsigned char memory[LIF_ROM41_SIZE]; /* HP41 program memory */

    int i;

    SETMODE_STDIN_BINARY;
    SETMODE_STDOUT_BINARY;

    for(i=0;i<LIF_ROM41_SIZE;i++) memory[i]=0;

    /* Process LIF-filename from command line */
    if(argc != 2) {
       usage();
       exit(1);
    }

    /* Check file name */
    if(check_filename(argv[1])==0)
      {
        fprintf(stderr,"Illegal file name\n");
        exit(1);
      }
    /* Pad the filename with spaces */
    pad_name(argv[1],lif_filename);

    lif_filename[NAME_LEN]=(char) 0;
    debug_print("LIF filename: %s\n", lif_filename);
    length=read_rom(memory); /* Read in the program file */
    debug_print("rom length %d\n", length);

    /* create and write directory entry */
    num_reg= length/8;
    create_entry(dir_entry,lif_filename,0xE0D0,0,length,0);
    /* Implementation bytes for HP41 files */
    dir_entry[26]=0x00;
    dir_entry[27]=0x00;
    dir_entry[28]=num_reg >> 8;
    dir_entry[29]=num_reg & 0xff;
    dir_entry[30]=0x00;
    dir_entry[31]=0x00;
    /* output lif entry */
    for(i=0;i< ENTRY_SIZE; i++) {
       putchar((int) dir_entry[i]);
    }
    debug_print("%s\n","LIF directory entry written");
    for(i=0;i<length;i++) putchar ((int) memory[i]);
    debug_print("rom image %d bytes written\n",length);
    exit(0);
  }

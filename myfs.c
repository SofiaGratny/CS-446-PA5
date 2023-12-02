// Author: Sofia Gratny
// Date: 11/30/23
// Purpose: PA5


#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <stddef.h>
#include <sys/types.h>
#include <string.h>
#include <sys/mman.h>
#include <pthread.h>
#include <ctype.h>
#include <time.h>
#define BLKSIZE 4048 


const int root_inode_number = 2;  
const int badsectors_inode_number = 1; 
const int invalid_inode_number = 0;  
const int root_datablock_number = 0;  

typedef struct _block_t {
  char data[BLKSIZE];
}
 block_t;
typedef struct _inode_t {
  int size;  
  int blocks;  
  struct timeval ctime;  
  block_t* data[15];  
} 
inode_t;
typedef struct _dirent_t {
  int inode;  
  char file_type;  
  int name_len; 
  char name[255];  
} 
dirent_t;
typedef struct _superblock_info_t {
  int blocks;  
  char name[255];  
} 
superblock_info_t;
union superblock_t {
  block_t block;  
  superblock_info_t superblock_info;  
};
typedef struct _groupdescriptor_info_t {
  inode_t* inode_table; 
  block_t* block_data;  
} 
groupdescriptor_info_t;
union groupdescriptor_t {
  block_t block;  
  groupdescriptor_info_t groupdescriptor_info; 
};
typedef struct _myfs_t {
  union superblock_t super;  
  union groupdescriptor_t groupdescriptor;  
  block_t bmap;  
  block_t imap; 
} 
myfs_t;
myfs_t* my_mkfs(int size, int maxfiles);
void my_dumpfs(myfs_t* myfs);
void dump_dirinode(myfs_t* myfs, int inode_number, int level);
void my_crawlfs(myfs_t* myfs);
void my_creatdir(myfs_t* myfs, int cur_dir_inode_number, const char* new_dirname);

int roundup(int x, int y) {
  return x == 0 ? 0 : 1 + ((x - 1) / y);
}

int main(int argc, char *argv[]){
    
  inode_t* cur_dir_inode = NULL;

  myfs_t* myfs = my_mkfs(100*BLKSIZE, 10);

  int cur_dir_inode_number = 2; 
  my_creatdir(myfs, cur_dir_inode_number, "mystuff");  
  my_creatdir(myfs, cur_dir_inode_number, "homework");  
  
  cur_dir_inode_number = 4;  
  my_creatdir(myfs, cur_dir_inode_number, "assignment5"); 

  cur_dir_inode_number = 5; 
  my_creatdir(myfs, cur_dir_inode_number, "mycode");  

  cur_dir_inode_number = 3;  
  my_creatdir(myfs, cur_dir_inode_number, "mydata");  

  printf("\nDumping filesystem structure:\n");
  my_dumpfs(myfs);

  printf("\nCrawling filesystem structure:\n");
  my_crawlfs(myfs);

  return 0;
}


myfs_t* my_mkfs(int size, int maxfiles) {
  int num_data_blocks = roundup(size, BLKSIZE);

  int num_inode_table_blocks = roundup(maxfiles*sizeof(inode_t), BLKSIZE);  

  size_t fs_size = sizeof(myfs_t) +  
          num_inode_table_blocks * sizeof(block_t) +  
          num_data_blocks * sizeof(block_t);  

  void *ptr;

  ptr = malloc(fs_size);
  if (ptr == NULL) {
    printf("malloc failed\n");
    return NULL;
  }

  if (mlock(ptr, size)) {
    printf("mlock failed\n");
    free(ptr);
    return NULL;
  }

  myfs_t* myfs = (myfs_t*)ptr;

  void *super_ptr = calloc(BLKSIZE, sizeof(char));

  union superblock_t* super = (union superblock_t*)super_ptr;
  super->superblock_info.blocks = num_data_blocks;
  strcpy(super->superblock_info.name, "MYFS");

  memcpy((void*)&myfs->super, super_ptr, BLKSIZE);

  void *groupdescriptor_ptr = calloc(BLKSIZE, sizeof(char));

  union groupdescriptor_t* groupdescriptor = (union groupdescriptor_t*)groupdescriptor_ptr;
  groupdescriptor->groupdescriptor_info.inode_table = (inode_t*)((char*)ptr + sizeof(myfs_t));
  groupdescriptor->groupdescriptor_info.block_data = (block_t*)((char*)ptr + sizeof(myfs_t) + num_inode_table_blocks * sizeof(block_t));
  
  memcpy((void*)&myfs->groupdescriptor, groupdescriptor_ptr, BLKSIZE);

 
  void *inodetable_ptr = calloc(BLKSIZE, sizeof(char));
  
  inode_t* inodetable = (inode_t*)inodetable_ptr;
  inodetable[root_inode_number].size = 2 * sizeof(dirent_t);  
  inodetable[root_inode_number].blocks = 1;  
  for (uint i=1; i<15; ++i)  
    inodetable[root_inode_number].data[i] = NULL;
  inodetable[root_inode_number].data[0] = &(groupdescriptor->groupdescriptor_info.block_data[root_datablock_number]);  
  
  memcpy((void*)groupdescriptor->groupdescriptor_info.inode_table, inodetable_ptr, BLKSIZE);

  void *dir_ptr = calloc(BLKSIZE, sizeof(char));
  dirent_t* dir = (dirent_t*)dir_ptr;
  dirent_t* root_dirent_self = &dir[0];
  {
    root_dirent_self->name_len = 1;
    root_dirent_self->inode = root_inode_number;
    root_dirent_self->file_type = 2;
    strcpy(root_dirent_self->name, ".");
  }

  dirent_t* root_dirent_parent = &dir[1];
  {
    root_dirent_parent->name_len = 2;
    root_dirent_parent->inode = root_inode_number;
    root_dirent_parent->file_type = 2;
    strcpy(root_dirent_parent->name, "..");
  }

  memcpy((void*)(inodetable[root_inode_number].data[0]), dir_ptr, BLKSIZE);

  void* bmap_ptr = calloc(BLKSIZE, sizeof(char));
  
  block_t* bmap = (block_t*)bmap_ptr;
  bmap->data[root_datablock_number / 8] |= 0x1<<(root_datablock_number % 8);
  
  memcpy((void*)&myfs->bmap, bmap_ptr, BLKSIZE);

  void* imap_ptr = calloc(BLKSIZE, sizeof(char));
  
  block_t* imap = (block_t*)imap_ptr;
  imap->data[root_inode_number / 8] |= 0x1<<(root_inode_number % 8);
  imap->data[badsectors_inode_number / 8] |= 0x1<<(badsectors_inode_number % 8);
  imap->data[invalid_inode_number / 8] |= 0x1<<(invalid_inode_number % 8);
  
  memcpy((void*)&myfs->imap, imap_ptr, BLKSIZE);

  return myfs;
}

void my_dumpfs(myfs_t* myfs) {
  printf("superblock_info.name: %s\n", myfs->super.superblock_info.name);
  printf("superblock_info.blocks: %d\n", myfs->super.superblock_info.blocks);
  printf("groupdescriptor_info.inode_table: %p\n", myfs->groupdescriptor.groupdescriptor_info.inode_table);
  printf("groupdescriptor_info.block_data: %p\n", myfs->groupdescriptor.groupdescriptor_info.block_data);
  for (size_t byte = 0; byte < BLKSIZE; ++byte) {
  for (uint bit = 0; bit < 8; ++bit) {

    if (myfs->imap.data[byte] & (0x1 << bit)) {
        int inode_number = byte*8 + bit;

    if (inode_number < root_inode_number) { 
          continue;
    }

    if (inode_number == root_inode_number) { 
      printf("  ROOT inode %d occupied - initialized filesystem found!\n", inode_number);
      dump_dirinode(myfs, inode_number, 0);
}
}
}
}
}

#define LEVEL_TAB for(int o=0; o<4*level; ++o){ printf(" "); }
#define NEXTLEVEL_TAB for(int o=0; o<4*(level+1); ++o){ printf(" "); }

void dump_dirinode(myfs_t* myfs, int inode_number, int level) {
  inode_t* inodetable = myfs->groupdescriptor.groupdescriptor_info.inode_table;
  LEVEL_TAB printf("  inode.size: %d\n", inodetable[inode_number].size);
  LEVEL_TAB printf("  inode.blocks: %d\n", inodetable[inode_number].blocks);
  for (uint block_nr = 0; block_nr < 11; ++block_nr) {  

    if (inodetable[inode_number].data[block_nr] != NULL) {
      int num_direntries = inodetable[inode_number].size / sizeof(dirent_t); 
      LEVEL_TAB printf("    num_direntries: %d\n", num_direntries);
      dirent_t* direntries = (dirent_t*)(inodetable[inode_number].data[block_nr]);
      
    for (size_t dirent_num = 0; dirent_num < num_direntries; ++dirent_num) {
      LEVEL_TAB printf("    direntries[%ld].inode: %d\n", dirent_num, direntries[dirent_num].inode);
      LEVEL_TAB printf("    direntries[%ld].name: %s\n", dirent_num, direntries[dirent_num].name);
      LEVEL_TAB printf("    direntries[%ld].file_type: %s\n", dirent_num, (int)direntries[dirent_num].file_type==2?"folder":(int)direntries[dirent_num].file_type==1?"file":"unknown");
        
      if ((int)direntries[dirent_num].file_type == 2) {  
          if (strcmp(direntries[dirent_num].name,".") && strcmp(direntries[dirent_num].name,"..")) {  
            NEXTLEVEL_TAB printf("  inode %d occupied:\n", direntries[dirent_num].inode);
            dump_dirinode(myfs, direntries[dirent_num].inode, level+1);
      }
      }
        
     if ((int)direntries[dirent_num].file_type == 1) {  
        LEVEL_TAB printf("    FILE:\n");
        LEVEL_TAB printf("    inode.size: %d\n", inodetable[direntries[dirent_num].inode].size);
        LEVEL_TAB printf("    inode.blocks: %d\n", inodetable[direntries[dirent_num].inode].blocks);
        LEVEL_TAB printf("    inode.data[0]: "); for (int i=0; i<inodetable[direntries[dirent_num].inode].size; ++i) { printf("%c", ((char*)inodetable[direntries[dirent_num].inode].data[0])[i]); } printf("\n"); 
}
}
}
}
}

#define LEVEL_TREE for(int o=0; o<2*level; ++o){ printf(" "); } printf("|"); for(int o=0; o<2*level; ++o){ printf("_"); }

void crawl_dirinode(myfs_t* myfs, int inode_number, int level) {
  inode_t* inodetable = myfs->groupdescriptor.groupdescriptor_info.inode_table;
  for (uint block_nr = 0; block_nr < 11; ++block_nr) {  
    
    if (inodetable[inode_number].data[block_nr] != NULL) {
      int num_direntries = inodetable[inode_number].size / sizeof(dirent_t); 
      dirent_t* direntries = (dirent_t*)(inodetable[inode_number].data[block_nr]);
      
    for (size_t dirent_num = 0; dirent_num < num_direntries; ++dirent_num) {
      LEVEL_TREE printf("_ %s\n", direntries[dirent_num].name);
        
        if ((int)direntries[dirent_num].file_type == 2) {  
        if (strcmp(direntries[dirent_num].name,".") && strcmp(direntries[dirent_num].name,"..")) {  
          crawl_dirinode(myfs, direntries[dirent_num].inode, level+1);
}
}
}
}
}
}

void my_crawlfs(myfs_t* myfs) {
  for (size_t byte = 0; byte < BLKSIZE; ++byte) {
  for (uint bit = 0; bit < 8; ++bit) {
      
    if (myfs->imap.data[byte] & (0x1 << bit)) {
      int inode_number = byte*8 + bit;
        
    if (inode_number < root_inode_number) {  
      continue;
        }
        
    if (inode_number == root_inode_number) {  
        printf("/\n");
        crawl_dirinode(myfs, inode_number, 0);
}
}
}
}
}


void my_creatdir(myfs_t* myfs, int cur_dir_inode_number, const char* new_dirname) {
  int new_inode_number = -1;
  for (size_t byte = 0; byte < BLKSIZE; ++byte) {
  for (unsigned int bit = 0; bit < 8; ++bit) {
      
  if (!(myfs->imap.data[byte] & (0x1 << bit))) {
      new_inode_number = byte * 8 + bit;
      myfs->imap.data[byte] |= (0x1 << bit);
      break;
    }
    }

    if (new_inode_number != -1) {
      break;
  }
  }

  int new_data_block_number = -1;
  for (size_t byte = 0; byte < BLKSIZE; ++byte) {
  for (unsigned int bit = 0; bit < 8; ++bit) {
      
    if (!(myfs->bmap.data[byte] & (0x1 << bit))) {
        new_data_block_number = byte * 8 + bit;
        myfs->bmap.data[byte] |= (0x1 << bit);
        break;
    }
    }
    
    if (new_data_block_number != -1) {
      break;
  }
  }

  inode_t* inodeTable = myfs->groupdescriptor.groupdescriptor_info.inode_table;
  inode_t* cur_dir_inode = &inodeTable[cur_dir_inode_number];

  cur_dir_inode->size += sizeof(dirent_t);

  inode_t* new_dir_inode = &inodeTable[new_inode_number];
  new_dir_inode->size = 2 * sizeof(dirent_t);  
  new_dir_inode->blocks = 1;  
  for (unsigned int i = 1; i < 15; ++i) {
  new_dir_inode->data[i] = NULL;
  }
  new_dir_inode->data[0] = &(myfs->groupdescriptor.groupdescriptor_info.block_data[new_data_block_number]);

  memcpy((void*)myfs->groupdescriptor.groupdescriptor_info.inode_table, inodeTable, BLKSIZE);

  block_t* cur_dir_data_block = cur_dir_inode->data[0];
  dirent_t* dir_entries = (dirent_t*)cur_dir_data_block->data;

  int num_direntries = cur_dir_inode->size / sizeof(dirent_t);
  dir_entries[num_direntries].inode = new_inode_number;
  dir_entries[num_direntries].file_type = 2;
  dir_entries[num_direntries].name_len = strlen(new_dirname);
  strcpy(dir_entries[num_direntries].name, new_dirname);

  memcpy((void*)cur_dir_data_block->data, dir_entries, BLKSIZE);

  cur_dir_inode->size += sizeof(dirent_t);

  block_t* new_dir_data_block = new_dir_inode->data[0];
  dirent_t* new_dir_entries = (dirent_t*)new_dir_data_block->data;

  new_dir_entries[1].inode = cur_dir_inode_number;
  new_dir_entries[1].file_type = 2;
  new_dir_entries[1].name_len = 2;
  strcpy(new_dir_entries[1].name, "..");
  
  new_dir_entries[0].inode = new_inode_number;
  new_dir_entries[0].file_type = 2;
  new_dir_entries[0].name_len = 1;
  strcpy(new_dir_entries[0].name, ".");

  memcpy((void*)new_dir_data_block->data, new_dir_entries, BLKSIZE);
}


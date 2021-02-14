#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "disk.h"
#include "fs.h"

#define FAT_EOC 0xFFFF
#define MAX_FD 32
#define MAX_FAT_ENTRIES 2048

int find_fat_index(void);
int find_data_index(void);
int root_contains(const char *filename);
int find_fd(int fd);
int offset_to_data(int root_idx);
void new_data_block(void);

struct superblock {
        uint8_t signature[8];
        uint16_t tot_num_blk;
        uint16_t root_idx;
        uint16_t data_idx;
        uint16_t data_blk_num;
        uint8_t FAT_blk_num;
        uint8_t padding[BLOCK_SIZE-17];
} __attribute__((__packed__));

struct root {
        uint8_t file_name[FS_FILENAME_LEN];
        uint32_t size;
        uint16_t first_db_idx;
        uint8_t padding[10];
} __attribute__((__packed__));

struct data_block {
        uint16_t content[MAX_FAT_ENTRIES];
} __attribute__((__packed__));

struct FD {
        int fd;
        int offset;
        int root_idx;

};

static struct superblock sb;
uint16_t *fat;
static struct root root_dir[FS_FILE_MAX_COUNT];
static struct data_block *data_block;
static struct FD fd_obj[FS_FILE_MAX_COUNT] = {{.fd = 0}, {.offset = 0}, {.root_idx = 0}};
int root_count, free_fat_idx, free_data_idx, cur_root_idx;
int fd_count;
static struct FD *cur_fd;

int FAT_blocks(int blocks) {
        int num = (blocks - 2)/(BLOCK_SIZE);
        if((blocks - 2)%(BLOCK_SIZE) != 0)
                num++;
        return num;
}

int fs_mount(const char *diskname)
{
        if(block_disk_open(diskname))
                return -1;

        block_read(0, &sb);

        fat = (uint16_t *)malloc(sb.FAT_blk_num * sizeof(uint16_t)*MAX_FAT_ENTRIES);
        for (int i = 0; i < sb.FAT_blk_num; i++)
        	block_read((i+1), (fat+i*MAX_FAT_ENTRIES));

	block_read(sb.FAT_blk_num+1, &root_dir);

	data_block = (struct data_block*)malloc(sb.data_blk_num*sizeof(struct data_block));
	for(int i=0; i<sb.data_blk_num; i++)
		block_read(2+sb.FAT_blk_num+i, &data_block[i]);

	if(strncmp((char*)sb.signature, "ECS150FS", 8) || sb.tot_num_blk != block_disk_count())
		return -1;

	fat[0] = FAT_EOC;
	block_write(1, &fat[0]);
	data_block[0].content[0] = FAT_EOC;
	block_write(sb.data_idx, &data_block[0]);

	return 0;
}

int fs_umount(void)
{
	if(block_disk_close())
		return -1;

	for(int i=1; i<=FS_FILE_MAX_COUNT; i++)
		if(fd_obj[i].fd>0 && fd_obj[i].fd<=FS_FILE_MAX_COUNT) {
			return -1;
		}

	free(fat);
	free(data_block);
	return 0;
}

int fs_info(void)
{
	if(!sb.signature)
                return -1;

        int free_data_blocks = sb.data_blk_num;
        for(int i=0; i<sb.data_blk_num; i++) {
                        if(data_block[i].content[0])
                                free_data_blocks--;
        }

	int root_count;
        for(int i=0; i<FS_FILE_MAX_COUNT; i++) {
                if (root_dir[i].file_name[0])
                        root_count++;
        }

        printf("FS Info:\n");
        printf("total_blk_count=%d\n", sb.tot_num_blk);
        printf("fat_blk_count=%d\n", sb.FAT_blk_num);
        printf("rdir_blk=%d\n", sb.root_idx);
        printf("data_blk=%d\n", sb.data_idx);
        printf("data_blk_count=%d\n", sb.data_blk_num);
        printf("fat_free_ratio=%d/%d\n", (free_data_blocks),sb.data_blk_num);
        printf("rdir_free_ratio=%d/%d\n", (FS_FILE_MAX_COUNT-root_count),FS_FILE_MAX_COUNT);

        return 0;

}

int fs_create(const char *filename)
{
	if(!filename || strlen(filename)>=FS_FILENAME_LEN || filename[strlen(filename)] != '\0' ||  root_count >= FS_FILE_MAX_COUNT)
		return -1;

	if(!root_contains(filename))
		return -1;

	int root_index;
	for(root_index = 0; root_index < FS_FILE_MAX_COUNT; root_index++)
		if(!root_dir[root_index].file_name[0])
			break;

	if(root_index == FS_FILE_MAX_COUNT)
		return -1;

	if(find_fat_index())
		return -1;

	memcpy(root_dir[root_index].file_name, filename, strlen(filename));
	root_dir[root_index].size = 0;
	root_dir[root_index].first_db_idx = free_fat_idx;

	fat[free_fat_idx] = FAT_EOC;
	block_write(sb.root_idx, root_dir);
	block_write(1 + free_fat_idx/MAX_FAT_ENTRIES, (fat + (free_fat_idx/MAX_FAT_ENTRIES)));


	return 0;
}

int fs_delete(const char *filename)
{
	if(!filename || root_contains(filename) || filename[strlen(filename)] != '\0')
		return -1;

	root_dir[cur_root_idx].file_name[0] = 0;
	block_write(sb.root_idx, root_dir);

	int fat_idx = root_dir[cur_root_idx].first_db_idx;
	int data_idx;
	while(fat[fat_idx] != FAT_EOC) {
		data_idx = fat[fat_idx];
		fat[fat_idx] = 0;
		data_block[data_idx].content[0] = 0;
		block_write(1 + fat_idx/MAX_FAT_ENTRIES, (fat + (fat_idx/MAX_FAT_ENTRIES)));
		block_write(1 + sb.data_idx + data_idx, (data_block+data_idx));
		fat_idx = data_idx;
	}

	fat[fat_idx] = 0;
	block_write(1 + fat_idx/MAX_FAT_ENTRIES, (fat + (fat_idx/MAX_FAT_ENTRIES)));

	return 0;
}

int fs_ls(void)
{
	printf("FS Ls:\n");
	for(int i=0; i < FS_FILE_MAX_COUNT; i++) {
		if(root_dir[i].file_name[0] != '\0') {
			printf("file: %s, size: %d, data_blk: %d\n",
				root_dir[i].file_name, root_dir[i].size, root_dir[i].first_db_idx);
		}
	}
	return 0;
}

int fs_open(const char *filename)
{
	if(!filename || strlen(filename)>=FS_FILENAME_LEN ||  filename[strlen(filename)] != '\0' || fd_count>MAX_FD)
		return -1;

	if(root_contains(filename)) {
		printf("root contains\n");
		for(int i=1; i<=FS_OPEN_MAX_COUNT; i++) {
			printf("i=%d\n", i);
			if(!fd_obj[i].fd) {
				fd_obj[i].fd = i;
				fd_obj[i].offset = 0;
				fd_obj[i].root_idx = cur_root_idx;
				fd_count++;
				printf("fd = %d\n", i);
				return i;
			}
		}
	} else { 
		for(int i=0; i<FS_OPEN_MAX_COUNT; i++) {
			int root_idx = fd_obj[i].root_idx;
			if(!strcmp((char*)root_dir[root_idx].file_name, filename)) {
				fd_count++;
				return fd_obj[i].fd;
			}
		}
	}
			
	return -1;
}

int fs_close(int fd)
{	
	if(!(fd>0 &&fd<=MAX_FD))
		return -1;

	for(int i=1; i<=FS_OPEN_MAX_COUNT; i++) {
		if(fd_obj[i].fd==fd) {
			fd_obj[i].fd = 0;
			fd_obj[i].offset = 0;
			fd_obj[i].root_idx = 0;
			break;
		}
	}

	return 0;
}

int fs_stat(int fd)
{
	printf("fd in stat = %d\n", fd);
	if(!(fd>0 &&fd<=MAX_FD))
		return -1;

	for(int i=1; i<=FS_OPEN_MAX_COUNT; i++)
		if(fd_obj[i].fd == fd) {
			printf("size(stat) = %d\n", root_dir[fd_obj[i].root_idx].size);
			return root_dir[fd_obj[i].root_idx].size;
		}

	return 0;
}

int fs_lseek(int fd, size_t offset)
{
	if(!(fd>0 &&fd<=MAX_FD)) 
	       return -1;	

	for(int i=1; i<=FS_FILE_MAX_COUNT; i++) {
		if(fd_obj[i].fd==i) {
			fd_obj[i].offset = offset;
			if(root_dir[fd_obj[i].root_idx].size<offset)
				return -1;
		}
	}


	return 0;
}

int fs_write(int fd, void *buf, size_t count)
{
	if(!(fd>0 &&fd<=MAX_FD))
		return -1;
	
	if(!find_fd(fd))
		return -1;
	
	cur_root_idx = cur_fd->root_idx;

	int data_blk = offset_to_data(cur_fd->offset);
	free_data_idx = root_dir[cur_root_idx].first_db_idx;
	for(int i=0; i<data_blk; i++)
		free_data_idx = fat[free_data_idx];

	char bounce_buf[BLOCK_SIZE];
	int tot_written = 0;

	int offset = (cur_fd->offset)%BLOCK_SIZE;
	while(count>0) {
		int amount_written = 0;
		if(count<BLOCK_SIZE) {
			amount_written = count;
			int buf_start = data_blk*BLOCK_SIZE + offset;
			block_read(free_data_idx+sb.data_idx, bounce_buf);
			memcpy(bounce_buf+buf_start, buf, count);	
			block_write(free_data_idx+sb.data_idx, bounce_buf);
			count -= amount_written;
			cur_fd->offset += count;

		} else {
			block_write(free_data_idx+sb.data_idx, buf);
			amount_written += BLOCK_SIZE;
			count -= amount_written;
			cur_fd->offset += amount_written;
			if(find_fat_index())
					break;
			if(find_data_index())
					new_data_block();
			fat[free_fat_idx] = free_data_idx;
			block_write(1 + free_fat_idx/MAX_FAT_ENTRIES, (fat + (free_fat_idx/MAX_FAT_ENTRIES)));
			data_blk++;
		}
		
		offset += amount_written;
		tot_written += amount_written;
		
	}
	if(offset<BLOCK_SIZE) {
		fat[free_fat_idx] = FAT_EOC;
	} else if(!find_fat_index()) {
		if(find_data_index())
			new_data_block();
		fat[free_fat_idx] = FAT_EOC;
		block_write(1 + free_fat_idx/MAX_FAT_ENTRIES, (fat + (free_fat_idx/MAX_FAT_ENTRIES)));
	}

	root_dir[cur_root_idx].size += tot_written;
	block_write(sb.root_idx, root_dir);
	return tot_written;
}

int fs_read(int fd, void *buf, size_t count)
{
	if(!(fd>0 &&fd<=MAX_FD))
		return -1;

	if(!find_fd(fd)) 
		return -1;
	cur_root_idx = cur_fd->root_idx;

	int data_blk = offset_to_data(cur_fd->offset); 
	int data_idx = root_dir[cur_root_idx].first_db_idx;
	for(int i=0; i<data_blk; i++) {
		data_idx = fat[data_idx];
	}


	char bounce_buf[BLOCK_SIZE];
	int tot_amount_read = 0;

	int offset = (cur_fd->offset)%BLOCK_SIZE;
	if(count>root_dir[cur_root_idx].size)
		count = root_dir[cur_root_idx].size - offset;
	while(count>0) {
		printf("count in loop = %zu\n", count);
		int amount_read = 0;
		if(count<BLOCK_SIZE) {
			int buf_start = data_blk*BLOCK_SIZE + offset;
			block_read(data_idx+sb.data_idx, bounce_buf);
			count = count-buf_start;
			amount_read = count;
			memcpy(buf, bounce_buf+buf_start, amount_read);
			count -= amount_read;
			 cur_fd->offset += amount_read;

		} else {
			block_read(data_idx+sb.data_idx, buf);
			amount_read = BLOCK_SIZE;
			count -= amount_read;
			cur_fd->offset += amount_read;
		}

		offset += amount_read;
		tot_amount_read += amount_read;
		if(fat[data_idx] == FAT_EOC)
			break;

		data_idx = fat[data_idx];
		data_blk++;
	}

	return tot_amount_read;
}

// find next free fat index
int find_fat_index(void) {
        for(free_fat_idx=0; free_fat_idx<sb.FAT_blk_num*MAX_FAT_ENTRIES; free_fat_idx++)
		if(!fat[free_fat_idx]) {
			return 0;
		}
        return -1;
}

// find next free data index
int find_data_index(void) {
        for(free_data_idx=0; free_data_idx<sb.data_blk_num; free_data_idx++)
                if(!data_block->content[free_data_idx])
                        return 0;
        return -1;
}

// checks if root contains filename
int root_contains(const char *filename) {
	for(cur_root_idx=0; cur_root_idx<FS_FILE_MAX_COUNT; cur_root_idx++)
		if(!strcmp((char*)root_dir[cur_root_idx].file_name, filename))
			return 0;
	return -1;
}

// find the fd object 
int find_fd(int fd) {
	for(int i=1; i<=FS_OPEN_MAX_COUNT; i++)
		if(fd_obj[i].fd == fd) {
			cur_fd = &fd_obj[i];
			return i;
		}
	return 0;
}

//function that returns the index of the data block corresponding to the file’s offset.
int offset_to_data(int offset) {
	if(offset<BLOCK_SIZE)
		return 0;
	int block_num = offset/BLOCK_SIZE;
	if(offset%BLOCK_SIZE!=0)
		block_num++;

	return block_num;
}

// allocate new data block
void new_data_block(void) {
	data_block = realloc(data_block, sb.data_blk_num+1*sizeof(data_block));
	sb.data_blk_num++;
	free_data_idx = sb.data_blk_num;
}

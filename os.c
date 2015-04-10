#define FUSE_USE_VERSION 26
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <malloc.h>

#include <sys/statvfs.h> // to return statfs
#include <unistd.h> // for access() and getcwd()
#include <time.h> // for UNIX time

/*
CS3224: Homework Assignment #1
Programmer: Robert Ryszewski
Date Submitted: 4/13/2015

Notes:
* The above #define and first block of #includes are credited to the hello.c example
* fuse filesystem.
* 
* The log_msg() code is written originally, but the idea is credited to the
* bbfs FUSE file system, widely available online for educational purposes. 
* 
* The following block of prototypes is primarily for organizational purposes,
* as well as to provide a brief overview of this assignment.
* 
* All other notes are on made on a per function basis.
* 
* This code is recommended to be compiled with the following options:
	gcc -Wall -g os.c `pkg-config fuse --cflags --libs` -o os_hw
*/

#define FILE_SIZE 4096
#define MAX_FILES 10000
#define PTRS_PER_BLK 400
#define FREE_BLK_FILES (MAX_FILES/PTRS_PER_BLK)
#define MAX_DIGITS_BLOCK_NUM 100
#define MAX_FILE_SIZE PTRS_PER_BLK*FILE_SIZE

void log_msg(const char* msg);
void* init_rr(struct fuse_conn_info *conn);
	int bdir_path_size();
	void create_root_dir();
	void free_block_list();
	void create_super_block();
int opendir_rr(const char* path, struct fuse_file_info * fi);
int getattr_rr(const char *path, struct stat *stbuf);
int readdir_rr(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi);
int open_rr(const char *path, struct fuse_file_info *fi);
int read_rr(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
int release_rr(const char * path, struct fuse_file_info * fi);
int releasedir_rr(const char * path, struct fuse_file_info * fi);
int mkdir_rr(const char *path, mode_t mode);
	int first_free_block();
	void fill_dir_inode(char* file_data, int parent_block, int free_block);
	int remove_free_block(int rm_block);
int rename_rr(const char* path, const char* new_name);
	int is_entry(const char* parent_data, const char* old_name);
int rmdir_rr(const char * path);
int rm_file(const char* path, char dir_or_reg, char last_link);
	int flush_block_file(char* block_data, int block_num);
	FILE* open_block_file(int block_num);
	int get_indirect(char* file_data);
	int get_data_block_num(char* file_data);
	int find_block_num(char* inode_data, int inode_pos, char* entry_name_to_find, char* block_num_of_entry);
	int add_free_block(char* block_num);
	int free_indirect_blocks(char* block_num);
int unlink_rr(const char * path);	
	int get_block_num(const char* path, char dir_or_reg);
int create_rr(const char * path, mode_t mode, struct fuse_file_info * fi);
	void fill_reg_inode(char* file_data, int free_block);
	int insert_parent_entry(const char* path, int block_num);
void destroy_rr(void* private_data);
int statfs_rr(const char *path, struct statvfs *statv);
	int total_free_blocks();
	void get_heap_parent_child(const char* path, char* parent_path, char* dir_name);
	int get_link_count(char* file_data);
	int mod_link_count(char* file_data, int change);
int link_rr(const char *path, const char *newpath);
int write_rr(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
	int get_location(const char* inode_data);
	int set_indirect(char* file_data, int ind_val);
	int set_location(char* inode_data, int new_bnum);
	int set_size(char* inode_data, int size);
	
void log_msg(const char* msg){
	// log_msg() simply prints out any passed in c_str to the log file in the CWD.
	// Note: When passed a block file's data of FILE_SIZE bytes, the data is not null-terminated
	// so what will be written to the file may extend past the actual data for this reason.
	// However this does not necessarily represent the contents of the file data.
	
	char* logs_path=malloc(strlen(fuse_get_context()->private_data)+20);
	sprintf(logs_path, "%s", (char *)fuse_get_context()->private_data);
	sprintf(logs_path+strlen(logs_path), "%s", "/logs.txt");

	FILE* fh=fopen(logs_path, "a");
	if(msg!=NULL) { fwrite(msg, strlen(msg), 1, fh); }
	fwrite("\r\n", 2, 1, fh);
	fwrite("\r\n", 2, 1, fh);
	fclose(fh);

	free(logs_path);
}

void* init_rr(struct fuse_conn_info *conn){
	/*
	init(): creates all the fusedata.X block files that do not already exist, and, if none of them
	existed when init() was called, creates the super block, free block list, and root directory block.
	* RETURN: fuse_get_context()->private_data is returned, as fuse takes the return of init to re-initialize private_data.
	*/
	log_msg("init(): enter");
	
	// create a buffer of all 0s
 	char* buf=malloc(FILE_SIZE); 
 	memset(buf, '0', FILE_SIZE); 
	
	// build the blocks dir file name path
	char* blocks_dir=malloc(bdir_path_size());
	sprintf(blocks_dir, "%s", (char *) fuse_get_context()->private_data);
	sprintf(blocks_dir+strlen(blocks_dir), "%s", "/blocks");
	
	// make the directory for the blocks if it doesn't exist
	int result_bdir=access(blocks_dir, F_OK);
	if( result_bdir <0 && errno==ENOENT){ 
		mkdir(blocks_dir, S_IRWXU | S_IRWXG | S_IROTH | S_IWOTH | S_IXOTH); 
	}
	
	// only change the # after this position in the string
	sprintf(blocks_dir+strlen(blocks_dir), "%s",  "/fusedata.");
	int path_pos=strlen(blocks_dir);

	// create all the block files, if they don't already exist
	int i;
	for(i=0; i<MAX_FILES; i++){
		sprintf(blocks_dir+path_pos, "%d", i);		
		int result=access(blocks_dir, F_OK);		
		if( result <0 && errno==ENOENT){ 				
			FILE* fd=fopen(blocks_dir,"w+");
			fwrite(buf, FILE_SIZE, 1, fd);
			fclose(fd);			
		} // silently fails if an error opening that file
	}
	if( result_bdir<0 && errno==ENOENT){ // if the blocks dir hadn't already existed	
		create_super_block(); // make the super block and free block list
		free_block_list();
		create_root_dir();
	}
	
	free(buf);
	free(blocks_dir);
	log_msg("init(): exit");
	return fuse_get_context()->private_data; 
}

int bdir_path_size(){
	/* 
	bdir_path_size() returns the # of bytes to allocate to the file name to fusedata.X blocks.
	* This depends on the length of the CWD passed in from main().
	* This may seem unneeded, but as the CWD path may be excessively long, as can the maximum value of X,
	* this prevents the need to hardcore some upper limit for path names.
	*/
	log_msg("bdir_path_size(): enter");

	char* num_digits=malloc(MAX_DIGITS_BLOCK_NUM); // not allowed more than 100 digits in largest block # string
	sprintf(num_digits, "%d", MAX_FILES-1);
	int size=strlen(fuse_get_context()->private_data);
	size+=strlen("/blocks/fusedata.");
	size+=strlen(num_digits)+1; // for the null character at the end 
	sprintf(num_digits, "%d", size);

	log_msg("bdir_path_size(): bdir path size=");
	log_msg(num_digits);
	free(num_digits);
	log_msg("bdir_path_size(): exit");
	return size;
}

void create_super_block(){
	/*
	create_super_block() creates the super_block as fusedata.0, always.
	* It fills in the information expected.
	* RETURN: Nothing.
	*/
	log_msg("create_super_block(): enter");
	time_t current_time;
	time(&current_time);
	
	// size file_name string, fill it
	char* blocks_dir=malloc(bdir_path_size());
	sprintf(blocks_dir, "%s", (char *) fuse_get_context()->private_data);
	sprintf(blocks_dir+strlen(blocks_dir), "%s", "/blocks");
	sprintf(blocks_dir+strlen(blocks_dir), "%s", "/fusedata.0");
	log_msg(blocks_dir);
	
	// open superblock file, fill it
	int result=access(blocks_dir, F_OK);
	if( result==0 ){ // opened okay 				
		FILE* fd=fopen(blocks_dir,"r+");
		char* fields=malloc(1000);
		log_msg("create_super_block(): fields before fill");
		log_msg(fields);
		
		sprintf(fields, "%s%d", "{creationTime:", (int)current_time);
		sprintf(fields+strlen(fields), ",mounted:1");
		sprintf(fields+strlen(fields), ",devId:20,freeStart:1");
		sprintf(fields+strlen(fields), "%s%d", ",freeEnd:", FREE_BLK_FILES);
		sprintf(fields+strlen(fields), "%s%d", ",root:", FREE_BLK_FILES+1);
		sprintf(fields+strlen(fields), "%s%d}", ",maxBlocks:", MAX_FILES);
		
		log_msg("create_super_block(): fields after fill");
		log_msg(fields);
		fwrite(fields, 1, strlen(fields), fd);
		fclose(fd);			
		free(fields);
	} // else fail silently
	
	free(blocks_dir);
}

void free_block_list(){
	/*
	free_block_list() creates the free block list for the file system, putting in PTRS_PER_BLK pointers
	per block to free blocks. The free blocks occupied by the super block, free block list,
	and root directory are omitted from the list.
	* RETURN: nothing
	*/
	
	// build base file name for all fusedata.X files
	log_msg("free_block_list(): enter");
	char* blocks_dir=malloc(bdir_path_size());
	sprintf(blocks_dir, "%s",(char *) fuse_get_context()->private_data);
	sprintf(blocks_dir+strlen(blocks_dir), "%s", "/blocks/fusedata.");
	int path_pos=strlen(blocks_dir); // since only change digits at the end
	
	// create each free block file, fill it with the appropriate free block #s
	int file_number=1;
	char* curr_block=malloc(MAX_DIGITS_BLOCK_NUM); // no more than 100 digits
	curr_block[0]='\0';
	int j;
	for(j=1; j<(FREE_BLK_FILES+1); j++){ 
		sprintf(blocks_dir+path_pos, "%d", file_number);	
		int result=access(blocks_dir, F_OK);
		if( result==0 ){ // opened okay 				
			FILE* fd=fopen(blocks_dir,"r+");
			int k;
			for(k=0; k<PTRS_PER_BLK; k++){ 
				if( (j-1)*(PTRS_PER_BLK)+k>(FREE_BLK_FILES + 1) ){ 
					sprintf(curr_block, "%d,", (j-1)*PTRS_PER_BLK+k);
					fwrite(curr_block, 1, strlen(curr_block), fd);
				}
			} 
			fclose(fd);
			file_number++;
		}
	}
	free(blocks_dir);
	free(curr_block);
	log_msg("free_block_list(): exit");
}

void create_root_dir(){
	/*
	create_root_dir() creates the root directory inode for the file system. mkdir() is not used for this task,
	as it is a special directory entry, and is conceptually very distinct and should be kept separate.
	* RETURN: nothing
	*/
	log_msg("create_root_dir(): enter");
	time_t current_time;
	time(&current_time);
	
	// build file path string
	int bdir_size=bdir_path_size();
	char* blocks_dir=malloc(bdir_size);
	blocks_dir[0]='\0'; 
	sprintf(blocks_dir+strlen(blocks_dir), "%s",(char *) fuse_get_context()->private_data);
	sprintf(blocks_dir+strlen(blocks_dir), "%s%d", "/blocks/fusedata.", FREE_BLK_FILES+1);
	
	// open the fusedata.X block/file, fill it with the root dir data
	int result=access(blocks_dir, F_OK);
	if(result==0){ // opened okay 				
		FILE* fd=fopen(blocks_dir,"r+");
		char* fields=malloc(1000);
		log_msg("create_root_dir(): fields before fill");
		log_msg(fields);
		
		// gid,uid,mode were given values. dir inode is FILE_SIZE since only takes up 1 FILE_SIZE'd block
		sprintf(fields, "%s%d%s", "{size:", FILE_SIZE, ",uid:1,gid:1,mode:16877");  
		sprintf(fields+strlen(fields), "%s%d%s%d%s%d", ",atime:", (int)current_time, ",ctime:", (int)current_time, ",mtime:", (int)current_time );
		sprintf(fields+strlen(fields), "%s%d%s%d%s", ",linkcount:2,file_name_to_inode_dict:{d:.:", FREE_BLK_FILES+1, ",d:..:", FREE_BLK_FILES+1, "}}");

		log_msg("create_root_dir(): fields after fill");
		log_msg(fields);
		fwrite(fields, 1, strlen(fields), fd);
		fclose(fd);			
		free(fields);
	}
	free(blocks_dir);
	log_msg("create_root_dir(): exit");
}

int opendir_rr(const char* path, struct fuse_file_info * fi){	
	/*
	opendir_rr() takes in a path relative to the filesystem root, and a pointer to a fuse_file_info struct.
	* It then iteratively traverses the file path from the root directory. If the directory is found,
	* then the directory inode is read into memory, and fi->fh is assigned a pointer to this data.
	* Releasedir() will free this pointer/heap data.
	* The caller of this function is responsible for freeing that allocated memory.
	* RETURN: 0 if success, else appropriate error code
	*/
	log_msg("opendir_rr(): enter\r\nopendir_rr(): path=");
	log_msg(path);

	int full_path_pos=0;
	char* next_dir=malloc(strlen(path)); // the upper limit for its size
	char* next_block_num=malloc(MAX_DIGITS_BLOCK_NUM); // no more than 100 digits for block_num
	char* blocks_dir=malloc(bdir_path_size());	
	char* block_data=malloc(FILE_SIZE); // FIX HARD CODING
	next_dir[0]=next_block_num[0]=blocks_dir[0]=block_data[0]='\0';
	
	sprintf(next_block_num+strlen(next_block_num), "%d", FREE_BLK_FILES+1); // FIX HARD CODED INITIALIZATION
	log_msg("opendir_rr(): first next block #");
	log_msg(next_block_num);
	sprintf(blocks_dir, "%s%s", (char *)fuse_get_context()->private_data, "/blocks/fusedata."); 
	int path_pos=strlen(blocks_dir);
	while(1){
		next_dir[0]='\0'; // reset next_dir, so can regenerate it
				
		// get the next dir you're searching for
		while(full_path_pos<strlen(path)){
			full_path_pos++; // skipping the /
			if(path[full_path_pos]=='/') { break; }
			sprintf(next_dir+strlen(next_dir), "%c", path[full_path_pos]);
		} 
		log_msg("opendir_rr(): The next dir will be");
		log_msg(next_dir);
		log_msg("opendir_rr(): Next block num is");
		log_msg(next_block_num);
		
		// enter the current dir you've found
		sprintf(blocks_dir+path_pos, "%s", next_block_num);
		next_block_num[0]='\0'; // next block # is now empty, has to be regenerated
		int result=access(blocks_dir, F_OK);
		if(result!=0) { 
			log_msg("opendir_rr(): error trying to access");
			log_msg(blocks_dir);
			free(next_dir);
			free(next_block_num);
			free(blocks_dir);
			free(block_data);		
			return -EACCES; 
		} // else fail silently
		log_msg("opendir_rr(): successful access of");
		log_msg(blocks_dir);
		
		// read directory file block into memory
		FILE* fd=fopen(blocks_dir, "r+");
		fread(block_data, 1, FILE_SIZE, fd); 
		fclose(fd); 
		if(next_dir[0]=='\0') { // have resolved the entire full path
				free(next_dir);
				free(next_block_num);
				free(blocks_dir);
				if(fi!=NULL){ fi->fh=(uint64_t)block_data; } 
				log_msg("opendir_rr(): exit with data=");
				log_msg(block_data);
				return 0; // success
		} 

		// skip to the dictionary part
		int dict_pos=1;
		while(1){
			dict_pos++;
			if(block_data[dict_pos]=='{'){ break;}
		}
		char* temp_cmp=malloc(strlen(path)); // upper limit 
		// search for all 'd' entries, check if their names match the next dir you're searching for
		while(1){
			dict_pos++;
			if(block_data[dict_pos]=='d'||block_data[dict_pos]=='f'){ 
				int cmp_pos=0;
				while(block_data[dict_pos+2]==next_dir[cmp_pos]){ // +2 to get past the ':' to the actual file name
					dict_pos++;
					cmp_pos++;
				} // if full success, now on the : bit
				char end_of_name=block_data[dict_pos+2]; 
				dict_pos+=2; // if success, dict_pos+2=':', if fail, dict_pos falls short, so advance it to the next ':'
				while(block_data[dict_pos]!=':') { dict_pos++; } 
				dict_pos++; // move past second ':' to get block #
				if(cmp_pos==strlen(next_dir) && end_of_name==':'){ 
					// the dir you're looking for next and this one match, and are not substrings of one another
					log_msg("opendir_rr(): before next block num"); 
					log_msg(next_block_num);
					
					while(block_data[dict_pos]!=',' && block_data[dict_pos]!='}'){ // could be last item in dict
						sprintf(next_block_num+strlen(next_block_num), "%c", block_data[dict_pos]);
						dict_pos++; // get the next block num to search in
					}
					
					log_msg("opendir_rr(): after next block num");
					log_msg(next_block_num);
					break; // exit the loop searching this directory, search for the next
				} // if this 'd' is not the dir you want, keep searching
			}
			else if(block_data[dict_pos]=='}' && block_data[dict_pos+1]=='}'){
				free(next_dir);
				free(next_block_num);
				free(blocks_dir);
				free(temp_cmp);
				free(block_data);
				log_msg("opendir_rr(): exit on directory not found in dict");
				if(fi!=NULL) { fi->fh=(u_int64_t)NULL; } 
				return -ENOENT;// ERROR, read the whole dict and directory not found
			}
		}
		free(temp_cmp);
	} // overall loop ends
	return 0;
}

int getattr_rr(const char *path, struct stat *stbuf) {
	/*
	getattr_rr() takes in a file path and a struct stat object. The file path is then opened using opendir_rr()
	to verify its existence, and to load the contents of the block file into a pointer, namely fi->fh.
	The struct stat object's fields are then filled according to the attributes of the file opened.
	* RETURN: 0 if success, -ENOENT if error opening the file
	*/
	log_msg("getattr_rr(): enter\r\ngetattr_rr(): path=");
	log_msg(path);
	memset(stbuf, 0, sizeof(struct stat)); // so if field left empty, its '0' not garbage
		
	// open directory/file, load it into memory; FUSE does not pass the opened dir/file from our other functions
	struct fuse_file_info * fi=malloc(11*sizeof(uint64_t)); // about right
	opendir_rr(path, fi);
	if(fi->fh==(u_int64_t)NULL){
		log_msg("getattr_rr(): fi->fh==NULL");
		return -ENOENT; 
	}
	log_msg("getattr_rr(): fi->fh!=NULL, is");
	log_msg((char *)fi->fh);
	char *block_data=(char *)fi->fh;

	// load up attributes	
	int field_count=1, data_pos=0, field_start_ch=0; 
	char* current_field=malloc(FILE_SIZE); // excessive upper limit for field value's length
	current_field[0]='\0';
	while(1){
		data_pos++; // so skip { at start, and so field starts at byte after the ','
		field_start_ch=data_pos;
		while(block_data[data_pos]!=':'&& data_pos<FILE_SIZE){ data_pos++; }
		data_pos++; // move past ':'
		while(block_data[data_pos]!=',' && data_pos<FILE_SIZE){
			sprintf(current_field+strlen(current_field), "%c", block_data[data_pos]);
			data_pos++;
		} // copy current field value into a c_str
		// based on field count, set appropriate attribute. files and dirs match fields up until the 5th
		if(field_count==1){ stbuf->st_size=atoi(current_field); }
		else if(field_count==2) { stbuf->st_uid=atoi(current_field); }
		else if(field_count==3) { stbuf->st_gid=atoi(current_field); }
		else if(field_count==4) { stbuf->st_mode=atoi(current_field); }
		else if(field_count==5) { 
			if(block_data[field_start_ch]=='l'){ stbuf->st_nlink=atoi(current_field); } // reg file
			else if(block_data[field_start_ch]=='a') { stbuf->st_atime=atoi(current_field); }
		}
		else if(field_count==6) { 
			if(block_data[field_start_ch]=='a'){ stbuf->st_atime=atoi(current_field); } // reg file
			else if(block_data[field_start_ch]=='c') { stbuf->st_ctime=atoi(current_field); }
		}
		else if(field_count==7) { 
			if(block_data[field_start_ch]=='c'){ stbuf->st_ctime=atoi(current_field); } // reg file
			else if(block_data[field_start_ch]=='m') { stbuf->st_mtime=atoi(current_field); }
		}
		else if(field_count==8) { 
			if(block_data[field_start_ch]=='m'){ stbuf->st_mtime=atoi(current_field); } // reg file
			else if(block_data[field_start_ch]=='l') { stbuf->st_nlink=atoi(current_field); }
		}
		else { break; } // reached the end
		field_count++;		
		current_field[0]='\0'; // empty the string
	}
	
	free(current_field);
	free((void *)fi->fh); // already tested if NULL above
	free(fi);
	log_msg("getattr_rr(): exit");
	return 0;
}

int readdir_rr(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
	/*
	readdir_rr() reads a directory specified by the path parameter and fills the buffer "buf" passed in with strings
	representing the name of each entry in the directory. The fuse_fill_dir_t filler function is used to do so.
	* RETURN: 0 if success, -EBADFD (bad file descriptor) if fi->fh is invalid
	*/
	log_msg("readdir_rr(): path=");
	log_msg(path);
	
	// load the directory inode data passed in from opendir_rr()'s fi->fh
	char* block_data=(char *)fi->fh;
	if(fi->fh==(u_int64_t)NULL) { 
		log_msg("\readdir_rr(): fi->fh==NULL");
		return -EBADFD; 
	}

	// set up memory for current_entry
	char* current_entry=malloc(strlen(path)); // upper limit
	sprintf(current_entry, "%c%c", '/', '\0');
	int path_pos=strlen(current_entry);
	int dict_pos=1; // skip initial {
	while(1){ // loop until find dictionary
		dict_pos++;
		if(block_data[dict_pos]=='{'){ break;}
	}
	dict_pos+=3; // 1 to "d", 1 to ":", 1 to file name start
	
	// loop through reading each entry into fuse's "buf", return when done
	while(1){
		while(block_data[dict_pos]!=':' && dict_pos < FILE_SIZE) { 
			sprintf(current_entry+strlen(current_entry), "%c", block_data[dict_pos]);
			dict_pos++; 
		}
		filler(buf, current_entry +1, NULL, 0); // was only giving me 1 char since overwrote [0] to \0, got rid of the "/"
		log_msg("readdir_rr(): current_entry=");
		log_msg(current_entry);
		current_entry[path_pos]='\0'; // str is now "empty"
	
		// skip until next entry begins or end of dict
		while( (block_data[dict_pos]!=',' && block_data[dict_pos]!='}') && dict_pos < FILE_SIZE ){ dict_pos++;}
		if(block_data[dict_pos]==','){ dict_pos+=3; } // advance past those 3 chars ',' 'd' ':'
		else if(block_data[dict_pos]=='}'){  // success
			free(current_entry); 
			log_msg("readdir_rr(): exit -> end of dict");
			return 0; // you've read the dictionary
		}
		if(dict_pos>=FILE_SIZE){ 
			free(current_entry);
			log_msg("readdir_rr(): exit -> exceeded 4096");
			return -EBADFD; 
		} // serious error
	}
}

int open_rr(const char *path, struct fuse_file_info *fi) {
	/*
	open_rr() opens the file specified by the path parameter and loads its inode's data into
	fi->fh for other functions like read_rr() to make use of. release_rr() frees this heap
	allocated memory.
	* RETURN:
	*/
	
	// open the directory path, load the file inode
	log_msg("open_rr(): enter\r\nopen(): path=");
	log_msg(path);
	opendir_rr(path, fi); // load the file's inode into fi->fh
	log_msg("open_rr(): fi="); 
	if(fi->fh !=(u_int64_t) NULL){ log_msg((char *)fi->fh); }
	else {  // if NULL, problem opening the file
		log_msg("open_rr(): fi->fh==NULL"); 
		return -ENOENT;
	}	
	char* block_data=(char*)fi->fh;
	
	// find status of INDIRECT field
	int loc_pos=1, indirect=-1;	
	while(block_data[loc_pos]!='i' || block_data[loc_pos+1] !='n' || block_data[loc_pos+2] !='d'){ loc_pos++; }
	while(block_data[loc_pos-1]!=':'){ loc_pos++; } // get to first char after :
	if(block_data[loc_pos]=='1'){
		log_msg("open_rr(): indirect is 1");		
		indirect=1;
	}
	else if(block_data[loc_pos]=='0'){
		log_msg("open_rr(): indirect is 0");				
		indirect=0;
	}
	
	// get data/indirect block location
	while(block_data[loc_pos]!='l' || block_data[loc_pos+1] !='o' || block_data[loc_pos+2] !='c'){ loc_pos++; }
	while(block_data[loc_pos-1]!=':'){ loc_pos++; } // get to first char after :
	char* location=malloc(MAX_DIGITS_BLOCK_NUM); // no more than 100 digits for block #
	location[0]='\0'; // strlen =0
	while(block_data[loc_pos]!='}'){ 
		sprintf(location+strlen(location), "%c", block_data[loc_pos]); 
		loc_pos++;
	} 
	char* abs_loc=malloc(bdir_path_size()); 
	sprintf(abs_loc, "%s%s%s", (char*)fuse_get_context()->private_data, "/blocks/fusedata.", location);
	int path_name_fd=strlen(abs_loc)-2; // so counts from 0, and precedes .	
	
	// take action based on INDIRECT field
	if(!indirect){
		char* file_data=malloc(FILE_SIZE);
		int result=access(abs_loc, F_OK);
		log_msg("open_rr(): direct location=");
		log_msg(abs_loc);			
		if(result==0){
			log_msg("open_rr() direct file opened");
			FILE* fh=fopen(abs_loc, "r");
			fread(file_data, 1, FILE_SIZE, fh); 
			fclose(fh);
		} // else fail silently
		log_msg("open_rr(): direct file_data=");
		log_msg(file_data);		
		if(fi!=NULL){ fi->fh=(u_int64_t)file_data; }
		// passing out file_data, do not free
	}
	else { // if INDIRECT, need to know # of blocks
		int num_blocks=0;
		
		// get the indirect block's data
		char* file_data=malloc(FILE_SIZE);
		int result=access(abs_loc, F_OK);
		log_msg("open_rr(): ind_location=");
		log_msg(abs_loc);
		if(result==0){
			log_msg("open_rr() ind file opened");
			FILE* fh=fopen(abs_loc, "r");
			fread(file_data, 1, FILE_SIZE, fh);
			fclose(fh);
		} // else fail silently
		log_msg("open_rr(): indirect block data=");
		log_msg(file_data);

		// just count the # of blocks for this file, don't record what they are
		int ind_pos=0;
		while(file_data[ind_pos]!=',' || file_data[ind_pos+1]!='0'){
			ind_pos++; // move past the comma, if first run, skip a byte, at least 1 byte before ,
			while(file_data[ind_pos]!=','){ ind_pos++; }
			num_blocks++;
		}
		
		// read a block #, add its data to the buffer, repeat
		char* current_block_num=malloc(MAX_DIGITS_BLOCK_NUM); // max 100 digits for block #
		char* ultra_file=malloc(num_blocks*FILE_SIZE); 
		current_block_num[0]=ultra_file[0]='\0';
		int len_ufile=0;
		ind_pos=0; // now read in block #s
		while(ind_pos==0 || file_data[ind_pos-1]!=',' || file_data[ind_pos]!='0'){ // first run, just proceed, then continue until end of block list
			while(file_data[ind_pos]!=','){ 
				sprintf(current_block_num+strlen(current_block_num), "%c", file_data[ind_pos]);
				ind_pos++; 
			}
			sprintf(abs_loc+path_name_fd, "%s", current_block_num);
			current_block_num[0]='\0'; // empty str now
			
			int result=access(abs_loc, F_OK);
			log_msg("open_rr(): ind_location=");
			log_msg(abs_loc);
			if(result==0){
				log_msg("open_rr() ind file opened");
				FILE* fh=fopen(abs_loc, "r");
				fread(ultra_file+len_ufile, 1, FILE_SIZE, fh); 
				fclose(fh);
				len_ufile+=FILE_SIZE;
			} // else fail silently
			ind_pos++;
		}
		
		free(file_data);
		free(current_block_num);
		log_msg("open_rr(): ultra_file");
		log_msg(ultra_file);
		if(fi!=NULL){ fi->fh=(u_int64_t)ultra_file; }
		// passing out ultra_file, do not free
	}
	free(abs_loc);
	free(location);
	log_msg("open_rr(): exit");
	return 0;
}

int read_rr(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
	/*
	read_rr() takes in a path to a file, an offset to read from, and a size of bytes to read.
	* The size parameter corresponds to the size of the fi->fh c_str passed in, holding the contents
	* of the file in memory.
	* The buffer "buf" is filled by the data the caller wants to read from the file.
	* FUSE then will only present to the end user the # of bytes specified in the "size" attribute of the file,
	* retrieved during getattr_rr().
	* RETURN: the size parameter passed in
	*/
	log_msg("read_rr(): enter\r\nread_rr(): path=");
	log_msg(path);
	log_msg("read_rr(): size/offset to read");
	char* flags=malloc(MAX_DIGITS_BLOCK_NUM); // fix hard coding
	sprintf(flags, "%d/%d", (int)size, (int)offset);
	log_msg(flags);
	free(flags);
	log_msg("read_rr(): fi->fh");
	if(fi!=NULL && fi->fh!=(u_int64_t)NULL) { log_msg((char*)fi->fh); }	
	log_msg("read_rr(): buf before=");
	log_msg(buf);

	// actual, non-logging action
	memcpy(buf, (void*)(fi->fh + offset) , size); // size, offset, and fi already validly provided by fuse. nothing else to do

	log_msg("read_rr(): buf after=");
	log_msg(buf);
	log_msg("read_rr(): exit");
	return size;
}

int release_rr(const char * path, struct fuse_file_info * fi){
	/*
	release_rr() closes a file opened by open_rr() by freeing the file's data loaded into memory as fi->fh.
	*/
	log_msg("release(): enter");
	if( fi!=NULL && fi->fh!=(u_int64_t)NULL){
		log_msg("release(): fi->fh");
		log_msg((char*)fi->fh);
		free((void*)fi->fh);
		fi->fh=(u_int64_t)NULL;
	}
	log_msg("release(): exit");
	return 0;
}

int releasedir_rr(const char * path, struct fuse_file_info * fi){
	/*
	releasedir_rr() closes a directory opened by opendir_rr() by freeing the file's data loaded into memory as fi->fh.
	*/
	log_msg("releasedir(): enter");
	if( fi!=NULL && fi->fh!=(u_int64_t)NULL){
		log_msg("releasedir(): fi->fh");
		log_msg((char*)fi->fh);
		free((void*)fi->fh);
		fi->fh=(u_int64_t)NULL;
	}
	log_msg("releasedir(): exit");
	return 0;
}

int mkdir_rr(const char *path, mode_t mode){
	/*
	mkdir_rr() takes in a file path and creates a directory at that path if the directory does not already exist.
	* The mode paramter is ignored for this implementation, as modes are not used.
	* NOTE: if a file exists of the same name as the directory to make, the directory will not be made.
	* NOTE: does not check if there is enough space in the parent directory for an additional entry.
		* If caller pushes these limits, the resulting behavior is not guaranteed to be stable.
	* NOTE: no behavior guaranteed for case of mkdir d1/d2 where d1 and d2 both don't exist.
	* RETURN: 0 if success, else appropriate error code following ERRNO standard error codes.
	*/
	// doesnt allow for making dirs of same name as files
	log_msg("mkdir_rr(): enter\r\nmkdir_rr(): path=");
	log_msg(path);
	
	// defensively verify that directory does not already exist
	struct fuse_file_info * fi=malloc(sizeof(struct fuse_file_info));
	int result=opendir_rr(path, fi);
	if(result!=-ENOENT){ 
		log_msg("mkdir_rr(): exit, file already exists/error"); // log the inode's data if it does
		if(fi->fh!=(u_int64_t)NULL) { 
			log_msg("mkdir_rr(): fi->fh="); 
			log_msg((char*)fi->fh); 
			free((void*)fi->fh);
		}
		free(fi);
		return -EEXIST; 
	}
	
	log_msg("mkdir_rr(): SUCCESS. dir does not already exist");	
	int free_block=first_free_block();
	char* debug=malloc(MAX_DIGITS_BLOCK_NUM);
	sprintf(debug, "%d", free_block);
	log_msg("mkdir_rr(): free_block #=");
	log_msg(debug);
		
	// open that block, get fi->fh as the FILE_SIZE bytes of 0s
	char* file_data=malloc(FILE_SIZE); 
	char* file_name=malloc(bdir_path_size()); 
	sprintf(file_name, "%s%s%d", (char*)fuse_get_context()->private_data, "/blocks/fusedata.", free_block);
	result = access(file_name, F_OK);
	if(result!=0){
		log_msg("mkdir_rr(): error opening free block");
		log_msg("mkdir_rr(): file_name=");
		log_msg(file_name);
		free(fi);
		free(debug);
		free(file_data);
		free(file_name);
		return -EACCES;
	}
	FILE* fh=fopen(file_name, "r+");
	fread(file_data, 1, FILE_SIZE, fh); // hard coded
	
	// load the parent dir's inode
	int parent_pos=strlen(path)-1; // so starts at last char
	while(path[parent_pos]!='/'){ parent_pos--; }
	char* parent_path=malloc(strlen(path)); // upper limit 
	strncpy(parent_path, path, parent_pos+1); // so / is pos 0, but 1 byte gets copied
	parent_path[parent_pos+1]='\0';
	result=opendir_rr(parent_path, fi); // assumes this doesn't fail
	free(parent_path);
	int new_dir_pos=parent_pos+1;

	// extract the parent dir's block #
	char* parent_data=(char*)fi->fh; // no malloc needed
	int parent_block_pos=0;
	while(parent_data[parent_block_pos]!=':' || parent_data[parent_block_pos+1]!='.' || parent_data[parent_block_pos+2]!=':'){
		parent_block_pos++;
	} 
	parent_block_pos+=3; // move past :.:
	char* parent_block_num=malloc(MAX_DIGITS_BLOCK_NUM); // upper limit for num digits in block #
	parent_block_num[0]='\0'; // strlen=0
	while(parent_data[parent_block_pos]!=','){
		sprintf(parent_block_num+strlen(parent_block_num), "%c", parent_data[parent_block_pos]);
		parent_block_pos++;
	}
	
	// fill the new dir with an appropriate entry
	log_msg("mkdir_rr(): file_data before filling=");
	log_msg(file_data);
	fill_dir_inode(file_data, atoi(parent_block_num), free_block); // also add size
	file_data[strlen(file_data)]='0'; // not \0 anymore
	log_msg("mkdir_rr(): file_data after filling=");
	log_msg(file_data);
	// write the new dir file's data out to disk
	fseek(fh,0,SEEK_SET);
	int success=fwrite(file_data, 1, FILE_SIZE, fh);
	fclose(fh);
	
	// write entry into parent dir data
	parent_block_pos=0;
	while(parent_data[parent_block_pos]!='}' || parent_data[parent_block_pos+1]!='}'){ parent_block_pos++; }
	char* new_entry=malloc(strlen(path)); // upper limit 
	new_entry[0]='\0';
	sprintf(new_entry, "%s", ",d:");
	snprintf(new_entry+strlen(new_entry), strlen(path)-new_dir_pos+1, "%s", path+new_dir_pos);
	sprintf(new_entry+strlen(new_entry), "%s%d%s", ":", free_block, "}}");
	sprintf(parent_data+parent_block_pos, "%s", new_entry);
	parent_data[strlen(parent_data)]='0'; // so not '\0' and truncated
	free(new_entry);

	// form file name of parent dir, open, and write entry
	file_name[0]='\0';
	sprintf(file_name, "%s%s%d", (char*)fuse_get_context()->private_data, "/blocks/fusedata.", atoi(parent_block_num));
	result = access(file_name, F_OK);
	if(result!=0){
		log_msg("mkdir_rr(): error opening parent block");
		log_msg("mkdir_rr(): file_name=");
		log_msg(file_name);
		free(fi);
		free(debug);
		free(file_data);
		free(file_name);
		free(parent_block_num);
		return -EACCES;
	}
	FILE* fh_parent=fopen(file_name, "w"); //overwrite everything
	fwrite(parent_data, 1, FILE_SIZE, fh_parent);
	fclose(fh_parent);	
	
	// only remove the free block from the list if this all succeeded
	if(success){
		// once file written successfully, remove the free block from the free block list
		int removed=remove_free_block(free_block);
		debug[0]='\0';
		sprintf(debug, "%d", removed);
		log_msg("mkdir_rr(): removed errors?");
		log_msg(debug);
	}
	free(debug);
	free(fi);
	free(file_name);
	free(file_data);
	free(parent_block_num);
	log_msg("mkdir_rr(): exit");
	return 0;
}
	
int first_free_block(){
	/*
	first_free_block returns an int representing the first free block available in the file system.
	* When all free blocks are removed from the free block list, a leading ',' is left in place so that
	* it can be determined there are no free blocks left from examining the first two characters.
	*/
	log_msg("first_free_block(): enter");
	int current_block_num=1;
	int first_free=-1;
	char* full_path=malloc(bdir_path_size());
	full_path[0]='\0';
	sprintf(full_path, "%s%s", (char *)fuse_get_context()->private_data, "/blocks/fusedata.");
	int full_path_pos=strlen(full_path);
	while(current_block_num < FREE_BLK_FILES+1) { 
		sprintf(full_path+full_path_pos, "%d", current_block_num);
		int result=access(full_path, F_OK);
		if(result==0){
			FILE* fh=fopen(full_path, "r");
			char* file_data=malloc(FILE_SIZE); 
			fread(file_data, 1, FILE_SIZE, fh); 
			int file_pos=0;
			char * first_free_ch=malloc(MAX_DIGITS_BLOCK_NUM); // max 100 digits for num digits in block #
			first_free_ch[0]='\0';
			log_msg("first_free_block(): opened file data=");
			log_msg(file_data);
			while(file_data[file_pos]!=','){
				sprintf(first_free_ch+strlen(first_free_ch), "%c", file_data[file_pos]);
				file_pos++;
			}
			if(first_free_ch[0]!='\0'){ // if there is a free block in this block				
				first_free=atoi(first_free_ch);
				log_msg("first_free_block(): exit, found a free block=");
				log_msg(first_free_ch);
				free(file_data);
				free(first_free_ch);
				free(full_path);
				return first_free;
			}
			free(file_data);
			free(first_free_ch);
		} // you've found no blocks in this file, proceed to next free block file
		current_block_num++;
	}
	log_msg("first_free_block(): exit, found no blocks");
	free(full_path);
	return first_free;
}	

void fill_dir_inode(char* file_data, int parent_block, int free_block){ // add SIZE
	/*
	fill_dir_inode() takes in a char* to a dir inode's data, the # of its parent block, the # of its free block,
	* and appropriately fills in all the fields for the directory inode.
	* RETURN: none
	*/
	time_t current_time;
	time(&current_time);
	sprintf(file_data, "%s%d%s", "{size:", FILE_SIZE, ",uid:1,gid:1,mode:16877,atime:"); // have to update this with code
	sprintf(file_data+strlen(file_data), "%d%s%d%s%d", (int)current_time, ",ctime:", (int)current_time, ",mtime:", (int)current_time);
	sprintf(file_data+strlen(file_data), "%s%d%s%d%s", ",linkcount:2,file_name_to_inode_dict:{d:.:", free_block, ",d:..:", parent_block, "}}"); 
}

int remove_free_block(int rm_block){
	/*
	remove_free_block() removes the given block number from the free block list, wherever it may reside.
	* If that number is the last free block in a given list, a leading ',' is left so that a leading ',0'
	* indicates a free block list with no free blocks left. No block number contains a leading 0,
	* thus this indicates the end of the list.
	* RETURN: 0 if success, -1 otherwise if block not found
	*/
	int current_block_num=1;
	char* full_path=malloc(bdir_path_size());
	sprintf(full_path, "%s%s", (char*)fuse_get_context()->private_data, "/blocks/fusedata.");
	int full_path_pos=strlen(full_path);
	
	while(current_block_num < FREE_BLK_FILES+1) { 
		sprintf(full_path+full_path_pos, "%d", current_block_num);
		int result=access(full_path, F_OK);
		if(result==0){
			FILE* fh=fopen(full_path, "r+");
			char* file_data=malloc(FILE_SIZE);
			fread(file_data, 1, FILE_SIZE, fh); 
			int file_pos=0;
			char * current_block_num=malloc(MAX_DIGITS_BLOCK_NUM); // max 100 digits in max block #
			current_block_num[0]='\0';	// search until the last free number
			while(file_pos == 0 || file_data[file_pos-1]!=',' || file_data[file_pos]!='0'){
				while(file_data[file_pos]!=','){ // record each current_block number to compare
					sprintf(current_block_num+strlen(current_block_num), "%c", file_data[file_pos]);
					file_pos++;
				}
				if(atoi(current_block_num)==rm_block){// remove that block, and its comma only if 0 is not after the comma
					int bytes_removed=strlen(current_block_num);
					if(file_data[file_pos+1]!='0'){ bytes_removed++; } // to know how many bytes to append to end
					fseek(fh,0,SEEK_SET);
					int written=fwrite(file_data+bytes_removed, 1, FILE_SIZE-bytes_removed, fh);					
					char* append_zeros=malloc(MAX_DIGITS_BLOCK_NUM);
					log_msg("rm_fb(): # file_data written");
					sprintf(append_zeros, "%d", written);
					log_msg(append_zeros);
					memset(append_zeros, '0', bytes_removed);
					fwrite(append_zeros, 1, bytes_removed, fh);
					fclose(fh);
					free(append_zeros);
					free(file_data);
					return 0; // success
				}
				current_block_num[0]='\0'; // sets strlen =0, so can compare next block #
				file_pos++; // move past the ,
			}			
		}
		current_block_num++;
	}
	free(full_path);
	return -1; 
}

int rename_rr(const char* path, const char* new_name){
	/*
	rename_rr() takes in a path name to a file and renames the file.
	* The file's entry in its parent directory is updated.
	* RETURN: 0 if successful, else appropriate ERRNO error code
	*/
	log_msg("rename_rr(): enter\r\nrename_rr(): current=");
	log_msg(path);
	log_msg("rename_rr(): new_name=");
	log_msg(new_name);
	
	// find pos in path that separates parent dir and file name
	int parent_pos=strlen(path)-1; // so starts at last char
	while(path[parent_pos]!='/'){  parent_pos--; }
	int file_name_pos=parent_pos+1; // to pull out file name
	
	// copy over parent dir path, old name, new name
	char* parent_path=malloc(strlen(path)); // upper limit
	strncpy(parent_path, path, parent_pos+1); // so / is pos 0, but 1 byte gets copied
	parent_path[parent_pos+1]='\0'; // so acts like sprintf
	char* old_name=malloc(strlen(path));
	sprintf(old_name, "%s", path+file_name_pos);
	char* new_name_ch=malloc(strlen(path));
	sprintf(new_name_ch, "%s", new_name+file_name_pos); // same dir, different name
	int len_diff=strlen(new_name_ch)-strlen(old_name);
	log_msg("rename_rr(): old_name=");
	log_msg(old_name);
	log_msg("rename_rr(): new_name_ch=");
	log_msg(new_name_ch);
	log_msg("rename_rr(): parent_path=");	
	log_msg(parent_path);
	// load parent dir inode
	struct fuse_file_info * fi=malloc(sizeof(struct fuse_file_info));
	fi->fh=(u_int64_t)NULL; // just to be safe
	int result=opendir_rr(parent_path, fi); // assuming this doesn't fail
	if(fi->fh==(u_int64_t)NULL){
		free(fi);
		free(old_name);
		free(new_name_ch);
		free(parent_path);
		log_msg("rename_rr(): failed to get parent dir inode");
		return -1;
	}
	char* parent_data=(char*)fi->fh; 
	
	// extract the parent dir's block #
	int parent_block_pos=0;
	while(parent_data[parent_block_pos]!=':' || parent_data[parent_block_pos+1]!='.' || parent_data[parent_block_pos+2]!=':'){
		parent_block_pos++;
	} 
	parent_block_pos+=3; // move past :.:
	char* parent_block_num=malloc(MAX_DIGITS_BLOCK_NUM); // hardcoded 100 digits max for block #
	parent_block_num[0]='\0'; // strlen=0
	while(parent_data[parent_block_pos]!=','){
		sprintf(parent_block_num+strlen(parent_block_num), "%c", parent_data[parent_block_pos]);
		parent_block_pos++;
	}	
	
	
	int exists=is_entry(parent_data, new_name_ch); // the parent dir removed
	if(exists==1){ // if the new name you want is already taken
		unlink_rr(new_name); // unlink that file, then rename yourself
		if(fi->fh!=(u_int64_t)NULL){ free((char*)fi->fh); } // discard the stale parent inode
		opendir_rr(parent_path, fi); // load a fresh parent inode reflecting the change
		log_msg("rename_rr(): non-stale inode=");
		log_msg((char*)fi->fh); // assumes opendir_rr() worked
		parent_data=(char*)fi->fh;
	}
	
	// find position for old entry start and end
	int dict_pos=1, old_name_start=0, old_name_end=0;
	while(parent_data[dict_pos]!='{'){ dict_pos++; } // skip to the dictionary
	// then look for every 'd' and 'f' entry, compare
	while(1){
		dict_pos++;
		if(parent_data[dict_pos]=='d'||parent_data[dict_pos]=='f'){ // so can getattr_rr better // +1 is the :
			int cmp_pos=0;
			if(parent_data[dict_pos+2]==old_name[cmp_pos]) { old_name_start=dict_pos+2; }
			log_msg("rename_rr(): current pos");
			while(parent_data[dict_pos+2]==old_name[cmp_pos]){
				dict_pos++;
				cmp_pos++;
			} // if full success, now on the : bit
			char end_of_name=parent_data[dict_pos+2];
			dict_pos+=2; // if success, dict_pos+2=':', if fail, dict_pos='d', so move past it and first ':'
			while(parent_data[dict_pos]!=':') { dict_pos++; } // if success, this gets skipped, if fail, this moves to ':'
			dict_pos++; // in either case, move past second ':' to get block #
			if(cmp_pos==strlen(old_name) && end_of_name==':'){ // you have a match from start of entry to start of searching entry, and searching entry is not a substring of current entry matching from start
				old_name_end=dict_pos-1; // at the semicolon
				// ie, so "Fold" and "Folder" don't match
				char* after_old_entry=malloc(FILE_SIZE); // max upper limit
				strncpy(after_old_entry, parent_data+old_name_end, FILE_SIZE-old_name_end);
				log_msg("rename_rr(): after old entry");
				log_msg(after_old_entry);
				log_msg("rename_rr(): parent_data b/f fix");
				log_msg(parent_data);
				parent_data[old_name_start]='\0'; // string ends before the old name starts
				sprintf(parent_data+strlen(parent_data), "%s", new_name_ch);	
				if(len_diff>=0){  // +1 since old_name_start a ':', 1 to get past it, and new_name_ch to get past the full name
					// -1 since '\0' char usign sprintf, and so sum is 0
					strncpy(parent_data+old_name_start+strlen(new_name_ch), after_old_entry, FILE_SIZE-old_name_end-len_diff);
					log_msg("rename_rr(): len_diff>0");
				}
				else { // new name is shorter
					log_msg("rename_rr(): len_diff<0");
					// +1 since old_name_start a ':', 1 to get past it, and new_name_ch to get past the full name
					// -1 since '\0' char usign sprintf, and so sum is 0
					strncpy(parent_data+old_name_start+strlen(new_name_ch), after_old_entry, FILE_SIZE-old_name_end); // copy over all of that data
					int i;
					for(i=0; i<abs(len_diff); i++){ // add zeros
						parent_data[FILE_SIZE-abs(len_diff)+i-1]='0'; // overwrite the '\0'
					}
				}
				log_msg("rename_rr(): parent_data a/f fix");
				log_msg(parent_data);
				free(after_old_entry);
				break; // exit the loop, your job is done
			} // if this 'd' is not the dir you want, keep searching
		}
		else if(parent_data[dict_pos]=='}' && parent_data[dict_pos+1]=='}'){
			log_msg("rename_rr(): exit on directory not found in dict");
			if(fi->fh!=(u_int64_t)NULL) { free((char*)fi->fh); } 
			free(fi);
			free(parent_block_num);
			free(new_name_ch);
			free(old_name);
			free(parent_path);
			return -ENOENT;// ERROR, read the whole dict and directory not found, maybe return -1
		}
	}

	// open parent dir inode file, so can write to it
	char * file_name=malloc(bdir_path_size()); 
	file_name[0]='\0';
	sprintf(file_name, "%s%s%d", (char*)fuse_get_context()->private_data, "/blocks/fusedata.", atoi(parent_block_num));
	result = access(file_name, F_OK);
	if(result!=0){
		log_msg("mkdir_rr(): error opening parent block");
		log_msg("mkdir_rr(): file_name=");
		log_msg(file_name);
		free(file_name);
		free(parent_block_num);
		free(new_name_ch);
		free(old_name);
		free(parent_path);
		free(fi);
		return -EACCES;
	} // else fails silently
	FILE* fh_parent=fopen(file_name, "w"); //overwrite everything
	fwrite(parent_data, 1, FILE_SIZE, fh_parent);
	fclose(fh_parent);	

	free(file_name);
	free(parent_block_num);
	free(new_name_ch);
	free(old_name);
	free(parent_path);
	free(fi);
	log_msg("rename_rr(): exit");
	return 0;
}

int is_entry(const char* parent_data, const  char* entry){
	log_msg("is_entry(): enter, parent_data=, entry  t=");
	log_msg(parent_data);
	log_msg(entry);
	
	// find position for old entry start and end
	int dict_pos=1;
	while(parent_data[dict_pos]!='{'){ dict_pos++; } // skip to the dictionary
	// then look for every 'd' and 'f' entry, compare
	while(1){
		dict_pos++;
		if(parent_data[dict_pos]=='d'||parent_data[dict_pos]=='f'){ // so can getattr_rr better // +1 is the :
			int cmp_pos=0;
			while(parent_data[dict_pos+2]==entry[cmp_pos]){
				dict_pos++;
				cmp_pos++;
			} // if full success, now on the : bit
			char end_of_name=parent_data[dict_pos+2];
			dict_pos+=2; // if success, dict_pos+2=':', if fail, dict_pos='d', so move past it and first ':'
			while(parent_data[dict_pos]!=':') { dict_pos++; } // if success, this gets skipped, if fail, this moves to ':'
			dict_pos++; // in either case, move past second ':' to get block #
			if(cmp_pos==strlen(entry) && end_of_name==':'){ // you have a match from start of entry to start of searching entry, and searching entry is not a substring of current entry matching from start
				// entry has been found
				log_msg("is_entry(): exit on found entry");
				return 1;
			} 
		}
		else if(parent_data[dict_pos]=='}' && parent_data[dict_pos+1]=='}'){
			log_msg("is_entry(): exit on directory not found in dict"); // entry not found
			return -ENOENT;// ERROR, read the whole dict and directory not found, maybe return -1
		}
	}
}

int rmdir_rr(const char * path){
	/*
	 rmdir_rr() removes the directory pointed to by path.
	 * It does so by calling rm_file, specifying a directory 'd', and 'y'
	 * this is the last link to the directory.
	 * RETURN: returns whatever error code or success code rm_file does
	*/
	return rm_file(path, 'd', 'y');
}

int rm_file(const char* path, char dir_or_reg, char last_link){
	/*
	rm_file() takes in a path to a file to be removed, a character indicating its type ('d' for dir, 'f' for regular file)
	* and removes that file. If it is a regular file and it is using an indirect block, and this is the last link being moved,
	* then all the blocks of the indirect block list are removed. Otherwise the entry is just removed from the parent dir, and 
	* unlink() takes care of decrementing the link count.
	*/
	log_msg("rm_file(): enter\r\nrm_file(): path=");
	log_msg(path);
	
	// extract dir name from file path
	int parent_pos=strlen(path)-1; // so starts at last char
	while(path[parent_pos]!='/'){  parent_pos--; }
	int file_name_pos=parent_pos+1; // to pull out file name
	// copy over parent dir path,  dir name
	char* parent_path=malloc(strlen(path));
	strncpy(parent_path, path, parent_pos+1); // so / is pos 0, but 1 byte gets copied
	parent_path[parent_pos+1]='\0'; // so acts like sprintf
	char* dir_name=malloc(strlen(path));
	sprintf(dir_name, "%s", path+file_name_pos);
	log_msg("rm_file(): dir_name=");
	log_msg(dir_name);
	log_msg("rm_file(): parent_path=");	
	log_msg(parent_path);
	
	// pull up parent dir inode
	struct fuse_file_info * fi=malloc(sizeof(struct fuse_file_info));
	fi->fh=(u_int64_t)NULL; // just to be safe
	opendir_rr(parent_path, fi); // assumes this doesn't fail
	if(fi->fh==(u_int64_t)NULL){
		free(fi);
		free(dir_name);
		free(parent_path);
		log_msg("rm_file(): failed to get parent dir inode");
		return -ENOENT;
	}
	char* parent_data=(char*)fi->fh; 
	
	// extract the parent dir's block #, so can open fusedata.X later to write back changes
	int parent_block_pos=0;
	while(parent_data[parent_block_pos]!=':' || parent_data[parent_block_pos+1]!='.' || parent_data[parent_block_pos+2]!=':'){
		parent_block_pos++;
	} 
	parent_block_pos+=3; // move past :.:
	char* parent_block_num=malloc(MAX_DIGITS_BLOCK_NUM); // hardcoded
	parent_block_num[0]='\0'; // strlen=0
	while(parent_data[parent_block_pos]!=','){
		sprintf(parent_block_num+strlen(parent_block_num), "%c", parent_data[parent_block_pos]);
		parent_block_pos++;
	}
	
	// get the file's inode block # from parent entry
	char* file_inode_block_num=malloc(MAX_DIGITS_BLOCK_NUM); // hard coding
	file_inode_block_num[0]='\0'; // will be modified to what we need
	int succeeded=find_block_num(parent_data, parent_block_pos, dir_name, file_inode_block_num);	// your entry must be after the d:.:xx , so save a bit of scanning
	if(succeeded==-ENOENT || succeeded==-1){
		log_msg("rm_file(): exit on error finding block # in parent directory");
		return -ENOENT;
	}
	
	// find dir name position in parent dir inode's dictionary, get rid of it, handle byte appending
	int len_diff=strlen(dir_name);
	int dict_pos=1, dir_name_start=0, dir_name_end=0;
	while(parent_data[dict_pos]!='{'){ dict_pos++; } // skip to the dictionary, then look for every 'd' and 'f' entry, compare
	while(1){
		dict_pos++;
		if(parent_data[dict_pos]==dir_or_reg){ // so if called for dirs, only considers dirs as valid entries to check
			int cmp_pos=0;
			if(parent_data[dict_pos+2]==dir_name[cmp_pos]) { dir_name_start=dict_pos-1; } // get rid of the preceding comma, leave the trailing symbol, ',' or '}'
			while(parent_data[dict_pos+2]==dir_name[cmp_pos]){ // see how many bytes match, in case of subs trings
				dict_pos++;
				cmp_pos++;
			} // if full success, now on the : bit
			char end_of_name=parent_data[dict_pos+2];
			dict_pos+=2; // if success, dict_pos+2=':', if fail, dict_pos fell short, so move it to the first ':'
			while(parent_data[dict_pos]!=':') { dict_pos++; } // if success, this gets skipped, if fail, this moves to ':'
			dict_pos++; // in either case, move past second ':' to get block #
			if(cmp_pos==strlen(dir_name) && end_of_name==':'){ // you have a match from start of entry to start of searching entry, and searching entry is not a substring of current entry matching from start
				while(parent_data[dict_pos]!=',' && parent_data[dict_pos]!='}') { dict_pos++; }
				dir_name_end=dict_pos; // at the comma // ie, so "Fold" and "Folder" don't match
				char* after_old_entry=malloc(FILE_SIZE); // hard coded
				strncpy(after_old_entry, parent_data+dir_name_end, FILE_SIZE-dir_name_end);
				log_msg("rm_file(): after old entry");
				log_msg(after_old_entry);
				log_msg("rm_file(): parent_data b/f fix");
				log_msg(parent_data);
				parent_data[dir_name_start]='\0'; // string ends before the old name starts
				log_msg("rm_file(): len_diff<0"); // +1 since dir_name_start a ':', 1 to get past it, and new_name_ch to get past the full name
				// -1 since '\0' char usign sprintf, and so sum is 0
				strncpy(parent_data+dir_name_start, after_old_entry, FILE_SIZE-dir_name_end); // copy over all of that data
				int i;
				for(i=0; i<abs(len_diff); i++){ // add zeros
					parent_data[FILE_SIZE-abs(len_diff)+i-1]='0'; // overwrite the '\0'
				}				
				log_msg("rm_file(): parent_data a/f fix");
				log_msg(parent_data);
				free(after_old_entry);
				break; // exit the loop, your job is done
			} // if this 'd' is not the dir you want, keep searching
		}
		else if(parent_data[dict_pos]=='}' && parent_data[dict_pos+1]=='}'){
			log_msg("rm_file(): exit on directory not found in dict");
			if(fi->fh!=(u_int64_t)NULL) { free((char*)fi->fh); } 
			free(fi);
			free(parent_block_num);
			free(dir_name);
			free(parent_path);
			return -ENOENT;// ERROR, read the whole dict and directory not found, maybe return -1
		}
	}

	// write parent data to its block file
	int okay=flush_block_file(parent_data, atoi(parent_block_num));
	if(okay<0) {  return okay; } // message alreadry logged

	// get file's inode data
	if(dir_or_reg=='f'){
		FILE* curr_file=open_block_file(atoi(file_inode_block_num));
		char* curr_file_data=malloc(FILE_SIZE);
		curr_file_data[0]='\0';
		fread(curr_file_data, 1, FILE_SIZE, curr_file);
		log_msg("rm_file(): curr_file_data=");
		log_msg(curr_file_data);
		int indirect=get_indirect(curr_file_data);
		int dbn_int=get_data_block_num(curr_file_data);
		fclose(curr_file);	
		char* data_block_num=malloc(MAX_DIGITS_BLOCK_NUM); // max num digits for block #
		sprintf(data_block_num, "%d", dbn_int);
		if(last_link=='y'){ 
			// go into the inode block, find the data block
			// if indirect==1, add_free_block for each entry in there, do not clear indirect block contents
			// whether its indirect=1 or 0, free up the data block, and the inode block afterward
			if(indirect==1){
				int success=free_indirect_blocks(data_block_num);
				if(success<0){ return success; } // error, already logged
			}
			// if indirect==0, just add_free_block the data block and the inode block
			add_free_block(file_inode_block_num);
			add_free_block(data_block_num);
		}
			free(curr_file_data);
	}
	else if(dir_or_reg=='d'){ add_free_block(file_inode_block_num); }
	// if indirect=1, go into the inode block, find the indirect block

	free(file_inode_block_num);
	free(parent_block_num);
	free(dir_name);
	free(parent_path);
	free(fi);
	log_msg("rm_file(): exit");
	return 0;
}

int find_block_num(char* inode_data, int inode_pos, char* entry_name_to_find, char* block_num_of_entry){
	/*
	find_block_num() takes loaded inode data, your data position in it, what entry you're looking for,
	*  and stores the block # of that entry in a string you pass in.
	* RETURN: 0 if success, else appropriate ERRNO error code
	*/
	block_num_of_entry[0]='\0';
	log_msg("find_block_num(): enter");
	while(1){ // scan all entries or until end of dictionary
		inode_pos++;
		if(inode_data[inode_pos]=='d'||inode_data[inode_pos]=='f'){ 
			int cmp_pos=0;
			while(inode_data[inode_pos+2]==entry_name_to_find[cmp_pos]){
				inode_pos++;
				cmp_pos++;
			} // if full success, now on the : bit
			char end_of_name=inode_data[inode_pos+2];
			inode_pos+=2; // if success, inode_pos+2=':', if fail, inode_pos='d', so move past it and first ':'
			while(inode_data[inode_pos]!=':') { inode_pos++; } // if success, this gets skipped, if fail, this moves to ':'
			inode_pos++; // in either case, move past second ':' to get block #
			if(cmp_pos==strlen(entry_name_to_find) && end_of_name==':'){ //full match, one is not a substring of the other
				log_msg("find_block_num(): before next block num"); // ie, so "Fold" and "Folder" don't match
				log_msg(block_num_of_entry);
				block_num_of_entry[0]='\0';
				while(inode_data[inode_pos]!=',' && inode_data[inode_pos]!='}'){ // could be last item in dict
					sprintf(block_num_of_entry+strlen(block_num_of_entry), "%c", inode_data[inode_pos]);
					inode_pos++; // get the next block num to search in
				}
				log_msg("find_block_num(): exit with inode block #=");
				log_msg(block_num_of_entry);
				return 0; // all okay
			} // if this 'd' is not the dir you want, keep searching
		}
		else if(inode_data[inode_pos]=='}' && inode_data[inode_pos+1]=='}'){
			log_msg("find_block_num(): exit on directory not found in dict");
			return -ENOENT;// ERROR, read the whole dict and directory not found, maybe return -1
		}
	}
	return -1; // inexplicable error
}
	
int flush_block_file(char* block_data, int block_num){
	/*
	flush_block_file() takes in some block_data and a block number,
	* opens the block file, and flushes the data to the block file.
	* RETURN: 0 if success, else appropriat ERRNO error code.
	*/
	log_msg("flush_block_file(): enter");
	
	FILE* fh=open_block_file(block_num);
	if(fh<0) { return (long int)fh; } // error code
	fwrite(block_data, 1, FILE_SIZE, fh);
	fclose(fh);	
	
	log_msg("flush_block_file(): exit");
	return 0;
}

FILE* open_block_file(int block_num){
	/*
	open_block_file() takes in a block # and returns a FILE* to that opened block file.
	*/
	log_msg("open_block_file(): enter");
	char * file_name=malloc(bdir_path_size());
	file_name[0]='\0';
	sprintf(file_name, "%s%s%d", (char*)fuse_get_context()->private_data, "/blocks/fusedata.", block_num);
	log_msg("open_block_file: file_name=");
	log_msg(file_name);
	
	int result = access(file_name, F_OK);
	if(result!=0){
		log_msg("open_block_file(): error opening parent block");
		log_msg("open_block_file(): file_name=");
		log_msg(file_name);
		free(file_name);
		return (FILE*)-EACCES;
	}
	FILE* fh=fopen(file_name, "r+"); 
	free(file_name);
	log_msg("open_block_file(): exit");
	return fh;
}	

int get_indirect(char* file_data){
	/*
	get_indirect() takes in a regular file's inode data and returns a 1 if it has an indirect blcok and a 0 otherwise;
	*/
	log_msg("get_indirect(): enter\r\nget_indirect(): file_data=");
	log_msg(file_data);
	int file_pos=0;
	while(file_data[file_pos]!='i' || file_data[file_pos+1]!='n' || file_data[file_pos+2]!='d'){ file_pos++;}
	while(file_data[file_pos]!=':') { file_pos++; }
	file_pos++; // move past the : 
	char* indirect_status=malloc(5); // a 1 or a 0, extra space to be safe
	indirect_status[0]='\0';
	while(file_data[file_pos]!=','){
		sprintf(indirect_status+strlen(indirect_status), "%c", file_data[file_pos]);
		file_pos++;
	}
	int answer=atoi(indirect_status);
	free(indirect_status);
	log_msg("get_indirect(): exit");
	return answer;
}

int get_data_block_num(char* file_data){
	/*
	get_data_block_num() takes in a regular file's inode data and returns the location of the data block,
	* whether its an indirect block or a direct block.
	*/
	log_msg("get_data_block_num(): enter\r\nget_data_block_num(): file_data=");
	log_msg(file_data);
	int file_pos=0;
	while(file_data[file_pos]!='l' || file_data[file_pos+1]!='o' || file_data[file_pos+2]!='c'){ file_pos++;}
	while(file_data[file_pos]!=':') { file_pos++; }
	file_pos++; // move past the : 
	char* data_block=malloc(MAX_DIGITS_BLOCK_NUM); // max 100 digits for block # 
	data_block[0]='\0';
	while(file_data[file_pos]!='}'){
		sprintf(data_block+strlen(data_block), "%c", file_data[file_pos]);
		file_pos++;
	}
	int answer=atoi(data_block);
	free(data_block);
	log_msg("get_data_block_num(): exit");
	return answer;
}

int add_free_block(char* block_num){
	/*
	add_free_block() takes in a block number that has been freed and inserts it into the appropriate free block list.
	* NOTE: a leading ',' is left in any empty free block list file
	* RETURN: 0 if successful
	*/
	log_msg("add_free_block(): enter\r\nadd_free_block(): block #=");
	log_msg(block_num);	
	int current_block_num=1+(atoi(block_num)/PTRS_PER_BLK); 
	char* full_path=malloc(bdir_path_size()); // definitely hard coded
	full_path[0]='\0';
	
	sprintf(full_path, "%s%s", (char*)fuse_get_context()->private_data, "/blocks/fusedata.");
	int full_path_pos=strlen(full_path);
	sprintf(full_path+full_path_pos, "%d", current_block_num);	
	int result=access(full_path, F_OK);
	if(result==0){
		FILE* fh=fopen(full_path, "r+");
		char* file_data=malloc(FILE_SIZE); 
		fread(file_data, 1, FILE_SIZE, fh); 
		int file_pos=0;
		while(file_data[file_pos]!=',' || file_data[file_pos+1]!='0' ) { file_pos++; }
		sprintf(file_data+file_pos+1, "%s", block_num);
		int old_len=strlen(file_data);
		file_data[strlen(file_data)]=','; // get rid of the \0
		file_data[old_len+1]='0';
		fseek(fh, 0, SEEK_SET);
		fwrite(file_data, 1, FILE_SIZE, fh); // assumes success
		fclose(fh);		
		free(file_data);	
		return 0; // success
	}
	free(full_path);
	return -EACCES;
}

int free_indirect_blocks(char* block_num){
	/*
	free_indirect_blocks() frees all the blocks in the list stored at indirect block # block_num.
	* RETURN: 0 if success, else appropriate error code
	*/
	FILE* fh=open_block_file(atoi(block_num));
	if(fh<0) { return (long int)fh; } // error, already logged
	
	char* file_data=malloc(FILE_SIZE);
	file_data[0]='\0';
	fread(file_data, 1, FILE_SIZE, fh);
	
	char* current_block=malloc(MAX_DIGITS_BLOCK_NUM); // max 100 digits for block num #
	current_block[0]='\0';
	int file_pos=0;
	while(file_pos==0 || file_data[file_pos-1]!=',' || file_data[file_pos]!='0'){
		while(file_data[file_pos]!=','){
			sprintf(current_block+strlen(current_block), "%c", file_data[file_pos]);
			file_pos++;
		}
		add_free_block(current_block);
		file_pos++;
		current_block[0]='\0'; // "empty" the string
	}
	free(file_data);
	free(current_block);
	fclose(fh);
	return 0;
}

int unlink_rr(const char * path){
	/*
	unlink_rr() unlinks the file pointed at by path.
	* RETURN: 0 if success, otherwise appropriate ERRNO error code.
	*/
	// load file, load link count #
	log_msg("unlink_rr(): enter");
	struct fuse_file_info* fi = malloc(sizeof(struct fuse_file_info));
	opendir_rr(path, fi); // assumes this worked
	if(fi==NULL||fi->fh==(u_int64_t)NULL){
		log_msg("unlink_rr(): file inode not found");
		return -ENOENT;
	}
	log_msg("unlink_rr(): fi->fh=");
	log_msg((char*)fi->fh);
	char* inode_data=(char*)fi->fh;
	mod_link_count(inode_data, -1);
	log_msg("unlink_rr(): inode_data=");
	log_msg(inode_data);
	int block_num=get_block_num(path, 'f');	// get block # from inode
	
	// open up the inode block's file on the disk, write back these changes
	char * file_name=malloc(1000); // fix hard coding
	file_name[0]='\0';
	sprintf(file_name, "%s%s%d", (char *)fuse_get_context()->private_data, "/blocks/fusedata.", block_num);
	int result = access(file_name, F_OK);
	if(result!=0){
		log_msg("mkdir_rr(): error opening parent block");
		log_msg("mkdir_rr(): file_name=");
		log_msg(file_name);
		free(file_name);
		free(fi);
		return -EACCES;
	}
	FILE* fh=fopen(file_name, "w"); //overwrite everything
	fseek(fh, 0, SEEK_SET);
	fwrite(inode_data, 1, FILE_SIZE, fh);
	fclose(fh);	
	int new_link_count=get_link_count(inode_data);

	// call rm_file to remove it from the parent entry
	// if linkcount==0, call with 'y' to free up blocks
	if(new_link_count==0){ rm_file(path, 'f', 'y'); }
	else { rm_file(path, 'f', 'n'); }
	
	free(file_name);
	if(fi->fh!=(u_int64_t)NULL){ free((char*)fi->fh); }
	free(fi);
	
	return 0;
}

int get_block_num(const char* path, char dir_or_reg){
	/*
	get_block_num() takes in a path to a file and a char indicating 'd' for dir or 'f' for regular file,
	* and returns the block number of that file in its parent directory's inode.
	*/
	int got_block_num=-1;
	// find pos in path that separates parent dir and file name
	int parent_pos=strlen(path)-1; // so starts at last char
	while(path[parent_pos]!='/'){  parent_pos--; }
	int file_name_pos=parent_pos+1; // to pull out file name

	// copy over parent dir path,  dir name
	char* parent_path=malloc(strlen(path)); // upper limit
	strncpy(parent_path, path, parent_pos+1); // so / is pos 0, but 1 byte gets copied
	parent_path[parent_pos+1]='\0'; // so acts like sprintf
	char* dir_name=malloc(strlen(path)); // upper limit
	sprintf(dir_name, "%s", path+file_name_pos);
	log_msg("get_block_num(): dir_name=");
	log_msg(dir_name);
	log_msg("get_block_num(): parent_path=");	
	log_msg(parent_path);
	
	// load parent dir inode, which is NOT the fi loaded here
	struct fuse_file_info * fi=malloc(sizeof(struct fuse_file_info)); 
	fi->fh=(u_int64_t)NULL; // just to be safe
	opendir_rr(parent_path, fi); // assumes this doesn't fail
	if(fi->fh==(u_int64_t)NULL){
		free(fi);
		free(dir_name);
		free(parent_path);
		log_msg("get_block_num(): failed to get parent dir inode");
		return -ENOENT;
	}
	char* parent_data=(char*)fi->fh; 
	// find position for old entry start and end
	int dict_pos=1;
	while(parent_data[dict_pos]!='{'){ dict_pos++; } // skip to the dictionary
	// then look for every 'd' and 'f' entry, compare
	while(1){
		dict_pos++;
		if(parent_data[dict_pos]==dir_or_reg){ 
			int cmp_pos=0;
			log_msg("get_block_num(): current pos");
			char* one_char=malloc(2);
			sprintf(one_char, "%c", parent_data[dict_pos+2]);
			log_msg(one_char);
			free(one_char);
			while(parent_data[dict_pos+2]==dir_name[cmp_pos]){
				dict_pos++;
				cmp_pos++;
			} // if full success, now on the : bit
			char end_of_name=parent_data[dict_pos+2];
			dict_pos+=2; // if success, dict_pos+2=':', if fail, dict_pos='d', so move past it and first ':'
			while(parent_data[dict_pos]!=':') { dict_pos++; } // if success, this gets skipped, if fail, this moves to ':'
			dict_pos++; // in either case, move past second ':' to get block #
			if(cmp_pos==strlen(dir_name) && end_of_name==':'){ // you have a match, and one is not merely a substring of the other
				// now one past the semi colon
				char* your_block_num=malloc(10); // no more than 10 digits per block #
				your_block_num[0]='\0';
				log_msg("get_block_num(): comparison succeeded");
				while(parent_data[dict_pos]!=',' && parent_data[dict_pos]!='}') {
					sprintf(your_block_num+strlen(your_block_num), "%c", parent_data[dict_pos]);
					dict_pos++;
				 }
				got_block_num=atoi(your_block_num);
				free(your_block_num);
				break; // exit the loop, your job is done
			} // if this 'd' is not the dir you want, keep searching
		}
		else if(parent_data[dict_pos]=='}' && parent_data[dict_pos+1]=='}'){
			log_msg("get_block_num(): exit on directory not found in dict");
			if(fi->fh!=(u_int64_t)NULL) { free((char*)fi->fh); } 
			free(fi);
			free(parent_path);
			free(dir_name);
			return -ENOENT;// ERROR, read the whole dict and directory not found
		}
	}
	if(fi->fh!=(u_int64_t)NULL) { free((char*)fi->fh); }
	free(fi);
	free(parent_path);
	free(dir_name);
	return got_block_num; // success
} 

int create_rr(const char * path, mode_t mode, struct fuse_file_info * fi){
	/*
	create_rr() takes in a path to a new file to create, and creates it.
	* The mode parameter is ignored as modes are not implemented in this filesystem.
	* RETURN: 0 if success, else appropriate ERRNO error code.	
	*/
	log_msg("create_rr(): enter\r\ncreate_rr(): path=");
	log_msg(path);	
	if(fi==NULL){
		log_msg("create_rr(): exit on null FI");
		return -EBADFD;
	}
	if(fi->fh!=(u_int64_t)NULL){ 
		log_msg("create_rr(): fi->fh=");
		log_msg((char*)fi->fh); 
	} // continues regardless
	int result=opendir_rr(path, fi);
	if(result!=-ENOENT){ 
		log_msg("create_rr(): exit, file already exists/error");
		return -EEXIST;
	}
	
	// get two free blocks, 1 for inode, 1 for data
	int free_inode_block=first_free_block();
	remove_free_block(free_inode_block); // need to snatch up two
	int free_data_block=first_free_block();
	remove_free_block(free_data_block); // both calls assumes no errors take place
	
	// get parent directory inode, insert this new file's inode block #
	insert_parent_entry(path, free_inode_block);

	// fill the inode file and write it to a block
	FILE* fh=open_block_file(free_inode_block);
	char* file_data=malloc(FILE_SIZE);
	file_data[0]='\0';
	fread(file_data, 1, FILE_SIZE, fh);
	log_msg("create_rr(): file_data read from file");
	log_msg(file_data);
	fill_reg_inode(file_data, free_data_block);
	fseek(fh, 0, SEEK_SET);
	fwrite(file_data, 1, FILE_SIZE, fh);
	log_msg("create_rr(): file_data written to file");
	log_msg(file_data);
	fclose(fh);	
	free(file_data);
	log_msg("create_rr(): exit");
	return 0;
}

void fill_reg_inode(char* file_data, int free_block){
	/*
	fill_reg_inode() takes in the inode file_data of a new file inode being created, along with its block #, and 
	* fills in the inode attributes appropriately. 
	*/
	time_t current_time;
	time(&current_time);
	file_data[0]='\0';
	log_msg("fill_reg_inode(): enter\r\nfill_reg_inode(): file_data b/f");
	log_msg(file_data);
	sprintf(file_data, "%s", "{size:0,uid:1,gid:1,mode:33261,linkcount:1"); 
	sprintf(file_data+strlen(file_data), "%s%d%s%d%s%d", ",atime:", (int)current_time, ",ctime:", (int)current_time, ",mtime:", (int)current_time);
	sprintf(file_data+strlen(file_data), "%s%d%s", ",indirect:0,location:", free_block, "}"); 
	file_data[strlen(file_data)]='0'; // get rid of the '\0'
	log_msg("fill_reg_inode(): exit\r\nfill_reg_inode(): file_data a/f");
	log_msg(file_data);
}

int insert_parent_entry(const char* path, int block_num){
	/*
	insert_parent_entry() inserts an entry into the parent directory of the entry specified by "path".
	* The block number of the entry to insert must be passed. The name for the entry is extracted from
	* "path", as is the parent directory.
	* RETURN: 0 if success. Does not check for many failure conditions internally.
		* Assumes critical ones checked for before calling this function
	*/
	log_msg("insert_parent_entry(): enter\r\ninsert_parent_entry(): path=");
	log_msg(path);

	// load the parent dir's inode
	int parent_pos=strlen(path)-1; // so starts at last char
	while(path[parent_pos]!='/'){ parent_pos--; }
	char* parent_path=malloc(strlen(path)); // upper limit
	strncpy(parent_path, path, parent_pos+1); // so / is pos 0, but 1 byte gets copied
	parent_path[parent_pos+1]='\0';
	struct fuse_file_info* fi=malloc(sizeof(struct fuse_file_info));
	opendir_rr(parent_path, fi); // assumes this doesn't fail, checking done before calling function
	free(parent_path);
	int new_dir_pos=parent_pos+1;

	// extract the parent dir's block #
	char* parent_data=(char*)fi->fh;
	log_msg("insert_parent_entry(): parent_data=");
	log_msg(parent_data);
	int parent_block_pos=0;
	while(parent_data[parent_block_pos]!=':' || parent_data[parent_block_pos+1]!='.' || parent_data[parent_block_pos+2]!=':'){
		parent_block_pos++;
	} 
	parent_block_pos+=3; // move past :.:
	char* parent_block_num=malloc(MAX_DIGITS_BLOCK_NUM); // 100 max num digits for max block #
	parent_block_num[0]='\0'; // strlen=0
	while(parent_data[parent_block_pos]!=','){
		sprintf(parent_block_num+strlen(parent_block_num), "%c", parent_data[parent_block_pos]);
		parent_block_pos++;
	}
	
	// write entry into parent dir data
	parent_block_pos=0;
	while(parent_data[parent_block_pos]!='}' || parent_data[parent_block_pos+1]!='}'){ parent_block_pos++; }
	char* new_entry=malloc(MAX_DIGITS_BLOCK_NUM);
	new_entry[0]='\0';
	sprintf(new_entry, "%s", ",f:");
	log_msg("insert_parent_entry(): new entry=");
	log_msg(new_entry);
	snprintf(new_entry+strlen(new_entry), strlen(path)-new_dir_pos+1, "%s", path+new_dir_pos);
	sprintf(new_entry+strlen(new_entry), "%s%d%s", ":", block_num, "}}");
	sprintf(parent_data+parent_block_pos, "%s", new_entry);
	parent_data[strlen(parent_data)]='0'; // so not '\0' and truncated
	log_msg("insert_parent_entry(): after mod parent_data=");
	log_msg(parent_data);
	free(new_entry);
	
	// form file name of parent dir, open, and write entry
	FILE* fh_parent=open_block_file(atoi(parent_block_num));
	fseek(fh_parent, 0, SEEK_SET);
	fwrite(parent_data, 1, FILE_SIZE, fh_parent);
	fclose(fh_parent);
	free((char*)fi->fh);
	return 0;
}

void destroy_rr(void* private_data){
	// destroy_rr() called on exit of file_system, frees private_data memory.
	log_msg("destroy_rr(): enter");
	if(private_data!=NULL){ free(private_data); }
	log_msg("destroy_rr(): exit");
}

int statfs_rr(const char *path, struct statvfs *statv) {
	// statfs_rr() provides information to any statfs() calls on this file system.
	statv->f_bsize=FILE_SIZE;
	statv->f_bfree=total_free_blocks();
	statv->f_bavail=statv->f_bfree;
	statv->f_fsid=20; 
	return 0; // success
}

int total_free_blocks(){
	// total_free_blocks() implemented for the purpose of providing information for statfs_rr().
	// Returns the total # of free blocks.
	log_msg("total_free_blocks(): enter");
	char* blocks_dir=malloc(bdir_path_size());
	blocks_dir[0]='\0'; 
	sprintf(blocks_dir+strlen(blocks_dir), "%s%s",(char*) fuse_get_context()->private_data, "/blocks/fusedata.");
	int path_pos=strlen(blocks_dir); // since only change digits at the end
	
	// create each free block file, fill it with the appropriate free block #s
	char* curr_block=malloc(MAX_DIGITS_BLOCK_NUM); // no more than 100 digits for block #
	curr_block[0]='\0';
	int free_blocks_total=0;
	int j;
	for(j=1; j<FREE_BLK_FILES+1; j++){ 
		sprintf(blocks_dir+path_pos, "%d", j);	
		FILE* fh=open_block_file(j);		
		char* file_data=malloc(FILE_SIZE);
		file_data[0]='\0';
		fread(file_data, 1, FILE_SIZE, fh);
		int file_pos=0;
		while(file_pos==0 || file_data[file_pos-1]!=',' || file_data[file_pos]!='0'){
			while(file_data[file_pos]!=','){ file_pos++; }
			free_blocks_total++;
			file_pos++;
		}
		fclose(fh);		
		free(file_data);
	}
	free(blocks_dir);
	free(curr_block);
	log_msg("total_free_blocks(): exit");
	return free_blocks_total;
}

void get_heap_parent_child(const char* path, char* parent_path, char* dir_name){
	/*
	 get_heap_parent_child() takes a c_str to the path and separates the last /entry on the full path
	 * to store it as dir_name, and stores the c_str preceding that entry in the full path as parent_path.
	 * Both parent_path and dir_name are expected to be pointing to dynamically allocated memory.	  
	 * RETURN: none
	 */
	
	// extract dir name from file path
	int parent_pos=strlen(path)-1; // so starts at last char
	while(path[parent_pos]!='/'){  parent_pos--; }
	int file_name_pos=parent_pos+1; // to pull out file name
	
	// copy over parent dir path,  dir name
	parent_path[0]='\0';
	strncpy(parent_path, path, parent_pos+1); // so / is pos 0, but 1 byte gets copied
	parent_path[parent_pos+1]='\0'; // so acts like sprintf
	dir_name[0]='\0';
	sprintf(dir_name, "%s", path+file_name_pos);
	log_msg("get_heap_parent_child(): dir_name=");
	log_msg(dir_name);
	log_msg("get_heap_parent_child(): parent_path=");	
	log_msg(parent_path);	
}

int get_link_count(char* file_data){
	/*
    get_link_count() takes in a char* to some file's inode data and returns the extracted link count as an int.
	*/
	log_msg("get_link_count(): enter\r\nmod_link_count(): file_data=");
	log_msg(file_data);
	int file_pos=0;
	// find link count field
	while(file_data[file_pos]!=',' || file_data[file_pos+1]!='l' ||  file_data[file_pos+2]!='i'){
		file_pos++;
	} // get to link count, then get to colon
	while(file_data[file_pos-1]!=':'){ file_pos++; }

	// copy over link count
	char* count=malloc(MAX_DIGITS_BLOCK_NUM); // max 100 digits for num links
	count[0]='\0';
	while(file_data[file_pos]!=','){
		sprintf(count+strlen(count), "%c", file_data[file_pos]);
		file_pos++;
	}
	log_msg("count:");
	log_msg(count);
	int answer=atoi(count);
	free(count);
	return answer;
}

int mod_link_count(char* file_data, int change){
	/*
	mod_link_count() takes in a char* to some file's inode data, an int indicating by how much to change
	* the linkcount, and then appropriately updates the inode file's linkcount.
	* RETURN: 0 if success. The passed in file_data is modified, but not written to any files.
	*/	
	log_msg("mod_link_count(): enter\r\nmod_link_count(): file_data=");
	log_msg(file_data);
	int file_pos=0;
	
	// find link count field
	while(file_data[file_pos]!=',' || file_data[file_pos+1]!='l' ||  file_data[file_pos+2]!='i'){
		file_pos++;
	} // get to colon
	while(file_data[file_pos-1]!=':'){ file_pos++; }
	int colon_pos=file_pos-1;

	// copy over link count
	char* count=malloc(MAX_DIGITS_BLOCK_NUM);
	count[0]='\0';
	while(file_data[file_pos]!=','){
		sprintf(count+strlen(count), "%c", file_data[file_pos]);
		file_pos++;
	}
	log_msg("count:");
	log_msg(count);
	int new_count=atoi(count)+change;
	count[0]='\0';
	sprintf(count+strlen(count), "%d", new_count);
	
	// copy after , handle byte appending
	char* after_old_entry=malloc(FILE_SIZE); // upper limit
	strncpy(after_old_entry, file_data+file_pos, FILE_SIZE-file_pos);
	file_data[file_pos]='\0'; // string ends before the old name starts
	sprintf(file_data+colon_pos+1, "%d", new_count);
	
	int new_len=strlen(file_data)-colon_pos;
	int old_len=file_pos-colon_pos-1;
	int len_diff=new_len-old_len;
	if(len_diff>=0){ 
		strncpy(file_data+colon_pos+new_len, after_old_entry, FILE_SIZE-file_pos-len_diff);
		log_msg("mod_link_count()(): len_diff>0");
	}
	else { // new name is shorter
		log_msg("mod_link_count()(): len_diff<0");
		strncpy(file_data+colon_pos+new_len, after_old_entry, FILE_SIZE-file_pos); // copy over all of that data
		int i;
		for(i=0; i<abs(len_diff); i++){ // add zeros
			file_data[FILE_SIZE-abs(len_diff)+i-1]='0'; // overwrite the '\0'
		}
	}
	log_msg("mod_link_count()(): file_data a/f fix");
	log_msg(file_data);
	free(after_old_entry);
	free(count);
	return 0;
}

int link_rr(const char *path, const char *newpath){
	/*
	link_rr() creates a link from the inode pointed at by "path" at location "newpath".
	* NOTE: linking directories simply does not work
	* RETURN: 0 if success, error code otherwise. 
	*/
	log_msg("link_rr(): enter\r\nlink_rr(): newpath=");
	log_msg(newpath);
	
	// get parent dir name from path, and original link's file name
	char* parent_path=malloc(strlen(path)); // upper limit
	char* orig_name=malloc(strlen(path));
	get_heap_parent_child(path, parent_path, orig_name);
	
	// load parent dir inode for parent_path
	log_msg("link_rr(): parent_path");
	log_msg(parent_path);
	struct fuse_file_info* fi=malloc(sizeof(struct fuse_file_info));
	int success=opendir_rr(parent_path, fi);
	if(success<0){ return success; }
	
	// get path's block #, and insert a new entry into the new path's parent directory
	char* parent_data=(char*)fi->fh;
	char* file_block_num=malloc(MAX_DIGITS_BLOCK_NUM); // 100 digit limit for block #
	success=find_block_num(parent_data, 0, orig_name, file_block_num);
	if(success<0){ return success; }
	success=insert_parent_entry(newpath, atoi(file_block_num)); 
	if(success<0){ return success; }
	
	FILE* orig_fh=open_block_file(atoi(file_block_num));
	char* orig_data=malloc(FILE_SIZE);
	fread(orig_data, 1, FILE_SIZE, orig_fh);
	
	success=mod_link_count(orig_data,1);
	if(success<0){ return success; }
	fseek(orig_fh, 0, SEEK_SET);
	fwrite(orig_data, 1, FILE_SIZE, orig_fh);
	fclose(orig_fh);
	
	free(parent_path);
	free(orig_name);
	free(file_block_num);
	if(fi->fh!=(u_int64_t)NULL) { free((void*)fi->fh); }
	if(fi!=NULL) { free(fi); }
 	log_msg("link_rr(): exit");
	return 0; // success
}

int write_rr(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
	/*
	write_rr() writes the data in buf to a file specified by path and pointed to by fi->fh.
	* NOTE: files larger than 1 block are not supported yet. Thus a MAX FILE SIZE limit is also not implemented.
	* RETURN: the number of bytes successfully written, else error code.
	*/
	
	log_msg("write_rr(): enter\r\nwrite_rr(): path=");
	log_msg(path);
    log_msg("write_rr(): buf=");
    log_msg(buf);
	char* debug=malloc(1000);
	sprintf(debug, "%d \t %d", (int)size, (int)offset);
	log_msg(debug);
	free(debug);
	log_msg("write_rr(): fi->fh=");
	log_msg((char*)fi->fh);
	
			// this should not open the file inode into fi->fh but should load the data itself
	if(fi->fh==(u_int64_t)NULL){
		log_msg("write_rr(): fi->fh==NULL");
		open_rr(path, fi);
	}
	// load file inode so know where data/indirect blocks are
	int inode_num=get_block_num(path, 'f');
	FILE* inode_fh=open_block_file(inode_num);
	char* inode_data=malloc(FILE_SIZE);
	fread(inode_data, FILE_SIZE, 1, inode_fh);
	set_size(inode_data, size+offset-1);	
	fseek(inode_fh, 0, SEEK_SET);
	fwrite(inode_data, FILE_SIZE, 1, inode_fh);
	int old_loc=get_location(inode_data);
	fclose(inode_fh);
	
	//~ // if size >4096 first time, handle setting up indirect
	int indirect=get_indirect(inode_data);
	if(size+offset > FILE_SIZE && indirect == 0){	
		remove_free_block(get_data_block_num(inode_data));
		// update indirect and location fields
		set_indirect(inode_data, 1);
		int ind_bnum=first_free_block();
		set_location(inode_data, ind_bnum);
		remove_free_block(ind_bnum);
		
		// write it back to the file
		FILE* inode_fh=open_block_file(inode_num);
		fseek(inode_fh, 0, SEEK_SET);
		fwrite(inode_data, FILE_SIZE, 1, inode_fh);
		fclose(inode_fh);
		
		// open new block for indirect list, fill it
		FILE* ind_fh=open_block_file(ind_bnum);
		char* block_list=malloc(FILE_SIZE);
		sprintf(block_list, "%d,", old_loc);
		log_msg("write_rr(): bl=");
		log_msg(block_list);
		
		char* debug=malloc(FILE_SIZE);
		fread(debug, FILE_SIZE, 1, ind_fh);
		log_msg(debug);
		free(debug);
		
		fseek(ind_fh, 0, SEEK_SET);
		fwrite(block_list, strlen(block_list), 1, ind_fh);
		fclose(ind_fh);
		
		free(block_list);
		free(inode_data);
		inode_data=NULL;
	
	}
	// if indirect =1 case later
	
	
	
	// have to iteratively segment the size+offset into FILE_SIZE segments and read the block list
	// to know where to store them
	
	// and since storing at some OFFSET, should not go through all blocks
	// if offset 5000, open SECOND block, go to 5000-4096 offset, and store there
	// make sure your files are actually 4096, now it matters
	
	// and then we are done
	char* file_data=(char*)fi->fh;
	snprintf(file_data+offset, size, "%s", buf);
	// open the data block location
	FILE* data_block=open_block_file(old_loc);
	fwrite(file_data, size+offset-1, 1, data_block);
	log_msg("write_rr(): file_data=");
	log_msg(file_data);
	fclose(data_block);
	
	// check for total blocks < max_fiel_size given in specs
	if(inode_data!=NULL) { free(inode_data); }
	
	log_msg("write_rr(): exit");
    return size;
}

int get_location(const char* inode_data){
	// get_location() returns the value of the location field in a regular file inode.
	log_msg("get_location(): enter , inode_data=");
	log_msg(inode_data);
	int file_pos=0;
	while(inode_data[file_pos]!=',' || inode_data[file_pos+1]!='l' || inode_data[file_pos+2]!='o' || inode_data[file_pos+3]!='c'){ file_pos++; }
	while(inode_data[file_pos-1]!=':'){ file_pos++; }
	char* bnum=malloc(MAX_DIGITS_BLOCK_NUM);
	bnum[0]='\0';
	while(inode_data[file_pos]!='}'){
		sprintf(bnum+strlen(bnum), "%c", inode_data[file_pos]);
		log_msg(bnum);
		file_pos++;
	}
	int answer=atoi(bnum);
	log_msg("get_location(): exit , bnum=");
	log_msg(bnum);
	free(bnum);
	return answer;
}

int set_location(char* inode_data, int new_bnum){
	// get_location() sets the value of the location field in a regular file inode.
	// RETURN: 0 if success.
	log_msg("set_location(): enter, data=");
	log_msg(inode_data);
	int file_pos=0;
	while(inode_data[file_pos]!=',' || inode_data[file_pos+1]!='l' || inode_data[file_pos+2]!='o' || inode_data[file_pos+3]!='c'){ file_pos++; }
	while(inode_data[file_pos-1]!=':'){ file_pos++; }
	char* after_old_entry=malloc(FILE_SIZE);
	strcpy(after_old_entry, inode_data+file_pos);
	sprintf(inode_data+file_pos, "%d}", new_bnum); // since this is the last field
	inode_data[strlen(inode_data)]='0'; // so not '\0' and truncated
	
	free(after_old_entry);
	log_msg("set_location(): exit, data=");
	log_msg(inode_data);
	return 0;
}

int set_indirect(char* file_data, int ind_val){
	/*
	set_indirect() takes in a regular file's inode data and sets the indirect field to the 1 digit parameter passed in.
	* RETURN: 0 if success, else error code
	*/
	log_msg("set_indirect(): enter\r\nset_indirect(): file_data=");
	log_msg(file_data);
	int file_pos=0;
	while(file_data[file_pos]!='i' || file_data[file_pos+1]!='n' || file_data[file_pos+2]!='d'){ file_pos++;}
	while(file_data[file_pos]!=':') { file_pos++; }
	file_pos++; // move past the : 
	
	char* indirect_status=malloc(5); // a 1 or a 0, extra space to be safe
	sprintf(indirect_status, "%d", ind_val);
	file_data[file_pos]=indirect_status[0];
	
	free(indirect_status);
	log_msg("set_indirect(): exit, file_data=");
	log_msg(file_data);
	return 0;
}

int set_size(char* inode_data, int size){
	/*
	set_size() takes in a regular file's inode data and sets the indirect field to the size parameter passed in.
	* RETURN: 0 if success, else error code
	*/
	log_msg("set_size(): enter\r\nset_size(): file_data=");
	log_msg(inode_data);
	int file_pos=0;
	while(inode_data[file_pos]!='s' || inode_data[file_pos+1]!='i' || inode_data[file_pos+2]!='z'){ file_pos++;}
	while(inode_data[file_pos]!=':') { file_pos++; }
	file_pos++; // move past the : 
	
	char* inode_size=malloc(20); // 7 digits for max file size, but to be safe
	sprintf(inode_size, "%d", size);

	char* after_old_entry=malloc(FILE_SIZE);
	int comma_pos=file_pos;
	while(inode_data[comma_pos]!=',') { comma_pos++; }
	strcpy(after_old_entry, inode_data+comma_pos);
	sprintf(inode_data+file_pos, "%d", size);
	snprintf(inode_data+strlen(inode_data), FILE_SIZE-strlen(inode_size), "%s", after_old_entry);
	int end_of_file=FILE_SIZE-1;
	while(inode_data[end_of_file]!='0'){ // append as many zeros as needed
		inode_data[end_of_file]='0';
		end_of_file--;
	}
	inode_data[strlen(inode_data)]='0'; // so not '\0' and truncated

	free(after_old_entry);
	free(inode_size);
	log_msg("set_size(): exit, inode_data=");
	log_msg(inode_data);
	return 0;
}

int truncate_rr(const char *path, off_t size){
	log_msg("truncate_rr(): enter, path=");
	log_msg(path);
	FILE* inode=open_block_file(get_block_num(path, 'f'));
	char* inode_data=malloc(FILE_SIZE);
	fread(inode_data, FILE_SIZE, 1, inode);
	set_size(inode_data, size);	
	fseek(inode, 0, SEEK_SET);
	fwrite(inode_data, FILE_SIZE, 1, inode);
	free(inode_data);
	fclose(inode);
	
	log_msg("truncate_rr(): exit");
	return 0;
}
			
static struct fuse_operations oper = {
	.init	    = init_rr,
	.opendir 	= opendir_rr,
	.getattr	= getattr_rr,
	.readdir	= readdir_rr,
	.open		= open_rr,
	.read		= read_rr,
	.release 	= release_rr,
	.releasedir = releasedir_rr,
	.mkdir 		= mkdir_rr,
	.rename		= rename_rr,
	.rmdir 		= rmdir_rr,
	.unlink 	= unlink_rr,
	.create 	= create_rr,
	.destroy 	= destroy_rr,
	.link 		= link_rr,
	.write 		= write_rr,
	.truncate 	= truncate_rr,
};

int main(int argc, char *argv[]){
	char* fullpath = malloc(FILE_SIZE); // arbitrary max limit on CWD
	//~ fullpath=getcwd(fullpath, FILE_SIZE); 
	fullpath="/fusedata";
	printf("\n\nObtained CWD:%s\n\n", fullpath);
	int results=fuse_main(argc, argv, &oper, fullpath);
	free(fullpath);
	return results;
}


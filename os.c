#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include <stdlib.h>
#include <malloc.h>

#include <unistd.h> // is this okay, just to get cwd, and access
#include <time.h> // for UNIX time

/*

fusermount -u ~/Desktop/test/m; gcc -Wall -g ~/Desktop/test/os.c `pkg-config fuse --cflags --libs` -o OS_hw; ./OS_hw ./m; ls -al m

*/
static int opendir_rr(const char* path, struct fuse_file_info * fi);

void log_msg(const char* msg){
	FILE* fh=fopen("/home/rahhbertt/Desktop/test/logs.txt", "a");
	if(msg!=NULL) { fwrite(msg, strlen(msg), 1, fh); }
	fwrite("\r\n", 2, 1, fh);
	fwrite("\r\n", 2, 1, fh);
	fwrite("\r\n", 2, 1, fh);
	fclose(fh);
}

int releasedir_rr(const char * path, struct fuse_file_info * fi){
	log_msg("releasedir(): enter");
	if(fi!=NULL && fi->fh!=NULL){
		log_msg("releasedir(): fi->fh");
		log_msg(fi->fh);
		free(fi->fh);
		fi->fh=NULL;
	}
	log_msg("releasedir(): exit");
	return 0;
}

int release_rr(const char * path, struct fuse_file_info * fi){
	log_msg("release(): enter");
	if(fi!=NULL && fi->fh!=NULL){
		log_msg("release(): fi->fh");
		log_msg(fi->fh);
		free(fi->fh);
		fi->fh=NULL;
	}
	log_msg("release(): exit");
	return 0;
}

static int getattr_rr(const char *path, struct stat *stbuf) {
	int res = 0;
log_msg("getattr_rr(): enter\r\ngetattr_rr(): path=");
log_msg(path);
	memset(stbuf, 0, sizeof(struct stat)); // so if field left empty, its '0' not garbage

	// no dir passed in, no chance to do otherwise
	// now for every getattr_rr have to recursively find the dir, then the block #
	// then load the file into memory
	// then load up the stbuf
	
	// open directory/file, load it into memory; FUSE does not pass anything to get to this
	struct fuse_file_info * fi_2=malloc(11*sizeof(uint64_t)); // about right
	opendir_rr(path, fi_2);
	if(fi_2->fh==NULL){
		log_msg("getattr_rr(): fi_2->fh==NULL");
		return 0; 
	}
	log_msg("getattr_rr(): fi_2->fh!=NULL, is");
	log_msg(fi_2->fh);
	char *block_data=fi_2->fh;
	// load up attributes
	// once get to fifth field, if first letter = l its a file, if it =a, its a dir
	
	// load up attributes
	// once get to fifth field, if first letter = l its a file, if it =a, its a dir
	int field_count=1; // first field
	int data_pos=0; // skip the {size:
	int field_start_ch=0;
	char* current_field=malloc(1000); //
	current_field[0]='\0'; // strlen =0;
	while(1){
		data_pos++; // so skip { at start, and so field starts at byte after the ','
		field_start_ch=data_pos;
		while(block_data[data_pos]!=':'&& data_pos<4096){ data_pos++; }
		data_pos++; // move past ':'
		while(block_data[data_pos]!=',' && data_pos<4096){
			sprintf(current_field+strlen(current_field), "%c", block_data[data_pos]);
			data_pos++;
		}
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
		//~ log_msg("getattr_rr(): current field");
		//~ log_msg(current_field);
		//~ log_msg("getattr_rr(): field_start_ch");
		//~ char * one=malloc(4);
		//~ sprintf(one, "%c", block_data[field_start_ch]);
		//~ log_msg(one);
		//~ free(one);
		
		current_field[0]='\0'; // empty the string
	}
	free(current_field);
	free(fi_2);
// this puts the file attributes you wan to show into the stat buffer
// so ls can output it to the screen from there
// this does not MAKE the file
log_msg("getattr_rr(): exit");
	return res;
}

static int readdir_rr(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
	log_msg("readdir_rr(): path=");
	log_msg(path);
	
	// the loaded directory inode
	char* block_data=fi->fh;
	if(fi->fh==NULL) { 
		log_msg("\readdir_rr(): fi->fh==NULL");
		return 0; 
	} // call open dir in here to get a pointer to data, and then call closedir/free the data?

	// set up memory for current_entry
	
	char* current_entry=malloc(1000); // FIX HARD CODING
	current_entry[0]='\0';
	//strcpy(current_entry, path);	
	if(current_entry[strlen(current_entry)-1]!='/'){ sprintf(current_entry+strlen(current_entry), "%c", '/'); } // append a / if none there
	int path_pos=strlen(current_entry);
	// loop until find dictionary
	int dict_pos=1;
	while(1){
		dict_pos++;
		if(block_data[dict_pos]=='{'){ break;}
	}
	dict_pos+=3; // 1 to "d", 1 to ":", 1 to file name start
	
	// loop through reading each entry into fuse's "buf", return when done
	while(1){
		while(block_data[dict_pos]!=':' && dict_pos < 4096){ // to be safe, FIX HARD CODING 
			sprintf(current_entry+strlen(current_entry), "%c", block_data[dict_pos]);
			dict_pos++; 
		}
		filler(buf, current_entry +1, NULL, 0); // was only giving me 1 char since overwrote [0] to \0, got rid of the "/"
		log_msg("readdir_rr(): current_entry=");
		log_msg(current_entry);
		
		current_entry[path_pos]='\0'; // str is now "empty"
	
		// skip until next entry begins or end of dict
		while( (block_data[dict_pos]!=',' && block_data[dict_pos]!='}') && dict_pos < 4096 ){ dict_pos++;}
		if(block_data[dict_pos]==','){ dict_pos+=3; } // advance past those 3 chars ',' 'd' ':'
		else if(block_data[dict_pos]=='}'){ 
			free(current_entry); // one point of exit
			log_msg("readdir_rr(): exit -> end of dict");
			return 0; // you've read the dictionary
		}
		if(dict_pos>=4096){ 
			free(current_entry);
			log_msg("readdir_rr(): exit -> exceeded 4096");
			return 0; 
		} // serious error
	}
}

static int open_rr(const char *path, struct fuse_file_info *fi) {
// for every file here needs to check strcmp is okay
	log_msg("open_rr(): enter\r\nopen(): path=");
log_msg(path);
// clean up the fi->fh on release

	//~ if (strcmp(path, path) != 0 && strcmp(path, "/testing") !=0 && strcmp(path, "/fusedata.0")  !=0)
		//~ return -ENOENT;
//~ 
	//~ if ((fi->flags & 3) != O_RDONLY)
		//~ return -EACCES;
		//~ 
		
		// how and when to handle indirect?
	opendir_rr(path, fi); // this just gets you the inode
	log_msg("open_rr(): fi="); // maybe READ and write should handle indirects?
	if(fi->fh != NULL){ log_msg(fi->fh); }
	else { 
		log_msg("open_rr(): fi->fh==NULL"); 
		return 0;
	}	
	char* block_data=fi->fh;
	int loc_pos=1;
	
	
	
	//~ char* file_size=malloc(100); // FIX HARD CODING
	//~ file_size[0]='\0'; // strlen =0
	//~ while(block_data[loc_pos-1]!=':') { loc_pos++; } // get file size
	//~ while(block_data[loc_pos]!=',') { 
		//~ sprintf(file_size+strlen(file_size), "%c", block_data[loc_pos]);
		//~ loc_pos++;
		//~ log_msg("open(): file_size building");
		//~ log_msg(file_size);
	//~ }
	//~ // check if fi = null?
	//~ fi->fh_old=file_size;
	//~ log_msg("open(): fi->fh_old");
	//~ //file_size[0]='\0'; // fh_old has a literal pointer to this
	//~ char* temp=malloc(10);
	//~ sprintf(temp, "%s", fi->fh_old);
	//~ log_msg(temp);
	//~ free(file_size); // free it at close
	//~ free(temp);
	//~ 
	
	int indirect=-1;
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
	while(block_data[loc_pos]!='l' || block_data[loc_pos+1] !='o' || block_data[loc_pos+2] !='c'){ loc_pos++; }
	char * log=malloc(10);
	sprintf(log, "%d", loc_pos);
	//~ log_msg("open(): loc_pos=");
	//~ log_msg(log);
	//l, o, c, a, t, i, o, n, :
	while(block_data[loc_pos-1]!=':'){ loc_pos++; } // get to first char after :
	char* location=malloc(100); // FIX HARD CODING
	location[0]='\0'; // strlen =0
	while(block_data[loc_pos]!='}'){ 
		sprintf(location+strlen(location), "%c", block_data[loc_pos]); 
		loc_pos++;
	} // sprintf adds the null terminator ?
	char * abs_loc=malloc(1000); // FIX HARD CODING
	abs_loc[0]='\0'; // to be safe
	sprintf(abs_loc, "%s", (char *)fuse_get_context()->private_data);
	sprintf(abs_loc+strlen(abs_loc), "%s", "/blocks/fusedata.");
	int path_name_fd=strlen(abs_loc);
	sprintf(abs_loc+strlen(abs_loc), "%s", location);
	
	
	if(!indirect){
		char* file_data=malloc(4096); // multiples here
		int result=access(abs_loc, F_OK);
		log_msg("open_rr(): direct location=");
		log_msg(abs_loc);
		
		
		if(result==0){
			log_msg("open_rr() direct file opened");
			FILE* fh=fopen(abs_loc, "r");
			fread(file_data, 4096, 1, fh); // FIX HARD CODING
			fclose(fh);
		}
		log_msg("open_rr(): direct file_data=");
		log_msg(file_data);
		
		if(fi!=NULL){
			fi->fh=file_data; 
		}
	}
	else {
		
		// if INDIRECT, need to know # of blocks
		int num_blocks=0;
		
		// get the indirect block's data
		char* file_data=malloc(4096); // multiples here
		int result=access(abs_loc, F_OK);
		log_msg("open_rr(): ind_location=");
		log_msg(abs_loc);
		if(result==0){
			log_msg("open_rr() ind file opened");
			FILE* fh=fopen(abs_loc, "r");
			fread(file_data, 4096, 1, fh); // FIX HARD CODING
			fclose(fh);
		} // else.. what?
		log_msg("open_rr(): indirect block data=");
		log_msg(file_data);

		// count the # of blocks
		int ind_pos=0;
		while(file_data[ind_pos]!=',' || file_data[ind_pos+1]!='0'){
			ind_pos++; // move past the comma, if first run, skip a byte, at least 1 byte before ,
			while(file_data[ind_pos]!=','){ ind_pos++; }
			num_blocks++;
		}
		
		// read a block #, add its data to the buffer, repeat
		char* current_block_num=malloc(20); // FIX HARD CODING
		current_block_num[0]='\0';
		char* ultra_file=malloc(num_blocks*4096); // FIX HARD CODING
		ultra_file[0]='\0';
		int len_ufile=0;
		ind_pos=0; // now read in block #s
		while(ind_pos==0 || file_data[ind_pos-1]!=',' || file_data[ind_pos]!='0'){ // 400 ptrs guarnatees this
			// when start, just go ahead
			// after a loop, the ind_pos is one AFTER the comma
			// when ind_pos==0, dont want to access ind_pos-1 and cause havoc
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
				fread(ultra_file+len_ufile, 4096, 1, fh); // FIX HARD CODING
				fclose(fh);
				len_ufile+=4096;
			} // undefined if error? or just log_msg if error?
			ind_pos++;
		}
		
		free(file_data);
		free(current_block_num);
		log_msg("open_rr(): ultra_file");
		log_msg(ultra_file);
		if(fi!=NULL){
			fi->fh=ultra_file; 
		}
		
		//free(ultra_file);
	}
	
	
	//~ free(file_data);
	//~ free(file_size);
	free(abs_loc);
	free(location);
	log_msg("open_rr(): exit");
	return 0;
}

//release(const char* path, struct fuse_file_info *fi)

static int read_rr(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
 log_msg("readdir_rr(): enter\r\readdir_rr(): path=");
log_msg(path);
 	
 	//~ const char* strz="ht there";
	//~ size_t len;
	//~ (void) fi;
	//~ // for every file here needs to check strcmp is okay
	//~ if(strcmp(path, path) != 0 && strcmp(path, "/testing") !=0 && strcmp(path, "/fusedata.0")  !=0)
		//~ return -ENOENT;
//~ 
	//~ if(strcmp(path, "/testing")==0){
		//~ len = strlen(strz);
	//~ } else {
		//~ len = strlen(str);
	//~ }
	//~ if (offset < len) {
	
	// size is uninitialzied, it wants you to read the whole file
	
	
	log_msg("read_rr(): size to read");
	char* flags=malloc(100); // fix hard coding
	sprintf(flags, "%d", size);
	log_msg(flags);
	
	sprintf(flags, "%d", offset);
	log_msg("read_rr(): offset to read");
	log_msg(flags);
	free(flags);
	
	size=4096*2; // originally 4096 for whatever reason
	log_msg("read_rr(): fi->fh");
	if(fi!=NULL && fi->fh!=NULL) { log_msg(fi->fh); }
	//~ if(size+offset>4096) { // fix hard coding for indirect
		//~ size=4096-offset; 
	//~ }
	memcpy(buf, fi->fh + offset, size);
	log_msg("read_rr(): buf=");
	log_msg(buf);
	log_msg("readdir_rr(): exit");
	return size;
}

	
void create_super_block(); 
void free_block_list();
void create_root_dir();
const int FILE_SIZE=4096;
const int MAX_FILES=10000;
// Undocumented but extraordinarily useful fact:  the fuse_context is
// set up before this function is called, and
// fuse_get_context()->private_data returns the user_data passed to
// fuse_main().  Really seems like either it should be a third
// parameter coming in here, or else the fact should be documented
// (and this might as well return void, as it did in older versions of
// FUSE).
static void* init_rr(struct fuse_conn_info *conn) {
log_msg("init(): enter");
	// create a buffer of 4096 "0" characters
	int letter='0';
 	char* buf;
 	buf=(char *)malloc(FILE_SIZE); 
 	memset(buf, letter, FILE_SIZE);
	
	// create a buffer for the full name
	// in theory, strlen(priv_data)+strlen(num_digits)=1, but first 1 is unknown
	char* blocks_dir=malloc(256*4*4); // arbitrary max size
	//strcpy(blocks_dir, fullpathz);
	strcpy(blocks_dir, fuse_get_context()->private_data);
	strcat(blocks_dir, "/blocks");
	
	// make the directory for the blocks if it doesn't exist
	int result_bdir=access(blocks_dir, F_OK);
	if( result_bdir <0 && errno==ENOENT){ 
		mkdir(blocks_dir, S_IRWXU | S_IRWXG | S_IROTH | S_IWOTH | S_IXOTH); 
	}
	
	// only changing the # after this position in the string
	strcat(blocks_dir, "/fusedata.");
	int path_pos=strlen(blocks_dir);
	
	// create all the block files, if they don't already exist
	int i;
	for(i=0; i<MAX_FILES/100; i++){
		sprintf(blocks_dir+path_pos, "%d", i);		
		int result=access(blocks_dir, F_OK);
		if( result <0 && errno==ENOENT){ 				
			FILE* fd=fopen(blocks_dir,"w+");
			fwrite(buf, FILE_SIZE, 1, fd);
			fclose(fd);			
		}
		//else {} // figure out a return message or errstr somehow later
	}
	if( result_bdir<0 && errno==ENOENT){ // if the blocks dir hadn't already existed	
		create_super_block(); // make the super block and free block list
		free_block_list();
		create_root_dir(); // should be mdkir("/") in the future, and a test case for if dir=="/" do this code
	}
	free(buf);
	free(blocks_dir);
	blocks_dir=NULL;
log_msg("init(): exit");
	return fuse_get_context()->private_data; // WHAT INIT RETURNS BECOMES PRIVATE_DATA FOR GOOD
}
	//~ FILE* log=fopen("/home/rahhbertt/Desktop/test/log.txt", "w+");
	//~ fclose(log);

void create_super_block(){
	time_t current_time;
	time(&current_time);
	
	char* blocks_dir=malloc(1000);
	//strcpy(blocks_dir, fullpathz);
	strcpy(blocks_dir, fuse_get_context()->private_data);
	strcat(blocks_dir, "/blocks");
	strcat(blocks_dir, "/fusedata.0");
	
	int result=access(blocks_dir, F_OK);
	if( result==0 ){ // opened okay 				
		FILE* fd=fopen(blocks_dir,"r+");
		
		// is it okay to strcpy for something so small when have to update a value?
		char* fields=malloc(1000);
		sprintf(fields+strlen(fields), "%s", "{creationTime:");
		sprintf(fields+strlen(fields), "%d", (int)current_time );	
		sprintf(fields+strlen(fields), "%s", ",mounted:");
		sprintf(fields+strlen(fields), "%d", 1); // FIX HARD CODING
		sprintf(fields+strlen(fields), "%s", ",devId:20,freeStart:");
		sprintf(fields+strlen(fields), "%d", 1); // FIX HARD CODING
		sprintf(fields+strlen(fields), "%s", ",freeEnd:");
		sprintf(fields+strlen(fields), "%d", 25); // FIX HARD CODING
		sprintf(fields+strlen(fields), "%s", ",root:");
		sprintf(fields+strlen(fields), "%d", 26); // FIX HARD CODING
		sprintf(fields+strlen(fields), "%s", ",maxBlocks:");
		sprintf(fields+strlen(fields), "%d", MAX_FILES);
		sprintf(fields+strlen(fields), "%c", '}');
		fwrite(fields, strlen(fields), 1, fd);
		fclose(fd);			
		free(fields);
	}
	free(blocks_dir);
}

void free_block_list(){
	char* blocks_dir=malloc(1000);
	//strcpy(blocks_dir, fullpathz);
	strcpy(blocks_dir, fuse_get_context()->private_data);
	strcat(blocks_dir, "/blocks");
	strcat(blocks_dir, "/fusedata.");
	int path_pos=strlen(blocks_dir); // since only change digits at the end
	int file_number=1;
	char* curr_block=malloc(100); // no more than 100 digits

	int j;
	for(j=1; j<26; j++){ // FIX THIS HARDCODING
		sprintf(blocks_dir+path_pos, "%d", file_number);	
		int result=access(blocks_dir, F_OK);
		if( result==0 ){ // opened okay 				
			FILE* fd=fopen(blocks_dir,"r+");
			int i;
			for(i=0; i<400; i++){ // FIX THIS HARD CODING
				curr_block[0]='\0'; // "empty" the string					
				if((j-1)*(400)+i>26){ // FIX THIS HARD CODING
					sprintf(curr_block+strlen(curr_block), "%d", (j-1)*400+i);
					sprintf(curr_block+strlen(curr_block), "%c", ','); // yes will end with a trailing comma
					fwrite(curr_block, strlen(curr_block), 1, fd); // fd is a resource identifier, not a pointer
				}
			} // should have an ELSE and error message or exit code or something, or log file write
			// creat a log(char*) function to write log messages in case of error
			fclose(fd);
			file_number++;
		}
	}
	free(blocks_dir);
	free(curr_block);
}

void create_root_dir(){
	time_t current_time;
	time(&current_time);
	
	char* blocks_dir=malloc(1000);
	//strcpy(blocks_dir, fullpath);
	strcpy(blocks_dir, fuse_get_context()->private_data);
	strcat(blocks_dir, "/blocks");
	strcat(blocks_dir, "/fusedata.");
	sprintf(blocks_dir+strlen(blocks_dir), "%d", 26); // FIX THIS HARDCODING
	
	int resultz=access(blocks_dir, F_OK);
	if( resultz==0 ){ // opened okay 				
		FILE* fd=fopen(blocks_dir,"r+");
		
		// is it okay to strcpy for something so small when have to update a value?
		char* fields=malloc(1000);
		sprintf(fields+strlen(fields), "%s", "{size:"); // have to update this with code
		sprintf(fields+strlen(fields), "%d", 0 ); // FIX HARD CODING
		sprintf(fields+strlen(fields), "%s", ",uid:");
		sprintf(fields+strlen(fields), "%d", 1); // this is okay hard coding
		sprintf(fields+strlen(fields), "%s", ",gid:");
		sprintf(fields+strlen(fields), "%d", 1); // this is okay hard coding
		sprintf(fields+strlen(fields), "%s", ",mode:");
		sprintf(fields+strlen(fields), "%d", 16877); // this is okay hard coding
		sprintf(fields+strlen(fields), "%s", ",atime:");
		sprintf(fields+strlen(fields), "%d", (int)current_time); // FIX HARD CODING
		sprintf(fields+strlen(fields), "%s", ",ctime:");
		sprintf(fields+strlen(fields), "%d", (int)current_time); // FIX HARD CODING
		sprintf(fields+strlen(fields), "%s", ",mtime:");
		sprintf(fields+strlen(fields), "%d", (int)current_time);
		sprintf(fields+strlen(fields), "%s", ",linkcount:");
		sprintf(fields+strlen(fields), "%d", 2); // this is okay, you link to yourself twice
		sprintf(fields+strlen(fields), "%s", ",file_name_to_inode_dict:{d:.:");
		sprintf(fields+strlen(fields), "%d", 26); // FIX HARD CORDING
		sprintf(fields+strlen(fields), "%s", ",d:..:");
		sprintf(fields+strlen(fields), "%d", 26); // FIX HARD CORDING
		sprintf(fields+strlen(fields), "%s", "}}");
		fwrite(fields, strlen(fields), 1, fd);
		fclose(fd);			
		free(fields);
	}
	free(blocks_dir);
	//{size:0, uid:1, gid:1, mode:16877, atime:1323630836, ctime:1323630836, mtime:1323630836, linkcount:2, filename_to_inode_dict: {d:.:26,d:..:26}}
	
}

// this is essentially OPEN, so opendir_rr should just call open(path, fi)
// if limiting your amt of malloc, means you forgot to return an int
static int opendir_rr(const char* path, struct fuse_file_info * fi){	//initialize all needed variables and paths
log_msg("opendir_rr(): enter\r\nopendir_rr(): path=");
log_msg(path);
	int full_path_pos=0;
	char* next_dir=malloc(1000); // random number
	char* next_block_num=malloc(100); // FIX HARD CODING
	char* blocks_dir=malloc(1000);	
	char* block_data=malloc(4096); // FIX HARD CODING
	int hard_coded=26;
	next_block_num[0]='\0'; // so strlen is 0, otherwise strlen might just read 4096 until '\0' and your suffix is that long full of garbage
	sprintf(next_block_num+strlen(next_block_num), "%d", hard_coded); // FIX HARD CODED INITIALIZATION
//	sprintf(blocks_dir+strlen(blocks_dir), "%s", fullpath);
log_msg("opendir_rr(): Original next block #");
log_msg(next_block_num);
	sprintf(blocks_dir, "%s", (char *)fuse_get_context()->private_data); // always prints to position 0
	//~ sprintf(blocks_dir, "%s", "/home/rahhbertt/Desktop/test"); // always prints to position 0
	
	sprintf(blocks_dir+strlen(blocks_dir), "%s", "/blocks/fusedata.");	
	int path_pos=strlen(blocks_dir);
	while(1){
		next_dir[0]='\0'; // reset next_dir, so can regenerate it
		// also sets strlent o 0 right off the bat
		
		// fix this strlen stuff, or add these '\0' statements where eneded
		// for example, next_block_num does not need to be init with strlen(next_block-num) for hard coded
		// just use next_block_num, strlen should be 0 on create
		
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
		//~ // enter the current dir you've found
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
			return errno; 
		} // you're screwed, do an exit or something
		log_msg("opendir_rr(): successful access of");
		log_msg(blocks_dir);
		
		// read directory file block into memory
		FILE* fd=fopen(blocks_dir, "r+");

		fread(block_data, 4096, 1, fd); // FIX HARD CODING
		fclose(fd); // reads to block_data[0] as start
		if(next_dir[0]=='\0') { // have resolved the entire full path
				free(next_dir);
				free(next_block_num);
				free(blocks_dir);
				if(fi!=NULL){ fi->fh=(uint64_t)block_data; } // EDITED THIS
				log_msg("opendir_rr(): exit with data=");
				log_msg(block_data); // will read to \0 but does not mean exactly 4096 bytes, do not worry
				return 0; // success
//				return block_data; // a pointer to the dir block loaded into memory
		} 
		// skip to the dictionary part
		int dict_pos=1;
		while(1){
			dict_pos++;
			if(block_data[dict_pos]=='{'){ break;}
		}
		char* temp_cmp=malloc(1000); // truncate dir entries at some point
		// search for all 'd' entries, check if their names match the next dir you're searching for
		while(1){
			dict_pos++;
			if(block_data[dict_pos]=='d'||block_data[dict_pos]=='f'){ // so can getattr_rr better // +1 is the :
				int cmp_pos=0;
				while(block_data[dict_pos+2]==next_dir[cmp_pos]){
					dict_pos++;
					cmp_pos++;
				} // if full success, now on the : bit
				dict_pos+=3; // to be caught up on the +2, and then to skip the :
				if(cmp_pos==strlen(next_dir)){ // you have a match			
					log_msg("opendir_rr(): before next block num");
					log_msg(next_block_num);
					while(block_data[dict_pos]!=',' && block_data[dict_pos]!='}'){ // could be last item in dict
						sprintf(next_block_num+strlen(next_block_num), "%c", block_data[dict_pos]);
						dict_pos++; // get the next block num to search in
					}
					log_msg("opendir_rr(): after next block num");
					log_msg(next_block_num);
					free(temp_cmp);
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
				if(fi!=NULL) { fi->fh=NULL; } 
				return ENOENT;// ERROR, read the whole dict and directory not found, maybe return -1
			}
		}
	} // overall loop ends
	return 0;
}
static struct fuse_operations oper = {
	.init	    = init_rr,
	.getattr	= getattr_rr,
	.readdir	= readdir_rr,
	.open		= open_rr,
	.read		= read_rr,
	.opendir 	= opendir_rr,
	.release 	= release_rr,
	.releasedir = releasedir_rr,
};


int main(int argc, char *argv[])
{
	char* fullpath = malloc(1000);
	fullpath=getcwd(fullpath, 1000); // max limit on CWD
	
	printf("\n\n\nTesting\n%s\n\n", fullpath);


	//~ struct stat *stbuf=malloc(sizeof(stbuf));
	//~ 
	//~ char* path="/three";
		//~ // open directory/file, load it into memory; FUSE does not pass anything to get to this
	//~ struct fuse_file_info * fi_2=malloc(11*sizeof(uint64_t)); // about right
	//~ opendir_rr(path, fi_2);
	//~ log_msg("getattr_rr(): fi_2->fh");
	//~ log_msg(fi_2->fh);
	//~ char* block_data=fi_2->fh;
	

	
	
	
	//~ free(stbuf);
	int result=fuse_main(argc, argv, &oper, fullpath);
	free(fullpath); // causing errors?
	return result;
	//~ return 0;
} // for ADDING to free block list, just add to the end of it, read until hit double comma or zero or whatever


// make notes for future use of how to concat strings and int sin c
// using sprint
// using sprintf(fields+strlen(fields) to grow


// figure out later how to un hard code things
// so acn do NUM_FILES / 400, but ceil up (without math.h??) so 10001 isnt "25" free block files

/*
There's only enough space for 400 ptrs in a block of 4096, regardless of teh # of blocks you want. 
* So the # of free blocks would be BLOCKS YOU WANT / 400, rounded up if a remainder, so not int division

Since we have 400 ptrs per blocks, that means you can figure out which block list block to check
* by doing ceil(/400). If densely indexed, easy calculation, but to find first free one, have to keep reading and reading
* and reading to end if nothing free.
* 
Since only the first 400 go in that first block, and they're guaranteed to fit nomatter what,
* can write "#","#", and  FINDING a free block is much faster
* REMOVING a block could be O(n) up to 400 in the worst case
* 
dense indexing is O(1) REMOVAL but O(N) lookup
* 
* consider WHY you need a free block list, to find the first free block in the system
* if densely indexed, O(n) lookup PER 400 ptr block, super super slow
* if sparsely index, O(1) lookup, and O(n) removal for taht ONE 400 ptr block
* 
* do this sparsely indexed, with comma delimiters 



 */ 

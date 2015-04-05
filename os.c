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
		return -ENOENT; 
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
	return 0;
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
	
	// size IS the size of the fi->fh you wrote,
	// and FUSE will take your buf fill and only show the first however many chars
	// your stbuf->st_size was
	// size is.. known?
	//~ size=4096*2000; // originally 4096 for whatever reason
	log_msg("read_rr(): fi->fh");
	if(fi!=NULL && fi->fh!=NULL) { log_msg(fi->fh); }
	//~ if(size+offset>4096) { // fix hard coding for indirect
		//~ size=4096-offset; 
	//~ }
	
	
	
	
	log_msg("read_rr(): buf before=");
	log_msg(buf);
	memcpy(buf, fi->fh + offset, size);
	log_msg("read_rr(): buf after");
	log_msg(buf);
	log_msg("read_rr(): exit");
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

int create_rr(const char * path, mode_t mode, struct fuse_file_info * fi){
	// check if exists
	log_msg("create(): enter\r\ncreate(): path=");
	log_msg(path);
	
	// this doesnt get called when trying to make a file?
	
	if(fi==NULL){
		log_msg("create(): exit on null");
		return 0;
	}
	int result=opendir_rr(path, fi);
	if(result!=ENOENT){ 
		log_msg("create(): exit, file already exists/error");
		return 0;
	}
	
	//~ int free_block=first_free_block();
	
	// if not, get parent directoy node
	// check if enough space to put in file name and not exceed 4096 bytes.
	// fail if no room left
	
	
	// if not, look up first free block
	
	// open that block, get fi->fh as the 4096 bytes
	
	// sprintf all the expected values, follow create_root_dir basically, but different field order
	
	
	// needs to check
	log_msg("create(): exit");
	return 0;
}

int first_free_block();
void fill_dir_inode(char* file_data, int parent_block, int free_block);

int mkdir_rr(const char *path, mode_t mode){
	log_msg("mkdir_rr(): enter\r\nmkdir_rr(): path=");
	log_msg(path);
	
	// looks like mkdir not even called if already exists, but just in case
	struct fuse_file_info * fi=malloc(sizeof(struct fuse_file_info));
	int result=opendir_rr(path, fi);
	if(result!=-ENOENT){ 
		log_msg("mkdir_rr(): exit, file already exists/error");
		// log the inode's data if it does
		if(fi->fh!=NULL) { 
			log_msg("mkdir_rr(): fi->fh="); 
			log_msg(fi->fh); 
			free(fi->fh);
		}
		free(fi);
		return -1;
	}
	
	log_msg("mkdir_rr(): SUCCESS/dir does not already exist");
	// if not, get parent directoy node // check if enough space to put in file name and not exceed 4096 bytes.
	// fail if no room left
	
	
	// get the first free block off the free block list
	int free_block=first_free_block();
	char* debug=malloc(100);
	sprintf(debug, "%d", free_block);
	log_msg("mkdir_rr(): free_block #=");
	log_msg(debug);
		
	// open that block, get fi->fh as the 4096 bytes of 0s
	char* file_data=malloc(4096); // hard coded
	char* file_name=malloc(1000); // hard coded
	file_name[0]='\0';
	sprintf(file_name, "%s", fuse_get_context()->private_data);
	sprintf(file_name+strlen(file_name), "%s", "/blocks/fusedata.");
	sprintf(file_name+strlen(file_name), "%d", free_block);
	result = access(file_name, F_OK);
	if(result!=0){
		log_msg("mkdir_rr(): error opening free block");
		log_msg("mkdir_rr(): file_name=");
		log_msg(file_name);
		free(fi);
		free(debug);
		free(file_data);
		free(file_name);
		return -2;
	}
	FILE* fh=fopen(file_name, "r+");
	fread(file_data, 4096, 1, fh); // hard coded
	
	// load the parent dir's inode
	int parent_pos=strlen(path)-1; // so starts at last char
	while(path[parent_pos]!='/'){ 
		parent_pos--; 
	}
	char* parent_path=malloc(1000);
	strncpy(parent_path, path, parent_pos+1); // so / is pos 0, but 1 byte gets copied
	parent_path[parent_pos+1]='\0';
	result=opendir_rr(parent_path, fi); // assuming this doesn't fail
	free(parent_path);
	int new_dir_pos=parent_pos+1;
	// maybe should check if try to mk two dirs at once that dont exist?

	// extract the parent dir's block #
	char* parent_data=fi->fh; // no malloc needed
	int parent_block_pos=0;
	while(parent_data[parent_block_pos]!=':' || parent_data[parent_block_pos+1]!='.' || parent_data[parent_block_pos+2]!=':'){
		parent_block_pos++;
	} 
	parent_block_pos+=3; // move past :.:
	char* parent_block_num=malloc(100); // hardcoded
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
	int success=fwrite(file_data, 4096, 1, fh);
	fclose(fh);
	
	// write entry into parent dir data
	parent_block_pos=0;
	while(parent_data[parent_block_pos]!='}' || parent_data[parent_block_pos+1]!='}'){ parent_block_pos++; }
	char* new_entry=malloc(100);
	new_entry[0]='\0';
	sprintf(new_entry, "%s", ",d:");
	snprintf(new_entry+strlen(new_entry), strlen(path)-new_dir_pos+1, "%s", path+new_dir_pos);
	sprintf(new_entry+strlen(new_entry), "%s", ":");
	sprintf(new_entry+strlen(new_entry), "%d", free_block); 
	sprintf(new_entry+strlen(new_entry), "%s", "}}");
	sprintf(parent_data+parent_block_pos, "%s", new_entry);
	parent_data[strlen(parent_data)]='0'; // so not '\0' and truncated
	free(new_entry);
	// form file name of parent dir, open, and write entry
	file_name[0]='\0';
	sprintf(file_name, "%s", fuse_get_context()->private_data);
	sprintf(file_name+strlen(file_name), "%s", "/blocks/fusedata.");
	sprintf(file_name+strlen(file_name), "%d", atoi(parent_block_num));
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
		return -2;
	}
	FILE* fh_parent=fopen(file_name, "w"); //overwrite everything
	fwrite(parent_data, 4096, 1, fh_parent);
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
	
void fill_dir_inode(char* file_data, int parent_block, int free_block){ // add SIZE
	// sprintf all the expected values, follow create_root_dir basically, but different field order
	time_t current_time;
	time(&current_time);
	sprintf(file_data, "%s", "{size:"); // have to update this with code
	sprintf(file_data+strlen(file_data), "%d", 4096 ); // FIX HARD CODING
	sprintf(file_data+strlen(file_data), "%s", ",uid:");
	sprintf(file_data+strlen(file_data), "%d", 1); // this is okay hard coding
	sprintf(file_data+strlen(file_data), "%s", ",gid:");
	sprintf(file_data+strlen(file_data), "%d", 1); // this is okay hard coding
	sprintf(file_data+strlen(file_data), "%s", ",mode:");
	sprintf(file_data+strlen(file_data), "%d", 16877); // this is okay hard coding
	sprintf(file_data+strlen(file_data), "%s", ",atime:");
	sprintf(file_data+strlen(file_data), "%d", (int)current_time); 
	sprintf(file_data+strlen(file_data), "%s", ",ctime:");
	sprintf(file_data+strlen(file_data), "%d", (int)current_time); 
	sprintf(file_data+strlen(file_data), "%s", ",mtime:");
	sprintf(file_data+strlen(file_data), "%d", (int)current_time);
	sprintf(file_data+strlen(file_data), "%s", ",linkcount:");
	sprintf(file_data+strlen(file_data), "%d", 2); // this is okay, you link to yourself twice
	sprintf(file_data+strlen(file_data), "%s", ",file_name_to_inode_dict:{d:.:");
	sprintf(file_data+strlen(file_data), "%d", free_block); // FIX HARD CORDING
	sprintf(file_data+strlen(file_data), "%s", ",d:..:");
	sprintf(file_data+strlen(file_data), "%d", parent_block); // FIX HARD CORDING
	sprintf(file_data+strlen(file_data), "%s", "}}");
}

int first_free_block(){
	int current_block_num=1; // hard coded?
	int first_free=-1;
	char* full_path=malloc(1000); // definitely hard coded
	full_path[0]='\0';
	sprintf(full_path, "%s", fuse_get_context()->private_data);
	sprintf(full_path+strlen(full_path), "%s", "/blocks/fusedata.");
	int full_path_pos=strlen(full_path);
	while(current_block_num < 26) { // fix hard coding
		sprintf(full_path+full_path_pos, "%d", current_block_num);
		int result=access(full_path, F_OK);
		if(result==0){
			FILE* fh=fopen(full_path, "r");
			char* file_data=malloc(4096); // hard coded
			fread(file_data, 4096, 1, fh); // hard coded
			int file_pos=0;
			char * first_free_ch=malloc(1000); // FIX HARD CODING
			first_free_ch[0]='\0';
			while(file_data[file_pos]!=','){
				sprintf(first_free_ch+strlen(first_free_ch), "%c", file_data[file_pos]);
				file_pos++;
			}
			if(first_free_ch[0]!='\0'){ // if there is a free block in this block
				first_free=atoi(first_free_ch);
				free(file_data);
				free(first_free_ch);
				free(full_path);
				return first_free;
			}
			free(file_data);
			free(first_free_ch);
		}
		current_block_num++;
	}
	free(full_path);
	return first_free;
}	

int remove_free_block(int rm_block){
	// must leave a ',' if NO blocks left in this chunk, so "first free block" doesn't crash
	// simply delete everything, but check if after your comma, if there is a '0'
	// no # starts with a leading 0, so thats teh signal to leave your comma
	int current_block_num=1; // hard coded?
	char* full_path=malloc(1000); // definitely hard coded
	full_path[0]='\0';
	
	sprintf(full_path, "%s", fuse_get_context()->private_data);
	sprintf(full_path+strlen(full_path), "%s", "/blocks/fusedata.");
	int full_path_pos=strlen(full_path);
	
	while(current_block_num < 26) { // fix hard coding
		sprintf(full_path+full_path_pos, "%d", current_block_num);
		int result=access(full_path, F_OK);
		if(result==0){
			FILE* fh=fopen(full_path, "r+");
			char* file_data=malloc(4096); // hard coded
			fread(file_data, 4096, 1, fh); // hard coded
			
			int file_pos=0;
			char * current_block_num=malloc(1000); // FIX HARD CODING
			current_block_num[0]='\0';
			// search until the last free number
			while(file_pos ==0 || file_data[file_pos-1]!=',' || file_data[file_pos]!='0'){
				while(file_data[file_pos]!=','){
					sprintf(current_block_num+strlen(current_block_num), "%c", file_data[file_pos]);
					file_pos++;
				}
				if(atoi(current_block_num)==rm_block){
					// remove that block, and its comma, only if 0 is not after the comma
					int bytes_removed=strlen(current_block_num);
					if(file_data[file_pos+1]!='0'){ // we know filepos is ',', and no leading 0s, so if next is zero that's end of list
						bytes_removed++; // remove comma unless at the end
					}
					fseek(fh,0,SEEK_SET);
					int written=fwrite(file_data+bytes_removed, 4096-bytes_removed, 1, fh);
					//~ int written=fwrite(file_data, 4096-4, 1, fh);
					
					char* debug=malloc(100);
					sprintf(debug, "%d", written);
					log_msg("rm_fb(): # file_data written");
					log_msg(debug);
					memset(debug, '0', bytes_removed);
					fwrite(debug,1, bytes_removed, fh);
					//~ 
					//~ int letter='3';
					//~ char* buf;
					//~ buf=(char *)malloc(bytes_removed); 
					//~ memset(buf, letter, bytes_removed);
					//~ 
					//~ fseek(fh, 0, SEEK_END); // to append # of 0s
	
					//~ fwrite(buf, bytes_removed, 1, fh);
					fclose(fh);
					// implement this
					free(file_data);
					//~ free(buf);
					// NOTE: fusedata.3 removed values 800-811 for testing, should fix that
					// appended appropriate # of zeros at end, so keep total bytes to 4096
					
					
					// also test this by putting a ',0' after the # we'll be removing (at the start)
					// test this be doing the same thing, but burying the number in the middle of the free
					// block block
					
					// redo the while not , or 0 to account for
					// file_pos ++ after hit a ,
					// but HOW?
					
					
					// thi sdoes in fact stop if put a ,0 at front, and leaves the comma
					// have now also modified fusedata.1 to fusedata.7 free block blocks
					return 0;
				}
				current_block_num[0]='\0'; // strlen =0, so can compare next blcok #
				file_pos++; // move past the ,
			}
			
		}
		current_block_num++;
	}
	free(full_path);
	return -1; // did not find it
}

int mknod_rr(const char *path, mode_t mode, dev_t dev){
	log_msg("mknod(): enter\r\nmknod(): path=");
	log_msg(path);
	//create_rr(path, mode, NULL);

	log_msg("mknod(): exit");
	return 0;
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
			return -1; 
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
				char end_of_name=block_data[dict_pos+2];
				dict_pos+=2; // if success, dict_pos+2=':', if fail, dict_pos='d', so move past it and first ':'
				while(block_data[dict_pos]!=':') { dict_pos++; } // if success, this gets skipped, if fail, this moves to ':'
				dict_pos++; // in either case, move past second ':' to get block #
				if(cmp_pos==strlen(next_dir) && end_of_name==':'){ // you have a match from start of entry to start of searching entry, and searching entry is not a substring of current entry matching from start
					log_msg("opendir_rr(): before next block num"); // ie, so "Fold" and "Folder" don't match
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
				if(fi!=NULL) { fi->fh=NULL; } 
				return -ENOENT;// ERROR, read the whole dict and directory not found, maybe return -1
			}
		}
	} // overall loop ends
	return 0;
}

int write_rr(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
	log_msg("write_rr(): enter\r\nwrite_rr(): path=");
	log_msg(path);
    
  
	log_msg("write_rr(): exit");
    return size;
}

int rm_file(const char* path, char dir_or_reg, char last_link); 

int rmdir_rr(const char * path, struct fuse_file_info * ffi){
	return rm_file(path, 'd', 'y');
}

int rm_file(const char* path, char dir_or_reg, char last_link){
	log_msg("rmdir_rr(): enter");
	//~ if(ffi==NULL){ // don't even use it
		//~ log_msg("rmdir_rr(): NULL ffi, how?");
		//~ return -1;
	//~ }
	struct fuse_file_info* ffi_2=malloc(sizeof(struct fuse_file_info)); // ignore ffi, errors
	opendir_rr(path, ffi_2); // assumes this will work
	char* dir_data=ffi_2->fh;
	// extract dir name from file path
	
	
	// pull up parent dir inode
	
	
	// find dir name in parent dir inode
	
	
	// copy over, leave that out, and handle byte appending
	
	
	// call func to put block # back on free block list
	
	// find pos in path that separates parent dir and file name
	int parent_pos=strlen(path)-1; // so starts at last char
	while(path[parent_pos]!='/'){  parent_pos--; }
	int file_name_pos=parent_pos+1; // to pull out file name
	// copy over parent dir path,  dir name
	char* parent_path=malloc(1000);
	strncpy(parent_path, path, parent_pos+1); // so / is pos 0, but 1 byte gets copied
	parent_path[parent_pos+1]='\0'; // so acts like sprintf
	char* dir_name=malloc(1000);
	sprintf(dir_name, "%s", path+file_name_pos);
	log_msg("rmdir_rr(): dir_name=");
	log_msg(dir_name);
	log_msg("rmdir_rr(): parent_path=");	
	log_msg(parent_path);
	// load parent dir inode, which is NOT the fi loaded here
	struct fuse_file_info * fi=malloc(sizeof(struct fuse_file_info));
	fi->fh=NULL; // just to be safe
	int result=opendir_rr(parent_path, fi); // assuming this doesn't fail
	if(fi->fh==NULL){
		free(fi);
		free(dir_name);
		free(parent_path);
		log_msg("rmdir_rr(): failed to get parent dir inode");
		return -1;
	}
	int len_diff=strlen(dir_name);
	char* parent_data=fi->fh; 
	
	// extract the parent dir's block #
	int parent_block_pos=0;
	while(parent_data[parent_block_pos]!=':' || parent_data[parent_block_pos+1]!='.' || parent_data[parent_block_pos+2]!=':'){
		parent_block_pos++;
	} 
	parent_block_pos+=3; // move past :.:
	char* parent_block_num=malloc(100); // hardcoded
	parent_block_num[0]='\0'; // strlen=0
	while(parent_data[parent_block_pos]!=','){
		sprintf(parent_block_num+strlen(parent_block_num), "%c", parent_data[parent_block_pos]);
		parent_block_pos++;
	}
	
	
	// find position for old entry start and end
	int dict_pos=1, dir_name_start=0, dir_name_end=0;
	while(parent_data[dict_pos]!='{'){ dict_pos++; } // skip to the dictionary
	// then look for every 'd' and 'f' entry, compare
	while(1){
		dict_pos++;
		//~ if(parent_data[dict_pos]=='d'||parent_data[dict_pos]=='f'){ // so can getattr_rr better // +1 is the :
		if(parent_data[dict_pos]==dir_or_reg){ // so can getattr_rr better // +1 is the :
			int cmp_pos=0;
			if(parent_data[dict_pos+2]==dir_name[cmp_pos]) { dir_name_start=dict_pos-1; } // before the comma, remove this comma, leave next one, might be }
			log_msg("rmdir_rr(): current pos");
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
			if(cmp_pos==strlen(dir_name) && end_of_name==':'){ // you have a match from start of entry to start of searching entry, and searching entry is not a substring of current entry matching from start
				
				log_msg("rmdir_rr(): made it here");
				while(parent_data[dict_pos]!=',' && parent_data[dict_pos]!='}') { dict_pos++; }
				dir_name_end=dict_pos; // at the comma
				// ie, so "Fold" and "Folder" don't match
				char* after_old_entry=malloc(4096); // hard coded
				strncpy(after_old_entry, parent_data+dir_name_end, 4096-dir_name_end);
				
				log_msg("rmdir_rr(): after old entry");
				log_msg(after_old_entry);
				log_msg("rmdir_rr(): parent_data b/f fix");
				log_msg(parent_data);
				parent_data[dir_name_start]='\0'; // string ends before the old name starts
				
			
				log_msg("rmdir_rr(): len_diff<0");
				// +1 since dir_name_start a ':', 1 to get past it, and new_name_ch to get past the full name
				// -1 since '\0' char usign sprintf, and so sum is 0
				strncpy(parent_data+dir_name_start, after_old_entry, 4096-dir_name_end); // copy over all of that data
				int i;
				for(i=0; i<abs(len_diff); i++){ // add zeros
					parent_data[4096-abs(len_diff)+i-1]='0'; // overwrite the '\0'
				}
				
				log_msg("rmdir_rr(): parent_data a/f fix");
				log_msg(parent_data);
				free(after_old_entry);
				break; // exit the loop, your job is done
			} // if this 'd' is not the dir you want, keep searching
		}
		else if(parent_data[dict_pos]=='}' && parent_data[dict_pos+1]=='}'){
			log_msg("rmdir_rr(): exit on directory not found in dict");
			if(fi->fh!=NULL) { free(fi->fh); } 
			free(fi);
			free(parent_block_num);
			free(dir_name);
			free(parent_path);
			return -ENOENT;// ERROR, read the whole dict and directory not found, maybe return -1
		}
	}
	// copy over everything up to that
	
	// insert new name
	
	// copy over everything after that
	// NOTE: if new name shorter, append 0s. if new name longer, omitt zeros

	// open parent dir inode file, so can write to it


	log_msg("rmdir_rr(): file name malloc");
	char * file_name=malloc(1000); // fix hard coding
		log_msg("rmdir_rr(): file name malloc2");
	file_name[0]='\0';
	sprintf(file_name, "%s", fuse_get_context()->private_data);
	sprintf(file_name+strlen(file_name), "%s", "/blocks/fusedata.");
	sprintf(file_name+strlen(file_name), "%d", atoi(parent_block_num));
	result = access(file_name, F_OK);
		log_msg("rmdir_rr(): file name malloc3");
	if(result!=0){
		log_msg("mkdir_rr(): error opening parent block");
		log_msg("mkdir_rr(): file_name=");
		log_msg(file_name);
		free(file_name);
		free(parent_block_num);
		free(dir_name);
		free(parent_path);
		free(fi);
		return -2;
	}
	FILE* fh_parent=fopen(file_name, "w"); //overwrite everything
			log_msg("rmdir_rr(): file name malloc5");
	fwrite(parent_data, 4096, 1, fh_parent);
	fclose(fh_parent);	
	

	// extract the current dir's block #
	int current_block_pos=0;
	log_msg("rmdir_rr(): file name malloc8");
	while(dir_data[current_block_pos]!=':' || dir_data[current_block_pos+1]!='.' || dir_data[current_block_pos+2]!=':'){
		current_block_pos++;
	} 
	current_block_pos+=3; // move past :.:
	log_msg("rmdir_rr(): file name malloc9");
	char* current_block_num=malloc(100); // hardcoded
	current_block_num[0]='\0'; // strlen=0
	log_msg("rmdir_rr(): file name malloc10");
	while(dir_data[current_block_pos]!=','){
		sprintf(current_block_num+strlen(current_block_num), "%c", dir_data[current_block_pos]);
		current_block_pos++;
	}
	log_msg("rmdir_rr(): add free block");
	
	if(last_link=='y'){
		add_free_block(current_block_num);
	}

	free(current_block_num);
	free(file_name);
	free(parent_block_num);
	free(dir_name);
	free(parent_path);
	free(fi);
	log_msg("rmdir_rr(): exit");
	return 0;
}
int add_free_block(char* block_num){
	// must leave a ',' if NO blocks left in this chunk, so "first free block" doesn't crash
	// simply delete everything, but check if after your comma, if there is a '0'
	// no # starts with a leading 0, so thats teh signal to leave your comma
		log_msg("rmdir_rr(): add free block1");
	
int current_block_num=1+(atoi(block_num)/400); // hard coded? // 0 is superblock
	char* full_path=malloc(1000); // definitely hard coded
	full_path[0]='\0';
	
	sprintf(full_path, "%s", fuse_get_context()->private_data);
	sprintf(full_path+strlen(full_path), "%s", "/blocks/fusedata.");
	int full_path_pos=strlen(full_path);
	

	sprintf(full_path+full_path_pos, "%d", current_block_num);
	
	int result=access(full_path, F_OK);
	if(result==0){
			log_msg("rmdir_rr(): add free block2");
		FILE* fh=fopen(full_path, "r+");
		char* file_data=malloc(4096); // hard coded
		fread(file_data, 4096, 1, fh); // hard coded
		log_msg("rmdir_rr(): add free block3");
		int file_pos=0;
		while(file_data[file_pos]!=',' || file_data[file_pos+1]!='0' ) { file_pos++; }
		sprintf(file_data+file_pos+1, "%s", block_num);
		int old_len=strlen(file_data);
		file_data[strlen(file_data)]=','; // get rid of the \0
		file_data[old_len+1]='0';
		fseek(fh, 0, SEEK_SET);
		int written=fwrite(file_data, 4096, 1, fh);
		
	
		fclose(fh);
		
		free(file_data);
		
		return 0;
	}
	log_msg("rmdir_rr(): add free block4");
	free(full_path);
	
	return -1;
}


int rename_rr(const char* path, const char* new_name){
	log_msg("rename_rr(): enter\r\nrename_rr(): current=");
	log_msg(path);
	log_msg("rename_rr(): new_name=");
	log_msg(new_name);
	
	// find pos in path that separates parent dir and file name
	int parent_pos=strlen(path)-1; // so starts at last char
	while(path[parent_pos]!='/'){  parent_pos--; }
	int file_name_pos=parent_pos+1; // to pull out file name
	// copy over parent dir path, old name, new name
	char* parent_path=malloc(1000);
	strncpy(parent_path, path, parent_pos+1); // so / is pos 0, but 1 byte gets copied
	parent_path[parent_pos+1]='\0'; // so acts like sprintf
	char* old_name=malloc(1000);
	sprintf(old_name, "%s", path+file_name_pos);
	char* new_name_ch=malloc(1000);
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
	fi->fh=NULL; // just to be safe
	int result=opendir_rr(parent_path, fi); // assuming this doesn't fail
	if(fi->fh==NULL){
		free(fi);
		free(old_name);
		free(new_name_ch);
		free(parent_path);
		log_msg("rename_rr(): failed to get parent dir inode");
		return -1;
	}
	char* parent_data=fi->fh; 
	
	// extract the parent dir's block #
	int parent_block_pos=0;
	while(parent_data[parent_block_pos]!=':' || parent_data[parent_block_pos+1]!='.' || parent_data[parent_block_pos+2]!=':'){
		parent_block_pos++;
	} 
	parent_block_pos+=3; // move past :.:
	char* parent_block_num=malloc(100); // hardcoded
	parent_block_num[0]='\0'; // strlen=0
	while(parent_data[parent_block_pos]!=','){
		sprintf(parent_block_num+strlen(parent_block_num), "%c", parent_data[parent_block_pos]);
		parent_block_pos++;
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
			char* one_char=malloc(2);
			sprintf(one_char, "%c", parent_data[dict_pos+2]);
			log_msg(one_char);
			free(one_char);
			while(parent_data[dict_pos+2]==old_name[cmp_pos]){
				dict_pos++;
				cmp_pos++;
			} // if full success, now on the : bit
			char end_of_name=parent_data[dict_pos+2];
			dict_pos+=2; // if success, dict_pos+2=':', if fail, dict_pos='d', so move past it and first ':'
			while(parent_data[dict_pos]!=':') { dict_pos++; } // if success, this gets skipped, if fail, this moves to ':'
			dict_pos++; // in either case, move past second ':' to get block #
			if(cmp_pos==strlen(old_name) && end_of_name==':'){ // you have a match from start of entry to start of searching entry, and searching entry is not a substring of current entry matching from start
				
				log_msg("rename_rr(): made it here");
				char* one_char2=malloc(2);
				sprintf(one_char2, "%c", parent_data[dict_pos-2]);
				log_msg(one_char2);
				free(one_char);
				
				old_name_end=dict_pos-1; // at the semicolon
				// ie, so "Fold" and "Folder" don't match
				char* after_old_entry=malloc(4096); // hard coded
				strncpy(after_old_entry, parent_data+old_name_end, 4096-old_name_end);
				
				log_msg("rename_rr(): after old entry");
				log_msg(after_old_entry);
				log_msg("rename_rr(): parent_data b/f fix");
				log_msg(parent_data);
				parent_data[old_name_start]='\0'; // string ends before the old name starts
				sprintf(parent_data+strlen(parent_data), "%s", new_name_ch);
				
				if(len_diff>=0){ 
					// +1 since old_name_start a ':', 1 to get past it, and new_name_ch to get past the full name
					// -1 since '\0' char usign sprintf, and so sum is 0
					strncpy(parent_data+old_name_start+strlen(new_name_ch), after_old_entry, 4096-old_name_end-len_diff);
						log_msg("rename_rr(): len_diff>0");
				}
				else { // new name is shorter
					log_msg("rename_rr(): len_diff<0");
					// +1 since old_name_start a ':', 1 to get past it, and new_name_ch to get past the full name
					// -1 since '\0' char usign sprintf, and so sum is 0
					strncpy(parent_data+old_name_start+strlen(new_name_ch), after_old_entry, 4096-old_name_end); // copy over all of that data
					int i;
					for(i=0; i<abs(len_diff); i++){ // add zeros
						parent_data[4096-abs(len_diff)+i-1]='0'; // overwrite the '\0'
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
			if(fi->fh!=NULL) { free(fi->fh); } 
			free(fi);
			free(parent_block_num);
			free(new_name_ch);
			free(old_name);
			free(parent_path);
			return -ENOENT;// ERROR, read the whole dict and directory not found, maybe return -1
		}
	}
	// copy over everything up to that
	
	// insert new name
	
	// copy over everything after that
	// NOTE: if new name shorter, append 0s. if new name longer, omitt zeros

	// open parent dir inode file, so can write to it
	char * file_name=malloc(1000); // fix hard coding
	file_name[0]='\0';
	sprintf(file_name, "%s", fuse_get_context()->private_data);
	sprintf(file_name+strlen(file_name), "%s", "/blocks/fusedata.");
	sprintf(file_name+strlen(file_name), "%d", atoi(parent_block_num));
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
		return -2;
	}
	FILE* fh_parent=fopen(file_name, "w"); //overwrite everything
	fwrite(parent_data, 4096, 1, fh_parent);
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


int get_block_num(const char* path, char dir_or_reg){
	int got_block_num=-1;
	
	// find pos in path that separates parent dir and file name
	int parent_pos=strlen(path)-1; // so starts at last char
	while(path[parent_pos]!='/'){  parent_pos--; }
	int file_name_pos=parent_pos+1; // to pull out file name
	// copy over parent dir path,  dir name
	char* parent_path=malloc(1000);
	strncpy(parent_path, path, parent_pos+1); // so / is pos 0, but 1 byte gets copied
	parent_path[parent_pos+1]='\0'; // so acts like sprintf
	char* dir_name=malloc(1000);
	sprintf(dir_name, "%s", path+file_name_pos);
	log_msg("get_block_num(): dir_name=");
	log_msg(dir_name);
	log_msg("get_block_num(): parent_path=");	
	log_msg(parent_path);
	// load parent dir inode, which is NOT the fi loaded here
	struct fuse_file_info * fi=malloc(sizeof(struct fuse_file_info)); 
	fi->fh=NULL; // just to be safe
	opendir_rr(parent_path, fi); // assuming this doesn't fail
	if(fi->fh==NULL){
		free(fi);
		free(dir_name);
		free(parent_path);
		log_msg("get_block_num(): failed to get parent dir inode");
		return -1;
	}
	char* parent_data=fi->fh; 
	
	// find position for old entry start and end
	int dict_pos=1;
	while(parent_data[dict_pos]!='{'){ dict_pos++; } // skip to the dictionary
	// then look for every 'd' and 'f' entry, compare
	while(1){
		dict_pos++;
		//~ if(parent_data[dict_pos]=='d'||parent_data[dict_pos]=='f'){ // so can getattr_rr better // +1 is the :
		if(parent_data[dict_pos]==dir_or_reg){ // so can getattr_rr better // +1 is the :
			int cmp_pos=0;
			//~ if(parent_data[dict_pos+2]==dir_name[cmp_pos]) { dir_name_start=dict_pos-1; } // before the comma, remove this comma, leave next one, might be }
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
			if(cmp_pos==strlen(dir_name) && end_of_name==':'){ // you have a match from start of entry to start of searching entry, and searching entry is not a substring of current entry matching from start
				// now one past the semi colon
				char* your_block_num=malloc(10); // no more than 10 digits per block #
				your_block_num[0]='\0';
				log_msg("get_block_num(): made it here");
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
			if(fi->fh!=NULL) { free(fi->fh); } 
			free(fi);
			free(parent_path);
			free(dir_name);
			return -ENOENT;// ERROR, read the whole dict and directory not found, maybe return -1
		}
	}
	free(fi);
	free(parent_path);
	free(dir_name);
	return got_block_num;
} 

int unlink_rr(const char * path){
	// load file, load link count #
	log_msg("unlink_rr(): enter");
	struct fuse_file_info* fi = malloc(sizeof(struct fuse_file_info));
	int caught=opendir_rr(path, fi);
	if(fi==NULL||fi->fh==NULL){
		log_msg("unlink_rr(): file inode not found");
		return -1;
	}
	log_msg("unlink_rr(): 1");
	log_msg(fi->fh);
		
	char* inode_data=(char *)fi->fh;
	int data_pos=0;
	while(inode_data[data_pos]!='l' || inode_data[data_pos+1]!='i' || inode_data[data_pos+2]!='n' || inode_data[data_pos+3]!='k') {
		data_pos++; // advance to linkcount field
	}  
	while(inode_data[data_pos-1]!=':') { data_pos++; } // advance to link count #
	int num_link_pos=data_pos;
	char* num_links=malloc(10); // no more than 10 digits worth of links, lets be real
	while(inode_data[data_pos]!=','){
		sprintf(num_links, "%c", inode_data[data_pos]);
		data_pos++;
	}
	int old_str_len=strlen(num_links);

		log_msg("unlink_rr(): 2");
	// copy everything after the old link count
	char* after_old_entry=malloc(4096); // hard coded
	strncpy(after_old_entry, inode_data+data_pos, 4096-data_pos);
	// write a decremented link count, copy back everything
	
		log_msg("unlink_rr(): 2.25");
	int new_link_count=(atoi(num_links)-1);
	log_msg(inode_data);
	inode_data[num_link_pos+1]='\0'; // so string "ends" there
	sprintf(inode_data+num_link_pos, "%d", new_link_count); // appends a '\0' after it
	log_msg(inode_data);
	int len_diff=( strlen(inode_data)-num_link_pos ) - old_str_len;
	
		log_msg("unlink_rr(): 2.5");
		log_msg("unlink_rr(): 3");
	if(len_diff<0){
		strncpy(inode_data+strlen(inode_data), after_old_entry, 4096-data_pos);
		int i;
		for(i=0; i<abs(len_diff); i++){ // add zeros
			inode_data[4096-abs(len_diff)+i-1]='0'; // overwrite the '\0'
		}
	}
	else if(len_diff==0){
		inode_data[strlen(inode_data)]=','; // replace the '\0' with the ',' that should be there
	} // if len_diff >0, this is not unlinking
	free(after_old_entry);
	
		log_msg("unlink_rr(): 3.5");
		log_msg(inode_data);
	// get block # from inode
	int block_num=get_block_num(path, 'f');
	
	// open up the inode block's file on the disk, write back these changes
	char * file_name=malloc(1000); // fix hard coding
	file_name[0]='\0';
	sprintf(file_name, "%s", (char *)fuse_get_context()->private_data);
	sprintf(file_name+strlen(file_name), "%s", "/blocks/fusedata.");
	sprintf(file_name+strlen(file_name), "%d", block_num );
	int result = access(file_name, F_OK);
	if(result!=0){
		log_msg("mkdir_rr(): error opening parent block");
		log_msg("mkdir_rr(): file_name=");
		log_msg(file_name);
		free(file_name);
		free(num_links);
		free(fi);
		return -2;
	}
	
		log_msg("unlink_rr(): 4");
		log_msg(file_name);
		log_msg(inode_data);
	FILE* fh=fopen(file_name, "w"); //overwrite everything
	fseek(fh, 0, SEEK_SET);
	fwrite(inode_data, 4096, 1, fh);
	fclose(fh);	
	
	
		log_msg("unlink_rr(): 5");
	
	// call rm_file to remove it from the parent entry
	// if linkcount==0, call with 'y' to free up blocks
	if(new_link_count==0){ rm_file(path, 'f', 'y'); }
	else { rm_file(path, 'f', 'n'); }
			
	log_msg("unlink_rr(): 6");
	free(num_links);
	free(fi);
	
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
	.create 	= create_rr,
	.mkdir 		= mkdir_rr,
	.write 		= write_rr,
	.rename		= rename_rr,
	.rmdir 		= rmdir_rr,
	.unlink 	= unlink_rr,
};

// replace all '_rr(): ' with '(): ' for log_msg
int main(int argc, char *argv[])
{
	char* fullpath = malloc(1000);
	fullpath=getcwd(fullpath, 1000); // max limit on CWD
	
	printf("\n\n\nTesting\n%s\n\n", fullpath);

// that weird line break thats the same line is a null character
// fix that when creating files from scratch as well, make sure nothing else messes up

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
	int results=fuse_main(argc, argv, &oper, fullpath);
	free(fullpath); // causing errors?
	return results;
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

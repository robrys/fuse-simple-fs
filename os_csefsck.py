#!/usr/bin/env python

from time import time;
BLOCK_SIZE=4096
DEV_ID='20'
FDATA_PATH='/fusedata'
PTRS_PER_BLK=400

'''
CS3224: Homework Assignment #3
Programmer: Robert Ryszewski
Date Due: 4/26/2015

The following assumptions were made in this file checker:
    1) Any entries listed in a directory have a valid inode structure at their specified block #
    2) If updating of any inode leads to data exceeding BLOCK_SIZE, this error is not handled
        as our FS does not handle multi-block inodes. Thus this program simply truncates the inode
        and leaves it in an invalid/"unsaveable" state.
    3) All entries in a directory inode follow the format of a 'file_type_char:file_name_str:block_num'
    4) In general, this fsck does not handle arbitrarily corrupted inodes (where expected delimiters such as
        {, :, and , themselves might be corrupted, or field names like atime might be corrupted.

NOTE: In the fashion of many UNIX utilities, if there are no errors to report,
      csefsck simply produces no output.
'''

def csefsck():
    used_block_list=[]

    sb_info=handle_super_block()
    free_block_list=get_free_list(sb_info)
    process_blocks((free_block_list, used_block_list), sb_info[2], sb_info[2], sb_info[3])
    process_free_list(free_block_list, used_block_list, sb_info)


def handle_super_block():
    '''
    handle_super_block() checks the device ID and creation time fields.
    If the device ID is incorrect, the program exits. If the creation time is in the future, it is fixed and written to the file

    RETURNED: freeStart block #, freeEnd block #, rootDir block #, maxBlocks constant
    '''
    super_block=open('%s/fusedata.0' %(FDATA_PATH), 'r+')
    fields=((super_block.read().replace(',', ' ')).replace(':', ' ')).split()
    if(fields[fields.index('devId')+1]!=DEV_ID): 
        print "ERROR: Device ID is not 20 as expected. Exiting now"
        raise SystemExit()
    if( int(fields[fields.index('{creationTime')+1]) > int(time()) ):
        print "ERROR: Superblock creation time is in the future"
        fields[fields.index('{creationTime')+1]=int(time())
        write_super_block(fields, super_block) # only rewrite superblock if made changes
    super_block.close()
    return (fields[fields.index('freeStart')+1], fields[fields.index('freeEnd')+1], fields[fields.index('root')+1], (fields[fields.index('maxBlocks')+1].rstrip('0').rstrip('}') ) )# freeStart, freeEnd, rootdir, maxBlocks
    # much slower and inefficient than hardcoding, but more robust and well organized

def future_time(inode_data, bnum):
    '''
    future_time() checks to see if the atime, ctime, and mtime timestamps for a file's inode
    are in the future, and fixes them if so.
    
    RETURNED: The fixed inode data fields.
    '''
    current_time=int(time())
    which_time=['atime', 'ctime', 'mtime']
    time_num=0
    while(time_num<3):
        inode_index=inode_data.index(which_time[time_num])+1
        if( int(inode_data[inode_index]) > current_time ):
                print "ERROR: BLOCK # %s's inode's %s was set to a future time of %s and fixed to current time of %d" %(bnum, which_time[time_num], inode_data[inode_index], current_time)
                inode_data[inode_index] = current_time
        time_num+=1
    return inode_data
       
def write_super_block(fields, super_block):
    '''
    write_super_block() writes the given super block inode fields to the given, open super block file.
    The maximum file size is kept to BLOCK_SIZE bytes, and padded with trailing zeros as needed.
    
    RETURNED: Nothing.
    '''
    index=0
    write_str=''
    while(index<len(fields)): # put ':'s and ', 's back appropriately surrounding the fields
        write_str+=str(fields[index])+':'+str(fields[index+1])
        if(index!=len(fields)-2): write_str+=', '
        index+=2;
    if(len(write_str)<BLOCK_SIZE): # pad 0s if needed
       write_str=write_str.ljust(BLOCK_SIZE, '0')
    elif(len(write_str)>BLOCK_SIZE): # truncate length if needed
        write_str=write_str[:BLOCK_SIZE] 
    super_block.seek(0)
    super_block.write(write_str)
    super_block.truncate(BLOCK_SIZE) # truncate file to be safe
    
def get_free_list(free_list_bounds):
    '''
    get_free_list() loads and returns the free block list from all the block files specified within the free_list_bounds.    
    '''
    free_start, free_end=free_list_bounds[0:2]
    current=int(free_start)
    master_block_list=[];
    while(current<=int(free_end)):
        fb_file=open('%s/fusedata.%d' %(FDATA_PATH, current), 'r') 
        new_list=(((fb_file.read().strip().strip('0')).replace(',', ' ')).split()) 
        master_block_list.extend(new_list)
        fb_file.close()
        current+=1
    return master_block_list

def process_blocks(free_block_list, curr_blk, parent_blk, max_blocks):  
    '''
    process_blocks() checks the entire file system recursively, starting from the root directory inode.
    Every file inode in each directory is first processed, then sub directories are recursed into.
    
    RETURNED: Nothing.
    '''
    more_dirs=[curr_blk] # root_dir when function starts
    more_files=[]
    while(len(more_dirs)>0 or len(more_files)>0):
        if(curr_blk=='-1'): break
        inode=open('%s/fusedata.%s' %(FDATA_PATH, curr_blk), 'r+') 
        inode_data=future_time( inode.read().strip().strip('0').replace(',', ' ').replace(':', ' ').split(), curr_blk )
        if(inode_data.count('filename_to_inode_dict')==1 or inode_data.count('file_name_to_inode_dict')==1): # to account for this difference in implementation
            # if reading a directory inode, put all its entries on a queue, proceed to handle all file entries first
            if(len(more_dirs)>0): more_dirs.pop(0) # pop from the front
            more_entries=process_dir(inode_data, free_block_list, (curr_blk, parent_blk), inode )
            for index, item in enumerate(more_entries[0]):
                more_entries[0][index]=(more_entries[0][index], curr_blk) # wrap each dir on the dir_queue with its parent dir
            more_dirs.extend(more_entries[0]) 
            more_files.extend(more_entries[1])
            if(len(more_files)==0 and len(more_dirs)>0): 
                curr_blk=more_dirs[0][0] # only recurse into next dir if all file entries handled
                parent_blk=more_dirs[0][1] # the tuple holds (dir_bnum, parent_bnum)
            elif(len(more_files)!=0): curr_blk=more_files[0] # if have file entries to handle, handle those first
            else : curr_blk='-1' # done with more files and more dirs
        elif(inode_data.count('location')==1):
            # if reading a regular file inode, process it, and load the next file inode if any left, else the next dir inode 
            if(len(more_files)>0): more_files.pop(0)
            process_reg(inode_data, free_block_list, curr_blk, max_blocks, inode )
            if(len(more_files) > 0): curr_blk=more_files[0]
            elif(len(more_dirs)>0): 
                curr_blk=more_dirs[0][0] # if no more files in this directory, handle the sub directories
                parent_blk=more_dirs[0][1]
            else : curr_blk='-1' #if this file is the last to check
        else : print "ERROR: File inode follows neither DIR or REG_FILE conventions"
        inode.close()

def process_dir(inode_data, free_block_list, bnums, dir_file):
    '''
    process_dir() processes a given directory inode's fields and updates the free block list
    and the directory's block file accordingly.
    
    RETURNED: A list containing the block #s of all child dirs and child files (for later processing).
    '''
    need_write=False # only write to the file if any fields are changed
    child_dirs, child_files=[], []
    # to acount for this difference in implementation
    if(inode_data.count('filename_to_inode_dict')==1): iter_pos=inode_data.index('filename_to_inode_dict')+1
    elif(inode_data.count('file_name_to_inode_dict')==1): iter_pos=inode_data.index('file_name_to_inode_dict')+1 
    
    size_field=inode_data.index('{size')
    link_count,self_bnum, parent_bnum=0, -1, -1 # initialize these now in case not found in inode
    if(inode_data[size_field+1]!=BLOCK_SIZE): inode_data[size_field+1]=BLOCK_SIZE # every dir occupies only 1 block in this FS
    
    # process all entries
    while(iter_pos<len(inode_data)): 
        curr_bnum=inode_data[iter_pos+2].strip().strip('{}')
        free_block_list[1].append(curr_bnum) # add every block to a USED BLOCK LIST
        if(free_block_list[0].count(curr_bnum)!=0): 
            free_block_list[0].remove(curr_bnum) # remove any used blocks present on the free block list
            print "ERROR: BLOCK # %s directory was incorrectly listed on the free block list" % (curr_bnum)
            
        if(inode_data[iter_pos].strip('{}')=='d'): # take note of the '.' and '..' entries
            link_count+=1 # link count is # of directories pointing to the current directory
            if(inode_data[iter_pos+1]=='.'): self_bnum=inode_data[iter_pos+2].strip().strip('{}')
            elif(inode_data[iter_pos+1]=='..'): parent_bnum=inode_data[iter_pos+2].strip().strip('{}')
            else : child_dirs.append(curr_bnum) # only a child dir if not '.' or '..'
        elif(inode_data[iter_pos].strip('{}')=='f'): child_files.append(curr_bnum) # have to later process reg file entries too
        iter_pos+=3 #assuming that all entries in proper triple format d:dir_name:block#
    
    # handle errors with '.' entry
    if(self_bnum==-1): #if '.' entry not found, add it
        inode_data[len(inode_data)-1]=inode_data[len(inode_data)-1].strip().strip('{}')
        inode_data.extend(['d', '.', str(bnums[0])+'}}'])
        link_count+=1 #one more link now
        free_block_list[1].append(bnums[0]) # add this encountered bnum to the USED block list
        print "ERROR: BLOCK # %s directory has no entry for '.'" %(bnums[0])
        need_write=True
    elif(self_bnum!=bnums[0]): #if '.' bnum is wrong, fix it
        inode_data[inode_data.index(self_bnum)]=bnums[0]
        free_block_list[1].remove(self_bnum) # remove the incorrect bnum from the USED  block list
        free_block_list[1].append(bnums[0]) # add this encountered bnum to the USED block list
        print "ERROR: BLOCK # %s directory has entry for '.' incorrectly set to %s and fixed to %s" %(bnums[0], self_bnum, bnums[0])        
        need_write=True

    # handle errors with '..' entry
    if(parent_bnum==-1): #if '..' entry not found, add it
        inode_data[len(inode_data)-1]=inode_data[len(inode_data)-1].strip().strip('{}')
        inode_data.extend(['d', '..', str(bnums[1])+'}}'])
        link_count+=1 # one more link now
        free_block_list[1].append(bnums[1]) # add this encountered bnum to the USED block list
        print "ERROR: BLOCK # %s directory has no entry for '..'" %(bnums[0])
        need_write=True
    elif(parent_bnum!=bnums[1]): # if '..' bnum is wrong, fix it
        inode_data[inode_data.index(parent_bnum)]=bnums[1]
        free_block_list[1].remove(parent_bnum) # remove the incorrect bnum from the USED block list
        free_block_list[1].append(bnums[1]) # add this encountered bnum to the USED block list
        print "ERROR: BLOCK # %s directory has entry for '..' incorrectly set to %s and fixed to %s" %(bnums[0],parent_bnum, bnums[1])        
        need_write=True

    # handle errors with linkcount
    link_pos=inode_data.index('linkcount')+1
    if(link_count!=int(inode_data[link_pos])): #fix any invalid linkcounts
        print "ERROR: BLOCK # %s directory had invalid link count of %s fixed to %s" %(bnums[0], inode_data[link_pos], link_count)
        inode_data[link_pos]=link_count
        need_write=True
    
    inode_data[len(inode_data)-1]=inode_data[len(inode_data)-1].strip('{}')+'}}' #in case '.' or '..' being added were the last entries
    if(need_write): write_dir(inode_data, dir_file)
    return (child_dirs, child_files)

def write_dir(fields, dir_file):
    '''
    write_dir() flushes the given fields of a directory inode to a given, open directory block file.    
    The maximum file size is kept to BLOCK_SIZE bytes, and padded with trailing zeros as needed.

    RETURNED: Nothing.
    '''
    index=0
    write_str=''
    if(fields.count('filename_to_inode_dict')==1): dict_pos=fields.index('filename_to_inode_dict')
    elif(fields.count('file_name_to_inode_dict')==1): dict_pos=fields.index('file_name_to_inode_dict')
    
    # all fields before dict are just "field:value, " 
    while(index<dict_pos): 
        write_str+=str(fields[index])+':'+str(fields[index+1])
        if(index!=len(fields)-2): write_str+=', '
        index+=2;
    write_str+=str(fields[index])+': '
    index+=1 # advance to the start of the dict
    while(index+3<=len(fields)):
        write_str+=str(fields[index])+':'+str(fields[index+1])+':'+str(fields[index+2])
        if(index!=len(fields)-3): write_str+=', '
        index+=3
    if(len(write_str)<BLOCK_SIZE): write_str=write_str.ljust(BLOCK_SIZE, '0')
    elif(len(write_str)>BLOCK_SIZE): write_str=write_str[:BLOCK_SIZE] 
    dir_file.seek(0)
    dir_file.write(write_str)
    dir_file.truncate(BLOCK_SIZE)

def process_reg(inode_data, free_block_list, curr_blk, max_blocks, inode):
    '''
    process_reg() processes a given reg file inode's fields and updates the free block list
    and the reg file's block file/indirect file accordingly.
    
    RETURNED: Nothing.
    '''    
    curr_blk=int(curr_blk)
    need_write, is_ind=False, True # assume no changes, assume indirect; modify as process inode data
    ind_pos=inode_data.index('indirect')+1
    loc_bnum=inode_data[inode_data.index('location')+1].strip().strip('{}')  
    loc_file=open('%s/fusedata.%s' %(FDATA_PATH, loc_bnum), 'r+')
    ind_list=loc_file.read().strip().strip('0').replace(',', ' ').split()
    file_size=inode_data[inode_data.index('{size')+1]
    for item in (ind_list):
        if(not(item.isdigit() and int(item)<max_blocks)): 
            is_ind=False 
            break
    if(len(ind_list)==0): #if "indirect file" is all 0s, or an empty file, is not an indirect list
        is_ind=False
        if(inode_data[ind_pos]=='1'):
            print "ERROR: BLOCK # %d regular file had an indirect of '1' when indirect file is empty" %(curr_blk)        
            inode_data[ind_pos]='0'
            need_write=True
        
    # if indirect, handle indirect block errors
    if(is_ind): 
        if(inode_data[ind_pos]=='0'): # if indirect field was '0' and should be '1'
            print "ERROR: BLOCK # %s regular file has an indirect field of '0' when it occupies %s blocks" %(curr_blk, len(ind_list))
            inode_data[ind_pos]='1'
            need_write=True
        if(int(file_size)>BLOCK_SIZE*len(ind_list)): # if file size is larger than # of blocks allocated
            print "ERROR: BLOCK # %s regular file has a size of %s when it's total size given by the indirect block is %s" %(curr_blk, file_size, BLOCK_SIZE*len(ind_list))
            inode_data[inode_data.index('{size')+1]=BLOCK_SIZE*len(ind_list)
            need_write=True
        elif( int(file_size)/BLOCK_SIZE +1 < len(ind_list) ): # if file size is fewer blocks than # allocated
            print "ERROR: BLOCK # %s regular file has size of %s, which does not require the %d blocks listed in the indirect block" %(curr_blk, file_size, len(ind_list))
            blocks_unneeded=len(ind_list)-(int(file_size)/BLOCK_SIZE +1)
            while(blocks_unneeded > 0): # even if it only neesd 1 block now, leaves indirect as 1 for future use
                ind_list.pop(len(ind_list)-1)
                blocks_unneeded-=1
                write_ind(ind_list, loc_file)
            need_write=True

        # if any of the blocks in ind list are on the free block list, remove them
        for index, item in enumerate(ind_list): 
            free_block_list[1].append(item) # add every block in the ind list to the USED block list
            if(free_block_list[0].count(item)!=0): 
                free_block_list[0].remove(item)
                print "ERROR: BLOCK # %s indirect block has an entry for block # %s incorrectly listed on the free block list" % (loc_bnum, item)
     # if no ind block exists, make sure size is less than a block size   
    elif(int(file_size)>BLOCK_SIZE):
        print "ERROR: BLOCK # %s regular file size exceeds block size of %d while indirect is 0" %(curr_blk, BLOCK_SIZE)
        inode_data[inode_data.index('{size')+1]=BLOCK_SIZE     
        need_write=True     

    # whether ind=1 or 0, the location block num is being used
    free_block_list[1].append(loc_bnum) # add it to the USED block list
    if(free_block_list[0].count(loc_bnum)!=0): 
        free_block_list[0].remove(loc_bnum)
        print "ERROR: BLOCK # %s location block was incorrectly listed on the free block list" % (loc_bnum)

    if(need_write): write_reg(inode_data, inode)
    loc_file.close()   
 
def write_reg(fields, reg_file):
    '''
    write_reg() flushes the given fields of a reg file inode to a given, open reg file block file.    
    The maximum file size is kept to BLOCK_SIZE bytes, and padded with trailing zeros as needed.

    RETURNED: Nothing.
    '''
    write_str=''
    index=0
    while(index<len(fields)):
        write_str+=str(fields[index])+':'+str(fields[index+1])
        if(index!=len(fields)-2): write_str+=', '
        index+=2
    if(len(write_str)<BLOCK_SIZE): write_str=write_str.ljust(BLOCK_SIZE, '0')
    elif(len(write_str)>BLOCK_SIZE): write_str=write_str[:BLOCK_SIZE] 
    reg_file.seek(0)
    reg_file.write(write_str)
    reg_file.truncate(BLOCK_SIZE)

def write_ind(fields, ind_file):
    '''
    write_ind() flushes the given blocks of an indirect block file to the given, open indirect block file.    
    The maximum file size is kept to BLOCK_SIZE bytes, and padded with trailing zeros as needed.

    RETURNED: Nothing.
    '''    
    write_str=''
    index=0
    while(index<len(fields)):
       write_str+=str(fields[index])+', ' # leave a trailing comma
       index+=1
    if(len(write_str)<BLOCK_SIZE): write_str=write_str.ljust(BLOCK_SIZE, '0')
    elif(len(write_str)>BLOCK_SIZE): write_str=write_str[:BLOCK_SIZE] 
    ind_file.seek(0)
    ind_file.write(write_str)
    ind_file.truncate(BLOCK_SIZE)

def process_free_list(free_block_list, used_block_list, sb_info):
    '''
    process_free_list() handles all error checking of the free block list.
    Used blocks have already been removed from the FBL in process_blocks(),
    and now any blocks not in either the FBL or UBL are added to the FBL.
    The updated free block list is then written back to its block files.
    
    RETURNED: Nothing.
    '''
    used_block_list=list(set(used_block_list)) 
    for block_num in range(int(sb_info[2]), int(sb_info[3])-1): # from root dir to end of all blocks
        if(used_block_list.count(str(block_num))==0 and free_block_list.count(str(block_num))==0): 
            free_block_list.append(str(block_num)) # if a block is missing from both FBL and UBL, add it to FBL
            print "ERROR: BLOCK # %s was neither in USED block list or FREE block list" %(block_num)
    free_block_list=list(set(free_block_list)) # now this fsck handles any duplicate free blocks :D
    free_block_list.sort(key=float) # this will be VERY slow
    write_free_list(free_block_list, sb_info)

def write_free_list(free_block_list, sb_info):
    '''
    write_free_list() takes the master FBL and writes it back to its appropriate block files.
    PTRS_PER_BLK are written to each BLK file from freeStart to freeEnd as specified in the superBlock.
    
    RETURNED: Nothing.
    '''
    curr_bnum=int(sb_info[0])
    bfile_num=0    
    while(curr_bnum<=int(sb_info[1])):
        curr_bfile=open('%s/fusedata.%d' %(FDATA_PATH, curr_bnum), 'r+')
        write_str=''
        while(len(free_block_list) > 0):
            if( int(free_block_list[0]) > (PTRS_PER_BLK*(bfile_num+1))-1 ): break
            write_str+=str(free_block_list[0])+', ' #if you have ptrs for this block, insert them
            free_block_list.pop(0)
        write_str=write_str.ljust(BLOCK_SIZE, '0')
        write_str=write_str[:BLOCK_SIZE] # truncate any erroneous overflow of data
        curr_bfile.seek(0)
        curr_bfile.write(write_str)
        curr_bfile.truncate(BLOCK_SIZE)
        curr_bfile.close()
        curr_bnum+=1
        bfile_num+=1

if __name__ == "__main__":
    csefsck()
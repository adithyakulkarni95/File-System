#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/stat.h>
#include<unistd.h>
#include<time.h>
#include<stdint.h>
#include <signal.h>
#include <errno.h>

#define NUM_BLOCKS 4226
#define BLOCK_SIZE 8192
#define NUM_FILES  128
#define NUM_INODES 128
#define MAX_BLOCKS_PER_FILE 32

#define WHITESPACE " \t\n"
#define MAX_COMMAND_SIZE 255 

unsigned char data_blocks[NUM_BLOCKS] [BLOCK_SIZE];
         int  used_blocks[NUM_BLOCKS];
         uint8_t *freeInodeList;


struct directory_entry{
    char * name;
    int    valid;
    int    inode_idx;
};

struct directory_entry *directory_ptr;

struct inode {
    time_t date;
    int valid;
    int size;
    int blocks[MAX_BLOCKS_PER_FILE];
    int hidden;
    int readonly;
};

struct inode *inode_array_ptr[NUM_INODES];

char image[225];

/**
 * createfs command
 * filename: name of the image to be create
 */

void create_fs(char *filename) {
    FILE * fd = fopen(filename, "w");

    memset(&data_blocks[0], 0, NUM_BLOCKS * BLOCK_SIZE);

    fwrite(&data_blocks[0], NUM_BLOCKS, BLOCK_SIZE, fd);

    fclose(fd);

}


/**
 * open command
 * filename: name of the image to be open
 */


void open_fs(char *filename) {
    FILE * fs = fopen(filename, "r+");
    memset(data_blocks, 0, NUM_BLOCKS * BLOCK_SIZE);

    fread(data_blocks, NUM_BLOCKS, BLOCK_SIZE, fs);

    fclose(fs);

    strncpy(image, filename, 255);

}

/**
 * close command
 * filename: name of the image to be close
 */
void close_fs() {
    FILE * fs = fopen(image, "w");

    fwrite(data_blocks, NUM_BLOCKS, BLOCK_SIZE, fs);

    fclose(fs);

    image[0] = '\0';
}

/**
 * to set all the used blocks to 0
 * 
 */
void init_block_list() {
    int i;
    for (i = 130; i < 4226; i++) {
        used_blocks[i] = 0;
    }
}

/**
 * df command
 * Display the amount of disk space left in the 
file system
 */
int df ()
{
    int count = 0;
    int i = 0;

    for (i=130; i<4226; i++)
        if (used_blocks[i] == 0)
            count++;
    return count * BLOCK_SIZE;
}

/**
 * to find the free inode block
 */
int find_free_Inode_block (int inode_idx)
{
    int i;
    int ret = -1;
    for (i = 0; i < 32; i++)
    {
        if (inode_array_ptr[inode_idx]->blocks[i] == -1)
        {
            ret = i;
            break;
        }
    }
    return ret;
}


/**
 * to find the free directory block
 */
int find_freeDirectory()
{
    int i;
    int ret = -1;
    for (i=0; i<128; i++)
    {
        if(directory_ptr[i].valid == 0)
        {
            ret=i;
            break;
        }
    }
    return ret;
}


/**
 * to find the free block
 */
int find_freeBlock()
{   
    int i;
    int ret = -1;

    for (i=130; i<4226; i++)
    {
        if (used_blocks[i] == 0)
        {
            ret = i;
            break;
        }
    }    

    return ret;
}

/**
 * to initialize all the inode blocks
 */
void init_inodes() {
    int i;
    for (i = 0; i < 128; i++) {
        inode_array_ptr[i]->valid = 0;
        inode_array_ptr[i]->size = 0;
        inode_array_ptr[i]->hidden = 0;
        inode_array_ptr[i]->readonly = 0;
        int j;
        for (j = 0; j < 1250; j++) {
            inode_array_ptr[i]->blocks[j] = -1;
        }
    }
}



/**
 * to initialize all the directory blocks
 */
void init_dir()
{
    int i;
    directory_ptr = (struct directory_entry*) &data_blocks[0];

    for (i=0; i<NUM_FILES; i++)
        directory_ptr[i].valid = 0;

    int inode_index = 0;

    for (i=1; i<130; i++)
        inode_array_ptr[inode_index++] = (struct inode*) &data_blocks[i];
}
/**
 * to find free Inode
 */
int find_freeInode()
{
    int i;
    int ret =-1;
    for (i=0; i< 128; i++)
    {
        if (inode_array_ptr[i]->valid == 0)
        {
            ret = i;
            break;
        }
    }

    return ret;
}



/**
 * to put the file in to image
 */
void put(char * filename)
{
    struct stat buf;
    int status = stat(filename, &buf);

    if (status ==-1)
    {
        printf("Error: File not found\n");
        return;
    }

    if (buf.st_size > df())
    {
        printf("Error: Not enough room in the file system\n");
        return;
    }
    int dir_index = find_freeDirectory();


    if (dir_index == -1)
    {
        printf("Error: Not enough room in the file system\n");
        return;
    }

    directory_ptr[dir_index].valid = 1;

    directory_ptr[dir_index].name = (char*)malloc(strlen(filename));
    

    strncpy(directory_ptr[dir_index].name, filename, strlen(filename));

    // printf("%s\n", directory_ptr[dir_index].name);

    int inode_index = find_freeInode();

    // printf("Inode Index = %d\n", inode_index);



    if(inode_index == -1)
    {
        printf("Error: No free Inodes\n");
        return;
    }

    directory_ptr[dir_index].inode_idx =  inode_index;

    inode_array_ptr[inode_index]->size = buf.st_size;
    inode_array_ptr[inode_index]->date = time(NULL);
    inode_array_ptr[inode_index]->valid = 1;
    inode_array_ptr[inode_index]->hidden = 0;


    FILE * ifp = fopen(filename, "r");

    int copy_size = buf.st_size;
    int offset = 0;

    int block_index = find_freeBlock();

    if (block_index == -1)
    {
        printf("Error: Cant find free block\n");
        //clean a bunch of directory and inode stuff
        return;
    }


    while(copy_size >= BLOCK_SIZE)
    {   
        int block_index = find_freeBlock();
        
        if (block_index == -1)
        {
            printf("Error: Cant find free block\n");
            return;
        }
        // printf("2\n");

        used_blocks[block_index] = 1;

        int inode_block_entry = find_free_Inode_block(inode_index);

        if (inode_block_entry == 1)
        {
            printf("Error: Cant find free node block\n");
            return;
        }

        inode_array_ptr[inode_index]->blocks[inode_block_entry] = block_index;

        fseek(ifp, offset, SEEK_SET);

        int bytes = fread(data_blocks[block_index], BLOCK_SIZE, 1, ifp);

        if (bytes == 0 && !feof(ifp))
        {
            printf("An error occured reading from the input file.\n");
            return;
        };

        clearerr(ifp);

        copy_size -= BLOCK_SIZE;
        offset += BLOCK_SIZE;

    }

    if (copy_size > 0)
    {   
        int block_index = find_freeBlock();

        
        if (block_index == -1)
        {
            printf("Error: Cant find free block\n");
            return;
        }

        // printf("3\n");


        int inode_block_entry = find_free_Inode_block(inode_index);

        if (inode_block_entry == 1)
        {
            printf("Error: Cant find free node block\n");
            return;
        }

        used_blocks[block_index] = 1;


        fseek(ifp, offset, SEEK_SET);
        int bytes = fread(data_blocks[block_index], copy_size, 1, ifp);

        if (bytes == 0 && !feof(ifp))
        {
            printf("An error occured reading from the input file.\n");
            return;
        };

        clearerr(ifp);

    }

    fclose(ifp);
    return;
}


/**
 * to display all the files in image
 */
void list() {
    int i;
    int flag = 0;

    for (i = 0; i < NUM_FILES; i++) {
        // if valid and not hidden print it
        if (directory_ptr[i].valid == 1 && inode_array_ptr[directory_ptr[i].inode_idx]->hidden == 0) {
            int inode_idex = directory_ptr[i].inode_idx;
            printf("%d %s %s\n", inode_array_ptr[inode_idex]->size, directory_ptr[i].name, ctime( &inode_array_ptr[inode_idex]->date));
            flag++;
        }
    }

    if (flag == 0) {
        printf("list: No files found.\n");
    }
}

/**
 * to find the directory by name
 */
int find_directory_by_name(char *filename) {
    int i;
    int ret = -1;
    
    for (i = 0; i < NUM_FILES; i++) {
        if (directory_ptr[i].valid == 1)
            if (strcmp(filename, directory_ptr[i].name) == 0) {
                ret =i;
                break;
        }
    }
    
    return ret;
}

/**
 * del function
 */
int deleteFile(char * filename) {
    int dir_index = find_directory_by_name(filename);
    int inode_index = directory_ptr[dir_index].inode_idx;


    // check if the file is read only, if so, we cant delete it
    if (inode_array_ptr[inode_index]->readonly == 1) {
        printf("del: File is read only, can't be deleted sorry\n");
        return 1;
    }

    // at the inodes, set every block pointer to by the inode to free
    int i;
    for (i = 0; i < sizeof(inode_array_ptr[inode_index]->blocks); i++) {
        int cur = inode_array_ptr[inode_index]->blocks[i];
        if (inode_array_ptr[inode_index]->blocks[i] == -1)
            break;

        used_blocks[cur] = 0;
        inode_array_ptr[inode_index]->blocks[i] = -1;

    }

    inode_array_ptr[inode_index]->valid = 0;
    inode_array_ptr[inode_index]->size = 0;
    inode_array_ptr[inode_index]->hidden = 0;
    inode_array_ptr[inode_index]->readonly = 0;

    directory_ptr[dir_index].valid = 0;
    directory_ptr[dir_index].inode_idx = -1;

    directory_ptr[dir_index].name = "";

    return 0;

}

/**
 * attrib function
 */

void attrib(char * attrib, char * filename) {
    int dir_index = find_directory_by_name(filename);

    if (dir_index == -1) {
        printf("attrib: File not found\n");
        return;
    }

    // Find the correct inode;
    int inode_index = directory_ptr[dir_index].inode_idx;

    // Mark the file accordingly
    if (strncmp(attrib, "+h", MAX_COMMAND_SIZE) == 0) {
        inode_array_ptr[inode_index]->hidden = 1;
        return;
    }

    if (strncmp(attrib, "-h", MAX_COMMAND_SIZE) == 0) {
        inode_array_ptr[inode_index]->hidden = 0;
        return;
    }

    if (strncmp(attrib, "+r", MAX_COMMAND_SIZE) == 0) {
        inode_array_ptr[inode_index]->readonly = 1;
        return;
    }

    if (strncmp(attrib, "-r", MAX_COMMAND_SIZE) == 0) {
        inode_array_ptr[inode_index]->readonly = 0;
        return;
    }

    printf("attrib: Bad input\n");

}

/**
 * get function
 */

int get(char *filename, char *newfilename) {
    

    int offset = 0;

    int dir_index = find_directory_by_name(filename);

    if (dir_index == -1) {
        printf("get error: File not found\n");
        return -1;
    }

    FILE *ifp;
    ifp = fopen(newfilename, "w");


    if (ifp == NULL) {
        printf("Could not open output file: %s\n", filename);
        perror("Opening output file returned");
        return -1;
    }

    int inode_index = directory_ptr[dir_index].inode_idx;
    int size = inode_array_ptr[inode_index]->size;

    int block_counter = 0;
    int block_index = inode_array_ptr[inode_index]->blocks[block_counter];
    while (size > 0) {

        int bytes;

        if (size < BLOCK_SIZE) {
            bytes = size;
        } else {
            bytes = BLOCK_SIZE;
        }

        fwrite(data_blocks[block_index], bytes, 1, ifp);

        size -= BLOCK_SIZE;
        offset += BLOCK_SIZE;

        block_counter++;
        block_index = inode_array_ptr[inode_index]->blocks[block_counter];
    }

    fclose(ifp);

    return 0;
}


/**
 * main function
 */

int main()
{   
    
    inode_array_ptr[NUM_INODES] = (struct inode *) &data_blocks[3];
    freeInodeList = (uint8_t *)&data_blocks[1]; 
    
    init_dir();
    init_inodes();
    init_block_list();

   char * cmd_str = (char*) malloc( 5 );

   while( 1 )
   {
    // Print out the mfs prompt
        printf ("mfs> ");

        while( !fgets (cmd_str, MAX_COMMAND_SIZE, stdin) );
        
        char *token[5];

        int   token_count = 0;                                 
                                                            
        // Pointer to point to the token
        // parsed by strsep
        char *arg_ptr;                                         
                                                            
        char *working_str  = strdup( cmd_str );                

        // we are going to move the working_str pointer so
        // keep track of its original value so we can deallocate
        // the correct amount at the end
        char *working_root = working_str;


        while (((arg_ptr = strsep(&working_str, WHITESPACE ) ) != NULL) && (token_count<5))
        {
            token[token_count] = (char*)malloc(MAX_COMMAND_SIZE);
            strcpy(token[token_count], arg_ptr);
            if( strlen( token[token_count] ) == 0 )
            {
                token[token_count] = NULL;
            }
                token_count++;
        }

        if (token[0] == NULL) 
        {
            printf("Cannot parse input\n");
        }

        else if (strncmp(token[0], "quit", MAX_COMMAND_SIZE) == 0)
            break;
        
        else if (strncmp(token[0], "createfs", MAX_COMMAND_SIZE) == 0)
            create_fs(token[1]);
        
        else if (strncmp(token[0], "open", MAX_COMMAND_SIZE) == 0)
            open_fs(token[1]);

        else if (strncmp(token[0], "close", MAX_COMMAND_SIZE) == 0)
            close_fs(token[1]);

        else if (strncmp(token[0], "list", MAX_COMMAND_SIZE) == 0)
            list();
        
        else if (strncmp(token[0], "df", MAX_COMMAND_SIZE) == 0) 
            printf("%d bytes free.\n", df());

        

        else if (strncmp(token[0], "put", MAX_COMMAND_SIZE) == 0)
            put(token[1]);
        
        else if (strncmp(token[0], "del", MAX_COMMAND_SIZE) == 0)
            deleteFile(token[1]);

        else if (strncmp(token[0], "get", MAX_COMMAND_SIZE) == 0) 
        {
            if (token_count == 4) 
            {
                get(token[1], token[2]);
            } 
            else 
            {
                get(token[1], token[1]);
            }
        }

        else if (strncmp(token[0], "attrib", MAX_COMMAND_SIZE) == 0) 
        {
            if (token_count != 4) 
            {
                printf("Need a attribute and a file name\n");
                continue;
            }
            attrib(token[1], token[2]);
        }
    
        free(working_root);
    } 
    return 0;
}




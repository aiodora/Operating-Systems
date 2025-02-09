#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <semaphore.h>

#define PIPE_NAME_1 "RESP_PIPE_32455"
#define PIPE_NAME_2 "REQ_PIPE_32455"
#define SHARED_MEMORY_SIZE 5223893

#define SUCCESS "SUCCESS"
#define ERROR "ERROR"

#define VARIANT "VARIANT"
#define VALUE "VALUE"
#define CREATE_SHM "CREATE_SHM"
#define WRITE_TO_SHM "WRITE_TO_SHM"
#define MAPFILE "MAP_FILE"
#define READ_FROM_FILE_OFFSET "READ_FROM_FILE_OFFSET"
#define READ_FROM_FILE_SECTION "READ_FROM_FILE_SECTION"
#define READ_FROM_LOGICAL_SPACE_OFFSET "READ_FROM_LOGICAL_SPACE_OFFSET"
#define EXIT "EXIT"

#define LOGICAL_OFFSET 4096

char* file = NULL;
unsigned int fileSize = 0;
void* share_memory = NULL;

int create_shm(unsigned int shmSize) {
    int fd = shm_open("/65xzGR7c", O_CREAT | O_RDWR, 0664);
    if(fd == -1) {
        return -1;
    }

    ftruncate(fd, shmSize);

    share_memory = mmap(NULL, shmSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (share_memory == MAP_FAILED) {
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

int write_to_shm(unsigned int offset, unsigned int value) {
    if(offset < 0 || offset > SHARED_MEMORY_SIZE) {
        return -1;
    }

    if(offset + sizeof(value) > SHARED_MEMORY_SIZE) {
        return -1;
    }

    int fd = shm_open("/65xzGR7c", O_RDWR, 0664);

    void* writeShm = (unsigned int*)mmap(NULL, SHARED_MEMORY_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(writeShm == MAP_FAILED) {
        close(fd);
        return -1;
    }

    *((unsigned int*)((char*)writeShm + offset)) = value; 
    
    close(fd);
    return 0;
}

int map_file(char* fileName) {
    int fdFile = open(fileName, O_RDONLY, 0664);

    fileSize = lseek(fdFile, 0, SEEK_END);
    lseek(fdFile, 0, SEEK_SET);

    // if(fileSize > SHARED_MEMORY_SIZE) {
    //     return -1;
    // }

    file = (char*)mmap(NULL, fileSize, PROT_READ, MAP_PRIVATE, fdFile, 0);
    if(file == MAP_FAILED) {
        close(fdFile);
        return -1;
    }

    close(fdFile);
    return 0;
}

//The read bytes must be copied at the beginning of the shared memory region, 
//before sending the response message back to the testing program.
int read_from_file_offset(unsigned int offset, unsigned int no_of_bytes) {
    if(offset + no_of_bytes > fileSize || file == NULL) {
        return -1;
    }

    if(offset == 0) {
        return 0;
    }
    
    //memcpy(share_memory, (char*)file + offset, no_of_bytes);
    char* takeFrom = (char*)file + offset;
    char* putHere = (char*)share_memory;
    for(int i = 0; i < no_of_bytes; i++) {
        putHere[i] = takeFrom[i];
    }

    return 0;
}

int read_from_file_section(unsigned int section_no, unsigned int offset, unsigned int no_of_bytes) {
    if (file == NULL) {
        return -1;
    }

    unsigned char *base_ptr = (unsigned char *)file;
    unsigned int start = 9;
    unsigned int sections = *(base_ptr + 8);

    if (section_no > sections) {
        return -1;
    }

    unsigned int section_header_size = 13 + 2 + 4 + 4;
    unsigned int to_offset = 15;
    unsigned int section_offset = *(unsigned int*)(base_ptr + start + (section_no - 1) * section_header_size + to_offset);
    unsigned int section_size = *(unsigned int*)(base_ptr + start + (section_no - 1) * section_header_size + to_offset + 4);

    if (offset + no_of_bytes > section_size) {
        return -1; 
    }

    char* src = (char*)file + section_offset + offset;
    char* dest = (char*)share_memory; 

    for (unsigned int i = 0; i < no_of_bytes; ++i) {
        dest[i] = src[i];
    }

    return 0;
}

int read_from_logical_space_offset(unsigned int logical_offset, unsigned int no_of_bytes) {
    if (file == NULL) {
        return -1;
    }

    unsigned char* base_ptr = (unsigned char*)file;
    unsigned int no_of_sections = *(base_ptr + 8);
    unsigned int start = 9;
    unsigned int current_logical_offset = 0;
    unsigned int section_start_logical_offset = 0;

    for (unsigned int section_no = 1; section_no <= no_of_sections; section_no++) {
        unsigned int section_header_size = 13 + 2 + 4 + 4;
        unsigned int to_offset = 15;
        unsigned int section_offset = *(unsigned int*)(base_ptr + start + (section_no - 1) * section_header_size + to_offset);
        unsigned int section_size = *(unsigned int*)(base_ptr + start + (section_no - 1) * section_header_size + to_offset + 4);
        
        section_start_logical_offset = current_logical_offset;
        current_logical_offset += ((section_size + (LOGICAL_OFFSET - 1)) / LOGICAL_OFFSET) * LOGICAL_OFFSET; 

        if (logical_offset >= section_start_logical_offset && logical_offset < current_logical_offset) {
            unsigned int offset_in_section = logical_offset - section_start_logical_offset;
            if (offset_in_section + no_of_bytes > section_size) {
                return -1;
            }

            char* src = (char*)file + section_offset + offset_in_section;
            char* dest = (char*)share_memory; 

            for (unsigned int i = 0; i < no_of_bytes; ++i) {
                dest[i] = src[i];
            }

            return 0;
        }
    }

    return -1; 
}

int main() {
    unlink(PIPE_NAME_1);
    if(mkfifo(PIPE_NAME_1, 0600) != 0) {
        printf("ERROR1\ncannot create the response pipe | cannot open the request pipe\n");
        fflush(stdout);
        unlink(PIPE_NAME_1);
        exit(1);
    }

    unsigned int fdReq = open(PIPE_NAME_2, O_RDONLY);
    if(fdReq == -1) {
        printf("ERROR3\ncannot create the response pipe | cannot open the request pipe\n");
        fflush(stdout);
        close(fdReq);
        unlink(PIPE_NAME_1);
        exit(1);
    }

    unsigned int fdResp = open(PIPE_NAME_1, O_WRONLY);
    if(fdResp == -1) {
        printf("ERROR3\ncannot create the response pipe | cannot open the request pipe\n");
        fflush(stdout);
        close(fdResp);
        close(fdReq);
        unlink(PIPE_NAME_1);
        exit(1);
    }

    char beginMsg[] = "BEGIN";
    unsigned int beginMsgSize = strlen(beginMsg);
    if(write(fdResp, &beginMsgSize, 1) != 1 || write(fdResp, beginMsg, beginMsgSize) != beginMsgSize) {
        printf("ERROR\ncannot create the response pipe | cannot open the request pipe\n");
        fflush(stdout);
        close(fdResp);
        close(fdReq);
        unlink(PIPE_NAME_1);
        exit(1);
    }

    printf("SUCCESS\n");
    fflush(stdout);

    unsigned int successMsgSize = strlen(SUCCESS);
    unsigned int errorMsgSize = strlen(ERROR);

    while(1) {
        unsigned int cmdSize;
        read(fdReq, &cmdSize, 1);

        char* cmd = (char*)malloc(cmdSize + 1);
        read(fdReq, cmd, cmdSize);
        cmd[cmdSize] = '\0';

        if(strcmp(cmd, VARIANT) == 0) {
            unsigned int variantMsgSize = strlen(VARIANT);
            unsigned int valueMsgSize = strlen(VALUE);
            unsigned int variantNumber = 32455;
            write(fdResp, &variantMsgSize, 1);
            write(fdResp, VARIANT, variantMsgSize);
            write(fdResp, &variantNumber, sizeof(unsigned int));
            write(fdResp, &valueMsgSize, 1);
            write(fdResp, VALUE, valueMsgSize);
        } else if(strcmp(cmd, CREATE_SHM) == 0) {
            unsigned int shmSize = 0;
            read(fdReq, &shmSize, sizeof(shmSize));
            unsigned int createShmSize = strlen(CREATE_SHM);
            write(fdResp, &createShmSize, 1);
            write(fdResp, CREATE_SHM, createShmSize);
            if(create_shm(shmSize) == 0) {
                write(fdResp, &successMsgSize, 1);
                write(fdResp, SUCCESS, successMsgSize);
            } else {
                write(fdResp, &errorMsgSize, 1);
                write(fdResp, ERROR, errorMsgSize);
            }
        } else if(strcmp(cmd, WRITE_TO_SHM) == 0) {
            unsigned int writeToShmSize = strlen(WRITE_TO_SHM);
            unsigned int offset, value;
            read(fdReq, &offset, sizeof(unsigned int));
            read(fdReq, &value, sizeof(unsigned int));
            write(fdResp, &writeToShmSize, 1);
            write(fdResp, WRITE_TO_SHM, writeToShmSize);
            if(write_to_shm(offset, value) == 0) {
                write(fdResp, &successMsgSize, 1);
                write(fdResp, SUCCESS, successMsgSize);
            } else {
                write(fdResp, &errorMsgSize, 1);
                write(fdResp, ERROR, errorMsgSize);
            }
        } else if(strcmp(cmd, MAPFILE) == 0) {
            unsigned int mapFileSize = strlen(MAPFILE);
            unsigned int fileNameSize;
            read(fdReq, &fileNameSize, 1);
            char* fileName = (char*)malloc(fileNameSize + 1);
            read(fdReq, fileName, fileNameSize);
            fileName[fileNameSize] = '\0';
            //printf("%s\n", fileName);
            write(fdResp, &mapFileSize, 1);
            write(fdResp, MAPFILE, mapFileSize);
            if(map_file(fileName) == 0) {
                write(fdResp, &successMsgSize, 1);
                write(fdResp, SUCCESS, successMsgSize);
            } else {
                write(fdResp, &errorMsgSize, 1);
                write(fdResp, ERROR, errorMsgSize);
            }
        } else if(strcmp(cmd, READ_FROM_FILE_OFFSET) == 0) {
            unsigned int readFromFileOffsetSize = strlen(READ_FROM_FILE_OFFSET);
            unsigned int offset, no_of_bytes;
            read(fdReq, &offset, sizeof(unsigned int));
            read(fdReq, &no_of_bytes, sizeof(unsigned int));
            write(fdResp, &readFromFileOffsetSize, 1);
            write(fdResp, READ_FROM_FILE_OFFSET, readFromFileOffsetSize);
            //put in the if the function
            if(read_from_file_offset(offset, no_of_bytes) == 0) {
                write(fdResp, &successMsgSize, 1);
                write(fdResp, SUCCESS, successMsgSize);
            } else {
                write(fdResp, &errorMsgSize, 1);
                write(fdResp, ERROR, errorMsgSize);
            }
        } else if(strcmp(cmd, READ_FROM_FILE_SECTION) == 0) {
            unsigned int readFromFileOffset = strlen(READ_FROM_FILE_SECTION);
            unsigned int section_no, offset, no_of_bytes;
            read(fdReq, &section_no, sizeof(unsigned int));
            read(fdReq, &offset, sizeof(unsigned int));
            read(fdReq, &no_of_bytes, sizeof(unsigned int));
            write(fdResp, &readFromFileOffset, 1);
            write(fdResp, READ_FROM_FILE_SECTION, readFromFileOffset);
            //put in the if the function
            if(read_from_file_section(section_no, offset, no_of_bytes) == 0) {
                write(fdResp, &successMsgSize, 1);
                write(fdResp, SUCCESS, successMsgSize);
            } else {
                write(fdResp, &errorMsgSize, 1);
                write(fdResp, ERROR, errorMsgSize);
            }
        } else if(strcmp(cmd, READ_FROM_LOGICAL_SPACE_OFFSET) == 0) {
            unsigned int readFromLogicalSpaceOffsetSize = strlen(READ_FROM_LOGICAL_SPACE_OFFSET);
            unsigned int logical_offset, no_of_bytes;
            read(fdReq, &logical_offset, sizeof(unsigned int));
            read(fdReq, &no_of_bytes, sizeof(unsigned int));
            write(fdResp, &readFromLogicalSpaceOffsetSize, 1);
            write(fdResp, READ_FROM_LOGICAL_SPACE_OFFSET, readFromLogicalSpaceOffsetSize);
            //put in the if the function
            if(read_from_logical_space_offset(logical_offset, no_of_bytes) == 0) {
                write(fdResp, &successMsgSize, 1);
                write(fdResp, SUCCESS, successMsgSize);
            } else {
                write(fdResp, &errorMsgSize, 1);
                write(fdResp, ERROR, errorMsgSize);
            }
        } else if(strcmp(cmd, EXIT) == 0) { 
            free(cmd);
            close(fdResp);
            close(fdReq);
            unlink(PIPE_NAME_1);
            fflush(stdout);
            return 0;
        } 

        free(cmd);
    }

    return 0; 
}
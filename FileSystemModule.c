#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PATH_LENGTH 1024
#define MAX_NAME_LENGTH 20

typedef struct {
    unsigned char name[MAX_NAME_LENGTH];
    unsigned int type;
    unsigned int offset;
    unsigned int size;
} section_header;

void list(int recursive, char* starts_with, int write_perm, char* dir_path, int init) {
    DIR* dir = opendir(dir_path);
    if (dir == NULL) {
        printf("ERROR\ninvalid directory path\n");
        return;
    }

    if(init) {
        printf("SUCCESS\n");
    }

    char full_path[MAX_PATH_LENGTH];
    struct dirent* entry;
    struct stat inode;
    
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        snprintf(full_path, MAX_PATH_LENGTH, "%s/%s", dir_path, entry->d_name);

        if (lstat(full_path, &inode) < 0) {
            continue; 
        }
        if (starts_with != NULL && strncmp(entry->d_name, starts_with, strlen(starts_with)) != 0) {
            continue; 
        }
        if (write_perm && !(inode.st_mode & S_IWUSR)) {
            continue; 
        }

        printf("%s\n", full_path);

        if (recursive && S_ISDIR(inode.st_mode)) {
            list(recursive, starts_with, write_perm, full_path, 0);
        } else {
            continue;
        }
    }

    closedir(dir);
}

void parse(int fd) {
    unsigned char magic[2];
    if(read(fd, magic, 2) != 2) {
        printf("ERROR\nreading the magic\n");
        return;
    }

    if(!(magic[0] == 'M' && magic[1] == 'q')) { 
        printf("ERROR\nwrong magic\n");
        return;
    }

    unsigned int size;
    if(read(fd, &size, 2) != 2);

    unsigned int version;
    if(read(fd, &version, 4) != 4) {
        printf("ERROR\nreading the version\n");
        return;
    }

    if(version < 22 || version > 94) {
        printf("ERROR\nwrong version\n");
        return;
    }

    unsigned int no_of_sections;
    if(read(fd, &no_of_sections, 1) != 1) {
        printf("ERROR\nreading the number of sections\n");
        return;
    }

    if(no_of_sections != 2 && (no_of_sections < 4 || no_of_sections > 11)) {
        printf("ERROR\nwrong sect_nr\n");
        return;
    }

    section_header* headers_info = (section_header*)malloc(no_of_sections * sizeof(section_header));
    for(int i = 0; i < no_of_sections; i++) {
        if(read(fd, headers_info[i].name, 13) != 13 ||
           read(fd, &headers_info[i].type, 2) != 2 ||
           read(fd, &headers_info[i].offset, 4) != 4 ||
           read(fd, &headers_info[i].size, 4) != 4) {
            printf("ERROR\nreading section header\n");
            free(headers_info);
            return;
        } else {
            if(headers_info[i].type != 27 && headers_info[i].type != 40 && 
               headers_info[i].type != 67 && headers_info[i].type != 81) {
                printf("ERROR\nwrong sect_types\n");
                free(headers_info);
                return;
            }
        }
    }

    printf("SUCCESS\n");
    printf("version=%u\n", version);
    printf("nr_sections=%u\n", no_of_sections); 
    for(int i = 0; i < no_of_sections; i++) {
        printf("section%d: %s %u %u\n", i+1, headers_info[i].name, headers_info[i].type, headers_info[i].size);
    }

    free(headers_info);
    
    return; 
}


int is_sf_file(int fd, int sect_nr, unsigned int* sect_offset, unsigned int* sect_size, int extract) {
    unsigned char magic[2];
    unsigned int version;
    unsigned int no_of_sections = 0;

    if (read(fd, magic, 2) != 2) {
        printf("ERROR\nreading the magic\n");
        return 0; 
    }

    if (!(magic[0] == 'M' && magic[1] == 'q')) {
        return 0;
    }

    unsigned int size;
    if(read(fd, &size, 2) != 2) {
        printf("ERROR\nreading the size\n");
        return 0;
    }

    if (read(fd, &version, 4) != 4) {
        printf("ERROR\nreading the version\n");
        return 0; 
    }

    if (version < 22 || version > 94) {
        return 0; 
    }

    if (read(fd, &no_of_sections, 1) != 1) {
        printf("ERROR\nreading the number of sections\n");
        return 0;
    }

    if (no_of_sections != 2 && (no_of_sections < 4 || no_of_sections > 11)) {
        return 0; 
    }
    
    section_header* headers_info = (section_header*)malloc(no_of_sections * sizeof(section_header));
    
    for (int i = 0; i < no_of_sections; i++) {
        headers_info[i].type = 0;
        if (read(fd, headers_info[i].name, 13) != 13 ||
            read(fd, &headers_info[i].type, 2) != 2 ||
            read(fd, &headers_info[i].offset, 4) != 4 ||
            read(fd, &headers_info[i].size, 4) != 4) {
            printf("ERROR\nreading section header\n");
            free(headers_info);
            return 0;
        } else {
            if (headers_info[i].type != 27 && headers_info[i].type != 40 && 
                headers_info[i].type != 67 && headers_info[i].type != 81) {
                free(headers_info);
                return 0;
            }
        }
    }

    if(extract) {
        if(sect_nr < 1 || sect_nr > no_of_sections) {
            printf("ERROR\ninvalid section\n");
            free(headers_info);
            return 0;
        }
    } else {
        //printf("%d", no_of_sections);
        for(int i = 0; i < no_of_sections; i++) {
            lseek(fd, headers_info[i].offset, SEEK_SET);
            int line_count = 0;
            char* buffer = (char*)malloc(headers_info[i].size);

            int bytes_read = read(fd, buffer, headers_info[i].size);

            if(bytes_read != headers_info[i].size) {
                printf("ERROR\nreading from the section\n");
                free(buffer);
                free(headers_info);
                return 0;
            }

            for(int i = 0; i < bytes_read; i++) {
                if(buffer[i] == '\n' || buffer[i] == '\r' || buffer[i] == 0) {
                    line_count++;
                }
            }
            line_count++;

            free(buffer);

            if(line_count == 13) {
                free(headers_info);
                return 1;
            }
        }
        free(headers_info);
        return 0;
    }

    *sect_offset = headers_info[sect_nr - 1].offset;
    *sect_size = headers_info[sect_nr - 1].size;

    free(headers_info);

    return 1;
}

void extract(int fd, int sect_nr, int line_nr) {
    unsigned int offset, size;

    if(is_sf_file(fd, sect_nr, &offset, &size, 1) == 0) {
        printf("ERROR\ninvalid file\n");
    }

    char* buffer = (char*)malloc(size * sizeof(char));
    if (buffer == NULL) {
        printf("ERROR\nMemory allocation failed\n");
        return;
    }

    lseek(fd, offset, SEEK_SET);
    int bytes_read = read(fd, buffer, size);

    int line_count = 0;
    for (int i = 0; i < bytes_read; i++) {
        if (buffer[i] == '\n') {
            line_count++;
        }
    }

    if (line_count < line_nr || line_nr < 1) {
        printf("ERROR\nInvalid line number\n");
        free(buffer);
        return;
    }

    int to_print = line_count - line_nr + 1;

    int start_pos = 0;
    int end_pos = bytes_read - 1;
    int current_line = 1;

    for (int j = 0; j < bytes_read; j++) {
        if (buffer[j] == '\n') {
            if (current_line == to_print) {
                start_pos = j + 1;
            }
            if (current_line == to_print + 1) {
                end_pos = j - 1;
                break;
            }
            current_line++;
        }
    }

    printf("SUCCESS\n");
    for (int k = start_pos; k <= end_pos; k++) {
        printf("%c", buffer[k]);
    }
    printf("\n");

    free(buffer);

    return;
}

void findall(char* dir_path, int init) {
    DIR* dir = opendir(dir_path);
    if (dir == NULL) {
        printf("ERROR\ninvalid directory path\n");
        return;
    } 

    char full_path[MAX_PATH_LENGTH];
    struct dirent* entry;
    struct stat inode;

    if(init) {
        printf("SUCCESS\n");
    }
    
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        snprintf(full_path, MAX_PATH_LENGTH, "%s/%s", dir_path, entry->d_name);

        if (lstat(full_path, &inode) < 0) {
            continue;
        }

        if (S_ISDIR(inode.st_mode)) {
            findall(full_path, 0);
        } else {
            int fd = open(full_path, O_RDONLY);
            if(fd > 0) {
                int valid = is_sf_file(fd, 0, 0, 0, 0);
                if(valid) {
                    printf("%s\n", full_path);
                }
            }
            close(fd);
        }
    }

    closedir(dir);
    
    return;
}

int main(int argc, char** argv) { 
    if(argc >= 2) {
        if(strcmp(argv[1], "variant") == 0) { 
            printf("32455\n");
        } else if(strcmp(argv[1], "list") == 0) {
            int recursive = 0;
            char* starts_with = NULL;
            int write_perm = 0;
            char* dir_path = NULL;

            struct stat dirName;

            for(int i = 2; i < argc; i++){
                if(strcmp(argv[i], "recursive") == 0) {
                    recursive = 1;
                } else if(strncmp(argv[i], "name_starts_with=", 17) == 0) {
                    starts_with = argv[i] + 17;
                } else if(strcmp(argv[i], "has_perm_write") == 0) {
                    write_perm = 1;
                } else if(strncmp(argv[i], "path=", 5) == 0) {
                    dir_path = argv[i] + 5;
                    if(stat(dir_path, &dirName) < 0) {
                        printf("ERROR\ninvalid directory path\n");
                        exit(1);
                    }
                    if(!S_ISDIR(dirName.st_mode)) {
                        printf("ERROR\npath provided is not a directory\n");
                        exit(1);
                    }
                } else {
                    printf("ERROR\ninvalid parameters introduced\n");
                    exit(1);
                }
            }

            if(dir_path == NULL) {
                printf("ERROR\ndirectory path not provided\n");
                exit(1);
            }

            list(recursive, starts_with, write_perm, dir_path, 1); 
        } else if(strcmp(argv[1], "parse") == 0) {
            if(argc != 3) { 
                printf("Error\nusage ./a1 parse path=<file_path>\n");
                exit(2);
            }

            char* file_path = NULL;
            int fd = -1;
            if(strncmp(argv[2], "path=", 5) == 0) {
                file_path = argv[2] + 5;
            }

            fd = open(file_path, O_RDONLY);
            if(fd < 0) {
                printf("ERROR\nopening the file\n");
                exit(2);
            }
            parse(fd);
            close(fd);
        } else if(strcmp(argv[1], "extract") == 0) {
            char* file_path = NULL;
            int sect_nr = -1;
            int line_nr = -1;

            for(int i = 2; i < argc; i++) {
                if(strncmp(argv[i], "path=", 5) == 0) {
                    file_path = argv[i] + 5;
                } else if(strncmp(argv[i], "section=", 8) == 0) {
                    sect_nr = atoi(argv[i] + 8);
                } else if(strncmp(argv[i], "line=", 5) == 0) {
                    line_nr = atoi(argv[i] + 5);
                }
            }

            int fd = open(file_path, O_RDONLY);
            if(fd < 0) {
                printf("ERROR\nopening the file\n");
                exit(3);
            }

            extract(fd, sect_nr, line_nr);

            close(fd);
        } else if(strcmp(argv[1], "findall") == 0) {
            if(argc != 3) { 
                printf("Error\nusage ./a1 findall path=<dir_path>\n");
                exit(2);
            }

            char* dir_path = NULL;
            struct stat dirName;
            if(strncmp(argv[2], "path=", 5) == 0) {
                dir_path = argv[2] + 5;
            }

            if(stat(dir_path, &dirName) < 0) {
                printf("ERROR\ninvalid directory path\n");
                exit(4);
            }

            if(!S_ISDIR(dirName.st_mode)) {
                printf("ERROR\npath provided is not a directory\n");
                exit(4);
            }

            findall(dir_path, 1);
        } else {
            printf("Usage: ./a1 [OPTIONS] [PARAMETERS]\n");
            exit(5);
        }
    }

    return 0;
}
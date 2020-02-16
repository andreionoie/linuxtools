#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>
#include <stdint.h>

#define MAX_PATH_LEN 600

typedef struct __attribute__((__packed__)) section_header {
    char sect_name[6];
    uint16_t sect_type;
    uint32_t sect_offset;
    uint32_t sect_size;
} section_header;

typedef struct __attribute__((__packed__)) header {
    // 1635267923 = "S5xa" in decimal
    // HEX(53 35 78 61) in little endian
    // or hex 61783553 in big endian
    uint32_t magic;
    uint16_t header_size;
    uint32_t version;
    uint8_t no_of_sections;
} header;

int check_permission(char *permission, struct stat fileMetadata) {
    char perm_check[10];
    int i=0;
    if (fileMetadata.st_mode & S_IRUSR)		// check owner's READ permission (S_IRUSR is the mask 0400)
		perm_check[i++] = 'r';
	else	
		perm_check[i++] = '-';	
	
	if (fileMetadata.st_mode & S_IWUSR)		// check owner's WRITE permission (S_IWUSR is the mask 0200)
		perm_check[i++] = 'w';
	else	
		perm_check[i++] = '-';	
	
	if (fileMetadata.st_mode & S_IXUSR)		// check owner's EXECUTION permission (S_IXUSR is the mask 0100)
		perm_check[i++] = 'x';
	else	
		perm_check[i++] = '-';
		
	if (fileMetadata.st_mode & S_IRGRP)		// check group's READ permission (S_IRGRP is the mask 0040)
		perm_check[i++] = 'r';
	else	
		perm_check[i++] = '-';	
	
	if (fileMetadata.st_mode & S_IWGRP)		// check group's WRITE permission (S_IWGRP is the mask 0020)
		perm_check[i++] = 'w';
	else	
		perm_check[i++] = '-';	
	
	if (fileMetadata.st_mode & S_IXGRP)		// check group's EXECUTION permission (S_IXGRP is the mask 0010)
		perm_check[i++] = 'x';
	else	
		perm_check[i++] = '-';
		
	if (fileMetadata.st_mode & S_IROTH)		// check others' READ permission (S_IROTH is the mask 0004)
		perm_check[i++] = 'r';
	else	
		perm_check[i++] = '-';	
	
	if (fileMetadata.st_mode & S_IWOTH)		// check others' WRITE permission (S_IWOTH is the mask 0002)
		perm_check[i++] = 'w';
	else	
		perm_check[i++] = '-';	
	
	if (fileMetadata.st_mode & S_IXOTH)		// check others' EXECUTION permission (S_IXOTH is the mask 0001)
		perm_check[i++] = 'x';
	else	
		perm_check[i++] = '-';

    perm_check[i] = '\0';
    return strcmp(permission, perm_check);
}

int search_dir(char *dir_name, int maxSize, char *permissions, 
                bool rec, bool checkSize, bool lookPermission) {
    DIR* dir;
	struct dirent *dirEntry;
	struct stat inode;
	char name[MAX_PATH_LEN];

	dir = opendir(dir_name);
	if (dir == 0) {
		perror("Error opening directory");
		exit(4);
	}

	// iterate the directory contents
	while ((dirEntry=readdir(dir)) != 0) {
        // build the complete path to the element in the directory
        if (strcmp(".", dirEntry->d_name) == 0 || strcmp("..", dirEntry->d_name) == 0)
            continue;

		snprintf(name, MAX_PATH_LEN, "%s/%s",dir_name, dirEntry->d_name);
		
		// get info about the directory's element
		lstat (name, &inode);
        if (S_ISDIR(inode.st_mode) && rec) {
            //printf("opening %s\n", name);
            if (rec) {
                search_dir(name, maxSize, permissions, true, checkSize, lookPermission);
            }
        } else if (inode.st_size > maxSize && checkSize) {
                continue;
        }

        if (lookPermission && check_permission(permissions, inode) != 0)
            continue;

        if (checkSize) {
            if (!S_ISDIR(inode.st_mode))
		        printf("%s\n", name);
        } else {
            printf("%s\n", name);
        }
    }

    return closedir(dir);
}

void do_listing(int argc, char **argv) {
    char path[MAX_PATH_LEN];
    bool isRecursive = false;
    bool checkSize = false;
    bool lookPermission = false;
    unsigned long int size_smaller = ULONG_MAX;
    char *permissions = strdup(".........");

    if (sscanf(argv[argc-1], "path=%s", path) < 1) {
        perror("ERROR\ninvalid directory path\n");
        free(permissions);
        exit(5);
    }
    printf("SUCCESS\n");

    for (int i=2; i < argc-1; i++) {
        if (strcmp(argv[i], "recursive") == 0) {
            isRecursive = true;
            break;
        }
    }
    for (int i=2; i < argc-1; i++) {
        if (strstr(argv[i], "size_smaller=") != NULL) {
            sscanf(argv[i], "size_smaller=%lu", &size_smaller);
            checkSize = true;
            break;
        }
    }
    for (int i=2; i < argc-1; i++) {
        if (strstr(argv[i], "permissions=") != NULL) {
            sscanf(argv[i], "permissions=%s", permissions);
            lookPermission = true;
            break;
        }
    }

    //printf("Listing files rec=%d, sz<%d, perm=%s\n", isRecursive, (int) size_smaller, permissions);
    search_dir(path, (int) size_smaller, permissions, isRecursive, checkSize, lookPermission);
    free(permissions);
}

void do_parse(char *path_from_arg) {
    char path[MAX_PATH_LEN];
    char secName[7];
    header file_header;
    section_header *section_headers = NULL;
    int fd;
    sscanf(path_from_arg, "path=%s", path);

    fd = open(path, O_RDONLY);
    read(fd, &file_header, sizeof(header));


    // 1635267923 = "S5xa" in decimal
    if (file_header.magic != 1635267923) {
        printf("ERROR\n");
        printf("wrong magic\n");
        close(fd);
        exit(-1);
    } else if (file_header.version < 17 || file_header.version > 105) {
        printf("ERROR\n");
        printf("wrong version\n");
        close(fd);
        exit(-1);
    } else if (file_header.no_of_sections < 2 || file_header.no_of_sections > 19) {
        printf("ERROR\n");
        printf("wrong sect_nr\n");
        close(fd);
        exit(-1);
    } else {    
        section_headers = calloc(file_header.no_of_sections, sizeof(section_header));
        

        for (int i=0; i < file_header.no_of_sections; i++) {
            read(fd, section_headers + i, sizeof(section_header));
            // for (int j=0; j < 6; j++)
            //     secName[j] = section_headers[i].sect_name[j];
            // secName[6] = '\0';

            // printf("section%d: %s %d %d\n", i+1, secName, section_headers[i].sect_type, section_headers[i].sect_size);
            if (section_headers[i].sect_type != 41 && section_headers[i].sect_type != 76) {
                printf("ERROR\n");
                printf("wrong sect_types\n");
                free(section_headers);
                close(fd);
                exit(-1);
            }
        }
        
        printf("SUCCESS\n");
        printf("version=%d\n", file_header.version);
        printf("nr_sections=%d\n", file_header.no_of_sections);
        for (int i=0; i < file_header.no_of_sections; i++) {
            for (int j=0; j < 6; j++)
                secName[j] = section_headers[i].sect_name[j];
            secName[6] = '\0';

            printf("section%d: %s %d %d\n", i+1, secName, section_headers[i].sect_type, section_headers[i].sect_size);
        }

        free(section_headers);
    }


    close(fd);
}

// char* get_line(int fd, int line_no) {
//     char current_char, prev_char = '0';
//     int current_line_no = 1;
//     int chars_read = 0;
//     char *line = calloc(MAX_PATH_LEN, sizeof(char));

//     while (read(fd, &current_char, 1) == 1) {
//         if (prev_char == 0x0D && current_char == 0x0A) {
   
//             if (current_line_no == line_no) {
//                 lseek(fd, -chars_read-1, SEEK_CUR);
//                 if (chars_read > MAX_PATH_LEN) {
//                     line = realloc(line, chars_read+2);

//                     if (line == NULL) {
//                         printf("Error allocating %u bytes\n", chars_read);
//                         exit(11);
//                     }
//                 }

//                 if (read(fd, line, chars_read-1) <= 0) {
//                     printf("error reading line\n");
//                     return NULL;
//                 }

//                 return line;
//             }

//             chars_read = 0;
//             current_line_no++;
//         } else {
//             chars_read++;
//         }
        
//         prev_char = current_char;
//     } 

//     return NULL;
// }

char* get_line(int fd, int line_no, int sectionsize) {
    int offset = 0;
    int current_line_no = 1;
    int chars_read = 0;
    char *line = calloc(sectionsize + 5, sizeof(char));
    char *buf = calloc(sectionsize + 5, sizeof(char));

    if (read(fd, buf,  sectionsize + 5) > 0) {
        while (sscanf(buf + offset, "%s\r\n%n", line, &chars_read) > 0) {
            if (current_line_no == line_no) {
                free(buf);
                return line;
            }

            offset += chars_read;
            current_line_no++;
        }
        
    }
    free(buf);
    return NULL;
}

char* check_valid_extract(char *path, int sn, int ln) {
    header file_header;
    section_header *section_headers = NULL;
    int fd, ss;
    struct stat inode;
    fd = open(path, O_RDONLY);
    if (fd < 0) {
        printf("ERROR\ninvalid file");
        return NULL;
    }
    if (read(fd, &file_header, sizeof(header)) < 0) {
        printf("ERROR\ninvalid file");
        return NULL;
    }

    // 1635267923 = "S5xa" in decimal
    if (file_header.magic != 1635267923) {
        close(fd);
        printf("ERROR\ninvalid file\n");
        return NULL;
    } else if (file_header.version < 17 || file_header.version > 105) {
        close(fd);
        printf("ERROR\ninvalid file\n");
        return NULL;
    } else if (file_header.no_of_sections < 2 || file_header.no_of_sections > 19) {
        close(fd);
        printf("ERROR\ninvalid file\n");
        return NULL;
    } else if (file_header.no_of_sections < sn ) {
        close(fd);
        printf("ERROR\ninvalid section\n");
        return NULL;
    } else {    
        section_headers = calloc(file_header.no_of_sections, sizeof(section_header));

        for (int i=0; i < file_header.no_of_sections; i++) {
            read(fd, section_headers + i, sizeof(section_header));

            if (section_headers[i].sect_type != 41 && section_headers[i].sect_type != 76) {
                free(section_headers);
                close(fd);
                printf("ERROR\ninvalid section\n");
                return NULL;
            }
        }
        
        lseek(fd, section_headers[sn-1].sect_offset, SEEK_SET);
        fstat(fd, &inode);
        ss = section_headers[sn-1].sect_size;
        free(section_headers);

        return get_line(fd, ln, ss);
    }
        
    
    close(fd);
    printf("ERROR\ninvalid line\n");
    return NULL;
}

void do_extract(char *path_from_arg, char *section_from_arg, char *line_nr_from_arg) {
    char path[MAX_PATH_LEN];
    int section;
    int line_nr;
    char *line;

    if (sscanf(path_from_arg, "path=%s", path) < 1) {
        printf("ERROR\ninvalid file\n");
        exit(6);
    }

    if (sscanf(section_from_arg, "section=%d", &section) < 1) {
        printf("ERROR\ninvalid section\n");
        exit(7);
    }

    if (sscanf(line_nr_from_arg, "line=%d", &line_nr) < 1) {
        printf("ERROR\ninvalid line=%s\n", line_nr_from_arg);
        exit(8);
    }

    line = check_valid_extract(path, section, line_nr);

    if (line)
        printf("SUCCESS\n%s\n", line);

    free(line);
}

bool check_valid_sf(char *path) {
    header file_header;
    section_header *section_headers = NULL;
    int fd;
    int fh76_count=0;

    fd = open(path, O_RDONLY);
    read(fd, &file_header, sizeof(header));

    // 1635267923 = "S5xa" in decimal
    if (file_header.magic != 1635267923) {
        close(fd);
        return false;
    } else if (file_header.version < 17 || file_header.version > 105) {
        close(fd);
        return false;
    } else if (file_header.no_of_sections < 2 || file_header.no_of_sections > 19) {
        close(fd);
        return false;
    } else {    
        section_headers = calloc(file_header.no_of_sections, sizeof(section_header));
        

        for (int i=0; i < file_header.no_of_sections; i++) {
            read(fd, section_headers + i, sizeof(section_header));

            if (section_headers[i].sect_type != 41 && section_headers[i].sect_type != 76) {
                free(section_headers);
                close(fd);
                return false;
            }


            if (section_headers[i].sect_type == 76)
                fh76_count++;
        }
        
        free(section_headers);
    }
        
    
    close(fd);
    return fh76_count >= 5;
}

void do_findall_help(char *dir_name) {
    DIR* dir;
	struct dirent *dirEntry;
	struct stat inode;
	char name[MAX_PATH_LEN];

    dir = opendir(dir_name);
	if (dir == 0) {
        printf("ERROR\ninvalid directory path\n");
		exit(3);
	}

	// iterate the directory contents
	while ((dirEntry=readdir(dir)) != 0) {
        // build the complete path to the element in the directory
        if (strcmp(".", dirEntry->d_name) == 0 || strcmp("..", dirEntry->d_name) == 0)
            continue;

		snprintf(name, MAX_PATH_LEN, "%s/%s",dir_name, dirEntry->d_name);
		
		// get info about the directory's element
		lstat (name, &inode);
        if (S_ISDIR(inode.st_mode)) {
            do_findall_help(name);
        } 

        if (check_valid_sf(name))
            printf("%s\n", name);

    }

    closedir(dir);
}

void do_findall(char *dir_name_arg)
{
    char dir_name[MAX_PATH_LEN];
    DIR* dir;

    if (sscanf(dir_name_arg, "path=%s", dir_name) < 1) {
        printf("ERROR\ninvalid directory path\n");
        exit(4);
    }

    dir = opendir(dir_name);
    if (dir == 0) {
        printf("ERROR\ninvalid directory path\n");
		exit(3);
	} 
    closedir(dir);
    printf("SUCCESS\n");
    do_findall_help(dir_name);
}


int main(int argc, char **argv){
    if (argc >= 2) {
        if(strcmp(argv[1], "variant") == 0) {
            printf("34554\n");
        } else if (strcmp(argv[1], "list") == 0) {
            do_listing(argc, argv);
        } else if (strcmp(argv[1], "parse") == 0) {
            do_parse(argv[2]);
        } else if (strcmp(argv[2], "parse") == 0) {
            do_parse(argv[1]); 
        } else if (strcmp(argv[1], "extract") == 0) {
            do_extract(argv[2], argv[3], argv[4]);
        } else if (strcmp(argv[1], "findall") == 0) {
            do_findall(argv[2]);
        }
    }
    return 0;
}
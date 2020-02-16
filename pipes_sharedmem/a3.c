#include <dirent.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define SHM_KEY 19787
#define RESPONSE_FIFO_NAME "RESP_PIPE_34554"
#define REQUEST_FIFO_NAME "REQ_PIPE_34554"

void init_pipes();
void close_all();

void read_request();
void write_special_req_string(char *str);

void handle_exit();
void handle_ping();
void handle_create_shm();
void handle_shm_write();
void handle_map_file();
void handle_offset();
void handle_file_section();
void handle_logical();

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

unsigned int variant = 34554;

char buf[256];
char bufSize;

char *my_fifo_resp;
char *my_fifo_req;
int fd_resp, fd_req;

int shmId;
unsigned int shmSize;

char *mem_data = NULL;
off_t mapped_file_size;
int section_file_fd;

int main() { 
    init_pipes();

    while (1) {
        read_request();

        handle_exit();
        
        handle_ping();
        handle_create_shm();
        handle_shm_write();
        handle_map_file();
        handle_offset();
        handle_file_section();
        handle_logical();
    }

    close_all();

    return 0;    
}

void write_special_req_string(char *str) {
    char msg_len = strlen(str); 
    write(fd_resp, &msg_len, 1);
    write(fd_resp, str, msg_len);
}

void read_request() {
    read(fd_req, &bufSize, 1);
    read(fd_req, buf, bufSize);
    buf[(int) bufSize] = '\0';
}

void init_pipes() {
    my_fifo_resp = malloc(16);
    my_fifo_req = malloc(15);

    strcpy(my_fifo_resp, RESPONSE_FIFO_NAME);
    strcpy(my_fifo_req, REQUEST_FIFO_NAME);

    unlink(my_fifo_resp);

    if (mkfifo(my_fifo_resp, 0666) < 0) {
        printf("ERROR\ncannot create the response pipe\n");
        exit(-1);
    }

    printf("Opening request pipe..\n");
    fd_req = open(my_fifo_req, O_RDONLY);

    if (fd_req <= 2) {
        printf("ERROR\ncannot open the request pipe");
        exit(-1);
    }

    printf("Opening response pipe..\n");
    fd_resp = open(my_fifo_resp, O_WRONLY);
    
    if (fd_resp <= 2) {
        printf("ERROR\ncannot open the response pipe");
        exit(-1);
    }

    write_special_req_string("CONNECT");

    printf("SUCCESS\n");
}

void close_all() {
    close(fd_resp);
    close(fd_req);
    unlink(my_fifo_resp);
    unlink(my_fifo_req);

        
    munmap(mem_data, mapped_file_size);
    close(section_file_fd);
    shmdt(shmat(shmId, NULL, 0));
    shmctl(shmId, IPC_RMID, 0);
}

void handle_ping() {
    if (strcmp(buf, "PING") == 0) {
        write_special_req_string("PING");
        write_special_req_string("PONG");
        
        // 34554
        write(fd_resp, &variant, sizeof(unsigned int));
    }
}

void handle_exit() {
    if (strcmp(buf, "EXIT") == 0) {
        exit(0);
    }
}

void handle_create_shm() {
    if (strcmp(buf, "CREATE_SHM") == 0) {
        read(fd_req, &shmSize, sizeof(unsigned int));

        shmId = shmget(SHM_KEY, shmSize, IPC_CREAT | 0664);

       
        write_special_req_string("CREATE_SHM");
        if (shmId < 0) {
            write_special_req_string("ERROR");
        } else {
            write_special_req_string("SUCCESS");
        }
    }
}

void handle_shm_write() {
    if (strcmp(buf, "WRITE_TO_SHM") == 0) {
        unsigned int offset;
        char value[5];

        read(fd_req, &offset, sizeof(unsigned int));
        read(fd_req, &value, sizeof(unsigned int));
        value[4] = '\0';

        write_special_req_string("WRITE_TO_SHM");

        if (offset < 0 || offset + sizeof(unsigned int) >= shmSize) {
            write_special_req_string("ERROR");
        } else {
            char *shm_ptr = NULL;

            shm_ptr = (char*) shmat(shmId, NULL, 0);
            snprintf(shm_ptr + offset, sizeof(unsigned int), "%s", value);
            
            shmdt(shm_ptr);
            write_special_req_string("SUCCESS");
        }
    }
}

void handle_map_file() {
    if (strcmp(buf, "MAP_FILE") == 0) {
        read_request();
        printf("File %s\n", buf);
        section_file_fd = open(buf, O_RDWR);

        write_special_req_string("MAP_FILE");
        if (section_file_fd <= 2) {
            section_file_fd = open(buf, O_RDONLY);
            if (section_file_fd <= 2) {
                perror("File not found");
                write_special_req_string("ERROR");
                return;
            }
        }

        mapped_file_size = lseek(section_file_fd, 0, SEEK_END);
        lseek(section_file_fd, 0, SEEK_SET);

        if ((mem_data = mmap(NULL, mapped_file_size, PROT_READ, MAP_PRIVATE, section_file_fd, 0)) == NULL) {
            perror("Couldn't map file");
            write_special_req_string("ERROR");
        } else {
            write_special_req_string("SUCCESS");
        }

    }
}

void handle_offset() {
    if (strcmp(buf, "READ_FROM_FILE_OFFSET") == 0) {
        write_special_req_string("READ_FROM_FILE_OFFSET");

        if (mem_data == NULL) {
            perror("mem_data is NULL");
            write_special_req_string("ERROR");
            return;
        }

        unsigned int offset, no_of_bytes;
        read(fd_req, &offset, sizeof(unsigned int));
        read(fd_req, &no_of_bytes, sizeof(unsigned int));

        if (offset + no_of_bytes >= mapped_file_size || offset < 0 || no_of_bytes < 0) {
            perror("bad offset and no of bytes");
            write_special_req_string("ERROR");
            return;
        }

        char *shm_ptr = (char*) shmat(shmId, NULL, 0);
        memcpy(shm_ptr, mem_data + offset, no_of_bytes + 1);

        write_special_req_string("SUCCESS");
       
    }
}

void handle_file_section() {
    if (strcmp(buf, "READ_FROM_FILE_SECTION") == 0) {
        write_special_req_string("READ_FROM_FILE_SECTION");

        unsigned int section_no, offset, no_of_bytes;
        read(fd_req, &section_no, sizeof(unsigned int));
        read(fd_req, &offset, sizeof(unsigned int));
        read(fd_req, &no_of_bytes, sizeof(unsigned int));

        header file_header;
        memcpy(&file_header, mem_data, sizeof(header));
        section_header *section_headers = calloc(file_header.no_of_sections, sizeof(section_header));

        int read_offset = sizeof(header);
        for (int i=0; i < file_header.no_of_sections; i++) {
            memcpy(section_headers + i, mem_data + read_offset, sizeof(section_header));
            read_offset += sizeof(section_header);
        }

        if (section_no < 1 || section_no > file_header.no_of_sections) {
            write_special_req_string("ERROR");
            return;
        }

        if (offset < 0 || no_of_bytes < 0 || offset + no_of_bytes >= section_headers[section_no - 1].sect_size) {
            write_special_req_string("ERROR");
            return;
        }

        char *shm_ptr = (char*) shmat(shmId, NULL, 0);

        int sectionDataStart = section_headers[section_no - 1].sect_offset + offset;

        memcpy(shm_ptr, mem_data + sectionDataStart, no_of_bytes);
        write_special_req_string("SUCCESS");
    }
}

void handle_logical() {
    if (strcmp(buf, "READ_FROM_LOGICAL_SPACE_OFFSET") == 0) {
        write_special_req_string("READ_FROM_LOGICAL_SPACE_OFFSET");

        unsigned int logical_offset, no_of_bytes;
        read(fd_req, &logical_offset, sizeof(unsigned int));
        read(fd_req, &no_of_bytes, sizeof(unsigned int));

        header file_header;
        memcpy(&file_header, mem_data, sizeof(header));

        section_header *section_headers = calloc(file_header.no_of_sections, sizeof(section_header));
        int read_offset = sizeof(header);
        for (int i=0; i < file_header.no_of_sections; i++) {
            memcpy(section_headers + i, mem_data + read_offset, sizeof(section_header));
            read_offset += sizeof(section_header);
        }

        char *logical_mem = calloc(4096 * (file_header.no_of_sections + 5), sizeof(char));
        int write_logical_offset = 0;

        for (int i=0; i < file_header.no_of_sections; i++) {
            memcpy(logical_mem + write_logical_offset, 
                        mem_data + section_headers[i].sect_offset, 
                                section_headers[i].sect_size);
            write_logical_offset += 4096 * (1 + section_headers[i].sect_size / 4096);
        }

        if (logical_offset < 0 || logical_offset >= write_logical_offset) {
            write_special_req_string("ERROR");
            return;
        }
        
        char *shm_ptr = (char*) shmat(shmId, NULL, 0);
        memcpy(shm_ptr, logical_mem + logical_offset, no_of_bytes + 1);
        write_special_req_string("SUCCESS");
    }
}

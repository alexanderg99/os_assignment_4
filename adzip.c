// Include necessary headers
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <libgen.h>
#include <errno.h>

//create header structure
#define HEADER_SIGNATURE 0xADADADAD
#define MAX_PATH_LEN 256

// Header structure for the .ad file
typedef struct {
    uint32_t signature;      // A unique signature for .ad file format
    uint32_t num_entries;    // Number of metadata entries in the archive
    uint32_t metadata_offset;// Offset to the metadata section in the archive
} Header;

typedef struct {
    char name[MAX_PATH_LEN]; // Name of the file or directory
    uint8_t type;            // Type of the entry (e.g., regular file or directory)
    char path[MAX_PATH_LEN]; // Relative path of the file or directory
    uid_t owner;             // Owner user ID
    gid_t group;             // Group ID
    mode_t rights;           // Access rights (permissions)
    uint32_t size;           // Size of the file (0 for directories)
    uint32_t content_offset; // Offset to the file content in the archive
} Metadata;

void write_file_to_archive(FILE *archive, const char *file_path, uint32_t *header_offset, uint32_t *metadata_offset, int* num_entries) {
    // Open the file
    int fd = open(file_path, O_RDONLY);
    if (fd == -1) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    // Get the file size
    struct stat st;
    if (fstat(fd, &st) == -1) {
        perror("Error getting file size");
        exit(EXIT_FAILURE);
    }
    uint32_t file_size = st.st_size;

    //Get the metadata of the file with stat
    struct stat file_stat;
    if (stat(file_path, &file_stat) == -1) {
        perror("Error getting file metadata");
        exit(EXIT_FAILURE);
    }

    //Initialize a metadata struct and populate it with the relevant details
    Metadata metadata;
    strcpy(metadata.name, file_path);
    metadata.type = DT_REG;
    strcpy(metadata.path, file_path);
    metadata.owner = file_stat.st_uid;
    metadata.group = file_stat.st_gid;
    metadata.rights = file_stat.st_mode;
    metadata.size = file_size;
    metadata.content_offset = *header_offset;
    //metadata.content_offset = 0;

    //print the metadata content offset and the metadata name
    printf("Writing Content at Position: %d\n", metadata.content_offset);
    //print the metadata name
    printf("metadata name: %s\n", metadata.name);
    printf("Writing Metadata at Position: %d\n", *metadata_offset);




    // Allocate a buffer for the file content
    char *buffer = malloc(file_size);
    if (buffer == NULL) {
        perror("Error allocating buffer");
        exit(EXIT_FAILURE);
    }

    // Read the file content into the buffer
    if (read(fd, buffer, file_size) == -1) {
        perror("Error reading file");
        exit(EXIT_FAILURE);
    }

    // Write the file content to the archive
    fseek(archive, metadata.content_offset, SEEK_SET);
    if (fwrite(buffer, file_size, 1, archive) != 1) {
        perror("Error writing to archive");
        exit(EXIT_FAILURE);
    }

    //Go to position in archive where metadata should be written
    fseek(archive, *metadata_offset, SEEK_SET);
    // Write the metadata to the archive
    if (fwrite(&metadata, sizeof(Metadata), 1, archive) != 1) {
        perror("Error writing to archive");
        exit(EXIT_FAILURE);
    }


    // Update the offset
    *header_offset += file_size;
    *metadata_offset += sizeof(Metadata);

    //update num entries
    *num_entries += 1;

    // Close the file
    if (close(fd) == -1) {
        perror("Error closing file");
        exit(EXIT_FAILURE);
    }
}

void traverse_directory(FILE *archive_path, const char *dir_path, const char *base_path, uint32_t *header_offset, uint32_t *metadata_offset, int* num_entries) {

    //printf("dictionary travelled\n");
    DIR *dir = opendir(dir_path);
    if (dir == NULL) {
        perror("Error opening directory");
        exit(EXIT_FAILURE);
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }

            char subdir_path[1024];
            snprintf(subdir_path, sizeof(subdir_path), "%s/%s", dir_path, entry->d_name);
            traverse_directory(archive_path, subdir_path, base_path, header_offset, metadata_offset, num_entries);
        } else if (entry->d_type == DT_REG) {

            char file_path[1024];
            snprintf(file_path, sizeof(file_path), "%s/%s", dir_path, entry->d_name);
            const char *relative_path = file_path + strlen(base_path) - 4;
            write_file_to_archive(archive_path, relative_path, header_offset, metadata_offset, num_entries);



            printf("file written to archive: %s\n", relative_path);
            //(*file_count)++;
        }
    }

    closedir(dir);
}

void calculate_metadata_offset(FILE *archive_path, const char *dir_path, const char *base_path, uint32_t *header_offset, uint32_t *metadata_offset, int* num_entries){
    //almost the same as traverse directory above, just that it doesn't write to the archive and just increments metadata offset

    DIR *dir = opendir(dir_path);
    if (dir == NULL) {
        perror("Error opening directory");
        exit(EXIT_FAILURE);
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }

            char subdir_path[1024];
            snprintf(subdir_path, sizeof(subdir_path), "%s/%s", dir_path, entry->d_name);
            calculate_metadata_offset(archive_path, subdir_path, base_path, header_offset, metadata_offset, num_entries);
        } else if (entry->d_type == DT_REG) {

            char file_path[1024];
            snprintf(file_path, sizeof(file_path), "%s/%s", dir_path, entry->d_name);
            const char *relative_path = file_path + strlen(base_path) - 4;

            // Get the file size
            struct stat st;
            if (stat(file_path, &st) == -1) {
                perror("Error getting file size");
                exit(EXIT_FAILURE);
            }
            uint32_t file_size = st.st_size;

            //increment metadata offset by file size
            *metadata_offset += file_size;


            //print metadata offset
            printf("metadata offset: %d\n", *metadata_offset);
            //(*file_count)++;
        }
    }

    closedir(dir);
}


void create_archive(const char *archive_path, const char *dir_path, int* num_entries) {


    // Initialize a header struct
    Header header;
    header.signature = HEADER_SIGNATURE;
    header.num_entries = 0;
    header.metadata_offset = sizeof(Header);

    //Create an Archive File
    FILE *archive_file = fopen(archive_path, "wb");
    if (archive_file == NULL) {
        perror("Error opening archive file");
        exit(EXIT_FAILURE);
    }

    //calculate offset of header
    uint32_t metadata_offset = sizeof(Header);
    uint32_t header_offset = sizeof(Header);

    //calculate the size of all files + header. it becomes the offset of the first metadata
    calculate_metadata_offset(archive_file, dir_path, dir_path, &header_offset, &metadata_offset, num_entries);
    header.metadata_offset = metadata_offset;


    //traverse the directory specified by input path, and append all file contents to the archive recursively
    traverse_directory(archive_file, dir_path, dir_path, &header_offset, &metadata_offset, num_entries);

    header.num_entries = *num_entries;

    //write the header to the archive
    fseek(archive_file, 0, SEEK_SET);
    if (fwrite(&header, sizeof(Header), 1, archive_file) != 1) {
        perror("Error writing to archive");
        exit(EXIT_FAILURE);
    }

    // Close the archive
    if (fclose(archive_file) == EOF) {
        perror("Error closing archive file");
        exit(EXIT_FAILURE);
    }


}

//implement print_hierarchies function
void print_hierarchies(const char *archive_path){
    //open the archive
    FILE *archive_file = fopen(archive_path, "rb");
    if (archive_file == NULL) {
        perror("Error opening archive file");
        exit(EXIT_FAILURE);
    }

    //go to the header and retrieve the metadata offset
    Header header;
    if (fread(&header, sizeof(Header), 1, archive_file) != 1) {
        perror("Error reading archive header");
        exit(EXIT_FAILURE);
    }
    uint32_t metadata_offset = header.metadata_offset;
    uint32_t num_entries = header.num_entries;

    //find out the size of each piece of metadata
    uint32_t metadata_size = sizeof(Metadata);

    //go to the metadata offset
    fseek(archive_file, metadata_offset, SEEK_SET);

    //read the path of each metadata file
    for (int i = 0; i < num_entries; i++){
        Metadata metadata;
        if (fread(&metadata, metadata_size, 1, archive_file) != 1) {
            perror("Error reading archive metadata");
            exit(EXIT_FAILURE);
        }
        printf("%s\n", metadata.path);
    }

}

//implement extract_hierarchy function
//this function recreates the directory structure of the archive

void create_directories_and_files(const char *path) {
    char temp_path[256];
    char *pointer;
    int fd;

    snprintf(temp_path, sizeof(temp_path), "%s", path);
    for (pointer = temp_path + 1; *pointer; pointer++) {
        if (*pointer == '/') {
            *pointer = '\0';
            mkdir(temp_path, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
            *pointer = '/';
        }
    }
    fd = open(temp_path, O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd != -1) {
        close(fd);
    } else {
        perror("Error creating file");
    }
}


void extract_hierarchy(const char *archive_path, const char *new_path){

    //open the archive
    FILE *archive_file = fopen(archive_path, "rb");
    if (archive_file == NULL) {
            perror("Error opening archive file");
            exit(EXIT_FAILURE);
        }

    //go to the header and retrieve the metadata offset
    Header header;
    if (fread(&header, sizeof(Header), 1, archive_file) != 1) {
        perror("Error reading archive header");
        exit(EXIT_FAILURE);
    }
    uint32_t metadata_offset = header.metadata_offset;
    uint32_t num_entries = header.num_entries;

    //find out the size of each piece of metadata
    uint32_t metadata_size = sizeof(Metadata);

    //read the path of each metadata file
    for (int i = 0; i < num_entries; i++){
        //go to the metadata for the file
        fseek(archive_file, metadata_offset + i * metadata_size, SEEK_SET);
        Metadata metadata;
        if (fread(&metadata, metadata_size, 1, archive_file) != 1) {
            perror("Error reading archive metadata");
            exit(EXIT_FAILURE);
        }

        //concatenate string "new_path/" with the path of the metadata file

        const char *prefix = new_path;
        char full_path[256];
        snprintf(full_path, sizeof(full_path), "%s%s", prefix, metadata.path);
        printf("full path: %s\n", full_path);

        //create the directory and an empty file at the path
        create_directories_and_files(full_path);

        // Open the file at the path
        FILE *file = fopen(full_path, "wb");
        if (file == NULL) {
            perror("Error opening file");
            exit(EXIT_FAILURE);
        }

        //go to the offset of the file contents in the archive
        fseek(archive_file, metadata.content_offset, SEEK_SET);

        //read the file contents from the archive
        char buffer[metadata.size];
        if (fread(buffer, metadata.size, 1, archive_file) != 1) {
            perror("Error reading archive file contents");
            exit(EXIT_FAILURE);
        }

        //print the file contents to terminal
        //printf("%s\n", buffer);

        //write it into the file
        if (fwrite(buffer, metadata.size, 1, file) != 1) {
            perror("Error writing to file");
            exit(EXIT_FAILURE);
        }

        // Close the file
        if (fclose(file) == EOF) {
            perror("Error closing file");
            exit(EXIT_FAILURE);
        }
    }

}


int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s {-c|-p} <archive_path> <input_path>\n", argv[0]);
        return 1;
    }
    const char *flag = argv[1];
    const char *archive_path = argv[2];
    const char *input_path = argv[3];
    int num_entries = 0;

    if (strcmp(flag, "-c") == 0) {
        create_archive(archive_path, input_path, &num_entries);

    } else if (strcmp(flag, "-p") == 0) {
        print_hierarchies(archive_path);
    }
    else if (strcmp(flag, "-x") == 0) {
        extract_hierarchy(archive_path, input_path);
    }

    else {
        fprintf(stderr, "Unknown flag: %s\n", flag);
        return 1;
    }

//./adzip -c test.ad /Users/alexandergunawan/work/Spring2023/OS/os_assignment_4/test
//./adzip -p test.ad /Users/alexandergunawan/work/Spring2023/OS/os_assignment_4/test
//./adzip -x test.ad /Users/alexandergunawan/work/Spring2023/OS/os_assignment_4/dog/


/* ------------ Tests ------------
 *
 *   //read archive and print out the contents
    FILE *archive_file = fopen(archive_path, "rb");
    if (archive_file == NULL) {
        perror("Error opening archive file");
        exit(EXIT_FAILURE);
    }
    //print out the contents
    Header header;
    if (fread(&header, sizeof(Header), 1, archive_file) != 1) {
        perror("Error reading archive");
        exit(EXIT_FAILURE);
    }

    printf("header signature: %u\n", header.signature);
    printf("header num_entries: %u\n", header.num_entries);
    printf("header metadata_offset: %u\n", header.metadata_offset);

    //print the rest of the contents with a buffer
    char *buffer = malloc(9);
    if (buffer == NULL) {
        perror("Error allocating buffer");
        exit(EXIT_FAILURE);
    }
    if (fread(buffer, 9, 1, archive_file) != 1) {
        perror("Error reading archive");
        exit(EXIT_FAILURE);
    }
    printf("buffer: %s\n", buffer);
    free(buffer);

    //print the metadata at position 592
    fseek(archive_file, 592, SEEK_SET);
    Metadata metadata;
    if (fread(&metadata, sizeof(Metadata), 1, archive_file) != 1) {
        perror("Error reading archive");
        exit(EXIT_FAILURE);
    }


    //print metadata name, type and path
    printf("metadata name: %s\n", metadata.name);
    printf("metadata type: %u\n", metadata.type);
    printf("metadata path: %s\n", metadata.path);
*/

    return 0;

}



//traversal is correct.
//
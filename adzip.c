// Include necessary headers
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

//test
// Define constants and structures
#define HEADER_SIGNATURE 0xADADADAD
#define MAX_PATH_LEN 256

// Header structure for the .ad file
typedef struct {
    uint32_t signature;      // A unique signature for .ad file format
    uint32_t num_entries;    // Number of metadata entries in the archive
    uint32_t metadata_offset;// Offset to the metadata section in the archive
} Header;

// Metadata structure for files and directories in the archive
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

void print_directory(FILE *archive, Metadata *parent_metadata, int depth) {
    for (int i = 0; i < depth; i++) {
        printf("  ");
    }
    printf("%s\n", parent_metadata->path);

    for (int i = 0; i < parent_metadata->size; i++) {
        Metadata metadata;
        fread(&metadata, sizeof(Metadata), 1, archive);

        if (metadata.type == DT_DIR) {
            print_directory(archive, &metadata, depth + 1);
        } else {
            for (int j = 0; j < depth + 1; j++) {
                printf("  ");
            }
            printf("%s\n", metadata.path);
        }
    }
}

void print_hierarchy(FILE *archive) {
    Header header;
    fread(&header, sizeof(Header), 1, archive);

    if (header.signature != HEADER_SIGNATURE) {
        fprintf(stderr, "Invalid archive file.\n");
        return;
    }
    //print the header object
    printf("header.signature: %d\n", header.signature);
    printf("header.num_entries: %d\n", header.num_entries);
    printf("header.metadata_offset: %d\n", header.metadata_offset);

    fseek(archive, header.metadata_offset, SEEK_SET);

    Metadata root_metadata;
    fread(&root_metadata, sizeof(Metadata), 1, archive);

    if (root_metadata.type == DT_DIR) {
        print_directory(archive, &root_metadata, 0);
    } else {
        printf("%s\n", root_metadata.path);
    }
}

void display_hierarchy(const char *archive_path) {
    FILE *archive = fopen(archive_path, "rb");
    if (!archive) {
        perror("fopen");
        return;
    }

    printf("Archive: %s\n", archive_path);
    print_hierarchy(archive);

    fclose(archive);
}


// Function to traverse a directory recursively and store its contents in the archive
void store_directory(FILE *archive, const char *dir_path, int *metadata_offset, int *data_offset) {
    // Open the directory
    DIR *dir = opendir(dir_path);
    if (!dir) {
        perror("opendir");
        return;
    }
    printf("dir_path: %s\n", dir_path);

    struct dirent *entry;
    // Iterate over directory entries
    while ((entry = readdir(dir))) {

        // Skip "." and ".." entries
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Create the full path for the current entry
        char entry_path[MAX_PATH_LEN];
        snprintf(entry_path, sizeof(entry_path), "%s/%s", dir_path, entry->d_name);
        printf("entry_path: %s\n", entry_path);

        // Get the entry's metadata (e.g., file type, owner, group, rights)
        struct stat st;
        if (stat(entry_path, &st) == -1) {
            perror("stat");
            continue;
        }
        //print the entry's metadata
        printf("st_uid: %d\n", st.st_uid);
        printf("st_gid: %d\n", st.st_gid);
        printf("st_mode: %d\n", st.st_mode);
        printf("st_size: %d\n", st.st_size);
        //print the name of the file
        printf("entry->d_name: %s\n", entry->d_name);

        // Populate the metadata structure
        Metadata metadata;
        strncpy(metadata.name, entry->d_name, MAX_PATH_LEN);
        strncpy(metadata.path, entry_path, MAX_PATH_LEN);
        metadata.type = entry->d_type;
        metadata.owner = st.st_uid;
        metadata.group = st.st_gid;
        metadata.rights = st.st_mode;
        metadata.content_offset = *data_offset;

        // If the entry is a directory, process it recursively
        if (entry->d_type == DT_DIR) {
            //print that it is a directory
            printf("DT_DIR\n");
            store_directory(archive, entry_path, metadata_offset, data_offset);
        } else if (entry->d_type == DT_REG) {
            // If the entry is a regular file, store its contents in the archive
            metadata.size = st.st_size;
            FILE *input_file = fopen(entry_path, "rb");
            if (!input_file) {
                perror("fopen");
                continue;
            }

            // Write the file's content to the archive
            fseek(archive, * , SEEK_SET);
            char buffer[1024];
            size_t bytes_read;
            while ((bytes_read = fread(buffer, 1, sizeof(buffer), input_file)) > 0) {
                fwrite(buffer, 1, bytes_read,
                       archive);
                *data_offset += bytes_read;
            }
            fclose(input_file);
            //print the file's content
            printf("file content: %s\n", buffer);

        } else {
            // If the entry is not a regular file or a directory, skip it
            continue;
        }

        // Write the metadata to the archive
        fseek(archive, *metadata_offset, SEEK_SET);
        fwrite(&metadata, sizeof(Metadata), 1, archive);
        *metadata_offset += sizeof(Metadata);
        //print that it has been written to the archive
        printf("metadata has been written to the archive\n");

    }

// Close the directory
    closedir(dir);
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s {-c|-p} <archive_path> <input_path>\n", argv[0]);
        return 1;
    }

    const char *flag = argv[1];
    const char *archive_path = argv[2];
    const char *input_path = argv[3];

    if (strcmp(flag, "-c") == 0) {
        // Create the archive file
        FILE *archive = fopen(archive_path, "wb");
        if (!archive) {
            perror("fopen");
            return 1;
        }

        // Write an empty header as a placeholder, will update later
        Header header = {0};
        fwrite(&header, sizeof(Header), 1, archive);

        int metadata_offset = sizeof(Header);
        int data_offset = sizeof(Header);

        // Check if the input is a file or directory
        struct stat st;
        if (stat(input_path, &st) == -1) {
            perror("stat");
            fclose(archive);
            return 1;
        }

        if (S_ISDIR(st.st_mode)) {
            store_directory(archive, input_path, &metadata_offset, &data_offset);
        } else {
            // If input is a file, wrap it in a single-entry directory for simplicity
            char parent_dir[MAX_PATH_LEN];
            strncpy(parent_dir, input_path, MAX_PATH_LEN);
            char *last_slash = strrchr(parent_dir, '/');
            if (last_slash) {
                *last_slash = '\0';
            } else {
                strcpy(parent_dir, ".");
            }

            store_directory(archive, parent_dir, &metadata_offset, &data_offset);
        }

        // Update the header with the actual values and write it to the beginning of the file
        header.signature = HEADER_SIGNATURE;
        header.num_entries = (metadata_offset - sizeof(Header)) / sizeof(Metadata);
        header.metadata_offset = metadata_offset;
        //print the header's metadata
        printf("header.signature: %d\n", header.signature);
        printf("header.num_entries: %d\n", header.num_entries);
        printf("header.metadata_offset: %d\n", header.metadata_offset);

        fseek(archive, 0, SEEK_SET);
        fwrite(&header, sizeof(Header), 1, archive);

        fclose(archive);

    } else if (strcmp(flag, "-p") == 0) {
        display_hierarchy(archive_path);
    } else {
        fprintf(stderr, "Unknown flag: %s\n", flag);
        return 1;
    }

    return 0;

}
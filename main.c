#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#define MAX_PATH_LENGTH 256

int count_files_in_folder(const char *path);

int main(int argc, char *argv[]){
    char path_folder_1[MAX_PATH_LENGTH];
    char path_folder_2[MAX_PATH_LENGTH];

    if (argc != 3){
        printf("Not enough routes.\n");
        return 1;
    }

    strncpy(path_folder_1, argv[1], MAX_PATH_LENGTH -1);
    strncpy(path_folder_2, argv[2], MAX_PATH_LENGTH -1);

    path_folder_1[MAX_PATH_LENGTH - 1] = '\0';
    path_folder_2[MAX_PATH_LENGTH - 1] = '\0';

    printf("First folder path is: %s\n", path_folder_1);
    printf("Second folder path is: %s\n", path_folder_2);

    int file_count = count_files_in_folder(path_folder_1);

    printf("Total number of files in forlder (%s) is: %d\n",path_folder_1,file_count);
    return 0;
}

int count_files_in_folder(const char *path){
    DIR *dir;
    struct dirent *entry;
    struct stat statbuf;
    int count = 0;

    if ((dir = opendir(path)) == NULL){
        fprintf(stderr, "Cannot open directory: %s\n", path);
    }

    while ((entry = readdir(dir)) != NULL) {
        char full_path[MAX_PATH_LENGTH];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        if (lstat(full_path, &statbuf) == -1) {
            fprintf(stderr, "Cannot stat file: %s\n", full_path);
            continue;
        }

        if (S_ISDIR(statbuf.st_mode)) {
            // Ignore . and .. directories
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;

            count += count_files_in_folder(full_path); // Recursively count files in subdirectory
        } else if (S_ISREG(statbuf.st_mode)) {
            count++; // Regular file
        }
    }

    closedir(dir);
    return count;
}
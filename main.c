#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

#include "queue.h"

#define MAX_PATH_LENGTH 256
#define FOUND 0
#define NOT_FOUND 1
#define OPEN_ERROR 2
#define READ_ERROR 4

int count_files_in_folder(const char *path, struct Queue *file_queue);

int main(int argc, char *argv[]){
    char path_folder_1[MAX_PATH_LENGTH];
    char path_folder_2[MAX_PATH_LENGTH];

    struct Queue *path_queue = createQueue();

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

    int file_count = count_files_in_folder(path_folder_1, path_queue);

    printf("Total number of files in forlder (%s) is: %d\n",path_folder_1,file_count);
    printf("Content in queue: \n");
    printQueue(path_queue);
    return 0;
}

int count_files_in_folder(const char *path, struct Queue *file_queue){
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

            count += count_files_in_folder(full_path,file_queue); // Recursively count files in subdirectory
        } else if (S_ISREG(statbuf.st_mode)) {
            enqueue(file_queue,full_path);
            //printf("File name: %s\n",full_path);
            count++; // Regular file
        }
    }

    closedir(dir);
    return count;
}
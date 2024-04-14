#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

#include "queue.h"

#define MAX_PATH_LENGTH 512
#define BUFFER_SIZE 4096

int count_files_in_folder(const char *path, struct Queue *file_queue);
void copy_files(struct Queue *file_queue);
int copy_file(char *source_path, char *destination_path);
char *replace_source_path(char *source_path, char *file_source_path, char *destination_path);

int main(int argc, char *argv[]){
    char path_folder_1[MAX_PATH_LENGTH];
    char path_folder_2[MAX_PATH_LENGTH];

    struct Queue *path_queue = createQueue();

    if (argc != 3){
        printf("Not enough routes.\n");
        return 1;
    }

    strncpy(path_folder_1, argv[1], MAX_PATH_LENGTH - 1);
    strncpy(path_folder_2, argv[2], MAX_PATH_LENGTH - 1);

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
    /*
        Recieves a directory path and a pointer to a queue that stores paths
        Counts every file in the directory and each of the subdirectories and 
        returns that count while also storing those files paths into the queue 
    */
    DIR *dir;
    struct dirent *entry;
    struct stat statbuf;
    int count = 0;

    if ((dir = opendir(path)) == NULL){
        fprintf(stderr, "Cannot open directory: %s\n", path);
    }

    while ((entry = readdir(dir)) != NULL) {
        char full_path[MAX_PATH_LENGTH];
        snprintf(full_path, sizeof(full_path)*2, "%s/%s", path, entry->d_name);

        if (lstat(full_path, &statbuf) == -1) {
            fprintf(stderr, "Cannot stat file: %s\n", full_path);
            continue;
        }

        if (S_ISDIR(statbuf.st_mode)) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;

            count += count_files_in_folder(full_path,file_queue);
        } else if (S_ISREG(statbuf.st_mode)) {
            enqueue(file_queue,full_path);
            replace_source_path("/home/kenneth/Desktop/copy-this",full_path,"/home/kenneth/Desktop/paste-here");
            count++;
        }
    }

    closedir(dir);
    return count;
}

void copy_files(struct Queue *file_queue){

}

int copy_file(char *source_path, char *destination_path){
    FILE *source_file, *destination_file;
    char buffer[BUFFER_SIZE];
    size_t bytes_read;

    // Open the source file for reading
    source_file = fopen(source_path, "rb");
    if (source_file == NULL) {
        perror("Failed to open source file");
        return 1;
    }

    // Create the directory for the destination file if it doesn't exist
    if (mkdir("home/kenneth/Desktop/paste-here", 0777) == -1) {
        perror("Failed to create destination directory");
        fclose(source_file);
        return 1;
    }

    // Open the destination file for writing
    destination_file = fopen("home/kenneth/Desktop/paste-here/file-1.txt", "wb");
    if (destination_file == NULL) {
        perror("Failed to open destination file");
        fclose(source_file);
        return 1;
    }

    // Copy data from source to destination
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, source_file)) > 0) {
        if (fwrite(buffer, 1, bytes_read, destination_file) != bytes_read) {
            perror("Error writing to destination file");
            fclose(source_file);
            fclose(destination_file);
            return 1;
        }
    }

    // Close files
    fclose(source_file);
    fclose(destination_file);

    printf("File copied successfully\n");
}

char *replace_source_path(char *source_path, char *file_source_path, char *destination_path){
    /*
        Recieves the original folders source path, the orginal files source path and the destination folder path
        It transfors the original files path into the destination file path
        For example:
            source_path = /home/kenneth/Desktop/copy-this
            file_source_path = /home/kenneth/Desktop/copy-this/file-1.txt
            destination_path = /home/kenneth/Desktop/paste-here
        It returns /home/kenneth/Desktop/paste-here/file-1.txt which is the new path of the new file
    */
    size_t source_path_len = strlen(source_path);
    file_source_path += source_path_len;
    memmove(file_source_path + strlen(destination_path), file_source_path, strlen(file_source_path) + 1);

    memcpy(file_source_path, destination_path, strlen(destination_path));

    printf("%s\n",file_source_path);
}
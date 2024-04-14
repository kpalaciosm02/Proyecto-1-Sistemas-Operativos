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

int count_files_in_folder(const char *path, struct Queue *file_queue, struct Queue *new_path_queue);
int copy_file(char *source_path, char *destination_path);
char *replace_source_path(const char *source_path, const char *file_source_path, const char *destination_path);
int create_folders(const char *path);
char *remove_file_name(const char *path);
void create_all_folders();

int main(int argc, char *argv[]){
    char path_folder_1[MAX_PATH_LENGTH];
    char path_folder_2[MAX_PATH_LENGTH];

    struct Queue *path_queue = createQueue();
    struct Queue *new_path_queue = createQueue();

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

    int file_count = count_files_in_folder(path_folder_1, path_queue, new_path_queue);

    //printf("Total number of files in forlder (%s) is: %d\n",path_folder_1,file_count);
    printf("Content in queue: \n");
    printQueue(path_queue);
    printf("Content in new path queue: \n");
    printQueue(new_path_queue);
    //create_folders(remove_file_name(path_queue));
    //create_folders("/home/kenneth/Desktop/paste-here/Folder-1/Folder-2");
    return 0;
}

int count_files_in_folder(const char *path, struct Queue *file_queue, struct Queue *new_path_queue){
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

            count += count_files_in_folder(full_path, file_queue, new_path_queue);
        } else if (S_ISREG(statbuf.st_mode)) {
            enqueue(file_queue,full_path);
            char * full_path_tmp = malloc(strlen(full_path) + 1);
            strcpy(full_path_tmp,full_path);
            char * new_file_path = replace_source_path("/home/kenneth/Desktop/copy-this",full_path_tmp,"/home/kenneth/Desktop/paste-here");
            enqueue(new_path_queue,new_file_path);
            char * expected_folder_name = remove_file_name(new_file_path);
            create_folders(expected_folder_name);
            count++;
        }
    }

    closedir(dir);
    return count;
}

int copy_file(char *source_path, char *destination_path){
    //printf("Source path: %s\n",source_path);
    //printf("Destination path: %s\n",source_path);



    FILE *source_file, *destination_file;
    char buffer[BUFFER_SIZE];
    size_t bytes_read;

    // Open the source file for reading
    source_file = fopen(source_path, "rb");
    if (source_file == NULL) {
        //printf("Trying to open: %s\n",source_path);
        perror("Failed to open source file");
        return 1;
    }

    // Create the directory for the destination file if it doesn't exist
    if (mkdir(destination_path, 0777) == -1) {
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

char *replace_source_path(const char *source_path, const char *file_source_path, const char *destination_path){
    /*
        This function recieves the original folders source path, the orginal files source path and the destination folder path
        It transfors the original files path into the destination file path
        For example:
            source_path = /home/kenneth/Desktop/copy-this
            file_source_path = /home/kenneth/Desktop/copy-this/file-1.txt
            destination_path = /home/kenneth/Desktop/paste-here
        It returns /home/kenneth/Desktop/paste-here/file-1.txt which is the new path of the new file
    */
    size_t source_path_len = strlen(source_path);
    size_t file_source_path_len = strlen(file_source_path);
    size_t destination_path_len = strlen(destination_path);
    
    char *new_path = malloc(source_path_len + file_source_path_len - source_path_len + destination_path_len + 1);
    if (new_path == NULL) {
        perror("Failed to allocate memory");
        return NULL;
    }
    
    strcpy(new_path, destination_path);
    
    strcat(new_path, file_source_path + source_path_len);
    
    //printf("Path created: %s\n", new_path);
    return new_path;
}

int create_folders(const char *path){
    /*
        This function receives the path to a folder and creates all folders needed to have that folder
        For example:
            path = /home/kenneth/Desktop/paste-here/Folder-1/Folder-2
            If my computer only has folders created until /home/kenneth/Desktop the function will create
            the folder paste-here inside Desktop, and then Folder-1 inside paste-here and lastly Folder-2 inside Folder-1
        It returns 0 if everything went alright or -1 if there was an error
    */
    char *path_copy = strdup(path); 
    if (path_copy == NULL) {
        perror("Failed to duplicate path");
        return -1;
    }
    
    char *token = strtok(path_copy, "/");
    char dir_path[MAX_PATH_LENGTH] = "/";
    
    while (token != NULL) {
        strcat(dir_path, token);
        
        struct stat st;
        if (stat(dir_path, &st) != 0) {
            if (mkdir(dir_path, 0777) == -1) {
                perror("Failed to create directory");
                free(path_copy);
                return -1;
            } else {
                printf("Directory created: %s\n", dir_path);
            }
        }
        
        strcat(dir_path, "/");
        token = strtok(NULL, "/");
    }
    
    free(path_copy);
    return 0;
}

char *remove_file_name(const char *path){
    /*
        This function receives a file path and deletes the file name from the full path
        For example:
            path = /home/kenneth/Desktop/paste-here/Folder-1/file.txt
        This case would return /home/kenneth/Desktop/paste-here/Folder-1/
    */
    const char *last_slash = strrchr(path, '/');
    if (last_slash == NULL) {
        return strdup(path);
    }
    
    size_t new_length = last_slash - path;
    
    char *new_path = malloc(new_length + 1);
    if (new_path == NULL) {
        perror("Failed to allocate memory");
        return NULL;
    }
    
    strncpy(new_path, path, new_length);
    new_path[new_length] = '\0';
    
    return new_path;
}


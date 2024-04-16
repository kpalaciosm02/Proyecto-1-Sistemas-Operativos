#include <dirent.h>
#include <sys/stat.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <stdio.h>
#include <unistd.h>

#include "queue.h"

#define MAX_PATH_LENGTH 512
#define BUFFER_SIZE 4096
#define PROCESS_AMOUNT 3
#define SHM_KEY 1234
#define SEM_KEY 5678

int count_files_in_folder(const char *path, struct Queue *file_queue, struct Queue *new_path_queue);
void copy_file(const char *source_path, const char *destination_path);
char *replace_source_path(const char *source_path, const char *file_source_path, const char *destination_path);
int create_folders(const char *path);
char *remove_file_name(const char *path);
void call_copy_file(struct Queue *file_queue, struct Queue *new_path_queue);

int main(int argc, char *argv[]){
    char path_folder_1[MAX_PATH_LENGTH];
    char path_folder_2[MAX_PATH_LENGTH];

    struct Queue *path_queue = createQueue();
    struct Queue *new_path_queue = createQueue();

        int shmid, semid;
    struct SharedMemory {
        struct Queue *path_queue;
        struct Queue *new_path_queue;
    };
    struct SharedMemory *shared_memory;
    int pid;

    shmid = shmget(SHM_KEY, sizeof(struct SharedMemory), IPC_CREAT | 0666);
    if (shmid == -1) {
        perror("shmget");
        exit(1);
    }

    shared_memory = (struct SharedMemory *)shmat(shmid, NULL, 0);
    if (shared_memory == (void *)-1) {
        perror("shmat");
        exit(1);
    }

    shared_memory->path_queue = createQueue();
    shared_memory->new_path_queue = createQueue();

    semid = semget(SEM_KEY, 1, IPC_CREAT | 0666);
    if (semid == -1) {
        perror("semget");
        exit(1);
    }

    semctl(semid, 0, SETVAL, 1);

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

    int file_count = count_files_in_folder(path_folder_1, shared_memory->path_queue, shared_memory->new_path_queue);

    printf("Content in queue: \n");
    printQueue(path_queue);
    printf("Content in new path queue: \n");
    printQueue(new_path_queue);

    for (int i = 0; i < PROCESS_AMOUNT; i++) {
        pid = fork();
        if (pid == -1) {
            perror("fork");
            exit(1);
        } else if (pid == 0) {
            while (!isEmpty(shared_memory->path_queue) && !isEmpty(shared_memory->new_path_queue)) {
                struct sembuf sem_op;
                sem_op.sem_num = 0;
                sem_op.sem_op = -1;
                sem_op.sem_flg = 0;
                semop(semid, &sem_op, 1);

                char *file = dequeue(shared_memory->path_queue);
                char *new_file = dequeue(shared_memory->new_path_queue);

                sem_op.sem_op = 1;
                semop(semid, &sem_op, 1);

                if (file != NULL && new_file != NULL) {
                    printf("Process %d dequeued file: %s, new path: %s\n", getpid(), file, new_file);
                    copy_file(file,new_file);
                }
            }
            exit(0);
        } else {
            wait(NULL);
        }
    }

    for (int i = 0; i < PROCESS_AMOUNT; i++) {
        wait(NULL);
    }

    shmdt(shared_memory);

    shmctl(shmid, IPC_RMID, NULL);
    semctl(semid, 0, IPC_RMID);
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

void copy_file(const char *source_path, const char *destination_path) {
    FILE *src_file, *dest_file;
    char buffer[BUFFER_SIZE];
    size_t bytes_read;

    // Open the source file for reading
    src_file = fopen(source_path, "rb");
    if (src_file == NULL) {
        perror("Error opening source file");
        return;
    }

    // Open the destination file for writing
    dest_file = fopen(destination_path, "wb");
    if (dest_file == NULL) {
        perror("Error opening destination file");
        fclose(src_file);
        return;
    }

    // Copy contents from source file to destination file
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), src_file)) > 0) {
        fwrite(buffer, 1, bytes_read, dest_file);
    }

    // Close files
    fclose(src_file);
    fclose(dest_file);

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

void call_copy_file(struct Queue *file_queue, struct Queue *new_path_queue){
    int pid = (int) getpid();
    char *file = dequeue(file_queue);
    char *new_file = dequeue(new_path_queue);
    if (!file && !new_file){
        printf("No more files to copy.");
        return;
    } else if (!file | !new_file) {
        printf("Error, files and new paths do not match.");
    } else {
        printf("Process: %d copying file from %s to %s.\n", pid, file, new_file);
        copy_file(file,new_file);
        return;
    }
}

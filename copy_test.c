#include <dirent.h>
#include <sys/stat.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <stdio.h>
#include <unistd.h>

#include "queue.h"

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>

#define MAX_PATH_LENGTH 512
#define BUFFER_SIZE 4096
#define PROCESS_AMOUNT 3
#define SHM_SIZE sizeof(struct Queue)
#define SEM_MUTEX_KEY 1234
#define SEM_EMPTY_KEY 5678
#define SEM_FULL_KEY 9101

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

int count_files_in_folder(const char *path, struct Queue *file_queue, struct Queue *new_path_queue);
void copy_file(const char *source_path, const char *destination_path);
char *replace_source_path(const char *source_path, const char *file_source_path, const char *destination_path);
int create_folders(const char *path);
char *remove_file_name(const char *path);
void call_copy_file(struct Queue *file_queue, struct Queue *new_path_queue);

int main(int argc, char *argv[]){
    char path_folder_1[MAX_PATH_LENGTH];
    char path_folder_2[MAX_PATH_LENGTH];

    struct Queue * path_queue = createQueue();
    struct Queue * new_path_queue = createQueue();

    int shmid, sem_mutex, sem_empty, sem_full;
    key_t key = ftok(".", 'q');
    pid_t child_pids[PROCESS_AMOUNT];

    if ((shmid = shmget(key, SHM_SIZE, IPC_CREAT | 0666)) < 0) {
        perror("shmget");
        exit(1);
    }

    if ((path_queue = shmat(shmid, NULL, 0)) == (struct Queue *) -1) {
        perror("shmat");
        exit(1);
    }

    if ((sem_mutex = semget(SEM_MUTEX_KEY, 1, IPC_CREAT | IPC_EXCL | 0666)) < 0) {
        if ((sem_mutex = semget(SEM_MUTEX_KEY, 1, 0)) < 0) {
            perror("semget(mutex)");
            exit(1);
        }
    } else {
        union semun arg;
        arg.val = 1;
        if (semctl(sem_mutex, 0, SETVAL, arg) < 0) {
            perror("semctl(mutex)");
            exit(1);
        }
    }

    strncpy(path_folder_1, argv[1], MAX_PATH_LENGTH - 1);
    strncpy(path_folder_2, argv[2], MAX_PATH_LENGTH - 1);

    path_folder_1[MAX_PATH_LENGTH - 1] = '\0';
    path_folder_2[MAX_PATH_LENGTH - 1] = '\0';

    printf("First folder path is: %s\n", path_folder_1);
    printf("Second folder path is: %s\n", path_folder_2);

    int file_count = count_files_in_folder(path_folder_1, path_queue, new_path_queue);

    for (int i = 0; i < PROCESS_AMOUNT; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            exit(1);
        } else if (pid == 0) {
            while (1) {
                struct sembuf sem_wait_empty = {0, -1, 0};
                semop(sem_empty, &sem_wait_empty, 1);

                struct sembuf sem_lock_mutex = {0, -1, 0};
                semop(sem_mutex, &sem_lock_mutex, 1);

                char *path = dequeue(path_queue);
                char *new_path = dequeue(new_path_queue);

                struct sembuf sem_release_mutex = {0, 1, 0};
                semop(sem_mutex, &sem_release_mutex, 1);

                struct sembuf sem_signal_full = {0, 1, 0};
                semop(sem_full, &sem_signal_full, 1);

                if (path != NULL) {
                    printf("Child %d dequeued: %s\n", getpid(), path);
                    copy_file(path,new_path);
                    free(path);
                    free(new_path);
                } else {
                    wait(NULL);
                    exit(0);
                }
            }
        } else {
            child_pids[i] = pid;
        }
    }

    for (int i = 0; i < PROCESS_AMOUNT; i++) {
        int status;
        waitpid(child_pids[i], &status, 0);
        printf("Child process %d finished with status %d\n", child_pids[i], status);
    }

    for (int i = 1; i < argc; i++) {
        struct sembuf sem_op_wait_full = {0, -1, 0};
        semop(sem_full, &sem_op_wait_full, 1);

        struct sembuf sem_op_lock = {0, -1, 0};
        semop(sem_mutex, &sem_op_lock, 1);

        enqueue(path_queue, argv[i]);

        struct sembuf sem_op_unlock = {0, 1, 0};
        semop(sem_mutex, &sem_op_unlock, 1);

        struct sembuf sem_op_signal_empty = {0, 1, 0};
        semop(sem_empty, &sem_op_signal_empty, 1);
    }

    if (shmdt(path_queue) == -1) {
        perror("shmdt");
        exit(1);
    }

    if (shmctl(shmid, IPC_RMID, NULL) == -1) {
        perror("shmctl");
        exit(1);
    }

    semctl(sem_mutex, 0, IPC_RMID);

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
    /*
        Recieves two file paths and copies source path into destination path in a binary level
        Uses BUFFER_SIZE to read chunks of the source file and copy them into the destination file
        until there is no more data to copy
        Does not return anything.
    */
    FILE *src_file, *dest_file;
    char buffer[BUFFER_SIZE];
    size_t bytes_read;
    printf("Copying %s to %s in process: %d\n", source_path, destination_path, getpid());

    src_file = fopen(source_path, "rb");
    if (src_file == NULL) {
        perror("Error opening source file");
        return;
    }

    dest_file = fopen(destination_path, "wb");
    if (dest_file == NULL) {
        perror("Error opening destination file");
        fclose(src_file);
        return;
    }

    while ((bytes_read = fread(buffer, 1, sizeof(buffer), src_file)) > 0) {
        fwrite(buffer, 1, bytes_read, dest_file);
    }

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
    /*
        Recieves two file paths queues and copies files from file_queue to new_path_queue
        Used merely for testing.
    */
    int pid = (int) getpid();
    char *file = dequeue(file_queue);
    char *new_file = dequeue(new_path_queue);
    if (!file && !new_file){
        printf("No more files to copy.");
        return;
    } else if (!file | !new_file) {
        printf("Error, files and new paths do not match.");
        return;
    } else {
        printf("Process: %d copying file from %s to %s.\n", pid, file, new_file);
        copy_file(file,new_file);
        return;
    }
}

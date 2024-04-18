#include <dirent.h>
#include <sys/stat.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <stdio.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/msg.h>
#include <string.h>
#include <signal.h>

#include "queue.h"

#define MAX_PATH_LENGTH 512
#define BUFFER_SIZE 4096
#define PROCESS_AMOUNT 3

#define MSGSZ 128
#define SHARED_MEM_SIZE sizeof(struct SharedMemory)

int count_files_in_folder(const char *path, struct Queue *file_queue, struct Queue *new_path_queue);
void copy_file(const char *source_path, const char *destination_path);
char *replace_source_path(const char *source_path, const char *file_source_path, const char *destination_path);
int create_folders(const char *path);
char *remove_file_name(const char *path);
void send_child_copy_file(char *source_path, char *destination_path, int type, int msqid);
void receive_path_from_parent(char *source_path, char *destination_path, int child_id, int msqid);
void send_path_to_child(const char *source_path, const char *destination_path, int msqid);

struct paths {
    long type;
    char source_path[MAX_PATH_LENGTH];
    char destination_path[MAX_PATH_LENGTH];
};

int main(int argc, char *argv[]){
    char path_folder_1[MAX_PATH_LENGTH];
    char path_folder_2[MAX_PATH_LENGTH];

    struct Queue *path_queue = createQueue();
    struct Queue *new_path_queue = createQueue();

    strncpy(path_folder_1, argv[1], MAX_PATH_LENGTH - 1);
    strncpy(path_folder_2, argv[2], MAX_PATH_LENGTH - 1);

    path_folder_1[MAX_PATH_LENGTH - 1] = '\0';
    path_folder_2[MAX_PATH_LENGTH - 1] = '\0';

    printf("First folder path is: %s\n", path_folder_1);
    printf("Second folder path is: %s\n", path_folder_2);

    int file_count = count_files_in_folder(path_folder_1, path_queue, new_path_queue);

    int status;
    key_t msqkey = 999;
    int msqid = msgget(msqkey, IPC_CREAT | S_IRUSR | S_IWUSR);
    key_t conkey = 888;
    int conid = msgget(conkey, IPC_CREAT | S_IRUSR | S_IWUSR);
    int pids[PROCESS_AMOUNT];

    struct paths path_msg;

    int terminated_amount = 0;

    for (int i = 0; i < PROCESS_AMOUNT; i++){
        pid_t pid = fork();
        pids[i] = pid;
        if (pid < 0) {
            perror("Fork failed");
            exit(EXIT_FAILURE);
        } else if (pid == 0) {
            int confirmation;
            while (1){
                
                char source_path[MAX_PATH_LENGTH];
                char destination_path[MAX_PATH_LENGTH];

                receive_path_from_parent(source_path, destination_path, i + 1, msqid);

                if (strlen(source_path) == 0 && strlen(destination_path) == 0) {
                    if (msgrcv(conid,&confirmation,sizeof(confirmation),0,0) == -1){
                        perror("Error receiving confirmation message");
                        exit(EXIT_FAILURE);
                    }
                    break;
                }
                copy_file(source_path,destination_path);
                printf("Child process with id: %d received source path: %s and destination path: %s\n", getpid(), source_path, destination_path);
            }
            if (confirmation){
                exit(EXIT_SUCCESS);
            }
            
        }
    }

    while(!isEmpty(path_queue)){
        for (int i = 0; i < PROCESS_AMOUNT; i++){
            char *source_path = dequeue(path_queue);
            char *destination_path = dequeue(new_path_queue);
            
            if (source_path != NULL){
                send_child_copy_file(source_path, destination_path, i+1, msqid);
            }
            free(source_path);
            free(destination_path);
        }
    }
    
    for (int i = 0; i < PROCESS_AMOUNT; i++){
        send_child_copy_file("","",i+1,msqid);
        int confirmation = 1;
        if (msgsnd(conid, &confirmation, sizeof(int), 0) == -1) {
            perror("Confirmation message failed");
            exit(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < PROCESS_AMOUNT; i++) {
        wait(&status);
    }

    msgctl(msqid, IPC_RMID, NULL);

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
        Recieves two files paths, one is the source (file to be copied)
        and the other one is the destination (where the data will be pasted)
        Uses a 4096 bytes buffer to read chunks from the file
        Does not return anything but prints a message when the job is done
    */
    FILE *src_file, *dest_file;
    char buffer[BUFFER_SIZE];
    size_t bytes_read;

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

void send_child_copy_file(char *source_path, char *destination_path, int type, int msqid){
    /*
        This function receives the source path, the destination path, the message type and the message queue id
        It is only runned in the main (parent) process.
        It sends the paths inside a paths struct to the child that consumes that data and uses it.
    */
    struct paths path_msg;
    path_msg.type = type;
    strcpy(path_msg.source_path, source_path);
    strcpy(path_msg.destination_path, destination_path);

    if (source_path != ""){
        printf("Source path send from parent process: %s\n",path_msg.source_path);
        printf("Destination path send from parent process: %s\n\n",path_msg.source_path);
    }
    

    if (msgsnd(msqid, (void *)&path_msg, sizeof(path_msg), IPC_NOWAIT) == -1) {
        perror("Send child copy file failed");
        exit(EXIT_FAILURE);
    }
}

void send_path_to_child(const char *source_path, const char *destination_path, int msqid) {
    /*
        This function receives the source path, the destination path, the message type and the message queue id
        It is only runned in the main (parent) process.
        It sends the paths inside a paths struct to the child that consumes that data and uses it.
    */
    struct paths path_msg;

    strncpy(path_msg.source_path, source_path, sizeof(path_msg.source_path));
    strncpy(path_msg.destination_path, destination_path, sizeof(path_msg.destination_path));

    if (msgsnd(msqid, &path_msg, sizeof(path_msg), 0) == -1) {
        perror("Path message sending failed");
        exit(EXIT_FAILURE);
    }
}

void receive_path_from_parent(char *source_path, char *destination_path, int child_id, int msqid) {
    /*
        This function receives the source path, the destination path, the child_id and the message queue id.
        It is only runned in the child processes.
        It receives the paths send by the parent process.
    */
    struct paths path_msg;

    if (msgrcv(msqid, &path_msg, sizeof(path_msg), child_id, 0) == -1) {
        perror("Failed to receive path message");
        exit(EXIT_FAILURE);
    }

    strncpy(source_path, path_msg.source_path, sizeof(path_msg.source_path));
    strncpy(destination_path, path_msg.destination_path, sizeof(path_msg.destination_path));
}

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <ctype.h>
#include <time.h>
#include <dirent.h>

typedef struct {
    uint32_t fileSize;
    uint32_t reserved;
    uint32_t dataOffset;
    uint32_t headerSize;
    uint32_t width;
    uint32_t height;
} BMPHeader;

void showErrorAndExit(const char *errorMessage) {
    perror(errorMessage);
    exit(5);
}

int isBmp(const char *file_name) {
    struct stat file_stats;
    if (lstat(file_name, &file_stats) == -1) return -1;

    if (!S_ISREG(file_stats.st_mode)) return 0;

    int bmp_file = open(file_name, O_RDONLY);
    if (bmp_file == -1) return -1;

    char signature[2];
    if (read(bmp_file, signature, 2) != 2) {
        close(bmp_file);
        return 0;
    }

    if (signature[0] == 'B' && signature[1] == 'M') {
        close(bmp_file);
        return 1;
    }

    close(bmp_file);
    return 0;
}

void readBMPHeader(int fIn, BMPHeader *bmpHeader) {
    char signature[2];
    if (read(fIn, signature, 2) != 2 || signature[0] != 'B' || signature[1] != 'M') {
        showErrorAndExit("Eroare: Fisierul nu are un antet BMP valid");
    }

    if (read(fIn, bmpHeader, sizeof(BMPHeader)) != sizeof(*bmpHeader)) {
        showErrorAndExit("Eroare: Fisierul nu a putut fi citit");
    }
}
int isOrdinaryFileWithoutBMPExtension(const char *file_name) {
    struct stat file_stats;
    if (lstat(file_name, &file_stats) == -1) return -1;

    if (!S_ISREG(file_stats.st_mode)) return 0;  

    const char *extension = strrchr(file_name, '.');
    if (extension != NULL && strcmp(extension, ".bmp") == 0) {
        return 0;  
    }

    return 1;  // Este un fișier obișnuit fără extensia BMP
}

int isSymbolicLink(const char *file_name) {
    struct stat file_stats;
    if (lstat(file_name, &file_stats) == -1) return -1;

    return S_ISLNK(file_stats.st_mode);
}

int isFolder(const char *file_name) {
    struct stat file_stats;
    if (lstat(file_name, &file_stats) == -1) return -1;

    return S_ISDIR(file_stats.st_mode);
}

void processDirectory(const char *dirPath, int fOut) {
    DIR *dir;
    struct dirent *entry;

    if ((dir = opendir(dirPath)) == NULL) {
        showErrorAndExit("Nu s-a reusit deschiderea directorului");
    }

    char buffer[512];

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue; // Ignoră intrările "." și ".."
        }

        char filePath[512];
        sprintf(filePath, "%s/%s", dirPath, entry->d_name);

        if (isSymbolicLink(filePath)) {
            // Este o legatura simbolica
            struct stat linkStat;
            if (lstat(filePath, &linkStat) == -1) {
                perror("Eroare la obtinerea informatiilor despre legatura simbolica");
                continue;
            }

            char targetPath[512];
            ssize_t targetSize = readlink(filePath, targetPath, sizeof(targetPath) - 1);
            if (targetSize == -1) {
                perror("Eroare la citirea legaturii simbolice");
                continue;
            }
            targetPath[targetSize] = '\0';

            sprintf(buffer, "\n nume legatura: %s\n", entry->d_name);
            write(fOut, buffer, strlen(buffer));

            sprintf(buffer, "dimensiune legatura: %ld\n", (long)linkStat.st_size);
            write(fOut, buffer, strlen(buffer));

            sprintf(buffer, "dimensiune fisier target: %ld\n", (long)linkStat.st_size);
            write(fOut, buffer, strlen(buffer));

            sprintf(buffer, "drepturi de acces user legatura: RWX\n");
            write(fOut, buffer, strlen(buffer));

            sprintf(buffer, "drepturi de acces grup legatura: R--\n");
            write(fOut, buffer, strlen(buffer));

            sprintf(buffer, "drepturi de acces altii legatura: ---\n");
            write(fOut, buffer, strlen(buffer));
        } else if (isFolder(filePath)) {
            // Este un director
            struct stat dirStat;
            if (lstat(filePath, &dirStat) == -1) {
                perror("Eroare la obtinerea informatiilor despre director");
                continue;
            }

            sprintf(buffer, "\n nume director: %s\n", entry->d_name);
            write(fOut, buffer, strlen(buffer));

            sprintf(buffer, "identificatorul utilizatorului: %ld\n", (long)dirStat.st_uid);
            write(fOut, buffer, strlen(buffer));

            sprintf(buffer, "drepturi de acces user: RWX\n");
            write(fOut, buffer, strlen(buffer));

            sprintf(buffer, "drepturi de acces grup: R--\n");
            write(fOut, buffer, strlen(buffer));

            sprintf(buffer, "drepturi de acces altii: ---\n");
            write(fOut, buffer, strlen(buffer));
        } else if (isBmp(filePath)) {
            // Este un fisier BMP
            BMPHeader bmpHeader;
            int fIn;

            if ((fIn = open(filePath, O_RDONLY)) < 0) {
                perror("Nu s-a reusit deschiderea fisierului");
                continue;
            }

            readBMPHeader(fIn, &bmpHeader);

            char timeBuffer[30];
            struct stat fileStat;
            if (lstat(filePath, &fileStat) == -1) {
                perror("Eroare la obtinerea informatiilor despre fisier");
                continue;
            }

            strftime(timeBuffer, sizeof(timeBuffer), "%d.%m.%Y", localtime(&fileStat.st_mtime));

            sprintf(buffer, "\n nume fisier: %s\n", entry->d_name);
            write(fOut, buffer, strlen(buffer));

            sprintf(buffer, "inaltime: %u\n", bmpHeader.height);
            write(fOut, buffer, strlen(buffer));

            sprintf(buffer, "lungime: %u\n", bmpHeader.width);
            write(fOut, buffer, strlen(buffer));

            sprintf(buffer, "dimensiune: %ld bytes\n", (long)fileStat.st_size);
            write(fOut, buffer, strlen(buffer));

            sprintf(buffer, "identificatorul utilizatorului: %ld\n", (long)fileStat.st_uid);
            write(fOut, buffer, strlen(buffer));

            sprintf(buffer, "timpul ultimei modificari: %s\n", timeBuffer);
            write(fOut, buffer, strlen(buffer));

            sprintf(buffer, "contorul de legaturi: %ld\n", (long)fileStat.st_nlink);
            write(fOut, buffer, strlen(buffer));

            sprintf(buffer, "drepturi de acces user: %c%c%c\n",
                    (fileStat.st_mode & S_IRUSR) ? 'R' : '-',
                    (fileStat.st_mode & S_IWUSR) ? 'W' : '-',
                    (fileStat.st_mode & S_IXUSR) ? 'X' : '-');
            write(fOut, buffer, strlen(buffer));

            sprintf(buffer, "drepturi de acces grup: %c%c%c\n",
                    (fileStat.st_mode & S_IRGRP) ? 'R' : '-',
                    (fileStat.st_mode & S_IWGRP) ? 'W' : '-',
                    (fileStat.st_mode & S_IXGRP) ? 'X' : '-');
            write(fOut, buffer, strlen(buffer));

            sprintf(buffer, "drepturi de acces altii: %c%c%c\n",
                    (fileStat.st_mode & S_IROTH) ? 'R' : '-',
                    (fileStat.st_mode & S_IWOTH) ? 'W' : '-',
                    (fileStat.st_mode & S_IXOTH) ? 'X' : '-');
            write(fOut, buffer, strlen(buffer));

            close(fIn);
        }else if (isOrdinaryFileWithoutBMPExtension(filePath)) {
            struct stat fileStat;
    if (lstat(filePath, &fileStat) == -1) {
        perror("Eroare la obtinerea informatiilor despre fisier");
        continue;
    }

    char timeBuffer[30];
    strftime(timeBuffer, sizeof(timeBuffer), "%d.%m.%Y", localtime(&fileStat.st_mtime));

    sprintf(buffer, "\n nume fisier: %s\n", entry->d_name);
    write(fOut, buffer, strlen(buffer));

    sprintf(buffer, "dimensiune: %ld bytes\n", (long)fileStat.st_size);
    write(fOut, buffer, strlen(buffer));

    sprintf(buffer, "identificatorul utilizatorului: %ld\n", (long)fileStat.st_uid);
    write(fOut, buffer, strlen(buffer));

    sprintf(buffer, "timpul ultimei modificari: %s\n", timeBuffer);
    write(fOut, buffer, strlen(buffer));

    sprintf(buffer, "contorul de legaturi: %ld\n", (long)fileStat.st_nlink);
    write(fOut, buffer, strlen(buffer));

    sprintf(buffer, "drepturi de acces user: %c%c%c\n",
            (fileStat.st_mode & S_IRUSR) ? 'R' : '-',
            (fileStat.st_mode & S_IWUSR) ? 'W' : '-',
            (fileStat.st_mode & S_IXUSR) ? 'X' : '-');
    write(fOut, buffer, strlen(buffer));

    sprintf(buffer, "drepturi de acces grup: %c%c%c\n",
            (fileStat.st_mode & S_IRGRP) ? 'R' : '-',
            (fileStat.st_mode & S_IWGRP) ? 'W' : '-',
            (fileStat.st_mode & S_IXGRP) ? 'X' : '-');
    write(fOut, buffer, strlen(buffer));

    sprintf(buffer, "drepturi de acces altii: %c%c%c\n",
            (fileStat.st_mode & S_IROTH) ? 'R' : '-',
            (fileStat.st_mode & S_IWOTH) ? 'W' : '-',
            (fileStat.st_mode & S_IXOTH) ? 'X' : '-');
    write(fOut, buffer, strlen(buffer));
        }
    }

    closedir(dir);
}



int main(int argc, char *argv[]) {
    if (argc != 2) {
        showErrorAndExit("Usage: ./proiect <imagine.bmp>");
    }

    const char *dirPath = argv[1];
    int fOut;

    if ((fOut = open("statistica.txt", O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR)) < 0) {
        showErrorAndExit("Nu s-a reusit crearea fisierului statistica.txt");
    }

    processDirectory(dirPath, fOut);

    close(fOut);
    return 0;
}

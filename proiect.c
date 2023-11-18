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
#include <sys/wait.h>

typedef struct {
    uint32_t fileSize;
    uint32_t reserved;
    uint32_t dataOffset;
    uint32_t headerSize;
    uint32_t width;
    uint32_t height;
} BMPHeader;

void showErrorAndExit(const char *errorMessage) 
{
    perror(errorMessage);
    exit(5);
}

//este un fisier cu extensia .bmp
int isBmp(const char *file_name) 
{
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

void readBMPHeader(int fIn, BMPHeader *bmpHeader) 
{
    char signature[2];
    if (read(fIn, signature, 2) != 2 || signature[0] != 'B' || signature[1] != 'M') {
        showErrorAndExit("Eroare: Fisierul nu are un antet BMP valid");
    }

    if (read(fIn, bmpHeader, sizeof(BMPHeader)) != sizeof(*bmpHeader)) {
        showErrorAndExit("Eroare: Fisierul nu a putut fi citit");
    }
}

int isOrdinaryFileWithoutBMPExtension(const char *file_name) 
{
    struct stat file_stats;
    if (lstat(file_name, &file_stats) == -1) return -1;

    if (!S_ISREG(file_stats.st_mode)) return 0;  

    const char *extension = strrchr(file_name, '.');
    if (extension != NULL && strcmp(extension, ".bmp") == 0) {
        return 0;  
    }

    return 1;  // Este un fișier obișnuit fără extensia BMP
}

//este legatura simbolica
int isSymbolicLink(const char *file_name) 
{
    struct stat file_stats;
    if (lstat(file_name, &file_stats) == -1) return -1;

    return S_ISLNK(file_stats.st_mode);
}

//este folder
int isFolder(const char *file_name) 
{
    struct stat file_stats;
    if (lstat(file_name, &file_stats) == -1) return -1;

    return S_ISDIR(file_stats.st_mode);
}


void convertToGrayscale(const char *filePath, const char *outputDir) 
{
    int fIn, fOut;
    if ((fIn = open(filePath, O_RDONLY)) < 0) 
    {
        showErrorAndExit("Nu s-a reusit deschiderea fisierului");
    }

    // creez o copie a fisierului .bmp in folderul de outpu
    char copyPath[512];
    sprintf(copyPath, "%s/%s_copy.bmp", outputDir, strrchr(filePath, '/') + 1);
    if ((fOut = open(copyPath, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR)) < 0) 
    {
        perror("Nu s-a reusit crearea fisierului de copie");
        close(fIn);
        exit(EXIT_FAILURE);
    }

    char buffer[4096];
    ssize_t bytesRead;
    while ((bytesRead = read(fIn, buffer, sizeof(buffer))) > 0) 
    {
        if (write(fOut, buffer, bytesRead) != bytesRead) 
        {
            perror("Eroare la scrierea in fisierul de copie");
            close(fIn);
            close(fOut);
            exit(EXIT_FAILURE);
        }
    }

    close(fIn);
    close(fOut);

    // deschid copia pt a o modifica
    if ((fOut = open(copyPath, O_RDWR)) < 0) 
    {
        showErrorAndExit("Nu s-a reusit deschiderea fisierului de copie");
    }

    BMPHeader bmpHeader;
    readBMPHeader(fOut, &bmpHeader);
    size_t headerSize = bmpHeader.headerSize;

    lseek(fOut, headerSize, SEEK_SET); // Skip header

    uint8_t pixel[3];
    ssize_t bytesReadOut;
    while ((bytesReadOut = read(fOut, pixel, sizeof(pixel))) == sizeof(pixel)) 
    {
        uint8_t grayValue = (uint8_t)(0.299 * pixel[2] + 0.587 * pixel[1] + 0.114 * pixel[0]);
        lseek(fOut, -3, SEEK_CUR);
        write(fOut, &grayValue, sizeof(grayValue));
        write(fOut, &grayValue, sizeof(grayValue));
        write(fOut, &grayValue, sizeof(grayValue));
    }

    close(fOut);
}

void processDirectory(const char *dirPath, const char *outputDir, int fOut) 
{
    DIR *dir;
    struct dirent *entry;

    if ((dir = opendir(dirPath)) == NULL) 
    {
        showErrorAndExit("Nu s-a reusit deschiderea directorului");
    }

    char buffer[512];

    while ((entry = readdir(dir)) != NULL) 
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) 
        {
            continue; // ignor "." and ".."
        }

        char filePath[512];
        sprintf(filePath, "%s/%s", dirPath, entry->d_name);

        pid_t pid = fork();
        if (pid == -1) 
        {
            perror("Eroare la fork");
            exit(EXIT_FAILURE);
        }

        if (pid == 0) 
        { // procesare fiu
            char outputFileName[512];
            sprintf(outputFileName, "%s/%s_statistica.txt", outputDir, entry->d_name);

            int childFOut;
            if ((childFOut = open(outputFileName, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR)) < 0) 
            {
                showErrorAndExit("Nu s-a reusit deschiderea fisierului de statistica");
            }

            if (isSymbolicLink(filePath)) 
            {
                struct stat linkStat;
                if (lstat(filePath, &linkStat) == -1) 
                {
                    showErrorAndExit("Eroare la obtinerea informatiilor despre legatura simbolica");
                }

            char targetPath[512];
            ssize_t targetSize = readlink(filePath, targetPath, sizeof(targetPath) - 1);
            if (targetSize == -1) {
                showErrorAndExit("Eroare la citirea legaturii simbolice");
            }
            targetPath[targetSize] = '\0';

            sprintf(buffer, "\n nume legatura: %s\n", entry->d_name);
            write(childFOut, buffer, strlen(buffer));

            sprintf(buffer, "dimensiune legatura: %ld\n", (long)linkStat.st_size);
            write(childFOut, buffer, strlen(buffer));

            sprintf(buffer, "dimensiune fisier target: %ld\n", (long)linkStat.st_size);
            write(childFOut, buffer, strlen(buffer));

            sprintf(buffer, "drepturi de acces user legatura: RWX\n");
            write(childFOut, buffer, strlen(buffer));

            sprintf(buffer, "drepturi de acces grup legatura: R--\n");
            write(childFOut, buffer, strlen(buffer));

            sprintf(buffer, "drepturi de acces altii legatura: ---\n");
            write(childFOut, buffer, strlen(buffer));



            } 
            else if (isFolder(filePath)) 
            {
                struct stat dirStat;
            if (lstat(filePath, &dirStat) == -1) 
            {
                showErrorAndExit("Eroare la obtinerea informatiilor despre director");
            }

            sprintf(buffer, "\n nume director: %s\n", entry->d_name);
            write(childFOut, buffer, strlen(buffer));

            sprintf(buffer, "identificatorul utilizatorului: %ld\n", (long)dirStat.st_uid);
            write(childFOut, buffer, strlen(buffer));

            sprintf(buffer, "drepturi de acces user: %c%c%c\n",
                    (dirStat.st_mode & S_IRUSR) ? 'R' : '-',
                    (dirStat.st_mode & S_IWUSR) ? 'W' : '-',
                    (dirStat.st_mode & S_IXUSR) ? 'X' : '-');
            write(childFOut, buffer, strlen(buffer));

            sprintf(buffer, "drepturi de acces grup: %c%c%c\n",
                    (dirStat.st_mode & S_IRGRP) ? 'R' : '-',
                    (dirStat.st_mode & S_IWGRP) ? 'W' : '-',
                    (dirStat.st_mode & S_IXGRP) ? 'X' : '-');
            write(childFOut, buffer, strlen(buffer));

            sprintf(buffer, "drepturi de acces altii: %c%c%c\n",
                    (dirStat.st_mode & S_IROTH) ? 'R' : '-',
                    (dirStat.st_mode & S_IWOTH) ? 'W' : '-',
                    (dirStat.st_mode & S_IXOTH) ? 'X' : '-');
            write(childFOut, buffer, strlen(buffer));


            }
            else if (isBmp(filePath)) 
            {
            pid_t pidConvert = fork();
            if (pidConvert == -1) 
            {
                showErrorAndExit("Eroare la fork pentru conversia la tonuri de gri");
            }

            if (pidConvert == 0) 
            { 
                convertToGrayscale(filePath, outputDir);
                exit(EXIT_SUCCESS);
            } else { 
                int statusConvert;
                waitpid(pidConvert, &statusConvert, 0);
                printf("S-a incheiat procesul de conversie cu pid-ul %d si codul %d\n", pidConvert, statusConvert);
            }
            
                
            BMPHeader bmpHeader;
            int fIn;

            if ((fIn = open(filePath, O_RDONLY)) < 0) 
            {
                showErrorAndExit("Nu s-a reusit deschiderea fisierului");
            }

            readBMPHeader(fIn, &bmpHeader);

            char timeBuffer[30];
            struct stat fileStat;
            if (lstat(filePath, &fileStat) == -1)
            {
                showErrorAndExit("Eroare la obtinerea informatiilor despre fisier");
            }

            strftime(timeBuffer, sizeof(timeBuffer), "%d.%m.%Y", localtime(&fileStat.st_mtime));

            sprintf(buffer, "\n nume fisier: %s\n", entry->d_name);
            write(childFOut, buffer, strlen(buffer));

            sprintf(buffer, "inaltime: %u\n", bmpHeader.height);
            write(childFOut, buffer, strlen(buffer));

            sprintf(buffer, "lungime: %u\n", bmpHeader.width);
            write(childFOut, buffer, strlen(buffer));

            sprintf(buffer, "dimensiune: %ld bytes\n", (long)fileStat.st_size);
            write(childFOut, buffer, strlen(buffer));

            sprintf(buffer, "identificatorul utilizatorului: %ld\n", (long)fileStat.st_uid);
            write(childFOut, buffer, strlen(buffer));

            sprintf(buffer, "timpul ultimei modificari: %s\n", timeBuffer);
            write(childFOut, buffer, strlen(buffer));

            sprintf(buffer, "contorul de legaturi: %ld\n", (long)fileStat.st_nlink);
            write(childFOut, buffer, strlen(buffer));

            sprintf(buffer, "drepturi de acces user: %c%c%c\n",
                    (fileStat.st_mode & S_IRUSR) ? 'R' : '-',
                    (fileStat.st_mode & S_IWUSR) ? 'W' : '-',
                    (fileStat.st_mode & S_IXUSR) ? 'X' : '-');
            write(childFOut, buffer, strlen(buffer));

            sprintf(buffer, "drepturi de acces grup: %c%c%c\n",
                    (fileStat.st_mode & S_IRGRP) ? 'R' : '-',
                    (fileStat.st_mode & S_IWGRP) ? 'W' : '-',
                    (fileStat.st_mode & S_IXGRP) ? 'X' : '-');
            write(childFOut, buffer, strlen(buffer));

            sprintf(buffer, "drepturi de acces altii: %c%c%c\n",
                    (fileStat.st_mode & S_IROTH) ? 'R' : '-',
                    (fileStat.st_mode & S_IWOTH) ? 'W' : '-',
                    (fileStat.st_mode & S_IXOTH) ? 'X' : '-');
            write(childFOut, buffer, strlen(buffer));

            close(fIn);

            }
            else if (isOrdinaryFileWithoutBMPExtension(filePath)) 
            {
            struct stat fileStat;
            if (lstat(filePath, &fileStat) == -1) 
            {
                showErrorAndExit("Eroare la obtinerea informatiilor despre fisier");
            }

            char timeBuffer[30];
            strftime(timeBuffer, sizeof(timeBuffer), "%d.%m.%Y", localtime(&fileStat.st_mtime));

            sprintf(buffer, "\n nume fisier: %s\n", entry->d_name);
            write(childFOut, buffer, strlen(buffer));

            sprintf(buffer, "dimensiune: %ld bytes\n", (long)fileStat.st_size);
            write(childFOut, buffer, strlen(buffer));

            sprintf(buffer, "identificatorul utilizatorului: %ld\n", (long)fileStat.st_uid);
            write(childFOut, buffer, strlen(buffer));

            sprintf(buffer, "timpul ultimei modificari: %s\n", timeBuffer);
            write(childFOut, buffer, strlen(buffer));

            sprintf(buffer, "contorul de legaturi: %ld\n", (long)fileStat.st_nlink);
            write(childFOut, buffer, strlen(buffer));

            sprintf(buffer, "drepturi de acces user: %c%c%c\n",
                    (fileStat.st_mode & S_IRUSR) ? 'R' : '-',
                    (fileStat.st_mode & S_IWUSR) ? 'W' : '-',
                    (fileStat.st_mode & S_IXUSR) ? 'X' : '-');
            write(childFOut, buffer, strlen(buffer));

            sprintf(buffer, "drepturi de acces grup: %c%c%c\n",
                    (fileStat.st_mode & S_IRGRP) ? 'R' : '-',
                    (fileStat.st_mode & S_IWGRP) ? 'W' : '-',
                    (fileStat.st_mode & S_IXGRP) ? 'X' : '-');
            write(childFOut, buffer, strlen(buffer));

            sprintf(buffer, "drepturi de acces altii: %c%c%c\n",
                    (fileStat.st_mode & S_IROTH) ? 'R' : '-',
                    (fileStat.st_mode & S_IWOTH) ? 'W' : '-',
                    (fileStat.st_mode & S_IXOTH) ? 'X' : '-');
            write(childFOut, buffer, strlen(buffer));
            }

        close(childFOut);
        exit(EXIT_SUCCESS);
        } 
        else 
        {
            wait(NULL);
        }
    }

    closedir(dir);
}

int main(int argc, char *argv[]) 
{
    if (argc != 3) 
    {
        showErrorAndExit("Usage: ./proiect <director_intrare> <director_iesire>");
    }

    const char *inputDirPath = argv[1];
    const char *outputDirPath = argv[2];

    char outputFilePath[512];
    sprintf(outputFilePath, "%s/statistica.txt", outputDirPath);

    int fOut;
    if ((fOut = open(outputFilePath, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR)) < 0) 
    {
        showErrorAndExit("Nu s-a reusit deschiderea fisierului statistica.txt");
    }

    processDirectory(inputDirPath, outputDirPath, fOut);

    close(fOut);
    return 0;
}

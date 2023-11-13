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

void validateFileExtension(const char *filePath) {
    if (strstr(filePath, ".bmp") == NULL || strcmp(filePath + strlen(filePath) - 4, ".bmp") != 0) {
        showErrorAndExit("Eroare: fisierul trebuie sa aiba extensia .bmp");
    }
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

void processFile(const char *filePath, int fOut) {
    BMPHeader bmpHeader;
    int fIn;

    validateFileExtension(filePath);

    if ((fIn = open(filePath, O_RDONLY)) < 0) {
        showErrorAndExit("Nu s-a reusit deschiderea fisierului");
    }

    readBMPHeader(fIn, &bmpHeader);

    struct stat fileStat;
    if (fstat(fIn, &fileStat) == -1) {
        showErrorAndExit("Eroare la obtinerea informatiilor despre fisier");
    }

    char timeBuffer[30];
    strftime(timeBuffer, sizeof(timeBuffer), "%d.%m.%Y", localtime(&fileStat.st_mtime));

    char buffer[512];
    sprintf(buffer, "nume fisier: %s\n", filePath);
    write(fOut, buffer, strlen(buffer));

    sprintf(buffer, "inaltime: %u\n", bmpHeader.height);
    write(fOut, buffer, strlen(buffer));

    sprintf(buffer, "lungime: %u\n", bmpHeader.width);
    write(fOut, buffer, strlen(buffer));

    sprintf(buffer, "dimensiune: %u bytes\n", bmpHeader.fileSize);
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
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        showErrorAndExit("Usage: ./proiect <imagine.bmp>");
    }

    const char *filePath = argv[1];
    int fOut;

    if ((fOut = open("statistica1.txt", O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR)) < 0) {
        showErrorAndExit("Nu s-a reusit crearea fisierului statistica.txt");
    }

    processFile(filePath, fOut);

    close(fOut);
    return 0;
}

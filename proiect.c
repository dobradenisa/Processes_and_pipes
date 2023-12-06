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

typedef struct
{
    uint32_t fileSize; //dim intreg fis
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

// este un fisier cu extensia .bmp
int isBmp(const char *file_name)
{
    struct stat file_stats;
    if (lstat(file_name, &file_stats) == -1) // pt a obtine inf despre fis
        return -1;

    if (!S_ISREG(file_stats.st_mode)) //verif daca e fis regulat
        return 0;

    int bmp_file = open(file_name, O_RDONLY);
    if (bmp_file == -1)
        return -1;

    char signature[2];
    if (read(bmp_file, signature, 2) != 2) //citesct prim 2 bytes
    {
        close(bmp_file);
        return 0;
    }

    if (signature[0] == 'B' && signature[1] == 'M')
    {
        close(bmp_file);
        return 1;
    }

    close(bmp_file);
    return 0;
}

void readBMPHeader(int fIn, BMPHeader *bmpHeader)
{
    char signature[2];
    if (read(fIn, signature, 2) != 2 || signature[0] != 'B' || signature[1] != 'M')
    {
        showErrorAndExit("Eroare: Fisierul nu are un antet BMP valid");
    }

    if (read(fIn, bmpHeader, sizeof(BMPHeader)) != sizeof(*bmpHeader))
    {
        showErrorAndExit("Eroare: Fisierul nu a putut fi citit");
    }
}

int isOrdinaryFileWithoutBMPExtension(const char *file_name)
{
    struct stat file_stats;
    if (lstat(file_name, &file_stats) == -1)
        return -1;

    if (!S_ISREG(file_stats.st_mode))
        return 0;

    const char *extension = strrchr(file_name, '.');
    if (extension != NULL && strcmp(extension, ".bmp") == 0)
    {
        return 0;
    }

    return 1; // Este un fișier obișnuit fără extensia BMP
}

// este legatura simbolica
int isSymbolicLink(const char *file_name)
{
    struct stat file_stats;
    if (lstat(file_name, &file_stats) == -1)
        return -1;

    return S_ISLNK(file_stats.st_mode);
}

// este folder
int isFolder(const char *file_name)
{
    struct stat file_stats;
    if (lstat(file_name, &file_stats) == -1)
        return -1;

    return S_ISDIR(file_stats.st_mode);
}

void convertToGrayscale(const char *filePath, const char *outputDir)
{
    int fIn, fOut;
    if ((fIn = open(filePath, O_RDONLY)) < 0)
    {
        showErrorAndExit("Nu s-a reusit deschiderea fisierului");
    }

   // printf("Converting %s to grayscale...\n", filePath);

    // creez o copie a fisierului .bmp in folderul de output
    char copyPath[512]; 
    sprintf(copyPath, "%s/%s_copy.bmp", outputDir, strrchr(filePath, '/') + 1);//construiesc calea catre copie
    //strrchr(filepath, '/') - ret un pointer la ultimul caract '/' din fis filepath, adaugand 1 se obtine un pointer la primul caract dupa ultimul '/' 
    if ((fOut = open(copyPath, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR)) < 0)
    {
        perror("Nu s-a reusit crearea fisierului de copie");
        close(fIn);
        exit(EXIT_FAILURE);
    }

    char buffer[4096];//pt a stoca temporar datele citite din fis
    ssize_t bytesRead;//stocheaza nr de octeti cititi intr o iteratie
    while ((bytesRead = read(fIn, buffer, sizeof(buffer))) > 0) 
    {
        if (write(fOut, buffer, bytesRead) != bytesRead)//verif daca nr de octeti scrisi nu este egsl cu nr de octeti cititi 
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
    size_t headerSize = bmpHeader.headerSize;//extrag dim antetului

    lseek(fOut, headerSize, SEEK_SET); // Skip header

    uint8_t pixels[3000];//datele pixelilor imaginii
    ssize_t bytesReadOut; //pt a urmari nr de octeti cititi in fiecare bucla
    while ((bytesReadOut = read(fOut, pixels, sizeof(pixels))) > 0)
    {   //dupa ce sunt cititi octetii, cursorul fis este mutat inapoi cu o dist egala cu nr de octeti cititi
        //ca sa se poata scrie valorile noilor pixeli in acelasi loc
        lseek(fOut, -bytesReadOut, SEEK_CUR); 
        for(int i = 0; i < bytesReadOut; i=i + 3)//blocuri de cate 3 octeti reprez val componentelor RGB
        {
            uint8_t grayValue = (uint8_t)(0.299 * pixels[i] + 0.587 * pixels[i+1] + 0.114 * pixels[i+2]);
            //val calculata este scrisa de 3 ori ca sa se pastreze aceeasi val pt toate cele 3 comp R G B
            write(fOut, &grayValue, sizeof(grayValue));
            write(fOut, &grayValue, sizeof(grayValue));
            write(fOut, &grayValue, sizeof(grayValue));
        }
    }

    close(fOut);
}

void processDirectory(const char *dirPath, const char *outputDir, int fOut, char *caracter)
{
    DIR *dir;
    struct dirent *entry;

    if ((dir = opendir(dirPath)) == NULL)
    {
        showErrorAndExit("Nu s-a reusit deschiderea directorului");
    }

    char buffer[512];//pt mesajele pe care le scriu in fis statistica

    int sentence_pipe[2];//pipe pt comunicarea intre procesul principal si procesele copil

    if (pipe(sentence_pipe) == -1)//creare canal de comunicare intre procese
    {
        showErrorAndExit("Pipe creation failed");
    }

    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)//iterez prin continutul dir deschis
        {
            continue; // ignor "." and ".."
        }

        char filePath[512];
        sprintf(filePath, "%s/%s", dirPath, entry->d_name);

        //pt fiecare fisier/ director gasit se creeaza un proces copil
        pid_t pid = fork();

        if (pid == -1)
        {
            showErrorAndExit("Eroare la fork");
        }

        if (pid == 0)
        { // procesare fiu
            //se constr numele fis de statistici in dir de output
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

                char targetPath[512];//calea catre dest legaturii
                ssize_t targetSize = readlink(filePath, targetPath, sizeof(targetPath) - 1);//pt a citi destinatia leg
                if (targetSize == -1)
                {
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
                BMPHeader bmpHeader;
                int fIn;

                if ((fIn = open(filePath, O_RDONLY)) < 0)
                {
                    showErrorAndExit("Nu s-a reusit deschiderea fisierului");
                }

                readBMPHeader(fIn, &bmpHeader);

                char timeBuffer[30];//pt a stoca data si ora ultimei modif
                struct stat fileStat;
                if (lstat(filePath, &fileStat) == -1)
                {
                    showErrorAndExit("Eroare la obtinerea informatiilor despre fisier");
                }

                strftime(timeBuffer, sizeof(timeBuffer), "%d.%m.%Y", localtime(&fileStat.st_mtime));
                //strftime- pt a formata data si ora

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
            //se inchid capetele pipe ului
            close(sentence_pipe[0]); // read
            close(sentence_pipe[1]); // write
            exit(EXIT_SUCCESS);//procesul copil se termina cu succes
        }
        //in procesul parinte
        if (isBmp(filePath)){
                pid_t pidConvert = fork();//nou proces pt conversie care va apela functia de conversie

                if (pidConvert == -1)
                {
                    showErrorAndExit("Eroare la fork pentru conversia la tonuri de gri");
                }

                if (pidConvert == 0)
                {   
                    convertToGrayscale(filePath, outputDir); 
                    close(sentence_pipe[0]); // read
                    close(sentence_pipe[1]); // write
                    exit(EXIT_SUCCESS);
                }
        }
        else if(isOrdinaryFileWithoutBMPExtension(filePath)){

                pid_t pid2 = fork();//nou proces pt a exec script shell pe fis obisnuit
                if (pid2 == -1)
                {
                    showErrorAndExit("Eroare la fork pentru fisier obisnuit");
                }

                if (pid2 == 0)
                {
                    close(sentence_pipe[0]); // read
                    char script[50];//pt a stoca comanda scriptului
                    strcpy(script, "./script.sh "); //calea catre script
                    strcat(script, caracter); //caracterul dat ca arg
                    strcat(script, " < ");//redirect fis
                    strcat(script, dirPath);
                    strcat(script, "/");
                    strcat(script, entry->d_name);//numele fis obisnuit
                    int status = WEXITSTATUS(system(script));//execut comanda specificata de sirul script
                    //WEXITSTATUS - util pt a obt statusul de iesire al procesului fiu
                    if (write(sentence_pipe[1], &status, 4) == -1)//scriu statusul de iesire in capatul de write al pipe ului
                    {
                        showErrorAndExit("Error writing to pipe");
                    }
                    close(sentence_pipe[1]);//write
                    exit(EXIT_SUCCESS);//procesul fiu se incheie cu succes
                }
        }
    }

    closedir(dir);
    close(sentence_pipe[1]); // write
    pid_t child_pid;
    int status;
    while ((child_pid = wait(&status)) > 0)//pt a astepta terminarea oricarui proces copil
    {
        printf("Process with pid = %d finished with code: %d \n", child_pid, WEXITSTATUS(status));
    }
    int sum = 0;//pt a calcula suma rez provenite de la procesul copil
    int number;//valorile citite din pipe
    while (read(sentence_pipe[0], &number, 4) > 0)
    {
        sum += number;
    }

    printf("Total number of correct sentences %d \n", sum);
    close(sentence_pipe[0]); // read
}

int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        showErrorAndExit("Usage: ./program <director_intrare> <director_iesire> <c>");
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
    processDirectory(inputDirPath, outputDirPath, fOut, argv[3]);

    close(fOut);
    return 0;
}

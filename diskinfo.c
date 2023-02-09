#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

int SectorPerFat(char* p) {
	return p[22] + (p[23] << 8);
}

void getDiskLabel(char* Label, char* p) {
	memcpy(Label, (p + 43), 11);
	if (Label[0] == ' ') {
    int Location = 512*19;
    do{
    	int att = p[Location + 11];
    	if((att) == 0x08){
    		int i;
    		for(i = 0; i < 11; i++){
    			Label[i] = p[Location + i];
    		}
    		break;
    	}
    	Location += 32;
		}while(Location < 33*512);
	}
}

int Sectorcount(char* p) {
	return p[19] + (p[20] << 8);
}


int FreeDiskSize(char *p){
    int free_size = 0;
    int Secount = Sectorcount(p) ;
    int segment =512;
    int loop = 2;
    do{
        int num;
        int soap=loop/2;
        if(loop%2 == 0){
            int small = p[segment + 1+(3*soap)];
            int mid = p[segment + (3*soap)];
            num = (small << 8) + mid;
        } 
        if (loop%2 != 0){
            int high = p[segment + (3*soap)];
            int mid = p[segment + 1+(3*soap)];
            num = (high >> 4) + (mid << 4);
        }
        if(num == 0x00){
            free_size++;
        }
        loop++;
    }while(loop < Secount-31);
    free_size = free_size * 512;

    return free_size;
}



int NumOfRootFiles(char *p){
	int num_files = 0;
    int Location = 512*19;
    do{
    char nameoffile[9];
        int att = p[Location + 11];
		memcpy(nameoffile, (p + Location), 8);
    	if(att == 0x0F || nameoffile[0] == 0x00 || nameoffile[0] == 0xE5 || (att & 0x08) == 0x08){
    		Location += 32;
            continue;
    	}
		if(att != 0x10){
    	num_files++;
		}
    	Location += 32;
    }while(Location < 33*512);
    return num_files;
}

int NumOfFatCopies(char *p){
    
    return p[16];
}
// Please remember to give write permission to your argv[1] (the image you want map) by using chmod (if it doest not have the write permission by default), midwise you will fail the map.
void printInfo(char* osName, char* diskLabel, int diskSize, int freeSize, int numberOfRootFiles, int numberOfFatCopies, int sectorsPerFat) {
	printf("OS Name: %s\n", osName);
	printf("Label of the disk: %s\n", diskLabel);
	printf("Total size of the disk: %d bytes\n", diskSize);
	printf("Free size of the disk: %d bytes\n\n", freeSize);
	printf("==============\n");
	printf("The number of files in the root directory (not including subdirectories): %d\n\n", numberOfRootFiles);
	printf("=============\n");
	printf("Number of FAT copies: %d\n", numberOfFatCopies);
	printf("Sectors per FAT: %d\n", sectorsPerFat);
}


int main(int argc, char *argv[])
{
	int fd;
	fd = open(argv[1], O_RDWR);
	if (fd < 0) {
		printf("could not read disk image\n");
		exit(1);
	}
	struct stat sb;
	fstat(fd, &sb);

	char * p = mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0); // p points to the starting pos of your mapped memory
	if (p == MAP_FAILED) {
		printf("Error: failed to map memory\n");
		exit(1);
	}
	

	char* osName = malloc(sizeof(char));
	memcpy(osName, (p + 3), 8);
	char diskLabel[12];
	getDiskLabel(diskLabel, p);
	int freeSize = FreeDiskSize(p);
	int numberOfRootFiles = NumOfRootFiles(p);
	int numberOfFatCopies = NumOfFatCopies(p);
	int sectorsPerFat = SectorPerFat(p);
	//p[0] = 0x00; // an example to manipulate the memory, be careful when you try to uncomment this line because your image will be modified
	printInfo(osName, diskLabel, sb.st_size, freeSize, numberOfRootFiles, numberOfFatCopies, sectorsPerFat);
	// printf("modified the memory\n");
	munmap(p, sb.st_size); // the modifed the memory p would be mapped to the disk image
	close(fd);
	return 0;
}

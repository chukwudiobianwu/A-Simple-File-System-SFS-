#include<stdio.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<sys/mman.h>kni
#include<fcntl.h>
#include<unistd.h>
#include<stdlib.h>
#include<string.h>

#define timeLocation 14 //Location of creation time in directory entry
#define dateLocation 16 //Location of creation date in directory entry

static unsigned int bytes_per_sector;
void print_date_time(char * directory_entry_startPos){
	
	int time, date;
	int hours, minutes, day, month, year;
	
	time = *(unsigned short *)(directory_entry_startPos + timeLocation);
	date = *(unsigned short *)(directory_entry_startPos + dateLocation);
	
	//the year is stored as a num since 1980
	//the year is stored in the high seven bits
	year = ((date & 0xFE00) >> 9) + 1980;
	//the month is stored in the middle four bits
	month = (date & 0x1E0) >> 5;
	//the day is stored in the low five bits
	day = (date & 0x1F);
	
	printf("%d-%02d-%02d ", year, month, day);
	//the hours are stored in the high five bits
	hours = (time & 0xF800) >> 11;
	//the minutes are stored in the middle 6 bits
	minutes = (time & 0x7E0) >> 5;
	
	printf("%02d:%02d\n\n", hours, minutes);
	
	return;	
}

void byt_per_sec(char *p){
	memcpy(&bytes_per_sector, (p + 11), 2);
	print_date_time(p + bytes_per_sector * 19);
}

int get_file_size(char *p, int Location){
	int bron = ((p[Location+31]&0xFF)<<24);
	int blew = ((p[Location+30]&0xFF)<<16);
	int go = ((p[Location+29]&0xFF)<<8);
	int dlop =(p[Location+28]&0xFF);
    return bron | blew | go | dlop;
}

int FreeDiskSizeLocation(int bro , char *p){
   	int num;
   	if(bro % 2 == 0){
    	int low = p[512 + 1+(3*bro/2)] & 0x0F;  
    	int other = p[512 + (3*bro/2)] & 0xFF;
    	num = (low << 8) + other; 
    } else{
    
    	int high = p[512 + (3*bro/2)] & 0xF0;
    	int other = p[512 + 1+(3*bro/2)] & 0xFF;
    	num = (high >> 4) + (other << 4);
    }
    return num;
}

void  Printlist(char *p, int Location) {
    int i;
    int clust = Location/512-31;
    int cn;
	if(-12 == clust){
		cn =-208;
	} else{
		cn = 0;
	}
	char *entry_name = malloc(sizeof(char));
    char *file_ext = malloc(sizeof(char));
    while (0x00 != p[Location+0] && 16 > cn) {
        char Type;
		entry_name = (char *)realloc(entry_name, sizeof(char));
        file_ext = (char *)realloc(file_ext, sizeof(char));
		int att = p[Location+11];
        if (0x00 == (att&0x02) && 0x00 == (att&0x08) && 0xE5 != p[Location+0] && 0x0F != att && '.' != p[Location+0]) {
            if (0x00 == (att)) {
                Type = 'F';
            }
            if(0x10 == (att)) {
                Type = 'D';
            }
// get entry name
            for (i=0; i<8; i++) {
                if (' ' == p[Location+i]) {
                    break;
                }
                else {
                    entry_name[i] = p[Location+i];
                }
            }
            entry_name[i] = '\0';

            // get extension
            for (i=8; i<11; i++) {
                if (' ' == p[Location+i]) {
                    break;
                }
                else {
                    file_ext[i-8] = p[Location+i];
                }
            }
            file_ext[i-8] = '\0';

            // if is file, add extension to the file name
            strncpy(entry_name, entry_name, 8);
            if ('F' == Type) {
                strcat(entry_name, ".");
                strncat(entry_name, file_ext, 3);
            }

		int file_size = get_file_size(p,Location);
            if ('F' == Type) {
                printf("%c %10d %20s ", Type, file_size, entry_name);
				print_date_time(&p[Location]);
            }
            else {
                printf("%c %10s %20s ", Type, "0", entry_name);
				print_date_time(&p[Location]);
            }
        }

        // next directory entry
        Location += 32;
        cn++;
    }
	free(entry_name);
    free(file_ext);
}



void showfiles(char *dir_name,int Location, char *p) {
    int clust = Location/512-31;
    int *subad = malloc(sizeof(int) * 1000);
    char subname[100][100]; 
    int subcn = 0;
    int cn;
	int i;
    int temp = clust;
    do {
        Location = 512 * (temp+31);
        cn = (-12 == clust) ? -208 : 0;
        while (0x00 != p[Location+0] && 16 > cn) {
            char nameoffile[8];
			int att = p[Location+11];
            if ('.' != p[Location+0] && 0xE5 != p[Location+0] && 0x0F != att && 0x00 == (att&0x02) && 0x00 == (att&0x08)) {
                if (0x10 == (att)) {
                    // get entry name
					memcpy(nameoffile, (p + Location), 8);
	
                    // stores into arrays to be called later
                    strcpy(subname[subcn], nameoffile);
                    int first_clust = (p[Location + 26])+ (p[Location + 27]<<8);
                    subad[subcn] = 512*(31+first_clust);
                    subcn++;
					
                }
            }
            // next directory entry
            Location += 32;
            // one sector holds 16 entries
            cn++;
        }
		if(temp > 1 ){
			temp =FreeDiskSizeLocation(temp, p);
		}else{
			temp = 0x00;
		}
		
    } while (0x00 != temp && 0xff0 > temp);

    // starts print out info about current directory
    printf("%s\n==================\n", dir_name);
    do {
        Location = 512 * (clust+31);
        Printlist(p, Location);
        // check FAT tables

	if(clust > 1){
		clust = FreeDiskSizeLocation(clust, p);
	} else{
		clust = 0x00;
	}
    } while (0x00 != clust && 0xff0 > clust);

    for (i=0; i<subcn; i++) {
        showfiles(subname[i],subad[i],p);
    }
    free(subad);
}

int main(int argc, char *argv[]){
	int fd;
	struct stat sb;

	fd = open(argv[1], O_RDWR);
	fstat(fd, &sb);
	//printf("Size: %lu\n\n", (uint64_t)sb.st_size);

	char * p = mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0); // p points to the starting pos of your mapped memory
	if (p == MAP_FAILED) {
		printf("Error: failed to map memory\n");
		exit(1);
	}
	//p[0] = 0x00; // an example to manipulate the memory, be careful when you try to uncomment this line because your image will be modified
	byt_per_sec(p);
    int bull = 512*19;
	char *di = "ROOT";
    showfiles(di,bull ,p);
	munmap(p, sb.st_size); // the modifed the memory p would be mapped to the disk image
	close(fd);
	return 0;
}



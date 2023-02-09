
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>


int FreeDiskSizeLocation(int vi, char *p){
   	int num;
    p+=512;
   	if(vi % 2 == 0){
    	int low = p[512 + 1+(3*vi/2)] & 0x0F;  
    	int other = p[512 + (3*vi/2)] & 0xFF;
    	num = (low << 8) + other; 
    } else{
    	int high = p[512 + (3*vi/2)] & 0xF0;
    	int other = p[512 + 1+(3*vi/2)] & 0xFF;
    	num = (high >> 4) + (other << 4);
    }
    return num;
}

int freedisk(char *p) {
    int secnt = 0;
    for (int i=2; i<=2848; i++) {
        if (0x00 == FreeDiskSizeLocation(i, p)) {
            secnt++;
        }
    }

    return secnt*512;
}
int firstclust(char *fname, char *p, int Location) {
    int logical_cluster = Location/512-31;
    int result = -1;
    int cn;
    char nameoffile[12];
    int j;
    int k;
    do {
        // one sector contains 512/32 = 16 entries
        // if curr dir is root(Location = 19 * 512) then 14 sectors (19~32)
        cn = (-12 == logical_cluster) ? -208 : 0;
        Location = 512 * (logical_cluster+31);
        while (0x00 != p[Location+0] && 16 > cn) {
        int att = p[Location+11];
        if (0x00 == (att&0x02) && 0x00 == (att&0x08) && 0xE5 != p[Location+0] && 0x0F != att && '.' != p[Location+0]) {
                if (0x00 == (p[Location+11]&0x10)) {
                    // get file name
            for(j = 0; j < 8; j++){
    		if(p[Location + j] == ' '){
    			j++;
    			break;
    		}
    		nameoffile[j] = p[Location + j];
    		k++;
    	}
		nameoffile[k] = '.';
    	k = 8-k;
    	for(j = 8; j < 12; j++){
    		nameoffile[j+1-k] = p[Location + j];
		}

                    // combine file name and extension
                    // check if is a match
                    if (0 == strcmp(fname, nameoffile)) {
                        // starting Location: 26, len: 2
                        result = ((p[Location+27]&0xFF)<<8) + (p[Location+26]&0xFF);
                        break;
                    }
                }
                else {
                    int first_logical_cluster = ((p[Location+27]&0xFF)<<8) + (p[Location+26]&0xFF);
                    int subdir_Location = 512*(31+first_logical_cluster);
                    result = firstclust(fname, p, subdir_Location);
                    // once get the result return it
                    if (-1 != result) {
                        break;
                    }
                }
            }
            Location += 32;
            cn++;
        }
        // once get the result return it
        if (result != -1) {
            break;
        }
        logical_cluster = (1<logical_cluster) ? FreeDiskSizeLocation(logical_cluster, p) : 0x00;
    } while (0x00 != logical_cluster && 0xff0 > logical_cluster);
    return result;
}

void Writeto(int index, int value, char *p) {
    p += 512;
    if (0 == index%2) {
        p[1+(3*index)/2] = ((value>>8)&0x0F)|(p[1+(3*index)/2]&0xF0);
        p[(3*index)/2] = value&0xFF;
    }
    else {
        p[(3*index)/2] = ((value<<4)&0xF0)|(p[(3*index)/2]&0x0F);
        p[1+(3*index)/2] = (value>>4)&0xFF;
    }
}

void editdir(char *fname, int fsize, int flc, char *p, int Location) {
    int logical_cluster = Location/512-31;
    int posit = logical_cluster;
    int move = posit;
    // find last sector of dir listing
    while (-12 != logical_cluster && 0xff0 > move) {
        posit = move;
        move = FreeDiskSizeLocation(posit, p);
    }
    
    //found Location of target sector
    int target_Location = 512*(posit+31);
    int sym = -1;
    int cn = 0;
    int cnt;
    if (logical_cluster == -12){
        cnt = 224;
    }else{
        cnt = 16;
    }

    while (cnt > cn) {
        if (0x00 == p[target_Location+0]) {
            sym = 0;
            break;
        }
        target_Location += 32;
        cn++;
    }

    if (-1 == sym) {
        int new_cluster = freedisk(p);

        // handle error when not found new space in FAT table
        if (-1 == new_cluster) {
            printf("error: not enough free space\n");
            exit(1);
        }

        Writeto(posit, new_cluster, p);
        Writeto(new_cluster, 0xFFF, p);
        editdir(fname, fsize, flc, p, 512*(new_cluster+31));
        return;
    }
    int i;
    int ext_pos = -1;

    for (i=0; i<8; i++) {
        if ('.' == fname[i]) {
            break;
        }
        else {
            p[target_Location+i] = fname[i];
        }
    }
    for (; i<8; i++) {
        // fill the rest with space
        p[target_Location+i] = ' ';
    }

    for (i=0; i<256; i++) {
        if ('.' == fname[i]) {
            ext_pos = i+1;
            break;
        }
    }

    // extension: 8~10
    for (i=0; i<3; i++) {
        p[target_Location+8+i] = fname[ext_pos+i];
    }

    // set file attributes: 11
    p[target_Location+11] = 0x00;

    time_t t = time(NULL);
    struct tm *now = localtime(&t);
    int year = now->tm_year+1900; // year since 1900
    int month = now->tm_mon+1;
    int day = now->tm_mday; // day of month [1, 31]
    int hour = now->tm_hour;
    int minute = now->tm_min;
    // init
    p[target_Location+14] = 0;
    p[target_Location+15] = 0;
    p[target_Location+16] = 0;
    p[target_Location+17] = 0;
    // write
    p[target_Location+17] |= (year-1980)<<1; // high 7 bits
    p[target_Location+17] |= (month-((p[target_Location+16]&0b11100000)>>5))>>3; // middle 4 bits
    p[target_Location+16] |= (month-((p[target_Location+17]&0b00000001)<<3))<<5;
    p[target_Location+16] |= (day&0b00011111); // lower 5 bits
    p[target_Location+15] |= (hour<<3)&0b11111000; // high 5 bits
    p[target_Location+15] |= (minute-((p[target_Location+14]&0b11100000)>>5))>>3; // middle 6 bits
    p[target_Location+14] |= (minute-((p[target_Location+15]&0b00000111)<<3))<<5;
    // set first logical cluster value: 26~27
    p[target_Location+26] = flc&0xFF;
    p[target_Location+27] = (flc&0xFF00)>>8;
    // set file size: 28~31
    p[target_Location+28] = (fsize&0x000000FF);
    p[target_Location+29] = (fsize&0x0000FF00)>>8;
    p[target_Location+30] = (fsize&0x00FF0000)>>16;
    p[target_Location+31] = (fsize&0xFF000000)>>24;

}


int make_filecopy(char *fname, int fsize, char *p1, char *p2, int Location) {
    int remaining_byte = fsize;
    int prclust = freedisk(p1);
    int cluster_Location;
    int i;
    if (-1 == prclust) {
        return -1;
    }

    // make sure the file does not exist in the directory
    if (-1 == firstclust(fname, p1, Location)) {
        // update info in directory
        editdir(fname, fsize, prclust, p1, Location);
        while (0 < remaining_byte) {
            if (-1 == prclust) {
                return -1;
            }
            cluster_Location = 512*(prclust+31); // physical address of the cluster index

            // write into sector by sector, one sector is corresponding to one cluster value
            for (i=0; i<512; i++) {
                if (0 == remaining_byte) {
                    Writeto(prclust, 0xFFF, p1);
                    return 0;
                }
                p1[i+cluster_Location] = p2[fsize-remaining_byte];
                remaining_byte--;
            }

            if (0 == remaining_byte) {
                Writeto(prclust, 0xFFF, p1);
                return 0;
            }
            Writeto(prclust, prclust, p1);
            int move_cluster = freedisk(p1);
            // write the value of the move cluster in position of current cluster
            Writeto(prclust, move_cluster, p1);
            prclust = move_cluster;
        }
    }
    else {
        printf("File already exists.\n");
    }

    return 0;
}


int showfiles(char **subdirs, int cur_pos, int des_pos, char *p, int Location) {
    int logical_cluster = Location/512-31;
    int entries_count;
    int i;

    char *dir_name = malloc(sizeof(char));

    do {
        // one sector contains 512/32 = 16 entries
        // if curr dir is root(Location = 19 * 512) then 14 sectors (19~32)
        entries_count = (-12 == logical_cluster) ? -208 : 0;
        Location = 512*(logical_cluster+31);

        while (0x00 != p[Location+0] && 16 > entries_count) {
            dir_name = (char *)realloc(dir_name, sizeof(char));

        	int att = p[Location+11];
            if ('.' != p[Location+0] && 0xE5 != p[Location+0] && 0x0F != att && 0x00 == (att&0x02) && 0x00 == (att&0x08)) {
                if (0x10 == (att)) {
                    // get directory name
                    for (i=0; i<8; i++) {
                        if (' ' == p[Location+i]) {
                            break;
                        }
                        else {
                            dir_name[i] = p[Location+i];
                        }
                    }
                    dir_name[i] = '\0';

                    // check if dir_name matches subdir
                    if (0 == strcmp(dir_name, subdirs[cur_pos])) {
                        if (cur_pos == des_pos) {
                            // base case where it reached the last subdir
                            return ((p[Location+27]&0xFF)<<8) + (p[Location+26]&0xFF);
                        } else {
                            // get Location(physical address) of this subdir
                            int first_logical_cluster = ((p[Location+27]&0xFF)<<8) + (p[Location+26]&0xFF);
                            int subdir_Location = 512*(31+first_logical_cluster);

                            return 0 + showfiles(subdirs, cur_pos+1, des_pos, p, subdir_Location);
                        }
                    }
                }
            }

            // next entry
            Location += 32;
            entries_count++;
        }
        // get number of next cluster
        logical_cluster = FreeDiskSizeLocation(logical_cluster, p);
    } while(0x00 != logical_cluster && 0xff0 > logical_cluster);

    return -1;
}

int main(int argc, char *argv[]) {

    char inputs[255] = "";

    strncpy(inputs, argv[2], 255);
    inputs[254] = '\0';
    // holds subdir(s)
    char **subdir = malloc(sizeof(char *) * 1000);
    char *file_name;
    char *temp;
    int subdir_len = 0;

    char *delim = "/";
    temp = strtok(inputs, delim);

    while (NULL != temp) {
        subdir[subdir_len] = temp;
        temp = strtok(NULL, delim);
        subdir_len++;
    }
    // last one is the file
    file_name = strdup(subdir[subdir_len-1]);
    subdir_len--;
    char * pp;
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
    int ft;
	struct stat sbt;
	ft = open(file_name, O_RDONLY);
	fstat(ft, &sbt);
    if (ft == -1){
        printf("File not found.\n");
    }else{
	pp = mmap(NULL, sbt.st_size, PROT_READ, MAP_SHARED, ft, 0); // p points to the starting pos of your mapped memory
	if (pp == MAP_FAILED) {
		printf("Error: failed to map memory\n");
		exit(1);
	}
    }
    if (sbt.st_size <= freedisk(p)) {
        int root_dir_Location = 512*19;
        int brew = showfiles(subdir, 0, subdir_len-1, p, root_dir_Location);
        if (0 != subdir_len && -1 == brew ){
            printf("The directory not found.\n");
            exit(1);
        }

        int start_Location = (0 == subdir_len) ? root_dir_Location : 512*(brew+31);

        // start copying file to the disk
        int solo = make_filecopy(file_name, (int)sbt.st_size, p, pp, start_Location);
        if (0 == solo){
            printf("error: cannot copy file to disk.\n");
            exit(1);
        }
    }
    else {
        // not enough free space to copy this file
        printf("No enough free space in the disk image.\n");
        exit(1);
    }
    munmap(p, sb.st_size);
    munmap(pp, sbt.st_size);
    close(fd);
    close(ft);
    free(subdir);
    return 0;

}
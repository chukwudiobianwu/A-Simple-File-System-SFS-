#include<stdio.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<sys/mman.h>
#include<fcntl.h>
#include<unistd.h>
#include<stdlib.h>
#include<string.h>
#include <ctype.h>

int getsize(char *p, int Location){
	int bron = ((p[Location+31]&0xFF)<<24);
	int blew = ((p[Location+30]&0xFF)<<16);
	int go = ((p[Location+29]&0xFF)<<8);
	int dlop =(p[Location+28]&0xFF);
    return bron | blew | go | dlop;
}

int FreeDiskSizeLocation(int vi, char *p){
   	int num;
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


void Copytofile(int cluster, char *p, char *pfilen, int sizeoffile){
	int nxtc = FreeDiskSizeLocation(cluster, p);
	FILE *out_file;
   	out_file = fopen(pfilen, "wb");
	do{
		//read 512 bytes from the cluster
		int Location = (cluster + 31) * 512;
		for(int i = 0; i < 512; i++){
			fputc(p[Location + i], out_file);
		}
		cluster = nxtc;
		nxtc = FreeDiskSizeLocation(cluster, p);
	}while(0xfff != nxtc);

	//read the remaining bytes from last cluster
	int retain = sizeoffile - (sizeoffile/512)*512;
	int Location = (cluster + 31) * 512;
	for(int i = 0; i < retain; i++){
		fputc(p[Location + i], out_file);
	}
	fclose(out_file);
	return;
}



int Showfiles(char *dir_name, int cil, char *p){
    int Location = cil;
    do{
    	char nameoffile[12];
    	nameoffile[11] = '\0';
    	char file_type;
    	int j;
    	int k = 0;    	
        int att = p[Location + 11];
    	for(j = 0; j < 8; j++){
    		if(p[Location + j] == ' '){
    			j++;
    			continue;
    		}
    		nameoffile[j] = p[Location + j];
    		k++;
    	}
    	nameoffile[k] = '.';
    	k = 8-k;
    	
    	for(j = 8; j < 12; j++){
    		nameoffile[j+1-k] = p[Location + j];
    	}
    	
    	if(att == 0x0F || nameoffile[0] == 0x00 || nameoffile[0] == 0xE5 || (att & 0x08) == 0x08){
    		Location += 32;
    		continue;
    	}

		if(strcmp(nameoffile, dir_name) == 0){ //Found the file
    		return Location;
    	}

   		Location += 32;
    }while(Location < 33*512);
	return 0;
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
	char *pfilen = argv[2];
	int i = 0;
	while(pfilen[i]) {
      pfilen[i] = toupper(pfilen[i]);
      i++;
   	}
   	int found = 0;
	int bull = 512*19;
   	found = Showfiles(pfilen, bull ,p);
    if(found != 0){
    	int first_logical_cluster = (p[found + 26])+ (p[found + 27]<<8);
    	int sizeoffile = getsize(p, found);
    	Copytofile(first_logical_cluster, p, pfilen, sizeoffile);
    }else {
		printf("File not found.\n");
    }
	munmap(p, sb.st_size); // the modifed the memory p would be mapped to the disk image
	close(fd);
	return 0;
}



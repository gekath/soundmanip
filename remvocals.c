#include<stdio.h>
#include<stdlib.h>

#define HEADER_SIZE 44

int main(int argc, char *argv[]) {
    
    // Check that 3 arguments were passed in (remvocals, sourcewav, destwav)
    if (argc != 3) {
        fprintf(stderr, "Usage: remvocals sourcewav destwav\n");
        exit(EXIT_FAILURE);
    }
    
    // Open the source and destination files for read and write respectively
    FILE *source, *dest;
    
    // source file opened for reading, from sourcewav argument
    if ((source = fopen(argv[1], "rb")) == NULL) {
        printf("%s can't be opened\n", argv[1]);
        exit(EXIT_FAILURE);
    }
    // destination file opened for writing, from destwav argument
    if ((dest = fopen(argv[2], "wb")) == NULL) {
        printf("%s can't be opened\n", argv[2]);
        exit(EXIT_FAILURE);
    }

    // Get size of wav file minus the 44 bytes from the header
    fseek(source, 0L, SEEK_END);
    int bytecount = (ftell(source) - 44);
    rewind(source);

    // Read in first 44 bytes, store in buffer first44
    int first44[HEADER_SIZE];

    if ((fread(first44, 1, HEADER_SIZE, source)) != HEADER_SIZE) {
	    if (ferror(source)) {
		    perror("fread, error reading first 44 bytes\n");
	    } else {
		    fprintf(stderr, "Unexpected end of file while reading first 44 bytes \n");
	    } exit(EXIT_FAILURE);
    }

    // Write to header to dest file, from buffer first44

    if ((fwrite(first44, 1, HEADER_SIZE, dest)) != HEADER_SIZE) {
	    if (ferror(dest)) {
		    perror("fwrite, error writing first 44 bytes\n");
	    } else {
		    fprintf(stderr, "Unexpected end of file while writing first 44 bytes \n");
	    } exit(EXIT_FAILURE);
    }

    // Read in remaining bytes in file, 1 sample at a time (2 short ints)
    while (bytecount > 0) {

    	short left[0];
    	short right[0];

        // Read in one short int (left speaker of sample)
    	if ((fread(left, sizeof(short), 1, source)) != 1) {
	    	if (ferror(source)) {
		    	perror("fread, error reading left shorts\n");
    		} else {
	    		fprintf(stderr, "Unexpected end of file while reading left shorts\n");
		    } exit(EXIT_FAILURE);
	    }

        // Read in one short int (right speaker of sample)
	    if ((fread(right, sizeof(short), 1, source)) != 1) {
		    if (ferror(source)) {
			    perror("fread, error reading right shorts\n");
    		} else {
	    		fprintf(stderr, "Unexpected end of file while reading right shorts\n");
		    } exit(EXIT_FAILURE);
	    }

        // combined = (left - right) / 2
	    short combined[0];
    	combined[0] = (left[0] - right[0]) / 2;
	
        // Write combined to dest file twice
        if ((fwrite(combined, sizeof(short), 1, dest)) != 1) {
            if (ferror(dest)) {
                perror("fwrite, error writing sequence of shorts\n");
            } else {
                fprintf(stderr, "Unexpected end of file while writing sequence of shorts\n");
            } exit(EXIT_FAILURE);
       }

	    if ((fwrite(combined, sizeof(short), 1, dest)) != 1) {
		    if (ferror(dest)) {
			    perror("fwrite, error writing sequence of shorts\n");
    		} else {
	    		fprintf(stderr, "Unexpected end of file while writing sequence of shorts\n");
		    } exit(EXIT_FAILURE);
	    }

        // decrement bytecount by size of one sample (2 short ints)
        bytecount = bytecount - (2* (sizeof(short)));
	
    }
    
    // Close source and dest files
    fclose(source);
    fclose(dest);
    return 0;
    
}

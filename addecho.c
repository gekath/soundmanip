#include<stdio.h>
#include<stdlib.h>
#include <unistd.h>

#define DELAY_LEVEL 8000
#define VOLUME_LEVEL 4
#define HEADER_SIZE 22
#define SHORT0 0

void writeheader(FILE *, FILE *, int);
void delaygreater(FILE *, FILE *, int, int, int);
void delayless(FILE *, FILE *, int, int, int);
void readwritefile(short **, FILE *, FILE *, int);

int main(int argc, char *argv[]) {

	// Check that arguments are between 3 and 7 inclusive
	if (argc < 3 || argc > 7) {
		fprintf(stderr, "Usage: addecho [-d delay] [-v volume_scale] sourcewav destwav\n");
		exit(EXIT_FAILURE);
	}

	int opt, delay, volume_level;
	
	delay = DELAY_LEVEL;
	volume_level = VOLUME_LEVEL;
	while (( opt = getopt(argc, argv, "d:v:")) != -1) {
        switch (opt) {
		case 'd':
	        if (atoi(optarg) >= 0) {
                delay = atoi(optarg);
                break;
            } else {
                fprintf(stderr, "Argument must be positive integer\n");
                exit(1);
            }
		case 'v':
            if (atoi(optarg) >= 0) {
                volume_level = atoi(optarg);
                break;
            } else {
                fprintf(stderr, "Argument must be positive integer\n");
                exit(1);
            }
        case ':':
            fprintf(stderr, "Expected argument after options\n");
            break;
        default:
			fprintf(stderr, "Usage: addecho [-d delay] [-v volume_scale] sourcewav destwav\n");
			exit(EXIT_FAILURE);	
		}
	}
    if (optind >= argc) {
        fprintf(stderr, "Expected argument after options\n");
        exit(1);
    }

    // Open files source and dest for reading and writing respectively
	FILE *source, *dest;

    // source opened for reading from argument sourcewav
	if ((source = fopen(argv[argc-2], "rb")) == NULL) {
		printf("%s can't be opened\n", argv[argc-2]);
		exit(EXIT_FAILURE);
	}
	// dest opened for writing from argument destwav
	if ((dest = fopen(argv[argc-1], "wb")) == NULL) {
		printf("%s can't be opened\n", argv[argc-1]);
		exit(EXIT_FAILURE);
	}

	// Get size of file bytecount minus the header (44 bytes)
	fseek(source, 0L, SEEK_END);
	int bytecount = ftell(source) - 44;
	rewind(source);

    // Read and write header from source to dest
    writeheader(source, dest, delay);

    // If delay specified is larger than the bytecount, this function handles the case
    // where the delay between the orig, unmodified buffer and the volume scaled buffer needs
    // to be padded with samples of 0s
    if (delay * (sizeof(short)) > bytecount) {
        delaygreater(source, dest, bytecount, delay, volume_level);
    
    // If delay is less than bytecount, this function writes volume_scaled samples to dest
    // file using a buffer of delay samples at a time.
    } else {

        delayless(source, dest, bytecount, delay, volume_level);
    }

    // Close source and dest files
    fclose(source);
    fclose(dest);
    return 0;
}

/* Reads header of size HEADER_SIZE from source file and writes to dest file. 
 *
 * @param FILE * source The source file to be read from.
 * @param FILE * dest The destination file to be written to.
 * @param int delay The specified number of bytes the echo is delayed. 
 * @return 
 */
void writeheader(FILE * source, FILE * dest, int delay) {

    // Reading and modifying header using buffer header (first 44 bytes)
    short header[HEADER_SIZE];

    // Read the header from source (first 44 bytes) to buffer header
    if ((fread(header, sizeof(short), 22, source)) != 22) {
        if (ferror(source)) {
            perror("fread");
        } else {
            fprintf(stderr, "Unexpected end of file while reading in first 44 bytes\n");
        } exit(1);
    }

	// shorts 2 and 20 tell us the size of the wav file
    // increase each of these integers by delay * 2, otherwise they won't
    // accurately represent the length of the wav files with echo in them

    // increase short 2 by delay * 2
    unsigned int * sizeptr1;
	sizeptr1 = (unsigned int * )(header + 2);
	* sizeptr1 = *sizeptr1 + delay * 2;

    // increase short 20 by delay * 2 
	unsigned int * sizeptr2;
	sizeptr2 = (unsigned int *)(header + 20);
	* sizeptr2 = *sizeptr2 + delay * 2;
	
	// Write header to dest (first 44 bytes) from buffer header
	if ((fwrite(header, sizeof(short), 22, dest)) != 22) {
		if (ferror(dest)) {
			perror("fwrite");
		} else {
			fprintf(stderr, "Unexpected end of file while writing first 44 bytes \n");
		} exit(1);
	}
}

/* Writes the wav file (bytecount number of bytes) to dest unmodified (excluding header bytes), 
 * then writes delay - bytecount number of 0s to the dest,
 * then writes echoed wav file scaled by a factor of volume_scale to dest (delayed by delay bytes).
 *
 * @param FILE * source The source file to be read from.
 * @param FILE * dest The destination file to be written to.
 * @param int bytecount The number of bytes of the source file minus the 44 bytes from the header.
 * @param int delay The number of bytes to wait before the echo is written to the destination file.
 * @param int volume_level The factor by which the wav file is scaled down for the echo. 
 * @return 
 */ 
void delaygreater(FILE * source, FILE * dest, int bytecount, int delay, int volume_level) {

    int buffersize = bytecount / sizeof(short);
    short * origbuffer;

    // Initialize buffer origbuffer of buffersize samples
    origbuffer = malloc(sizeof(short) * buffersize);
    if (origbuffer == NULL) {
        perror("malloc");
        exit(1);
    }

    // Read and write buffersize number of bytes from source to dest
    readwritefile(&origbuffer, source, dest, buffersize);

    // Write (delay - buffersize) 0 samples to dest
    // (Original samples might not have delay samples in it, we have to wait a total of delay samples
    // before producing echo)
    int diff = delay - buffersize;
    int i = 0;
    short shortarray[diff];
    for (i = 0; i < diff; i++) {
        shortarray[i] = SHORT0;
    }

    // Write diff number of 0 shorts to dest file
    if ((fwrite(shortarray, sizeof(short), diff, dest)) != diff) {
        if (ferror(dest)) {
            perror("fwrite");
        } else {
            fprintf(stderr, "Unexpected end of file, writing 0 samples\n");
        } exit(1);
    }

    // Each byte in orig buffer is scaled down by a factor of volume_scale
    // Stored in echobuffer, this is to be written to dest file after delay bytes.
    short echobuffer[buffersize];
    i=0;
    for (i=0; i < buffersize; i++) {
        echobuffer[i] = origbuffer[i] / volume_level;
    }

    // Write echobuffer to dest file
    if ((fwrite(echobuffer, sizeof(short), buffersize, dest)) != buffersize) {
        if (ferror(dest)) {
            perror("fwrite");
        } else {
            fprintf(stderr, "Unexpected end of file, delay > bytecount, writing echo\n");
        } exit(1);
    }
}

/* Reads bufsize number of samples from source and writes to an array of shorts pointed to by buf.
 * The array pointed to by buf is then written to dest.
 *
 * @param short ** buf Pointer to an array which is to store bufsize number of samples from source and write to dest.
 * @param FILE * source The source file to be read from.
 * @param FILE * dest The dest file to be written to.
 * @param int bufsize The number of samples to be read from source file, stored in array pointed to by buf, and written to dest.
 * @return
 */
void readwritefile(short ** buf, FILE * source, FILE * dest, int bufsize) {
    
    // Read in samples of bufsize bytes from source to buffer buf
    if ((fread(* buf, sizeof(short), bufsize, source)) != bufsize) {
        if (ferror(source)) {
            perror("fread");
        } else {
            fprintf(stderr, "Unexpected end of file\n");
        } exit(1);
    }

    // Write samples of bufsize bytes from buffer buf to dest
    if ((fwrite(* buf, sizeof(short), bufsize, dest)) != bufsize) {
        if (ferror(dest)) {
            perror("fwrite");
        } else {
            fprintf(stderr, "fwrite, error writing orig samples before delays\n");
        } exit(1);
    }
}

/* For delay samples at a time, read from source file, scale down the samples by volume_level and write to dest.
 * Precondition: delay <= bytecount
 *
 * @param FILE * source The source file to be read from.
 * @param FILE * dest The destination file to be written to.
 * @param int bytecount The number of bytes left in the source file to be read from.
 * @param int delay The number of samples to wait before writing volume-scaled bytes to destination.
 * @param int volume_level The factor by which delay samples are to be scaled down. 
 * @return
 */
void delayless(FILE * source, FILE * dest, int bytecount, int delay, int volume_level) {

    short * origbuffer; // delay samples read from source file
    short * echobuffer; // volume scaled samples read from source file
    short * mix;

    origbuffer = malloc(sizeof(short) * delay);
    if (origbuffer == NULL) {
        perror("malloc");
        exit(1);
    }

    echobuffer = malloc(sizeof(short) * delay);
    if (echobuffer == NULL) {
        perror("malloc");
        exit(1);
    }
    
    mix = malloc(sizeof(short) * delay);
    if (mix == NULL) {
        perror("malloc");
        exit(1);
    }

    // Read delay samples from source and write to destination. These delay samples stored in origbuffer.
    readwritefile(&origbuffer, source, dest, delay);

    // Decrement bytecount by sizeof(short) * delay
    bytecount = bytecount - sizeof(short) * delay;

    // Keep track if delay > bytecount of tail of sound file
    int nread = delay; 

    // While there are still bytes to be read from source
    while (bytecount > 0) {
        
        // Store volume-scaled samples from origbuffer in echobuffer
        int i;
        for (i=0; i < delay; i++) {
            echobuffer[i] = origbuffer[i] / volume_level;
        }

        // Read from source file and store in origbuffer
        if ((nread = fread(origbuffer, sizeof(short), delay, source)) < delay) {
            if (delay > nread) {
                for (i = nread; i < delay; i++) {
                    origbuffer[i] = SHORT0;
                }
            } else {
                perror("fread");
                exit(1);
            }
        } 
      
        // Mix echobuffer and origbuffer by adding corresponding samples together
        // i.e. mix is an array of the sum of each element of echobuffer and corresponding element in origbuffer
        for (i = 0; i < delay; i++) {
            mix[i] = echobuffer[i] + origbuffer[i];
        }

        // Write mix samples to dest
        if ((fwrite(mix, sizeof(short), delay, dest)) != delay) {
            if (ferror(dest)) {
                perror("fwrite");
            } exit(1);
        }

        // Decrement bytecount by sizeof(short) * delay
        bytecount = bytecount - delay * sizeof(short);
    }
    // Add volume_scaled tail to the end of dest file
    int j;
    short tailbuffer[nread];
    for (j = 0; j < nread; j++) {
        tailbuffer[j] = origbuffer[j] / volume_level;
    }

    // Write tail to dest file
    if ((fwrite(tailbuffer, sizeof(short), nread, dest)) != nread) {
        if (ferror(dest)) {
            perror("fwrite");
        } else {
            fprintf(stderr, "Unexpected end of file writing tail\n");
        } exit(1);
    }
}

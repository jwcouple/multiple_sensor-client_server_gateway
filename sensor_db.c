/**
 * \author {Jeffee Hsiung}
 */

#include <stdio.h>
#include <stdlib.h>
#include "sensor_db.h"

#define NO_ERROR "no error"
#define MEMORY_ERROR "mem err" // error  mem alloc failure
#define INVALID_ERROR "invalid err" //error list or sensor null
#define FIFO_PATH "myfifo"
#define MAX_BUFF 1024
#define NEWCSV 'a'
#define APPENDCSV 'b'
#define INSERTED 'c'
#define INSERTFAIL 'd'
#define CSVCLOSED 'e'

int fdw;
int sequence = 0;
char* filename = "sensordata.csv";
bool append = true;

char* writer_create_fifo(char* myfifo){
	// FIFO file path
	printf("strmgr creating fifo \n");
	// creating the named file(FIFO), mkfifo(<pathname>,<permission>)
	mkfifo(myfifo,0666);
	if(mkfifo(myfifo,0666) == -1){if(errno != EEXIST){printf("strmgr could not create fifo \n");}}
	printf("strmgr fifo created \n");
	return myfifo;
}

void writer_open_and_write_fifo(char* myfifo, char* message){
        // open fifo for write only
        if(myfifo != NULL){
                // fifo create succeed
                fdw = open(myfifo,O_WRONLY);
                // take input from user and put to STDIN
		char msg[100];
		time_t t;
		time(&t);
		sprintf(msg, "%d %s %s", sequence,ctime(&t),message);
                printf("strmgr log message: %s \n",msg);
		// write input on fifo and close it
                write(fdw, msg, strlen(msg)+1);
                close(fdw);
		sequence++;
        }else{
                printf("strmgr create fifo failed. exit \n");
                exit(0);
        }
}

//not used
void writer_open_and_read_fifo(char* myfifo){
	// open fifo for read only
	fdw = open(myfifo, O_RDONLY);
	if(fdw > 0){
		//read from fifo succeed
		char array[MAX_BUFF];
		read(fdw, &array, sizeof(array));
		// print to stdout the read message
		printf("logger: %s\n", array);
		close(fdw);
	}
	printf("read fifo failed. exit \n");
	exit(0);
}

FILE* open_db(char* myfifo,char* filename, bool append){
	/* filename of the csv file
	bool: csv file exist, overwritten = false; exist: append = true; */
	FILE* fileptr = fopen(filename, ((append == true)? "a+": "w+"));
	if(append){
		printf("opening to write fifo \n");
		char* message = "log-event: append to csv";
		writer_open_and_write_fifo(myfifo,message);
	}else{char* message = "log-event: csv overwritten";  writer_open_and_write_fifo(myfifo,message);}
	return fileptr;
}

/* append single sensor reading to the csv file */
int insert_sensor(char* myfifo, sensor_id_t id, sensor_value_t value, sensor_ts_t ts){
	FILE* csv = open_db(myfifo, filename, append);
	fprintf(csv,"%hu,%lf,%ld\n", id, value, ts); // %hu, short unsigned int; time_t, %li
	int total_rows = get_total_rows_csv(csv,sizeof(sensor_data_t));
	if (ferror(csv)){
		printf("error writing to file, return 0 \n");
		char* message = "log-event: sensor data insertion failed";
		writer_open_and_write_fifo(myfifo,message);
		return 0;// return 0 means false
	}
	char* message = "log-event: sensor data insertion successful";
        writer_open_and_write_fifo(myfifo,message);
	int err = close_db(myfifo,csv);
	if(err != 0){printf("strmgr closing db failed. \n"); exit(0);}
	return total_rows;
}

/* close the csv file */
int close_db(char* myfifo,FILE* f){
	fclose(f);
	char* message = "log-event: csv closed";
	writer_open_and_write_fifo(myfifo,message);
	return 0;
}

/* calculating the total rows in the csv */
int get_total_rows_csv(FILE* f, int sizeofstruct){
	fseek(f,0,SEEK_END);
	int len = ftell(f); // returns -1L if error
	//fseek(f,0,SEEK_SET); // move the cursor back. not actviated here since append is needed.
	if(len == -1l){ printf("csv length error. set length = 0, return size = 0 \n"); len = 0;}
	int size = len/sizeofstruct;
	return size;
}

/* read sensor data from binary file fprint into a csv file */
int storemgr_parse_sensordata_in_csv(char* myfifo, FILE* openedbinaryfile){
	int insertrows = 0;
	sensor_data_t* sensordata = malloc(sizeof(sensor_data_t)); // heap, be freed
        ERROR_HANDLER(sensordata == NULL, MEMORY_ERROR);
        while(fread(&(sensordata->id), sizeof(sensordata->id), 1, openedbinaryfile)>0){ 
                fread(&(sensordata->value), sizeof(sensordata->value), 1, openedbinaryfile);
                fread(&(sensordata->ts), sizeof(sensordata->ts), 1, openedbinaryfile);
		insertrows = insert_sensor(myfifo,sensordata->id, sensordata->value, sensordata->ts);
		if(insertrows == 0){ printf("store manager parsing data failed \n"); return -1; }
        }
	free(sensordata); //heap, freed
	return insertrows;
}

int writer_get_fd(){
	return fdw;
}
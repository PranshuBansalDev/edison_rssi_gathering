#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/time.h>
#include <unistd.h>

#define BUF_SIZE 256
#define MILLION 1000000.0

typedef unsigned char byte_t;

struct rssi_info {
	int quality;
	int signal;
	uint64_t addr;
};

/*
 * Function:	get_addr
 * ---------------------
 *  Returns: integer representation of MAC address
 *
 *  line_addr: pointer to string in format
 *  	"          Cell %d - Address: %hhX:%hhX:%hhX:%hhX:%hhX:%hhX\n"
 *  len_addr: strlen(line_addr)
 */
uint64_t get_addr(char *line_addr, size_t len_addr)
{
	char *addr;
	char c;
	int i, j, flag;
	byte_t bytes[6];
	uint64_t rv;

	addr = (char*) malloc(sizeof(char)*BUF_SIZE);
	if (addr == NULL)
		perror("");
	memset(addr, 0, BUF_SIZE);

	i = 0; j = 0; flag = 0;
	while(line_addr[i] != '\0') {
		// when the ':' token is found, grab the address
		if (line_addr[i] == ':') {
			flag = 1;
			i+=2;
		}
		while (flag == 1 && line_addr[i] != '\n') {
			addr[j] = line_addr[i];
			i++; j++;
		}
		i++;
	}
	// convert the address from a string to a byte array
	sscanf(addr, "%hhX:%hhX:%hhX:%hhX:%hhX:%hhX",
			&bytes[0], &bytes[1], &bytes[2],
			&bytes[3], &bytes[4], &bytes[5]
			);
	
	// convert the byte array into an integer	
	rv = 0;
	for (i = 0; i < 6; i++) {
		rv |= (((uint64_t) bytes[i]) << (40 - 8*i));
	}
	return rv;
}

/*
 * Function:	process_file
 * -------------------------
 *  Return: array of (struct rssi_info*) objects sorted by RSSI's
 *
 *  Input: FILE *fp
 *  	file contents: output from command
 *  	"iwlist wlan0 scanning | egrep 'Quality|Address' > output.txt"
 */
struct rssi_info** process_file(FILE *fp, int router_count)
{
	char *line_addr, *line_quality;
	size_t len_addr, len_quality;
	ssize_t read_addr, read_quality;
	uint64_t addr;
	int quality, signal, cell;
	int allocation;
	int i;
	struct rssi_info **rssi_arr;
	struct rssi_info *rssi_obj;

	allocation = router_count * sizeof(struct rssi_info);
	printf("Router count: %d\n", router_count);
	rssi_arr = (struct rssi_info**) malloc(allocation);
	
	for (i = 0; i < router_count; i++) {
		len_addr = 0;
		read_addr = getline(&line_addr, &len_addr, fp);
		if (read_addr == -1) return NULL;
		addr = get_addr(line_addr, len_addr);

		len_quality = 0;
		read_quality = getline(&line_quality, &len_quality, fp);
		if (read_quality == -1) return NULL;
		sscanf(line_quality, " Quality=%d/70 Signal level=%d dBm\n",
				&quality, &signal);

		rssi_obj = malloc(sizeof(struct rssi_info));
		rssi_obj->signal = signal;
		rssi_obj->quality = quality;
		rssi_obj->addr = addr;

		rssi_arr[i] = rssi_obj;
	}
	return rssi_arr;	
}

/*
 * Function:	count_routers
 * --------------------------
 *  Returns: number of routers found within file *fp
 *
 *  Input: FILE *fp
 *  	file contents: output from command
 *  	"iwlist wlan0 scanning | egrep 'Quality|Address' > output.txt"
 */
int count_routers(FILE *fp)
{
	char *line;
	size_t len;
	ssize_t read;
	int router_count;

	len = 0;
	router_count = 0;

	while( (read = getline(&line, &len, fp)) != -1 ){
		router_count++;
	}
	router_count = router_count/2;

	fseek(fp, 0L, SEEK_SET);
	return router_count;
}

/*
 * Function:	sort_arr
 * ---------------------
 *  sorts rssi_arr based on 'quality'
 */
void sort_arr(struct rssi_info **rssi_arr, int router_count)
{
	int i, j;
	int cmp_a, cmp_b;
	struct rssi_info *tmp;
	tmp = rssi_arr[0];
	rssi_arr[0] = rssi_arr[router_count-1];
	rssi_arr[router_count-1] = tmp;
	for (i = 0; i < router_count; i++) {
		j = i;
		while ( (j > 0) && 
			(rssi_arr[j-1]->signal	< rssi_arr[j]->signal)
		      ) {
			tmp = rssi_arr[j];
			rssi_arr[j] = rssi_arr[j-1];
			rssi_arr[j-1] = tmp;
			j--;
		}
	}	
}

/*
 * Function:	print_rssi_info
 * ----------------------------
 *  prints the information in the structure rssi_arr
 */
void print_rssi_info(struct rssi_info **rssi_arr, int router_count)
{
	int i;
	struct rssi_info *rssi_obj;
	
	printf("--------------------\n");
	for(i = 0; i < router_count; i++) {
		rssi_obj = rssi_arr[i];
		printf("%020llu: %d/70\t%d dBm\n", 
				rssi_obj->addr,
				rssi_obj->quality,
				rssi_obj->signal
		      );
	}
	printf("--------------------\n");
}


/*
 * function:	write_rssi_to_file
 * -------------------------------
 *  writes to_write (rounded to nearest ciel even number) 
 *  	RSSI information to file specified by the name fname
 *
 *  writes top to_write/2 and bottom to_write/2
 */
void write_rssi_to_file(struct rssi_info **rssi_arr, int router_count,
		int to_write, const char *fname)
{
	FILE *fp;
	int i;

	to_write += to_write%2;

	if (to_write > router_count) {
		fprintf(stderr, 
			"There are less routers discovered than requested."
		       	"Terminating program.\n"
		       );
		exit(EXIT_FAILURE);
	}

	fp = fopen(fname, "w");
	if (fp == NULL) {
		fprintf(stderr, "Failed to open file \'%s\'. Exiting.\n",
				fname
		       );
		exit(EXIT_FAILURE);
	}

	for (i = 0; i < to_write; i++) {
		fprintf(fp, "%llu,%d,%d\n",
				rssi_arr[i]->addr,
				rssi_arr[i]->signal,
				rssi_arr[i]->quality
		       );
	}
	for (i = (router_count-to_write); i < router_count; i++) {
		fprintf(fp, "%llu,%d,%d\n",
				rssi_arr[i]->addr,
				rssi_arr[i]->signal,
				rssi_arr[i]->quality
		       );
	}
	fclose(fp);
}

int main()
{
	char command[BUF_SIZE];
	char fname[BUF_SIZE];
	FILE *fp;
	int router_count;
	int i;
	struct rssi_info **rssi_arr;
	struct timeval start, end;
	double start_epoch, end_epoch;

	memset(fname, 0, sizeof(fname));
	memset(command, 0, sizeof(command));

	snprintf(fname, sizeof(fname), "output.txt");
	snprintf(command, sizeof(command), 
		"iwlist wlan0 scanning | egrep \'Quality|Address\' > %s", fname
		);

	printf("Executing command \'%s\'\n", command);
	
	gettimeofday(&start, NULL);
	system(command);
	gettimeofday(&end, NULL);

	fp = fopen(fname, "r");
	if (fp == NULL) {
		fprintf(stderr, "Error opening file \'%s\'. Terminating.\n", 
				fname);
		exit(EXIT_FAILURE);
	}

	router_count = count_routers(fp);
	rssi_arr = process_file(fp, router_count);
	fclose(fp);
	
	sort_arr(rssi_arr, router_count);
	print_rssi_info(rssi_arr, router_count);
	write_rssi_to_file(rssi_arr, 
			router_count, 
			router_count/2, 
			"rssi_agg.csv");

	start_epoch = start.tv_sec + start.tv_usec/MILLION;
	end_epoch = end.tv_sec + end.tv_usec/MILLION;
	printf("Time taken for command \'%s\' to return: %lf\n", 
			command,
			end_epoch - start_epoch);

	printf("Data has been stored in \'%s\'.\n", "rssi_agg.csv");
	printf("get_rssi has completed successfully. Exiting.\n");
	exit(EXIT_SUCCESS);
}

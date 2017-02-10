all: get_rssi

get_rssi: get_rssi.c
	gcc -o get_rssi get_rssi.c

clean:
	rm -f get_rssi

clobber:
	rm -f *~ get_rssi *.csv *.txt

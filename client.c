#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>				//serial port handling
#include <poll.h>					//polling mode

#define SERIAL_PATH "/dev/ttyACM0"
#define BAUD B19200					//B necessary for termios

int main(void) {
	//open serial
	int fd = open(SERIAL_PATH, O_RDWR | O_NOCTTY);
	if (fd == -1) {
		perror("error opening serial");
		return EXIT_FAILURE;
	}

	//termios
	//get attributes from serial
	struct termios tty;
	if (tcgetattr(fd, &tty) != 0) {
		perror("tcgetattr");
		close(fd);
		return EXIT_FAILURE;
	}
	//set baudrate
	cfsetispeed(&tty, BAUD);
	cfsetospeed(&tty, BAUD);
	//8 bit transmission unit
	tty.c_cflag |= CS8;
	//no parity
	tty.c_cflag &= ~PARENB;
	//1 stop bit
	tty.c_cflag &= ~CSTOPB;

	//set attributes immediately
	if (tcsetattr(fd, TCSANOW, &tty) != 0) {
		perror("tcsetattr");
		close(fd);
		return EXIT_FAILURE;
	}

	//poll mode to wait for some I/O event on a fd
	//2 fds to listen: stdin and serial
	struct pollfd fds[2];
	fds[0].fd = STDIN_FILENO;
	fds[0].events = POLLIN;

	fds[1].fd = fd;
	fds[1].events = POLLIN;


	//flush prevoius unwanted data
	tcflush(fd, TCIFLUSH);
	//sleep to let arduino start and send welcome message
	sleep(2);

	char buffer[1024];
	while (1) {
		//poll the fds and wait until something happens
		int ret = poll(fds, 2, -1);
		if (ret < 0) {
			perror("poll");
			break;
		}

		//input from keyboard (stdin)
		//if an input event has happened (cmd sent), get it and write it on serial
		if (fds[0].revents & POLLIN) {
			if (fgets(buffer, sizeof(buffer), stdin) != NULL) {
				if (buffer[0] == 'q') break;
				write(fd, buffer, strlen(buffer));
			}
		}

		//input from serial (arduino)
		//if an input event has happened (msg recieved), get it and print it on screen
		if (fds[1].revents & POLLIN) {
			int n = read(fd, buffer, sizeof(buffer) - 1);
			if (n > 0) {
				buffer[n] = '\0';
				printf("%s", buffer);
				fflush(stdout);
			}
		}
	}
	//close the serial when finished
	close(fd);
	return 0;
}

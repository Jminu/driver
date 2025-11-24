#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/shm.h>
#include <sys/ipc.h>

int main(void) {
	int fd_sensor;
	int fd_lcd;
	int fd_btn;
	char buf[32];
	pid_t pid;

	int shmid;
	char *shmaddr_p = NULL;
	char *shmaddr_c = NULL;

	shmid = shmget(IPC_PRIVATE, sizeof(char), IPC_CREAT|0666);
	if (shmid < 0) {
		perror("shmget error\n");
		return -1;
	}

	fd_lcd = open("/dev/hd44780_device", O_WRONLY);
	if (fd_lcd < 0) {
		perror("sht20 open error\n");
		return -1;
	}

	fd_sensor = open("/dev/sht20_device", O_RDONLY);
	if (fd_sensor < 0) {
		perror("lcd open error\n");
		return -1;
	}

	fd_btn = open("/dev/button_device", O_RDONLY);
	if (fd_btn < 0) {
		perror("button device open error\n");
		return -1;
	}



	pid = fork();
	if (pid == 0) { // child -> wait of button interrupt
		shmaddr_c = (char *)shmat(shmid, NULL, 0);
		*shmaddr_c = '0';
		char system_mode[1];

		while (1) {
			int len = read(fd_btn, system_mode, 1);
			printf("child : len = %d, system_mode: %c\n", len, system_mode[0]);
			if (system_mode[0] == '0') {
				*shmaddr_c = '0';
			}
			else if (system_mode[0] == '1') {
				*shmaddr_c = '1';
			}
		}
	}
	else if (pid > 0) { // parent -> read sensor and write LCD
		shmaddr_p = (char *)shmat(shmid, NULL, 0);
		*shmaddr_p = '0';

		while (1) {
			int len = read(fd_sensor, buf, sizeof(buf) - 1);
			if (len < 0) {
				perror("read error\n");
				return -1;
			}
			buf[len] = '\0';
			char *tok = strtok(buf, "|");
			int humid_raw; // 원본 습도/온도 데이터 (변환 전)
			int temp_raw;

			if (tok != NULL) {
				temp_raw = atoi(tok);
			}
			tok = strtok(NULL, "|");
			if (tok != NULL) {
				humid_raw = atoi(tok);
			}

			double temp_val = -46.85 + 175.72 * ((double)temp_raw / 65536.0);
			double humid_val = -6 + 125 * ((double)humid_raw / 65536.0);

			int temp = temp_val;
			int humid = humid_val;

			char temp_str[10];
			char humid_str[10];

			snprintf(temp_str, sizeof(temp_str), "Temp: %d", temp);
			snprintf(humid_str, sizeof(humid_str), "Humid: %d", humid);

			if (len > 0) {
				if (*shmaddr_p == '0') {
					printf("%s\n", temp_str);
					write(fd_lcd, temp_str, strlen(temp_str));
				}
				else if (*shmaddr_p == '1') {
					printf("%s\n", humid_str);
					write(fd_lcd, humid_str, strlen(humid_str));
				}
			}
			sleep(1);
		}
	}
	else {
		perror("fork error\n");
		return -1;
	}

	close(fd_sensor);
	close(fd_lcd);
	close(fd_btn);

	return 0;
}

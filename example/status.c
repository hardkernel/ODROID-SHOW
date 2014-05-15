#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <sys/utsname.h>
#include <sys/statvfs.h>

#include <math.h>

#include <fcntl.h>
#include <termios.h>
#include <errno.h>

#define baudrate	B500000

#define String_startsWith(s, match) (strstr((s), (match)) == (s))

const char procstat[] = "/proc/stat";
const char serialPort[] = "/dev/ttyUSB0";
const char mountPath[] = "/";

typedef struct CPUData_ {
	unsigned long long int totalTime;
	unsigned long long int userTime;

	unsigned long long int totalPeriod;
	unsigned long long int userPeriod;
} CPUData;

struct diskData_ {
	unsigned long disk_size;
	unsigned long used;
	unsigned long free;
};

int serialSetup(void)
{
	int usbdev;
	struct termios options;

	usbdev = open(serialPort, O_RDWR | O_NOCTTY | O_NDELAY);

	if (usbdev == -1)
		perror("open_port : Unable to open:");

	tcgetattr(usbdev, &options);

	cfsetispeed(&options, baudrate);
	cfsetospeed(&options, baudrate);

	options.c_cflag |= CS8;
	options.c_iflag |= IGNBRK;
	options.c_iflag &= ~( BRKINT | ICRNL | IMAXBEL | IXON);
	options.c_oflag &= ~( OPOST | ONLCR );
	options.c_lflag &= ~( ISIG | ICANON | IEXTEN | ECHO | ECHOE | ECHOK |
							ECHOCTL | ECHOKE);
	options.c_lflag |= NOFLSH;
	options.c_cflag &= ~CRTSCTS;

	tcsetattr(usbdev, TCSANOW, &options);

	return usbdev;
}

struct diskData_ diskSpace(const char *Path)
{
	struct statvfs vfs;
	struct diskData_ diskData;
	if (!statvfs(Path, &vfs)) {
		diskData.disk_size = vfs.f_blocks * vfs.f_bsize;
		diskData.free = vfs.f_bfree * vfs.f_bsize;
		diskData.used = diskData.disk_size - diskData.free;
	}
	return diskData;
}

int cpuCount(char *buf, int cpus)
{
	FILE *file = fopen(procstat, "r");
	assert(file != NULL);

	do {
		cpus++;
		fgets(buf, 255, file);
	} while (String_startsWith(buf, "cpu"));

	fclose(file);

	return cpus;
}

void systemInfo(int fd, char *buf)
{
	time_t t;
	struct utsname uts;
	time(&t);
	sprintf(buf, "\e[35m%s\r", ctime(&t));
	write(fd, buf, strlen(buf) + 1);
	usleep(300000);
	uname(&uts);
	sprintf(buf, "\e[37mOSname:\e[36m%s\n\r", uts.sysname);
	write(fd, buf, strlen(buf) + 1);
	sprintf(buf, "\e[37mVersion:\e[36m%s\n\r", uts.release);
	write(fd, buf, strlen(buf) + 1);
	sprintf(buf, "\e[37mMachine:\e[36m%s\n\r", uts.machine);
	write(fd, buf, strlen(buf) + 1);
}

int main(void)
{
	unsigned long long int usertime, nicetime, systemtime, systemalltime,
				idlealltime, idletime, totaltime, virtalltime;
	double total = 0;
	char buffer[256];
	int cpus = -1;
	int i;
	time_t t;
	struct utsname uts;
	int usbdev;
	usbdev = serialSetup();
	FILE *file;
	const unsigned int GB = 1024*1024*1024;
	struct diskData_ diskData;
	diskData = diskSpace(mountPath);
	diskData.disk_size /= GB;
	diskData.used /= GB;
	diskData.free /= GB;
//	printf("Disk usage : %lu \t Free space %lu,%lu\n", diskData.disk_size/GB, diskData.free/GB, diskData.used/GB);
//	return 0;

	cpus = cpuCount(buffer, cpus);
	CPUData cpuData[cpus];

	for (i = 0; i < cpus; i++) {
		cpuData[i].totalTime = 1;
		cpuData[i].userTime = 0;
		cpuData[i].totalPeriod = 1;
		cpuData[i].userPeriod = 0;
	}

	sprintf(buffer, "\ec\e[2s\e[1r");
	write(usbdev, buffer, strlen(buffer) + 1);
	fsync(usbdev);

	int cpuid;
	unsigned long long int ioWait, irq, softIrq, steal, guest;
	ioWait = irq = softIrq = steal = guest = 0;

	while (1) {
		sprintf(buffer, "\e[H");
		write(usbdev, buffer, strlen(buffer) + 1);
		fsync(usbdev);
		usleep(300000);
		systemInfo(usbdev, buffer);
		fsync(usbdev);
		usleep(300000);
		file = fopen(procstat, "r");
		for (i = 0; i < cpus; i++) {
			fgets(buffer, 255, file);
			if (i == 0) {
				sscanf(buffer, "cpu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
					&usertime, &nicetime, &systemtime, &idletime,
					&ioWait, &irq, &softIrq, &steal, &guest);
			} else {
				sscanf(buffer, "cpu%d %llu %llu %llu %llu %llu %llu %llu %llu %llu",
					&cpuid, &usertime, &nicetime, &systemtime, &idletime,
					&ioWait, &irq, &softIrq, &steal, &guest);
				assert(cpuid == i - 1);
			}

			idlealltime = idletime + ioWait;
			systemalltime = systemtime + irq + softIrq;
			virtalltime = steal + guest;
			totaltime = usertime + nicetime + systemalltime +
						idlealltime + virtalltime;

			assert(usertime >= cpuData[i].userTime);
			assert(totaltime >= cpuData[i].totalTime);

			cpuData[i].userPeriod = usertime - cpuData[i].userTime;
			cpuData[i].totalPeriod = totaltime - cpuData[i].totalTime;

			cpuData[i].totalTime = totaltime;
			cpuData[i].userTime = usertime;

			total = (double)cpuData[i].totalPeriod;
			if ((i != 0) && (i%2 == 1)) {	
				sprintf(buffer, "\e[33mcpu%d :\e[32m%4.1f%% ",
					i, cpuData[i].userPeriod/total*100.0);
				write(usbdev, buffer, strlen(buffer) + 1);
				fsync(usbdev);
			} else if ((i != 0) && (i%2 == 0)) {
				sprintf(buffer, "\e[33mcpu%d :\e[32m%4.1f%%  \n\r",
					i, cpuData[i].userPeriod/total*100.0);
				write(usbdev, buffer, strlen(buffer) + 1);
				fsync(usbdev);
			}
		}
		sprintf(buffer, "\e[37msda1\n\r\e[33mdisk_size : \e[32m%ldGB\n\r",
							diskData.disk_size);
		write(usbdev, buffer, strlen(buffer) + 1);
		usleep(100000);
		sprintf(buffer, "\e[35mused : \e[32m%ldGB\n\r",
							diskData.used);
		write(usbdev, buffer, strlen(buffer) + 1);
		usleep(100000);
		sprintf(buffer, "\e[36mfree : \e[32m%ldGB",
							diskData.free);
		write(usbdev, buffer, strlen(buffer) + 1);
		usleep(100000);
		fclose(file);
	}
	
	return 0;
}

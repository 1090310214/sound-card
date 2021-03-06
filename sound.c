
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <stdio.h>
#include <linux/soundcard.h>

#define LENGTH 3    
#define RATE 9000   
#define SIZE 8      
#define CHANNELS 1  

unsigned char buf[LENGTH*RATE*SIZE*CHANNELS/8];

int main()
{
  int fd;
  int arg;	
  int status;   

  fd = open("/dev/dsp", O_RDWR);
  if (fd < 0) {
    perror("open of /dev/dsp failed");
    exit(1);
  }

  arg = SIZE;
  status = ioctl(fd, SOUND_PCM_WRITE_BITS, &arg);
  if (status == -1)
    perror("SOUND_PCM_WRITE_BITS ioctl failed");
  if (arg != SIZE)
    perror("unable to set sample size");

  arg = CHANNELS; 
  status = ioctl(fd, SOUND_PCM_WRITE_CHANNELS, &arg);
  if (status == -1)
    perror("SOUND_PCM_WRITE_CHANNELS ioctl failed");
  if (arg != CHANNELS)
    perror("unable to set number of channels");

  arg = RATE;
  status = ioctl(fd, SOUND_PCM_WRITE_RATE, &arg);
  if (status == -1)
    perror("SOUND_PCM_WRITE_WRITE ioctl failed");

  while (1) {
    printf("Say something:\n");
    status = read(fd, buf, sizeof(buf)); 
    if (status != sizeof(buf))
      perror("read wrong number of bytes");

    printf("You said:\n");
    status = write(fd, buf, sizeof(buf)); 
    if (status != sizeof(buf))
      perror("wrote wrong number of bytes");
    status = ioctl(fd, SOUND_PCM_SYNC, 0); 
    if (status == -1)
      perror("SOUND_PCM_SYNC ioctl failed");
  }
}

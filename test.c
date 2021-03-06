/* Use the newer ALSA API */
//扬声器
#define ALSA_PCM_NEW_HW_PARAMS_API

#include <alsa/asoundlib.h>


/* this buffer holds the digitized audio */
//unsigned char buf[LENGTH*RATE*SIZE*CHANNELS/8];
typedef unsigned char   BYTE;
typedef unsigned short  WORD;
typedef unsigned int  DWORD;
typedef unsigned int  FOURCC;    /* a four character code */

/* flags for 'wFormatTag' field of WAVEFORMAT */
#define WAVE_FORMAT_PCM 1

/* MMIO macros */
#define mmioFOURCC(ch0, ch1, ch2, ch3) \
  ((DWORD)(BYTE)(ch0) | ((DWORD)(BYTE)(ch1) << 8) | \
  ((DWORD)(BYTE)(ch2) << 16) | ((DWORD)(BYTE)(ch3) << 24))

#define FOURCC_RIFF    mmioFOURCC ('R', 'I', 'F', 'F')
#define FOURCC_LIST    mmioFOURCC ('L', 'I', 'S', 'T')
#define FOURCC_WAVE    mmioFOURCC ('W', 'A', 'V', 'E')
#define FOURCC_FMT    mmioFOURCC ('f', 'm', 't', ' ')
#define FOURCC_DATA    mmioFOURCC ('d', 'a', 't', 'a')

typedef struct CHUNKHDR {
    FOURCC ckid;        /* chunk ID */
    DWORD dwSize;             /* chunk size */
} CHUNKHDR;

/* simplified Header for standard WAV files */
typedef struct WAVEHDR {
    CHUNKHDR chkRiff;
    FOURCC fccWave;
    CHUNKHDR chkFmt;
    WORD wFormatTag;       /* format type */
    WORD nChannels;       /* number of channels (i.e. mono, stereo, etc.) */
    DWORD nSamplesPerSec;  /* sample rate */
    DWORD nAvgBytesPerSec; /* for buffer estimation */
    WORD nBlockAlign;       /* block size of data */
    WORD wBitsPerSample;
    CHUNKHDR chkData;
} WAVEHDR;

#if BYTE_ORDER == BIG_ENDIAN
# define cpu_to_le32(x) SWAP4((x))
# define cpu_to_le16(x) SWAP2((x))
# define le32_to_cpu(x) SWAP4((x))
# define le16_to_cpu(x) SWAP2((x))
#else
# define cpu_to_le32(x) (x)
# define cpu_to_le16(x) (x)
# define le32_to_cpu(x) (x)
# define le16_to_cpu(x) (x)
#endif

static void wav_init_header(WAVEHDR *fileheader)
{
    /* stolen from cdda2wav */
    int nBitsPerSample = 16;
    int channels       = 1;
    int rate           = 8000;

    unsigned long nBlockAlign = channels * ((nBitsPerSample + 7) / 8);
    unsigned long nAvgBytesPerSec = nBlockAlign * rate;
    unsigned long temp = /* data length */ 0 +
    sizeof(WAVEHDR) - sizeof(CHUNKHDR);

    fileheader->chkRiff.ckid    = cpu_to_le32(FOURCC_RIFF);
    fileheader->fccWave         = cpu_to_le32(FOURCC_WAVE);
    fileheader->chkFmt.ckid     = cpu_to_le32(FOURCC_FMT);
    fileheader->chkFmt.dwSize   = cpu_to_le32(16);
    fileheader->wFormatTag      = cpu_to_le16(WAVE_FORMAT_PCM);
    fileheader->nChannels       = cpu_to_le16(channels);
    fileheader->nSamplesPerSec  = cpu_to_le32(rate);
    fileheader->nAvgBytesPerSec = cpu_to_le32(nAvgBytesPerSec);
    fileheader->nBlockAlign     = cpu_to_le16(nBlockAlign);
    fileheader->wBitsPerSample  = cpu_to_le16(nBitsPerSample);
    fileheader->chkData.ckid    = cpu_to_le32(FOURCC_DATA);
    fileheader->chkRiff.dwSize  = cpu_to_le32(temp);
    fileheader->chkData.dwSize  = cpu_to_le32(0 /* data length */);
}

static void wav_start_write(FILE* fd, WAVEHDR *fileheader)
{
    wav_init_header(fileheader);
    fwrite(fileheader,1, sizeof(WAVEHDR), fd);
}

static void wav_stop_write(FILE* fd, WAVEHDR *fileheader, int wav_size)
{
    unsigned long temp = wav_size + sizeof(WAVEHDR) - sizeof(CHUNKHDR);

    fileheader->chkRiff.dwSize = cpu_to_le32(temp);
    fileheader->chkData.dwSize = cpu_to_le32(wav_size);
    fseek(fd,0,SEEK_SET);
       fwrite(fileheader,1, sizeof(WAVEHDR), fd);
}


int main()
{
    long loops;
    int rc;
    int size;
    snd_pcm_t *rec_handle;
    snd_pcm_hw_params_t *rec_params;
    unsigned int val;
    int dir;
    snd_pcm_uframes_t frames;
    char *buffer;
    WAVEHDR wavheader;
    int total_len = 0;
    FILE *fp_rec = fopen("rec.wav", "wb");
    if(fp_rec==NULL)
    {
        return 0;
    }
    wav_start_write(fp_rec, &wavheader);
    /* Open PCM device for recording (capture). */
    rc = snd_pcm_open(&rec_handle, "default",
                        SND_PCM_STREAM_CAPTURE, 0);
    if (rc < 0) {
        fprintf(stderr,
                "unable to open pcm device: %s\n",
                snd_strerror(rc));
        exit(1);
    }

    /* Allocate a hardware parameters object. */
    snd_pcm_hw_params_alloca(&rec_params);

    /* Fill it in with default values. */
    snd_pcm_hw_params_any(rec_handle, rec_params);

    /* Set the desired hardware parameters. */

    /* Interleaved mode */
    snd_pcm_hw_params_set_access(rec_handle, rec_params,
                          SND_PCM_ACCESS_RW_INTERLEAVED);

    /* Signed 16-bit little-endian format */
    snd_pcm_hw_params_set_format(rec_handle, rec_params,
                                  SND_PCM_FORMAT_S16_LE);

    /* Two channels (stereo) */
    snd_pcm_hw_params_set_channels(rec_handle, rec_params, 1);

    /* 44100 bits/second sampling rate (CD quality) */
    val = 8000;
    snd_pcm_hw_params_set_rate_near(rec_handle, rec_params,
                                      &val, &dir);

    /* Set period size to 32 frames. */
    frames = 32;
    snd_pcm_hw_params_set_period_size_near(rec_handle,
                                  rec_params, &frames, &dir);

    /* Write the parameters to the driver */
    rc = snd_pcm_hw_params(rec_handle, rec_params);
    if (rc < 0)
    {
        fprintf(stderr,
                "unable to set hw parameters: %s\n",
                snd_strerror(rc));
        exit(1);
    }

    /* Use a buffer large enough to hold one period */
    snd_pcm_hw_params_get_period_size(rec_params,  &frames, &dir);
    size = frames * 2; /* 2 bytes/sample, 2 channels */
    buffer = (char *) malloc(size);

    /* We want to loop for 5 seconds */
    snd_pcm_hw_params_get_period_time(rec_params, &val, &dir);
    loops = 2000;

    while (loops-- > 0)
    {
        rc = snd_pcm_readi(rec_handle, buffer, frames);
        if (rc == -EPIPE)
        {
          /* EPIPE means overrun */
          fprintf(stderr, "overrun occurred\n");
          snd_pcm_prepare(rec_handle);
        }
        else if (rc < 0)
        {
          fprintf(stderr, "error from read: %s\n", snd_strerror(rc));
        }
        else if (rc != (int)frames)
        {
          fprintf(stderr, "short read, read %d frames\n", rc);
        }
        rc = fwrite(buffer, 1, size, fp_rec);
        total_len += size;
        printf("#\n");
        if (rc != size)
          fprintf(stderr,"short write: wrote %d bytes\n", rc);
    }
    wav_stop_write(fp_rec, &wavheader, total_len);
    snd_pcm_drain(rec_handle);
    snd_pcm_close(rec_handle);
    free(buffer);
    fclose(fp_rec);
    return 0;
}





//录音
//#include < fcntl.h >
#include < stdio.h >
#include < stdlib.h >
#include < string.h >
#include < unistd.h >
#include < sys/ioctl.h >
#include < sys/types.h >
#include < linux/kd.h >

#define DEFAULT_FREQ 440 
#define DEFAULT_LENGTH 200 
#define DEFAULT_REPS 1 
#define DEFAULT_DELAY 100 

typedef struct {
int freq; /* 我们期望输出的频率 */
int length; /* 发声长度*/
int reps; /* 重复的次数*/
int delay; 
} beep_parms_t;

void usage_bail ( const char *executable_name ) {
printf ( "Usage: \n \t%s [-f frequency] [-l length] [-r reps] [-d delay] \n ",
executable_name );
exit(1);
}

void parse_command_line(char **argv, beep_parms_t *result) {
char *arg0 = *(argv++);
while ( *argv ) {
if ( !strcmp( *argv,"-f" )) { /*频率*/
int freq = atoi ( *( ++argv ) );
if ( ( freq <= 0 ) | | ( freq > 10000 ) ) {
fprintf ( stderr, "Bad parameter: frequency must be from 1..10000\n" );
exit (1) ;
} else {
result->freq = freq;
argv++;
}
} else if ( ! strcmp ( *argv, "-l" ) ) { /*时长*/
int length = atoi ( *(++argv ) );
if (length < 0) {
fprintf(stderr, "Bad parameter: length must be >= 0\n");
exit(1);
} else {
result->length = length;
argv++;
}
} else if (!strcmp(*argv, "-r")) { /*重复次数*/
int reps = atoi(*(++argv));
if (reps < 0) {
fprintf(stderr, "Bad parameter: reps must be >= 0\n");
exit(1);
} else {
result->reps = reps;
argv++;
}
} else if (!strcmp(*argv, "-d")) { /* 延时 */
int delay = atoi(*(++argv));
if (delay < 0) {
fprintf(stderr, "Bad parameter: delay must be >= 0\n");
exit(1);
} else {
result->delay = delay;
argv++;
}
} else {
fprintf(stderr, "Bad parameter: %s\n", *argv);
usage_bail(arg0);
}
}
}

int main(int argc, char **argv) {
int console_fd;
int i; 

beep_parms_t parms = {DEFAULT_FREQ, DEFAULT_LENGTH, DEFAULT_REPS,
DEFAULT_DELAY};

parse_command_line(argv, &parms);


if ( ( console_fd = open ( "/dev/console", O_WRONLY ) ) == -1 ) {
fprintf(stderr, "Failed to open console.\n");
perror("open");
exit(1);
}


for (i = 0; i < parms.reps; i++) {

int magical_fairy_number = 1190000/parms.freq;

ioctl(console_fd, KIOCSOUND, magical_fairy_number); /* 开始发声 */
usleep(1000*parms.length); 
ioctl(console_fd, KIOCSOUND, 0); /* 停止发声*/
usleep(1000*parms.delay); 
} /* 重复播放*/
return EXIT_SUCCESS;
}
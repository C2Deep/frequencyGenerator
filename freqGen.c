// Name        : Sound Waves Generator.
// Description : Generate different types of sound waves through the default speaker of your system
//                             - Generate audible frequencies in different types ( sine , square, triangualr, sawtooth).
//                             - Adjust frequency, phase , volume, and the wave type of each channel independently.
//

#include<stdio.h>
#include<string.h>
#include<sys/ioctl.h>
#include<sound/asound.h>
#include<unistd.h>
#include<fcntl.h>
#include<math.h>
#include<poll.h>
#include<limits.h>
#include<pthread.h>
#include<stdbool.h>
#include<ctype.h>
#include<termios.h>
#include<stdlib.h>

#define CHANNELS        2
#define PERIODS         32
#define PERIOD_SIZE     256
#define RATE            44100   // Sample rate
#define BUFFER_TIME     50000   // 50 ms
#define MAX_DEVICE_NAME    256
#define MAX_FILE_PATH      256

#define f_FREQ      "Set channel1 (LEFT) frequency : "
#define F_FREQ      "Set channel2 (RIGHT) frequency : "
#define p_PHASE     "Set channel1 (LEFT) phase : "
#define P_PHASE     "Set channel2 (RIGHT) phase : "
#define v_VOL       "Set channel1 (LEFT) volume : "
#define V_VOL       "Set channel2 (RIGHT) volume : "

// INPUT_RESET() only used in Tspeaker_CTRL()
#define INPUT_RESET()     fp = 1;\
                        decPoint = 1;\
                        i = 0

unsigned int Grate = RATE;                                                         // Default sample rate
unsigned int GbufferTime = BUFFER_TIME;                                            // Default buffer time (latency) in us
unsigned int GbufferSize = BUFFER_TIME * (RATE * 0.000001);                          // Default buffer size (frames)
unsigned int GperiodSize = (BUFFER_TIME * (RATE * 0.000001))/16;                     // It's NOT included with hardware parameters settings

enum { SINE_WAVE, SQUARE_WAVE, TRIANGULAR_WAVE, SAWTOOTH_WAVE};
char *GwaveTypes[4] = { "SINE", "SQUARE", "TRIANGULAR", "SAWTOOTH"};
double GmaxPhase = 2.0 * M_PI;

double Gfreq[CHANNELS]  = {101.0, 101.0};   // Default frequencies for both channels
double Gvol[CHANNELS]   = {0.37, 0.37};     // Default volumes for both channels
double Gphase[CHANNELS] = {0.0, 0.0};       // Defaule phases for both channels

int GwaveType[CHANNELS] = {SINE_WAVE, SINE_WAVE};     // Default wave types for both channels
int GvolContactor[CHANNELS];
int GvolContactee[CHANNELS];
int GphaseShift[CHANNELS];

unsigned int GfFreqLen = strlen(f_FREQ);
unsigned int GFFreqLen = strlen(F_FREQ);
unsigned int GpPhaseLen = strlen(p_PHASE);
unsigned int GPPhaseLen = strlen(P_PHASE);
unsigned int GvVolLen = strlen(v_VOL);
unsigned int GVVolLen = strlen(V_VOL);

bool   Gexit = false;               // Exit the write_loop

void set_hwparams(int pcmFD);        // Setting hardware parameters
void set_swparams(int pcmFD);        // Setting software parameters
void sound_generator(int pcmFD);    // Generate sound
void *Tspeaker_CTRL(void *);        // [Thread function] Speaker Control
void terminal_CTRL(int state);      // Terminal control
void phase_shift(void);             // Sound wave phase shift
void update_info(void);             // Information update in terminal
void clear_line(char fp);           // Clear terminal line
void write_frames(int pcmFD, short *ptr, snd_pcm_uframes_t cPtr);   // Write frames to speaker
void wave_generator(int chn, short *buf, snd_pcm_uframes_t frames); // Generate wave pattern
void back_space(void);              // Back space in terminal

int main(int argc, char *argv[])
{
    int pcmFD = 0;

    snd_pcm_sframes_t delay = 1;

    struct snd_pcm_status sndPcmStatus;
    struct snd_pcm_sw_params sndPcmSwParams;

    pthread_t threadID;

    int c;

    while((c = getopt(argc, argv, "r:l:f:F:v:V:w:W:")) != -1)
    {
        switch(c)
        {
            // Initializing the Sample rate
            case 'r':
                Grate = atoi(optarg);
                GbufferTime = ((double)GbufferSize / (double) Grate) * 1000000.0;   // Default buffer time (latency) in us
                printf("Sample rate = %d\n", Grate);
                break;

            // Initializing the Buffer time in micro seconds (Latency)
            case 'l':
                GbufferTime = atoi(optarg);
                GbufferSize = GbufferTime * Grate * 0.000001;
                GperiodSize = GbufferSize/16;
/*
                printf("GbufferTime = %duSec\n", GbufferTime);
                printf("GbufferTime * Grate = %d\n", GbufferTime * Grate);
                printf("GbufferSize = %d frames\n", GbufferSize);*/
                break;

            // Initialize channel 1 (Left) frequency
            case 'f':
                Gfreq[0] = atoi(optarg) % (Grate/2);
                break;
            // Initialize channel 2 (Right) frequency
            case 'F':
                Gfreq[1] = atoi(optarg) % (Grate/2);
                break;
            // Initialize channel 1 (Left)Gvol volume
            case 'v':
                Gvol[0] = (atoi(optarg) % 101) / 100.0;
                break;
            // Initialize channel 2 (Right) volume
            case 'V':
                Gvol[1] = (atoi(optarg) % 101) / 100.0;
                break;
            // Initialize channel 1 (Left) wave type
            case 'w':
                GwaveType[0] = atoi(optarg) % 4;
                break;
            // Initialize channel 2 (Right) wave Type
            case 'W':
                GwaveType[1] = atoi(optarg) % 4;
                break;
        }

    }

    if((pcmFD = open("/dev/snd/pcmC0D0p", O_RDWR)) < 0)
    {
        fprintf(stderr, "Couldn't open /dev/snd/pcmC0D0p file\n");
        perror(NULL);
        return -1;
    }

    set_hwparams(pcmFD);
    set_swparams(pcmFD);

    // Prepare device
    ioctl(pcmFD, SNDRV_PCM_IOCTL_PREPARE);

    // Thread to control the speaker frequency, phase and the volume
    pthread_create(&threadID, NULL, Tspeaker_CTRL, NULL);

    // Generate sound
    sound_generator(pcmFD);
    pthread_join(threadID, NULL);

    ioctl(pcmFD, SNDRV_PCM_IOCTL_DRAIN);
    close(pcmFD);

    return 0;
}

// Name             : set_hwparams
// Parameters       : pcmFD > PCM playback file descriptor
// Call             : memset(), sizeof(), ioctl(), fprintf(), perror(), exit()
// Called by        : main()
// Return           : void
// Description      : Set PCM hardware parameters configuration
void set_hwparams(int pcmFD)
{

    // double periodTime = ((double) PERIOD_SIZE / (double) Grate) * 1000000.0;   // in us

    struct snd_pcm_hw_params sndPcmHwParams;

    // Zero out the structure
    memset(&sndPcmHwParams, 0, sizeof(struct snd_pcm_hw_params));

    sndPcmHwParams.masks[0].bits[0] = 1U << SNDRV_PCM_ACCESS_RW_INTERLEAVED;     // Access type
    sndPcmHwParams.masks[1].bits[0] = 1U << SNDRV_PCM_FORMAT_S16_LE;             // Format
    sndPcmHwParams.masks[2].bits[0] = 1U << SNDRV_PCM_SUBFORMAT_STD;             // Subformat

    //  Reserved masks
    for(int i = 0 ; i < 5 ; i++)
        sndPcmHwParams.mres[i].bits[0] = -1;

    // Sample Size (bits)
    sndPcmHwParams.intervals[0].min = 16;       // 16-bit singed sample
    sndPcmHwParams.intervals[0].max = 16;       // 1 sample = 2 bytes

    // Frame size (bits)
    sndPcmHwParams.intervals[1].max = CHANNELS * sizeof(int16_t) * 8;   // Frame bits => 1 frame = (num_channels) * (1 sample size in bytes)
    sndPcmHwParams.intervals[1].min = CHANNELS * sizeof(int16_t) * 8;   // (2 channels) * (2 bytes (16 bits) per sample) = 4 bytes (32 bits)

    // Number of channels
    sndPcmHwParams.intervals[2].min = CHANNELS;
    sndPcmHwParams.intervals[2].max = CHANNELS;

    // Sampling rate
    sndPcmHwParams.intervals[3].min = Grate;
    sndPcmHwParams.intervals[3].max = Grate;

    // Period time (us)
    sndPcmHwParams.intervals[4].min = 0;    //periodTime;
    sndPcmHwParams.intervals[4].max = -1;   //periodTime + 1;

    // Period size (frames)
    sndPcmHwParams.intervals[5].min = 0;    //PERIOD_SIZE;
    sndPcmHwParams.intervals[5].max = -1;   //PERIOD_SIZE;

    // Period size (bytes)
    sndPcmHwParams.intervals[6].min = 0;    //PERIOD_SIZE*CHANNELS*sizeof(int16_t);
    sndPcmHwParams.intervals[6].max = -1;   //PERIOD_SIZE*CHANNELS*sizeof(int16_t);

    // Periods
    sndPcmHwParams.intervals[7].min = 0;    //PERIODS;
    sndPcmHwParams.intervals[7].max = -1;   //PERIODS;

    // Buffer time (us)
    sndPcmHwParams.intervals[8].min = 0;    //GbufferTime;
    sndPcmHwParams.intervals[8].max = -1;   //GbufferTime + 1;

//     printf("min buffer time = %dus\n", GbufferTime);
//     printf("max buffer time = %dus\n", GbufferTime + 1);

    // Buffer size (frames) = Periods * PeriodSize (frames)
    sndPcmHwParams.intervals[9].min = GbufferSize;    //BUFFER_SIZE;      // Buffer size (Frames)
    sndPcmHwParams.intervals[9].max = GbufferSize;   //BUFFER_SIZE;      // Buffer size (Frames)

    // Buffer size (bytes)
    sndPcmHwParams.intervals[10].min = 0;   //BUFFER_SIZE * CHANNELS * sizeof(int16_t);  // Buffer bytes   // Periods * PeriodsBytes
    sndPcmHwParams.intervals[10].max = -1;  //BUFFER_SIZE * CHANNELS * sizeof(int16_t);  // Buffer bytes

    // Tick time (us)
    sndPcmHwParams.intervals[11].min = 0;
    sndPcmHwParams.intervals[11].max = -1;

    // Set hardware parameters
    if((ioctl(pcmFD, SNDRV_PCM_IOCTL_HW_PARAMS, &sndPcmHwParams)) < 0)
    {
        fprintf(stderr, "Error Setting sound hardware parameters:  ");
        perror(NULL);
        exit(-1);
    }
}

// Name             : set_swparams
// Parameters       : pcmFD > PCM palyback file descriptor
// Call             : memset(), sizeof(), ioctl(), fprintf(), perror(), exit()
// Called by        : main()
// Return           : void
// Description      : Set PCM software parameters configuration
void set_swparams(int pcmFD)
{
    struct snd_pcm_sw_params sndPcmSwParams;

    memset(&sndPcmSwParams, 0, sizeof(struct snd_pcm_sw_params));

    sndPcmSwParams.tstamp_mode = SNDRV_PCM_TSTAMP_NONE;          /* timestamp mode */
    sndPcmSwParams.period_step = 1;
    sndPcmSwParams.sleep_min =   0;
    sndPcmSwParams.avail_min =  GperiodSize;          // Allow the transfer when at least period_size samples can be processed

    sndPcmSwParams.xfer_align = 1;

    sndPcmSwParams.start_threshold = GbufferSize;      /* start the transfer when the buffer is almost full: */
    sndPcmSwParams.stop_threshold = GbufferSize;
    sndPcmSwParams.silence_threshold = 0;
    sndPcmSwParams.silence_size = 0;

    // Buffer size in frames // from alsa source code
    sndPcmSwParams.boundary = GbufferSize;      // Buffer size (Frames)

    while (sndPcmSwParams.boundary * 2 <= LONG_MAX - GbufferSize)
		sndPcmSwParams.boundary *= 2;

    // Filling the reserved area
    for(int i = 0 ; i < sizeof(sndPcmSwParams.reserved)/sizeof(sndPcmSwParams.reserved[0]) ; ++i)
        sndPcmSwParams.reserved[i] = -1;

     // Set software parameters
    if((ioctl(pcmFD, SNDRV_PCM_IOCTL_SW_PARAMS, &sndPcmSwParams)) < 0)
    {
        fprintf(stderr, "Error Setting sound software parameters:  ");
        perror(NULL);
        exit(-1);
    }

}

// Name             : sound_generator
// Parameters       : pcmFD > PCM playback file descriptor
// Call             : memset(), update_info(), sizeof(), wave_generator(), write_frames()
// Called by        : main()
// Return           : void
// Description      : Generate sound waves frames and send it to the speaker
void sound_generator(int pcmFD)
{
     short buf[GbufferSize * CHANNELS];

    memset(buf, 0, sizeof(buf));

    update_info();

    while(!Gexit)
    {
        for(int i = 0 ; i < CHANNELS ; ++i)
            wave_generator(i, buf, GperiodSize);

        write_frames(pcmFD, buf, GperiodSize);
    }

}

// Name             : wave_generator
// Parameters       : chn    > Channel number
//                    buf    >  buffer to be filled with frames
//                    frames >  number of frames to fill the buffer
// Call             : atan(), tan(), asin(), sin(), phase_shift()
// Called by        : sound_generator()
// Return           : void
// Description      : Generate wave with type that saved in the GwaveType for each channel up to *frames* and fill *buf* with it.
void wave_generator(int chn, short *buf, snd_pcm_uframes_t frames)
{

    double step;
    static double wt[CHANNELS] = {0.0, 0.0};
    static unsigned int maxVal = 32767; // Generalize?
    static double scale = 2.0 / M_PI;
    int i = 0;

    step = GmaxPhase * Gfreq[chn] / (double) Grate;

    do
    {
        switch(GwaveType[chn])
        {
            case SINE_WAVE:
                buf[chn + i] = Gvol[chn] * maxVal * sin(wt[chn] + Gphase[chn]);
                break;
            case SQUARE_WAVE:
                buf[chn + i] = sin(wt[chn] + Gphase[chn]) > 0.0 ? 1.0 * Gvol[chn] * maxVal : -1.0 * Gvol[chn] * maxVal;
                break;
            case SAWTOOTH_WAVE:
                buf[chn + i] = Gvol[chn] * maxVal * scale * atan(tan((wt[chn] + Gphase[chn])/2.0));
                break;
            case TRIANGULAR_WAVE:
                buf[chn + i] = Gvol[chn] * maxVal * scale * asin(sin(wt[chn] + Gphase[chn]));
                break;
        }
        wt[chn] += step;

        if(GphaseShift[chn])
            phase_shift();

        if(wt[chn] + Gphase[chn] >= GmaxPhase)
        {
            wt[chn] -= GmaxPhase;

           // Check if we want to c٬hange the volume
           if(GvolContactor[chn])
           {
               GvolContactee[chn] = 1;      // Ready for the change
               while(GvolContactor[chn]);   // Wait till the change in volume has been made
               GvolContactee[chn] = 0;      // Reset the flag
           }

        }

        i += CHANNELS;

    }while(--frames > 0);

}

// Name             : Tspeaker_CTRL [Thread function]
// Parameters       : void
// Call             : terminal_CTRL(), tolower(), read(), back_space(),
//                    clear_line(), INPUT_RESET(), dprintf(), isdigit(),
//                    sizeof(), write(), atof(), update_info(), usleep()
// Called by        : main()
// Description      : Control the speaker using keyboard keys at runtime
void *Tspeaker_CTRL(void *)
{

    char ch;
    char num[10];    // Up to 999,999,999 Hz
    char decPoint = 1;
    char fp = 1;
    int i = 0;

    double tmp = 0.0;
    int reset = 0;

    terminal_CTRL(1);

    for(; tolower(ch) != 'q' ;)
    {
        read(STDIN_FILENO, &ch, 1);

        if(ch == 127)       // ASCII BACK SPACE
        {
            back_space();
             --i;

            if(num[i] == '.')
                decPoint = 1;

            else if(i == -1)
            {
                clear_line(fp);
                INPUT_RESET();
            }

            continue;
        }

        else if((tolower(ch) == 'f' || tolower(ch) == 'p' || tolower(ch) == 'v') && fp == 1)
        {

            fp = ch;

            switch(ch)
            {
                case 'f':
                     dprintf(STDOUT_FILENO, f_FREQ);
                     break;
                case 'F':
                     dprintf(STDOUT_FILENO, F_FREQ);
                     break;
                case 'p':
                     dprintf(STDOUT_FILENO, p_PHASE);
                     break;
                case 'P':
                     dprintf(STDOUT_FILENO, P_PHASE);
                     break;
                case 'v':
                     dprintf(STDOUT_FILENO, v_VOL);
                     break;
                case 'V':
                     dprintf(STDOUT_FILENO, V_VOL);
                     break;
            }

            continue;
        }

        else if( isdigit(ch) || (ch == '.' && decPoint == 1) || ch == '\n')
        {
            if(i < sizeof(num) && fp != 1)
            {
                write(STDOUT_FILENO, &ch, 1);
                num[i++] = ch;

                if(ch == '.')
                    decPoint = 0;

                if(ch != '\n')
                    continue;
            }

            tmp = atof(num);

            if(ch == '\n' && (fp == 'f' || fp == 'F') )
            {
                if(fp == 'f')
                {
                    if(tmp >= 0.0 && tmp <= (Grate/2.0))
                        Gfreq[0] = tmp;
                }
                else
                {
                    if(tmp >= 0.0 && tmp <= (Grate/2.0))
                        Gfreq[1] = tmp;
                }

                INPUT_RESET();
                update_info();
            }
            else if(ch == '\n' && (fp == 'p' || fp == 'P') )
            {
                if(fp == 'p')
                {
                    if(tmp >= 0.0 && tmp <= GmaxPhase)
                        Gphase[0] = tmp;
                }
                else
                {
                    if(tmp >= 0.0 && tmp <= GmaxPhase)
                        Gphase[1] = tmp;
                }

                INPUT_RESET();
                update_info();
            }

            else if(ch == '\n' && (fp == 'v' || fp == 'V') )
            {
                if(fp == 'v')
                {
                    if(tmp >= 0.0 && tmp <= 100.0)
                        Gvol[0] = tmp/100.0;
                }

                else
                {
                    if(tmp >= 0.0 && tmp <= 100.0)
                        Gvol[1] = tmp/100.0;
                }

                INPUT_RESET();
                update_info();
            }
        }

        switch(tolower(ch))
        {
            // FREQUENCY KEYS
            case 'u':   // Increase left channel frequency by 1Hz
                Gfreq[0] = (Gfreq[0] + 1) > Grate/2.0 ? Grate/2 : Gfreq[0] + 1;
                INPUT_RESET();
                update_info();
                break;
            case 'j':  // Decrease left channel frequency by 1Hz
                Gfreq[0] = (Gfreq[0] - 1) > 0.0 ? Gfreq[0] - 1 : 0.0;
                INPUT_RESET();
                update_info();
                break;
            case 'i':   // Increase both channels frequencies by 1Hz
                Gfreq[0] = (Gfreq[0] + 1) > Grate/2.0 ? Grate/2 : Gfreq[0] + 1;
                Gfreq[1] = (Gfreq[1] + 1) > Grate/2.0 ? Grate/2 : Gfreq[1] + 1;
                INPUT_RESET();
                update_info();
                break;
            case 'k': // Decrease both channels frequencies by 1Hz
                Gfreq[0] = (Gfreq[0] - 1) > 0.0 ? Gfreq[0] - 1 : 0.0;
                Gfreq[1] = (Gfreq[1] - 1) > 0.0 ? Gfreq[1] - 1 : 0.0;
                INPUT_RESET();
                update_info();
                break;
            case 'o': // Increase right channel frequency by 1Hz
                Gfreq[1] = (Gfreq[1] + 1) > Grate/2.0 ? Grate/2 : Gfreq[1] + 1;
                INPUT_RESET();
                update_info();
                break;
            case 'l': // Decrease right channel frequency by 1Hz
                Gfreq[1] = (Gfreq[1] - 1) > 0.0 ? Gfreq[1] - 1 : 0.0;
                INPUT_RESET();
                update_info();
                break;

            // PHASE SHIFT KEYS
            case 'g': // Increase Channel1 phase
                GphaseShift[0] = 1;
                while(GphaseShift[0])
                    usleep(100);       // 0.1 ms
                INPUT_RESET();
                update_info();
                break;
            case 'b': // Decrease Channel1 phase
                GphaseShift[0] = -1;
                while(GphaseShift[0])
                    usleep(100);       // 0.1 ms
                INPUT_RESET();
                update_info();
                break;
            case 'h': // Increase Channel2 phase
                GphaseShift[1] = 1;
                while(GphaseShift[1])
                    usleep(100);       // 0.1 ms
                INPUT_RESET();
                update_info();
                break;
            case 'n': // Decrease Channel2 phase
                GphaseShift[1] = -1;
                while(GphaseShift[1])
                    usleep(100);       // 0.1 ms
                INPUT_RESET();
                update_info();
                break;

            // VOLUME KEYS
            case 'a':   // Increase the volume of 1'st channel (Left) by 5%
                GvolContactor[0] = 1;     // Tell the generate_sine() change in sound is possible
                while(!GvolContactee[0]);
                Gvol[0] = (Gvol[0] + 0.05) > 1.0 ? 1.0 : Gvol[0] + 0.05;
                GvolContactor[0] = 0;        // Possible change in has been made
                INPUT_RESET();
                update_info();
                break;
            case 'z':   // Decrease the volume of the 1'st channel (Left) by 5%
                GvolContactor[0] = 1;     // Tell the generate_sine() change in sound is possible
                while(!GvolContactee[0]);
                Gvol[0] = (Gvol[0] - 0.05) > 0.0 ? Gvol[0] - 0.05 : 0.0;
                GvolContactor[0] = 0;        // Possible change in has been made
                INPUT_RESET();
                update_info();
                break;
            case 's':   // Increase the volume of both channels by 5%
                GvolContactor[0] = 1;     // Tell the generate_sine() change in sound is possible
                while(!GvolContactee[0]);
                Gvol[0] = (Gvol[0] + 0.05) > 1.0 ? 1.0 : Gvol[0] + 0.05;
                GvolContactor[0] = 0;

                GvolContactor[1] = 1;
                while(!GvolContactee[1]);
                Gvol[1] = (Gvol[1] + 0.05) > 1.0 ? 1.0 : Gvol[1] + 0.05;
                GvolContactor[1] = 0;
                INPUT_RESET();
                update_info();
                break;
            case 'x':   // Decrease the volume of both channels by %5
                GvolContactor[0] = 1;
                while(!GvolContactee[0]);
                Gvol[0] = (Gvol[0] - 0.05) > 0.0 ? Gvol[0] - 0.05 : 0.0;
                GvolContactor[0] = 0;

                GvolContactor[1] = 1;
                while(!GvolContactee[1]);
                Gvol[1] = (Gvol[1] - 0.05) > 0.0 ? Gvol[1] - 0.05 : 0.0;
                GvolContactor[1] = 0;
                INPUT_RESET();
                update_info();
                break;
            case 'd':   // Increase the volume of the 2nd channel (Right) by 5%
                GvolContactor[1] = 1;     // Tell the generate_sine() change in sound is possible
                while(!GvolContactee[1]);
                Gvol[1] = (Gvol[1] + 0.05) > 1.0 ? 1.0 : Gvol[1] + 0.05;
                GvolContactor[1] = 0;        // Possible change in has been made
                INPUT_RESET();
                update_info();
                break;
            case 'c':   // Decrease the volume of the 2nd channel (Right) by 5%
                GvolContactor[1] = 1;     // Tell the generate_sine() change in sound is possible
                while(!GvolContactee[1]);
                Gvol[1] = (Gvol[1] - 0.05) > 0.0 ? Gvol[1] - 0.05 : 0.0;
                GvolContactor[1] = 0;        // Possible change in has been made
                INPUT_RESET();
                update_info();
                break;

            // WAVE TYPE KEYS

            // Left channel wave controllers
            case '1':
                GwaveType[0] = SINE_WAVE;
                INPUT_RESET();
                update_info();
                break;
            case '2':
                GwaveType[0] = SQUARE_WAVE;
                INPUT_RESET();
                update_info();
                break;
            case '3':
                GwaveType[0] = TRIANGULAR_WAVE;
                INPUT_RESET();
                update_info();
                break;
            case '4':
                GwaveType[0] = SAWTOOTH_WAVE;
                INPUT_RESET();
                update_info();
                break;

            // Right channel wave controllers
            case '0':
                GwaveType[1] = SINE_WAVE;
                INPUT_RESET();
                update_info();
                break;
            case '9':
                GwaveType[1] = SQUARE_WAVE;
                INPUT_RESET();
                update_info();
                break;
            case '8':
                GwaveType[1] = TRIANGULAR_WAVE;
                INPUT_RESET();
                update_info();
                break;
             case '7':
                GwaveType[1] = SAWTOOTH_WAVE;
                INPUT_RESET();
                update_info();
                break;

            // EXIT KEY
            case 'q':
                GvolContactor[0] = 1;     // Tell the generate_sine() change in sound is possible
                while(!GvolContactee[0]);
                Gvol[0] = 0.0;
                GvolContactor[0] = 0;        // Possible change in has been made

                GvolContactor[1] = 1;     // Tell the generate_sine() change in sound is possible
                while(!GvolContactee[1]);
                Gvol[1] = 0.0;
                GvolContactor[1] = 0;        // Possible change in has been made

                Gexit = true;

                break;
        }

    }
    terminal_CTRL(0);
}

// Name             : terminal_CTRL
// Parameters       : state > disable or enable the terminal ECHO and ICANON
// Call             : tcgetattr(), tcsetattr()
// Called by        : Tspeaker_CTRL(), write_frames()
// Description      : Enable and disable terminal ECHO and ICANON
//                    *state* value 0 for disable the ECHO and ICANON & 1 for the opposite

void terminal_CTRL(int state)
{
    static struct termios t;
    tcgetattr(STDIN_FILENO, &t);

    if(state)
    {
        t.c_lflag &= ~ECHO;
        t.c_lflag &= ~ICANON;
    }
    else
    {
        // Return terminal to it's orignal state
        t.c_lflag |= ECHO;
        t.c_lflag |= ICANON;
    }

    tcsetattr(STDIN_FILENO, 0, &t);
}

// Name             : phase_shift
// Parameters       : void
// Call             : void
// Called by        : wave_generator()
// Description      : Shift phase

void phase_shift(void)
{
   static double phStep = 0.016;        // Phase step
   static double tmp = 0.0;
    if(GphaseShift[0])
    {
        if(GphaseShift[0] == 1)
        {
            tmp = Gphase[0] + phStep;
            Gphase[0] = tmp < GmaxPhase ? tmp : Gphase[0];
            GphaseShift[0] = 0;
        }

        else if(GphaseShift[0] == -1)
        {
            tmp = Gphase[0] - phStep;
            Gphase[0] = tmp < 0.0 ? 0.0 : tmp;
            GphaseShift[0] = 0;
        }
    }

    else    // GphaseShift[1]
    {
        if(GphaseShift[1] == 1)
        {
            tmp = Gphase[1] + phStep;
            Gphase[1] = tmp < GmaxPhase ? tmp : Gphase[1];
            GphaseShift[1] = 0;
        }
        else if(GphaseShift[1] == -1)
        {
            tmp = Gphase[1] - phStep;
            Gphase[1] = tmp < 0.0 ? 0.0 : tmp;
            GphaseShift[1] = 0;
        }
    }

}

// Name             : update_info
// Parameters       : void
// Call             : printf()
// Called by        : Tspeaker_CTRL(), sound_generator()
// Description      : Update the infomation in the terminal at runtime
void update_info(void)
{

    printf("\e[1;1H\e[2J"); // Clear the screen and set the cursor at (1, 1)
    printf(".--------.------------------.------------------.\n");
    printf("|        |   Channel1(L)    |   Channel2(R)    |\n");
    printf("'--------+------------------+------------------'\n");
    printf("|  WAVE  | %-16s | %-16s |\n", GwaveTypes[GwaveType[0]], GwaveTypes[GwaveType[1]]);
    printf("'--------+------------------+------------------'\n");
    printf("|  FREQ  | %09.2lfHz      | %09.2lfHz      |\n", Gfreq[0], Gfreq[1]);
    printf("'--------+------------------+------------------'\n");
    printf("|  PHASE | %.2lfrad          | %.2lfrad          |\n", Gphase[0], Gphase[1]);
    printf("'--------+------------------+------------------'\n");
    printf("|  VOL   | %06.2lf%%          | %06.2lf%%          |\n", Gvol[0]*100.0, Gvol[1]*100.0);
    printf("'--------'------------------'------------------'\n");

}

// Name             : clear_line
// Parameters       : fp > indicate how many character in the line
// Call             : write(), dprintf()
// Called by        : Tspeaker_CTRL()
// Description      : Clear terminal line

void clear_line(char fp)
{
    char cr = 13;       // ASCII Carriage Return
    static int tmp = 0;

    write(STDOUT_FILENO, &cr, 1);

    switch(fp)
    {
        case 'F':
            tmp = GFFreqLen;
            break;
        case 'f':
            tmp = GfFreqLen;
            break;
        case 'P':
            tmp = GPPhaseLen;
            break;
        case 'p':
            tmp = GpPhaseLen;
            break;
        case 'V':
            tmp = GVVolLen;
            break;
        case 'v':
             tmp = GvVolLen;
            break;
    }

    for(int i = 0 ; i < tmp ; ++i)
        dprintf(STDOUT_FILENO, " ");

    write(STDOUT_FILENO, &cr, 1);

}

// Name             : write_frames
// Parameters       : pcmFD > PCM playback file descriptor
//                    ptr   > pointer to a buffer full of ready frames
//                    cPtr  > Number of frames to be written
// Call             : ioctl(), fprintf(), perror(), terminal_CTRL(), exit()
// Called by        : sound_generator()
// Description      : Write a buffer full of frames to the speaker buffer
void write_frames(int pcmFD, short *ptr, snd_pcm_uframes_t cPtr)
{
    struct snd_xferi sndXferi;

    while(cPtr > 0)
    {
        sndXferi.buf = ptr;
        sndXferi.frames = cPtr;

        if((ioctl(pcmFD, SNDRV_PCM_IOCTL_WRITEI_FRAMES, &sndXferi)) < 0)
        {
            fprintf(stderr, "Error write_loop() to snd driver: ");
            perror(NULL);
            terminal_CTRL(false);   // Return the terminal to it's perivous state
            exit(-1);
        }

        ptr += sndXferi.result;
        cPtr -= sndXferi.result;

    }
}

// Name             : back_space
// Parameters       : void
// Call             : write()
// Called by        : Tspeaker_CTRL()
// Description      : Terminal backspace
void back_space(void)
{
     char BS[3] = {8, 32, 8};    // BACK SPACE ASCII

     // BACK SPACE
            for(int i = 0 ; i < 3 ; ++i)
                write(STDOUT_FILENO, &BS[i], 1);

}

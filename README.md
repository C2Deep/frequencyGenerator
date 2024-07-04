# Frequency Generator
```freqGen```  Generate different types of sound waves through the default speaker of Linux system using Advanced Linux Sound Architecture (ALSA).
   - Generate sound waves in different types ( Sine , Square, Triangualr, Sawtooth).
   - Adjust Frequency, Phase , Volume, and the Wave type of each channel independently.

# Build
``` bash
gcc -o freqGen freqGen.c -lm -lpthread -lasound
```

# Usage
``` bash
./freqGen [options]

-r      sample rate (Hz)
-l      buffer time uSec (latency)
-f      channel 1 freqency (Left)
-F      channel 2 frequency (Right)
-v      channel 1 volume (Left)
-V      channel 2 volume (Right)
-h      help message
``` 

# How to use 

## Sound Wave Type

To change the wave type for channels press :

| Wave Type | Left chn  | Right chn |
|-----------|-----------|-----------|
| Sine      | 1         | 0         |
| Square    | 2         | 9         |
| Triangular| 3         | 8         |
| Sawtooth  | 4         | 7         |

----------------------------------------------------------------

## Frequency
There are two methods to change the frequency :

###  Immediate
  - Press 'f' and enter the new frequency for channel 1 (Left).
  - Press 'F' and enter the new frequency for channel 2 (Right).
###  Relative
  | Freqency  |  Left chn  | Right chn  | Both chns |
  |-----------|------------|------------|-----------|
  |   __+__   |     U      |      O     |     I     |
  |   __-__   |     J      |      L     |     K     |

----------------------------------------------------------------

## Wave Phase
  There are two methods to change the wave phase :
  
  ### Immediate
    - Press 'p' and enter the new wave phase for channel 1 (Left).
    - Press 'P' and enter the new wave phase for channel 2 (Right).

  ## Relative
  | Wave Phase | Left chn  | Right chn  |
  |------------|-----------|------------|
  |    __+__   |    G      |     H      |
  |    __-__   |    B      |     N      |

----------------------------------------------------------------

## Volume
  There are two methods to change the volume :

  ### Immediate  
    - Press 'v' and enter the new volume for channel 1 (Left).
    - Press 'V' and enter the new volume for channel 2 (Right).

  ### Relative
  |  Volume  |  Left chn  |  Right chn  |  Both chns  |
  |----------|------------|-------------|-------------|
  |  __+__   |     A      |      F      |      S      |
  |  __-__   |     Z      |      C      |      X      |
  
----------------------------------------------------------------

## Exit
  Press 'q' to exit from the program.





/* sysex.c -- example program showing how to send and receive sysex
    messages

   Messages are stored in a file using 2-digit hexadecimal numbers,
   one per byte, separated by blanks, with up to 32 numbers per line:
   F0 14 A7 4B ...

 */

#include "stdio.h"
#include "stdlib.h"
#include "assert.h"
#include "portmidi.h"
#include "porttime.h"
#include "string.h"

#define MIDI_SYSEX 0xf0
#define MIDI_EOX 0xf7

#define STRING_MAX 80

int latency = 0;

/* read a number from console */
/**/
int get_number(char *prompt)
{
    char line[STRING_MAX];
    int n = 0, i;
    printf(prompt);
    while (n != 1) {
        n = scanf("%d", &i);
        fgets(line, STRING_MAX, stdin);

    }
    return i;
}


/* loopback test -- send/rcv from 2 to 1000 bytes of random midi data */
/**/
void loopback_test()
{
    int outp;
    int inp;
    PmStream *midi_in;
    PmStream *midi_out;
    unsigned char msg[1024];
    char line[80];
    long len;
    int i;
    int data;
    PmEvent event;
    int shift;

    Pt_Start(1, 0, 0);
    
    printf("Connect a midi cable from an output port to an input port.\n");
    printf("This test will send random data via sysex message from output\n");
    printf("to input and check that the correct data was received.\n");
    outp = get_number("Type output device number: ");
    /* Open output with 1ms latency -- when latency is non-zero, the Win32
       implementation supports sending sysex messages incrementally in a 
       series of buffers. This is nicer than allocating a big buffer for the
       message, and it also seems to work better. Either way works.
     */
    Pm_OpenOutput(&midi_out, outp, NULL, 0, NULL, NULL, latency);
    inp = get_number("Type input device number: ");
    /* since we are going to send and then receive, make sure the input buffer
       is large enough for the entire message */
    Pm_OpenInput(&midi_in, inp, NULL, 512, NULL, NULL);

    srand((unsigned int) Pt_Time()); /* seed for random numbers */

    while (1) {
        PmError count;
        long start_time;
        long error_position;
        long expected = 0;
        long actual = 0;
        printf("Type return to send message, q to quit: ");
        fgets(line, STRING_MAX, stdin);
        if (line[0] == 'q') goto cleanup;

        /* compose the message */
        len = rand() % 998 + 2; /* len only counts data bytes */
        msg[0] = (char) MIDI_SYSEX; /* start of SYSEX message */
        /* data bytes go from 1 to len */
        for (i = 0; i < len; i++) {
            msg[i + 1] = rand() & 0x7f; /* MIDI data */
        }
        /* final EOX goes in len+1, total of len+2 bytes in msg */
        msg[len + 1] = (char) MIDI_EOX;

        /* sanity check: before we send, there should be no queued data */
        count = Pm_Read(midi_in, &event, 1);

        if (count != 0) {
			printf("Before sending anything, a MIDI message was found in\n");
			printf("the input buffer. Please try again.\n");
			break;
		}

        /* send the message */
        printf("Sending %ld byte sysex message.\n", len + 2);
        Pm_WriteSysEx(midi_out, 0, msg);

        /* receive the message and compare to msg[] */
        data = 0;
        shift = 0;
        i = 0;
        start_time = Pt_Time();
        error_position = -1;
        /* allow up to 2 seconds for transmission */
        while (data != MIDI_EOX && start_time + 2000 > Pt_Time()) {
            count = Pm_Read(midi_in, &event, 1);
            /* CAUTION: this causes busy waiting. It would be better to 
               be in a polling loop to avoid being compute bound. PortMidi
               does not support a blocking read since this is so seldom
               useful. There is no timeout, so if we don't receive a sysex
               message, or at least an EOX, the program will hang here.
             */
            if (count == 0) continue;
            
            /* printf("read %lx ", event.message);
               fflush(stdout); */
            
            /* compare 4 bytes of data until you reach an eox */
            for (shift = 0; shift < 32 && (data != MIDI_EOX); shift += 8) {
                data = (event.message >> shift) & 0xFF;
                if (data != msg[i] && error_position < 0) {
                    error_position = i;
                    expected = msg[i];
                    actual = data;
                }
                i++;
            }
        }
        if (error_position >= 0) {
            printf("Error at byte %ld: sent %lx recd %lx\n", error_position, expected, actual);
        } else if (i != len + 2) {
            printf("Error: byte %d not received\n", i);
        } else {
            printf("Correctly ");
        }
        printf("received %d byte sysex message.\n", i);
    }
cleanup:
    Pm_Close(midi_out);
    Pm_Close(midi_in);
    return;
}


#define is_real_time_msg(msg) ((0xF0 & Pm_MessageStatus(msg)) == 0xF8)


void receive_sysex()
{
    char line[80];
    FILE *f;
    PmStream *midi;
    int shift = 0;
    int data = 0;
    int bytes_on_line = 0;
    PmEvent msg;

    /* determine which output device to use */
    int i = get_number("Type input device number: ");

    /* open input device */
    Pm_OpenInput(&midi, i, NULL, 512, NULL, NULL);
    printf("Midi Input opened, type file for sysex data: ");

    /* open file */
    fgets(line, STRING_MAX, stdin);
    /* remove the newline character */
    if (strlen(line) > 0) line[strlen(line) - 1] = 0;
    f = fopen(line, "w");
    if (!f) {
        printf("Could not open %s\n", line);
        Pm_Close(midi);
        return;
    }

    printf("Ready to receive a sysex message\n");

    /* read data and write to file */
    while (data != MIDI_EOX) {
        PmError count;
        count = Pm_Read(midi, &msg, 1);
        /* CAUTION: this causes busy waiting. It would be better to 
           be in a polling loop to avoid being compute bound. PortMidi
           does not support a blocking read since this is so seldom
           useful.
         */
        if (count == 0) continue;
        /* ignore real-time messages */
        if (is_real_time_msg(Pm_MessageStatus(msg.message))) continue;

        /* write 4 bytes of data until you reach an eox */
        for (shift = 0; shift < 32 && (data != MIDI_EOX); shift += 8) {
            data = (msg.message >> shift) & 0xFF;
            /* if this is a status byte that's not MIDI_EOX, the sysex
               message is incomplete and there is no more sysex data */
            if (data & 0x80 && data != MIDI_EOX) break;
            fprintf(f, "%2x ", data);
            if (++bytes_on_line >= 16) {
                fprintf(f, "\n");
                bytes_on_line = 0;
            }
        }
    }
	fclose(f);
    Pm_Close(midi);
}


void send_sysex()
{
    char line[80];
    FILE *f;
    PmStream *midi;
    int data;
    int shift = 0;
    PmEvent msg;

	/* determine which output device to use */
    int i = get_number("Type output device number: ");

    msg.timestamp = 0; /* no need for timestamp */

	/* open output device */
    Pm_OpenOutput(&midi, i, NULL, 0, NULL, NULL, latency);
	printf("Midi Output opened, type file with sysex data: ");

    /* open file */
    fgets(line, STRING_MAX, stdin);
    /* remove the newline character */
    if (strlen(line) > 0) line[strlen(line) - 1] = 0;
    f = fopen(line, "r");
    if (!f) {
        printf("Could not open %s\n", line);
        Pm_Close(midi);
        return;
    }

    /* read file and send data */
    msg.message = 0;
    while (1) {
        /* get next byte from file */

        if (fscanf(f, "%x", &data) == 1) {
            /* printf("read %x, ", data); */
            /* OR byte into message at proper offset */
            msg.message |= (data << shift);
            shift += 8;
        }
        /* send the message if it's full (shift == 32) or if we are at end */
        if (shift == 32 || data == MIDI_EOX) {
            /* this will send sysex data 4 bytes at a time -- it would
               be much more efficient to send multiple PmEvents at once
               but this method is simpler. See Pm_WriteSysex for a more
               efficient code example.
             */
            Pm_Write(midi, &msg, 1);
            msg.message = 0;
            shift = 0;
        }
        if (data == MIDI_EOX) { /* end of message */
            fclose(f);
            Pm_Close(midi);
            return;
        }
    }
}


int main()
{
    int i;
    char line[80];
    
	/* list device information */
	for (i = 0; i < Pm_CountDevices(); i++) {
        const PmDeviceInfo *info = Pm_GetDeviceInfo(i);
        printf("%d: %s, %s", i, info->interf, info->name);
        if (info->input) printf(" (input)");
        if (info->output) printf(" (output)");
        printf("\n");
    }
	latency = get_number("Latency in milliseconds (0 to send data immediatedly,\n"
		                 "  >0 to send timestamped messages): ");    
    while (1) {
        printf("Type r to receive sysex, s to send,"
               " l for loopback test, q to quit: ");
        fgets(line, STRING_MAX, stdin);
        switch (line[0]) {
          case 'r':
            receive_sysex();
            break;
          case 's':
            send_sysex();
            break;
          case 'l':
            loopback_test();
          case 'q':
            exit(0);
          default:
            break;
        }
    }
    return 0;
}


     

            

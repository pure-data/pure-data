/*
    test_libpd: test libpd by creating two Pd instances and running them
    simultaneously.  The test patch has a phasor~ object - in one instance
    it runs forward, and in the other, backward.
*/

#include <stdio.h>
#include "z_libpd.h"
#include "m_imp.h"

void pdprint(const char *s) {
  printf("%s", s);
}

void pdnoteon(int ch, int pitch, int vel) {
  printf("noteon: %d %d %d\n", ch, pitch, vel);
}

int main(int argc, char **argv) {
    t_pdinstance *pd1, *pd2;
    int srate = 48000;
        /* one input channel, two output channels,
        block size 64, one tick per buffer: */
    float inbuf[64], outbuf[128];
    char *filename = "test_libpd.pd", *dirname = ".";

        /* maybe these two calls should be available per-instance somehow: */
    libpd_set_printhook(pdprint);
    libpd_set_noteonhook(pdnoteon);

    libpd_init();

    pd1 = libpd_new_instance();
    pd2 = libpd_new_instance();

    libpd_set_instance(pd1); /* talk to first pd instance */

    libpd_init_audio(1, 2, srate);
        /* send message:  [; pd dsp 1(   */
    libpd_start_message(1); /* one entry in list */
    libpd_add_float(1.0f);
    libpd_finish_message("pd", "dsp");

        /* open the file  */
    libpd_openfile(filename, dirname);

        /* repeat this all for the second instance */
    libpd_set_instance(pd2);
    libpd_init_audio(1, 2, srate);
    libpd_start_message(1);
    libpd_add_float(1.0f);
    libpd_finish_message("pd", "dsp");
    libpd_openfile(filename, dirname);

    libpd_set_instance(pd1);
    /* [; pd frequency 480 ( */
    libpd_start_message(1);
    libpd_add_float(480.0f);
    libpd_finish_message("frequency", "float");

    libpd_set_instance(pd2);
    /* [; pd frequency -480 ( */
    libpd_start_message(1);
    libpd_add_float(-480.0f);
    libpd_finish_message("frequency", "float");

    int i, j;
    for (i = 0; i < 3; i++)
    {
        libpd_set_instance(pd1);
        libpd_process_float(1, inbuf, outbuf);

        printf("instance 1, tick %d:\n", i);
        for (j = 0; j < 8; j++)
            printf("%f ", outbuf[j]);
        printf("... \n");

        libpd_set_instance(pd2);
        libpd_process_float(1, inbuf, outbuf);
        printf("instance 2, tick %d:\n", i);
        for (j = 0; j < 8; j++)
            printf("%f ", outbuf[j]);
        printf("... \n");
    }

    libpd_free_instance(pd1);
    libpd_free_instance(pd2);

    return 0;
}

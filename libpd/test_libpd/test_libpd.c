/*
    test_libpd: test libpd by creating two Pd instances and running them
    simultaneously.  The test patch has a phasor~ object - in one instance
    it runs forward, and in the other, backward.
*/

#include <stdio.h>
#include "z_libpd.h"

void pdprint(const char *s) {
  printf("%s", s);
}

void pdnoteon(int ch, int pitch, int vel) {
  printf("noteon: %d %d %d\n", ch, pitch, vel);
}

t_class *test_class;

typedef struct _test
{
    t_object x_obj;
} t_test;

typedef struct _test_data
{
    int x_count;
} t_test_data;

void test_data_free(t_test_data *data)
{
    post("free data for 'test' class");
    freebytes(data, sizeof(*data));
}

void *test_new(void)
{
    t_test *x = (t_test *)pd_new(test_class);

        /* lazily create per-instance data */
    t_test_data *data = class_getinstancedata(test_class);
    if (!data)
    {
        data = (t_test_data*)getbytes(sizeof(*data));
        data->x_count = 0;
        class_setinstancedata(test_class, data,
            (t_classdatafn)test_data_free);
    }
    data->x_count++;
    post("test new: %d objects", data->x_count);

    return x;
}

void test_free(t_test *x)
{
    t_test_data *data = class_getinstancedata(test_class);
    data->x_count--;
    post("test free: %d remaining objects", data->x_count);
}

void setup_test(void)
{
    test_class = class_new(gensym("test"),
        (t_newmethod)test_new, (t_method)test_free, sizeof(t_test), 0, 0);
}

int main(int argc, char **argv) {
    t_pdinstance *pd1, *pd2;
    int srate = 48000;
        /* one input channel, two output channels,
           block size 64, one tick per buffer: */
    float inbuf[64], outbuf[128];
    char *filename = "test_libpd.pd", *dirname = ".";

    /* accept overrides from the commandline:
       $ pdtest_multi file.pd ../dir */
    if (argc > 1) filename = argv[1];
    if (argc > 2) dirname = argv[2];

        /* If called *before* libpd_init(), this will set
           the global printhook. It can also be set per
           instance, see libpd_set_noteonhook() below. */
    libpd_set_printhook(pdprint);

    libpd_init();

    setup_test();

    pd1 = libpd_new_instance();
    pd2 = libpd_new_instance();

    libpd_set_instance(pd1); /* talk to first pd instance */

    libpd_set_noteonhook(pdnoteon); /* set note-on-hook (per instance) */

    libpd_init_audio(1, 2, srate);
        /* send message:  [; pd dsp 1(   */
    libpd_start_message(1); /* one entry in list */
    libpd_add_float(1.0f);
    libpd_finish_message("pd", "dsp");

        /* open the file  */
    libpd_openfile(filename, dirname);

        /* repeat this all for the second instance */
    libpd_set_instance(pd2);
    libpd_set_noteonhook(pdnoteon);
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

    /* now run pd for 3 ticks */
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

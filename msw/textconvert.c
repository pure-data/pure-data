/* Text file conversion unix/max/windows. duh. */

#include <stdio.h>

int main(int argc, char **argv)
{
    FILE *infile;
    int lastchar = 0, c;
    int from, to;
    if (argc < 3 || argc > 4) goto usage;
    from = argv[1][0];
    to = argv[2][0];
    if (argc == 4)
    {
    	if (!(infile = fopen(argv[3], "r")))
	{
	    perror(argv[3]);
	    exit(1);
    	}
    }
    else infile = stdin;
    if (from != 'u' && from != 'm' && from != 'w'
    	|| to != 'u' && to != 'm' && to != 'w')
	    goto usage;
    while ((c = getc(infile)) != EOF)
    {
    	if (from == 'u' && to == 'm')
	{
	    if (c == '\n')
	    	c = '\r';
	    putchar(c);
	}
    	else if (from == 'u' && to == 'w')
	{
	    if (c == '\n')
	    	putchar('\r');
	    putchar(c);
	}
    	else if (from == 'm' && to == 'u')
	{
	    if (c == '\r')
	    	c = '\n';
	    putchar(c);
	}
    	else if (from == 'm' && to == 'w')
	{
	    putchar(c);
	    if (c == '\r')
	    	putchar('\n');
	}
    	else if (from == 'w' && to == 'u')
	{
	    if (c != '\r')
	    	putchar(c);
	}
    	else if (from == 'w' && to == 'm')
	{
	    if (c != '\n')
	    	putchar(c);
	}
	else putchar(c);
    }
    exit(0);
usage:
    fprintf(stderr, "usage: textconvert <sysfrom> <systo> [file]\n");
    fprintf(stderr, "systems are u[nix], m[ac], w[indows].\n");
    exit (1);
}

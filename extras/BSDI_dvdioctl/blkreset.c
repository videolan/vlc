/*
 * This is a (very) simple program to be used when the blocksize on a CDrom
 * drive gets set to something other than 2048 (such as 2336 when ripping
 * VCDs).   
 *
 * Uses the 'cdrom_blocksize' routine from libdvd
*/

#include <stdio.h>
#include <err.h>
#include <fcntl.h>

main(int argc, char **argv)
	{
	int	fd;
	char	*device = "/dev/rsr1c";

	if	(argc != 2)
		errx(1, "raw device name of CDROM drive needed as arg");
	
	device = argv[1];
	fd = open(device, O_RDONLY, 0);
	if	(fd < 0)
		err(1, "open(%s)", device);

	if	(cdrom_blocksize(fd, 2048))
		errx(1, "cdrom_blocksize for %s failed\n", device);
	exit(0);
	}

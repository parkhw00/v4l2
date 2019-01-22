
#include <linux/videodev2.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>

void error (const char *fmt, ...)
{
	va_list ap;
	int en = errno;

	va_start (ap, fmt);
	vfprintf (stderr, fmt, ap);
	va_end (ap);
	fprintf (stderr, "errno %d, %s\n", en, strerror (en));
}

int print_fmt (struct v4l2_format *fmt)
{
#define print_field(s,f,t)	printf (#s"->"#f" : %"t"\n", s->f)
	print_field (fmt, fmt.pix.width, "d");
	print_field (fmt, fmt.pix.height, "d");
	print_field (fmt, fmt.pix.pixelformat, "08x");
	printf ("fmt->fmt.pix.pixelformat : %c%c%c%c\n",
			(fmt->fmt.pix.pixelformat>> 0)&0xff,
			(fmt->fmt.pix.pixelformat>> 8)&0xff,
			(fmt->fmt.pix.pixelformat>>16)&0xff,
			(fmt->fmt.pix.pixelformat>>24)&0xff);
	print_field (fmt, fmt.pix.field, "d");
	print_field (fmt, fmt.pix.bytesperline, "d");
	print_field (fmt, fmt.pix.sizeimage, "d");
	print_field (fmt, fmt.pix.colorspace, "d");
	print_field (fmt, fmt.pix.priv, "08x");
	print_field (fmt, fmt.pix.flags, "d");
	print_field (fmt, fmt.pix.hsv_enc, "d");
	print_field (fmt, fmt.pix.quantization, "d");
	print_field (fmt, fmt.pix.xfer_func, "d");

	return 0;
}

int v4l2_capture (const char *name, int width, int height, int fr_num, int fr_den, unsigned int pixel_format)
{
	struct v4l2_capability caps = { };
	struct v4l2_format fmt;
	int fd;
	int ret;

	fd = open (name, O_RDWR);
	if (fd < 0)
	{
		error ("open failed. %s\n", name);
		return -1;
	}

	ret = ioctl (fd, VIDIOC_QUERYCAP, &caps);
	if (ret < 0)
	{
		error ("VIDIOC_QUERYCAP failed.\n");
		close (fd);
		return -1;
	}

	if (!(caps.capabilities & V4L2_CAP_VIDEO_CAPTURE))
	{
		error ("no capturer\n");
		close (fd);
		return -1;
	}

	memset (&fmt, 0, sizeof (fmt));
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	ret = ioctl (fd, VIDIOC_G_FMT, &fmt);
	if (ret < 0)
	{
		error ("VIDIOC_G_FMT failed.\n");
		close (fd);
		return -1;
	}
	print_fmt (&fmt);

	if (width > 0)
	{
		fmt.fmt.pix.width = width;
		fmt.fmt.pix.height = width;
	}
	if (pixel_format)
		fmt.fmt.pix.pixelformat = pixel_format;
	fmt.fmt.pix.field = V4L2_FIELD_ANY;
	//fmt.fmt.pix.bytesperline = 0;
	//fmt.fmt.pix.sizeimage = 0;
	fmt.fmt.pix.colorspace = V4L2_COLORSPACE_DEFAULT;

	ret = ioctl (fd, VIDIOC_S_FMT, &fmt);
	if (ret < 0)
	{
		error ("VIDIOC_S_FMT failed.\n");
		close (fd);
		return -1;
	}

	ret = ioctl (fd, VIDIOC_G_FMT, &fmt);
	if (ret < 0)
	{
		error ("VIDIOC_G_FMT failed.\n");
		close (fd);
		return -1;
	}
	print_fmt (&fmt);

	return 0;
}

int main (int argc, char **argv)
{
	char *opt_device = "/dev/video0";
	int opt_width = -1;
	int opt_height = -1;
	unsigned int opt_pixelformat = 0;

	while (1)
	{
		int opt;

		opt = getopt (argc, argv, "d:w:h:f:");
		if (opt < 0)
			break;

		switch (opt)
		{
			case 'd':
				opt_device = optarg;
				break;

			case 'w':
				opt_width = atoi (optarg);
				break;

			case 'h':
				opt_height = atoi (optarg);
				break;

			case 'f':
				if (strlen (optarg) < 4)
				{
					fprintf (stderr, "-f require fourcc(4 characters)\n");
					exit (1);
				}
				opt_pixelformat = v4l2_fourcc (optarg[0], optarg[1], optarg[2], optarg[3]);
				break;
		}
	}

	v4l2_capture (opt_device, opt_width, opt_height, -1, -1, opt_pixelformat);

	return 0;
}

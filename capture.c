
#include <linux/videodev2.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
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
#define print_field(s,f,t)	fprintf (stderr, #s"->"#f" : %"t"\n", s->f)
	print_field (fmt, fmt.pix.width, "d");
	print_field (fmt, fmt.pix.height, "d");
	print_field (fmt, fmt.pix.pixelformat, "08x");
	fprintf (stderr, "fmt->fmt.pix.pixelformat : %c%c%c%c\n",
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
	print_field (fmt, fmt.pix.ycbcr_enc, "d");
	print_field (fmt, fmt.pix.quantization, "d");
	print_field (fmt, fmt.pix.xfer_func, "d");

	return 0;
}

int v4l2_capture (const char *name, int width, int height, int fr_num, int fr_den, unsigned int pixel_format, int *running, int (*got_data) (void *arg, void *data, int size), void *got_data_arg)
{
	struct v4l2_capability caps = { };
	struct v4l2_format fmt = { };
	struct v4l2_requestbuffers reqbufs = { };
	struct buf
	{
		struct v4l2_buffer vb;
		void *mem;
	} bufs[4];
#define buf_count (sizeof(bufs) / sizeof(bufs[0]))
	int fd;
	int ret;
	int i;

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
		goto done;
	}

	if (!(caps.capabilities & V4L2_CAP_VIDEO_CAPTURE))
	{
		error ("no capturer\n");
		ret = -1;
		goto done;
	}

	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	ret = ioctl (fd, VIDIOC_G_FMT, &fmt);
	if (ret < 0)
	{
		error ("VIDIOC_G_FMT failed.\n");
		goto done;
	}
	print_fmt (&fmt);

	/* set format */
	if (width > 0 || pixel_format )
	{
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
			goto done;
		}

		ret = ioctl (fd, VIDIOC_G_FMT, &fmt);
		if (ret < 0)
		{
			error ("VIDIOC_G_FMT failed.\n");
			goto done;
		}
		print_fmt (&fmt);
	}

	/* request buffer and map */
	reqbufs.count = buf_count;
	reqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	reqbufs.memory = V4L2_MEMORY_MMAP;
	ret = ioctl (fd, VIDIOC_REQBUFS, &reqbufs);
	if (ret < 0)
	{
		error ("VIDIOC_REQBUFS failed.\n");
		goto done;
	}

	for (i=0; i<buf_count; i++)
	{
		bufs[i].vb.index = i;
		bufs[i].vb.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		ret = ioctl (fd, VIDIOC_QUERYBUF, &bufs[i].vb);
		if (ret < 0)
		{
			error ("VIDIOC_QUERYBUF failed.\n");
			goto done;
		}

		fprintf (stderr, "bufs[%d].vb.offset 0x%x(%d)\n", i, bufs[i].vb.m.offset, bufs[i].vb.m.offset);
		fprintf (stderr, "bufs[%d].vb.length 0x%x(%d)\n", i, bufs[i].vb.length, bufs[i].vb.length);
		fprintf (stderr, "bufs[%d].vb.flags 0x%x\n", i, bufs[i].vb.flags);

		bufs[i].mem = mmap (NULL, bufs[i].vb.length, PROT_READ, MAP_SHARED, fd, bufs[i].vb.m.offset);
		if (bufs[i].mem == MAP_FAILED)
		{
			error ("mmap() failed for buf %d\n", i);
			goto done;
		}
		fprintf (stderr, "bufs[%d].mem %p\n", i, bufs[i].mem);

		if (!(bufs[i].vb.flags & V4L2_BUF_FLAG_QUEUED))
		{
			ret = ioctl (fd, VIDIOC_QBUF, &bufs[i].vb);
			if (ret < 0)
			{
				error ("VIDIOC_QBUF failed.\n");
				goto done;
			}
		}
	}

	/* stream on */
	ret = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	ret = ioctl (fd, VIDIOC_STREAMON, &ret);
	if (ret < 0)
	{
		error ("VIDIOC_STREAMON failed.n");
		goto done;
	}

	while (*running)
	{
		struct v4l2_buffer vb;
		char str[3*8 + 1];

		/* dequeue */
		memset (&vb, 0, sizeof (vb));
		vb.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		ret = ioctl (fd, VIDIOC_DQBUF, &vb);
		if (ret < 0)
		{
			error ("VIDIOC_DQBUF failed.\n");
			goto done;
		}

		for (i=0; i<8; i++)
			sprintf (str+3*i, " %02x", ((unsigned char*)bufs[vb.index].mem)[i]);
		fprintf (stderr, "bufs[%d] flags 0x%x, bytes %6d, field %d, seq %5d, data:%s\n", vb.index, vb.flags, vb.bytesused, vb.field, vb.sequence, str);
		got_data (got_data_arg, bufs[vb.index].mem, vb.bytesused);

		ret = ioctl (fd, VIDIOC_QBUF, &vb);
		if (ret < 0)
		{
			error ("VIDIOC_DQBUF failed.\n");
			goto done;
		}
	}

	/* stream off */
	ret = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	ret = ioctl (fd, VIDIOC_STREAMOFF, &ret);
	if (ret < 0)
	{
		error ("VIDIOC_STREAMON failed.n");
		goto done;
	}

done:
	close (fd);
	return ret;
}

int got_data (void *arg, void *data, int size)
{
	int outfd = *(int*)arg;

	if (outfd >= 0)
	{
		write (outfd, data, size);
	}

	return 0;
}

int main (int argc, char **argv)
{
	char *opt_device = "/dev/video0";
	char *opt_output = NULL;
	int outfd = -1;
	int opt_width = -1;
	int opt_height = -1;
	unsigned int opt_pixelformat = 0;
	int running = 1;

	while (1)
	{
		int opt;

		opt = getopt (argc, argv, "?d:w:h:f:o:");
		if (opt < 0)
			break;

		switch (opt)
		{
			case '?':
				fprintf (stderr,
					" $ capture <options>\n"
					"options:\n"
					" -d <devname>        : v4l2 device name. default:%s\n"
					" -w <width>          : width of captured screen\n"
					" -w <height>         : height of captured screen\n"
					" -f <pixelformat>    : pixel format\n"
					" -o <filename>       : filename of pixel dump\n"
					, opt_device);
				exit (1);

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

			case 'o':
				opt_output = optarg;
				break;
		}
	}

	if (opt_output)
	{
		outfd = open (opt_output, O_CREAT|O_WRONLY|O_TRUNC, 0644);
		if (outfd < 0)
		{
			error ("cannot open %s\n", opt_output);
			exit (1);
		}
	}

	v4l2_capture (opt_device, opt_width, opt_height, -1, -1, opt_pixelformat, &running, got_data, &outfd);

	return 0;
}

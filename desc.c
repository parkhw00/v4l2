
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

void error (const char *fmt, ...)
{
	va_list ap;
	int en = errno;

	va_start (ap, fmt);
	vfprintf (stderr, fmt, ap);
	va_end (ap);
	fprintf (stderr, "errno %d, %s\n", en, strerror (en));

	exit (1);
}

int desc_fmt (int fd, enum v4l2_buf_type type)
{
	int i;
	const char *buf_type_name[] =
	{
#define define_buf_type(n)	[n] = #n
		define_buf_type(V4L2_BUF_TYPE_VIDEO_CAPTURE),
		define_buf_type(V4L2_BUF_TYPE_VIDEO_OUTPUT),
		define_buf_type(V4L2_BUF_TYPE_VIDEO_OVERLAY),
		define_buf_type(V4L2_BUF_TYPE_VBI_CAPTURE),
		define_buf_type(V4L2_BUF_TYPE_VBI_OUTPUT),
		define_buf_type(V4L2_BUF_TYPE_SLICED_VBI_CAPTURE),
		define_buf_type(V4L2_BUF_TYPE_SLICED_VBI_OUTPUT),
		define_buf_type(V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY),
		define_buf_type(V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE),
		define_buf_type(V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE),
		define_buf_type(V4L2_BUF_TYPE_SDR_CAPTURE),
		define_buf_type(V4L2_BUF_TYPE_SDR_OUTPUT),
#ifdef V4L2_BUF_TYPE_META_CAPTURE
		define_buf_type(V4L2_BUF_TYPE_META_CAPTURE),
#endif
	};

	printf ("buf type %2d %s\n", type, type < (sizeof (buf_type_name)/sizeof (buf_type_name[0])) ? buf_type_name[type]:"unknown");

	for (i=0; ; i++)
	{
		struct v4l2_fmtdesc fmt;
		int ret;
		int j;

		memset (&fmt, 0, sizeof (fmt));
		fmt.index = i;
		fmt.type = type;

		ret = ioctl (fd, VIDIOC_ENUM_FMT, &fmt);
		if (ret < 0)
			break;

		printf ("  index %d\n", i);
		printf ("  flags 0x%x\n", fmt.flags);
		printf ("  description %s\n", fmt.description);
		printf ("  pixelformat %c%c%c%c\n",
				(fmt.pixelformat >>  0) & 0xff,
				(fmt.pixelformat >>  8) & 0xff,
				(fmt.pixelformat >> 16) & 0xff,
				(fmt.pixelformat >> 24) & 0xff);

		printf ("  frame sizes..\n");
		for (j=0; ; j++)
		{
			struct v4l2_frmsizeenum size;

			memset (&size, 0, sizeof (size));
			size.index = j;
			size.pixel_format = fmt.pixelformat;

			ret = ioctl (fd, VIDIOC_ENUM_FRAMESIZES, &size);
			if (ret < 0)
				break;

			if (size.type == V4L2_FRMSIZE_TYPE_DISCRETE)
			{
				int k;

				printf ("    %2d. %dx%d", j, size.discrete.width, size.discrete.height);

				for (k=0; ; k++)
				{
					struct v4l2_frmivalenum ival;

					memset (&ival, 0, sizeof (ival));
					ival.index = k;
					ival.pixel_format = fmt.pixelformat;
					ival.width = size.discrete.width;
					ival.height = size.discrete.height;

					ret = ioctl (fd, VIDIOC_ENUM_FRAMEINTERVALS, &ival);
					if (ret < 0)
						break;

					if (ival.type == V4L2_FRMIVAL_TYPE_DISCRETE)
						printf (" %d/%d", ival.discrete.numerator, ival.discrete.denominator);
				}
				printf ("\n");
			}
		}
	}

	return 0;
}

int desc (const char *name)
{
	struct v4l2_capability caps = { };
	int ret;
	int fd;
	int i;

	fd = open (name, O_RDWR);
	if (fd < 0)
		error ("open failed. %s\n", name);

	ret = ioctl (fd, VIDIOC_QUERYCAP, &caps);
	if (ret < 0)
		error ("VIDIOC_QUERYCAP failed.\n");

	printf ("driver      %s\n", caps.driver);
	printf ("card        %s\n", caps.card);
	printf ("bus_info    %s\n", caps.bus_info);
	printf ("version     0x%x(%d)\n", caps.version, caps.version);
	printf ("capabilities.. 0x%x\n", caps.capabilities);
	{
		struct
		{
			const char *name;
			unsigned int bits;
		} fields[] =
		{
#define define_field(n)	{ #n, n, }
			define_field(V4L2_CAP_VIDEO_CAPTURE),
			define_field(V4L2_CAP_VIDEO_OUTPUT),
			define_field(V4L2_CAP_VIDEO_OVERLAY),
			define_field(V4L2_CAP_VBI_CAPTURE),
			define_field(V4L2_CAP_VBI_OUTPUT),
			define_field(V4L2_CAP_SLICED_VBI_CAPTURE),
			define_field(V4L2_CAP_SLICED_VBI_OUTPUT),
			define_field(V4L2_CAP_RDS_CAPTURE),
			define_field(V4L2_CAP_VIDEO_OUTPUT_OVERLAY),
			define_field(V4L2_CAP_HW_FREQ_SEEK),
			define_field(V4L2_CAP_RDS_OUTPUT),
			define_field(V4L2_CAP_VIDEO_CAPTURE_MPLANE),
			define_field(V4L2_CAP_VIDEO_OUTPUT_MPLANE),
			define_field(V4L2_CAP_VIDEO_M2M_MPLANE),
			define_field(V4L2_CAP_VIDEO_M2M),
			define_field(V4L2_CAP_TUNER),
			define_field(V4L2_CAP_AUDIO),
			define_field(V4L2_CAP_RADIO),
			define_field(V4L2_CAP_MODULATOR),
			define_field(V4L2_CAP_SDR_CAPTURE),
			define_field(V4L2_CAP_EXT_PIX_FORMAT),
			define_field(V4L2_CAP_SDR_OUTPUT),
#ifdef V4L2_CAP_META_CAPTURE
			define_field(V4L2_CAP_META_CAPTURE),
#endif
			define_field(V4L2_CAP_READWRITE),
			define_field(V4L2_CAP_ASYNCIO),
			define_field(V4L2_CAP_STREAMING),
#ifdef V4L2_CAP_TOUCH
			define_field(V4L2_CAP_TOUCH),
#endif
			define_field(V4L2_CAP_DEVICE_CAPS),
			{ },
		};

		for (i=0; fields[i].name; i++)
		{
			if (caps.capabilities & fields[i].bits)
				printf ("  %s\n", fields[i].name);
		}
	}
	printf ("device_caps 0x%x\n", caps.device_caps);

	printf ("\n");
#define do_desc_fmt(n) \
	if (caps.capabilities & V4L2_CAP_##n) \
		desc_fmt (fd, V4L2_BUF_TYPE_##n);
	do_desc_fmt (VIDEO_CAPTURE);
	do_desc_fmt (VIDEO_OUTPUT);
	do_desc_fmt (VIDEO_OVERLAY);
	do_desc_fmt (VBI_CAPTURE);
	do_desc_fmt (VBI_OUTPUT);
	do_desc_fmt (SLICED_VBI_CAPTURE);
	do_desc_fmt (SLICED_VBI_OUTPUT);
	do_desc_fmt (VIDEO_OUTPUT_OVERLAY);
	do_desc_fmt (VIDEO_CAPTURE_MPLANE);
	do_desc_fmt (VIDEO_OUTPUT_MPLANE);
	do_desc_fmt (SDR_CAPTURE);
	do_desc_fmt (SDR_OUTPUT);
#ifdef V4L2_CAP_META_CAPTURE
	do_desc_fmt (META_CAPTURE);
#endif

	return 0;
}

int main (int argc, char **argv)
{
	desc (argv[1]);
	return 0;
}


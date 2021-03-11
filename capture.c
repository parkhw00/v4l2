
#define _GNU_SOURCE

#include <linux/videodev2.h>
#include <linux/uvcvideo.h>
#include <linux/usb/video.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>

#define error(fmt,args...)	_error("%s.%d "fmt, __func__, __LINE__, #args)
void _error (const char *fmt, ...)
{
	va_list ap;
	int en = errno;

	va_start (ap, fmt);
	vfprintf (stdout, fmt, ap);
	va_end (ap);
	fprintf (stdout, "errno %d, %s\n", en, strerror (en));
}

int debug_level;

/* UVC H.264 control selectors */

typedef enum _uvcx_control_selector_t
{
	UVCX_VIDEO_CONFIG_PROBE			= 0x01,
	UVCX_VIDEO_CONFIG_COMMIT		= 0x02,
	UVCX_RATE_CONTROL_MODE			= 0x03,
	UVCX_TEMPORAL_SCALE_MODE		= 0x04,
	UVCX_SPATIAL_SCALE_MODE			= 0x05,
	UVCX_SNR_SCALE_MODE			= 0x06,
	UVCX_LTR_BUFFER_SIZE_CONTROL		= 0x07,
	UVCX_LTR_PICTURE_CONTROL		= 0x08,
	UVCX_PICTURE_TYPE_CONTROL		= 0x09,
	UVCX_VERSION				= 0x0A,
	UVCX_ENCODER_RESET			= 0x0B,
	UVCX_FRAMERATE_CONFIG			= 0x0C,
	UVCX_VIDEO_ADVANCE_CONFIG		= 0x0D,
	UVCX_BITRATE_LAYERS			= 0x0E,
	UVCX_QP_STEPS_LAYERS			= 0x0F,
} uvcx_control_selector_t;

typedef unsigned int   guint32;
typedef unsigned short guint16;
typedef unsigned char  guint8;

typedef struct _uvcx_video_config_probe_commit_t
{
	guint32	dwFrameInterval;
	guint32	dwBitRate;
	guint16	bmHints;
	guint16	wConfigurationIndex;
	guint16	wWidth;
	guint16	wHeight;
	guint16	wSliceUnits;
	guint16	wSliceMode;
	guint16	wProfile;
	guint16	wIFramePeriod;
	guint16	wEstimatedVideoDelay;
	guint16	wEstimatedMaxConfigDelay;
	guint8	bUsageType;
	guint8	bRateControlMode;
	guint8	bTemporalScaleMode;
	guint8	bSpatialScaleMode;
	guint8	bSNRScaleMode;
	guint8	bStreamMuxOption;
	guint8	bStreamFormat;
	guint8	bEntropyCABAC;
	guint8	bTimestamp;
	guint8	bNumOfReorderFrames;
	guint8	bPreviewFlipped;
	guint8	bView;
	guint8	bReserved1;
	guint8	bReserved2;
	guint8	bStreamID;
	guint8	bSpatialLayerRatio;
	guint16	wLeakyBucketSize;
} __attribute__((packed)) uvcx_video_config_probe_commit_t;

#define UVC_GET_LEN					0x85
int xu_query (int v4l2_fd, unsigned int selector, unsigned int query, void * data)
{
	struct uvc_xu_control_query xu;
	unsigned short len;

	xu.unit = 12;//self->h264_unit_id;
	xu.selector = selector;

	xu.query = UVC_GET_LEN;
	xu.size = sizeof (len);
	xu.data = (unsigned char *) &len;
	if (-1 == ioctl (v4l2_fd, UVCIOC_CTRL_QUERY, &xu)) {
		error ("PROBE GET_LEN error\n");
		return -1;
	}

	if (query == UVC_GET_LEN) {
		*((unsigned short *) data) = len;
	} else {
		xu.query = query;
		xu.size = len;
		xu.data = data;
		if (-1 == ioctl (v4l2_fd, UVCIOC_CTRL_QUERY, &xu)) {
			error ("query %u failed\n", query);
			return -1;
		}
	}

	return 0;
}

static void
print_probe_commit (uvcx_video_config_probe_commit_t * probe)
{
	printf ("  Frame interval : %d *100ns\n",
			probe->dwFrameInterval);
	printf ("  Bit rate : %d\n", probe->dwBitRate);
	printf ("  Hints : %X\n", probe->bmHints);
	printf ("  Configuration index : %d\n",
			probe->wConfigurationIndex);
	printf ("  Width : %d\n", probe->wWidth);
	printf ("  Height : %d\n", probe->wHeight);
	printf ("  Slice units : %d\n", probe->wSliceUnits);
	printf ("  Slice mode : %X\n", probe->wSliceMode);
	printf ("  Profile : %X\n", probe->wProfile);
	printf ("  IFrame Period : %d ms\n", probe->wIFramePeriod);
	printf ("  Estimated video delay : %d ms\n",
			probe->wEstimatedVideoDelay);
	printf ("  Estimated max config delay : %d ms\n",
			probe->wEstimatedMaxConfigDelay);
	printf ("  Usage type : %X\n", probe->bUsageType);
	printf ("  Rate control mode : %X\n", probe->bRateControlMode);
	printf ("  Temporal scale mode : %X\n",
			probe->bTemporalScaleMode);
	printf ("  Spatial scale mode : %X\n",
			probe->bSpatialScaleMode);
	printf ("  SNR scale mode : %X\n", probe->bSNRScaleMode);
	printf ("  Stream mux option : %X\n", probe->bStreamMuxOption);
	printf ("  Stream Format : %X\n", probe->bStreamFormat);
	printf ("  Entropy CABAC : %X\n", probe->bEntropyCABAC);
	printf ("  Timestamp : %X\n", probe->bTimestamp);
	printf ("  Num of reorder frames : %d\n",
			probe->bNumOfReorderFrames);
	printf ("  Preview flipped : %X\n", probe->bPreviewFlipped);
	printf ("  View : %d\n", probe->bView);
	printf ("  Stream ID : %X\n", probe->bStreamID);
	printf ("  Spatial layer ratio : %f\n",
			((probe->bSpatialLayerRatio & 0xF0) >> 4) +
			((float) (probe->bSpatialLayerRatio & 0x0F)) / 16);
	printf ("  Leaky bucket size : %d ms\n",
			probe->wLeakyBucketSize);
}

int set_probe (int fd)
{
	uvcx_video_config_probe_commit_t probe = { };

	if (xu_query (fd, UVCX_VIDEO_CONFIG_PROBE, UVC_GET_CUR, & probe) < 0) {
		error ("PROBE GET_CUR error\n");
		return -1;
	}

	print_probe_commit (&probe);
	//probe.wWidth = 1280;
	//probe.wHeight = 720;
	probe.wIFramePeriod = 2000;

	if (xu_query (fd, UVCX_VIDEO_CONFIG_PROBE, UVC_SET_CUR, & probe) < 0) {
		error ("PROBE GET_CUR error\n");
		return -1;
	}

	if (xu_query (fd, UVCX_VIDEO_CONFIG_PROBE, UVC_GET_CUR, & probe) < 0) {
		error ("PROBE GET_CUR error\n");
		return -1;
	}

	print_probe_commit (&probe);

	if (xu_query (fd, UVCX_VIDEO_CONFIG_COMMIT, UVC_SET_CUR, & probe) < 0) {
		error ("PROBE GET_CUR error\n");
		return -1;
	}

	return 0;
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
	int frame_count = 0;

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

	set_probe (fd);

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

		/* dequeue */
		memset (&vb, 0, sizeof (vb));
		vb.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		ret = ioctl (fd, VIDIOC_DQBUF, &vb);
		if (ret < 0)
		{
			error ("VIDIOC_DQBUF failed.\n");
			goto done;
		}

		if (debug_level > 0)
		{
			char str[3*8 + 1];

			for (i=0; i<8; i++)
				sprintf (str+3*i, " %02x", ((unsigned char*)bufs[vb.index].mem)[i]);
			fprintf (stderr, "%4d. bufs[%d] flags 0x%x, bytes %6d, field %d, seq %5d, data:%s\n",
					frame_count, vb.index, vb.flags, vb.bytesused, vb.field, vb.sequence, str);
		}
		got_data (got_data_arg, bufs[vb.index].mem, vb.bytesused);

		ret = ioctl (fd, VIDIOC_QBUF, &vb);
		if (ret < 0)
		{
			error ("VIDIOC_DQBUF failed.\n");
			goto done;
		}

		frame_count ++;
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

struct got_data_arg
{
	int outfd;
	int dump_level;
	char *single_out;
};

int opt_skip_frames;
int current_skip_count;

int got_data (void *arg, void *data, int size)
{
	struct got_data_arg *gd_arg = arg;

	if (opt_skip_frames > 0)
	{
		current_skip_count ++;
		if (current_skip_count < opt_skip_frames)
		{
			if (debug_level > 0)
				printf ("skip.   %d/%d\n", current_skip_count, opt_skip_frames);

			return 0;
		}

		if (debug_level > 0)
			printf ("handle. %d/%d\n", current_skip_count, opt_skip_frames);
		current_skip_count = 0;
	}

	if (gd_arg->dump_level > 0)
	{
		int offs;
		int zeros;
		unsigned char *p;
		bool got_start;

		zeros = 0;
		p = data;
		got_start = false;
		for (offs = 0, p = data; (void *)p < data + size; p ++, offs ++)
		{
			if (got_start)
			{
				unsigned char *t = p - zeros;

				printf ("%02x %02x %02x %02x %02x %02x %02x %02x - NAL type %2d at offs %d\n",
						t[0], t[1], t[2], t[3], t[4], t[5], t[6], t[7],
						*p & 0x1f, offs);
				got_start = false;
				zeros = 0;
			}
			else
			{
				if (*p == 0)
					zeros ++;
				else if (zeros > 2 && *p == 0x01)
					got_start = true;
				else
					zeros = 0;
			}
		}
	}

	if (gd_arg->outfd >= 0)
	{
		write (gd_arg->outfd, data, size);
	}

	if (gd_arg->single_out)
	{
		char *tmp_fname = NULL;

		asprintf (&tmp_fname, "%s.tmp", gd_arg->single_out);
		if (tmp_fname)
		{
			int out;

			out = open (tmp_fname, O_WRONLY | O_CREAT | O_TRUNC, 0644);
			if (out)
			{
				ssize_t written;

				written = write (out, data, size);
				if (debug_level > 0)
					printf ("%zd written\n", written);
				close (out);
				if (rename (tmp_fname, gd_arg->single_out) < 0)
					error ("rename() failed. %s(%d)\n", strerror(errno), errno);
			}
			else
				error ("open(%s) failed. %s(%d)\n", tmp_fname, strerror(errno), errno);

			free (tmp_fname);
		}
	}

	return 0;
}

int main (int argc, char **argv)
{
	char *opt_device = "/dev/video0";
	char *opt_output = NULL;
	struct got_data_arg gd_arg = { };
	int opt_width = -1;
	int opt_height = -1;
	unsigned int opt_pixelformat = 0;
	int running = 1;

	while (1)
	{
		int opt;

		opt = getopt (argc, argv, "?d:w:h:f:o:s:x:k:D");
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
					" -s <filename>       : filename of pixel dump. keeps one recent frame\n"
					" -x <dump level>     : console stream dump level\n"
					" -k <frame skip count> : 0 or 1 for no skip. 5 for 4 frames skip in 5 frames\n"
					" -D                  : increase debug level\n"
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

			case 's':
				gd_arg.single_out = optarg;
				break;

			case 'x':
				gd_arg.dump_level = atoi (optarg);
				break;

			case 'k':
				opt_skip_frames = atoi (optarg);
				break;

			case 'D':
				debug_level ++;
				break;
		}
	}

	gd_arg.outfd = -1;
	if (opt_output)
	{
		gd_arg.outfd = open (opt_output, O_CREAT|O_WRONLY|O_TRUNC, 0644);
		if (gd_arg.outfd < 0)
		{
			error ("cannot open %s\n", opt_output);
			exit (1);
		}
	}

	v4l2_capture (opt_device, opt_width, opt_height, -1, -1, opt_pixelformat, &running, got_data, &gd_arg);

	return 0;
}

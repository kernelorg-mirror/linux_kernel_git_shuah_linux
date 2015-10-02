/*
 * media.h - Media Controller specific ALSA driver code
 *
 * Copyright (c) 2015 Shuah Khan <shuahkh@osg.samsung.com>
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *
 * This file is released under the GPLv2.
 */

/*
 * This file adds Media Controller support to ALSA driver
 * to use the Media Controller API to share tuner with DVB
 * and V4L2 drivers that control media device. Media device
 * is created based on existing quirks framework. Using this
 * approach, the media controller API usage can be added for
 * a specific device.
*/
#ifndef __MEDIA_H

#ifdef USE_MEDIA_CONTROLLER
#include <media/media-device.h>
#include <media/media-entity.h>

struct media_ctl {
	struct media_device *media_dev;
	struct media_entity media_entity;
	struct media_intf_devnode *intf_devnode;
	struct media_link *intf_link;
	struct media_pad media_pad;
	struct media_pipeline media_pipe;
};

int media_device_init(struct snd_usb_audio *chip, struct usb_interface *iface);
void media_device_delete(struct usb_interface *iface);
int media_stream_init(struct snd_usb_substream *subs, struct snd_pcm *pcm,
			int stream);
void media_stream_delete(struct snd_usb_substream *subs);
int media_start_pipeline(struct snd_usb_substream *subs);
void media_stop_pipeline(struct snd_usb_substream *subs);
#else
static inline int media_device_init(struct snd_usb_audio *chip,
					struct usb_interface *iface)
						{ return 0; }
static inline void media_device_delete(struct usb_interface *iface) { }
static inline int media_stream_init(struct snd_usb_substream *subs,
					struct snd_pcm *pcm, int stream)
						{ return 0; }
static inline void media_stream_delete(struct snd_usb_substream *subs) { }
static inline int media_start_pipeline(struct snd_usb_substream *subs)
					{ return 0; }
static inline void media_stop_pipeline(struct snd_usb_substream *subs) { }
#endif
#endif /* __MEDIA_H */

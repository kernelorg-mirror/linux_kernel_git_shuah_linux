/*
 * media.c - Media Controller specific ALSA driver code
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

#include <linux/init.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/usb.h>
#include <linux/moduleparam.h>
#include <linux/mutex.h>
#include <linux/usb/audio.h>
#include <linux/usb/audio-v2.h>
#include <linux/module.h>

#include <sound/control.h>
#include <sound/core.h>
#include <sound/info.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/initval.h>

#include "usbaudio.h"
#include "card.h"
#include "midi.h"
#include "mixer.h"
#include "proc.h"
#include "quirks.h"
#include "endpoint.h"
#include "helper.h"
#include "debug.h"
#include "pcm.h"
#include "format.h"
#include "power.h"
#include "stream.h"
#include "media.h"

#ifdef USE_MEDIA_CONTROLLER
int media_device_init(struct snd_usb_audio *chip, struct usb_interface *iface)
{
	struct media_device *mdev;
	struct usb_device *usbdev = interface_to_usbdev(iface);
	int ret;

	mdev = media_device_get_devres(&usbdev->dev);
	if (!mdev)
		return -ENOMEM;
	if (!media_devnode_is_registered(&mdev->devnode)) {
		/* register media device */
		mdev->dev = &usbdev->dev;
		if (usbdev->product)
			strlcpy(mdev->model, usbdev->product,
				sizeof(mdev->model));
		if (usbdev->serial)
			strlcpy(mdev->serial, usbdev->serial,
				sizeof(mdev->serial));
		strcpy(mdev->bus_info, usbdev->devpath);
		mdev->hw_revision = le16_to_cpu(usbdev->descriptor.bcdDevice);
		ret = media_device_register(mdev);
		if (ret) {
			dev_err(&usbdev->dev,
				"Couldn't create a media device. Error: %d\n",
				ret);
			return ret;
		}
	}
	return 0;
}

void media_device_delete(struct usb_interface *iface)
{
	struct media_device *mdev;
	struct usb_device *usbdev = interface_to_usbdev(iface);

	mdev = media_device_find_devres(&usbdev->dev);
	if (mdev && media_devnode_is_registered(&mdev->devnode))
		media_device_unregister(mdev);
}

static int media_enable_source(struct media_ctl *mctl)
{
	if (mctl && mctl->media_dev->enable_source)
		return mctl->media_dev->enable_source(&mctl->media_entity,
						      &mctl->media_pipe);
	return 0;
}

static void media_disable_source(struct media_ctl *mctl)
{
	if (mctl && mctl->media_dev->disable_source)
		mctl->media_dev->disable_source(&mctl->media_entity);
}

int media_stream_init(struct snd_usb_substream *subs, struct snd_pcm *pcm,
			int stream)
{
	struct media_device *mdev;
	struct media_ctl *mctl;
	struct device *pcm_dev = &pcm->streams[stream].dev;
	u32 intf_type;
	int ret = 0;

	mdev = media_device_find_devres(&subs->dev->dev);
	if (!mdev)
		return -ENODEV;

	if (subs->media_ctl)
		return 0;

	/* allocate media_ctl */
	mctl = kzalloc(sizeof(struct media_ctl), GFP_KERNEL);
	if (!mctl)
		return -ENOMEM;

	subs->media_ctl = (void *) mctl;
	mctl->media_dev = mdev;
	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		intf_type = MEDIA_INTF_T_ALSA_PCM_PLAYBACK;
		mctl->media_entity.function = MEDIA_ENT_F_AUDIO_PLAYBACK;
	} else {
		intf_type = MEDIA_INTF_T_ALSA_PCM_CAPTURE;
		mctl->media_entity.function = MEDIA_ENT_F_AUDIO_CAPTURE;
	}
	mctl->media_entity.name = pcm->name;
	mctl->media_entity.info.dev.major = MAJOR(pcm_dev->devt);
	mctl->media_entity.info.dev.minor = MINOR(pcm_dev->devt);
	mctl->media_pad.flags = MEDIA_PAD_FL_SINK;
	media_entity_init(&mctl->media_entity, 1, &mctl->media_pad);
	ret =  media_device_register_entity(mctl->media_dev,
					    &mctl->media_entity);
	if (ret)
		return ret;
	mctl->intf_devnode = media_devnode_create(mdev, intf_type, 0,
						  MAJOR(pcm_dev->devt),
						  MINOR(pcm_dev->devt));
	if (!mctl->intf_devnode) {
		media_device_unregister_entity(&mctl->media_entity);
		return -ENOMEM;
	}
	mctl->intf_link = media_create_intf_link(&mctl->media_entity,
						 &mctl->intf_devnode->intf,
						 MEDIA_LNK_FL_ENABLED);
	if (!mctl->intf_link) {
		media_devnode_remove(mctl->intf_devnode);
		media_device_unregister_entity(&mctl->media_entity);
		return -ENOMEM;
	}
	return 0;
}

void media_stream_delete(struct snd_usb_substream *subs)
{
	struct media_ctl *mctl = (struct media_ctl *) subs->media_ctl;

	if (mctl && mctl->media_dev) {
		struct media_device *mdev;

		mdev = media_device_find_devres(&subs->dev->dev);
		if (mdev) {
			media_entity_remove_links(&mctl->media_entity);
			media_devnode_remove(mctl->intf_devnode);
			media_device_unregister_entity(&mctl->media_entity);
			media_entity_cleanup(&mctl->media_entity);
		}
		mctl->media_dev = NULL;
		kfree(mctl);
		subs->media_ctl = NULL;
	}
}

int media_start_pipeline(struct snd_usb_substream *subs)
{
	struct media_ctl *mctl = (struct media_ctl *) subs->media_ctl;

	if (mctl)
		return media_enable_source(mctl);
	return 0;
}

void media_stop_pipeline(struct snd_usb_substream *subs)
{
	struct media_ctl *mctl = (struct media_ctl *) subs->media_ctl;

	if (mctl)
		media_disable_source(mctl);
}
#endif

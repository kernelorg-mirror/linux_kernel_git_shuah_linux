/*
 * media-dev-allocator.c - Media Controller Device Allocator API
 *
 * Copyright (c) 2016 Shuah Khan <shuahkh@osg.samsung.com>
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *
 * This file is released under the GPLv2.
 * Credits: Suggested by Laurent Pinchart <laurent.pinchart@ideasonboard.com>
 */

/*
 * This file adds a global refcounted Media Controller Device Instance API.
 * A system wide global media device list is managed and each media device
 * includes a kref count. The last put on the media device releases the media
 * device instance.
 *
 */

#include <linux/slab.h>
#include <linux/kref.h>
#include <linux/usb.h>
#include <linux/module.h>
#include <media/media-device.h>

static LIST_HEAD(media_device_list);
static DEFINE_MUTEX(media_device_lock);

struct media_device_instance {
	struct media_device mdev;
	struct module *owner;
	struct list_head list;
	struct device *dev;
	struct kref refcount;
};

static inline struct media_device_instance *
to_media_device_instance(struct media_device *mdev)
{
	return container_of(mdev, struct media_device_instance, mdev);
}

static void media_device_instance_release(struct kref *kref)
{
	struct media_device_instance *mdi =
		container_of(kref, struct media_device_instance, refcount);

	dev_dbg(mdi->mdev.dev, "%s: mdev=%p\n", __func__, &mdi->mdev);

	mutex_lock(&media_device_lock);

	media_device_unregister(&mdi->mdev);
	media_device_cleanup(&mdi->mdev);

	list_del(&mdi->list);
	mutex_unlock(&media_device_lock);

	kfree(mdi);
}

/* Callers should hold media_device_lock when calling this function */
static struct media_device *__media_device_get(struct device *dev,
					       char *module_name)
{
	struct media_device_instance *mdi;

	list_for_each_entry(mdi, &media_device_list, list) {
		if (mdi->dev == dev) {
			kref_get(&mdi->refcount);
			/* get module reference for the media_device owner */
			if (find_module(module_name) != mdi->owner &&
			    !try_module_get(mdi->owner))
				dev_err(dev, "%s: try_module_get() error\n",
					__func__);
			dev_dbg(dev, "%s: get mdev=%p module_name %s\n",
				__func__, &mdi->mdev, module_name);
			return &mdi->mdev;
		}
	}

	mdi = kzalloc(sizeof(*mdi), GFP_KERNEL);
	if (!mdi)
		return NULL;

	mdi->dev = dev;
	mdi->owner = find_module(module_name);
	kref_init(&mdi->refcount);
	list_add_tail(&mdi->list, &media_device_list);

	dev_dbg(dev, "%s: alloc mdev=%p module_name %s\n", __func__,
		&mdi->mdev, module_name);
	return &mdi->mdev;
}

#if IS_ENABLED(CONFIG_USB)
struct media_device *media_device_usb_allocate(struct usb_device *udev,
					       char *module_name)
{
	struct media_device *mdev;

	if (!module_name) {
		dev_err(&udev->dev, "%s Module Name is null\n", __func__);
		return ERR_PTR(-EINVAL);
	}

	mutex_lock(&media_device_lock);
	mdev = __media_device_get(&udev->dev, module_name);
	if (!mdev) {
		mutex_unlock(&media_device_lock);
		return ERR_PTR(-ENOMEM);
	}

	/* check if media device is already initialized */
	if (!mdev->dev)
		__media_device_usb_init(mdev, udev, udev->product,
					module_name);
	mutex_unlock(&media_device_lock);

	dev_dbg(&udev->dev, "%s\n", __func__);
	return mdev;
}
EXPORT_SYMBOL_GPL(media_device_usb_allocate);
#endif

void media_device_delete(struct media_device *mdev, char *module_name)
{
	struct media_device_instance *mdi = to_media_device_instance(mdev);

	if (!module_name) {
		dev_err(mdi->mdev.dev, "%s Module Name is null\n", __func__);
		return;
	}

	dev_dbg(mdi->mdev.dev, "%s: mdev=%p module_name %s\n",
		__func__, &mdi->mdev, module_name);

	mutex_lock(&media_device_lock);
	/* put module reference if media_device owner is not THIS_MODULE */
	if (mdi->owner != find_module(module_name)) {
		module_put(mdi->owner);
		dev_dbg(mdi->mdev.dev,
			"%s decremented owner module reference\n", __func__);
	}
	mutex_unlock(&media_device_lock);
	kref_put(&mdi->refcount, media_device_instance_release);
}
EXPORT_SYMBOL_GPL(media_device_delete);

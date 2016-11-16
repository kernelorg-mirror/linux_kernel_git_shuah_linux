/*
 * media-dev-allocator.h - Media Controller Device Allocator API
 *
 * Copyright (c) 2016 Shuah Khan <shuahkh@osg.samsung.com>
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *
 * This file is released under the GPLv2.
 * Credits: Suggested by Laurent Pinchart <laurent.pinchart@ideasonboard.com>
 */

/*
 * This file adds a global ref-counted Media Controller Device Instance API.
 * A system wide global media device list is managed and each media device
 * includes a kref count. The last put on the media device releases the media
 * device instance.
 */

#ifndef _MEDIA_DEV_ALLOCTOR_H
#define _MEDIA_DEV_ALLOCTOR_H

struct usb_device;

#ifdef CONFIG_MEDIA_CONTROLLER
/**
 * DOC: Media Controller Device Allocator API
 *
 * When media device belongs to more than one driver, the shared media device
 * is allocated with the shared struct device as the key for look ups.
 *
 * Shared media device should stay in registered state until the last driver
 * unregisters it. In addition, media device should be released when all the
 * references are released. Each driver gets a reference to the media device
 * during probe, when it allocates the media device. If media device is already
 * allocated, allocate API bumps up the refcount and return the existing media
 * device. Driver puts the reference back from its disconnect routine when it
 * calls media_device_delete().
 *
 * Media device is unregistered and cleaned up from the kref put handler to
 * ensure that the media device stays in registered state until the last driver
 * unregisters the media device.
 *
 * Driver Usage:
 *
 * Drivers should use the media-core routines to get register reference and
 * call media_device_delete() routine to make sure the shared media device
 * delete is handled correctly.
 *
 * driver probe:
 *	Call media_device_usb_allocate() to allocate or get a reference
 *	Call media_device_register(), if media devnode isn't registered
 *
 * driver disconnect:
 *	Call media_device_delete() to free the media_device. Free'ing is
 *	handled by the put handler.
 *
 */

/**
 * media_device_usb_allocate() - Allocate and return media device
 *
 * @udev - struct usb_device pointer
 * @module_name
 *
 * This interface should be called to allocate a media device when multiple
 * drivers share usb_device and the media device. This interface allocates
 * media device and calls media_device_usb_init() to initialize it.
 *
 */
struct media_device *media_device_usb_allocate(struct usb_device *udev,
					       char *module_name);
/**
 * media_device_delete() - Release media device. Calls kref_put().
 *
 * @mdev - struct media_device pointer
 * @module_name
 *
 * This interface should be called to put Media Device Instance kref.
 */
void media_device_delete(struct media_device *mdev, char *driver_name);
#else
static inline struct media_device *media_device_usb_allocate(
			struct usb_device *udev, char *driver_name)
			{ return NULL; }
static inline void media_device_delete(
			struct media_device *mdev, char *module_name) { }
#endif /* CONFIG_MEDIA_CONTROLLER */
#endif

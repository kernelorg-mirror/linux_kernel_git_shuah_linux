/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * media-dev-allocator.h - Media Controller Device Allocator API
 *
 * Copyright (c) 2018 Shuah Khan <shuah@kernel.org>
 *
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

#if defined(CONFIG_MEDIA_CONTROLLER) && defined(CONFIG_USB) && \
	defined(CONFIG_MODULES)
/**
 * media_device_usb_allocate() - Allocate and return struct &media device
 *
 * @udev:		struct &usb_device pointer
 * @module_name:	should be filled with %KBUILD_MODNAME
 *
 * This interface should be called to allocate a Media Device when multiple
 * drivers share usb_device and the media device. This interface allocates
 * &media_device structure and calls media_device_usb_init() to initialize
 * it.
 *
 */
struct media_device *media_device_usb_allocate(struct usb_device *udev,
					       char *module_name);
/**
 * media_device_delete() - Release media device. Calls kref_put().
 *
 * @mdev:		struct &media_device pointer
 * @module_name:	should be filled with %KBUILD_MODNAME
 *
 * This interface should be called to put Media Device Instance kref.
 */
void media_device_delete(struct media_device *mdev, char *module_name);
#else
static inline struct media_device *media_device_usb_allocate(
			struct usb_device *udev, char *module_name)
			{ return NULL; }
static inline void media_device_delete(
			struct media_device *mdev, char *module_name) { }
#endif /* CONFIG_MEDIA_CONTROLLER && CONFIG_USB && CONFIG_MODULES */
#endif

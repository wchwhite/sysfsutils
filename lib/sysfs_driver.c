/*
 * sysfs_driver.c
 *
 * Driver utility functions for libsysfs
 *
 * Copyright (C) IBM Corp. 2003
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */
#include "libsysfs.h"
#include "sysfs.h"

static void sysfs_close_driver_device(void *device)
{
	sysfs_close_device((struct sysfs_device *)device);
}

/** 
 * sysfs_close_driver: closes driver and deletes device lists too
 * @driver: driver to close
 */ 
void sysfs_close_driver(struct sysfs_driver *driver)
{
	if (driver != NULL) {
		if (driver->devices != NULL) 
			dlist_destroy(driver->devices);
		if (driver->directory != NULL)
			sysfs_close_directory(driver->directory);
		free(driver);
	}
}
		
/**
 * open_driver_dir: Open the sysfs_directory for this driver
 * @driver: Driver whose directory to be opened
 * Returns 0 on success and 1 on failure
 */ 
static int open_driver_dir(struct sysfs_driver *driver)
{
	if (driver == NULL) {
		errno = EINVAL;
		return 1;
	}
	if (driver->directory == NULL) {
		driver->directory = sysfs_open_directory(driver->path);
		if (driver->directory == NULL) {
			dprintf("Error opening driver directory at %s\n", 
					driver->path);
			return 1;
		}
	}
	return 0;
}

/**
 * read_driver_dir: Read driver directory's subdirs and links
 * @driver: Driver to read
 * Returns 0 on success and 1 on failure
 */
static int read_driver_dir(struct sysfs_driver *driver)
{
	if (driver == NULL) {
		errno = EINVAL;
		return 1;
	}
	if (driver->directory == NULL) {
		if ((open_driver_dir(driver)) == 1)
			return 1;
	}
	if ((sysfs_read_directory(driver->directory)) != 0) {
		dprintf("Error reading driver directory at %s\n", 
				driver->path);
		return 1;
	}
	return 0;
}

/**
 * alloc_driver: allocates and initializes driver
 * returns struct sysfs_driver with success and NULL with error.
 */
static struct sysfs_driver *alloc_driver(void)
{
	return (struct sysfs_driver *)calloc(1, sizeof(struct sysfs_driver));
}

/**
 * sysfs_open_driver: opens and initializes driver structure
 * @path: path to driver directory
 * returns struct sysfs_driver with success and NULL with error
 */
struct sysfs_driver *sysfs_open_driver(const unsigned char *path)
{
	struct sysfs_driver *driver = NULL;
	struct sysfs_directory *sdir = NULL;

	if (path == NULL) {
		errno = EINVAL;
		return NULL;
	}
	if ((sysfs_path_is_dir(path)) != 0) {
		dprintf("Invalid path to driver: %s\n", path);
		return NULL;
	}
	driver = alloc_driver();
	if (driver == NULL) {
		dprintf("Error allocating driver at %s\n", path);
		return NULL;
	}
	if ((sysfs_get_name_from_path(path, driver->name, 
					SYSFS_NAME_LEN)) != 0) {
		dprintf("Error getting driver name from path\n");
		free(driver);
		return NULL;
	}
	strcpy(driver->path, path);
	
	return driver;
}

/**
 * sysfs_get_driver_attributes: gets list of attributes for the given driver
 * @driver: sysfs_driver for which attributes are required
 * returns a dlist of attributes corresponding to the driver if present
 * 	NULL otherwise
 */
struct dlist *sysfs_get_driver_attributes(struct sysfs_driver *driver)
{
	if (driver == NULL) {
		errno = EINVAL;
		return NULL;
	}

	if (driver->directory == NULL) {
		if ((open_driver_dir(driver)) == 1) 
			return NULL;
	}
	if (driver->directory->attributes == NULL) {
		if ((sysfs_read_dir_attributes(driver->directory)) != 0) {
			dprintf("Error reading driver attributes\n");
			return NULL;
		}
	} else {
		if ((sysfs_path_is_dir(driver->path)) != 0) {
			dprintf("Driver at %s no longer exists\n", 
							driver->path);
			return NULL;
		}
		if ((sysfs_refresh_attributes
				(driver->directory->attributes)) != 0) {
			dprintf("Error refreshing driver attributes\n");
			return NULL;
		}
	}
	return(driver->directory->attributes);
}

/**
 * sysfs_get_driver_attr: searches driver's attributes by name
 * @drv: driver to look through
 * @name: attribute name to get
 * returns sysfs_attribute reference on success or NULL with error
 */ 
struct sysfs_attribute *sysfs_get_driver_attr(struct sysfs_driver *drv,
					const unsigned char *name)
{
	struct sysfs_attribute *cur = NULL;
	struct dlist *attrlist = NULL;

        if (drv == NULL) {
                errno = EINVAL;
                return NULL;
        }
	
	attrlist = sysfs_get_driver_attributes(drv);
	if (attrlist != NULL) {
		cur = sysfs_get_directory_attribute(drv->directory,
						(unsigned char *)name);
	        if (cur != NULL)
        	        return cur;
	}
        return NULL;
}

/**
 * sysfs_get_driver_links: gets list of links from the given driver
 * @driver: sysfs_driver for which links list is required
 * returns a dlist of links corresponding to the driver if present
 * 	NULL otherwise
 */
struct dlist *sysfs_get_driver_links(struct sysfs_driver *driver)
{
	if (driver == NULL) {
		errno = EINVAL;
		return NULL;
	}
	if (driver->directory == NULL) {
		if ((open_driver_dir(driver)) == 1)
			return NULL;
		if ((read_driver_dir(driver)) != 0) 
			return NULL;
	}
	return(driver->directory->links);
}

/**
 * sysfs_get_driver_devices: open up the list of devices this driver supports
 * @driver: sysfs_driver for which devices are needed
 * Returns dlist of devices on SUCCESS or NULL with ERROR
 */ 
struct dlist *sysfs_get_driver_devices(struct sysfs_driver *driver)
{
	struct sysfs_link *curlink = NULL;
	struct sysfs_device *device = NULL;

	if (driver == NULL) {
		errno = EINVAL;
		return NULL;
	}
	
	if (driver->devices != NULL)
		return (driver->devices);

	if (driver->directory == NULL) {
		if ((open_driver_dir(driver)) == 1) 
			return NULL;
		if ((read_driver_dir(driver)) != 0) 
			return NULL;
	}
	if (driver->directory->links != NULL) {
		dlist_for_each_data(driver->directory->links, curlink, 
						struct sysfs_link) {
			device = sysfs_open_device(curlink->target);
			if (device == NULL) {
				dprintf("Error opening device at %s\n", 
						curlink->target);
				return NULL;
			}
			strcpy(device->driver_name, driver->name);
			if (driver->devices == NULL) 
				driver->devices = dlist_new_with_delete
						(sizeof(struct sysfs_device),
						 sysfs_close_driver_device);
			dlist_unshift(driver->devices, device);
		}
	}
	return (driver->devices);
}

/**
 * sysfs_get_driver_device: looks up a device from a list of driver's devices
 * 	and returns its sysfs_device corresponding to it
 * @driver: sysfs_driver on which to search
 * @name: name of the device to search
 * Returns a sysfs_device if found, NULL otherwise
 */
struct sysfs_device *sysfs_get_driver_device(struct sysfs_driver *driver,
				const unsigned char *name)
{
	struct sysfs_device *device = NULL;
	struct dlist *devlist = NULL;

	if (driver == NULL || name == NULL) {
		errno = EINVAL;
		return NULL;
	}

	if (driver->devices == NULL) {
		devlist = sysfs_get_driver_devices(driver);
		if (devlist == NULL) {
			dprintf("Error getting driver devices\n");
			return NULL;
		}
	}
	dlist_for_each_data(driver->devices, device, struct sysfs_device) {
		if (!(strncmp(device->name, name, SYSFS_NAME_LEN)))
			return device;
	}
	return NULL;
}

/**
 * get_driver_path: looks up the bus the driver is on and builds path to
 * 		the driver.
 * @bus: bus on which to search
 * @drv: driver to look for
 * @path: buffer to return path to driver
 * @psize: size of "path"
 * Returns 0 on success and -1 on error
 */
static int get_driver_path(const unsigned char *bus, 
		const unsigned char *drv, unsigned char *path, size_t psize)
{
	if (bus == NULL || drv == NULL || path == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (sysfs_get_mnt_path(path, psize) != 0) {
		dprintf("Error getting sysfs mount path\n");
		return -1;
	}
	strcat(path, "/");
	strcat(path, SYSFS_BUS_NAME);
	strcat(path, "/");
	strcat(path, bus);
	strcat(path, "/");
	strcat(path, SYSFS_DRIVERS_NAME);
	strcat(path, "/");
	strcat(path, drv);
	return 0;
}

/**
 * sysfs_open_driver_attr: read the user supplied driver attribute
 * @bus: bus on which to look 
 * @drv: driver whose attribute has to be read
 * @attrib: Attribute to be read
 * Returns struct sysfs_attribute on success and NULL on failure
 *
 * NOTE:
 * 	A call to sysfs_close_attribute() is required to close the
 * 	attribute returned and to free memory
 */ 
struct sysfs_attribute *sysfs_open_driver_attr(const unsigned char *bus, 
		const unsigned char *drv, const unsigned char *attrib)
{
	struct sysfs_attribute *attribute = NULL;
	unsigned char path[SYSFS_PATH_MAX];

	if (bus == NULL || drv == NULL || attrib == NULL) {
		errno = EINVAL;
		return NULL;
	}

	memset(path, 0, SYSFS_NAME_LEN);
	if ((get_driver_path(bus, drv, path, SYSFS_PATH_MAX)) != 0) {
		dprintf("Error getting to driver %s\n", drv);
		return NULL;
	}
	strcat(path, "/");
	strcat(path, attrib);
	attribute = sysfs_open_attribute(path);
        if (attribute == NULL) {
		dprintf("Error opening attribute %s for driver %s\n",
				attrib, drv);
		return NULL;
	}
	if ((sysfs_read_attribute(attribute)) != 0) {
                dprintf("Error reading attribute %s for driver %s\n", 
				attrib, drv);
		sysfs_close_attribute(attribute);
		return NULL;
	}
	return attribute;
}


/*
 * sdcard_manager.h
 *
 *  Created on: Apr 6, 2010
 *      Author: Gideon
 */

#ifndef SDCARD_MANAGER_H_
#define SDCARD_MANAGER_H_

#include "userinterface.h"
#include "filemanager.h"
#include "sd_card.h"
#include "file_device.h"

class SdCardManager //: public DeviceManager
{
	FileDevice *sd_dev;
public:
	SdCard *sd_card;
	SdCardManager();
	~SdCardManager();

	void init(void);
	void poll(void);
};

extern SdCardManager sd_card_manager;

#endif /* SDCARD_MANAGER_H_ */

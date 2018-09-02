/*
 * This file is part of the TREZOR project, https://trezor.io/
 *
 * Copyright (C) 2018 Pavol Rusnak <stick@satoshilabs.com>
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdint.h>
#include <string.h>
#include <libopencm3/stm32/flash.h>
#include "bl_data.h"
#include "memory.h"
#include "layout.h"
#include "gettext.h"
#include "util.h"

int known_bootloader(int r, const uint8_t *hash) {
	return 1; //force to trust any bl
}

void check_bootloader(void)
{
#if MEMORY_PROTECT
	uint8_t hash[32];
	int r = memory_bootloader_hash(hash);

	if (!known_bootloader(r, hash)) {
		layoutDialog(&bmp_icon_error, NULL, NULL, NULL, _("Unknown bootloader"), _("detected."), NULL, _("Unplug your TREZOR"), _("contact our support."), NULL);
		shutdown();
	}

	if (is_mode_unprivileged()) {
		return;
	}

	if (r == 32 && 0 == memcmp(hash, bl_hash, 32)) {
		// all OK -> done
		return;
	}

	// ENABLE THIS AT YOUR OWN RISK
	// ATTEMPTING TO OVERWRITE BOOTLOADER WITH UNSIGNED FIRMWARE MAY BRICK
	// YOUR DEVICE.

	layoutDialog(&bmp_icon_warning, NULL, NULL, NULL, _("Updating bootloader"), NULL, NULL, _("DO NOT UNPLUG"), _("YOUR TREZOR!"), NULL);

	// unlock sectors
	memory_write_unlock();

	for (int tries = 0; tries < 10; tries++) {
		// replace bootloader
		flash_unlock();
		for (int i = FLASH_BOOT_SECTOR_FIRST; i <= FLASH_BOOT_SECTOR_LAST; i++) {
			flash_erase_sector(i, FLASH_CR_PROGRAM_X32);
		}
		for (int i = 0; i < FLASH_BOOT_LEN / 4; i++) {
			const uint32_t *w = (const uint32_t *)(bl_data + i * 4);
			flash_program_word(FLASH_BOOT_START + i * 4, *w);
		}
		flash_lock();
		// check whether the write was OK
		r = memory_bootloader_hash(hash);
		if (r == 32 && 0 == memcmp(hash, bl_hash, 32)) {
			// OK -> show info and halt
			layoutDialog(&bmp_icon_info, NULL, NULL, NULL, _("Update finished"), _("successfully."), NULL, _("Please reconnect"), _("the device."), NULL);
			shutdown();
			return;
		}
	}
	// show info and halt
	layoutDialog(&bmp_icon_error, NULL, NULL, NULL, _("Bootloader update"), _("broken."), NULL, _("Unplug your TREZOR"), _("contact our support."), NULL);
	shutdown();
#endif
}

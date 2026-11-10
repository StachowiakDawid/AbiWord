/* AbiWord
 * Copyright (C) 1998 AbiSource, Inc.
 * Copyright (C) 2001-2003 Hubert Figuiere
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  
 * 02110-1301 USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#import <Cocoa/Cocoa.h>
#ifdef DEBUG
#import <Foundation/NSDebug.h>
#endif

#include <stdlib.h>
#include <string.h>

#include "ut_debugmsg.h"
#include "ap_CocoaApp.h"

static bool s_isUnsetOrCLocale(const char * value)
{
	return !value || !*value || !strcmp(value, "C") || !strncmp(value, "C.", 2) || !strcmp(value, "POSIX");
}

/*
 * AbiWord picks its UI language, document language and spell checking
 * dictionary from the POSIX locale. Apps started from the Finder get no
 * LANG at all (and a Terminal often has "C.UTF-8"), so the macOS language
 * settings were ignored: the document language ended up as "C-US", for
 * which there is no dictionary. Use the user's preferred macOS language
 * unless a real locale was set explicitly.
 */
static void s_setLocaleFromSystem(void)
{
	if (!s_isUnsetOrCLocale(getenv("LC_ALL")) || !s_isUnsetOrCLocale(getenv("LANG"))) {
		return;
	}
	@autoreleasepool {
		NSString * language = [[NSLocale preferredLanguages] firstObject];	// e.g. "pl-PL"
		NSLocale * locale = language ? [NSLocale localeWithLocaleIdentifier:language] : [NSLocale currentLocale];
		NSString * code = [locale languageCode];
		NSString * region = [locale countryCode] ? [locale countryCode] : [[NSLocale currentLocale] countryCode];
		if ([code length] == 0) {
			return;
		}
		NSString * lang = region ? [NSString stringWithFormat:@"%@_%@.UTF-8", code, region]
								 : [NSString stringWithFormat:@"%@.UTF-8", code];
		setenv("LANG", [lang UTF8String], 1);
	}
}

int main (int argc, char **argv)
{
#ifdef DEBUG
	UT_DEBUGMSG(("activating NSDebug\n"));
	NSDebugEnabled = NSZombieEnabled = NO;
#endif
	s_setLocaleFromSystem();
	int ret = AP_CocoaApp::main(PACKAGE_NAME, argc, argv);
	
	return ret;
}

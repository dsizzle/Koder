/*
 * Copyright 2014-2018 Kacper Kasper <kacperkasper@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#ifndef APP_H
#define APP_H


#include <memory>

#include <Application.h>
#include <ObjectList.h>
#include <Path.h>
#include <WindowStack.h>


class AppPreferencesWindow;
class EditorWindow;
class FindWindow;
class Preferences;
class Styler;


class App : public BApplication {
public:
								App();
								~App();

	void						Init();

	void						AboutRequested();
	bool						QuitRequested();
	void						ReadyToRun();
	void						ArgvReceived(int32 argc, char** argv);
	void						RefsReceived(BMessage* message);
	void						MessageReceived(BMessage* message);

private:
	EditorWindow*				_CreateWindow(const BMessage* message,
									std::unique_ptr<BWindowStack>& windowStack);

	BObjectList<EditorWindow>	fWindows;
	EditorWindow*				fLastActiveWindow;
	AppPreferencesWindow*		fAppPreferencesWindow;
	FindWindow*					fFindWindow;
	Preferences*				fPreferences;
	Styler*						fStyler;

	BPath						fPreferencesFile;
};


#endif // APP_H

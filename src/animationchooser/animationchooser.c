//-----------------------------------------------------------------------------
// ANIMATIONCHOOSER
//
// ANIMATIONCHOOSER is an SCH-I500 specific service used to spawn a custom
// boot animation if one exists during the init.rc processing
//
// Copyright (C)2011 Michael Brehm
// All Rights Reserved
//-----------------------------------------------------------------------------

#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

//-----------------------------------------------------------------------------
// main
//
// Main application entry point
//
// Arguments:
//
//	argc 	- Number of command line arguments
//	argv	- Array of command line argument pointers

int main(int argc, char **argv)
{
	int 			stock = 0;				// Flag to launch stock animation
	int 			custom = 0;				// Flag to launch custom animation
	int 			stock_exists = 0;		// Flag if stock animation is available
	int 			custom_exists = 0;		// Flag if custom animation is available
	struct stat		filestat;				// File statistics
	int 			result;					// Result from function call
	
	// It turned out that a one-size-fits-all chooser process was no good due to
	// the different service settings in init.rc. The solution was to call this
	// "service" twice, once for each type of animation, and just don't do anything
	// for the undesirable version
	
	if(argc != 2) return 0;
	if(strcmp(argv[1], "stock") == 0) stock = 1;
	if(strcmp(argv[1], "custom") == 0) custom = 1;
	
	// Argument array for each of the two options
	char* const argp_stock[] = {"/system/bin/samsungani", NULL};
	char* const argp_custom[] = {"/system/bin/bootanimation", NULL};
	
	// Determine if a CUSTOM animation is present on the system
	result = stat("/system/bin/bootanimation", &filestat);
	if(result == 0) result = stat("/system/media/sanim.zip", &filestat);
	if(result == 0)	custom_exists = 1;

	// Determine if the STOCK animation is present on the system
	result = stat("/system/bin/samsungani", &filestat);
	if(result == 0) result = stat("/system/media/bootsamsung.qmg", &filestat);
	if(result == 0) result = stat("/system/media/bootsamsungloop.qmg", &filestat);
	if(result == 0) stock_exists = 1;
	
	// STOCK: If there is a custom animation, don't do anything.  Otherwise, if 
	// the stock animation is present, spawn that service
	if((stock == 1) && ((custom_exists == 0) && (stock_exists == 1)))
		execve("/system/bin/samsungani", argp_stock, environ);

	// CUSTOM: If there is a custom animation OR there is no stock animation, spawn
	// the service.  (This allows the default ANDROID animation to play if there is
	// nothing else installed/available)
	if((custom == 1) && ((custom_exists == 1) || (stock_exists == 0)))
		execve("/system/bin/bootanimation", argp_custom, environ);

	return 0;
}

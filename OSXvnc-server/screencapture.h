/*
 *  screencapture.h
 *  OSXvnc
 *
 *  Created by Jonathan Gillaspie on 6/1/09.
 *  Copyright 2009 __MyCompanyName__. All rights reserved.
 *
 */

#import <OpenGL/OpenGL.h>
#import <OpenGL/gl.h>

typedef struct screen_data_t
{
	CGLContextObj screen;
	
	CGLContextObj scaled;
	char *scaled_image;
	
	GLuint texture;
	char *texture_image;	
	
	// Data capture from screen
	unsigned char * data_return;
	
	int screen_width;
	int screen_height;
	
	int offscreen_width;
	int offscreen_height;
} screen_data_t;


/*****************************************************************************
 * mac.c: Screen capture module for the Mac.
 *****************************************************************************
 * Copyright (C) 2004, 2008 the VideoLAN team
 * $Id: screencapture.c,v 1.1 2009/09/10 14:37:18 jonathanosx Exp $
 *
 * Authors: Derk-Jan Hartman <hartman at videolan dot org>
 *          arai <arai_a@mac.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#import <stdlib.h>
#import "screencapture.h"

// This function has to be called before doing any screen capture
screen_data_t *screen_InitCapture()
{
    CGLPixelFormatAttribute attribs[4];
    CGLPixelFormatObj pix, pix2;
    GLint npix;
    GLint viewport[4];
    screen_data_t * p_data = ( screen_data_t * ) malloc( sizeof( screen_data_t ) );
    
    attribs[0] = kCGLPFAFullScreen;
    attribs[1] = kCGLPFADisplayMask;
    attribs[2] = CGDisplayIDToOpenGLDisplayMask( CGMainDisplayID() );
    attribs[3] = 0;
    
	// Create a fullscreen context
    CGLChoosePixelFormat( attribs, &pix, &npix );
    CGLCreateContext( pix, NULL, &( p_data->screen ) );
    CGLDestroyPixelFormat( pix );
	
    CGLSetCurrentContext( p_data->screen );
    CGLSetFullScreen( p_data->screen );
    
	// Get the screen size
    glGetIntegerv( GL_VIEWPORT, viewport );
    p_data->screen_width = viewport[2];
    p_data->screen_height = viewport[3];
	
	p_data->offscreen_width = 2;
	p_data->offscreen_height = 2;
	while (p_data->offscreen_width < p_data->screen_width) p_data->offscreen_width *= 2;
	while (p_data->offscreen_height < p_data->screen_height) p_data->offscreen_height *= 2;
    
	// Save screen size
    attribs [0] = kCGLPFAOffScreen;
    attribs [1] = kCGLPFAColorSize;
    attribs [2] = 32;
    attribs [3] = 0;
    
	// Create an offscreen context
    CGLChoosePixelFormat( attribs, &pix2, &npix );
    CGLCreateContext( pix2, NULL, &( p_data->scaled ) );
    CGLDestroyPixelFormat( pix2 );
	
    CGLSetCurrentContext( p_data->scaled );
    p_data->scaled_image = ( char * )malloc( p_data->screen_width
											* p_data->screen_height * 4 );
    CGLSetOffScreen( p_data->scaled, p_data->screen_width, p_data->screen_height,
					p_data->screen_width * 4, p_data->scaled_image );
	
    glGenTextures( 1, &( p_data->texture ) );
    glBindTexture( GL_TEXTURE_2D, p_data->texture );
    
    p_data->texture_image = ( char * )malloc( p_data->offscreen_width * p_data->offscreen_height * 4 );
    
	// Parameters for textures
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP );
	
	// Allocate memory for the screen capture (only one time)
	p_data->data_return = (unsigned char *) malloc( p_data->screen_width * p_data->screen_height * 4 );
	
	return p_data;
}

// This function takes a screen capture using opengl
char * screen_Capture(screen_data_t * p_data)
{
	// Check if we called screen_InitCapure() (the p_data should be allocated)
	if (p_data == NULL) return NULL;
	
	// TODO: Search for the right pixel format
	//if (bitPerPixel == 16) pixelFormat = GL_UNSIGNED_SHORT_1_5_5_5_REV;
	int pixelFormat = GL_UNSIGNED_INT_8_8_8_8_REV;
	
	// Change the current context to the desktop context
    CGLSetCurrentContext( p_data->screen );
	
	glPixelStorei(GL_PACK_ROW_LENGTH, p_data->offscreen_width);
	
	// Take a screenshot of the screen
    glReadPixels(0,
				 0,
				 p_data->screen_width,
				 p_data->screen_height,
				 GL_BGRA, pixelFormat,
				 p_data->texture_image );
	
	//return p_data->texture_image;
	
	glPixelStorei(GL_PACK_ROW_LENGTH, 0);
	
	// Change to our own context
    CGLSetCurrentContext( p_data->scaled );
	
	// Enable textures
    glEnable( GL_TEXTURE_2D );
    glBindTexture( GL_TEXTURE_2D, p_data->texture );
	
	// Change the texture with the screen capture made before
    glTexImage2D( GL_TEXTURE_2D, 0,
				 GL_RGBA, p_data->offscreen_width, p_data->offscreen_height, 0,
				 GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, p_data->texture_image );
    
	// Clear our context
    glClearColor( 0.0f, 0.0f, 0.0f, 1.0f );
    glClear( GL_COLOR_BUFFER_BIT );
	
	float max_width = p_data->screen_width/(double)p_data->offscreen_width;
	float max_height = p_data->screen_height/(double)p_data->offscreen_height;
	
	// Draw the screen capture (and we also flip it horizontally)
    glColor3f( 1.0f, 1.0f, 1.0f );
    glEnable( GL_TEXTURE_2D );
    glBindTexture( GL_TEXTURE_2D, p_data->texture );
    glBegin( GL_POLYGON );
    glTexCoord2f( 0.0      , max_height ); glVertex2f( -1.0, -1.0 );
    glTexCoord2f( max_width, max_height ); glVertex2f( 1.0, -1.0 );
    glTexCoord2f( max_width, 0.0        ); glVertex2f( 1.0, 1.0 );
    glTexCoord2f( 0.0      , 0.0        ); glVertex2f( -1.0, 1.0 );
    glEnd();
    glDisable( GL_TEXTURE_2D );
	
	// Get the pixels after transformation
    glReadPixels( 0, 0, 
				 p_data->screen_width,
				 p_data->screen_height,
				 GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV,
				 p_data->data_return );
	
    return (char *) p_data->data_return;
}

void screen_CloseCapture(screen_data_t * p_data)
{	
	// Free data
	if (p_data != NULL) {
		CGLSetCurrentContext( NULL );
		CGLClearDrawable( p_data->screen );
		CGLDestroyContext( p_data->screen );

		if (p_data->data_return != NULL) {
			free(p_data->data_return);
		}
		free(p_data);
	}
}


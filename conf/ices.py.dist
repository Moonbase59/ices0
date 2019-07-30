from string import *
import sys

# This is just a skeleton, something for you to start with.

songnumber = -1

# Function called to initialize your python environment.
# Should return 1 if ok, and 0 if something went wrong.
def ices_init ():
	print 'Executing initialize() function..'
	return 1

# Function called to shutdown your python enviroment.
# Return 1 if ok, 0 if something went wrong.
def ices_shutdown ():
	print 'Executing shutdown() function...'
	return 1

# Function called to get the next filename to stream. 
# Should return a string.
def ices_get_next ():
	print 'Executing get_next() function...'
	return 'Very nice song.mp3'

# This function, if defined, returns the string you'd like used
# as metadata (ie for title streaming) for the current song. You may
# return null to indicate that the file comment should be used.
def ices_get_metadata ():
        return 'Artist - Title (Label, Year)'

# Function used to put the current line number of
# the playlist in the cue file. If you don't care about this number
# don't use it.
def ices_get_lineno ():
	global songnumber
	print 'Executing get_lineno() function...'
	songnumber = songnumber + 1
	return songnumber

#------------------------------------------------------------------------------#
# This makefile was generated by 'cbp2make' tool rev.147                       #
#------------------------------------------------------------------------------#


WRKDIR = `pwd`
MAKE = make

all: media_media import_import mixerengine_mixerengine ffmpeg_ffmpeg mixertest_mixertest importtest_importtest

media_media: 
	$(MAKE) -C media $(ver) -f media.cbp.mak

import_import: 
	$(MAKE) -C import $(ver) -f import.cbp.mak

mixerengine_mixerengine: media_media
	$(MAKE) -C mixerEngine $(ver) -f mixerEngine.cbp.mak

ffmpeg_ffmpeg: media_media
	$(MAKE) -C ffmpeg $(ver) -f ffmpeg.cbp.mak

mixertest_mixertest: mixerengine_mixerengine
	$(MAKE) -C mixerTest $(ver) -f mixerTest.cbp.mak

importtest_importtest: import_import
	$(MAKE) -C importTest $(ver) -f importTest.cbp.mak

clean: clean_media_media clean_import_import clean_mixerengine_mixerengine clean_ffmpeg_ffmpeg clean_mixertest_mixertest clean_importtest_importtest

clean_media_media: 
	$(MAKE) -C media clean -f media.cbp.mak

clean_import_import: 
	$(MAKE) -C import clean -f import.cbp.mak

clean_mixerengine_mixerengine: 
	$(MAKE) -C mixerEngine clean -f mixerEngine.cbp.mak

clean_ffmpeg_ffmpeg: 
	$(MAKE) -C ffmpeg clean -f ffmpeg.cbp.mak

clean_mixertest_mixertest: 
	$(MAKE) -C mixerTest clean -f mixerTest.cbp.mak

clean_importtest_importtest: 
	$(MAKE) -C importTest clean -f importTest.cbp.mak

.PHONY: clean_media_media clean_import_import clean_mixerengine_mixerengine clean_ffmpeg_ffmpeg clean_mixertest_mixertest clean_importtest_importtest


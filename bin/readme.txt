Updated 30 June 2017 DGC

During install or update, some binaries are downloaded from github, and some
are compiled from source.  Details:

avc2ts is compiled from avc2ts.cpp

rpidatv is compiled from rpidatv.c

rpidatvgui is compiled from rpidatvtouch.c

leandvb is compiled

hello_video.bin is compiled

ffmpeg.old is downloaded from github and is the original version that Evariste
compiled in late 2016

ffmpeg.new is the bleeding edge version compiled by davecrump and then 
downloaded from github, with alsa and omx encoder support

ffmpeg is copied from either ffmpeg.old or ffmpeg.new depending on the setting
of the beta switch.  The ffmpeg.old is used as default after install.

testrig is a special compiled version of rpidatvgui used for testing 
filter-modulator baords and is downloaded compiled.

adf4351 is compiled from adf4351.c

tcanim is compiled

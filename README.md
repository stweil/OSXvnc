[![Build Status](https://travis-ci.org/stweil/OSXvnc.svg?branch=master)](https://travis-ci.org/stweil/OSXvnc)

Vine VNC Server (OSXvnc)
========================

The code in this Git repository is based on the CVS repository at
http://osxvnc.cvs.sourceforge.net/viewvc/osxvnc/.
It was created using this command:

    git cvsimport -d :pserver:anonymous@osxvnc.cvs.sourceforge.net:/cvsroot/osxvnc OSXvnc

Vine was developed at Redstone Software which was acquired by TestPlant.

This is a modified private copy of the original software.

News
----

2016-11-20

Builds now require https://sourceforge.net/projects/libjpeg-turbo/
or a compatible libjpeg.

2015-10-10

Vine VNC server now also works with high resolution (retina) displays
(thanks to Tom Sealy who wrote the new code).

Building
--------

Vine VNC server requires Xcode to build a distributable packet from sources:
- install a local copy of libjpeg/libjpeg-turbo
- open OSXvnc.xcodeproj in Xcode and build "Vine Server Package"

You will find the distributable at /tmp/VineServer.dmg

Links
-----

CVS repository at Sourceforge:
* http://osxvnc.cvs.sourceforge.net/viewvc/osxvnc/

Other Git clones of the CVS code:
* https://github.com/aaronbrethorst/OSXVnc
* https://github.com/browserstack/OSXVNC
* https://github.com/eventials/OSXVnc
* https://github.com/wingify/vnc

Documentation at TestPlant.com:
* http://docs.testplant.com/?q=content/installing-vnc-server

RFB Standard:
* https://tools.ietf.org/html/rfc6143

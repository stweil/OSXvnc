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

2017-12-15

libjpeg-turbo now is only needed for building.
It is no longer required for running the server.

The software license was updated from GPL 2 to GPL 3.

2016-11-20

Builds now require https://sourceforge.net/projects/libjpeg-turbo/
or a compatible libjpeg.

2015-10-10

Vine VNC server now also works with high resolution (retina) displays
(thanks to Tom Sealy who wrote the new code).

Building
--------

Vine VNC server requires Xcode to build a distributable packet from sources.

In addition, it expects the JPEG library [libjpeg-turbo](https://libjpeg-turbo.org/)
installed in directory `/opt/libjpeg-turbo`.
A suitable binary package for macOS is available at
[SourceForge](https://sourceforge.net/projects/libjpeg-turbo/files/).

### Building from Xcode

Open `OSXvnc.xcodeproj` in Xcode and build the "Vine Server Package".

You will find the distributable at /tmp/VineServer.dmg

### Building from the command line

Run this command from the command line:

    xcodebuild -configuration Deployment

Again you will find the distributable at /tmp/VineServer.dmg

License
-------

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>
or the file LICENSE in the distribution.

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

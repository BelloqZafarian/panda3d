// Filename: bioStreamPtr.cxx
// Created by:  drose (15Oct02)
//
////////////////////////////////////////////////////////////////////
//
// PANDA 3D SOFTWARE
// Copyright (c) 2001, Disney Enterprises, Inc.  All rights reserved
//
// All use of this software is subject to the terms of the Panda 3d
// Software license.  You should have received a copy of this license
// along with this source code; you will also find a current copy of
// the license at http://www.panda3d.org/license.txt .
//
// To contact the maintainers of this program write to
// panda3d@yahoogroups.com .
//
////////////////////////////////////////////////////////////////////

#include "bioPtr.h"

#ifdef HAVE_SSL

////////////////////////////////////////////////////////////////////
//     Function: BioStreamPtr::Destructor
//       Access: Public
//  Description:
////////////////////////////////////////////////////////////////////
BioStreamPtr::
~BioStreamPtr() {
  if (_stream != (IBioStream *)NULL) {
    delete _stream;
    _stream = (IBioStream *)NULL;
  }
}

#endif  // HAVE_SSL

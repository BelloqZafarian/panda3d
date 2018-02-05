/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file opusAudioCursor.h
 * @author rdb
 * @date 2017-05-24
 */

#ifndef OPUSAUDIOCURSOR_H
#define OPUSAUDIOCURSOR_H

#include "pandabase.h"
#include "movieAudioCursor.h"

#ifdef HAVE_OPUS

#include <ogg/ogg.h>

typedef struct OggOpusFile OggOpusFile;

class OpusAudio;

/**
 * Interfaces with the libopusfile library to implement decoding of Opus
 * audio files.
 */
class EXPCL_PANDA_MOVIES OpusAudioCursor : public MovieAudioCursor {
PUBLISHED:
  explicit OpusAudioCursor(OpusAudio *src, istream *stream);
  virtual ~OpusAudioCursor();
  virtual void seek(double offset);

public:
  virtual void read_samples(int n, int16_t *data);

  bool _is_valid;

protected:
  OggOpusFile *_op;

  int _link;
  double _byte_rate;
  int _block_align;
  int _bytes_per_sample;
  bool _is_float;

  streampos _data_start;
  streampos _data_pos;
  size_t _data_size;

public:
  static TypeHandle get_class_type() {
    return _type_handle;
  }
  static void init_type() {
    MovieAudioCursor::init_type();
    register_type(_type_handle, "OpusAudioCursor",
                  MovieAudioCursor::get_class_type());
  }
  virtual TypeHandle get_type() const {
    return get_class_type();
  }
  virtual TypeHandle force_init_type() {init_type(); return get_class_type();}

private:
  static TypeHandle _type_handle;
};

#include "opusAudioCursor.I"

#endif // HAVE_OPUS

#endif // OPUSAUDIOCURSOR_H

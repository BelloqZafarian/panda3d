// Filename: lodNode.cxx
// Created by:  drose (06Mar02)
//
////////////////////////////////////////////////////////////////////
//
// PANDA 3D SOFTWARE
// Copyright (c) 2001 - 2004, Disney Enterprises, Inc.  All rights reserved
//
// All use of this software is subject to the terms of the Panda 3d
// Software license.  You should have received a copy of this license
// along with this source code; you will also find a current copy of
// the license at http://etc.cmu.edu/panda3d/docs/license/ .
//
// To contact the maintainers of this program write to
// panda3d-general@lists.sourceforge.net .
//
////////////////////////////////////////////////////////////////////

#include "lodNode.h"
#include "cullTraverserData.h"
#include "cullTraverser.h"
#include "config_pgraph.h"
#include "geomVertexData.h"
#include "geomVertexWriter.h"
#include "geomVertexFormat.h"
#include "geomTristrips.h"
#include "mathNumbers.h"
#include "geom.h"
#include "geomNode.h"
#include "transformState.h"
#include "material.h"
#include "materialAttrib.h"
#include "materialPool.h"
#include "renderState.h"
#include "cullFaceAttrib.h"
#include "textureAttrib.h"
#include "boundingSphere.h"
#include "look_at.h"

TypeHandle LODNode::_type_handle;

////////////////////////////////////////////////////////////////////
//     Function: LODNode::make_copy
//       Access: Public, Virtual
//  Description: Returns a newly-allocated Node that is a shallow copy
//               of this one.  It will be a different Node pointer,
//               but its internal data may or may not be shared with
//               that of the original Node.
////////////////////////////////////////////////////////////////////
PandaNode *LODNode::
make_copy() const {
  return new LODNode(*this);
}

////////////////////////////////////////////////////////////////////
//     Function: LODNode::safe_to_combine
//       Access: Public, Virtual
//  Description: Returns true if it is generally safe to combine this
//               particular kind of PandaNode with other kinds of
//               PandaNodes, adding children or whatever.  For
//               instance, an LODNode should not be combined with any
//               other PandaNode, because its set of children is
//               meaningful.
////////////////////////////////////////////////////////////////////
bool LODNode::
safe_to_combine() const {
  return false;
}

////////////////////////////////////////////////////////////////////
//     Function: LODNode::xform
//       Access: Public, Virtual
//  Description: Transforms the contents of this PandaNode by the
//               indicated matrix, if it means anything to do so.  For
//               most kinds of PandaNodes, this does nothing.
////////////////////////////////////////////////////////////////////
void LODNode::
xform(const LMatrix4f &mat) {
  CDWriter cdata(_cycler);

  cdata->_center = cdata->_center * mat;

  // We'll take just the length of the y axis as the matrix's scale.
  LVector3f y;
  mat.get_row3(y, 1);
  float factor = y.length();

  SwitchVector::iterator si;
  for (si = cdata->_switch_vector.begin(); 
       si != cdata->_switch_vector.end(); 
       ++si) {
    (*si).rescale(factor);
  }
}

////////////////////////////////////////////////////////////////////
//     Function: LODNode::has_cull_callback
//       Access: Public, Virtual
//  Description: Should be overridden by derived classes to return
//               true if cull_callback() has been defined.  Otherwise,
//               returns false to indicate cull_callback() does not
//               need to be called for this node during the cull
//               traversal.
////////////////////////////////////////////////////////////////////
bool LODNode::
has_cull_callback() const {
  return true;
}

////////////////////////////////////////////////////////////////////
//     Function: LODNode::cull_callback
//       Access: Public, Virtual
//  Description: If has_cull_callback() returns true, this function
//               will be called during the cull traversal to perform
//               any additional operations that should be performed at
//               cull time.  This may include additional manipulation
//               of render state or additional visible/invisible
//               decisions, or any other arbitrary operation.
//
//               By the time this function is called, the node has
//               already passed the bounding-volume test for the
//               viewing frustum, and the node's transform and state
//               have already been applied to the indicated
//               CullTraverserData object.
//
//               The return value is true if this node should be
//               visible, or false if it should be culled.
////////////////////////////////////////////////////////////////////
bool LODNode::
cull_callback(CullTraverser *trav, CullTraverserData &data) {
  if (is_any_shown()) {
    return show_switches_cull_callback(trav, data);
  }

  int index = compute_child(trav, data);
  if (index >= 0 && index < get_num_children()) {
    CullTraverserData next_data(data, get_child(index));
    trav->traverse(next_data);
  }

  // Now return false indicating that we have already taken care of
  // the traversal from here.
  return false;
}

////////////////////////////////////////////////////////////////////
//     Function: LODNode::output
//       Access: Public, Virtual
//  Description:
////////////////////////////////////////////////////////////////////
void LODNode::
output(ostream &out) const {
  PandaNode::output(out);
  CDReader cdata(_cycler);
  out << " ";
  if (cdata->_switch_vector.empty()) {
    out << "no switches.";
  } else {
    SwitchVector::const_iterator si;
    si = cdata->_switch_vector.begin();
    out << "(" << (*si).get_in() << "/" << (*si).get_out() << ")";
    ++si;
    while (si != cdata->_switch_vector.end()) {
      out << " (" << (*si).get_in() << "/" << (*si).get_out() << ")";
      ++si;
    }
  }
}

////////////////////////////////////////////////////////////////////
//     Function: LODNode::is_lod_node
//       Access: Published, Virtual
//  Description: A simple downcast check.  Returns true if this kind
//               of node happens to inherit from LODNode, false
//               otherwise.
//
//               This is provided as a a faster alternative to calling
//               is_of_type(LODNode::get_class_type()).
////////////////////////////////////////////////////////////////////
bool LODNode::
is_lod_node() const {
  return true;
}

////////////////////////////////////////////////////////////////////
//     Function: LODNode::show_switch
//       Access: Published
//  Description: This is provided as a debugging aid.  show_switch()
//               will put the LODNode into a special mode where rather
//               than computing and drawing the appropriate level of
//               the LOD, a ring is drawn around the LODNode center
//               indicating the switch distances from the camera for
//               the indicated level, and the geometry of the
//               indicated level is drawn in wireframe.
//
//               Multiple different levels can be visualized this way
//               at once.  Call hide_switch() or hide_all_switches() to
//               undo this mode and restore the LODNode to its normal
//               behavior.
////////////////////////////////////////////////////////////////////
void LODNode::
show_switch(int index) {
  CDWriter cdata(_cycler);
  do_show_switch(cdata, index, get_default_show_color(index));
  mark_internal_bounds_stale();
}

////////////////////////////////////////////////////////////////////
//     Function: LODNode::show_switch
//       Access: Published
//  Description: This is provided as a debugging aid.  show_switch()
//               will put the LODNode into a special mode where rather
//               than computing and drawing the appropriate level of
//               the LOD, a ring is drawn around the LODNode center
//               indicating the switch distances from the camera for
//               the indicated level, and the geometry of the
//               indicated level is drawn in wireframe.
//
//               Multiple different levels can be visualized this way
//               at once.  Call hide_switch() or hide_all_switches() to
//               undo this mode and restore the LODNode to its normal
//               behavior.
////////////////////////////////////////////////////////////////////
void LODNode::
show_switch(int index, const Colorf &color) {
  CDWriter cdata(_cycler);
  do_show_switch(cdata, index, color);
  mark_internal_bounds_stale();
}

////////////////////////////////////////////////////////////////////
//     Function: LODNode::hide_switch
//       Access: Published
//  Description: Disables a previous call to show_switch().
////////////////////////////////////////////////////////////////////
void LODNode::
hide_switch(int index) {
  CDWriter cdata(_cycler);
  do_hide_switch(cdata, index);
  mark_internal_bounds_stale();
}

////////////////////////////////////////////////////////////////////
//     Function: LODNode::show_all_switches
//       Access: Published
//  Description: Shows all levels in their default colors.
////////////////////////////////////////////////////////////////////
void LODNode::
show_all_switches() {
  CDWriter cdata(_cycler);
  for (int i = 0; i < (int)cdata->_switch_vector.size(); ++i) {
    do_show_switch(cdata, i, get_default_show_color(i));
  }
  mark_internal_bounds_stale();
}

////////////////////////////////////////////////////////////////////
//     Function: LODNode::hide_all_switches
//       Access: Published
//  Description: Hides all levels, restoring the LODNode to normal
//               operation.
////////////////////////////////////////////////////////////////////
void LODNode::
hide_all_switches() {
  CDWriter cdata(_cycler);
  for (int i = 0; i < (int)cdata->_switch_vector.size(); ++i) {
    do_hide_switch(cdata, i);
  }
  mark_internal_bounds_stale();
}

////////////////////////////////////////////////////////////////////
//     Function: LODNode::compute_child
//       Access: Protected
//  Description: Determines which child should be visible according to
//               the current camera position.  If a child is visible,
//               returns its index number; otherwise, returns -1.
////////////////////////////////////////////////////////////////////
int LODNode::
compute_child(CullTraverser *trav, CullTraverserData &data) {
  if (data.get_net_transform(trav)->is_singular()) {
    // If we're under a singular transform, we can't compute the LOD;
    // select none of them instead.
    return -1;
  }
   
  CDReader cdata(_cycler);

  if (cdata->_got_force_switch) {
    return cdata->_force_switch;
  }

  // Get a pointer to the camera node.
  Camera *camera = trav->get_scene()->get_camera_node();
  
  // Get the LOD center in camera space.  If the camera has a special
  // LOD center defined, use that; otherwise, if it has a cull center,
  // use that; otherwise, use the modelview transform (which is camera
  // space).
  CPT(TransformState) rel_transform;

  NodePath lod_center = camera->get_lod_center();
  if (!lod_center.is_empty()) {
    rel_transform = 
      lod_center.get_net_transform()->invert_compose(data.get_net_transform(trav));
  } else {
    NodePath cull_center = camera->get_cull_center();
    if (!cull_center.is_empty()) {
      rel_transform = 
        cull_center.get_net_transform()->invert_compose(data.get_net_transform(trav));
    } else {
      rel_transform = data.get_modelview_transform(trav);
    }
  }

  LPoint3f center = cdata->_center * rel_transform->get_mat();

  // Now measure the distance to the LOD center, and use that to
  // determine which child to display.
  float dist2 = center.dot(center);

  for (int index = 0; index < (int)cdata->_switch_vector.size(); index++) {
    if (cdata->_switch_vector[index].in_range_2(dist2)) { 
      if (pgraph_cat.is_debug()) {
        pgraph_cat.debug()
          << data._node_path << " at distance " << sqrt(dist2)
          << ", selected child " << index << "\n";
      }

      return index;
    }
  }

  if (pgraph_cat.is_debug()) {
    pgraph_cat.debug()
      << data._node_path << " at distance " << sqrt(dist2)
      << ", no children in range.\n";
  }

  return -1;
}


////////////////////////////////////////////////////////////////////
//     Function: LODNode::show_switches_cull_callback
//       Access: Protected
//  Description: A special version of cull_callback() that is to be
//               invoked when the LODNode is in show_switch() mode.
//               This just draws the rings and the wireframe geometry
//               for the selected switches.
////////////////////////////////////////////////////////////////////
bool LODNode::
show_switches_cull_callback(CullTraverser *trav, CullTraverserData &data) {
  CDReader cdata(_cycler);

  // Get a pointer to the camera node.
  Camera *camera = trav->get_scene()->get_camera_node();
  
  // Get the camera space transform.  This bit is the same as the code
  // in compute_child(), above.
  CPT(TransformState) rel_transform;

  NodePath lod_center = camera->get_lod_center();
  if (!lod_center.is_empty()) {
    rel_transform = 
      lod_center.get_net_transform()->invert_compose(data.get_net_transform(trav));
  } else {
    NodePath cull_center = camera->get_cull_center();
    if (!cull_center.is_empty()) {
      rel_transform = 
        cull_center.get_net_transform()->invert_compose(data.get_net_transform(trav));
    } else {
      rel_transform = data.get_modelview_transform(trav);
    }
  }

  LPoint3f center = cdata->_center * rel_transform->get_mat();
  float dist2 = center.dot(center);

  // Now orient the disk(s) in camera space such that their origin is
  // at center, and the (0, 0, 0) point in camera space is on the disk.
  LMatrix4f mat;
  look_at(mat, -center, LVector3f(0.0f, 0.0f, 1.0f));
  mat.set_row(3, center);
  CPT(TransformState) viz_transform = TransformState::make_mat(mat);

  viz_transform = rel_transform->invert_compose(viz_transform);
  viz_transform = data.get_net_transform(trav)->compose(viz_transform);

  SwitchVector::const_iterator si;
  for (si = cdata->_switch_vector.begin(); 
       si != cdata->_switch_vector.end(); 
       ++si) {
    const Switch &sw = (*si);
    if (sw.is_shown()) {
      CullTraverserData next_data(data, sw.get_ring_viz());
      next_data._net_transform = viz_transform;
      trav->traverse(next_data);

      if (sw.in_range_2(dist2)) {
        // This switch level is in range.  Draw the spindle in this
        // color.
        CullTraverserData next_data2(data, sw.get_spindle_viz());
        next_data2._net_transform = viz_transform;
        trav->traverse(next_data2);

        // And draw its children in the funny wireframe mode.
        int index = (si - cdata->_switch_vector.begin());
        if (index < get_num_children()) {
          CullTraverserData next_data3(data, get_child(index));
          next_data3._state = next_data3._state->compose(sw.get_viz_model_state());
          trav->traverse(next_data3);
        }
      }
    }
  }

  // Now return false indicating that we have already taken care of
  // the traversal from here.
  return false;
}

////////////////////////////////////////////////////////////////////
//     Function: LODNode::compute_internal_bounds
//       Access: Protected, Virtual
//  Description: Returns a newly-allocated BoundingVolume that
//               represents the internal contents of the node.  Should
//               be overridden by PandaNode classes that contain
//               something internally.
////////////////////////////////////////////////////////////////////
PT(BoundingVolume) LODNode::
compute_internal_bounds(int pipeline_stage, Thread *current_thread) const {
  // First, get ourselves a fresh, empty bounding volume.
  PT(BoundingVolume) bound = PandaNode::compute_internal_bounds(pipeline_stage, current_thread);
  nassertr(bound != (BoundingVolume *)NULL, bound);

  // If we have any visible rings, those count in the bounding volume.
  if (is_any_shown()) {
    // Now actually compute the bounding volume by putting it around all
    // of our geoms' bounding volumes.
    pvector<const BoundingVolume *> child_volumes;
    pvector<PT(BoundingVolume) > pt_volumes;

    CDStageReader cdata(_cycler, pipeline_stage, current_thread);

    SwitchVector::const_iterator si;
    for (si = cdata->_switch_vector.begin();
         si != cdata->_switch_vector.end();
         ++si) {
      const Switch &sw = (*si);
      if (sw.is_shown()) {
        PT(BoundingVolume) sphere = new BoundingSphere(cdata->_center, sw.get_in());
        child_volumes.push_back(sphere);
        pt_volumes.push_back(sphere);
      }
    }
    
    const BoundingVolume **child_begin = &child_volumes[0];
    const BoundingVolume **child_end = child_begin + child_volumes.size();
    
    bound->around(child_begin, child_end);
  }

  return bound;
}

////////////////////////////////////////////////////////////////////
//     Function: LODNode::do_show_switch
//       Access: Private
//  Description: The private implementation of show_switch().
////////////////////////////////////////////////////////////////////
void LODNode::
do_show_switch(LODNode::CData *cdata, int index, const Colorf &color) {
  nassertv(index >= 0 && index < (int)cdata->_switch_vector.size());

  if (!cdata->_switch_vector[index].is_shown()) {
    ++cdata->_num_shown;
  }
  cdata->_switch_vector[index].show(color);
}

////////////////////////////////////////////////////////////////////
//     Function: LODNode::do_hide_switch
//       Access: Private
//  Description: The private implementation of hide_switch().
////////////////////////////////////////////////////////////////////
void LODNode::
do_hide_switch(LODNode::CData *cdata, int index) {
  nassertv(index >= 0 && index < (int)cdata->_switch_vector.size());

  if (cdata->_switch_vector[index].is_shown()) {
    --cdata->_num_shown;
  }
  cdata->_switch_vector[index].hide();
}

////////////////////////////////////////////////////////////////////
//     Function: LODNode::get_default_show_color
//       Access: Private, Static
//  Description: Returns a default color appropriate for showing the
//               indicated level.
////////////////////////////////////////////////////////////////////
const Colorf &LODNode::
get_default_show_color(int index) {
  static Colorf default_colors[] = {
    Colorf(1.0f, 0.0f, 0.0f, 0.7f),
    Colorf(0.0f, 1.0f, 0.0f, 0.7f),
    Colorf(0.0f, 0.0f, 1.0f, 0.7f),
    Colorf(0.0f, 1.0f, 1.0f, 0.7f),
    Colorf(1.0f, 0.0f, 1.0f, 0.7f),
    Colorf(1.0f, 1.0f, 0.0f, 0.7f),
  };
  static const int num_default_colors = sizeof(default_colors) / sizeof(Colorf);

  return default_colors[index % num_default_colors];
}


////////////////////////////////////////////////////////////////////
//     Function: LODNode::register_with_read_factory
//       Access: Public, Static
//  Description: Tells the BamReader how to create objects of type
//               LODNode.
////////////////////////////////////////////////////////////////////
void LODNode::
register_with_read_factory() {
  BamReader::get_factory()->register_factory(get_class_type(), make_from_bam);
}

////////////////////////////////////////////////////////////////////
//     Function: LODNode::write_datagram
//       Access: Public, Virtual
//  Description: Writes the contents of this object to the datagram
//               for shipping out to a Bam file.
////////////////////////////////////////////////////////////////////
void LODNode::
write_datagram(BamWriter *manager, Datagram &dg) {
  PandaNode::write_datagram(manager, dg);
  manager->write_cdata(dg, _cycler);
}

////////////////////////////////////////////////////////////////////
//     Function: LODNode::make_from_bam
//       Access: Protected, Static
//  Description: This function is called by the BamReader's factory
//               when a new object of type LODNode is encountered
//               in the Bam file.  It should create the LODNode
//               and extract its information from the file.
////////////////////////////////////////////////////////////////////
TypedWritable *LODNode::
make_from_bam(const FactoryParams &params) {
  LODNode *node = new LODNode("");
  DatagramIterator scan;
  BamReader *manager;

  parse_params(params, scan, manager);
  node->fillin(scan, manager);

  return node;
}

////////////////////////////////////////////////////////////////////
//     Function: LODNode::fillin
//       Access: Protected
//  Description: This internal function is called by make_from_bam to
//               read in all of the relevant data from the BamFile for
//               the new LODNode.
////////////////////////////////////////////////////////////////////
void LODNode::
fillin(DatagramIterator &scan, BamReader *manager) {
  PandaNode::fillin(scan, manager);
  manager->read_cdata(scan, _cycler);
}

////////////////////////////////////////////////////////////////////
//     Function: LODNode::CData::make_copy
//       Access: Public, Virtual
//  Description:
////////////////////////////////////////////////////////////////////
CycleData *LODNode::CData::
make_copy() const {
  return new CData(*this);
}

////////////////////////////////////////////////////////////////////
//     Function: LODNode::CData::check_limits
//       Access: Public
//  Description: Ensures that the _lowest and _highest members are set
//               appropriately after a change to the set of switches.
////////////////////////////////////////////////////////////////////
void LODNode::CData::
check_limits() {
  _lowest = 0;
  _highest = 0;
  for (size_t i = 1; i < _switch_vector.size(); ++i) {
    if (_switch_vector[i].get_out() > _switch_vector[_lowest].get_out()) {
      _lowest = i;
    }
    if (_switch_vector[i].get_in() < _switch_vector[_highest].get_in()) {
      _highest = i;
    }
  }
}

////////////////////////////////////////////////////////////////////
//     Function: LODNode::CData::write_datagram
//       Access: Public, Virtual
//  Description: Writes the contents of this object to the datagram
//               for shipping out to a Bam file.
////////////////////////////////////////////////////////////////////
void LODNode::CData::
write_datagram(BamWriter *manager, Datagram &dg) const {
  _center.write_datagram(dg);

  dg.add_uint16(_switch_vector.size());

  SwitchVector::const_iterator si;
  for (si = _switch_vector.begin();
       si != _switch_vector.end();
       ++si) {
    (*si).write_datagram(dg);
  }
}

////////////////////////////////////////////////////////////////////
//     Function: LODNode::CData::fillin
//       Access: Public, Virtual
//  Description: This internal function is called by make_from_bam to
//               read in all of the relevant data from the BamFile for
//               the new LODNode.
////////////////////////////////////////////////////////////////////
void LODNode::CData::
fillin(DatagramIterator &scan, BamReader *manager) {
  _center.read_datagram(scan);

  _switch_vector.clear();

  int num_switches = scan.get_uint16();
  _switch_vector.reserve(num_switches);
  for (int i = 0; i < num_switches; i++) {
    Switch sw(0, 0);
    sw.read_datagram(scan);

    _switch_vector.push_back(sw);
  }
}

////////////////////////////////////////////////////////////////////
//     Function: LODNode::Switch::compute_ring_viz
//       Access: Private
//  Description: Computes a Geom suitable for rendering the ring
//               associated with this switch.
////////////////////////////////////////////////////////////////////
void LODNode::Switch::
compute_ring_viz() {
  // We render the ring as a series of concentric ring-shaped triangle
  // strips, each of which has num_slices quads.
  static const int num_slices = 50;
  static const int num_rings = 1;

  // There are also two more triangle strips, one for the outer edge,
  // and one for the inner edge.
  static const float edge_ratio = 0.1;  // ratio of edge height to diameter.

  const GeomVertexFormat *format = GeomVertexFormat::get_v3n3cp();
  PT(GeomVertexData) vdata = new GeomVertexData("LOD_ring", format, Geom::UH_static);

  // Fill up the vertex table with all of the vertices.
  GeomVertexWriter vertex(vdata, InternalName::get_vertex());
  GeomVertexWriter normal(vdata, InternalName::get_normal());
  GeomVertexWriter color(vdata, InternalName::get_color());

  // First, the vertices for the flat ring.
  int ri, si;
  for (ri = 0; ri <= num_rings; ++ri) {
    // r is in the range [0.0, 1.0].
    float r = (float)ri / (float)num_rings;

    // d is in the range [_out, _in].
    float d = r * (_in - _out) + _out;

    for (si = 0; si < num_slices; ++si) {
      // s is in the range [0.0, 1.0).
      float s = (float)si / (float)num_slices;

      // t is in the range [0.0, 2pi).
      float t = MathNumbers::pi_f * 2.0f * s;

      float x = cosf(t);
      float y = sinf(t);
      vertex.add_data3f(x * d, y * d, 0.0f);
      normal.add_data3f(0.0f, 0.0f, 1.0f);
      color.add_data4f(_show_color);
    }
  }

  // Next, the vertices for the inner and outer edges.
  for (ri = 0; ri <= 1; ++ri) {
    float r = (float)ri;
    float d = r * (_in - _out) + _out;

    for (si = 0; si < num_slices; ++si) {
      float s = (float)si / (float)num_slices;
      float t = MathNumbers::pi_f * 2.0f * s;
      
      float x = cosf(t);
      float y = sinf(t);

      vertex.add_data3f(x * d, y * d, 0.5f * edge_ratio * d);
      normal.add_data3f(x, y, 0.0f);
      color.add_data4f(_show_color);
    }

    for (si = 0; si < num_slices; ++si) {
      float s = (float)si / (float)num_slices;
      float t = MathNumbers::pi_f * 2.0f * s;
      
      float x = cosf(t);
      float y = sinf(t);

      vertex.add_data3f(x * d, y * d, -0.5f * edge_ratio * d);
      normal.add_data3f(x, y, 0.0f);
      color.add_data4f(_show_color);
    }
  }

  // Now create the triangle strips.  One tristrip for each ring.
  PT(GeomTristrips) strips = new GeomTristrips(Geom::UH_static);
  for (ri = 0; ri < num_rings; ++ri) {
    for (si = 0; si < num_slices; ++si) {
      strips->add_vertex(ri * num_slices + si);
      strips->add_vertex((ri + 1) * num_slices + si);
    }
    strips->add_vertex(ri * num_slices);
    strips->add_vertex((ri + 1) * num_slices);
    strips->close_primitive();
  }

  // And then one triangle strip for each of the inner and outer
  // edges.
  for (ri = 0; ri <= 1; ++ri) {
    for (si = 0; si < num_slices; ++si) {
      strips->add_vertex((num_rings + 1 + ri * 2) * num_slices + si);
      strips->add_vertex((num_rings + 1 + ri * 2 + 1) * num_slices + si);
    }
    strips->add_vertex((num_rings + 1 + ri * 2) * num_slices);
    strips->add_vertex((num_rings + 1 + ri * 2 + 1) * num_slices);
    strips->close_primitive();
  }

  PT(Geom) ring_geom = new Geom(vdata);
  ring_geom->add_primitive(strips);

  PT(GeomNode) geom_node = new GeomNode("ring");
  geom_node->add_geom(ring_geom);

  // Get a material for two-sided lighting.
  PT(Material) material = new Material();
  material->set_twoside(true);
  material = MaterialPool::get_material(material);

  CPT(RenderState) viz_state = 
    RenderState::make(CullFaceAttrib::make(CullFaceAttrib::M_cull_none),
                      TextureAttrib::make_off(),
                      ShaderAttrib::make_off(),
                      MaterialAttrib::make(material),
                      RenderState::get_max_priority());
  if (_show_color[3] != 1.0f) {
    viz_state = viz_state->add_attrib(TransparencyAttrib::make(TransparencyAttrib::M_alpha),
                                        RenderState::get_max_priority());
  }

  geom_node->set_state(viz_state);

  _ring_viz = geom_node.p();
}

////////////////////////////////////////////////////////////////////
//     Function: LODNode::Switch::compute_spindle_viz
//       Access: Private
//  Description: Computes a Geom suitable for rendering the LODNode
//               spindle in the color of this switch.
////////////////////////////////////////////////////////////////////
void LODNode::Switch::
compute_spindle_viz() {
  // We render the spindle as a cylinder, which consists of num_rings
  // rings stacked vertically, each of which is a triangle strip of
  // num_slices quads.  The scale is -10 .. 10 vertically, with a radius
  // of 1.0.
  static const int num_slices = 10;
  static const int num_rings = 10;

  const GeomVertexFormat *format = GeomVertexFormat::get_v3n3cp();
  PT(GeomVertexData) vdata = new GeomVertexData("LOD_spindle", format, Geom::UH_static);

  // Fill up the vertex table with all of the vertices.
  GeomVertexWriter vertex(vdata, InternalName::get_vertex());
  GeomVertexWriter normal(vdata, InternalName::get_normal());
  GeomVertexWriter color(vdata, InternalName::get_color());

  int ri, si;
  for (ri = 0; ri <= num_rings; ++ri) {
    // r is in the range [0.0, 1.0].
    float r = (float)ri / (float)num_rings;

    // z is in the range [100.0, -100.0]
    float z = 100.0f - r * 200.0f;

    for (si = 0; si < num_slices; ++si) {
      // s is in the range [0.0, 1.0).
      float s = (float)si / (float)num_slices;

      // t is in the range [0.0, 2pi).
      float t = MathNumbers::pi_f * 2.0f * s;

      float x = cosf(t);
      float y = sinf(t);
      vertex.add_data3f(x, y, z);
      normal.add_data3f(x, y, 0.0f);
      color.add_data4f(_show_color);
    }
  }

  // Now create the triangle strips.  One tristrip for each ring.
  PT(GeomTristrips) strips = new GeomTristrips(Geom::UH_static);
  for (ri = 0; ri < num_rings; ++ri) {
    for (si = 0; si < num_slices; ++si) {
      strips->add_vertex(ri * num_slices + si);
      strips->add_vertex((ri + 1) * num_slices + si);
    }
    strips->add_vertex(ri * num_slices);
    strips->add_vertex((ri + 1) * num_slices);
    strips->close_primitive();
  }

  PT(Geom) spindle_geom = new Geom(vdata);
  spindle_geom->add_primitive(strips);

  PT(GeomNode) geom_node = new GeomNode("spindle");
  geom_node->add_geom(spindle_geom);

  CPT(RenderState) viz_state = 
    RenderState::make(CullFaceAttrib::make(CullFaceAttrib::M_cull_clockwise),
                      TextureAttrib::make_off(),
                      ShaderAttrib::make_off(),
                      RenderState::get_max_priority());
  if (_show_color[3] != 1.0f) {
    viz_state = viz_state->add_attrib(TransparencyAttrib::make(TransparencyAttrib::M_alpha),
                                      RenderState::get_max_priority());
  }

  geom_node->set_state(viz_state);

  _spindle_viz = geom_node.p();
}

////////////////////////////////////////////////////////////////////
//     Function: LODNode::Switch::compute_viz_model_state
//       Access: Private
//  Description: Computes a RenderState for rendering the children of
//               this switch in colored wireframe mode.
////////////////////////////////////////////////////////////////////
void LODNode::Switch::
compute_viz_model_state() {
  // The RenderState::make() function only takes up to four attribs at
  // once.  Since we need more attribs than that, we have to make up
  // our state in two steps.
  _viz_model_state = RenderState::make(RenderModeAttrib::make(RenderModeAttrib::M_wireframe),
                                       TextureAttrib::make_off(),
                                       ShaderAttrib::make_off(),
                                       ColorAttrib::make_flat(_show_color),
                                       RenderState::get_max_priority());
  CPT(RenderState) st2 = RenderState::make(TransparencyAttrib::make(TransparencyAttrib::M_none),
                                           RenderState::get_max_priority());
  _viz_model_state = _viz_model_state->compose(st2);
}

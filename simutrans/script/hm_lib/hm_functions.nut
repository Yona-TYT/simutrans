
this.hm_all_waytypes <- [wt_road,wt_rail,wt_water,wt_monorail,wt_maglev,wt_tram,wt_narrowgauge,wt_air,wt_power]
this.hm_all_systemtypes <- [st_flat, st_elevated, st_tram, st_runway]

function hm_get_way_desc(desc_name, wt, st) {
  local obj = null

  //gui.add_message_at(player, "hm_get_way_desc " + desc_name + " wt " + wt + " st " + st, world.get_time())
  if ( wt == null || st == null ) {
    // not set waytype and/or systemtype
    // object not found, break script
    foreach(wt in hm_all_waytypes) {
      foreach (st in hm_all_systemtypes) {
        foreach (w in way_desc_x.get_available_ways(wt, st)) {
          if(w.get_name()==desc_name) {
            obj = w
            break
          }
        }
      }
    }
  } else {
    // set waytype and systemtype
    // object not available then replace
    local list = way_desc_x.get_available_ways(wt, st)
    foreach (w in list) {
      //gui.add_message_at(player, "w.get_name() " + w.get_name(), world.get_time())
      if(w.get_name()==desc_name) {
        obj = w
        //gui.add_message_at(player, "found desc_name " + obj.get_name(), world.get_time())
        break
      }
      if ( obj == null ) {
        obj = w
        //gui.add_message_at(player, "fallback " + obj.get_name(), world.get_time())
      }
    }

  }

  return obj
}

function hm_get_bridge_desc(desc_name, wt) {
  local obj = null

  if ( wt == null ) {
    // not set waytype
    // object not found, break script
    foreach(wt in hm_all_waytypes) {
      foreach (b in bridge_desc_x.get_available_bridges(wt)) {
        if(b.get_name()==desc_name) {
          obj = b
          break
        }
      }
    }
  } else {
    // set waytype
    // object not available then replace
    local list = bridge_desc_x.get_available_bridges(wt)
    foreach (b in list) {
      if(b.get_name()==desc_name) {
        obj = b
        break
      }
      if ( obj == null ) {
        obj = b
      }
    }

  }

  return obj
}

function hm_get_sign_desc(desc_name, wt) {
  local obj = null

  if ( wt == null ) {
    // not set waytype
    // object not found, break script
    foreach(wt in hm_all_waytypes) {
      foreach (s in sign_desc_x.get_available_signs(wt)) {
        if(s.get_name()==desc_name) {
          obj = s
          break
        }
      }
    }
  } else {
    // set waytype
    // object not available then replace
    local list = sign_desc_x.get_available_signs(wt)
    foreach (s in list) {
      if(s.get_name()==desc_name) {
        obj = s
        break
      }
      if ( obj == null ) {
        obj = s
      }
    }

  }

  return obj
}

function hm_get_wayobjs_desc(desc_name, wt, overhead) {
  local obj = null

  if ( wt == null ) {
    // not set waytype
    // object not found, break script
    foreach(wt in hm_all_waytypes) {
      foreach (s in wayobj_desc_x.get_available_wayobjs(wt)) {
        if(s.get_name()==desc_name) {
          obj = s
          break
        }
      }
    }
  } else {
    // set waytype
    // object not available then replace
    local list = wayobj_desc_x.get_available_wayobjs(wt)
    foreach (s in list) {
      if(s.get_name()==desc_name) {
        obj = s
        break
      }
      if ( obj == null ) {
        obj = s
      } else if ( obj.get_topspeed() < 90 && s.get_topspeed() > 90 ) {
        obj = s
      }
      if ( overhead == 1 && !obj.is_overhead_line() ) {
        obj = null
      }
    }
  }
  return obj
}

function hm_get_building_desc(desc_name, wt, building_type) {
  local obj   = null
  local goods = {}

  if ( wt != null ) {
    // not set waytype => 0 searches all ways
    foreach (b in building_desc_x.get_available_stations(building_type, wt, goods) ) {
      if(b.get_name()==desc_name) {
        obj = b
        break
      }
    }
  }
  if ( obj == null ) {
    // not set waytype => 0 searches all ways
    foreach (b in building_desc_x.get_building_list(building_desc_x.station)) {
      if(b.get_name()==desc_name) {
        obj = b
        break
      }
    }
  }
  return obj
}

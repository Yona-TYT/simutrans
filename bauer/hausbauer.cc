/*
 * Copyright (c) 1997 - 2002 Hansj�rg Malthaner
 *
 * This file is part of the Simutrans project under the artistic licence.
 * (see licence.txt)
 */

#include <string.h>

#include "../besch/haus_besch.h"
#include "../besch/skin_besch.h"
#include "../besch/spezial_obj_tpl.h"

#include "../boden/boden.h"
#include "../boden/wasser.h"
#include "../boden/fundament.h"

#include "../dataobj/scenario.h"
#include "../obj/leitung2.h"
#include "../obj/tunnel.h"
#include "../obj/zeiger.h"

#include "../gui/karte.h"
#include "../gui/tool_selector.h"

#include "../simcity.h"
#include "../simdebug.h"
#include "../simdepot.h"
#include "../simhalt.h"
#include "../utils/simrandom.h"
#include "../simtool.h"
#include "../simworld.h"
#include "../tpl/stringhashtable_tpl.h"
#include "../tpl/weighted_vector_tpl.h"
#include "../tpl/vector_tpl.h"
#include "hausbauer.h"

karte_ptr_t hausbauer_t::welt;

/*
 * The different building groups are sorted into separate lists
 */
static vector_tpl<const building_desc_t*> city_residential;        ///< residential buildings (res)
static vector_tpl<const building_desc_t*> city_commercial;     ///< commercial buildings  (com)
static vector_tpl<const building_desc_t*> city_industry;   ///< industrial buildings  (ind)

vector_tpl<const building_desc_t *> hausbauer_t::attractions_land;
vector_tpl<const building_desc_t *> hausbauer_t::attractions_city;
vector_tpl<const building_desc_t *> hausbauer_t::townhalls;
vector_tpl<const building_desc_t *> hausbauer_t::monuments;
vector_tpl<const building_desc_t *> hausbauer_t::unbuilt_monuments;

/**
 * List of all registered house descriptors.
 * Allows searching for a desc by its name
 */
static stringhashtable_tpl<const building_desc_t*> desc_table;

const building_desc_t *hausbauer_t::elevated_foundation_desc = NULL;
vector_tpl<const building_desc_t *> hausbauer_t::station_building;
vector_tpl<const building_desc_t *> hausbauer_t::headquarter;

/// special objects directly needed by the program
static spezial_obj_tpl<building_desc_t> const special_objects[] = {
	{ &hausbauer_t::elevated_foundation_desc,   "MonorailGround" },
	{ NULL, NULL }
};


/**
 * Compares house descriptors.
 * Order of comparison:
 *  -# by level
 *  -# by name
 * @return true if @p a < @p b
 */
static bool compare_building_desc(const building_desc_t* a, const building_desc_t* b)
{
	int diff = a->get_level() - b->get_level();
	if (diff == 0) {
		/* As a last resort, sort by name to avoid ambiguity */
		diff = strcmp(a->get_name(), b->get_name());
	}
	return diff < 0;
}


/**
 * Compares headquarters.
 * Order of comparison:
 *  -# by level
 *  -# by name
 * @return true if @p a < @p b
 */
static bool compare_hq_desc(const building_desc_t* a, const building_desc_t* b)
{
	// the headquarters level is in the extra-variable
	int diff = a->get_extra() - b->get_extra();
	if (diff == 0) {
		diff = a->get_level() - b->get_level();
	}
	if (diff == 0) {
		/* As a last resort, sort by name to avoid ambiguity */
		diff = strcmp(a->get_name(), b->get_name());
	}
	return diff < 0;
}


/**
 * Compares stations.
 * Order of comparison:
 *  -# by good category
 *  -# by capacity
 *  -# by level
 *  -# by name
 * @return true if @p a < @p b
 */
static bool compare_station_desc(const building_desc_t* a, const building_desc_t* b)
{
	int diff = a->get_enabled() - b->get_enabled();
	if(  diff == 0  ) {
		diff = a->get_capacity() - b->get_capacity();
	}
	if(  diff == 0  ) {
		diff = a->get_level() - b->get_level();
	}
	if(  diff == 0  ) {
		/* As a last resort, sort by name to avoid ambiguity */
		diff = strcmp(a->get_name(), b->get_name());
	}
	return diff < 0;
}


bool hausbauer_t::successfully_loaded()
{
	FOR(stringhashtable_tpl<building_desc_t const*>, const& i, desc_table) {
		building_desc_t const* const desc = i.value;

		// now insert the desc into the correct list.
		switch(desc->get_type()) {
			case building_desc_t::city_res:
				city_residential.insert_ordered(desc,compare_building_desc);
				break;
			case building_desc_t::city_ind:
				city_industry.insert_ordered(desc,compare_building_desc);
				break;
			case building_desc_t::city_com:
				city_commercial.insert_ordered(desc,compare_building_desc);
				break;

				case building_desc_t::monument:
					monuments.insert_ordered(desc,compare_building_desc);
					break;
				case building_desc_t::attraction_land:
					attractions_land.insert_ordered(desc,compare_building_desc);
					break;
				case building_desc_t::headquarter:
					headquarter.insert_ordered(desc,compare_hq_desc);
					break;
				case building_desc_t::townhall:
					townhalls.insert_ordered(desc,compare_building_desc);
					break;
				case building_desc_t::attraction_city:
					attractions_city.insert_ordered(desc,compare_building_desc);
					break;

				case building_desc_t::factory:
					break;

				case building_desc_t::dock:
				case building_desc_t::flat_dock:
				case building_desc_t::depot:
				case building_desc_t::generic_stop:
				case building_desc_t::generic_extension:
					station_building.insert_ordered(desc,compare_station_desc);
					break;

				case building_desc_t::others:
					if(strcmp(desc->get_name(),"MonorailGround")==0) {
						// foundation for elevated ways
						elevated_foundation_desc = desc;
						break;
					}
					/* no break */

				default:
					// obsolete object, usually such pak set will not load properly anyway (old objects should be caught before!)
					dbg->error("hausbauer_t::successfully_loaded()","unknown subtype %i of \"%s\" ignored",desc->get_type(), desc->get_name());
		}
	}

	// now sort them according level
	warn_missing_objects(special_objects);
	return true;
}


bool hausbauer_t::register_desc(building_desc_t *desc)
{
	::register_desc(special_objects, desc);

	// avoid duplicates with same name
	const building_desc_t *old_desc = desc_table.get(desc->get_name());
	if(old_desc) {
		// do not overlay existing factories if the new one is not a factory
		if (old_desc->is_factory()  &&  !desc->is_factory()) {
			dbg->warning( "hausbauer_t::register_desc()", "Object %s could not be registered since it would overlay an existing factory building!", desc->get_name() );
			delete desc;
			return false;
		}
		dbg->warning( "hausbauer_t::register_desc()", "Object %s was overlaid by addon!", desc->get_name() );
		desc_table.remove(desc->get_name());
		tool_t::general_tool.remove( old_desc->get_builder() );
		delete old_desc->get_builder();
		delete old_desc;
	}

	// probably needs a tool if it has a cursor
	const skin_desc_t *sd = desc->get_cursor();
	if(  sd  &&  sd->get_image_id(1)!=IMG_EMPTY) {
		tool_t *tool;
		if(  desc->get_type()==building_desc_t::depot  ) {
			tool = new tool_build_depot_t();
		}
		else if(  desc->get_type()==building_desc_t::headquarter  ) {
			tool = new tool_headquarter_t();
		}
		else {
			tool = new tool_build_station_t();
		}
		tool->set_icon( desc->get_cursor()->get_image_id(1) );
		tool->cursor = desc->get_cursor()->get_image_id(0),
		tool->set_default_param(desc->get_name());
		tool_t::general_tool.append( tool );
		desc->set_builder( tool );
	}
	else {
		desc->set_builder( NULL );
	}
	desc_table.put(desc->get_name(), desc);

	/* Supply the tiles with a pointer back to the matching description.
	 * This is necessary since each building consists of separate tiles,
	 * even if it is part of the same description (building_desc_t)
	 */
	const int max_index = desc->get_all_layouts()*desc->get_size().x*desc->get_size().y;
	for( int i=0;  i<max_index;  i++  ) {
		const_cast<building_tile_desc_t *>(desc->get_tile(i))->set_desc(desc);
	}

	return true;
}


void hausbauer_t::fill_menu(tool_selector_t* tool_selector, building_desc_t::btype btype, waytype_t wt, sint16 /*sound_ok*/)
{
	// check if scenario forbids this
	uint16 toolnr = 0;
	switch(btype) {
		case building_desc_t::depot:
			toolnr = TOOL_BUILD_DEPOT | GENERAL_TOOL;
			break;
		case building_desc_t::dock:
		case building_desc_t::flat_dock:
		case building_desc_t::generic_stop:
		case building_desc_t::generic_extension:
			toolnr = TOOL_BUILD_STATION | GENERAL_TOOL;
			break;
		default:
			break;
	}
	if(  toolnr > 0  &&  !welt->get_scenario()->is_tool_allowed(welt->get_active_player(), toolnr, wt)  ) {
		return;
	}

	const uint16 time = welt->get_timeline_year_month();
	DBG_DEBUG("hausbauer_t::fill_menu()","maximum %i",station_building.get_count());

	FOR(  vector_tpl<building_desc_t const*>,  const desc,  station_building  ) {
//		DBG_DEBUG("hausbauer_t::fill_menu()", "try to add %s (%p)", desc->get_name(), desc);
		if(  desc->get_type()==btype  &&  desc->get_builder()  &&  (btype==building_desc_t::headquarter  ||  desc->get_extra()==(uint16)wt)  ) {
			if(  desc->is_available(time)  ) {
				tool_selector->add_tool_selector( desc->get_builder() );
			}
		}
	}
}


void hausbauer_t::new_world()
{
	unbuilt_monuments.clear();
	FOR(vector_tpl<building_desc_t const*>, const i, monuments) {
		unbuilt_monuments.append(i);
	}
}


void hausbauer_t::remove( player_t *player, gebaeude_t *gb )
{
	const building_tile_desc_t *tile  = gb->get_tile();
	const building_desc_t *bdsc = tile->get_desc();
	const uint8 layout = tile->get_layout();

	// get start position and size
	const koord3d pos = gb->get_pos() - koord3d( tile->get_offset(), 0 );
	koord size = tile->get_desc()->get_size( layout );
	koord k;

	if(  tile->get_desc()->get_type() == building_desc_t::headquarter  ) {
		gb->get_owner()->add_headquarter( 0, koord::invalid );
	}
	if(tile->get_desc()->get_type()==building_desc_t::monument) {
		unbuilt_monuments.append_unique(tile->get_desc());
	}

	// then remove factory
	fabrik_t *fab = gb->get_fabrik();
	if(fab) {
		// first remove fabrik_t pointers
		for(k.y = 0; k.y < size.y; k.y ++) {
			for(k.x = 0; k.x < size.x; k.x ++) {
				grund_t *gr = welt->lookup(koord3d(k,0)+pos);
				// for buildings with holes the hole could be on a different height ->gr==NULL
				if (gr) {
					gebaeude_t *gb_part = gr->find<gebaeude_t>();
					if(gb_part) {
						// there may be buildings with holes, so we only remove our building!
						if(gb_part->get_tile()  ==  bdsc->get_tile(layout, k.x, k.y)) {
							gb_part->set_fab( NULL );
							planquadrat_t *plan = welt->access( k+pos.get_2d() );
							for (size_t i = plan->get_haltlist_count(); i-- != 0;) {
								halthandle_t halt = plan->get_haltlist()[i];
								halt->remove_fabriken( fab );
								plan->remove_from_haltlist( halt );
							}
						}
					}
				}
			}
		}
		// tell players of the deletion
		for(uint8 i=0; i<MAX_PLAYER_COUNT; i++) {
			player_t *player = welt->get_player(i);
			if (player) {
				player->notify_factory(player_t::notify_delete, fab);
			}
		}
		// remove all transformers
		for(k.y = -1; k.y < size.y+1;  k.y ++) {
			for(k.x = -1; k.x < size.x+1;  k.x ++) {
				grund_t *gr = NULL;
				if (0<=k.x  &&  k.x<size.x  &&  0<=k.y  &&  k.y<size.y) {
					// look below factory
					gr = welt->lookup(koord3d(k,-1) + pos);
				}
				else {
					// find transformers near factory
					gr = welt->lookup_kartenboden(k + pos.get_2d());
				}
				if (gr) {
					senke_t *sk = gr->find<senke_t>();
					if (  sk  &&  sk->get_factory()==fab  ) {
						sk->mark_image_dirty(sk->get_image(), 0);
						delete sk;
					}
					pumpe_t* pp = gr->find<pumpe_t>();
					if (  pp  &&  pp->get_factory()==fab  ) {
						pp->mark_image_dirty(pp->get_image(), 0);
						delete pp;
					}
					// remove tunnel
					if(  (sk!=NULL ||  pp!=NULL)  &&  gr->ist_im_tunnel()  &&  gr->get_top()<=1  ) {
						if (tunnel_t *t = gr->find<tunnel_t>()) {
							t->cleanup( t->get_owner() );
							delete t;
						}
						const koord p = gr->get_pos().get_2d();
						welt->lookup_kartenboden(p)->clear_flag(grund_t::marked);
						// remove ground
						welt->access(p)->boden_entfernen(gr);
						delete gr;
					}
				}
			}
		}
		// cleared transformers successfully, now remove factory.
		welt->rem_fab(fab);
	}

	// delete our house only
	for(k.y = 0; k.y < size.y; k.y ++) {
		for(k.x = 0; k.x < size.x; k.x ++) {
			grund_t *gr = welt->lookup(koord3d(k,0)+pos);
			if(gr) {
				gebaeude_t *gb_part = gr->find<gebaeude_t>();
				// there may be buildings with holes, so we only remove our building!
				if(  gb_part  &&  gb_part->get_tile()==bdsc->get_tile(layout, k.x, k.y)  ) {
					// ok, now we can go on with deletion
					gb_part->cleanup( player );
					delete gb_part;
					// if this was a station building: delete ground
					if(gr->get_halt().is_bound()) {
						haltestelle_t::remove(player, gr->get_pos());
					}
					// and maybe restore land below
					if(gr->get_typ()==grund_t::fundament) {
						const koord newk = k+pos.get_2d();
						sint8 new_hgt;
						const uint8 new_slope = welt->recalc_natural_slope(newk,new_hgt);
						// test for ground at new height
						const grund_t *gr2 = welt->lookup(koord3d(newk,new_hgt));
						if(  (gr2==NULL  ||  gr2==gr) &&  new_slope!=slope_t::flat  ) {
							// and for ground above new sloped tile
							gr2 = welt->lookup(koord3d(newk, new_hgt+1));
						}
						bool ground_recalc = true;
						if(  gr2  &&  gr2!=gr  ) {
							// there is another ground below or above
							// => do not change height, keep foundation
							welt->access(newk)->kartenboden_setzen( new boden_t( gr->get_pos(), slope_t::flat ) );
							ground_recalc = false;
						}
						else if(  new_hgt <= welt->get_water_hgt(newk)  &&  new_slope == slope_t::flat  ) {
							welt->access(newk)->kartenboden_setzen( new wasser_t( koord3d( newk, new_hgt ) ) );
							welt->calc_climate( newk, true );
						}
						else {
							if(  gr->get_grund_hang() == new_slope  ) {
								ground_recalc = false;
							}
							welt->access(newk)->kartenboden_setzen( new boden_t( koord3d( newk, new_hgt ), new_slope ) );
							welt->calc_climate( newk, true );
						}
						// there might be walls from foundations left => thus some tiles may need to be redrawn
						if(ground_recalc) {
							if(grund_t *gr = welt->lookup_kartenboden(newk+koord::east)) {
								gr->calc_image();
							}
							if(grund_t *gr = welt->lookup_kartenboden(newk+koord::south)) {
								gr->calc_image();
							}
						}
					}
				}
			}
		}
	}
}


gebaeude_t* hausbauer_t::build(player_t* player_, koord3d pos, int org_layout, const building_desc_t* desc, void* param)
{
	gebaeude_t* first_building = NULL;
	koord k;
	koord dim;

	uint8 layout = desc->adjust_layout(org_layout);
	dim = desc->get_size(org_layout);
	bool needs_ground_recalc = false;

	for(k.y = 0; k.y < dim.y; k.y ++) {
		for(k.x = 0; k.x < dim.x; k.x ++) {
//DBG_DEBUG("hausbauer_t::build()","get_tile() at %i,%i",k.x,k.y);
			const building_tile_desc_t *tile = desc->get_tile(layout, k.x, k.y);

			// skip empty tiles
			if (tile == NULL || (
						!(desc->get_type() == building_desc_t::dock  ||  desc->get_type() == building_desc_t::flat_dock)  &&
						tile->get_background(0, 0, 0) == IMG_EMPTY &&
						tile->get_foreground(0, 0)    == IMG_EMPTY
					)) {
						// may have a rotation that is not recoverable
						DBG_MESSAGE("hausbauer_t::build()","get_tile() empty at %i,%i",k.x,k.y);
				continue;
			}
			gebaeude_t *gb = new gebaeude_t(pos + k, player_, tile);
			if (first_building == NULL) {
				first_building = gb;
			}

			if(desc->is_factory()) {
				gb->set_fab((fabrik_t *)param);
			}
			// try to fake old building
			else if(welt->get_zeit_ms() < 2) {
				// Hajo: after staring a new map, build fake old buildings
				gb->add_alter(10000);
			}

			grund_t *gr = welt->lookup_kartenboden(pos.get_2d() + k);
			if(gr->ist_wasser()) {
				gr->obj_add(gb);
			}
			else if(  desc->get_type() == building_desc_t::dock  ||  desc->get_type() == building_desc_t::flat_dock  ) {
				// it's a dock!
				gr->obj_add(gb);
			}
			else {
				// mostly remove everything
				vector_tpl<obj_t *> keptobjs;
				if(!gr->hat_wege()) {
					// save certain object types
					for(  uint8 i = 0;  i < gr->obj_count();  i++  ) {
						obj_t *const obj = gr->obj_bei(i);
						obj_t::typ const objtype = obj->get_typ();
						if(  objtype == obj_t::leitung  ||  objtype == obj_t::pillar  ) {
							keptobjs.append(obj);
						}
					}
					for(  size_t i = 0;  i < keptobjs.get_count();  i++  ) {
						gr->obj_remove(keptobjs[i]);
					}

					// delete everything except vehicles
					gr->obj_loesche_alle(player_);
				}

				// build new foundation
				needs_ground_recalc |= gr->get_grund_hang()!=slope_t::flat;
				grund_t *gr2 = new fundament_t(gr->get_pos(), gr->get_grund_hang());
				welt->access(gr->get_pos().get_2d())->boden_ersetzen(gr, gr2);
				gr = gr2;
//DBG_DEBUG("hausbauer_t::build()","ground count now %i",gr->obj_count());
				gr->obj_add( gb );

				// restore saved objects
				for(  size_t i = 0;  i < keptobjs.get_count();  i++  ) {
					gr->obj_add(keptobjs[i]);
				}

				if(needs_ground_recalc  &&  welt->is_within_limits(pos.get_2d()+k+koord(1,1))  &&  (k.y+1==dim.y  ||  k.x+1==dim.x)) {
					welt->lookup_kartenboden(pos.get_2d()+k+koord(1,0))->calc_image();
					welt->lookup_kartenboden(pos.get_2d()+k+koord(0,1))->calc_image();
					welt->lookup_kartenboden(pos.get_2d()+k+koord(1,1))->calc_image();
				}
			}
			gb->set_pos( gr->get_pos() );
			if(desc->is_attraction()) {
				welt->add_ausflugsziel( gb );
			}
			if (!desc->is_city_building()) {
				if(station_building.is_contained(desc)) {
					(*static_cast<halthandle_t *>(param))->add_grund(gr);
				}
				if(  desc->get_type() == building_desc_t::dock  ||  desc->get_type() == building_desc_t::flat_dock  ) {
					// its a dock!
					gb->set_yoff(0);
				}
			}
			gr->calc_image();
			reliefkarte_t::get_karte()->calc_map_pixel(gr->get_pos().get_2d());
		}
	}
	// remove only once ...
	if(desc->get_type()==building_desc_t::monument) {
		monument_erected(desc);
	}
	return first_building;
}


gebaeude_t *hausbauer_t::build_station_extension_depot(player_t *player, koord3d pos, int built_layout, const building_desc_t *desc, void *param)
{
	uint8 corner_layout = 6;	// assume single building (for more than 4 layouts)

	// adjust layout of neighbouring building
	if(desc->is_transport_building()  &&  desc->get_all_layouts()>1) {

		int layout = built_layout & 9;

		// detect if we are connected at far (north/west) end
		sint8 offset = welt->lookup( pos )->get_weg_yoff()/TILE_HEIGHT_STEP;
		koord3d checkpos = pos+koord3d( (layout & 1 ? koord::east : koord::south), offset);
		grund_t * gr = welt->lookup( checkpos );

		if(!gr) {
			// check whether bridge end tile
			grund_t * gr_tmp = welt->lookup( pos+koord3d( (layout & 1 ? koord::east : koord::south),offset - 1) );
			if(gr_tmp && gr_tmp->get_weg_yoff()/TILE_HEIGHT_STEP == 1) {
				gr = gr_tmp;
			}
			else {
				gr_tmp = welt->lookup( pos+koord3d( (layout & 1 ? koord::east : koord::south),offset - 2) );
				if(gr_tmp && gr_tmp->get_weg_yoff()/TILE_HEIGHT_STEP == 2) {
					gr = gr_tmp;
				}
			}
		}

		if(gr) {
			gebaeude_t* gb = gr->find<gebaeude_t>();
			if(gb==NULL) {
				// no building on same level, check other levels
				const planquadrat_t *pl = welt->access(checkpos.get_2d());
				if (pl) {
					for(  uint8 i=0;  i<pl->get_boden_count();  i++  ) {
						gr = pl->get_boden_bei(i);
						if(gr->is_halt() && gr->get_halt().is_bound() ) {
							break;
						}
					}
				}
				gb = gr->find<gebaeude_t>();
			}
			if(  gb  &&  gb->get_tile()->get_desc()->is_transport_building()  ) {
				corner_layout &= ~2; // clear near bit
				if(gb->get_tile()->get_desc()->get_all_layouts()>4) {
					koord xy = gb->get_tile()->get_offset();
					uint8 layoutbase = gb->get_tile()->get_layout();

					if((layoutbase & 1) == (layout & 1)) {
						layoutbase &= 0xb; // clear near bit on neighbour
						gb->set_tile( gb->get_tile()->get_desc()->get_tile(layoutbase, xy.x, xy.y), false );
					}
				}
			}
		}

		// detect if near (south/east) end
		gr = welt->lookup( pos+koord3d( (layout & 1 ? koord::west : koord::north), offset) );
		if(!gr) {
			// check whether bridge end tile
			grund_t * gr_tmp = welt->lookup( pos+koord3d( (layout & 1 ? koord::west : koord::north),offset - 1) );
			if(gr_tmp && gr_tmp->get_weg_yoff()/TILE_HEIGHT_STEP == 1) {
				gr = gr_tmp;
			}
			else {
				gr_tmp = welt->lookup( pos+koord3d( (layout & 1 ? koord::west : koord::north),offset - 2) );
				if(gr_tmp && gr_tmp->get_weg_yoff()/TILE_HEIGHT_STEP == 2) {
					gr = gr_tmp;
				}
			}
		}
		if(gr) {
			gebaeude_t* gb = gr->find<gebaeude_t>();
			if(gb  &&  gb->get_tile()->get_desc()->is_transport_building()) {
				corner_layout &= ~4; // clear far bit

				if(gb->get_tile()->get_desc()->get_all_layouts()>4) {
					koord xy = gb->get_tile()->get_offset();
					uint8 layoutbase = gb->get_tile()->get_layout();
					if((layoutbase & 1) == (layout & 1)) {
						layoutbase &= 0xd; // clear far bit on neighbour
						gb->set_tile( gb->get_tile()->get_desc()->get_tile(layoutbase, xy.x, xy.y), false );
					}
				}
			}
		}
	}

	// adjust layouts of the new building
	if(desc->get_all_layouts()>4) {
		built_layout = (corner_layout | (built_layout&9) ) % desc->get_all_layouts();
	}

	const building_tile_desc_t *tile = desc->get_tile(built_layout, 0, 0);
	gebaeude_t *gb;
	if(  desc->get_type() == building_desc_t::depot  ) {
		switch(  desc->get_extra()  ) {
			case track_wt:
				gb = new bahndepot_t(pos, player, tile);
				break;
			case tram_wt:
				gb = new tramdepot_t(pos, player, tile);
				break;
			case monorail_wt:
				gb = new monoraildepot_t(pos, player, tile);
				break;
			case maglev_wt:
				gb = new maglevdepot_t(pos, player, tile);
				break;
			case narrowgauge_wt:
				gb = new narrowgaugedepot_t(pos, player, tile);
				break;
			case road_wt:
				gb = new strassendepot_t(pos, player, tile);
				break;
			case water_wt:
				gb = new schiffdepot_t(pos, player, tile);
				break;
			case air_wt:
				gb = new airdepot_t(pos, player, tile);
				break;
			default:
				dbg->fatal("hausbauer_t::build_station_extension_depot()","waytpe %i has no depots!", desc->get_extra() );
				break;
		}
	}
	else {
		gb = new gebaeude_t(pos, player, tile);
	}
//DBG_MESSAGE("hausbauer_t::build_station_extension_depot()","building stop pri=%i",pri);

	// remove pointer
	grund_t *gr = welt->lookup(pos);
	zeiger_t* zeiger = gr->find<zeiger_t>();
	if(  zeiger  ) {
		gr->obj_remove(zeiger);
		zeiger->set_flag(obj_t::not_on_map);
	}

	gr->obj_add(gb);

	if(  station_building.is_contained(desc)  &&  desc->get_type()!=building_desc_t::depot  ) {
		// is a station/bus stop
		(*static_cast<halthandle_t *>(param))->add_grund(gr);
		gr->calc_image();
	}
	else {
		gb->calc_image();
	}

	if(desc->is_attraction()) {
		welt->add_ausflugsziel( gb );
	}

	// update minimap
	reliefkarte_t::get_karte()->calc_map_pixel(gb->get_pos().get_2d());

	return gb;
}


const building_tile_desc_t *hausbauer_t::find_tile(const char *name, int org_idx)
{
	int idx = org_idx;
	const building_desc_t *desc = desc_table.get(name);
	if(desc) {
		const int size = desc->get_y()*desc->get_x();
		if(  idx >= desc->get_all_layouts()*size  ) {
			idx %= desc->get_all_layouts()*size;
			DBG_MESSAGE("gebaeude_t::rdwr()","%s using tile %i instead of %i",name,idx,org_idx);
		}
		return desc->get_tile(idx);
	}
//	DBG_MESSAGE("hausbauer_t::find_tile()","\"%s\" not in hashtable",name);
	return NULL;
}


const building_desc_t* hausbauer_t::get_desc(const char *name)
{
	return desc_table.get(name);
}


const building_desc_t* hausbauer_t::get_random_station(const building_desc_t::btype type, const waytype_t wt, const uint16 time, const uint8 enables)
{
	weighted_vector_tpl<const building_desc_t*> stops;

	if(  wt < 0  ) {
		return NULL;
	}

	FOR(vector_tpl<building_desc_t const*>, const desc, station_building) {
		if(  desc->get_type()==type  &&  desc->get_extra()==(uint32)wt  &&  (enables==0  ||  (desc->get_enabled()&enables)!=0)  ) {
			// skip underground stations
			if(  !desc->can_be_built_aboveground()  ) {
				continue;
			}
			// ok, now check timeline
			if(  desc->is_available(time)  ) {
				stops.append(desc,max(1,16-desc->get_level()*desc->get_x()*desc->get_y()));
			}
		}
	}
	return stops.empty() ? 0 : pick_any_weighted(stops);
}


const building_desc_t* hausbauer_t::get_special(uint32 bev, building_desc_t::btype type, uint16 time, bool ignore_retire, climate cl)
{
	weighted_vector_tpl<const building_desc_t *> auswahl(16);

	vector_tpl<const building_desc_t*> *list = NULL;
	switch(type) {
		case building_desc_t::townhall:
			list = &townhalls;
			break;
		case building_desc_t::attraction_city:
			list = &attractions_city;
			break;
		default:
			return NULL;
	}
	FOR(vector_tpl<building_desc_t const*>, const desc, *list) {
		// extra data contains number of inhabitants for building
		if(  desc->get_extra()==bev  ) {
			if(  cl==MAX_CLIMATES  ||  desc->is_allowed_climate(cl)  ) {
				// ok, now check timeline
				if(  time==0  ||  (desc->get_intro_year_month()<=time  &&  (ignore_retire  ||  desc->get_retire_year_month() > time)  )  ) {
					auswahl.append(desc, desc->get_chance());
				}
			}
		}
	}
	if (auswahl.empty()) {
		return 0;
	}
	else if(auswahl.get_count()==1) {
		return auswahl.front();
	}
	// now there is something to choose
	return pick_any_weighted(auswahl);
}


/**
 * Tries to find a matching house desc from @p list.
 * This method will never return NULL if there is at least one valid entry in the list.
 * @param level the minimum level of the house/station
 * @param cl allowed climates
 */
static const building_desc_t* get_city_building_from_list(const vector_tpl<const building_desc_t*>& list, int level, uint16 time, climate cl, uint32 clusters )
{
	weighted_vector_tpl<const building_desc_t *> selections(16);

//	DBG_MESSAGE("hausbauer_t::get_aus_liste()","target level %i", level );
	const building_desc_t *besch_at_least=NULL;
	FOR(vector_tpl<building_desc_t const*>, const desc, list) {
		if(  desc->is_allowed_climate(cl)  &&  desc->get_chance()>0  &&  desc->is_available(time)  ) {
				besch_at_least = desc;
		}

		const int thislevel = desc->get_level();
		if(thislevel>level) {
			if (selections.empty()) {
				// Nothing of the correct level. Continue with search on a higher level.
				level = thislevel;
			}
			else {
				// We already found something of the correct level; stop.
				break;
			}
		}

		if(  thislevel == level  &&  desc->get_chance() > 0  ) {
			if(  cl==MAX_CLIMATES  ||  desc->is_allowed_climate(cl)  ) {
				if(  desc->is_available(time)  ) {
//					DBG_MESSAGE("hausbauer_t::get_city_building_from_list()","appended %s at %i", desc->get_name(), thislevel );
					/* Level, time period, and climate are all OK.
					 * Now modify the chance rating by a factor based on the clusters.
					 */
					// FIXME: the factor should be configurable by the pakset/
					int chance = desc->get_chance();
					if(  clusters  ) {
						uint32 my_clusters = desc->get_clusters();
						if(  my_clusters & clusters  ) {
							chance *= stadt_t::get_cluster_factor();
						}
						else {
							chance /= stadt_t::get_cluster_factor();
						}
					}
					selections.append(desc, chance);
				}
			}
		}
	}

	if(selections.get_sum_weight()==0) {
		// this is some level below, but at least it is something
		return besch_at_least;
	}
	if(selections.get_count()==1) {
		return selections.front();
	}
	// now there is something to choose
	return pick_any_weighted(selections);
}


const building_desc_t* hausbauer_t::get_commercial(int level, uint16 time, climate cl, uint32 clusters)
{
	return get_city_building_from_list(city_commercial, level, time, cl, clusters);
}


const building_desc_t* hausbauer_t::get_industrial(int level, uint16 time, climate cl, uint32 clusters)
{
	return get_city_building_from_list(city_industry, level, time, cl, clusters);
}


const building_desc_t* hausbauer_t::get_residential(int level, uint16 time, climate cl, uint32 clusters)
{
	return get_city_building_from_list(city_residential, level, time, cl, clusters);
}


const building_desc_t* hausbauer_t::get_headquarter(int level, uint16 time)
{
	if(  level<0  ) {
		return NULL;
	}
	FOR(vector_tpl<building_desc_t const*>, const desc, hausbauer_t::headquarter) {
		if(  desc->get_extra()==(uint32)level  &&  desc->is_available(time)  ) {
			return desc;
		}
	}
	return NULL;
}


const building_desc_t *hausbauer_t::get_random_desc(vector_tpl<const building_desc_t *> &list, uint16 time, bool ignore_retire, climate cl)
{
	if (!list.empty()) {
		// previously just returned a random object; however, now we look at the chance entry
		weighted_vector_tpl<const building_desc_t *> auswahl(16);
		FOR(vector_tpl<building_desc_t const*>, const desc, list) {
			if((cl==MAX_CLIMATES  ||  desc->is_allowed_climate(cl))  &&  desc->get_chance()>0  &&  (time==0  ||  (desc->get_intro_year_month()<=time  &&  (ignore_retire  ||  desc->get_retire_year_month()>time)  )  )  ) {
//				DBG_MESSAGE("hausbauer_t::get_random_desc()","appended %s at %i", desc->get_name(), thislevel );
				auswahl.append(desc, desc->get_chance());
			}
		}
		// now look what we have got ...
		if(auswahl.get_sum_weight()==0) {
			return NULL;
		}
		if(auswahl.get_count()==1) {
			return auswahl.front();
		}
		// now there is something to choose
		return pick_any_weighted(auswahl);
	}
	return NULL;
}


const vector_tpl<const building_desc_t*>* hausbauer_t::get_list(const building_desc_t::btype typ)
{
	switch (typ) {
		case building_desc_t::monument:         return &unbuilt_monuments;
		case building_desc_t::attraction_land: return &attractions_land;
		case building_desc_t::headquarter:      return &headquarter;
		case building_desc_t::townhall:         return &townhalls;
		case building_desc_t::attraction_city: return &attractions_city;
		case building_desc_t::dock:
		case building_desc_t::flat_dock:
		case building_desc_t::depot:
		case building_desc_t::generic_stop:
		case building_desc_t::generic_extension:
		                                    return &station_building;
		default:                            return NULL;
	}
}


const vector_tpl<const building_desc_t*>* hausbauer_t::get_citybuilding_list(const building_desc_t::btype typ)
{
	switch (typ) {
		case building_desc_t::city_res:  return &city_residential;
		case building_desc_t::city_com:  return &city_commercial;
		case building_desc_t::city_ind:  return &city_industry;
		default:                      return NULL;
	}
}

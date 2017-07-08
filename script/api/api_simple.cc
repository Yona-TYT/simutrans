#include "api.h"

/** @file api_simple.cc exports simple data types. */

#include "api_simple.h"
#include "../api_class.h"
#include "../api_function.h"
#include "../../dataobj/ribi.h"
#include "../../simworld.h"
#include "../../dataobj/scenario.h"

using namespace script_api;


#ifdef DOXYGEN
/**
 * Struct to hold information about time as month-year.
 * If used as argument to functions then only the value of @ref raw matters.
 * If filled by an api-call the year and month will be set, too.
 * Relation: raw = 12*month + year.
 * @see world::get_time obj_desc_time_x
 */
class time_x { // begin_class("time_x")
public:
#ifdef SQAPI_DOC // document members
	integer raw;   ///< raw integer value of date
	integer year;  ///< year
	integer month; ///< month in 0..11
#endif
}; // end_class

/**
 * Struct to get precise information about time.
 * @see world::get_time
 */
class time_ticks_x : public time_x { // begin_class("time_ticks_x", "time_x")
public:
#ifdef SQAPI_DOC // document members
	integer ticks;            ///< current time in ticks
	integer ticks_per_month;  ///< length of one in-game month in ticks
	integer next_month_ticks; ///< new month will start at this time
#endif
}; // end_class
#endif

// pushes table = { raw = , year = , month = }
SQInteger param<mytime_t>::push(HSQUIRRELVM vm, mytime_t const& v)
{
	sq_newtableex(vm, 6);
	create_slot<uint32>(vm, "raw",   v.raw);
	create_slot<uint32>(vm, "year",  v.raw/12);
	create_slot<uint32>(vm, "month", v.raw%12);
	return 1;
}

SQInteger param<mytime_ticks_t>::push(HSQUIRRELVM vm, mytime_ticks_t const& v)
{
	param<mytime_t>::push(vm, v);
	create_slot<uint32>(vm, "ticks",            v.ticks);
	create_slot<uint32>(vm, "ticks_per_month",  v.ticks_per_month);
	create_slot<uint32>(vm, "next_month_ticks", v.next_month_ticks);
	return 1;
}


mytime_t param<mytime_t>::get(HSQUIRRELVM vm, SQInteger index)
{
	// 0 has special meaning of 'no-timeline'
	SQInteger i=1;
	if (!SQ_SUCCEEDED(sq_getinteger(vm, index, &i))) {
		get_slot(vm, "raw", i, index);
	}
	return (uint16) (i >= 0 ? i : 1);
}


#define map_ribi_any(f,type) \
	type export_## f(my_ribi_t r) \
	{ \
		return ribi_t::f(r); \
	}

// export the ribi functions
map_ribi_any(is_single, bool);
map_ribi_any(is_twoway, bool);
map_ribi_any(is_threeway, bool);
map_ribi_any(is_bend, bool);
map_ribi_any(is_straight, bool);
map_ribi_any(doubles, my_ribi_t);
map_ribi_any(backward, my_ribi_t);

void export_simple(HSQUIRRELVM vm)
{

	/**
	 * Class that holds 2d coordinates.
	 * All functions that use this as input parameters will accept every data structure that contains members x and y.
	 *
	 * Coordinates always refer to the original rotation in @ref map.file.
	 * They will be rotated if transferred between the game engine and squirrel.
	 */
	begin_class(vm, "coord");
#ifdef SQAPI_DOC // document members
	/// x-coordinate
	integer x;
	/// y-coordinate
	integer y;
	// operators are defined in script_base.nut
	coord(int x, int y);
	coord operator + (coord other);
	coord operator - (coord other);
	coord operator - ();
	coord operator * (integer fac);
	coord operator / (integer fac);
	/**
	 * Converts coordinate to string containing the coordinates in the current rotation of the map.
	 *
	 * Cannot be used in links in scenario texts. Use @ref href instead.
	 */
	string _tostring();
	/**
	 * Generates text to generate links to coordinates in scenario texts.
	 * @param text text to be shown in the link
	 * @returns a-tag with link in href
	 * @see get_rule_text
	 */
	string href(string text);
#endif
	end_class(vm);

	/**
	 * Class that holds 3d coordinates.
	 * All functions that use this as input parameters will accept every data structure that contains members x, y, and z.
	 *
	 * Coordinates always refer to the original rotation in @ref map.file.
	 * They will be rotated if transferred between the game engine and squirrel.
	 */
	begin_class(vm, "coord3d", "coord");
#ifdef SQAPI_DOC // document members
	/// x-coordinate
	integer x;
	/// y-coordinate
	integer y;
	/// z-coordinate - height
	integer z;
	// operators are defined in script_base.nut
	coord(int x, int y, int z);
	coord3d operator + (coord3d other);
	coord3d operator - (coord other);
	coord3d operator + (coord3d other);
	coord3d operator - (coord other);
	coord3d operator - ();
	coord3d operator * (integer fac);
	coord3d operator / (integer fac);
	/**
	 * Converts coordinate to string containing the coordinates in the current rotation of the map.
	 *
	 * Cannot be used in links in scenario texts. Use @ref href instead.
	 */
	string _tostring();
	/**
	 * Generates text to generate links to coordinates in scenario texts.
	 * @param text text to be shown in the link
	 * @returns a-tag with link in href
	 * @see get_rule_text
	 */
	string href(string text);
#endif
	end_class(vm);

	/**
	 * Class holding static methods to work with directions.
	 * Directions are just bit-encoded integers.
	 */
	begin_class(vm, "dir");

#ifdef SQAPI_DOC // document members
	/** @name Named directions. */
	//@{
	static const dir none;
	static const dir north;
	static const dir east;
	static const dir northeast;
	static const dir south;
	static const dir northsouth;
	static const dir southeast;
	static const dir northsoutheast;
	static const dir west;
	static const dir northwest;
	static const dir eastwest;
	static const dir northeastwest;
	static const dir southwest;
	static const dir northsouthwest;
	static const dir southeastwest;
	static const dir all;
	//@}
#endif
	/**
	 * @param d direction to test
	 * @return whether direction is single direction, i.e. just one of n/s/e/w
	 */
	STATIC register_method(vm, &export_is_single,  "is_single", false, true);
	/**
	 * @param d direction to test
	 * @return whether direction is double direction, e.g. n+e, n+s.
	 */
	STATIC register_method(vm, &export_is_twoway,  "is_twoway", false, true);
	/**
	 * @param d direction to test
	 * @return whether direction is triple direction, e.g. n+s+e.
	 */
	STATIC register_method(vm, &export_is_threeway,  "is_threeway", false, true);
	/**
	 * @param d direction to test
	 * @return whether direction is curve, e.g. n+e, s+w.
	 */
	STATIC register_method(vm, &export_is_bend,  "is_curve", false, true);
	/**
	 * @param d direction to test
	 * @return whether direction is straight and has no curves in it, e.g. n+w, w.
	 */
	STATIC register_method(vm, &export_is_straight,  "is_straight", false, true);
	/**
	 * @param d direction
	 * @return complements direction to complete straight, i.e. w -> w+e, but n+w -> 0.
	 */
	STATIC register_method(vm, &export_doubles,  "double", false, true);
	/**
	 * @param d direction to test
	 * @return backward direction, e.g. w -> e, n+w -> s+e, n+w+s -> e.
	 */
	STATIC register_method(vm, &export_backward,  "backward", false, true);

	end_class(vm);
}

/*
 * Copyright (c) 2007 prissi
 *
 * This file is part of the Simutrans project and may not be used
 * in other projects without written permission of the author.
 */

#include <stdio.h>

#include "../simdebug.h"
#include "../simvehikel.h"
#include "../simworld.h"
#include "../simdings.h"
#include "../simimg.h"
#include "../simsound.h"

#include "../besch/kreuzung_besch.h"

#include "../boden/grund.h"
#include "../boden/wege/weg.h"

#include "../dataobj/loadsave.h"
#include "../dataobj/umgebung.h"

#include "../utils/simstring.h"
#include "../utils/cbuffer_t.h"

#include "crossing.h"



crossing_t::crossing_t(karte_t *welt, loadsave_t *file) : ding_t (welt)
{
	bild = after_bild = IMG_LEER;
	logic = NULL;
	rdwr(file);
	assert(logic);
}



crossing_t::crossing_t(karte_t *welt, spieler_t *sp, koord3d pos, const kreuzung_besch_t *besch, uint8 ns ) :  ding_t(welt, pos)
{
	this->ns = ns;
	this->besch = besch;
	logic = NULL;
	bild = after_bild = IMG_LEER;
	setze_besitzer( sp );
}



crossing_t::~crossing_t()
{
	grund_t *gr = welt->lookup( gib_pos() );
	if(gr) {
		gr->obj_remove(this);
		setze_pos(koord3d::invalid);
	}
	if(logic) {
		logic->remove(this);
	}
}



void crossing_t::rotate90()
{
	ding_t::rotate90();
	ns = ~ns;
}



// change state; mark dirty and plays sound
void crossing_t::state_changed()
{
	mark_image_dirty( bild, 0 );
	mark_image_dirty( after_bild, 0 );
	calc_bild();
	set_flag(ding_t::dirty);
}



/**
 * Dient zur Neuberechnung des Bildes
 * @author Hj. Malthaner
 */
void
crossing_t::calc_bild()
{
	if(logic) {
		uint8 zustand = logic->get_state();
		// recalc bild each step ...
		const bild_besch_t *a = besch->gib_bild_after( ns, zustand!=crossing_logic_t::CROSSING_CLOSED, 0 );
		after_bild = a ? a->gib_nummer() : IMG_LEER;
		const bild_besch_t *b = besch->gib_bild( ns, zustand!=crossing_logic_t::CROSSING_CLOSED, 0 );
		bild = b ? b->gib_nummer() : IMG_LEER;
	}
}



void
crossing_t::rdwr(loadsave_t *file)
{
	uint8 zustand = logic==NULL ? crossing_logic_t::CROSSING_INVALID : logic->get_state();
	ding_t::rdwr(file);

	// variables ... attention, logic now in crossing_logic_t
	file->rdwr_byte(zustand, " ");
	file->rdwr_byte(ns, " ");
	if(file->get_version()<99016) {
		uint32 ldummy=0;
		uint8 bdummy=0;
		file->rdwr_byte(bdummy, " ");
		file->rdwr_long(ldummy, " ");
	}
	// which waytypes?
	if(file->is_saving()) {
		uint8 wt = besch->get_waytype(0);
		file->rdwr_byte(wt,"w");
		wt = besch->get_waytype(1);
		file->rdwr_byte(wt,"w");
	}
	else {
		uint8 w1, w2;
		file->rdwr_byte(w1,"w");
		file->rdwr_byte(w2,"w");
		besch = crossing_logic_t::get_crossing( (waytype_t)w1, (waytype_t)w2 );
		if(besch==NULL) {
			dbg->fatal("crossing_t::crossing_t()","requested for waytypes %i and %i but nothing defined!", w1, w2 );
		}
		crossing_logic_t::add( welt, this, zustand );
	}
}




/**
 * Wird nach dem Laden der Welt aufgerufen - üblicherweise benutzt
 * um das Aussehen des Dings an Boden und Umgebung anzupassen
 *
 * @author Hj. Malthaner
 */
void crossing_t::laden_abschliessen()
{
	grund_t *gr=welt->lookup(gib_pos());
	if(gr==NULL  ||  !gr->hat_weg(besch->get_waytype(0))  ||  !gr->hat_weg(besch->get_waytype(1))) {
		dbg->error("crossing_t::laden_abschliessen","way/ground missing at %i,%i => ignore", gib_pos().x, gib_pos().y );
	}
	else {
		// after loading restore speedlimits
		weg_t *w1=gr->gib_weg(besch->get_waytype(0));
		w1->count_sign();
		weg_t *w2=gr->gib_weg(besch->get_waytype(1));
		w2->count_sign();
		ns = ribi_t::ist_gerade_ns(w2->gib_ribi_unmasked());
		if(logic==NULL) {
			crossing_logic_t::add( welt, this, crossing_logic_t::CROSSING_INVALID );
		}
		else {
			logic->recalc_state();
		}
	}
}


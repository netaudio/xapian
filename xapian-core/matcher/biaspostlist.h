/* biaspostlist.h: add on extra weight based on functor
 *
 * ----START-LICENCE----
 * Copyright 1999,2000,2001 BrightStation PLC
 * Copyright 2001 Ananova Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 * -----END-LICENCE-----
 */

#ifndef OM_HGUARD_BIASPOSTLIST_H
#define OM_HGUARD_BIASPOSTLIST_H

#include <time.h>
#include <math.h>
#include "om/omdatabase.h"
#include "postlist.h"
#include "multimatch.h"

class OmBiasFunctor {
    private:
#ifndef DOCID_BASED
	time_t now;
	OmDatabase db;
	om_weight max_w;
#else /* DOCID_BASED */
	om_docid max_id;
	om_weight max_w;
	PostList *pl;
#endif /* DOCID_BASED */
    public:
	OmBiasFunctor(const OmDatabase &db_)
#ifndef DOCID_BASED
	    : now(time(NULL)), db(db_), max_w(10000) {}
#else /* DOCID_BASED */
	    : max_id(db_.get_doccount()), max_w(7000) {}
#endif /* DOCID_BASED */

	om_weight get_maxweight() {
	    return max_w; 
	}

	om_weight get_weight(om_docid id) {
#ifndef DOCID_BASED
	    OmKey key = db.get_document(id).get_key(0);
	    time_t t = atoi(key.value.c_str());
	    if (t >= now) return max_w;
#else /* DOCID_BASED */
	    if (id >= max_id) return max_w;
#endif /* DOCID_BASED */
	    // the same story but 2 days old gets half the extra weight
#ifndef DOCID_BASED
	    return max_w * exp((t - now) * .6931472 / 60 / 60 / 24 / 2);
#else /* DOCID_BASED */
	    // (assumes ~250 docs per day)
	    return max_w * exp(double(max_id - id) * -0.6931472 / 250 / 2);
#endif /* DOCID_BASED */
	}
};

/// A postlist which adds on an extra weight contribution from a functor
class BiasPostList : public PostList {
    private:
	PostList *pl;
	OmDatabase db;
	OmBiasFunctor *bias;
	MultiMatch *matcher;
	om_weight max_bias;
	om_weight w;

    public:
	om_doccount get_termfreq_max() const { return pl->get_termfreq_max(); }
	om_doccount get_termfreq_min() const { return pl->get_termfreq_min(); }
	om_doccount get_termfreq_est() const { return pl->get_termfreq_est(); }

	om_docid get_docid() const { return pl->get_docid(); }

	om_weight get_weight() const {
	    return w + bias->get_weight(pl->get_docid());
	}

	om_weight get_maxweight() const {
	    return pl->get_maxweight() + max_bias;
	}

        om_weight recalc_maxweight() {
	    return pl->recalc_maxweight() + max_bias;
	}

	PostList *next(om_weight w_min) {
	    do {
		PostList *p = pl->next(w_min - max_bias);
		if (p) {
		    delete pl;
		    pl = p;
		    if (matcher) matcher->recalc_maxweight();
		}
		if (pl->at_end()) break;
		w = pl->get_weight();
	    } while (w + max_bias < w_min);
	    return NULL;
	}
	    
	PostList *skip_to(om_docid did, om_weight w_min) {
	    do {
		PostList *p = pl->skip_to(did, w_min - max_bias);
		if (p) {
		    delete pl;
		    pl = p;
		    if (matcher) matcher->recalc_maxweight();
		}
		if (pl->at_end()) break;
		w = pl->get_weight();
	    } while (w + max_bias < w_min);
	    return NULL;
	}

	bool at_end() const { return pl->at_end(); }

	std::string get_description() const {
	    return "( Bias " + pl->get_description() + " )";
	}

	/** Return the document length of the document the current term
	 *  comes from.
	 */
	virtual om_doclength get_doclength() const {
	    return pl->get_doclength();
	}

	virtual PositionList * read_position_list() {
	    return pl->read_position_list();
	}

	virtual AutoPtr<PositionList> open_position_list() const {
	    return pl->open_position_list();
	}

	BiasPostList(PostList *pl_, const OmDatabase &db_, OmBiasFunctor *bias_,
		     MultiMatch *matcher_)
		: pl(pl_), db(db_), bias(bias_), matcher(matcher_),
		  max_bias(bias->get_maxweight())
	{ }

	~BiasPostList() {
	    delete pl;
	    delete bias;
	}
};

#endif /* OM_HGUARD_BIASPOSTLIST_H */

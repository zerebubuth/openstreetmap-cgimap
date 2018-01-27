#ifndef CHANGESET_UPDATER
#define CHANGESET_UPDATER

#include "types.hpp"
#include "util.hpp"

#include "cgimap/api06/changeset_upload/osmchange_tracking.hpp"
#include "cgimap/backend/apidb/changeset_upload/transaction_manager.hpp"


class Changeset_Updater {

public:
	explicit Changeset_Updater(Transaction_Manager & _m, osm_changeset_id_t _changeset,
			osm_user_id_t _uid);

	void lock_current_changeset();

	void update_changeset(long num_new_changes, bbox_t bbox) ;

private:
	Transaction_Manager& m;
	int num_changes;
	osm_changeset_id_t changeset;
	osm_user_id_t uid;

};

#endif

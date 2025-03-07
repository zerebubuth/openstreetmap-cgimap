/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/backend/apidb/changeset_upload/changeset_updater.hpp"
#include "cgimap/backend/apidb/pqxx_string_traits.hpp"
#include "cgimap/backend/apidb/utils.hpp"
#include "cgimap/backend/apidb/transaction_manager.hpp"
#include "cgimap/http.hpp"
#include "cgimap/options.hpp"
#include "cgimap/request_context.hpp"

#include <fmt/core.h>
#include <pqxx/pqxx>


ApiDB_Changeset_Updater::ApiDB_Changeset_Updater(Transaction_Manager &_m,
                                                 const RequestContext& _req_ctx,
                                                 osm_changeset_id_t _changeset)
  : m(_m),
    req_ctx(_req_ctx),
    changeset(_changeset)
{
  if (!req_ctx.user.has_value())
  {
    throw http::server_error("Cannot create changeset - no user id");
  }
}

void ApiDB_Changeset_Updater::lock_current_changeset(bool check_max_elements_limit) {

  // check_max_elements_limit: raise a conflict exception, if the changeset contains the maximum
  // number of elements already, i.e. no further objects can be added to the changeset.

  bool is_closed = false;
  std::string closed_at;
  std::string current_time;

  check_user_owns_changeset();

  lock_cs(is_closed, closed_at, current_time);

  if (is_closed)
    throw http::conflict(fmt::format("The changeset {:d} was closed at {}", changeset, closed_at));

  // Some clients try to send further changes, although the changeset already
  // holds the maximum number of elements. As this is futile, we raise an error
  // as early as possible.
  if (check_max_elements_limit && cs_num_changes >= global_settings::get_changeset_max_elements())
    throw http::conflict(fmt::format("The changeset {:d} was closed at {}",  changeset, current_time));

}

void ApiDB_Changeset_Updater::update_changeset(const uint32_t num_new_changes,
                                               const bbox_t bbox) {

  // Don't raise an exception when reaching exactly changeset_max_elements!
  if (cs_num_changes + num_new_changes > global_settings::get_changeset_max_elements()) {

      auto r = m.exec(
	  R"(SELECT to_char((now() at time zone 'utc'),'YYYY-MM-DD HH24:MI:SS "UTC"') as current_time)");

      throw http::conflict(fmt::format("The changeset {:d} was closed at {}", changeset, r[0]["current_time"].as<std::string>()));
  }

  cs_num_changes += num_new_changes;

  bbox_t undefined_bbox; // bounding box with default value outside valid
  // lat/lon range

  // Update current changeset bounding box with new bounds
  cs_bbox.expand(bbox);

  bool valid_bbox = !(cs_bbox == undefined_bbox);

  /*
   update for closed_at according to logic in update_closed_at in
   changeset.rb:
   set the auto-close time to be one hour in the future unless
   that would make it more than 24h long, in which case clip to
   24h, as this has been decided is a reasonable time limit.

   */

  m.prepare("changeset_update_w_bbox",
	    R"(
       UPDATE changesets
       SET num_changes = ($1 :: integer),
           min_lat = $2,
           min_lon = $3,
           max_lat = $4,
           max_lon = $5,
           closed_at =
             CASE
                WHEN (closed_at - created_at) >
                     (($6 ::interval) - ($7 ::interval)) THEN
                  created_at + ($6 ::interval)
                ELSE
                  now() at time zone 'utc' + ($7 ::interval)
             END
       WHERE id = $8

       )"_M);

  m.prepare("changeset_update",
	    R"(
       UPDATE changesets
       SET num_changes = ($1 :: integer),
           closed_at =
             CASE
                WHEN (closed_at - created_at) >
                     (($2 ::interval) - ($3 ::interval)) THEN
                  created_at + ($2 ::interval)
                ELSE
                  now() at time zone 'utc' + ($3 ::interval)
             END
       WHERE id = $4

       )"_M);

  if (valid_bbox) {
      auto r = m.exec_prepared("changeset_update_w_bbox", cs_num_changes, cs_bbox.minlat, cs_bbox.minlon,
			  cs_bbox.maxlat, cs_bbox.maxlon,
			  global_settings::get_changeset_timeout_open_max(),
			  global_settings::get_changeset_timeout_idle(), changeset);
      if (r.affected_rows() != 1)
	throw http::server_error("Cannot update changeset");
  } else {
      auto r = m.exec_prepared("changeset_update", cs_num_changes,
			       global_settings::get_changeset_timeout_open_max(),
			       global_settings::get_changeset_timeout_idle(), changeset);
      if (r.affected_rows() != 1)
	throw http::server_error("Cannot update changeset");
  }
}

void ApiDB_Changeset_Updater::changeset_update_users_cs_count()
{

  m.prepare (
      "update_users",
      R"(
        UPDATE users
             SET "changesets_count" = COALESCE("changesets_count", 0) + 1
             WHERE "id" = $1
   )"_M);

  auto r = m.exec_prepared ("update_users", req_ctx.user->id);

  if (r.affected_rows () != 1)
    throw http::server_error (
        "Cannot create changeset - update changesets_count");
}

osm_changeset_id_t ApiDB_Changeset_Updater::api_create_changeset(const std::map<std::string, std::string>& tags)
{
  changeset_insert_cs();
  changeset_update_users_cs_count();
  changeset_delete_tags();
  changeset_insert_tags(tags);
  changeset_insert_subscriber();
  return changeset;
}

void ApiDB_Changeset_Updater::api_update_changeset(const std::map<std::string, std::string>& tags)
{
  lock_current_changeset(false);
  changeset_delete_tags();
  changeset_insert_tags(tags);
  update_changeset(0, {});
}

void ApiDB_Changeset_Updater::api_close_changeset()
{
  lock_current_changeset(false);

  // Set closed_at timestamp to now() to indicate that the changeset is closed
  m.prepare("changeset_close",
	    R"(
       UPDATE changesets
       SET closed_at = now() at time zone 'utc'
           WHERE id = $1 AND user_id = $2 )"_M);

  auto r = m.exec_prepared("changeset_close", changeset, req_ctx.user->id);

  if (r.affected_rows() != 1)
    throw http::server_error("Cannot close changeset");
}

void ApiDB_Changeset_Updater::lock_cs(bool& is_closed, std::string& closed_at, std::string& current_time)
{
  // Only lock changeset if it belongs to user_id = uid
  m.prepare(
      "changeset_current_lock",
      R"( SELECT id,
		 user_id,
		 created_at,
		 min_lat,
		 max_lat,
		 min_lon,
		 max_lon,
		 num_changes,
		 to_char(closed_at,'YYYY-MM-DD HH24:MI:SS "UTC"') as closed_at,
		 ((now() at time zone 'utc') > closed_at) as is_closed,
		 to_char((now() at time zone 'utc'),'YYYY-MM-DD HH24:MI:SS "UTC"') as current_time
	  FROM changesets WHERE id = $1 AND user_id = $2
	  FOR UPDATE NOWAIT
     )"_M);

  auto r = [&] {
    try {
      return m.exec_prepared("changeset_current_lock", changeset, req_ctx.user->id);
    } catch (const pqxx::sql_error &e) {
      if (e.sqlstate() == "55P03") // lock not available
        throw http::conflict(fmt::format("Changeset {:d} is currently locked by another process.", changeset));
      // rethrow all other sql errors
      throw;
    }
  }();

  if (r.affected_rows () != 1)
    throw http::conflict ("The user doesn't own that changeset");

  cs_num_changes = r[0]["num_changes"].as<int> ();
  is_closed = r[0]["is_closed"].as<bool> ();
  closed_at = r[0]["closed_at"].as<std::string> ();
  current_time = r[0]["current_time"].as<std::string> ();

  if (!(r.empty () || r[0]["min_lat"].is_null ()))
  {
      cs_bbox.minlat = r[0]["min_lat"].as<int64_t> ();
      cs_bbox.minlon = r[0]["min_lon"].as<int64_t> ();
      cs_bbox.maxlat = r[0]["max_lat"].as<int64_t> ();
      cs_bbox.maxlon = r[0]["max_lon"].as<int64_t> ();
  }
}

void ApiDB_Changeset_Updater::check_user_owns_changeset()
{
  m.prepare("changeset_exists",
	    R"( SELECT id, user_id
		FROM changesets
		WHERE id = $1)"_M);

  auto r = m.exec_prepared ("changeset_exists", changeset);

  if (r.affected_rows () != 1)
    throw http::not_found ("");

  if (r[0]["user_id"].as<osm_user_id_t> () != req_ctx.user->id)
    throw http::conflict ("The user doesn't own that changeset");
}

void ApiDB_Changeset_Updater::changeset_insert_subscriber()
{
  m.prepare ("insert_changeset_subscribers", R"( INSERT INTO "changesets_subscribers" ("subscriber_id", "changeset_id") VALUES ($1, $2) )");

  auto r = m.exec_prepared ("insert_changeset_subscribers", req_ctx.user->id,
                                    changeset);
  if (r.affected_rows () != 1)
    throw http::server_error("Cannot create changeset - insert_changeset_subscribers");
}

void ApiDB_Changeset_Updater::changeset_insert_tags(
    const std::map<std::string, std::string>& tags)
{
  if (tags.empty())
    return;

  m.prepare (
      "changeset_insert_tags",
      R"(

          WITH tmp_tag(changeset_id, k, v) AS (
             SELECT * FROM
             UNNEST( CAST($1 AS bigint[]),
                     CAST($2 AS character varying[]),
                     CAST($3 AS character varying[])
             )
          )
          INSERT INTO changeset_tags (changeset_id, k, v)
          SELECT * FROM tmp_tag
      )"_M);

  std::vector<osm_changeset_id_t> cs;
  std::vector<std::string> ks;
  std::vector<std::string> vs;
  unsigned total_tags = 0;

  for (const auto& [key, value] : tags)
  {
      cs.emplace_back(changeset);
      ks.emplace_back(escape(key));
      vs.emplace_back(escape(value));
      ++total_tags;
  }

  auto r = m.exec_prepared ("changeset_insert_tags", cs, ks, vs);

  if (r.affected_rows () != total_tags)
    throw http::server_error (
	"Cannot create changeset - changeset_insert_tags");
}

void ApiDB_Changeset_Updater::changeset_delete_tags()
{
  m.prepare ("delete_changeset_tags",  R"( DELETE FROM "changeset_tags" WHERE "changeset_tags"."changeset_id" = $1 )");
  auto r = m.exec_prepared ("delete_changeset_tags", changeset);
}

void ApiDB_Changeset_Updater::changeset_insert_cs()
{
  m.prepare (
      "create_changeset",
      R"(

       INSERT INTO changesets (user_id, created_at, closed_at)
            VALUES ($1,
                    (now() at time zone 'utc'),
                    (now() at time zone 'utc') + ($2 ::interval))
            RETURNING id
   )"_M);

  auto r = m.exec_prepared ("create_changeset", req_ctx.user->id, global_settings::get_changeset_timeout_idle());

  if (r.affected_rows () != 1)
    throw http::server_error ("Cannot create changeset");

  changeset = r[0]["id"].as<osm_changeset_id_t> ();
}

bbox_t ApiDB_Changeset_Updater::get_bbox() const
{
  return cs_bbox;
}

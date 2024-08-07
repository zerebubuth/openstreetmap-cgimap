#include <filesystem>
#include <iostream>

#include <benchmark/benchmark.h>

#include "test_database.hpp"
#include "test_request.hpp"
#include "cgimap/request_context.hpp"
#include "cgimap/rate_limiter.hpp"
#include "cgimap/routes.hpp"
#include "cgimap/process_request.hpp"

class BenchmarkTestDatabase : public test_database
{
public:
    BenchmarkTestDatabase()
    {
        setup(test_db_sql);
    }

    std::filesystem::path test_db_sql{"../test/structure.sql"};
};

static BenchmarkTestDatabase tdb{};

class BenchmarkDatabaseFixture : public benchmark::Fixture {
public:
    ~BenchmarkDatabaseFixture() override = default;

    void SetUp(::benchmark::State& state) override {
        tdb.testcase_starting();
    }

    void TearDown(::benchmark::State &state) override {
        tdb.testcase_ended();
    }

    inline test_database &test_db() { return tdb; }  
};

BENCHMARK_F(BenchmarkDatabaseFixture, NodeCreation)(benchmark::State& st) {

    test_db().run_sql(
            "INSERT INTO users (id, email, pass_crypt, creation_time, display_name, data_public) "
            "VALUES "
            "  (1, 'user_1@example.com', '', '2013-11-14T02:10:00Z', 'user_1', true), "
            "  (2, 'user_2@example.com', '', '2013-11-14T02:10:00Z', 'user_2', false); "

            "INSERT INTO changesets (id, user_id, created_at, closed_at) "
            "VALUES "
            "  (1, 1, '2013-11-14T02:10:00Z', '2013-11-14T03:10:00Z'), "
            "  (2, 1, '2013-11-14T02:10:00Z', '2013-11-14T03:10:00Z'), "
            "  (4, 2, '2013-11-14T02:10:00Z', '2013-11-14T03:10:00Z');"
        );

    test_request req{};
    RequestContext ctx{req};

    api06::OSMChange_Tracking change_tracking{};

    auto upd = test_db().get_data_update();
    auto node_updater = upd->get_node_updater(ctx, change_tracking);

    int64_t placeholder_id = -1;

    for (auto _ : st) {
        
        node_updater->add_node(-25.3448570, 131.0325171, 1, placeholder_id--, { {"name", "Uluṟu"}, {"ele", "863"} });
        node_updater->process_new_nodes();
        
        benchmark::ClobberMemory();
    }

    upd->commit();
}

BENCHMARK_F(BenchmarkDatabaseFixture, SimpleUpload)(benchmark::State& st) {

    tdb.run_sql(R"(
         INSERT INTO users (id, email, pass_crypt, pass_salt, creation_time, display_name, data_public, status)
         VALUES
           (1, 'demo@example.com', 'x', '', '2013-11-14T02:10:00Z', 'demo', true, 'confirmed'),
           (2, 'user_2@example.com', 'x', '', '2013-11-14T02:10:00Z', 'user_2', false, 'active');
  
        INSERT INTO changesets (id, user_id, created_at, closed_at, num_changes)
        VALUES
          (1, 1, now() at time zone 'utc', now() at time zone 'utc' + '1 hour' ::interval, 0),
          (2, 1, now() at time zone 'utc', now() at time zone 'utc' + '1 hour' ::interval, 10000),
          (3, 1, now() at time zone 'utc' - '12 hour' ::interval,
                 now() at time zone 'utc' - '11 hour' ::interval, 10000),
          (4, 2, now() at time zone 'utc', now() at time zone 'utc' + '1 hour' ::interval, 0),
          (5, 2, '2013-11-14T02:10:00Z', '2013-11-14T03:10:00Z', 0);
  
        INSERT INTO user_blocks (user_id, creator_id, reason, ends_at, needs_view)
        VALUES (1,  2, '', now() at time zone 'utc' - ('1 hour' ::interval), false);

        INSERT INTO oauth_applications (id, owner_type, owner_id, name, uid, secret, redirect_uri, scopes, confidential, created_at, updated_at) 
         VALUES (3, 'User', 1, 'App 1', 'dHKmvGkmuoMjqhCNmTJkf-EcnA61Up34O1vOHwTSvU8', '965136b8fb8d00e2faa2faaaed99c0ec10225518d0c8d9fb1d2af701e87eb68c', 
                'http://demo.localhost:3000', 'write_api read_gpx', false, '2021-04-12 17:53:30', '2021-04-12 17:53:30');
  
        INSERT INTO public.oauth_access_tokens (id, resource_owner_id, application_id, token, refresh_token, expires_in, revoked_at, created_at, scopes, previous_refresh_token) 
         VALUES (67, 1, 3, '4f41f2328befed5a33bcabdf14483081c8df996cbafc41e313417776e8fafae8', NULL, NULL, NULL, '2021-04-14 19:38:21', 'write_api', '');
        )"
    );

    const std::string bearertoken =
        "Bearer 4f41f2328befed5a33bcabdf14483081c8df996cbafc41e313417776e8fafae8";
    const std::string generator = "Test";

    auto sel_factory = test_db().get_data_selection_factory();
    auto upd_factory = test_db().get_data_update_factory();

    for (auto _ : st) {

        null_rate_limiter limiter;
        routes route;
        test_request req{};
        RequestContext ctx{req};

        req.set_header("REQUEST_METHOD", "POST");
        req.set_header("REQUEST_URI", "/api/0.6/changeset/1/upload");
        req.set_header("REMOTE_ADDR", "127.0.0.1");
        req.set_header("HTTP_AUTHORIZATION", bearertoken);

        req.set_payload(R"(<?xml version="1.0" encoding="UTF-8"?>
                    <osmChange version="0.6" generator="iD">
                    <create>
                    <node id="-5" lon="11" lat="46" version="0" changeset="1">
                        <tag k="highway" v="bus_stop" />
                    </node>
                    <node id="-6" lon="13" lat="47" version="0" changeset="1">
                        <tag k="highway" v="bus_stop" />
                    </node>
                    <node id="-7" lon="-54" lat="12" version="0" changeset="1"/>
                    <way id="-10" version="0" changeset="1">
                        <nd ref="-5"/>
                        <nd ref="-6"/>
                    </way>
                    <way id="-11" version="0" changeset="1">
                        <nd ref="-6"/>
                        <nd ref="-7"/>
                    </way>
                    <relation id="-2" version="0" changeset="1">
                        <member type="node" role="" ref="-5" />
                        <tag k="type" v="route" />
                        <tag k="name" v="AtoB" />
                    </relation>
                    <relation id="-3" version="0" changeset="1">
                        <member type="node" role="" ref="-6" />
                        <tag k="type" v="route" />
                        <tag k="name" v="BtoA" />
                    </relation>
                    <relation id="-4" version="0" changeset="1">
                        <member type="relation" role="" ref="-2" />
                        <member type="relation" role="" ref="-3" />
                        <tag k="type" v="route_master" />
                        <tag k="name" v="master" />
                    </relation>
                </create>
                </osmChange>)" );

    
        // execute the request
        process_request(req, limiter, generator, route, *sel_factory, upd_factory.get());

        benchmark::ClobberMemory();
    }
}

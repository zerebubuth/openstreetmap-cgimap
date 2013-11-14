-- add a public and non-public user for testing.
INSERT INTO users (id, email, pass_crypt, creation_time, display_name, data_public)
VALUES (1, 'user_1@example.com', '', '2013-11-14T02:10:00Z', 'user_1', true),
       (2, 'user_2@example.com', '', '2013-11-14T02:10:00Z', 'user_2', false);

-- give the users some changesets
INSERT INTO changesets (id, user_id, created_at, closed_at)
VALUES (1, 1, '2013-11-14T02:10:00Z', '2013-11-14T03:10:00Z'),
       (2, 1, '2013-11-14T02:10:00Z', '2013-11-14T03:10:00Z'),
       (3, 1, '2013-11-14T02:10:00Z', '2013-11-14T03:10:00Z'),
       (4, 2, '2013-11-14T02:10:00Z', '2013-11-14T03:10:00Z');

-- add some nodes for each of the users
INSERT INTO current_nodes (id, latitude, longitude, changeset_id, visible, "timestamp", tile, version)
VALUES (1,       0,       0, 1, true, '2013-11-14T02:10:00Z', 3221225472, 1),
       (2, 1000000, 1000000, 1, true, '2013-11-14T02:10:01Z', 3221227032, 1);
